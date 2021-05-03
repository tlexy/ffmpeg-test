#include "ffmpeg_test_op.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sutil.h"

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

int get_audio_obj_type(int aactype)
{
    //AAC HE V2 = AAC LC + SBR + PS
    //AAV HE = AAC LC + SBR
    //所以无论是 AAC_HEv2 还是 AAC_HE 都是 AAC_LC
    switch(aactype)
    {
        case 0:
        case 2:
        case 3:
            return aactype+1;
        case 1:
        case 4:
        case 28:
            return 2;
        default:
            return 2;

    }
}

int get_sample_rate_index(int freq, int aactype)
{

    int i = 0;
    int freq_arr[13] = {
        96000, 88200, 64000, 48000, 44100, 32000,
        24000, 22050, 16000, 12000, 11025, 8000, 7350
    };

    //如果是 AAC HEv2 或 AAC HE, 则频率减半
    if(aactype == 28 || aactype == 4)
    {
        freq /= 2;
    }

    for(i=0; i< 13; i++)
    {
        if(freq == freq_arr[i])
        {
            return i;
        }
    }
    return 4;//默认是44100
}

int get_channel_config(int channels, int aactype)
{
    //如果是 AAC HEv2 通道数减半
    if(aactype == 28)
    {
        return (channels / 2);
    }
    return channels;
}

void adts_header(char *szAdtsHeader, int dataLen, int aactype, int frequency, int channels)
{

    int audio_object_type = get_audio_obj_type(aactype);
    int sampling_frequency_index = get_sample_rate_index(frequency, aactype);
    int channel_config = get_channel_config(channels, aactype);

    printf("aot=%d, freq_index=%d, channel=%d\n", audio_object_type, sampling_frequency_index, channel_config);

    int adtsLen = dataLen + 7;

    szAdtsHeader[0] = 0xff;         //syncword:0xfff                          高8bits
    szAdtsHeader[1] = 0xf0;         //syncword:0xfff                          低4bits
    szAdtsHeader[1] |= (0 << 3);    //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
    szAdtsHeader[1] |= (0 << 1);    //Layer:0                                 2bits
    szAdtsHeader[1] |= 1;           //protection absent:1                     1bit

    szAdtsHeader[2] = (audio_object_type - 1)<<6;            //profile:audio_object_type - 1                      2bits
    szAdtsHeader[2] |= (sampling_frequency_index & 0x0f)<<2; //sampling frequency index:sampling_frequency_index  4bits
    szAdtsHeader[2] |= (0 << 1);                             //private bit:0                                      1bit
    szAdtsHeader[2] |= (channel_config & 0x04)>>2;           //channel configuration:channel_config               高1bit

    szAdtsHeader[3] = (channel_config & 0x03)<<6;     //channel configuration:channel_config      低2bits
    szAdtsHeader[3] |= (0 << 5);                      //original：0                               1bit
    szAdtsHeader[3] |= (0 << 4);                      //home：0                                   1bit
    szAdtsHeader[3] |= (0 << 3);                      //copyright id bit：0                       1bit
    szAdtsHeader[3] |= (0 << 2);                      //copyright id start：0                     1bit
    szAdtsHeader[3] |= ((adtsLen & 0x1800) >> 11);           //frame length：value   高2bits

    szAdtsHeader[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    中间8bits
    szAdtsHeader[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    低3bits
    szAdtsHeader[5] |= 0x1f;                                 //buffer fullness:0x7ff 高5bits
    szAdtsHeader[6] = 0xfc;
}

void extra_audio(const char* input_file, const char* output_file)
{
    if (input_file == NULL || output_file == NULL)
    {
        log_w("input file or output file is null\n");
        return;
    }
    FILE* output_fd = fopen(output_file, "wb");
    if (!output_fd)
    {
        log_w("open output file failed: %s", output_file);
        return;
    }

    char error_str[1024];
    AVFormatContext* fmt_ctx = NULL;
    int ret = 0;
    if ((ret = avformat_open_input(&fmt_ctx, input_file, NULL, NULL)) < 0)
    {
        log_e("open input file failed: %s\n", input_file);
        fclose(output_fd);
        return;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    {
        av_strerror(ret, error_str, sizeof(error_str));
        log_w("failed to read media info: %s, %d, %s\n", input_file, ret, error_str);
        fclose(output_fd);
        avformat_close_input(&fmt_ctx);
        return;
    }
    av_dump_format(fmt_ctx, 0, input_file, 0);

    AVFrame* frame = NULL;
    frame = av_frame_alloc();
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    int audio_index = -1;
    audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index < 0)
    {
        log_i("could't find %s stream in %s\n", av_get_media_type_string(AVMEDIA_TYPE_AUDIO), input_file);
        fclose(output_fd);
        avformat_close_input(&fmt_ctx);
        return;
    }
    log_i("audio index: %d\n", audio_index);
    //查找音频信息
    int aac_type = -1;
    int channels = -1;
    int sample_rate = -1;
    for (int i = 0; i < fmt_ctx->nb_streams; ++i)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
                && fmt_ctx->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC)

        {
            aac_type = fmt_ctx->streams[i]->codecpar->profile;
            channels = fmt_ctx->streams[i]->codecpar->channels;
            sample_rate = fmt_ctx->streams[i]->codecpar->sample_rate;
            break;
        }
    }
    if (aac_type == -1 || channels == -1 || sample_rate == -1)
    {
        log_w("could't find aac stream in %s\n", input_file);
        fclose(output_fd);
        avformat_close_input(&fmt_ctx);
        return;
    }
    char adts[7] = {0};
    while(av_read_frame(fmt_ctx, &pkt) >= 0)
    {
        if (pkt.stream_index == audio_index)
        {
            memset(adts, 0x0, sizeof(adts));
            adts_header(adts, pkt.size, aac_type, sample_rate, channels);
            fwrite(adts, 1, sizeof(adts), output_fd);

            int len = fwrite(pkt.data, 1, pkt.size, output_fd);
            if (len != pkt.size)
            {
                log_w("write adts data may be error: %d-%d\n", len, pkt.size);
            }
        }
        av_packet_unref(&pkt);
    }
    fclose(output_fd);
    avformat_close_input(&fmt_ctx);

}

