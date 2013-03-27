/*
 *
 */
extern "C" {
#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>
//#include <avutil.h>
}
#include <pthread.h>
#include <sys/time.h>
#include "mediarecorder.h"
#include <iostream>
#include <string>
#include <map>


pa_mainloop_api * m_api;

static void get_server_info_callback(pa_context *c, const pa_server_info *i, void *userdata) {
    printf("Default sink: %s\n", i->default_sink_name);
    ((MediaRecorder*)userdata)->defaultsink = i->default_sink_name;
    m_api->quit(m_api,1);
}
static void get_sink_info_callback(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) {

    if ( is_last )
    {
        pa_operation_unref(pa_context_get_server_info(c, get_server_info_callback, userdata));
        return;
    }
    printf("Sink: %s - %s\n",i->description,i->name);
}
static void get_source_info_callback(pa_context *c, const pa_source_info *i, int is_last, void *userdata) {
    if ( is_last )
    {
        pa_operation_unref(pa_context_get_sink_info_list(c, get_sink_info_callback, userdata));
        return;
    }
    printf("Source: %s - name: %s\n",i->description,i->name);
    if ( i->monitor_of_sink != PA_INVALID_INDEX )
    {
        printf("[ISMONITOR] of %s\n",i->monitor_of_sink_name);
        ((MediaRecorder*)userdata)->monitorsources.insert(std::pair<std::string,std::string>(i->monitor_of_sink_name,i->name));
    }
}
static void context_state_callback(pa_context *c, void *userdata) {
    if ( pa_context_get_state(c) == PA_CONTEXT_READY )
    {
        pa_operation_unref(pa_context_get_source_info_list(c, get_source_info_callback, userdata));
    }
}


/*int main(int argc, char **argv) {

    pa_context* ctx;
    pa_mainloop * m = pa_mainloop_new();
    m_api = pa_mainloop_get_api(m);
    ctx = pa_context_new(m_api,"Rec1");
    if ( pa_context_connect(ctx,NULL,(pa_context_flags_t)0,NULL) < 0 )
        printf("Cannot connect to pulseaudio\n");
    int ret;
    pa_context_set_state_callback(ctx, context_state_callback, NULL);
    pa_mainloop_run(m,&ret);
    
    std::cout << "Use source: " << monitorsources[defaultsink] << std::endl;
    
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };
    pa_context_disconnect(ctx);
    int error;
    pa_simple * s = pa_simple_new(NULL,"GLCAP Record",PA_STREAM_RECORD,monitorsources[defaultsink].c_str(), "record", &ss, NULL,NULL , &error);
    if ( !s )
    {
        printf("Cannot create pa_simple\n");
    }
    FILE * f = fopen("out.raw","wb");
    char buf[1024];
    for (;;)
    {
        pa_simple_read(s,buf,sizeof(buf),&error);
        fwrite(buf,1024,1,f);
        
    }
    return 0;
}*/


double getcurrenttime2()
{
  struct timeval t;
  gettimeofday(&t,NULL);
  double ti = 0.0;
  ti = t.tv_sec;
  ti += t.tv_usec/1000000.0;
  return ti;
}
static AVFrame *alloc_picture(int pix_fmt, int width, int height)
{
    AVFrame *picture;
    uint8_t *picture_buf;
    int size;

    picture = avcodec_alloc_frame();

    if (!picture)
        return NULL;
    size        = avpicture_get_size(pix_fmt, width, height);
    picture_buf = av_malloc(size);
    if (!picture_buf) {
        av_free(picture);
        return NULL;
    }
    avpicture_fill((AVPicture *)picture, picture_buf,
                   pix_fmt, width, height);
    picture->width = width;
    picture->height = height;
    return picture;
}
#ifndef AV_CODEC_ID_MPEG4
#define AV_CODEC_ID_MPEG4 CODEC_ID_MPEG4
#endif

void MediaRecorder::RecordingThread()
{
    int error;
    while ( run )
    {

        printf("Read 1024 samples\n");
        short * buf = new short[1024];
        int ret = pa_simple_read(s,buf,1024*2,&error);
        if ( ret < 0 )
            printf("Error audio\n");
        pthread_mutex_lock(&sound_buffer_lock);
        sound_buffers.push_back(buf);
        pthread_mutex_unlock(&sound_buffer_lock);
    }
}


