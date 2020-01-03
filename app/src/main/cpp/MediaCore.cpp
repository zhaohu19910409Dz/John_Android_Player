//
// Created by hu zhao on 2019-12-29.
//

#include "MediaCore.h"
MediaCore* MediaCore::pInstance = NULL;

#define TestCode
static int ffmpeg_interrupt(void* ctx)
{
    MediaCore* pMediaCore = (MediaCore*)ctx;
    return 0;
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
    //pthread_create(&pReadThreadID, NULL, readThread, this);
    ReadFile();
#if 0
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
        return;
    }

    //4:find video stream info
    err = avformat_find_stream_info(pContext->avFormatCtx, NULL);
    if(err < 0)
    {
        LOG("Could not find stream info\r\n");
        return;
    }

    int video_stream_index = -1;
    int audio_stream_index = -1;
    for(int i = 0; i < pContext->avFormatCtx->nb_streams; i++)
    {
        if(pContext->avFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
        }
        else if(pContext->avFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_stream_index = i;
        }
    }

    if(video_stream_index == -1)
    {
        LOG("Could not find video stream\r\n");
        return;
    }

    if(audio_stream_index == -1)
    {
        LOG("Could not find audio stream\r\n");
    }
    //5:find video decoder context
    //6:init video code
    pContext->videoCtx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(pContext->videoCtx, pContext->avFormatCtx->streams[video_stream_index]->codecpar);

    //7:find  decoder
    AVCodec* video_codec = avcodec_find_decoder(pContext->videoCtx->codec_id);
    if(video_codec == NULL)
    {
        LOG("Could not find video codec\r\n");
        return ;
    }
    //8:open video decoder
    err = avcodec_open2(pContext->videoCtx, video_codec, NULL);
    if(err < 0)
    {
        LOG("Could not find video stream\r\n");
        return;
    }
    //get video width height
    int videoWidth = pContext->videoCtx->width;
    int videoHeight = pContext->videoCtx->height;

    err = ANativeWindow_setBuffersGeometry(pWindow, videoWidth, videoHeight,WINDOW_FORMAT_RGBA_8888);
    if(err < 0)
    {
        LOG("Could not set native window buffer");
        ANativeWindow_release(pWindow);
        return ;
    }
    ANativeWindow_Buffer window_buffer;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *rgba_frame = av_frame_alloc();

    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    uint8_t* out_buffer = (uint8_t*)av_malloc(buffer_size * sizeof(uint8_t));
    av_image_fill_arrays(rgba_frame->data, rgba_frame->linesize, out_buffer, AV_PIX_FMT_RGBA, videoWidth, videoHeight,1);
    struct SwsContext *data_convert_contex = sws_getContext(
            videoWidth, videoHeight, pContext->videoCtx->pix_fmt,
            videoWidth, videoHeight, AV_PIX_FMT_RGBA,
            SWS_BICUBIC, NULL, NULL, NULL);

    while(av_read_frame(pContext->avFormatCtx, packet) >= 0)
    {
        if(packet->stream_index == video_stream_index)
        {
            err = avcodec_send_packet(pContext->videoCtx, packet);
            if(err < 0 && err != AVERROR(EAGAIN) && err != AVERROR_EOF)
            {
                LOG("Codec step 1 fail\r\n");
                return;
            }

            err = avcodec_receive_frame(pContext->videoCtx, frame);
            if(err < 0 && err != AVERROR_EOF)
            {
                LOG("Codec step2 faild\r\n");
                return ;
            }

            err = sws_scale(
                    data_convert_contex,
                    (const uint8_t* const*)frame->data, frame->linesize,
                    0,videoHeight,
                    rgba_frame->data, rgba_frame->linesize);
            if(err < 0)
            {
                //LOG("Data convert fail\r\n");
                PrintErrLog(err);
                return;
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
        }
        else if(packet->stream_index == audio_stream_index)
        {
        }
    }
    av_packet_unref(packet);
#endif
}

/*
 Read file packet to pVideoPacketQueue and pAudioPacketQueue
 */