//////////////////////////-----------------------
///
///
#ifndef AV_WB32
#define AV_WB32(p, val) do {                 \
        uint32_t d = (val);                     \
        ((uint8_t*)(p))[3] = (d);               \
        ((uint8_t*)(p))[2] = (d)>>8;            \
        ((uint8_t*)(p))[1] = (d)>>16;           \
        ((uint8_t*)(p))[0] = (d)>>24;           \
    } while(0)
#endif

#ifndef AV_RB16
#define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif

//增加特征码？0x001?0x0001
int alloc_and_copy(AVPacket *out,
                          const uint8_t *sps_pps, uint32_t sps_pps_size,
                          const uint8_t *in, uint32_t in_size)
{
    //当offset大于0时，说明带有sps和pps数据，要将sps和pps数据一些写入
    uint32_t offset         = out->size;
    uint8_t nal_header_size = offset ? 3 : 4;
    int err;

    err = av_grow_packet(out, sps_pps_size + in_size + nal_header_size);
    if (err < 0)
        return err;

    if (sps_pps)
        memcpy(out->data + offset, sps_pps, sps_pps_size);
    memcpy(out->data + sps_pps_size + nal_header_size + offset, in, in_size);
    if (!offset) {
        AV_WB32(out->data + sps_pps_size, 1);
    } else {
        (out->data + offset + sps_pps_size)[0] =
        (out->data + offset + sps_pps_size)[1] = 0;
        (out->data + offset + sps_pps_size)[2] = 1;
    }

    return 0;
}

