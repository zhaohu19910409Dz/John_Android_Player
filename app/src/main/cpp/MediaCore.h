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
    //audio data
    int              last_audio_stream;

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
    static MediaCore* pInstance;
    string fileName;
    MediaCoreContext* pContext;
    ANativeWindow*    pWindow;
public:
    static MediaCore* getInstance();
    string getVersion();
    string getFileName()    {   return fileName;    }
    void setFileName(string file)   {  fileName = file; }
    void setWindow(ANativeWindow* pWind) { pWindow = pWind; LOG("setWindow\r\n"); }

    void InitFFmpeg();
    void Start();
    bool ReadFile();
    bool DecodeVideo();
    bool DecodeAudio();
    bool RenderVideo();
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

    //video render thread
    pthread_t pVideoRenderThreadID;
    static void* renderVideoThread(void *params)
    {
        MediaCore* core = (MediaCore*)params;
        core->RenderVideo();
    }
};
#endif //TESTCPLUSPLUS_MEDIACORE_H
