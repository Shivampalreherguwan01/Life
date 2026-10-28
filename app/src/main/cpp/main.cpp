#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cmath>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"LIFE",__VA_ARGS__)

static EGLDisplay display=EGL_NO_DISPLAY;
static EGLSurface surface=EGL_NO_SURFACE;
static EGLContext context=EGL_NO_CONTEXT;
static GLuint program=0,vao=0,vbo=0,ebo=0;
static int width=1,height=1;
static float playerX=0.0f,lastTouchX=0.0f;
static bool touching=false;

static const char* vertexShader=R"(
#version 300 es
layout(location=0) in vec3 position;
uniform vec3 objectPosition;
uniform vec3 objectScale;
uniform vec3 tint;
out vec3 color;
void main(){
    vec3 p=position*objectScale+objectPosition;
    gl_Position=vec4(p.x*0.65,p.y*0.75,p.z*0.18+0.5,1.0);
    color=tint*(0.72+0.28*(position.y*0.5+0.5));
}
)";

static const char* fragmentShader=R"(
#version 300 es
precision mediump float;
in vec3 color;
out vec4 outputColor;
void main(){
    outputColor=vec4(color,1.0);
}
)";

static GLuint compileShader(GLenum type,const char* source){
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&source,nullptr);
    glCompileShader(s);
    GLint ok=0;
    glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){
        char log[1024]={0};
        glGetShaderInfoLog(s,sizeof(log),nullptr,log);
        LOGE("Shader error: %s",log);
    }
    return s;
}

static bool initGraphics(ANativeWindow* window){
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display==EGL_NO_DISPLAY ||
       !eglInitialize(display,nullptr,nullptr)) return false;

    const EGLint attrs[]={
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,
        EGL_DEPTH_SIZE,16,EGL_NONE
    };
    EGLConfig config;
    EGLint count=0;
    eglChooseConfig(display,attrs,&config,1,&count);
    if(count==0) return false;

    EGLint format;
    eglGetConfigAttrib(display,config,EGL_NATIVE_VISUAL_ID,&format);
    ANativeWindow_setBuffersGeometry(window,0,0,format);

    const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    surface=eglCreateWindowSurface(display,config,window,nullptr);
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,ca);
    if(surface==EGL_NO_SURFACE || context==EGL_NO_CONTEXT)
        return false;
    if(!eglMakeCurrent(display,surface,surface,context))
        return false;

    eglQuerySurface(display,surface,EGL_WIDTH,&width);
    eglQuerySurface(display,surface,EGL_HEIGHT,&height);

    GLuint vs=compileShader(GL_VERTEX_SHADER,vertexShader);
    GLuint fs=compileShader(GL_FRAGMENT_SHADER,fragmentShader);
    program=glCreateProgram();
    glAttachShader(program,vs);
    glAttachShader(program,fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked=0;
    glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(!linked) return false;

    const GLfloat vertices[]={
       -1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
       -1,-1, 1, 1,-1, 1, 1,1, 1, -1,1, 1
    };
    const GLuint indices[]={
       0,1,2,0,2,3, 4,6,5,4,7,6,
       0,4,5,0,5,1, 3,2,6,3,6,7,
       1,5,6,1,6,2, 0,3,7,0,7,4
    };

    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);
    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    glGenBuffers(1,&ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),nullptr);
    glEnableVertexAttribArray(0);
    return true;
}

static void box(float x,float y,float z,
                float sx,float sy,float sz,
                float r,float g,float b){
    glUniform3f(glGetUniformLocation(program,"objectPosition"),x,y,z);
    glUniform3f(glGetUniformLocation(program,"objectScale"),sx,sy,sz);
    glUniform3f(glGetUniformLocation(program,"tint"),r,g,b);
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,nullptr);
}

static void drawFrame(){
    if(display==EGL_NO_DISPLAY || surface==EGL_NO_SURFACE) return;
    eglMakeCurrent(display,surface,surface,context);
    glViewport(0,0,width,height);
    glClearColor(0.36f,0.68f,0.95f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(program);
    glBindVertexArray(vao);

    // Ground
    box(0.0f,-0.78f,0.8f,1.8f,0.08f,0.7f,
        0.20f,0.62f,0.25f);

    // Legs
    box(playerX-0.10f,-0.43f,0.0f,0.075f,0.25f,0.09f,
        0.10f,0.16f,0.85f);
    box(playerX+0.10f,-0.43f,0.0f,0.075f,0.25f,0.09f,
        0.10f,0.16f,0.85f);

    // Body
    box(playerX,-0.04f,0.0f,0.22f,0.29f,0.12f,
        0.05f,0.12f,0.25f);

    // Arms
    box(playerX-0.29f,-0.06f,0.0f,0.075f,0.25f,0.08f,
        0.05f,0.12f,0.25f);
    box(playerX+0.29f,-0.06f,0.0f,0.075f,0.25f,0.08f,
        0.05f,0.12f,0.25f);

    // Head and hair
    box(playerX,0.36f,0.0f,0.14f,0.14f,0.13f,
        0.85f,0.60f,0.40f);
    box(playerX,0.48f,0.0f,0.145f,0.055f,0.14f,
        0.08f,0.05f,0.03f);

    eglSwapBuffers(display,surface);
}

static int32_t handleInput(android_app*,AInputEvent* event){
    if(AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION)
        return 0;

    int action=AMotionEvent_getAction(event)&AMOTION_EVENT_ACTION_MASK;
    float x=AMotionEvent_getX(event,0);

    if(action==AMOTION_EVENT_ACTION_DOWN){
        touching=true;
        lastTouchX=x;
        return 1;
    }
    if(action==AMOTION_EVENT_ACTION_MOVE && touching){
        playerX+=(x-lastTouchX)*2.0f/(float)width;
        playerX=std::clamp(playerX,-1.25f,1.25f);
        lastTouchX=x;
        return 1;
    }
    if(action==AMOTION_EVENT_ACTION_UP ||
       action==AMOTION_EVENT_ACTION_CANCEL){
        touching=false;
        return 1;
    }
    return 0;
}

static void shutdownGraphics(){
    if(display!=EGL_NO_DISPLAY){
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if(program) glDeleteProgram(program);
        if(vbo) glDeleteBuffers(1,&vbo);
        if(ebo) glDeleteBuffers(1,&ebo);
        if(vao) glDeleteVertexArrays(1,&vao);
        if(context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
        if(surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
        eglTerminate(display);
    }
    display=EGL_NO_DISPLAY;
    surface=EGL_NO_SURFACE;
    context=EGL_NO_CONTEXT;
    program=vao=vbo=ebo=0;
}

static void handleCommand(android_app* app,int32_t command){
    if(command==APP_CMD_INIT_WINDOW && app->window){
        initGraphics(app->window);
    }else if(command==APP_CMD_TERM_WINDOW){
        shutdownGraphics();
    }
}

void android_main(android_app* app){
    app_dummy();
    app->onAppCmd=handleCommand;
    app->onInputEvent=handleInput;

    while(true){
        int events;
        android_poll_source* source;
        while(ALooper_pollOnce(0,nullptr,&events,
              (void**)&source)>=0){
            if(source) source->process(app,source);
            if(app->destroyRequested){
                shutdownGraphics();
                return;
            }
        }
        if(app->window && display!=EGL_NO_DISPLAY)
            drawFrame();
    }
}
