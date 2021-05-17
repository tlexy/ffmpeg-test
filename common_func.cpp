#include "common_func.h"

#define __STDC_CONSTANT_MACROS

extern "C"{
#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
//#include "libswresample/swresample.h"
}

#define RET(STR) \
    av_log(NULL, AV_LOG_ERROR, STR); \
    return;

void decode_audio_to_raw(const char* filename)
{
    AVFormatContext *format_ctx = NULL; //for opening multi-media file
    avformat_open_input(&format_ctx, filename, NULL, NULL);
    avformat_find_stream_info(format_ctx, NULL);

    av_dump_format(format_ctx, 0, filename, 0);

    int audio_index = -1;
    audio_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "audio index is invalid.\n");
        return;
    }
    av_log(NULL, AV_LOG_INFO, "audio index: %d\n", audio_index);

    AVCodec* audio_codec = avcodec_find_decoder(format_ctx->streams[audio_index]->codecpar->codec_id);
    if (audio_codec == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "audio decoder is NULL\n");
        return;
    }
    av_log(NULL, AV_LOG_INFO, "get avcodec\n");

    //编码上下文
    AVCodecContext* audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    avcodec_parameters_to_context(audio_codec_ctx, format_ctx->streams[audio_index]->codecpar);
    if (audio_codec == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "audio codec ctx alloc failed.\n");
        return;
    }
    av_log(NULL, AV_LOG_INFO, "get avcodec ctx\n");
    if(avcodec_open2(audio_codec_ctx, audio_codec, NULL)<0)
    {
        RET("open avcodec failed\n");
    }
    av_log(NULL, AV_LOG_INFO, "open avcodec\n");

    AVFrame* frame = av_frame_alloc();
    AVPacket audio_packet;
    int ret = -1;
    while(av_read_frame(format_ctx, &audio_packet) >= 0)
    {
       // av_log(NULL, AV_LOG_INFO, "read packet, stream_index: %d\n", audio_packet.stream_index);
        if (audio_packet.stream_index == audio_index)
        {
            ret = avcodec_send_packet(audio_codec_ctx, &audio_packet);
            //av_log(NULL, AV_LOG_INFO, "avcodec_send_packet, ret: %d\n", ret);
            if (ret == 0 || ret == AVERROR(EAGAIN))
            {
                //读取包
                int ret_code = 0;
                while(ret_code == 0)
                {
                    ret_code = avcodec_receive_frame(audio_codec_ctx, frame);
                    if (ret == AVERROR_EOF)
                    {
                        return;
                    }
                    else if (ret == AVERROR(EAGAIN))
                    {
                        break;
                    }
                    else if (ret < 0)
                    {
                         av_log(NULL, AV_LOG_ERROR, "Error encoding audio frame\n");
                         return;
                    }
                    if (ret_code >= 0)
                    {
                        av_log(NULL, AV_LOG_INFO, "linesize: %d, nb_samples: %d, pts: %lld, sample_rate: %d, channel_layout: %llu, pkt_duration: %lld, channels: %d\n",
                                frame->linesize[0], frame->nb_samples, frame->pts, frame->sample_rate, frame->channel_layout, frame->pkt_duration, frame->channels);
                    }
                }
            }
            //如果是因为内部解码队列没有空间而发送失败。。。
            if (ret == AVERROR(EAGAIN))
            {
                ret = avcodec_send_packet(audio_codec_ctx, &audio_packet);
                if (ret != 0)
                {
                    av_log(NULL, AV_LOG_WARNING, "something wrong may be happend.");
                }
            }
        }
        av_packet_unref(&audio_packet);
    }

    av_log(NULL, AV_LOG_INFO, "decode over...");
}
