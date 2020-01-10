//
// Created by hu zhao on 2019-12-29.
//

#include <assert.h>
#include "MediaCore.h"
MediaCore* MediaCore::pInstance = NULL;

static int ffmpeg_interrupt(void* ctx)
{
    MediaCore* pMediaCore = (MediaCore*)ctx;
    return 0;
}

void bqPlayerCallback1(SLAndroidSimpleBufferQueueItf bq, void * context)
{
    LOG("PCM Call Back\r\n");
    assert(context == NULL);
    MediaCore* pCtx = (MediaCore*)context;
    pCtx->bufferSize = 0;
    //assert(bq == pCtx->slBufferQueueItf);

    //if(pCtx->pAudioFrameQueue->queuen_nb_remaining() == 0)
    //    return;
    AVFrame* pFrame = pCtx->pAudioFrameQueue->queue_peek_readable();

#if 1
    swr_convert(pCtx->getSwrContext(), &pCtx->outputBuffer, pFrame->nb_samples, (const uint8_t**)pFrame->data, pFrame->nb_samples);
    int size = av_samples_get_buffer_size(pFrame->linesize, pCtx->getChannels(), pFrame->nb_samples, pCtx->getSampleFormat(), 1);
    LOG(">> getPcm data_size=%d", size);
    pCtx->bufferSize = size;

    if(pCtx->buffer!=NULL && pCtx->bufferSize!=0)
    {
        //将得到的数据加入到队列中
        (*pCtx->bqPlayerBufferQueue)->Enqueue(pCtx->bqPlayerBufferQueue,pCtx->outputBuffer,pCtx->bufferSize);
        pCtx->bufferSize = 0;
    }
#endif
    pCtx->pAudioFrameQueue->queue_next();
}

MediaCore::MediaCore():pContext(NULL),pWindow(NULL)
{
    pContext = new MediaCoreContext;
    memset(pContext, 0, sizeof(MediaCoreContext));

    pVideoPacketQueue = new JQueue<AVPacket>(3,"videoPacketQueue");
    pAudioPacketQueue = new JQueue<AVPacket>(9,"audioPacketQueue");

    pVideoFrameQueue = new JQueue<AVFrame>(3,"videoFrameQueue");
    pAudioFrameQueue = new JQueue<AVFrame>(9,"audioFrameQueue");
}

MediaCore::~MediaCore()
{
    if(pContext)
    {
        delete pContext;
        pContext = NULL;
    }

    if(pVideoPacketQueue)
    {
        delete pVideoPacketQueue;
        pVideoPacketQueue = NULL;
    }

    if(pAudioPacketQueue)
    {
        delete pAudioPacketQueue;
        pAudioPacketQueue = NULL;
    }

    if(pVideoFrameQueue)
    {
        delete pVideoFrameQueue;
        pVideoFrameQueue = NULL;
    }

    if(pAudioFrameQueue)
    {
        delete pAudioFrameQueue;
        pAudioFrameQueue = NULL;
    }
}

void MediaCore::releaseInstance()
{
    if(pInstance)
    {
        delete pInstance;
        pInstance = NULL;
    }
}

MediaCore* MediaCore::getInstance()
{
    if(pInstance == NULL)
    {
        pInstance = new MediaCore();
    }
    return pInstance;
}

string MediaCore::getVersion()
{
    return av_version_info();
}