bool MediaCore::ReadFile()
{
    LOG("OpenFile\r\n");
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
#if 0
    if(pContext->scan_all_pmts_set)
        av_dict_set(&pContext->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

    AVDictionaryEntry* t;
    if((t = av_dict_get(pContext->format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX)))
    {
        LOG("Option %s not found\r\n",t->key);
        return false;
    }
#endif

    //4:find video stream info
    err = avformat_find_stream_info(pContext->avFormatCtx, NULL);
    if(err < 0)
    {
        LOG("Could not find stream info\r\n");
        return false;
    }

    int video_stream_index = -1;
    int audio_stream_index = -1;
    for(int i = 0; i < pContext->avFormatCtx->nb_streams; i++)
    {
        if(pContext->avFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
        }
        else if(pContext->avFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_stream_index = i;
        }
    }

    if(video_stream_index == -1)
    {
        LOG("Could not find video stream\r\n");
        return false;
    }

    if(audio_stream_index == -1)
    {
        LOG("Could not find audio stream\r\n");
    }
    //5:find video decoder context
    //6:init video code
    if(video_stream_index != -1)
    {
        LOG("open video stream data\t\n");
        //StreamComponentOpen(video_stream_index);
        pContext->videoCtx = avcodec_alloc_context3(NULL);
        avcodec_parameters_to_context(pContext->videoCtx, pContext->avFormatCtx->streams[video_stream_index]->codecpar);

        //7:find  decoder
        AVCodec* video_codec = avcodec_find_decoder(pContext->videoCtx->codec_id);
        if(video_codec == NULL)
        {
            LOG("Could not find video codec\r\n");
            return false;
        }
        //8:open video decoder
        err = avcodec_open2(pContext->videoCtx, video_codec, NULL);
        if(err < 0)
        {
            LOG("Could not find video stream\r\n");
            return false;
        }
    }

    if(audio_stream_index != -1)
    {
        LOG("open audio stream data\t\n");
        //StreamComponentOpen(audio_stream_index);
    }
#ifdef TestCode
    ANativeWindow_Buffer window_buffer;
    int videoWidth = pContext->videoCtx->width;
    int videoHeight = pContext->videoCtx->height;

    err = ANativeWindow_setBuffersGeometry(pWindow, videoWidth, videoHeight,WINDOW_FORMAT_RGBA_8888);
    if(err < 0)
    {
        LOG("Could not set native window buffer");
        ANativeWindow_release(pWindow);
        return false;
    }
    AVFrame *frame = av_frame_alloc();
    AVFrame *rgba_frame = av_frame_alloc();

    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
    uint8_t* out_buffer = (uint8_t*)av_malloc(buffer_size * sizeof(uint8_t));
    av_image_fill_arrays(rgba_frame->data, rgba_frame->linesize, out_buffer, AV_PIX_FMT_RGBA, videoWidth, videoHeight,1);
    struct SwsContext *data_convert_contex = sws_getContext( \
            videoWidth, videoHeight, pContext->videoCtx->pix_fmt, \
            videoWidth, videoHeight, AV_PIX_FMT_RGBA, \
            SWS_BICUBIC, NULL, NULL, NULL);
#endif
    AVPacket *packet = av_packet_alloc();
    while(av_read_frame(pContext->avFormatCtx, packet) >= 0)
    {
        if(packet->stream_index == video_stream_index)
        {
            //AVPacket* pVideoPacket = pVideoPacketQueue->queue_peek_writable();
            //*pVideoPacket = *packet;
            //pVideoPacketQueue->queue_push();
            int err = avcodec_send_packet(pContext->videoCtx, packet);
            if(err < 0 && err != AVERROR(EAGAIN) && err != AVERROR_EOF)
            {
                LOG("Codec step 1 fail\r\n");
                return false;
            }
            err = avcodec_receive_frame(pContext->videoCtx, frame);
            if(err < 0 && err != AVERROR_EOF)
            {
                LOG("Codec step2 faild\r\n");
                return  false;
            }
            err = sws_scale(
                    data_convert_contex,
                    (const uint8_t* const*)frame->data, frame->linesize,
                    0,videoHeight,
                    rgba_frame->data, rgba_frame->linesize);
            if(err < 0)
            {
                LOG("Data convert fail\r\n");
                PrintErrLog(err);
                return false;
            }

            //AVFrame* pFrame = pVideoFrameQueue->queue_peek_writable();
            //*pFrame = *rgba_frame;
            //pVideoFrameQueue->queue_push();
            err = ANativeWindow_lock(pWindow,&window_buffer, NULL);
            if(err < 0)
            {
                LOG("Could not lock native window\r\n");
            }
            else
            {
                uint8_t *bits = (uint8_t *) window_buffer.bits;
                LOG("window buffer stride:%d,linesize[0]:%d\r\n",window_buffer.stride,rgba_frame->linesize[0]);
                for (int h = 0; h < videoHeight; h++)
                {
                    memcpy(bits + h * window_buffer.stride * 4,
                           rgba_frame->data + h * rgba_frame->linesize[0],
                           rgba_frame->linesize[0]);
                }
            }
            ANativeWindow_unlockAndPost(pWindow);
        }
        else if(packet->stream_index == audio_stream_index)
        {
            //AVPacket *pAudioPacket = pAudioPacketQueue->queue_peek_writable();
            //*pAudioPacket = *packet;
            //pAudioPacketQueue->queue_push();
        }
    }
    av_packet_unref(packet);
}

bool MediaCore::DecodeVideo()
{
#if 1
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
#endif
    LOG("Start Render Video Thread\r\n");
    //pthread_create(&pVideoRenderThreadID, NULL, renderVideoThread, this);

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
        AVPacket* packet = pVideoPacketQueue->queue_peek_readable();
        int err = avcodec_send_packet(pContext->videoCtx, packet);
        if(err < 0 && err != AVERROR(EAGAIN) && err != AVERROR_EOF)
        {
            LOG("Codec step 1 fail\r\n");
            return false;
        }

        err = avcodec_receive_frame(pContext->videoCtx, frame);
        if(err < 0 && err != AVERROR_EOF)
        {
            LOG("Codec step2 faild\r\n");
            return  false;
        }
        err = sws_scale( \
                data_convert_contex, \
                (const uint8_t* const*)frame->data, frame->linesize, \
                0,videoHeight, \
                rgba_frame->data, rgba_frame->linesize);
        if(err < 0)
        {
            LOG("Data convert fail\r\n");
            PrintErrLog(err);
            return false;
        }

        //AVFrame* pFrame = pVideoFrameQueue->queue_peek_writable();
        //*pFrame = *rgba_frame;
        //pVideoFrameQueue->queue_push();
        err = ANativeWindow_lock(pWindow,&window_buffer, NULL);
        if(err < 0)
        {
            LOG("Could not lock native window\r\n");
        }
        else
        {
            uint8_t *bits = (uint8_t *) window_buffer.bits;
            //LOG("window buffer stride:%d,linesize[0]:%d\r\n",window_buffer.stride,rgba_frame->linesize[0]);
            for (int h = 0; h < videoHeight; h++)
            {
                memcpy(bits + h * window_buffer.stride * 4, \
                       rgba_frame->data + h * rgba_frame->linesize[0], \
                       rgba_frame->linesize[0]);
            }
        }
        ANativeWindow_unlockAndPost(pWindow);
        pVideoPacketQueue->queue_next();
    }
}

