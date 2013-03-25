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
#include <pthread.h>
class MediaRecorder
{
public:
    MediaRecorder(const char* outfile, int width, int height);
    ~MediaRecorder();
    void AppendFrame(float time,int width, int height, char * data);
    bool isReady();
    void EncodingThread();
private:
    AVFrame* tmp_picture;
    AVFrame* picture;
    SwsContext* img_convert_ctx;
    AVCodec* codec;
    AVCodecContext* ctx;
    AVFormatContext* outCtx;
    pthread_t encode_thread;
    pthread_mutex_t encode_mutex;
    pthread_cond_t encode_cond;
    bool ready;
    bool run;
    int height;
    double time;
    int width;
    char* m_data;
    bool firstframe;
    double starttime;
};

#endif // MEDIARECORDER_H
