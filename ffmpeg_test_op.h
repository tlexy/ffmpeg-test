#ifndef FFMPEG_TEST_OP_H
#define FFMPEG_TEST_OP_H

#include <iostream>
extern "C"{
#include "libavutil/avutil.h"
#include "libavformat/avio.h"
#include "libavformat/avformat.h"
}

#define log_i(fmt, ...) av_log(NULL, AV_LOG_INFO, fmt, ##__VA_ARGS__)
#define log_w(fmt, ...) av_log(NULL, AV_LOG_WARNING, fmt, ##__VA_ARGS__)
#define log_e(fmt, ...) av_log(NULL, AV_LOG_ERROR, fmt, ##__VA_ARGS__)

int get_audio_obj_type(int aactype);
int get_sample_rate_index(int freq, int aactype);
int get_channel_config(int channels, int aactype);
void adts_header(char *szAdtsHeader, int dataLen, int aactype, int frequency, int channels);

void extra_audio(const char* input_file, const char* output_file);


int h264_mp4toannexb(AVFormatContext *fmt_ctx, AVPacket *in, FILE *dst_fd);
int h264_extradata_to_annexb(const uint8_t *codec_extradata, const int codec_extradata_size, AVPacket *out_extradata, int padding);
int alloc_and_copy(AVPacket *out,
                          const uint8_t *sps_pps, uint32_t sps_pps_size,
                          const uint8_t *in, uint32_t in_size);

void extra_video(const char* input_file, const char* output_file);

#endif
