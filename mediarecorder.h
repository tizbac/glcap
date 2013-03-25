/*
 *
 */

#ifndef MEDIARECORDER_H
#define MEDIARECORDER_H
extern "C" {
#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>
}
class MediaRecorder
{
public:
    MediaRecorder(const char* outfile, int width, int height);
    ~MediaRecorder();
    void AppendFrame(float time,int width, int height, char * data);
private:
    AVFrame* tmp_picture;
    AVFrame* picture;
    SwsContext* img_convert_ctx;
    AVCodec* codec;
    AVCodecContext* ctx;
    AVFormatContext* outCtx;


};

#endif // MEDIARECORDER_H
