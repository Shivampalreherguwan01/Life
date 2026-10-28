#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <cstdint>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "LIFE", __VA_ARGS__)

static EGLDisplay display;
static EGLSurface surface;
static EGLContext context;
static GLuint program, vao, vbo;
static int width, height;
static float angle = 0.0f;

static const char* vertexShader = R"(
#version 300 es
layout(location=0) in vec3 position;
uniform float turn;
out vec3 color;
void main() {
    float c=cos(turn), s=sin(turn);
    vec3 p=vec3(position.x*c-position.z*s,
                position.y,
                position.x*s+position.z*c);
    p.y-=0.12;
    gl_Position=vec4(p.x*0.65,p.y*0.75,p.z*0.25+0.5,1.0);
    color=position*0.5+0.5;
}
)";

static const char* fragmentShader = R"(
#version 300 es
precision mediump float;
in vec3 color;
out vec4 outputColor;
void main() {
    outputColor=vec4(color,1.0);
}
)";

static GLuint compileShader(GLenum type, const char* source) {
    GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&source,nullptr);
    glCompileShader(shader);
    GLint ok=0;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) LOGE("Shader compilation failed");
    return shader;
}

static bool initGraphics(ANativeWindow* window) {
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display==EGL_NO_DISPLAY || !eglInitialize(display,nullptr,nullptr))
        return false;

    const EGLint attrs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RED_SIZE,8,
        EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
    EGLConfig config;
    EGLint count;
    eglChooseConfig(display,attrs,&config,1,&count);
    if(count==0) return false;

    EGLint format;
    eglGetConfigAttrib(display,config,EGL_NATIVE_VISUAL_ID,&format);
    ANativeWindow_setBuffersGeometry(window,0,0,format);

    const EGLint ctxAttrs[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    surface=eglCreateWindowSurface(display,config,window,nullptr);
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,ctxAttrs);
    if(surface==EGL_NO_SURFACE || context==EGL_NO_CONTEXT) return false;
    if(!eglMakeCurrent(display,surface,surface,context)) return false;

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

    const GLfloat vertices[]={
       -1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
       -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1
    };
    const GLuint indices[]={
       0,1,2, 0,2,3, 4,6,5, 4,7,6,
       0,4,5, 0,5,1, 3,2,6, 3,6,7,
       1,5,6, 1,6,2, 0,3,7, 0,7,4
    };

    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);
    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    GLuint ebo;
    glGenBuffers(1,&ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(GLfloat),nullptr);
    glEnableVertexAttribArray(0);
    return true;
}

static void drawFrame() {
    if(!display || !surface || !context) return;
    eglMakeCurrent(display,surface,surface,context);
    glViewport(0,0,width,height);
    glClearColor(0.04f,0.07f,0.13f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(program);
    glUniform1f(glGetUniformLocation(program,"turn"),angle);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,nullptr);
    eglSwapBuffers(display,surface);
    angle+=0.015f;
}

static void shutdownGraphics() {
    if(display!=EGL_NO_DISPLAY) {
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if(context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
        if(surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
        eglTerminate(display);
    }
    display=EGL_NO_DISPLAY;
    surface=EGL_NO_SURFACE;
    context=EGL_NO_CONTEXT;
}

static void handleCommand(android_app* app,int32_t command) {
    if(command==APP_CMD_INIT_WINDOW && app->window) {
        initGraphics(app->window);
    } else if(command==APP_CMD_TERM_WINDOW) {
        shutdownGraphics();
    }
}

void android_main(android_app* app) {
    app_dummy();
    display=EGL_NO_DISPLAY;
    surface=EGL_NO_SURFACE;
    context=EGL_NO_CONTEXT;
    app->onAppCmd=handleCommand;

    while(true) {
        int events;
        android_poll_source* source;
        while(ALooper_pollOnce(0,nullptr,&events,
               (void**)&source)>=0) {
            if(source) source->process(app,source);
            if(app->destroyRequested) {
                shutdownGraphics();
                return;
            }
        }
        if(app->window && display!=EGL_NO_DISPLAY) drawFrame();
    }
}
