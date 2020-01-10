//
// Created by JZhao2 on 2020/1/8.
//

#ifndef JOHNPLAYER_SLAUDIO_H
#define JOHNPLAYER_SLAUDIO_H

#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
extern "C"
{
    #include <SLES/OpenSLES.h>
    #include <SLES/OpenSLES_Android.h>
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/samplefmt.h"

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
};

#define LOGD(...) __android_log_print(ANDROID_LOG_INFO,"OpenSL",__VA_ARGS__)

class SLAudio {
public:
    void initOpenSLES();
    void initBufferQueue(int rate, int channel, int bitsPerSample);
    void stop();
    void play(char *url);
    int initFFmpeg(int *rate, int *channel, char *url);
    int releaseFFmpeg();
    static void* threadAudio(void *params)
    {
        SLAudio* audio = (SLAudio*)params;
        audio->decodeAudio();
    }
    void decodeAudio();
};


#endif //JOHNPLAYER_SLAUDIO_H