int h264_extradata_to_annexb(const uint8_t *codec_extradata, const int codec_extradata_size, AVPacket *out_extradata, int padding)
{
    uint16_t unit_size  = 0;
    uint64_t total_size = 0;
    uint8_t *out        = NULL;
    uint8_t unit_nb     = 0;
    uint8_t sps_done    = 0;
    uint8_t sps_seen    = 0;
    uint8_t pps_seen    = 0;
    uint8_t sps_offset  = 0;
    uint8_t pps_offset  = 0;

    const uint8_t *extradata = codec_extradata + 4; //extradata存放数据的格式如上，前4个字节没用，所以将其舍弃
    static const uint8_t nalu_header[4] = { 0, 0, 0, 1 }; //每个H264裸数据都是以 0001 4个字节为开头的

    extradata++;//跳过一个字节，这个也没用

    sps_offset = pps_offset = -1;

    /* retrieve sps and pps unit(s) */
    unit_nb = *extradata++ & 0x1f;
    if (!unit_nb) {
           goto pps;
       }else {
           log_i("sps num: %d\n", unit_nb);
           sps_offset = 0;
           sps_seen = 1;
       }

    //尽管是一个while，但多数情况下，unit_nb为1
    while(unit_nb--) {
            int err;

            unit_size   = AV_RB16(extradata);
            total_size += unit_size + 4; //加上4字节的h264 header, 即 0001
            if (total_size > INT_MAX - padding) {
                av_log(NULL, AV_LOG_ERROR,
                       "Too big extradata size, corrupted stream or invalid MP4/AVCC bitstream\n");
                av_free(out);
                return AVERROR(EINVAL);
            }

            //2:表示上面 unit_size 的所占字结数
            //这句的意思是 extradata 所指的地址，加两个字节，再加 unit 的大小所指向的地址
            //是否超过了能访问的有效地址空间
            if (extradata + 2 + unit_size > codec_extradata + codec_extradata_size) {
                av_log(NULL, AV_LOG_ERROR, "Packet header is not contained in global extradata, "
                       "corrupted stream or invalid MP4/AVCC bitstream\n");
                av_free(out);
                return AVERROR(EINVAL);
            }

            //分配存放 SPS 的空间
            if ((err = av_reallocp(&out, total_size + padding)) < 0)
                return err;

            memcpy(out + total_size - unit_size - 4, nalu_header, 4);
            //拷贝sps数据
            memcpy(out + total_size - unit_size, extradata + 2, unit_size);
            extradata += 2 + unit_size;
    pps:
            //当 SPS 处理完后，开始处理 PPS
             if (!unit_nb && !sps_done++) {
                 unit_nb = *extradata++; /* number of pps unit(s) */
                 log_i("pps num: %d\n", unit_nb);
                 if (unit_nb) {
                     pps_offset = total_size;
                     pps_seen = 1;
                 }
             }
         }

    if (out){
            memset(out + total_size, 0, padding);
        }

        if (!sps_seen)
            av_log(NULL, AV_LOG_WARNING,
                   "Warning: SPS NALU missing or invalid. "
                   "The resulting stream may not play.\n");

        if (!pps_seen)
            av_log(NULL, AV_LOG_WARNING,
                   "Warning: PPS NALU missing or invalid. "
                   "The resulting stream may not play.\n");

        out_extradata->data      = out;
        out_extradata->size      = total_size;

    return 0;
}


int h264_mp4toannexb(AVFormatContext* fmt_ctx, AVPacket* in, FILE* dst_fd)
{
    AVPacket *out = NULL;
    AVPacket spspps_pkt;

    int len;
    uint8_t unit_type;
    int32_t nal_size;
    uint32_t cumul_size    = 0;
    const uint8_t *buf;
    const uint8_t *buf_end;
    int            buf_size;
    int ret = 0, i;

    out = av_packet_alloc();

    buf      = in->data;
    buf_size = in->size;
    buf_end  = in->data + in->size;

    do {
        ret= AVERROR(EINVAL);
        //因为每个视频帧的前 4 个字节是视频帧的长度
        //如果buf中的数据都不能满足4字节，所以后面就没有必要再进行处理了
        if (buf + 4 > buf_end)
            goto fail;

        //将前四字节转换成整型,也就是取出视频帧长度
        for (nal_size = 0, i = 0; i<4; i++)
            nal_size = (nal_size << 8) | buf[i];

        buf += 4; //跳过4字节（也就是视频帧长度），从而指向真正的视频帧数据
        unit_type = *buf & 0x1f; //视频帧的第一个字节里有NAL TYPE

        //如果视频帧长度大于从 AVPacket 中读到的数据大小，说明这个数据包肯定是出错了
        if (nal_size > buf_end - buf || nal_size < 0)
            goto fail;

        /* prepend only to the first type 5 NAL unit of an IDR picture, if no sps/pps are already present */
        if (unit_type == 5)
        {

            //在每个I帧之前都加 SPS/PPS
            h264_extradata_to_annexb( fmt_ctx->streams[in->stream_index]->codecpar->extradata,
                                      fmt_ctx->streams[in->stream_index]->codecpar->extradata_size,
                                      &spspps_pkt,
                                      AV_INPUT_BUFFER_PADDING_SIZE);

            //写帧数据到out中，并带上sps/pps数据
            if ((ret=alloc_and_copy(out,
                               spspps_pkt.data, spspps_pkt.size,
                               buf, nal_size)) < 0)
                goto fail;
        }
        else
        {
            //写帧数据到out中
            if ((ret=alloc_and_copy(out, NULL, 0, buf, nal_size)) < 0)
                goto fail;
        }


        len = fwrite( out->data, 1, out->size, dst_fd);
        if(len != out->size){
            av_log(NULL, AV_LOG_DEBUG, "warning, length of writed data isn't equal pkt.size(%d, %d)\n",
                    len,
                    out->size);
        }
        fflush(dst_fd);

next_nal:
        buf        += nal_size;
        cumul_size += nal_size + 4;//s->length_size;
        //使用while是因为一个packet可能包含多个帧
    } while (cumul_size < buf_size);

fail:
    av_packet_free(&out);

    return ret;
}