bool MediaCore::DecodeAudio()
{

}

bool MediaCore::RenderVideo()
{
#if 1
    int err;
    ANativeWindow_Buffer window_buffer;
    int videoWidth = pContext->videoCtx->width;
    int videoHeight = pContext->videoCtx->height;

    err = ANativeWindow_setBuffersGeometry(pWindow, videoWidth, videoHeight,WINDOW_FORMAT_RGBA_8888);
    if(err < 0)
    {
        LOG("Could not set native window buffer");
        ANativeWindow_release(pWindow);
        return false;
    }
#endif
    while(true)
    {
        AVFrame* rgba_frame = pVideoFrameQueue->queue_peek_readable();

        int err = ANativeWindow_lock(pWindow,&window_buffer, NULL);
        if(err < 0)
        {
            LOG("Could not lock native window\r\n");
        }
        else
        {
            uint8_t *bits = (uint8_t *) window_buffer.bits;
            //LOG("window buffer stride:%d,linesize[0]:%d\r\n",window_buffer.stride,rgba_frame->linesize[0]);
            for (int h = 0; h < 15/*videoHeight*/; h++)
            {
                memcpy(bits + h * window_buffer.stride * 4, \
                       rgba_frame->data + h * rgba_frame->linesize[0], \
                       rgba_frame->linesize[0]);
            }
        }
        ANativeWindow_unlockAndPost(pWindow);

        pVideoFrameQueue->queue_next();
        usleep(20);
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
            pContext->last_audio_stream = stream_index;
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
            sample_rate = avctx->sample_rate;
            nb_channels = avctx->channels;
            channel_layout = avctx->channel_layout;
            break;
        case AVMEDIA_TYPE_VIDEO:
            pContext->video_stream = stream_index;
            pContext->video_st = ic->streams[stream_index];

            //decoder_init
            //decoder_start
            LOG("Start Video Decode Thread\r\n");
            //pthread_create(&pVideoDecodeThreadID, NULL, decodeVideoThread, this);
            pContext->queue_attachments_req = 1;
            break;
        //case AVMEDIA_TYPE_SUBTITLE:
        //    break;
        //default:
        //    break;
    }
    return 0;
}