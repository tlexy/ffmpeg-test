QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
    ffmpeg_test_op.cpp \
    mp4_to_flv.cpp \
    sutil.cpp \
    common_func.cpp

INCLUDEPATH += \
    $$PWD/../ffmpeg/include

LIBS += $$PWD/../ffmpeg/lib/avformat-58.lib \
        $$PWD/../ffmpeg/lib/avcodec-58.lib \
        $$PWD/../ffmpeg/lib/avdevice-58.lib \
        $$PWD/../ffmpeg/lib/avfilter-7.lib \
        $$PWD/../ffmpeg/lib/avutil-56.lib \
        $$PWD/../ffmpeg/lib/postproc-55.lib \
        $$PWD/../ffmpeg/lib/swresample-3.lib \
        $$PWD/../ffmpeg/lib/swscale-5.lib

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    ffmpeg_test_op.h \
    mp4_to_flv.h \
    sutil.h \
    common_func.h
