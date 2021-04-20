#include "ffmpeg_test_op.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