//可以通过ffmpeg中的filter来实现
//h264_mp4toannexb_init + h264_mp4toannexb_filter
void extra_video(const char* input_file, const char* output_file)
{
    if (input_file == NULL || output_file == NULL)
    {
        log_w("input file or output file is null\n");
        return;
    }
    FILE* output_fd = fopen(output_file, "wb");
    if (!output_fd)
    {
        log_w("open output file failed: %s", output_file);
        return;
    }

    char error_str[1024];
    AVFormatContext* fmt_ctx = NULL;
    int ret = 0;
    if ((ret = avformat_open_input(&fmt_ctx, input_file, NULL, NULL)) < 0)
    {
        log_e("open input file failed: %s\n", input_file);
        fclose(output_fd);
        return;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    {
        av_strerror(ret, error_str, sizeof(error_str));
        log_w("failed to read media info: %s, %d, %s\n", input_file, ret, error_str);
        fclose(output_fd);
        avformat_close_input(&fmt_ctx);
        return;
    }
    av_dump_format(fmt_ctx, 0, input_file, 0);

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    int video_index = -1;
    video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index < 0)
    {
        log_i("could't find %s stream in %s\n", av_get_media_type_string(AVMEDIA_TYPE_AUDIO), input_file);
        fclose(output_fd);
        avformat_close_input(&fmt_ctx);
        return;
    }
    log_i("video index: %d\n", video_index);


    while(av_read_frame(fmt_ctx, &pkt) >= 0)
    {
        if (pkt.stream_index == video_index)
        {
            h264_mp4toannexb(fmt_ctx, &pkt, output_fd);
        }
        av_packet_unref(&pkt);
    }
    fclose(output_fd);
    avformat_close_input(&fmt_ctx);
}

void file_to_image(const char* ifile, int st, int et, const char* prefix)
{}


void pcm2aac(const char* pcm_file, int channals, int rates, int layout, const char* ofile)
{
    AVCodec* codec;
    AVFrame* frame;
    AVPacket pkt;

    codec = avcodec_find_encoder_by_name("libfdk_aac");
    AVCodecContext* ctx = avcodec_alloc_context3(codec);

    ctx->bit_rate = 192000;
    ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    ctx->sample_rate = rates;
    ctx->channel_layout = layout;
    ctx->channels = channals;

    avcodec_open2(ctx, codec, NULL);
    FILE* file = fopen(ofile, "wb");

    frame = av_frame_alloc();

    frame->nb_samples = ctx->frame_size;
    frame->format = ctx->sample_fmt;
    frame->channel_layout = ctx->channel_layout;

    int ret = av_frame_get_buffer(frame, 0);
    std::string pcm_buff;
    int psize = SUtil::readBinaryFile(pcm_file, pcm_buff);
    printf("psize: %d\n", psize);
    int pos = 0;
    int flag = 0;
    for (int i = 0; i < pcm_buff.size()/2/2; ++i)
    {
        printf("encode: %d-%d\n", i, frame->nb_samples);
        av_init_packet(&pkt);
        av_frame_make_writable(frame);
        memcpy(frame->data, pcm_buff.c_str() + pos, frame->nb_samples*2*2);

        flag = 0;
        ret = avcodec_encode_audio2(ctx, &pkt, frame, &flag);
        if (ret < 0)
        {
            printf("encode error.\n");
            return;
        }
        if (flag)
        {
            fwrite(pkt.data, 1, pkt.size, file);
            av_packet_unref(&pkt);
        }
    }

    for(flag = 1; flag; 1)
    {
        flag = 0;
        ret = avcodec_encode_audio2(ctx, &pkt, NULL, &flag);
        if (ret < 0)
        {
            printf("encode error 2.\n");
            return;
        }
        if (flag)
        {
            fwrite(pkt.data, 1, pkt.size, file);
            av_packet_unref(&pkt);
        }
    }

    fclose(file);
    printf("close file\n");
    av_frame_free(&frame);
    avcodec_free_context(&ctx);

}

/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * audio encoding with libavcodec API example.
 *
 * @example encode_audio.c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavcodec/avcodec.h>

#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

/* check that a given sample format is supported by the encoder */
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;

    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}

/* just pick the highest supported samplerate */
static int select_sample_rate(const AVCodec *codec)
{
    const int *p;
    int best_samplerate = 0;

    if (!codec->supported_samplerates)
        return 44100;

    p = codec->supported_samplerates;
    while (*p) {
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
            best_samplerate = *p;
        p++;
    }
    return best_samplerate;
}