void MediaCore::InitFFmpeg()
{
    LOG("InitFFmpeg\r\n");
    //1:register ffmpeg component
    avformat_network_init();

    //2:Init ffmpeg context
    pContext->avFormatCtx = avformat_alloc_context();
    if(!pContext->avFormatCtx)
    {
        LOG("Could not allocate context\r\n");
        return;
    }

    //pContext->avFormatCtx->interrupt_callback.callback = ffmpeg_interrupt;
    //pContext->avFormatCtx->interrupt_callback.opaque = this;
    if(!av_dict_get(pContext->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE))
    {
        av_dict_set(&pContext->format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        pContext->scan_all_pmts_set = 1;
    }
}

void MediaCore::Start()
{
    LOG("Start ReadThread\r\n");
    pthread_create(&pReadThreadID, NULL, readThread, this);
}

/*
 Read file packet to pVideoPacketQueue and pAudioPacketQueue
 */
bool MediaCore::ReadFile()
{
    int err;
    err = access(fileName.c_str(),0);
    if(err < 0)
    {
        LOG("Not find file\r\n");
    }

    //3:open file
    err = avformat_open_input(&pContext->avFormatCtx, fileName.c_str(), pContext->iFormatCtx, &pContext->format_opts);
    if(err < 0)
    {
        PrintErrLog(err);
        return false;
    }

    //4:find video stream info
    err = avformat_find_stream_info(pContext->avFormatCtx, NULL);
    if(err < 0)
    {
        LOG("Could not find stream info\r\n");
        return false;
    }

    pContext->video_stream = -1;
    pContext->audio_stream = -1;
    for(int i = 0; i < pContext->avFormatCtx->nb_streams; i++)
    {
        if(pContext->avFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            pContext->video_stream = i;
        }
        else if(pContext->avFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            pContext->audio_stream = i;
        }
    }
    //5:find video decoder context
    //6:init video code
    if(pContext->video_stream != -1)
    {
        //StreamComponentOpen(pContext->video_stream);
    }

    if(pContext->audio_stream != -1)
    {
        StreamComponentOpen(pContext->audio_stream);
    }

    AVPacket *packet = av_packet_alloc();
    int ret;
    while(true/*av_read_frame(pContext->avFormatCtx, packet) >= 0*/)
    {
        ret = av_read_frame(pContext->avFormatCtx, packet);
        if(ret < 0)
        {
            if((ret == AVERROR_EOF || avio_feof(pContext->avFormatCtx->pb)) && !pContext->eof)
            {
                //insert empty packet means read file end
                if(pContext->video_stream != -1)
                {
                    //put empty packet into queue
                    AVPacket emptyPacket;
                    av_init_packet(&emptyPacket);
                    emptyPacket.size = 0;
                    emptyPacket.data = NULL;
                    emptyPacket.stream_index = pContext->video_stream;
                    //emptyPacket.

                    AVPacket* pVideoPacket = pVideoPacketQueue->queue_peek_writable();
                    *pVideoPacket = emptyPacket;
                    pVideoPacketQueue->queue_push();
                }
                if(pContext->audio_stream != -1)
                {
                    AVPacket emptyPacket;
                    av_init_packet(&emptyPacket);
                    emptyPacket.size = 0;
                    emptyPacket.data = NULL;
                    emptyPacket.stream_index = pContext->audio_stream;
                    //emptyPacket.

                    AVPacket* pAudioPacket = pAudioPacketQueue->queue_peek_writable();
                    *pAudioPacket = emptyPacket;
                    pAudioPacketQueue->queue_push();
                }
                pContext->eof = 1;
            }

            if(pContext->avFormatCtx->pb && pContext->avFormatCtx->pb->error)
                break;
        }
        else
        {
            pContext->eof = 0;
        }

        if(packet->stream_index == pContext->video_stream)
        {
            //AVPacket* pVideoPacket = pVideoPacketQueue->queue_peek_writable();
            //*pVideoPacket = *packet;
            //pVideoPacketQueue->queue_push();
        }
        else if(packet->stream_index == pContext->audio_stream)
        {
            AVPacket* pAudioPacket = pAudioPacketQueue->queue_peek_writable();
            *pAudioPacket = *packet;
            pAudioPacketQueue->queue_push();
        }
    }
    av_packet_unref(packet);
}

bool MediaCore::DecodeVideo()
{
    AVFrame *frame = av_frame_alloc();
    AVFrame *rgba_frame = av_frame_alloc();

    int videoWidth = pContext->videoCtx->width;
    int videoHeight = pContext->videoCtx->height;
    LOG("Width:%d,Height:%d\r\n",videoWidth,videoHeight);

    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    uint8_t* out_buffer = (uint8_t*)av_malloc(buffer_size * sizeof(uint8_t));
    av_image_fill_arrays(rgba_frame->data, rgba_frame->linesize, out_buffer, AV_PIX_FMT_RGBA, videoWidth, videoHeight,1);
    struct SwsContext *data_convert_contex = sws_getContext( \
            videoWidth, videoHeight, pContext->videoCtx->pix_fmt, \
            videoWidth, videoHeight, AV_PIX_FMT_RGBA, \
            SWS_BICUBIC, NULL, NULL, NULL);

    ANativeWindow_Buffer window_buffer;
    int err = ANativeWindow_setBuffersGeometry(pWindow, videoWidth, videoHeight,WINDOW_FORMAT_RGBA_8888);
    if(err < 0)
    {
        LOG("Could not set native window buffer");
        ANativeWindow_release(pWindow);
        return false;
    }

    while(true)
    {
        //if(!pContext->bPause)
        {
            AVPacket* packet = pVideoPacketQueue->queue_peek_readable();
            err = avcodec_send_packet(pContext->videoCtx, packet);
            if(err < 0 && err != AVERROR(EAGAIN) && err != AVERROR_EOF)
            {
                LOG("Codec step 1 fail\r\n");
                return false;
            }

            err = avcodec_receive_frame(pContext->videoCtx, frame);
            if(err < 0 && err != AVERROR_EOF)
            {
                LOG("Codec step2 faild\r\n");
                return false;
            }

            frame->pts = frame->best_effort_timestamp;
            err = sws_scale( \
                    data_convert_contex, \
                    (const uint8_t* const*)frame->data, frame->linesize, \
                    0,videoHeight, \
                    rgba_frame->data, rgba_frame->linesize);
            if(err < 0)
            {
                //LOG("Data convert fail\r\n");
                PrintErrLog(err);
                return false;
            }

            err = ANativeWindow_lock(pWindow,&window_buffer, NULL);
            if(err < 0)
            {
                LOG("Could not lock native window\r\n");
            }
            else
            {
                uint8_t *bits = (uint8_t *) window_buffer.bits;
                for (int h = 0; h < videoHeight; h++)
                {
                    memcpy(bits + h * window_buffer.stride * 4, \
                           out_buffer + h * rgba_frame->linesize[0], \
                           rgba_frame->linesize[0]);
                }
            }
            ANativeWindow_unlockAndPost(pWindow);
            pVideoPacketQueue->queue_next();
        }
    }
}

bool MediaCore::DecodeAudio()
{
    AVPacket* pPacket = (AVPacket*)av_malloc(sizeof(AVPacket));
    AVFrame* pFrame = av_frame_alloc();
    pContext->swrContext = swr_alloc();

    int length = 0;
    int got_frame;
    //uint8_t *out_buffer = (uint8_t*)av_malloc(44100 * 2);
    buffer = (uint8_t*)av_malloc(44100 * 2);
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;

    enum AVSampleFormat out_format = AV_SAMPLE_FMT_S16;
    int out_sample_rate = pContext->sample_rate;

    swr_alloc_set_opts(pContext->swrContext, out_ch_layout, out_format, out_sample_rate, \
            pContext->audioCtx->channel_layout, pContext->audioCtx->sample_fmt, pContext->audioCtx->sample_rate, 0, NULL);
    swr_init(pContext->swrContext);

    int  out_channer_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);

    while(true)
    {
        //if(!pContext->bPause)
        {
            AVPacket* packet = pAudioPacketQueue->queue_peek_readable();

            int err = avcodec_send_packet(pContext->audioCtx, packet);
            if(err < 0 && err != AVERROR(EAGAIN) && err != AVERROR_EOF)
            {
                LOG("Codec step 1 fail\r\n");
                return false;
            }

            err = avcodec_receive_frame(pContext->audioCtx, pFrame);
            if(err < 0 && err != AVERROR_EOF)
            {
                LOG("Codec step2 faild\r\n");
                return false;
            }

            AVRational tb = (AVRational){1,pFrame->sample_rate};
            if(pFrame->pts != AV_NOPTS_VALUE)
            {
                pFrame->pts = av_rescale_q(pFrame->pts, pContext->audioCtx->pkt_timebase, tb);
            }

            AVFrame* f = pAudioFrameQueue->queue_peek_writable();
            *f = *pFrame;
            pAudioFrameQueue->queue_push();
            LOG("Audio:Packet Remains:%d,Frame Remains:%d\r\n",pAudioPacketQueue->queuen_nb_remaining(), pAudioFrameQueue->queuen_nb_remaining());
            pAudioPacketQueue->queue_next();
        }
    }
}

