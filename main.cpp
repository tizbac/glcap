/*
 * Fraps clone for linux
Copyright (C) 2013  Tiziano Bacocco

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <map>
#include <list>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <link.h>
#include <vector>
#include <pthread.h>
#define glFlush glFlush_nouse
#include <GL/gl.h>
#include <GL/glext.h>
#undef glFlush
#define XNextEvent _XNextEvent
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#undef XNextEvent
#include <ctime>
#include "mediarecorder.h"
#include <sys/time.h>
MediaRecorder * curr_mediarec = NULL;
//f
size_t strftime_c(char *s, size_t max, const char *format)
{
   struct tm *tmp;
    time_t  t;
    t = time(NULL);
    tmp = localtime(&t);
    return strftime(s,max,format,tmp); 
}
bool recording = false;
double getcurrenttime()
{
    struct timeval t;
    gettimeofday(&t,NULL);
    double ti = 0.0;
    ti = t.tv_sec;
    ti += t.tv_usec/1000000.0;
    return ti;
}
double lastframe = 0.0;
struct uniformF
{
    float v0;
    float v1;
    float v2;
    float v3;
};
std::list<double> framerates;
typedef union
{
    struct uniformF F;
} uniform;

std::map<int, int> shadertype;


int dummyidcount = 0;
bool init = false;


int curr_program = -1;

void (*glUniform1f_real)(int,float) = 0x0;
void (*glUniform2f_real)(int,float,float) = 0x0;
void (*glUniform3f_real)(int,float,float,float) = 0x0;
void (*glUniform4f_real)(int,float,float,float,float) = 0x0;
void (*glUseProgram_real)(int) = 0x0;
void (*glLinkProgram_real)(int) = 0x0;
void (*glDeleteProgram_real)(int) = 0x0;
void (*glCreateProgramObject_real)() = 0x0;
void (*glCompileShader_real)(int) = 0x0;
void (*glAttachObjectARB_real)(int,int) = 0x0;
int (*glCreateShader_real)(int) = 0x0;
void (*glPushAttrib_real)(int) = 0x0;
void (*glPushClientAttrib_real)(int) = 0x0;
void (*glPopAttrib_real)() = 0x0;
void (*glPopClientAttrib_real)() = 0x0;
void (*glEnable_real)(int) = 0x0;
void (*glDisable_real)(int) = 0x0;
void (*glShaderSource_real)(int,unsigned int,const char **,const int*) = 0x0;
void (*glXSwapBuffers_real)(void*,void*) = 0x0;
void* (* glXGetProcAddress_real)(const char *) = 0x0;
void (*glXQueryDrawable_real)(void* , void * , int , void *) = 0x0;
void (*XNextEvent_real)(void*,void*) = 0x0;
void * GL_lib = 0x0;

void * (*real_dlsym)(void * , const char *) = 0x0;


void (*glGetIntegerv_real)(int, void *) = 0x0;
void (*glViewport_real)(int,int,int,int) = 0x0;
void (*glRenderMode_real)(int) = 0x0;
void (*glDisableClientState_real)(int) = 0x0;
void (*glActiveTexture_real)(int) = 0x0;
void (*glMatrixMode_real)(int) = 0x0;
void (*glPushMatrix_real)() = 0x0;
void (*glLoadIdentity_real)() = 0x0;
void (*glOrtho_real)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble) = 0x0;
void (*glPopMatrix_real)() = 0x0;
void (*glPolygonMode_real)(GLenum,GLenum) = 0x0;
void (*glColor3f_real)(GLfloat,GLfloat,GLfloat) = 0x0;
void (*glBegin_real)(GLenum) = 0x0;
void (*glEnd_real)() = 0x0;
void (*glVertex3f_real)(GLfloat,GLfloat,GLfloat) = 0x0;
void (*glGenTextures_real)(GLsizei,GLuint*) = 0x0;
void (*glBindTexture_real)(GLenum,GLint) = 0x0;
void (*glCopyTexImage2D_real)(GLenum,GLint,GLenum,GLint,GLint,GLsizei,GLsizei,GLint) = 0x0;
void (*glPixelStorei_real)(GLenum,GLint) = 0x0;
void (*glGetTexImage_real)(GLenum,GLint,GLenum,GLenum,GLvoid *) = 0x0;
void (*glColor4f_real)(GLfloat,GLfloat,GLfloat,GLfloat) = 0x0;
void (*glDrawBuffer_real)(GLenum) = 0x0;
#define getprocaddr(f) f##_real = real_dlsym(GL_lib,#f)

int width = 0;
int height = 0;

pthread_mutex_t mediarecord_mutex;

__attribute__((constructor)) void OnLoad()
{
    pthread_mutex_init(&mediarecord_mutex,NULL);
    av_register_all();
    avcodec_register_all();
    printf("\033[34mGL_OPTIMIZER\033[0m\n");
    ElfW(Sym) * stab = NULL;

    void * libdl_handle = dlopen("libdl.so.2",RTLD_LAZY);
    struct link_map * lm = libdl_handle;
    const char * sstrtab = NULL;
    int nch = 0;
    ElfW(Dyn) * d = lm->l_ld;
    while ( d->d_tag) {
        switch ( d->d_tag )
        {
        case DT_HASH:
            nch = *(int *)(d->d_un.d_ptr + 4);
            break;
        case DT_STRTAB:
            sstrtab = d->d_un.d_ptr;
            break;
        case DT_SYMTAB:
            stab = d->d_un.d_ptr;
            break;
        }
        d++;
    }
    for ( int i = 0; i < nch; i++ )
    {
        if (ELF32_ST_TYPE(stab[i].st_info) != STT_FUNC)
            continue;
        if (strcmp(sstrtab+stab[i].st_name, "dlsym") == 0)
        {
            real_dlsym = (void*)lm->l_addr + stab[i].st_value;
            printf("Found dlsym at %p\n",real_dlsym);
        }
    }



    GL_lib = dlopen("libGL.so.1",RTLD_LAZY);
    printf("libGL.so opened: %p\n",GL_lib);
    if ( !GL_lib )
    {
        printf("Cannot load GL!\n");
        abort();
    }


    glUniform1f_real = real_dlsym(GL_lib,"glUniform1f");
    glUniform2f_real = real_dlsym(GL_lib,"glUniform2f");
    glUniform3f_real = real_dlsym(GL_lib,"glUniform3f");
    glUniform4f_real = real_dlsym(GL_lib,"glUniform4f");
    glUseProgram_real = real_dlsym(GL_lib,"glUseProgram");
    glLinkProgram_real = real_dlsym(GL_lib,"glLinkProgram");
    glDeleteProgram_real = real_dlsym(GL_lib,"glDeleteProgram");
    glShaderSource_real = real_dlsym(GL_lib,"glShaderSource");
    glCreateShader_real = real_dlsym(GL_lib,"glCreateShader");
    glXGetProcAddress_real = real_dlsym(GL_lib,"glXGetProcAddress");
    glXSwapBuffers_real = real_dlsym(GL_lib,"glXSwapBuffers");
    glPushAttrib_real = real_dlsym(GL_lib,"glPushAttrib");
    glPushClientAttrib_real = real_dlsym(GL_lib,"glPushClientAttrib");
    glPopAttrib_real = real_dlsym(GL_lib,"glPopAttrib");
    glPopClientAttrib_real = real_dlsym(GL_lib,"glPopClientAttrib");
    glEnable_real = real_dlsym(GL_lib,"glEnable");
    glDisable_real = real_dlsym(GL_lib,"glDisable");
    glXQueryDrawable_real = real_dlsym(GL_lib,"glXQueryDrawable");
    XNextEvent_real = real_dlsym(GL_lib,"XNextEvent");
    
    getprocaddr(glGetIntegerv);
    getprocaddr(glViewport);
    getprocaddr(glRenderMode);
    getprocaddr(glDisableClientState);
    getprocaddr(glActiveTexture);
    getprocaddr(glMatrixMode);
    getprocaddr(glLoadIdentity);
    getprocaddr(glOrtho);
    getprocaddr(glPushMatrix);
    getprocaddr(glPopMatrix);
    getprocaddr(glPolygonMode);
    getprocaddr(glColor3f);
    getprocaddr(glBegin);
    getprocaddr(glEnd);
    getprocaddr(glVertex3f);
    getprocaddr(glGenTextures);
    getprocaddr(glBindTexture);
    getprocaddr(glCopyTexImage2D);
    getprocaddr(glPixelStorei);
    getprocaddr(glGetTexImage);
    getprocaddr(glColor4f);
    getprocaddr(glDrawBuffer);
    init = true;
    lastframe = getcurrenttime();
    
    
}
GLint program;
GLint viewport[4];
void enterOverlayContext()
{
    glPushAttrib_real(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib_real(GL_ALL_ATTRIB_BITS);
   


    glGetIntegerv_real(GL_VIEWPORT, viewport);
    glGetIntegerv_real(GL_CURRENT_PROGRAM, &program);
    
    glUseProgram_real(0);
    glDisable_real(GL_ALPHA_TEST);
    glDisable_real(GL_AUTO_NORMAL);
    glViewport_real(0, 0, width, height);
    // Skip clip planes, there are thousands of them.
    glDisable_real(GL_COLOR_LOGIC_OP);
    glDisable_real(GL_COLOR_TABLE);
    glDisable_real(GL_CONVOLUTION_1D);
    glDisable_real(GL_CONVOLUTION_2D);
    glDisable_real(GL_CULL_FACE);
    glDisable_real(GL_DEPTH_TEST);
    glDisable_real(GL_DITHER);
    glDisable_real(GL_FOG);
    glDisable_real(GL_HISTOGRAM);
    glDisable_real(GL_INDEX_LOGIC_OP);
    glDisable_real(GL_LIGHTING);
    glDisable_real(GL_NORMALIZE);

    // Skip line smmooth
    // Skip map
    glDisable_real(GL_MINMAX);
    // Skip polygon offset
    glDisable_real(GL_SEPARABLE_2D);
    glDisable_real(GL_SCISSOR_TEST);
    glDisable_real(GL_STENCIL_TEST);
    glDisable_real(GL_TEXTURE_CUBE_MAP);
    glDisable_real(GL_VERTEX_PROGRAM_ARB);
    glDisable_real(GL_FRAGMENT_PROGRAM_ARB);
    glDisable_real(GL_BLEND);
    glRenderMode_real(GL_RENDER);

    glDisableClientState_real(GL_VERTEX_ARRAY);
    glDisableClientState_real(GL_NORMAL_ARRAY);
    glDisableClientState_real(GL_COLOR_ARRAY);
    glDisableClientState_real(GL_INDEX_ARRAY);
    glDisableClientState_real(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState_real(GL_EDGE_FLAG_ARRAY);

    GLint texunits = 1;

    glGetIntegerv_real(GL_MAX_TEXTURE_UNITS, &texunits);

    for (int i=texunits-1; i>=0; --i) {
        glActiveTexture_real(GL_TEXTURE0 + i);
        glDisable_real(GL_TEXTURE_1D);
        glDisable_real(GL_TEXTURE_2D);
        glDisable_real(GL_TEXTURE_3D);
    }

    glDisable_real(GL_TEXTURE_CUBE_MAP);
    glDisable_real(GL_VERTEX_PROGRAM_ARB);
    glDisable_real(GL_FRAGMENT_PROGRAM_ARB);

    glMatrixMode_real(GL_PROJECTION);
    glPushMatrix_real();
    glLoadIdentity_real();
    glOrtho_real(0, width, height, 0, -100.0, 100.0);
    glMatrixMode_real(GL_MODELVIEW);
    glPushMatrix_real();
    glLoadIdentity_real();
    glEnable_real(GL_COLOR_MATERIAL);
    glColor4f_real(1,1,1,1);
     glDrawBuffer_real(GL_BACK);
}
void leaveOverlayContext()
{

    glMatrixMode_real(GL_MODELVIEW);
    glPopMatrix_real();

    glMatrixMode_real(GL_PROJECTION);
    glPopMatrix_real();

    glPopClientAttrib_real();
    glPopAttrib_real();

    glViewport_real(viewport[0], viewport[1], viewport[2], viewport[3]);
    glUseProgram_real(program);
}
void drawBoxInside(float x1,float y1,float w , float h)
{
    glPolygonMode_real(GL_FRONT_AND_BACK,GL_FILL);
    if (!recording )
        glColor3f_real(1.0,1.0,0.0);
    else
        glColor3f_real(1.0,0.0,0.0);
    glBegin_real(GL_QUADS);
    {
        glVertex3f_real(x1,y1,0);
        glVertex3f_real(x1+w,y1,0);
        glVertex3f_real(x1+w,y1+h,0);
        glVertex3f_real(x1,y1+h,0);
    }
    glEnd_real();
   
}
void drawBoxOutside(float x1,float y1,float w , float h)
{
    glPolygonMode_real(GL_FRONT_AND_BACK,GL_LINE);
    glColor3f_real(0.0,0.0,0.0);
    glBegin_real(GL_QUADS);
    {
        glVertex3f_real(x1,y1,0);
        glVertex3f_real(x1+w,y1,0);
        glVertex3f_real(x1+w,y1+h,0);
        glVertex3f_real(x1,y1+h,0);
    }
    glEnd_real();
}
#define SEGW 10.0f
#define SEGH 30.0f
void drawN_A_O(int x, int y, float scale = 1.0)
{
    drawBoxOutside(x,y,SEGW*scale,SEGH*scale);
}
void drawN_B_O(int x, int y,float scale = 1.0)
{
    drawBoxOutside(x,y,SEGH*scale+SEGW*scale,SEGW*scale);
}
void drawN_C_O(int x, int y,float scale = 1.0)
{
    drawBoxOutside(x+SEGH*scale,y,SEGW*scale,SEGH*scale);
}
void drawN_D_O(int x, int y,float scale = 1.0)
{
    drawBoxOutside(x,y+SEGH*scale-(SEGW/2.0)*scale,SEGH*scale+SEGW*scale,SEGW*scale);
}
void drawN_E_O(int x, int y,float scale = 1.0)
{
    drawBoxOutside(x,y+SEGH*scale,SEGW*scale,SEGH*scale);
}
void drawN_F_O(int x, int y,float scale = 1.0)
{
    drawBoxOutside(x,y+60*scale-SEGW*scale,SEGH*scale+SEGW*scale,SEGW*scale);
}
void drawN_G_O(int x, int y,float scale = 1.0)
{
    drawBoxOutside(x+SEGH*scale,y+SEGH*scale,SEGW*scale,SEGH*scale);
}
void drawN_A_I(int x, int y, float scale = 1.0)
{
    drawBoxInside(x,y,SEGW*scale,SEGH*scale);
}
void drawN_B_I(int x, int y,float scale = 1.0)
{
    drawBoxInside(x,y,SEGH*scale+SEGW*scale,SEGW*scale);
}
void drawN_C_I(int x, int y,float scale = 1.0)
{
    drawBoxInside(x+SEGH*scale,y,SEGW*scale,SEGH*scale);
}
void drawN_D_I(int x, int y,float scale = 1.0)
{
    drawBoxInside(x,y+SEGH*scale-(SEGW/2.0)*scale,SEGH*scale+SEGW*scale,SEGW*scale);
}
void drawN_E_I(int x, int y,float scale = 1.0)
{
    drawBoxInside(x,y+SEGH*scale,SEGW*scale,SEGH*scale);
}
void drawN_F_I(int x, int y,float scale = 1.0)
{
    drawBoxInside(x,y+60*scale-SEGW*scale,SEGH*scale+SEGW*scale,SEGW*scale);
}
void drawN_G_I(int x, int y,float scale = 1.0)
{
    drawBoxInside(x+SEGH*scale,y+SEGH*scale,SEGW*scale,SEGH*scale);
}
void drawFrapsLikeNumber(int x, int y, int n,float scale = 1.0)
{
    switch ( n )
    {
    case 8:
        drawN_A_O(x,y,scale);
        drawN_B_O(x,y,scale);
        drawN_C_O(x,y,scale);
        drawN_D_O(x,y,scale);
        drawN_E_O(x,y,scale);
        drawN_F_O(x,y,scale);
        drawN_G_O(x,y,scale);
        break;
    case 1:
        drawN_C_O(x,y,scale);
        drawN_G_O(x,y,scale);
        break;
    case 2:
        drawN_B_O(x,y,scale);
        drawN_C_O(x,y,scale);
        drawN_D_O(x,y,scale);
        drawN_E_O(x,y,scale);
        drawN_F_O(x,y,scale);
        break;
    case 3:
        drawN_B_O(x,y,scale);
        drawN_C_O(x,y,scale);
        drawN_D_O(x,y,scale);
        drawN_F_O(x,y,scale);
        drawN_G_O(x,y,scale);
        break;
    case 4:
        drawN_A_O(x,y,scale);
        drawN_C_O(x,y,scale);
        drawN_D_O(x,y,scale);
        drawN_G_O(x,y,scale);
        break;
    case 5:
        drawN_A_O(x,y,scale);
        drawN_B_O(x,y,scale);
        drawN_D_O(x,y,scale);
        drawN_F_O(x,y,scale);
        drawN_G_O(x,y,scale);
        break;
    case 6:
        drawN_A_O(x,y,scale);
        drawN_B_O(x,y,scale);
        drawN_D_O(x,y,scale);
        drawN_E_O(x,y,scale);
        drawN_F_O(x,y,scale);
        drawN_G_O(x,y,scale);
        break;
    case 7:
        drawN_A_O(x,y,scale);
        drawN_B_O(x,y,scale);
        drawN_C_O(x,y,scale);
        drawN_G_O(x,y,scale);
        break;
    case 9:
        drawN_A_O(x,y,scale);
        drawN_B_O(x,y,scale);
        drawN_C_O(x,y,scale);
        drawN_D_O(x,y,scale);
        drawN_F_O(x,y,scale);
        drawN_G_O(x,y,scale);
        break;
    case 0:
        drawN_A_O(x,y,scale);
        drawN_B_O(x,y,scale);
        drawN_C_O(x,y,scale);
        drawN_E_O(x,y,scale);
        drawN_F_O(x,y,scale);
        drawN_G_O(x,y,scale);
        break;

    }
    switch ( n )
    {
    case 8:
        drawN_A_I(x,y,scale);
        drawN_B_I(x,y,scale);
        drawN_C_I(x,y,scale);
        drawN_D_I(x,y,scale);
        drawN_E_I(x,y,scale);
        drawN_F_I(x,y,scale);
        drawN_G_I(x,y,scale);
        break;
    case 1:
        drawN_C_I(x,y,scale);
        drawN_G_I(x,y,scale);
        break;
    case 2:
        drawN_B_I(x,y,scale);
        drawN_C_I(x,y,scale);
        drawN_D_I(x,y,scale);
        drawN_E_I(x,y,scale);
        drawN_F_I(x,y,scale);
        break;
    case 3:
        drawN_B_I(x,y,scale);
        drawN_C_I(x,y,scale);
        drawN_D_I(x,y,scale);
        drawN_F_I(x,y,scale);
        drawN_G_I(x,y,scale);
        break;
    case 4:
        drawN_A_I(x,y,scale);
        drawN_C_I(x,y,scale);
        drawN_D_I(x,y,scale);
        drawN_G_I(x,y,scale);
        break;
    case 5:
        drawN_A_I(x,y,scale);
        drawN_B_I(x,y,scale);
        drawN_D_I(x,y,scale);
        drawN_F_I(x,y,scale);
        drawN_G_I(x,y,scale);
        break;
    case 6:
        drawN_A_I(x,y,scale);
        drawN_B_I(x,y,scale);
        drawN_D_I(x,y,scale);
        drawN_E_I(x,y,scale);
        drawN_F_I(x,y,scale);
        drawN_G_I(x,y,scale);
        break;
    case 7:
        drawN_A_I(x,y,scale);
        drawN_B_I(x,y,scale);
        drawN_C_I(x,y,scale);
        drawN_G_I(x,y,scale);
        break;
    case 9:
        drawN_A_I(x,y,scale);
        drawN_B_I(x,y,scale);
        drawN_C_I(x,y,scale);
        drawN_D_I(x,y,scale);
        drawN_F_I(x,y,scale);
        drawN_G_I(x,y,scale);
        break;
    case 0:
        drawN_A_I(x,y,scale);
        drawN_B_I(x,y,scale);
        drawN_C_I(x,y,scale);
        drawN_E_I(x,y,scale);
        drawN_F_I(x,y,scale);
        drawN_G_I(x,y,scale);
        break;

    }
    

}

void drawFrapsLikeFps(int fps,float scale = 1.0)
{
    int x = width;
    int numbersize = SEGH*scale+SEGW*scale+SEGW*scale;
    while ( fps > 0 )
    {
        x -= numbersize;
        drawFrapsLikeNumber(x,5,fps%10,scale);
        fps /= 10;
    }
}
char * data = NULL;
bool first_frame = true;
GLint cap_tex;
GLint render_tex;
extern "C" {
    __attribute__((visibility("default")))
    void glXSwapBuffers(void * dpy,void *  Drawable)
    {
        int oldwidth = width;
        int oldheiht = height;
        glXQueryDrawable_real(dpy, Drawable, 0x801D, (unsigned int *) &width);
        glXQueryDrawable_real(dpy, Drawable, 0x801E, (unsigned int *) &height);
        if ( !data || oldwidth != width || oldheiht != height)
        {
            if ( !data )
                data = malloc(width*height*4);
            else
                data = realloc(data,width*height*4);
            printf("width=%d,height=%d\n",width,height);

        }

        enterOverlayContext();
        if ( first_frame )
        {
            glGenTextures_real(1,&cap_tex);
            glGenTextures_real(1,&render_tex);
            first_frame = false;
        }
        if ( recording )
        {
            if ( curr_mediarec->isReady() )
            {
                glPixelStorei_real(GL_PACK_ALIGNMENT,4);
                glBindTexture_real(GL_TEXTURE_2D,cap_tex);
                glCopyTexImage2D_real(GL_TEXTURE_2D,0,GL_RGBA,0,0,width,height,0);
            }

        }
        double currfps = (1.0/(getcurrenttime()-lastframe));
        framerates.push_back(currfps);
        if ( framerates.size() > 16 )
            framerates.pop_front();
        double avg = 0.0f;
        for ( std::list<double>::iterator it = framerates.begin(); it != framerates.end(); it++ )
        {
            avg += *it;
        }
        avg /= (double)framerates.size();
        avg = std::min(avg,999.0);
        lastframe = getcurrenttime();
        drawFrapsLikeFps((int)(avg+0.1),0.5);


        
        
        
        if ( recording && curr_mediarec->isReady() )
        {
       /*     glEnable(GL_TEXTURE_2D);
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
            glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
            glDisable(GL_COLOR_MATERIAL);
            glBindTexture(GL_TEXTURE_2D,cap_tex);
            glColor4f(1,1,1,1);
            
            glBegin(GL_QUADS);
            glTexCoord2f(0,0);
            glVertex3f(0,0,0);
            glTexCoord2f(1,0);
            glVertex3f(width,0,0);
            glTexCoord2f(1,1);
            glVertex3f(width,height,0);
            glTexCoord2f(0,1);
            glVertex3f(0,height,0);
            glEnd();*/
            glBindTexture_real(GL_TEXTURE_2D,cap_tex);
       //     glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,0,0,width,height,0);
            glGetTexImage_real(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
            //memset(data,width*height*4,125);
            pthread_mutex_lock(&mediarecord_mutex);
            curr_mediarec->AppendFrame(0.0,width,height,data);//POINTER MUST BE VALID UNTIL NEXT FRAME
            pthread_mutex_unlock(&mediarecord_mutex);
        }
        glXSwapBuffers_real(dpy,Drawable);
        leaveOverlayContext();

    }

    void XNextEvent(void * display, XEvent * event)
    {
        XNextEvent_real(display,event);
        if ( event->type == KeyPress  )
        {
            // printf("%x\n",event->xkey.keycode);
            if ( event->xkey.keycode == 0x60 /*F12*/)
            {
                recording = !recording;
                pthread_mutex_lock(&mediarecord_mutex);

                if ( recording )
                {
                    
                    char * homedir = getenv("HOME"); 
                    char date[512];
                    strftime_c(date,511,"%F %r");
                    char filename[1024];
                    sprintf(filename,"%s/GLCAP %s.avi",homedir,date);
                    curr_mediarec = new MediaRecorder(filename,width,height);
                }
                if ( !recording )
                {
                    delete curr_mediarec;
                    curr_mediarec = NULL;
                }
                pthread_mutex_unlock(&mediarecord_mutex);
            }

        }
    }
   /* void glFlush()
    {
        
    }*/
}


