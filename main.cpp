#define __USE_GNU
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
#include <GL/gl.h>
#include <GL/glext.h>
#define XNextEvent _XNextEvent
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#undef XNextEvent
#include <sys/time.h>
#include "mediarecorder.h"
MediaRecorder * curr_mediarec = NULL;
//f
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
void (*glPopAttrib_real)(int) = 0x0;
void (*glEnable_real)(int) = 0x0;
void (*glDisable_real)(int) = 0x0;
void (*glShaderSource_real)(int,unsigned int,const char **,const int*) = 0x0;
void (*glXSwapBuffers_real)(void*,void*) = 0x0;
void* (* glXGetProcAddress_real)(const char *) = 0x0;
void (*glXQueryDrawable_real)(void* , void * , int , void *) = 0x0;
void (*XNextEvent_real)(void*,void*) = 0x0;
void * GL_lib = 0x0;

void * (*real_dlsym)(void * , const char *) = 0x0;

int width = 0; int height = 0;

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
    glPopAttrib_real = real_dlsym(GL_lib,"glPopAttrib");
    glEnable_real = real_dlsym(GL_lib,"glEnable");
    glDisable_real = real_dlsym(GL_lib,"glDisable");
    glXQueryDrawable_real = real_dlsym(GL_lib,"glXQueryDrawable");
    XNextEvent_real = real_dlsym(GL_lib,"XNextEvent");
    init = true;
    lastframe = getcurrenttime();
}
GLint program;
GLint viewport[4];
void enterOverlayContext()
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_ALL_ATTRIB_BITS);



    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_AUTO_NORMAL);
    glViewport(0, 0, width, height);
    // Skip clip planes, there are thousands of them.
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_COLOR_TABLE);
    glDisable(GL_CONVOLUTION_1D);
    glDisable(GL_CONVOLUTION_2D);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);
    glDisable(GL_FOG);
    glDisable(GL_HISTOGRAM);
    glDisable(GL_INDEX_LOGIC_OP);
    glDisable(GL_LIGHTING);
    glDisable(GL_NORMALIZE);

    // Skip line smmooth
    // Skip map
    glDisable(GL_MINMAX);
    // Skip polygon offset
    glDisable(GL_SEPARABLE_2D);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_TEXTURE_CUBE_MAP);
        glDisable(GL_VERTEX_PROGRAM_ARB);
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glRenderMode(GL_RENDER);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_INDEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_EDGE_FLAG_ARRAY);

    GLint texunits = 1;

    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &texunits);

    for (int i=texunits-1;i>=0;--i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glDisable(GL_TEXTURE_1D);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_TEXTURE_3D);
    }

    glDisable(GL_TEXTURE_CUBE_MAP);
    glDisable(GL_VERTEX_PROGRAM_ARB);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, height, 0, -100.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glEnable(GL_COLOR_MATERIAL);
}
void leaveOverlayContext()
{

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopClientAttrib();
    glPopAttrib();

    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glUseProgram_real(program);
}
void drawBox(float x1,float y1,float w , float h)
{
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    if (!recording )
       glColor3f(1.0,1.0,0.0);
    else
       glColor3f(1.0,0.0,0.0);
    glBegin(GL_QUADS);
    {
        glVertex3f(x1,y1,0);
        glVertex3f(x1+w,y1,0);
        glVertex3f(x1+w,y1+h,0);
        glVertex3f(x1,y1+h,0);
    }
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glColor3f(0.0,0.0,0.0);
    glBegin(GL_QUADS);
    {
        glVertex3f(x1,y1,0);
        glVertex3f(x1+w,y1,0);
        glVertex3f(x1+w,y1+h,0);
        glVertex3f(x1,y1+h,0);
    }
    glEnd();
}
void drawN_A(int x, int y, float scale = 1.0)
{
    drawBox(x,y,5*scale,30*scale);
}
void drawN_B(int x, int y,float scale = 1.0)
{
    drawBox(x,y,30*scale+5*scale,5*scale);
}
void drawN_C(int x, int y,float scale = 1.0)
{
    drawBox(x+30*scale,y,5*scale,30*scale);
}
void drawN_D(int x, int y,float scale = 1.0)
{
    drawBox(x,y+30*scale-(5.0/2.0)*scale,30*scale+5*scale,5*scale);
}
void drawN_E(int x, int y,float scale = 1.0)
{
    drawBox(x,y+30*scale,5*scale,30*scale);
}
void drawN_F(int x, int y,float scale = 1.0)
{
    drawBox(x,y+60*scale-5*scale,30*scale+5*scale,5*scale);
}
void drawN_G(int x, int y,float scale = 1.0)
{
    drawBox(x+30*scale,y+30*scale,5*scale,30*scale);
}
void drawFrapsLikeNumber(int x, int y, int n,float scale = 1.0)
{

    switch ( n )
    {
        case 8:
            drawN_A(x,y,scale);
            drawN_B(x,y,scale);
            drawN_C(x,y,scale);
            drawN_D(x,y,scale);
            drawN_E(x,y,scale);
            drawN_F(x,y,scale);
            drawN_G(x,y,scale);
            break;
        case 1:
            drawN_C(x,y,scale);
            drawN_G(x,y,scale);
            break;
        case 2:
            drawN_B(x,y,scale);
            drawN_C(x,y,scale);
            drawN_D(x,y,scale);
            drawN_E(x,y,scale);
            drawN_F(x,y,scale);
            break;
        case 3:
            drawN_B(x,y,scale);
            drawN_C(x,y,scale);
            drawN_D(x,y,scale);
            drawN_F(x,y,scale);
            drawN_G(x,y,scale);
            break;
        case 4:
            drawN_A(x,y,scale);
            drawN_C(x,y,scale);
            drawN_D(x,y,scale);
            drawN_G(x,y,scale);
            break;
        case 5:
            drawN_A(x,y,scale);
            drawN_B(x,y,scale);
            drawN_D(x,y,scale);
            drawN_F(x,y,scale);
            drawN_G(x,y,scale);
            break;
        case 6:
            drawN_A(x,y,scale);
            drawN_B(x,y,scale);
            drawN_D(x,y,scale);
            drawN_E(x,y,scale);
            drawN_F(x,y,scale);
            drawN_G(x,y,scale);
            break;
        case 7:
            drawN_A(x,y,scale);
            drawN_B(x,y,scale);
            drawN_C(x,y,scale);
            drawN_G(x,y,scale);
            break;
        case 9:
            drawN_A(x,y,scale);
            drawN_B(x,y,scale);
            drawN_C(x,y,scale);
            drawN_D(x,y,scale);
            drawN_F(x,y,scale);
            drawN_G(x,y,scale);
            break;
        case 0:
            drawN_A(x,y,scale);
            drawN_B(x,y,scale);
            drawN_C(x,y,scale);
            drawN_E(x,y,scale);
            drawN_F(x,y,scale);
            drawN_G(x,y,scale);
            break;

    }

}