void MediaCore::PrintErrLog(int errID)
{
    char errbuf[128];
    const char* errbuf_ptr = errbuf;
    if(av_strerror(errID, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(errID));
    //av_log(NULL, AV_LOG_ERROR,"%s\n",errbuf_ptr);
    LOG("Error:%s\r\n",errbuf_ptr);
}

int MediaCore::StreamComponentOpen(int stream_index)
{
    AVFormatContext *ic = pContext->avFormatCtx;
    AVCodecContext *avctx = NULL;
    AVCodec *codec = NULL;
    const char *forced_codec_name = NULL;
    AVDictionary *opts = NULL;
    AVDictionaryEntry *t = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;

    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);

    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0)
    {
        LOG("avcodec_parameters_to_context  failed\r\n");
        avcodec_free_context(&avctx);
        return -1;
    }
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;
    codec = avcodec_find_decoder(avctx->codec_id);

    switch (avctx->codec_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            pContext->audioCtx = avctx;
            pContext->last_audio_stream = stream_index;
            pContext->sample_rate = avctx->sample_rate;
            //forced_codec_name = audio_codec_name;
            break;
        case AVMEDIA_TYPE_VIDEO:
            pContext->videoCtx = avctx;
            pContext->last_video_stream = stream_index;
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            pContext->last_subtitle_stream = stream_index;
            break;
    }
    avctx->codec_id = codec->id;