/*
 * Invoke the true dlopen() function.
 */
static void *_dlopen(const char *filename, int flag)
{
    typedef void * (*PFN_DLOPEN)(const char *, int);
    static PFN_DLOPEN dlopen_ptr = NULL;

    if (!dlopen_ptr) {
        dlopen_ptr = (PFN_DLOPEN)dlsym(RTLD_NEXT, "dlopen");
        if (!dlopen_ptr) {
            printf("apitrace: error: dlsym(RTLD_NEXT, \"dlopen\") failed\n");
            return NULL;
        }
    }

    return dlopen_ptr(filename, flag);
}

#define DL_DECLSYM(s) if ( strcmp(__name,#s) == 0 ) { return &s ; }

#define DL_DEFS \
    DL_DECLSYM(glXSwapBuffers)\
    DL_DECLSYM(XNextEvent)\




extern "C" {
    void* glXGetProcAddress_WITHOUTARB(const char * __name_)
    {
        char __name[128];
        strcpy(__name,__name_);
        int l = strlen(__name);
        if ( l > 3 && __name[l-1] == 'B' && __name[l-2] == 'R' && __name[l-3] == 'A')
        {
            __name[l-3] = 0x0;
        } else {
            return NULL;
        }
        DL_DEFS;
        return NULL;

    }

    void * glXGetProcAddress(const char * __name)
    {
        printf("GLXProc addr: %s\n",__name);
        void *proc_addr = glXGetProcAddress_WITHOUTARB(__name);
        if ( proc_addr )
            return proc_addr;
        DL_DEFS;
        printf("NOT HOOKED\n");
        return glXGetProcAddress_real(__name);
    }
    void * glXGetProcAddressARB(const char * __name)
    {
        return glXGetProcAddress(__name);
    }


    void * dlsym(void *__handle, const char *__name)
    {
        if ( !init )
            OnLoad();
        printf("dlsym %p, %p: %s\n",&dlsym,&real_dlsym,__name);
        DL_DEFS;
        DL_DECLSYM(glXGetProcAddressARB);
        return real_dlsym(__handle,__name);

    }
}