MediaRecorder::MediaRecorder(const char * outfile,int width, int height)
{
    /* INIT SOUND RECORDING */
    
    
    
    pa_context* pactx;
    pa_mainloop * m = pa_mainloop_new();
    m_api = pa_mainloop_get_api(m);
    pactx = pa_context_new(m_api,"Rec1");
    if ( pa_context_connect(pactx,NULL,(pa_context_flags_t)0,NULL) < 0 )
        printf("Cannot connect to pulseaudio\n");
    int ret;
    pa_context_set_state_callback(pactx, context_state_callback, this);
    pa_mainloop_run(m,&ret);
    std::cout << "Use source: " << monitorsources[defaultsink] << std::endl;
    
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };
    pa_context_disconnect(pactx);
    
    int error;
    s = pa_simple_new(NULL,"GLCAP Record",PA_STREAM_RECORD,monitorsources[defaultsink].c_str(), "record", &ss, NULL,NULL , &error);
    if ( !s )
    {
        printf("Cannot create pa_simple\n");
    }
    
    run = true;
    ready = false;
    firstframe = true;
    this->width = width;
    this->height = height;
    pthread_mutex_init(&encode_mutex,NULL);
    pthread_mutex_init(&sound_buffer_lock,NULL);
    pthread_cond_init(&encode_cond,NULL);
    pthread_create(&encode_thread,NULL,(void*(*)(void*))&MediaRecorder::EncodingThread,this);
    pthread_create(&record_sound_thread,NULL,(void*(*)(void*))&MediaRecorder::RecordingThread,this);

    av_log_set_level(AV_LOG_DEBUG);
    outCtx = avformat_alloc_context();

    outCtx->oformat = av_guess_format(NULL, outfile, NULL);
    snprintf(outCtx->filename, sizeof(outCtx->filename), "%s", outfile);
    codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    acodec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE);
    ctx = avcodec_alloc_context3(codec);
    actx = avcodec_alloc_context3(acodec);
    avcodec_get_context_defaults3(actx,acodec);
    //avcodec_get_context_defaults3(ctx,codec);
    ctx->width = width;
    ctx->height = height;
    ctx->bit_rate = 4000*1000;
    ctx->time_base.den = 1000.0;
    ctx->time_base.num = 1;
    ctx->thread_count = 4;
    ctx->qmin = 2;
    ctx->qmax = 31;
    ctx->b_sensitivity = 100;
    ctx->gop_size = 1;
    ctx->me_method = 1;
    ctx->global_quality = 100;
    ctx->lowres = 0;
    ctx->bit_rate_tolerance = 100000;
    actx->sample_fmt = AV_SAMPLE_FMT_S16;
    actx->sample_rate = 44100;
    actx->channels = 2;
    actx->time_base.den = 44100;
    actx->time_base.num = 1;
   /* ctx->compression_level = 0;
    ctx->trellis = 0;
    ctx->gop_size = 1; /* emit one intra frame every ten frames */
    /*ctx->me_pre_cmp = 0;
    ctx->me_cmp = 0;
    ctx->me_sub_cmp = 0;
    ctx->mb_cmp = 2;
    ctx->pre_dia_size = 0;
    ctx->dia_size = 1;

    ctx->quantizer_noise_shaping = 0; // qns=0
    ctx->noise_reduction = 0; // nr=0
    ctx->mb_decision = 0; // mbd=0 ("realtime" encoding)

    ctx->flags &= ~CODEC_FLAG_QPEL;
    ctx->flags &= ~CODEC_FLAG_4MV;
    ctx->trellis = 0;
    ctx->flags &= ~CODEC_FLAG_CBP_RD;
    ctx->flags &= ~CODEC_FLAG_QP_RD;
    ctx->flags &= ~CODEC_FLAG_MV0;*/
    //ctx->s
    ctx->pix_fmt = PIX_FMT_YUV420P;



    if (avcodec_open2(ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
    }
    if (avcodec_open2(actx, acodec, NULL) < 0) {
        fprintf(stderr, "Could not open audio codec\n");
    }
    AVStream* s = av_new_stream(outCtx,0);
    s->codec = ctx;
    s->r_frame_rate.den = 1000.0;
    s->r_frame_rate.num = 1;
    AVStream* as = av_new_stream(outCtx,1);
    as->codec = actx;
    
    picture = alloc_picture(PIX_FMT_YUV420P, ctx->width, ctx->height);
    if (!picture) {
        fprintf(stderr, "Could not allocate picture\n");
        exit(1);
    }
    tmp_picture = NULL;

    tmp_picture = alloc_picture(PIX_FMT_RGBA, ctx->width, ctx->height);
    if (!tmp_picture) {
        fprintf(stderr, "Could not allocate temporary picture\n");
        exit(1);
    }

    img_convert_ctx = sws_getContext(ctx->width, ctx->height,
                                            PIX_FMT_RGBA,
                                            ctx->width, ctx->height,
                                            PIX_FMT_YUV420P,
                                            SWS_FAST_BILINEAR , NULL, NULL, NULL);
    if (img_convert_ctx == NULL) {
        fprintf(stderr,
                "Cannot initialize the conversion context\n");
        exit(1);
    }
    av_dump_format(outCtx, 0, outfile, 1);
    avio_open2(&outCtx->pb, outfile, AVIO_FLAG_WRITE,NULL,NULL);
    avformat_write_header(outCtx,NULL);
}

