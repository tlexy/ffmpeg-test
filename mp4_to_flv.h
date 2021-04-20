#ifndef FFMPEG_MP4_TO_FLV_H
#define FFMPEG_MP4_TO_FLV_H

#include <iostream>
extern "C"{
#include "libavformat/avformat.h"
#include "libavutil/timestamp.h"
}

void mp4_to_flv(const char* input_file, const char* output_file);

#endif
