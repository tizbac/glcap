/*
 *
 */
extern "C" {
#include <avcodec.h>
#include <avformat.h>
#include <swscale.h>
#include <avutil.h>
}
#include <sys/time.h>
#include "mediarecorder.h"
float getcurrenttime2()
{
  struct timeval t;
  gettimeofday(&t,NULL);
  float ti = 0.0;
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


MediaRecorder::MediaRecorder(const char * outfile,int width, int height)
{
    av_log_set_level(AV_LOG_VERBOSE); 
    outCtx = avformat_alloc_context();
    outCtx->oformat = av_guess_format(NULL, outfile, NULL);
    snprintf(outCtx->filename, sizeof(outCtx->filename), "%s", outfile);
    codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    ctx = avcodec_alloc_context3(codec);
    avcodec_get_context_defaults3(ctx,codec);
    ctx->width = width;
    ctx->height = height;
    ctx->bit_rate = 1000000;
   ctx->time_base.den = 30;
    ctx->time_base.num = 1;
    ctx->trellis = 0;
    ctx->me_pre_cmp = 0;
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
    ctx->flags &= ~CODEC_FLAG_MV0;
    ctx->pix_fmt = PIX_FMT_YUV420P;
    AVStream* s = av_new_stream(outCtx,0);
    s->codec = ctx;
    s->r_frame_rate.den = 30;
    s->r_frame_rate.num = 1;


    if (avcodec_open2(ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
    } 
    
    picture = alloc_picture(PIX_FMT_YUV420P, ctx->width, ctx->height);
    if (!picture) {
        fprintf(stderr, "Could not allocate picture\n");
        exit(1);
    }
    tmp_picture = NULL;

    tmp_picture = alloc_picture(PIX_FMT_RGB0, ctx->width, ctx->height);
    if (!tmp_picture) {
        fprintf(stderr, "Could not allocate temporary picture\n");
        exit(1);
    }
    
    img_convert_ctx = sws_getContext(ctx->width, ctx->height,
                                            PIX_FMT_RGBA,
                                            ctx->width, ctx->height,
                                            ctx->pix_fmt,
                                            SWS_BICUBIC, NULL, NULL, NULL);
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
    av_write_trailer(outCtx);
    av_free(picture);
    avformat_free_context(outCtx);
}
int fcount = 0;
void MediaRecorder::AppendFrame(float time, int width, int height, char* data)
{
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
   printf("Start flip %f\n",(float)getcurrenttime2());
   
    for ( int y = 0; y < height; y++ )
    {
        /*for ( int x = 0; x < width; x++ )
        {*/
           char r,g,b;
           int oldindex = (y*width);
           int newindex = ((height-1-y)*width);
           memcpy(&tmp_picture->data[0][(newindex)*4],&data[oldindex*4],width*4);
          /* r = data[oldindex*4+0];
           g = data[oldindex*4+1];
           b = data[oldindex*4+2];
           tmp_picture->data[0][(newindex)*4+0] = r;
           tmp_picture->data[0][(newindex)*4+1] = g;
           tmp_picture->data[0][(newindex)*4+2] = b; */
     // }
       
   }
  // printf("End flip %f\n",(float)getcurrenttime2());
    
    //memcpy(tmp_picture->data[0],data,width*height*4);
    sws_scale(img_convert_ctx,tmp_picture->data,tmp_picture->linesize,0,height,picture->data,picture->linesize);

    
    AVPacket p;
  
    av_init_packet(&p);
    p.data = NULL;
    p.size = 0;
    picture->pts = ++fcount;
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
}
