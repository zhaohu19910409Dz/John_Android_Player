//
// Created by hu zhao on 2019-12-29.
//

#include "MediaCore.h"
MediaCore* MediaCore::pInstance = NULL;

static int ffmpeg_interrupt(void* ctx)
{
    MediaCore* pMediaCore = (MediaCore*)ctx;
    return 0;
}

void pcmCallBack(SLAndroidSimpleBufferQueueItf slBufferQueueItf, void * context)
{
    MediaCore* pCtx = (MediaCore*)context;
    pCtx->buffersize = 0;

    AVFrame* pFrame = pCtx->pAudioFrameQueue->queue_peek_readable();
#if 0
    swr_convert(pCtx->getSwrContext(), &pCtx->buffer, 44100 * 2, (const uint8_t**)pFrame->data, pFrame->nb_samples);
    int size = av_samples_get_buffer_size(NULL, pCtx->getChannels(), pFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);
    pCtx->buffersize = size;

    if(pCtx->buffer!=NULL && pCtx->buffersize!=0)
    {
        //将得到的数据加入到队列中
        (*slBufferQueueItf)->Enqueue(slBufferQueueItf,pCtx->buffer,pCtx->buffersize);
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
    realseResource();
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
        StreamComponentOpen(pContext->video_stream);
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
            AVPacket* pVideoPacket = pVideoPacketQueue->queue_peek_writable();
            *pVideoPacket = *packet;
            pVideoPacketQueue->queue_push();
        }
        else if(packet->stream_index == pContext->audio_stream)
        {
            //AVPacket* pAudioPacket = pAudioPacketQueue->queue_peek_writable();
            //*pAudioPacket = *packet;
            //pAudioPacketQueue->queue_push();
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
        if(!pContext->bPause)
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
                    memcpy(bits + h * window_buffer.stride * 4,
                           out_buffer + h * rgba_frame->linesize[0],
                           rgba_frame->linesize[0]);
                }
            }
            ANativeWindow_unlockAndPost(pWindow);
            pVideoPacketQueue->queue_next();
        }
        else
        {
            LOG("pause\r\n");
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

    swr_alloc_set_opts(pContext->swrContext, out_ch_layout, out_format, out_sample_rate,
            pContext->audioCtx->channel_layout, pContext->audioCtx->sample_fmt, pContext->audioCtx->sample_rate, 0, NULL);
    swr_init(pContext->swrContext);

    int  out_channer_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);

    while(true)
    {
        if(!pContext->bPause)
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
            createEngine();
            createMixVolume();
            createPlayer();
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
    slCreateEngine(&engineObeject, 0, NULL, 0, NULL, NULL);
    (*engineObeject)->Realize(engineObeject, SL_BOOLEAN_FALSE);
    (*engineObeject)->GetInterface(engineObeject, SL_IID_ENGINE, &engineEngine);
}

void MediaCore::createMixVolume()
{
    (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, 0, 0);
    (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    SLresult sLresult = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB, &outputMixEnvironmentalReverb);
    if(SL_RESULT_SUCCESS == sLresult)
    {
        (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(outputMixEnvironmentalReverb, &settings);
    }
}

void MediaCore::createPlayer()
{
    int rate = pContext->sample_rate;
    int channels = pContext->nb_channels;
    SLDataLocator_AndroidBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM, channels, rate * 1000,
                            SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                            SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource dataSource = {&android_queue, &pcm};
    SLDataLocator_OutputMix slDataLocator_outputMix = { SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink slDataSink = {&slDataLocator_outputMix, NULL};

    const SLInterfaceID ids[3] = { SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    const SLboolean req[3] = { SL_BOOLEAN_FALSE, SL_BOOLEAN_FALSE, SL_BOOLEAN_FALSE};
    (*engineEngine)->CreateAudioPlayer(engineEngine, &audioPlayer, &dataSource, &slDataSink, 3, ids, req);
    (*audioPlayer)->Realize(audioPlayer, SL_BOOLEAN_FALSE);

    (*audioPlayer)->GetInterface(audioPlayer, SL_IID_PLAY, &slPlayItf);
    (*audioPlayer)->GetInterface(audioPlayer, SL_IID_BUFFERQUEUE, &slBufferQueueItf);
    (*slBufferQueueItf)->RegisterCallback(slBufferQueueItf, pcmCallBack, this);
    (*slPlayItf)->SetPlayState(slPlayItf, SL_PLAYSTATE_PLAYING);
}

void MediaCore::realseResource()
{
    if(audioPlayer!=NULL){
        (*audioPlayer)->Destroy(audioPlayer);
        audioPlayer=NULL;
        slBufferQueueItf=NULL;
        slPlayItf=NULL;
    }
    if(outputMixObject!=NULL){
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject=NULL;
        outputMixEnvironmentalReverb=NULL;
    }
    if(engineObeject!=NULL){
        (*engineObeject)->Destroy(engineObeject);
        engineObeject=NULL;
        engineEngine=NULL;
    }
}