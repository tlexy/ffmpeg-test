#include "mp4_to_flv.h"
#include <stdio.h>
#include <iostream>
extern "C"{
#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
}

void println(const char* str)
{
    std::cout << str << std::endl;
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    printf("%s: pts: %d, timebase: %d\n", tag, pkt->pts, time_base->den);
    //printf("%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n", tag, av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base), av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base), av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base), pkt->stream_index);
}

void mp4_to_flv(const char* input_file, const char* output_file)
{
    println("mp4 to flv");
    AVOutputFormat* outfmt = NULL;
    AVFormatContext* ifmt_ctx = NULL;
    AVFormatContext* ofmt_ctx = NULL;

    int ret = avformat_open_input(&ifmt_ctx, input_file, 0, 0);
    if (ret < 0)
    {
        std::cout << "error 000" << std::endl;
        return;
    }
    ret = avformat_find_stream_info(ifmt_ctx, 0);
    if (ret < 0)
    {
        println("error 111");
        return;
    }
    av_dump_format(ifmt_ctx, 0, input_file, 0);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, output_file);
    if (!ofmt_ctx)
    {
        println("error 222");
        return;
    }
    int istream_count = ifmt_ctx->nb_streams;
    int* stream_mapping = (int*)av_mallocz_array(istream_count, sizeof(int));
    if (!stream_mapping)
    {
        println("error 333");
        return;
    }

    println("calc istream count");
    int stream_index = 0;
    for (int i = 0; i < istream_count; ++i)
    {
        AVStream* out_stream;
        AVStream* in_stream = ifmt_ctx->streams[i];
        AVCodecParameters* in_par = in_stream->codecpar;
        if (in_par->codec_type != AVMEDIA_TYPE_AUDIO
                && in_par->codec_type != AVMEDIA_TYPE_SUBTITLE
                && in_par->codec_type != AVMEDIA_TYPE_VIDEO)
        {
            stream_mapping[i] = -1;
            continue;
        }
        stream_mapping[i] = stream_index++;
        //创建新流到ofmt_ctx中，并返回流的指针
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream)
        {
            println("error 555");
            return;
        }
        ret = avcodec_parameters_copy(out_stream->codecpar, in_par);
        if (ret < 0)
        {
            println("error 666");
            return;
        }
        out_stream->codecpar->codec_tag = 0;
    }
    std::cout << "stream count: " << stream_index << std::endl;
    av_dump_format(ofmt_ctx, 0, output_file, 1);
    println("start to convert...");
    //下面这里是做什么？？？
    if (!(ofmt_ctx->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, output_file, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            println("error 777");
            return;
        }
    }

    if ((ret = avformat_write_header(ofmt_ctx, NULL)) < 0)
    {
        println("error 888");
        return;
    }
    println("start to convert...action.");
    AVPacket pkt;
    while(true)
    {
        AVStream* in_stream;
        AVStream* out_stream;
        if ((ret = av_read_frame(ifmt_ctx, &pkt)) < 0)
        {
            println("read frame error.");
            break;
        }
        in_stream = ifmt_ctx->streams[pkt.stream_index];
        if (pkt.stream_index >= istream_count ||
            stream_mapping[pkt.stream_index] < 0)
        {
            av_packet_unref(&pkt);
            continue;
        }
        pkt.stream_index = stream_mapping[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        log_packet(ifmt_ctx, &pkt, "in");

        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        log_packet(ofmt_ctx, &pkt, "out");

        if ((ret = av_interleaved_write_frame(ofmt_ctx, &pkt)) < 0)
        {
            println("error 999");
            break;
        }
        av_packet_unref(&pkt);
    }
    av_write_trailer(ofmt_ctx);
    println("finish convert.");
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->flags & AVFMT_NOFILE))
    {
        avio_closep(&ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);
    avformat_free_context(ifmt_ctx);
    av_freep(&stream_mapping);

}


