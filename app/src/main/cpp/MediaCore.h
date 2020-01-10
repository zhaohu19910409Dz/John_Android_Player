//
// Created by hu zhao on 2019-12-29.
//

#ifndef TESTCPLUSPLUS_MEDIACORE_H
#define TESTCPLUSPLUS_MEDIACORE_H

#include <jni.h>
#include <string>
#include <unistd.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
}

#include <pthread.h>
#include "JQueue.h"
#include "JQueue.cpp"
#include <jni.h>
#include <android/log.h>
#define LOG(...) __android_log_print(ANDROID_LOG_INFO,"MediaCore",__VA_ARGS__)

using namespace std;

typedef struct{
    AVFormatContext* avFormatCtx;
    AVInputFormat*   iFormatCtx;
    AVCodecContext*  videoCtx;
    AVCodecContext*  audioCtx;
    AVDictionary*    format_opts;
    int              scan_all_pmts_set;

    int              eof;
    bool             bPause;
    //audio data
    int              last_audio_stream;
    int              audio_stream;
    AVStream*        audio_st;
    int              nb_channels;
    int              sample_rate;
    int64_t          channel_layout;
    AVSampleFormat   bitsSampleFormat;
    SwrContext*      swrContext;

    //video data
    int              last_video_stream;
    int              video_stream;
    AVStream         *video_st;
    int              queue_attachments_req;

    //subtitle data
    int              last_subtitle_stream;
}MediaCoreContext;

class MediaCore {
private:
    MediaCore();
    ~MediaCore();
    static MediaCore* pInstance;
    string fileName;
    MediaCoreContext* pContext;
    ANativeWindow*    pWindow;
public:
    static MediaCore* getInstance();
    static void releaseInstance();
    string  getVersion();
    string  getFileName()                   { return fileName;      }
    void    setFileName(string file)        { fileName = file;      }
    void    setWindow(ANativeWindow* pWind) { pWindow = pWind;      }
    int     getChannels()                   { return pContext->nb_channels; }
    SwrContext* getSwrContext()             { return pContext->swrContext;  }
    AVSampleFormat getSampleFormat()        { return pContext->bitsSampleFormat;}


    //status control
    bool Play();
    bool Pause();
    void InitFFmpeg();
    void Start();
    bool ReadFile();
    bool DecodeVideo();
    bool DecodeAudio();
    /*open a given stream Return 0 if OK*/
    int StreamComponentOpen(int stream_index);


    void PrintErrLog(int errID);

    //read thread to video and audio packet queue
    pthread_t  pReadThreadID;
    static void* readThread(void *params)
    {
        MediaCore* core = (MediaCore*)params;
        core->ReadFile();
    }

    //video decode thread to video frame queue
    JQueue<AVPacket> *pVideoPacketQueue;
    JQueue<AVPacket> *pAudioPacketQueue;
    pthread_t  pVideoDecodeThreadID;
    JQueue<AVFrame> *pVideoFrameQueue;
    static void* decodeVideoThread(void *params)
    {
        MediaCore* core = (MediaCore*)params;
        core->DecodeVideo();
    }

    //audio decode thread to audio frame queue
    pthread_t pAudioDecodeThreadID;
    JQueue<AVFrame> *pAudioFrameQueue;
    static void* decodeAudioThread(void *params)
    {
        MediaCore* core = (MediaCore*)params;
        core->DecodeAudio();
    }
    //audio interface
    void createEngine();
    void createMixVolume();
    void createPlayer();
    void realseResource();
    // engine interfaces
    SLObjectItf engineObject = NULL;
    SLEngineItf engineEngine;

// output mix interfaces
    SLObjectItf outputMixObject = NULL;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;

// buffer queue player interfaces
    SLObjectItf bqPlayerObject = NULL;
    SLPlayItf bqPlayerPlay;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    SLEffectSendItf bqPlayerEffectSend;
    SLMuteSoloItf bqPlayerMuteSolo;
    SLVolumeItf bqPlayerVolume;
// aux effect on the output mix, used by the buffer queue player
    const SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

    void *buffer;
    size_t bufferSize;

    uint8_t *outputBuffer;
    size_t outputBufferSize;
};
#endif //TESTCPLUSPLUS_MEDIACORE_H