#if 0
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "thread", "auto", 0);

    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        av_dict_set(&opts, "refcounted_frames", "1", 0);
#endif
    if ((ret = avcodec_open2(avctx, codec, &opts)) < 0)
    {
        LOG("avcodec open failed\r\n");
        avcodec_free_context(&avctx);
    }
#if 0
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX)))
    {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        return ret;
    }
#endif
    pContext->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            pContext->sample_rate = avctx->sample_rate;
            pContext->nb_channels = avctx->channels;
            pContext->channel_layout = avctx->channel_layout;
            pContext->bitsSampleFormat = avctx->sample_fmt;
            createEngine();
            createMixVolume();
            LOG("Start Audio Decode Thread\r\n");
            pthread_create(&pVideoDecodeThreadID, NULL, decodeAudioThread, this);
            break;
        case AVMEDIA_TYPE_VIDEO:
            pContext->video_stream = stream_index;
            pContext->video_st = ic->streams[stream_index];

            //decoder_init
            //decoder_start
            LOG("Start Video Decode Thread\r\n");
            pthread_create(&pVideoDecodeThreadID, NULL, decodeVideoThread, this);
            pContext->queue_attachments_req = 1;
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            break;
        default:
            break;
    }
    return 0;
}

bool  MediaCore::Play()
{
    if(pContext->bPause)
        pContext->bPause = false;
}
bool  MediaCore::Pause()
{
    if(!pContext->bPause)
        pContext->bPause = true;
}

void MediaCore::createEngine()
{
    LOG(">> initOpenSLES...");
    SLresult result;

    // 1、create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    LOG(">> initOpenSLES... step 1, result = %d", result);

    // 2、realize the engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    LOG(">> initOpenSLES...step 2, result = %d", result);

    // 3、get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    LOG(">> initOpenSLES...step 3, result = %d", result);

    // 4、create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, 0, 0);
    LOG(">> initOpenSLES...step 4, result = %d", result);

    // 5、realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    LOG(">> initOpenSLES...step 5, result = %d", result);

    // 6、get the environmental reverb interface
    // this could fail if the environmental reverb effect is not available,
    // either because the feature is not present, excessive CPU load, or
    // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        LOG(">> initOpenSLES...step 6, result = %d", result);
    }
}
#define LOGD LOG
void MediaCore::createMixVolume()
{
    LOGD(">> initBufferQueue");
    SLresult result;
    int channel = pContext->nb_channels;
    int rate = pContext->sample_rate;
    int bitsPerSample = pContext->bitsSampleFormat;
    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm;
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = channel;
    format_pcm.samplesPerSec = rate * 1000;
    format_pcm.bitsPerSample = bitsPerSample;
    format_pcm.containerSize = 16;
    if (channel == 2)
        format_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    else
        format_pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
    format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    // create audio player
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND,
            /*SL_IID_MUTESOLO,*/ SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
            /*SL_BOOLEAN_TRUE,*/ SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
                                                3, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // realize the player
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the play interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the buffer queue interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // register callback on the buffer queue
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback1, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the effect send interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_EFFECTSEND,
                                             &bqPlayerEffectSend);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // get the volume interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    // set the player's state to playing
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;

    outputBufferSize = 8196;
    outputBuffer = (uint8_t*)av_malloc(sizeof(uint8_t) * outputBufferSize);
}