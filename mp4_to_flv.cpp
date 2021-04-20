#include "mp4_to_flv.h"
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

void mp4_to_flv(const char* input_file, const char* output_file)
{
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

    av_dump_format(ofmt_ctx, 0, output_file, 0);

    if (ofmt_ctx->flags & AVFMT_NOFILE)
    {
        ret = avio_open(&ofmt_ctx->pb, output_file, AVIO_FLAG_WRITE);

    }

}