MediaRecorder::~MediaRecorder()
{
    run = false;
    ready = false;
    pthread_cond_broadcast(&encode_cond);
    printf("Joining thread..\n");
    pthread_join(encode_thread,NULL);
    printf("Joining recording thread..\n");
    pthread_join(record_sound_thread,NULL);
    printf("Done\n");
    av_write_trailer(outCtx);
    av_free(picture);
    avformat_free_context(outCtx);
    pa_simple_free(s);
}
int fcount = 0;
void MediaRecorder::AppendFrame(float time, int width, int height, char* data)
{
    printf("AppendFrame\n");
    this->time = getcurrenttime2();
    if ( firstframe )
    {
        starttime = getcurrenttime2();
        firstframe = false;
    }
    this->height = height;
    this->width = width;
    m_data = data;
    pthread_cond_broadcast(&encode_cond);

   /*int i = 0;
    unsigned int numpixels = width * height;
    unsigned int ui = numpixels;
    unsigned int vi = numpixels + numpixels / 4;
    for ( int j = 0; j < height; j++ )
    {
        for ( int k = 0; k < width; k++ )
        {
            int sR = data[i*4+0];
            int sG = data[i*4+1];
            int sB = data[i*4+2];
            picture->data[0][i] = ( (66*sR + 129*sG + 25*sB + 128) >> 8) + 16;
            if (0 == j%2 && 0 == k%2)
            {
                picture->data[0][ui++] = ( (-38*sR - 74*sG + 112*sB + 128) >> 8) + 128;
                picture->data[0][vi++] = ( (112*sR - 94*sG - 18*sB + 128) >> 8) + 128;
            }
            i++;

        }
    }*/


  // printf("End flip %f\n",(float)getcurrenttime2());

    //memcpy(tmp_picture->data[0],data,width*height*4);

}

void MediaRecorder::EncodingThread()
{
    while ( run )
    {
        printf("Encode thread ready\n");
        ready = true;
        pthread_cond_wait(&encode_cond,&encode_mutex);
        ready = false;
        if (!run)
        {
            printf("Encoding finished\n");
            break;
        }
        for ( int y = 0; y < height; y++ )
        {
            /*for ( int x = 0; x < width; x++ )
            {*/
            char r,g,b;
            int oldindex = (y*width);
            int newindex = ((height-1-y)*width);
            memcpy(&tmp_picture->data[0][(newindex)*4],&m_data[oldindex*4],width*4);
            /* r = data[oldindex*4+0];
            g = data[oldindex*4+1];
            b = data[oldindex*4+2];
            tmp_picture->data[0][(newindex)*4+0] = r;
            tmp_picture->data[0][(newindex)*4+1] = g;
            tmp_picture->data[0][(newindex)*4+2] = b; */
        // }

        }
        sws_scale(img_convert_ctx,tmp_picture->data,tmp_picture->linesize,0,height,picture->data,picture->linesize);


        AVPacket p;

        av_init_packet(&p);
        p.data = NULL;
        p.size = 0;
        picture->pts = int64_t((time-starttime)*1000.0);
       // picture->pts = time*30.0;
        int got_frame;
        printf("%p %p\n",ctx, picture);
        if(avcodec_encode_video2(ctx, &p, picture, &got_frame) < 0) return;
        if(got_frame)
        {

            // outContainer is "mp4"
            av_interleaved_write_frame(outCtx, &p);

            av_free_packet(&p);
        }
        printf("End enc frame %f\n",(float)getcurrenttime2());
        pthread_mutex_lock(&sound_buffer_lock);
       AVFrame * aframe = avcodec_alloc_frame();
        
        
        while ( sound_buffers.size() > 0 )
        {
            printf("sound_buffers.size() = %d\n",sound_buffers.size());
            short * buf = sound_buffers.front();
            sound_buffers.pop_front();
            avcodec_get_frame_defaults(aframe);
            avcodec_fill_audio_frame(aframe,actx->channels,actx->sample_fmt,(char*)buf,1024*2,0 );
            av_init_packet(&p);
            p.data = NULL;
            p.size = 0;
            avcodec_encode_audio2(actx,&p,aframe,&got_frame);
            if ( got_frame )
            {
                p.stream_index = 1;
                p.flags |= AV_PKT_FLAG_KEY;
                av_interleaved_write_frame(outCtx,&p);
                av_free_packet(&p);
            }
            //printf("Consumed 1024 samples\n");
            delete[] buf;
        }
        avcodec_free_frame(&aframe);
        pthread_mutex_unlock(&sound_buffer_lock);
    }
}

bool MediaRecorder::isReady()
{
  return ready;
}