/* select layout with the highest channel count */
static int select_channel_layout(const AVCodec *codec)
{
    const uint64_t *p;
    uint64_t best_ch_layout = 0;
    int best_nb_channels   = 0;

    if (!codec->channel_layouts)
        return AV_CH_LAYOUT_STEREO;

    p = codec->channel_layouts;
    while (*p) {
        int nb_channels = av_get_channel_layout_nb_channels(*p);

        if (nb_channels > best_nb_channels) {
            best_ch_layout    = *p;
            best_nb_channels = nb_channels;
        }
        p++;
    }
    return best_ch_layout;
}

int get_pcm_data(int byte_depth, int channels, int num, const std::string& buff, int pos)
{
    int count = byte_depth*num*channels;
    if (buff.size() >= pos + count)
    {
        return pos;
    }
    return -1;
}

static void encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt,
                   FILE *output)
{
    int ret;

    /* send the frame for encoding */
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        exit(1);
    }

    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            exit(1);
        }
        printf("write data.\n");
        fwrite(pkt->data, 1, pkt->size, output);
        av_packet_unref(pkt);
    }
}

void pcm2aac2(const char* pcm_path, int channals, int bit_rates, int layout, const char* ofile)
{
    const char *filename;
    const AVCodec *codec;
    AVCodecContext *c= NULL;
    AVFrame *frame;
    AVPacket *pkt;
    int i, j, k, ret;
    FILE *f;
    uint16_t *samples;
    float t, tincr;

    /* find the MP2 encoder */
    //codec = avcodec_find_encoder(AV_CODEC_ID_MP2);//AV_CODEC_ID_MP2
    codec = avcodec_find_encoder_by_name("libfdk_aac");
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        return;
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        return;
    }

    /* put sample parameters */
    c->bit_rate = bit_rates;//比特率 64K？128K？192K？

    /* check that the encoder supports s16 pcm input */
    c->sample_fmt = AV_SAMPLE_FMT_S16;
    if (!check_sample_fmt(codec, c->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
                av_get_sample_fmt_name(c->sample_fmt));
        return;
    }

    /* select other audio parameters supported by the encoder */
    c->sample_rate    = 44100;//select_sample_rate(codec);
    c->channel_layout = layout;//select_channel_layout(codec);
    c->channels       = channals;//av_get_channel_layout_nb_channels(c->channel_layout);

    printf("before open frame size: %d\n", c->frame_size);
    /* open it */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return;
    }

    f = fopen(ofile, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", ofile);
        return;
    }

    /* packet for holding encoded output */
    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "could not allocate the packet\n");
        return;
    }

    /* frame containing input raw audio */
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        return;
    }

    printf("after open frame size: %d\n", c->frame_size);
    frame->nb_samples     = c->frame_size;
    frame->format         = c->sample_fmt;
    frame->channel_layout = c->channel_layout;

    /* allocate the data buffers */
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        return;
    }


    /* encode a single tone sound */
    //t = 0;
    //tincr = 2 * M_PI * 440.0 / c->sample_rate;
    //for (i = 0; i < 200; i++) {
    //    /* make sure the frame is writable -- makes a copy if the encoder
    //     * kept a reference internally */
    //    ret = av_frame_make_writable(frame);
    //    if (ret < 0)
    //        exit(1);
    //    samples = (uint16_t*)frame->data[0];

    //    for (j = 0; j < c->frame_size; j++) {
    //        samples[2*j] = (int)(sin(t) * 10000);

    //        for (k = 1; k < c->channels; k++)
    //            samples[2*j + k] = samples[2*j];
    //        t += tincr;
    //    }
    //    encode(c, frame, pkt, f);
    //}

    int pos = 0;
    std::string pcm_buff;
    int size = SUtil::readBinaryFile(pcm_path, pcm_buff);
    if (size <= 0)
    {
        printf("input file is zero.\n");
        return;
    }
    while (pos >= 0)
    {
        printf("before get data.\n");
        ret = av_frame_make_writable(frame);
        pos = get_pcm_data(sizeof(uint16_t), channals,  c->frame_size, pcm_buff, pos);
        printf("pos: %d\n", pos);
        if (pos > -1)
        {
            memcpy(frame->data[0], pcm_buff.c_str() + pos, c->frame_size*sizeof(uint16_t)*channals);
            encode(c, frame, pkt, f);
            pos += c->frame_size * sizeof(uint16_t)*channals;
        }
    }
    printf("flush data.\n");
    /* flush the encoder */
    encode(c, NULL, pkt, f);

    fclose(f);

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&c);

    return;
}

