#include <QCoreApplication>
#include <iostream>
extern "C"{
#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
}
#include "ffmpeg_test_op.h"
#include "mp4_to_flv.h"
#include "common_func.h"

void init()
{
    av_log_set_level(AV_LOG_DEBUG);
    //av_register_all();
}

void log_test()
{
    av_log_set_level(AV_LOG_DEBUG);

    av_log(NULL, AV_LOG_DEBUG, "this is a test: %d\n", 123);
    av_log(NULL, AV_LOG_ERROR, "this is a error: %d\n", 555);
}

void dump_test()
{
    av_log_set_level(AV_LOG_DEBUG);

    AVFormatContext* fmt_ctx = NULL;
    //av_register_all();

    const char* file_name = "G:\\video\\test1.mp4";
    int ret = avformat_open_input(&fmt_ctx, file_name, NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "open file failed: %s", file_name);
        return;
    }
    av_dump_format(fmt_ctx, 0, file_name, 0);
    avformat_close_input(&fmt_ctx);
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    std::cout << "ffmpeg version: " << av_version_info() << std::endl;

    init();
    //log_test();
    //dump_test();

    //extra_audio("G:\\video\\test1.mp4", "G:\\video\\test1.aac");
    //extra_video("G:\\video\\test1.mp4", "G:\\video\\test1.h264");
    //mp4_to_flv("G:\\video\\test1.mp4", "G:\\video\\test3.flv");
    //cut_video("G:\\video\\test1.mp4", 5, 50, "G:\\video\\test1_550.mp4");

    //pcm2aac("G:\\video\\u9001645_ermeng_B.pcm", 2, 44100, AV_CH_LAYOUT_STEREO, "G:\\video\\out.aac");
    //pcm2aac2("G:\\video\\u9001645_ermeng_A.pcm", 2, 192000, AV_CH_LAYOUT_STEREO, "G:\\video\\ermeng_A.aac");

    decode_audio_to_raw("G:\\video\\test1.mp4");
    return a.exec();
}