void drawFrapsLikeFps(int fps,float scale = 1.0)
{
    int x = width;
    int numbersize = 30+5*scale+5;
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
            glGenTextures(1,&cap_tex);
            glGenTextures(1,&render_tex);
            first_frame = false;
        }
        if ( recording )
        {
	    if ( curr_mediarec->isReady() )
	    {
		glPixelStorei(GL_PACK_ALIGNMENT,4);
		glBindTexture(GL_TEXTURE_2D,cap_tex);
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,0,0,width,height,0);
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
        drawFrapsLikeFps((int)(avg+0.1));
        lastframe = getcurrenttime();
        

        glXSwapBuffers_real(dpy,Drawable);
	if ( recording && curr_mediarec->isReady() )
	{
	    glColor4f(1,1,1,1);
	    glEnable(GL_TEXTURE_2D);
	    glBegin(GL_QUADS);
	      glTexCoord2f(0,0);
	      glVertex3f(0,0,0);
	      glTexCoord2f(1,0);
	      glVertex3f(width,0,0);
	      glTexCoord2f(1,1);
	      glVertex3f(width,height,0);
	      glTexCoord2f(0,1);
	      glVertex3f(0,height,0);
	    glEnd();
	    glBindTexture(GL_TEXTURE_2D,render_tex);
	    glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,0,0,width,height,0);
	    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
            //memset(data,width*height*4,125);
            pthread_mutex_lock(&mediarecord_mutex);
            //curr_mediarec->AppendFrame(0.0,width,height,data);
            pthread_mutex_unlock(&mediarecord_mutex);
	}
	
	leaveOverlayContext();
    }

    void XNextEvent(void * display, XEvent * event)
    {
        XNextEvent_real(display,event);
        if ( event->type == KeyPress )
        {
           // printf("%x\n",event->xkey.keycode);
            if ( event->xkey.keycode == 0x60 /*F12*/)
            {
                recording = !recording;
                pthread_mutex_lock(&mediarecord_mutex);
            
                if ( recording )
                    curr_mediarec = new MediaRecorder("/home/tiziano/test_cap.avi",width,height);
                if ( !recording )
                {
                    delete curr_mediarec;
                    curr_mediarec = NULL;
                }
                pthread_mutex_unlock(&mediarecord_mutex);
            }

        }
    }
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
    DL_DECLSYM(glXSwapBuffers)




extern "C" {
    void* glXGetProcAddress_WITHOUTARB(const char * __name_)
    {
        char __name[128];
        strcpy(__name,__name_);
        int l = strlen(__name);
        if ( l > 3 && __name[l-1] == 'B' && __name[l-2] == 'R' && __name[l-3] == 'A')
        {
            __name[l-3] = 0x0;
        }else{
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