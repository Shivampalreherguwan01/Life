#include <android_native_app_glue.h>
#include "gltf_model.h"
#include <android/input.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <vector>
#include <cmath>
#include <algorithm>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"LIFE",__VA_ARGS__)

static EGLDisplay display=EGL_NO_DISPLAY;
static EGLSurface surface=EGL_NO_SURFACE;
static EGLContext context=EGL_NO_CONTEXT;

static GLuint program=0;
static GLuint vao=0,vbo=0,ebo=0;
static GLsizei indexCount=0;

static GLuint cubeVao=0, cubeVbo=0, cubeEbo=0;
static GLsizei cubeIndexCount=0;

static int width=1,height=1;
static float playerX=0.0f,lastTouchX=0.0f;
static bool touching=false;
static bool playerMoving=false;
static bool movedTouch=false;
static float touchStartX=0.0f;
static float playerFacing=1.0f;
static float jumpY=0.0f;
static float jumpVelocity=0.0f;
static float animationTime=0.0f;

struct Vertex {
    float x,y,z,nx,ny,nz;
};

struct Vec3 {
    float x,y,z;
};

static Vec3 sub(Vec3 a,Vec3 b){
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}

static Vec3 cross(Vec3 a,Vec3 b){
    return {
        a.y*b.z-a.z*b.y,
        a.z*b.x-a.x*b.z,
        a.x*b.y-a.y*b.x
    };
}

static Vec3 normalize(Vec3 v){
    float l=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);
    if(l<0.0001f) return {0,1,0};
    return {v.x/l,v.y/l,v.z/l};
}

static void identity(float* m){
    for(int i=0;i<16;i++) m[i]=0;
    m[0]=m[5]=m[10]=m[15]=1;
}

static void multiply(float* out,const float* a,const float* b){
    float r[16];
    for(int c=0;c<4;c++)
        for(int row=0;row<4;row++){
            r[c*4+row]=0;
            for(int k=0;k<4;k++)
                r[c*4+row]+=a[k*4+row]*b[c*4+k];
        }
    for(int i=0;i<16;i++) out[i]=r[i];
}

static void perspective(float* m,float fov,float aspect,
                        float nearZ,float farZ){
    for(int i=0;i<16;i++) m[i]=0;
    float f=1.0f/tanf(fov*0.5f);
    m[0]=f/aspect;
    m[5]=f;
    m[10]=(farZ+nearZ)/(nearZ-farZ);
    m[11]=-1;
    m[14]=(2*farZ*nearZ)/(nearZ-farZ);
}

static void lookAt(float* m,Vec3 eye,Vec3 center,Vec3 up){
    Vec3 f=normalize(sub(center,eye));
    Vec3 s=normalize(cross(f,up));
    Vec3 u=cross(s,f);

    identity(m);
    m[0]=s.x;  m[4]=s.y;  m[8]=s.z;
    m[1]=u.x;  m[5]=u.y;  m[9]=u.z;
    m[2]=-f.x; m[6]=-f.y; m[10]=-f.z;

    m[12]=-(s.x*eye.x+s.y*eye.y+s.z*eye.z);
    m[13]=-(u.x*eye.x+u.y*eye.y+u.z*eye.z);
    m[14]= f.x*eye.x+f.y*eye.y+f.z*eye.z;
}

static void modelMatrix(float* m,float x,float y,float z,
                        float sx,float sy,float sz){
    identity(m);
    m[0]=sx;
    m[5]=sy;
    m[10]=sz;
    m[12]=x;
    m[13]=y;
    m[14]=z;
}

static void initCubeMesh(){
    const Vertex v[] = {
        {-0.5f,-0.5f, 0.5f, 0,0,1},
        { 0.5f,-0.5f, 0.5f, 0,0,1},
        { 0.5f, 0.5f, 0.5f, 0,0,1},
        {-0.5f, 0.5f, 0.5f, 0,0,1},

        { 0.5f,-0.5f,-0.5f, 0,0,-1},
        {-0.5f,-0.5f,-0.5f, 0,0,-1},
        {-0.5f, 0.5f,-0.5f, 0,0,-1},
        { 0.5f, 0.5f,-0.5f, 0,0,-1},

        {-0.5f, 0.5f, 0.5f, 0,1,0},
        { 0.5f, 0.5f, 0.5f, 0,1,0},
        { 0.5f, 0.5f,-0.5f, 0,1,0},
        {-0.5f, 0.5f,-0.5f, 0,1,0},

        {-0.5f,-0.5f,-0.5f, 0,-1,0},
        { 0.5f,-0.5f,-0.5f, 0,-1,0},
        { 0.5f,-0.5f, 0.5f, 0,-1,0},
        {-0.5f,-0.5f, 0.5f, 0,-1,0},

        {0.5f,-0.5f, 0.5f, 1,0,0},
        {0.5f,-0.5f,-0.5f, 1,0,0},
        {0.5f, 0.5f,-0.5f, 1,0,0},
        {0.5f, 0.5f, 0.5f, 1,0,0},

        {-0.5f,-0.5f,-0.5f,-1,0,0},
        {-0.5f,-0.5f, 0.5f,-1,0,0},
        {-0.5f, 0.5f, 0.5f,-1,0,0},
        {-0.5f, 0.5f,-0.5f,-1,0,0}
    };

    const unsigned int ix[] = {
        0,1,2, 0,2,3, 4,5,6, 4,6,7,
        8,9,10, 8,10,11, 12,13,14, 12,14,15,
        16,17,18, 16,18,19, 20,21,22, 20,22,23
    };

    cubeIndexCount = sizeof(ix)/sizeof(ix[0]);
    glGenVertexArrays(1,&cubeVao);
    glBindVertexArray(cubeVao);

    glGenBuffers(1,&cubeVbo);
    glBindBuffer(GL_ARRAY_BUFFER,cubeVbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);

    glGenBuffers(1,&cubeEbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,cubeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(ix),ix,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,
        sizeof(Vertex),(void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,
        sizeof(Vertex),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
}

static const char* vertexShader=R"(
#version 300 es
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;

uniform mat4 mvp;
uniform mat4 model;
uniform vec3 tint;

out vec3 vColor;

void main(){
    vec3 n=normalize(mat3(model)*normal);
    vec3 light=normalize(vec3(-0.5,1.0,0.7));
    float diffuse=max(dot(n,light),0.0);
    float lighting=0.32+0.68*diffuse;
    vColor=tint*lighting;
    gl_Position=mvp*vec4(position,1.0);
}
)";

static const char* fragmentShader=R"(
#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 outputColor;

void main(){
    outputColor=vec4(vColor,1.0);
}
)";

static GLuint compileShader(GLenum type,const char* src){
    GLuint s=glCreateShader(type);
    glShaderSource(s,1,&src,nullptr);
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

static bool initGraphics(ANativeWindow* window, AAssetManager* assets){
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display==EGL_NO_DISPLAY ||
       !eglInitialize(display,nullptr,nullptr)) return false;

    const EGLint attrs[]={
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,
        EGL_DEPTH_SIZE,24,EGL_NONE
    };

    EGLConfig config;
    EGLint count=0;
    eglChooseConfig(display,attrs,&config,1,&count);
    if(count==0) return false;

    EGLint format;
    eglGetConfigAttrib(display,config,EGL_NATIVE_VISUAL_ID,&format);
    ANativeWindow_setBuffersGeometry(window,0,0,format);

    const EGLint ca[]={
        EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE
    };

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
    if(!linked){
        char log[1024]={0};
        glGetProgramInfoLog(program,sizeof(log),nullptr,log);
        LOGE("Link error: %s",log);
        return false;
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    const int rings=20;
    const int sectors=28;
    const float pi=3.14159265f;

    for(int r=0;r<=rings;r++){
        float phi=pi*(float)r/rings;
        for(int s=0;s<=sectors;s++){
            float theta=2*pi*(float)s/sectors;

            float x=sinf(phi)*cosf(theta);
            float y=cosf(phi);
            float z=sinf(phi)*sinf(theta);

            vertices.push_back({x,y,z,x,y,z});
        }
    }

    for(int r=0;r<rings;r++){
        for(int s=0;s<sectors;s++){
            unsigned int a=r*(sectors+1)+s;
            unsigned int b=a+sectors+1;

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(a+1);

            indices.push_back(a+1);
            indices.push_back(b);
            indices.push_back(b+1);
        }
    }

    indexCount=(GLsizei)indices.size();

    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);

    glGenBuffers(1,&vbo);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size()*sizeof(Vertex),
        vertices.data(),GL_STATIC_DRAW);

    glGenBuffers(1,&ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size()*sizeof(unsigned int),
        indices.data(),GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,
        sizeof(Vertex),(void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,
        sizeof(Vertex),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    initCubeMesh();
    initGltfModel(assets);
    return true;
}

static float viewProjection[16];

static void drawPart(float x,float y,float z,
                     float sx,float sy,float sz,
                     float r,float g,float b){
    float model[16],mvp[16];

    glBindVertexArray(vao);
    modelMatrix(model,x,y,z,sx,sy,sz);
    multiply(mvp,viewProjection,model);

    glUniformMatrix4fv(
        glGetUniformLocation(program,"model"),1,GL_FALSE,model);
    glUniformMatrix4fv(
        glGetUniformLocation(program,"mvp"),1,GL_FALSE,mvp);
    glUniform3f(
        glGetUniformLocation(program,"tint"),r,g,b);

    glDrawElements(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,nullptr);
}

static void drawBlock(float x,float y,float z,
                       float sx,float sy,float sz,
                       float r,float g,float b){
    float model[16],mvp[16];
    glBindVertexArray(cubeVao);
    modelMatrix(model,x,y,z,sx,sy,sz);
    multiply(mvp,viewProjection,model);

    glUniformMatrix4fv(glGetUniformLocation(program,"model"),
                       1,GL_FALSE,model);
    glUniformMatrix4fv(glGetUniformLocation(program,"mvp"),
                       1,GL_FALSE,mvp);
    glUniform3f(glGetUniformLocation(program,"tint"),r,g,b);
    glDrawElements(GL_TRIANGLES,cubeIndexCount,
                   GL_UNSIGNED_INT,nullptr);
}

static void drawCity(){
    // Large grassy world base.
    drawBlock(0,-0.25f,-10,70,0.5f,70,0.22f,0.48f,0.19f);

    // Main asphalt road running through the town.
    drawBlock(0,0.015f,-10,6.5f,0.06f,60,
              0.19f,0.20f,0.22f);

    // A crossing road and intersection.
    drawBlock(0,0.02f,-8,38,0.07f,5.0f,
              0.19f,0.20f,0.22f);

    // Sidewalks along the main road.
    drawBlock(-3.65f,0.10f,-10,0.75f,0.20f,60,
              0.58f,0.58f,0.54f);
    drawBlock(3.65f,0.10f,-10,0.75f,0.20f,60,
              0.58f,0.58f,0.54f);

    // Broken white centre line.
    for(int i=0;i<12;i++){
        float z=7.0f-i*4.5f;
        drawBlock(0,0.065f,z,0.13f,0.025f,2.0f,
                  0.95f,0.93f,0.78f);
    }

    // Pedestrian crossing stripes.
    for(int i=0;i<7;i++){
        drawBlock(-1.8f+i*0.6f,0.075f,-7.2f,
                  0.32f,0.025f,1.4f,0.94f,0.94f,0.88f);
    }

    // Buildings on both sides of the street.
    const float zs[] = {4,-3,-13,-22};
    const float heights[] = {3.2f,4.5f,3.7f,5.0f};
    const float widths[] = {3.0f,3.5f,3.0f,3.8f};

    for(int i=0;i<4;i++){
        for(int side=-1;side<=1;side+=2){
            float x=side*7.0f;
            float h=heights[i];
            float w=widths[i];
            float z=zs[i];

            float r=(i%2==0)?0.76f:0.82f;
            float g=(i%2==0)?0.61f:0.72f;
            float b=(i%2==0)?0.43f:0.55f;

            drawBlock(x,h*0.5f,z,w,h,4.0f,r,g,b);

            // Flat roof edge.
            drawBlock(x,h+0.08f,z,w+0.15f,0.16f,4.15f,
                      0.40f,0.39f,0.36f);

            // Front windows facing the road.
            for(int row=0;row<2;row++){
                for(int col=-1;col<=1;col++){
                    drawBlock(x+col*0.82f,1.15f+row*1.15f,
                              z+2.025f,0.43f,0.58f,0.035f,
                              0.25f,0.66f,0.78f);
                }
            }

            // Shop-style door at street level.
            drawBlock(x,0.85f,z+2.04f,0.65f,1.55f,0.06f,
                      0.28f,0.20f,0.14f);
        }
    }

    // Simple street lights.
    for(int i=0;i<6;i++){
        float z=5.0f-i*8.0f;
        for(int side=-1;side<=1;side+=2){
            float x=side*4.25f;
            drawBlock(x,1.35f,z,0.12f,2.7f,0.12f,
                      0.22f,0.23f,0.25f);
            drawBlock(x,2.75f,z,0.55f,0.18f,0.35f,
                      0.98f,0.88f,0.48f);
        }
    }

    // Low park wall and a small park patch.
    drawBlock(13,0.05f,-13,9,0.1f,12,
              0.16f,0.39f,0.16f);
    drawBlock(13,0.28f,-13,8.5f,0.45f,0.10f,
              0.72f,0.72f,0.66f);
    drawBlock(13,0.28f,-7,8.5f,0.45f,0.10f,
              0.72f,0.72f,0.66f);
    drawBlock(8.8f,0.28f,-10,0.10f,0.45f,6,
              0.72f,0.72f,0.66f);
    drawBlock(17.2f,0.28f,-10,0.10f,0.45f,6,
              0.72f,0.72f,0.66f);

    // Tree trunks and leafy crowns.
    for(int i=0;i<4;i++){
        float z=4.0f-i*5.0f;
        for(int side=-1;side<=1;side+=2){
            float x=side*5.2f;
            drawBlock(x,0.85f,z,0.28f,1.7f,0.28f,
                      0.34f,0.20f,0.10f);
            drawPart(x,2.0f,z,0.85f,0.95f,0.85f,
                     0.12f,0.39f,0.12f);
        }
    }
}

static void drawPlayerPart(float x,float y,float z,
                            float sx,float sy,float sz,
                            float r,float g,float b){
    // Turn the whole character when facing the opposite direction.
    float facing = playerFacing;
    float worldX = playerX + x * facing;

    // Gentle body bounce while walking.
    float bob = playerMoving ? fabsf(sinf(animationTime * 2.0f))*0.025f : 0.0f;

    drawPart(worldX,y+jumpY+bob,z*facing,sx,sy,sz,r,g,b);
}

static void drawFrame(){
    if(display==EGL_NO_DISPLAY || surface==EGL_NO_SURFACE)
        return;

    if(!eglMakeCurrent(display,surface,surface,context))
        return;

    // Lightweight animation and jump physics.
    animationTime += 0.055f;
    if(playerMoving) {
        // Walking animation runs while the player is being dragged.
    }
    if(jumpY > 0.0f || jumpVelocity > 0.0f) {
        jumpY += jumpVelocity;
        jumpVelocity -= 0.0065f;
        if(jumpY <= 0.0f) {
            jumpY = 0.0f;
            jumpVelocity = 0.0f;
        }
    }

    glViewport(0,0,width,height);
    glClearColor(0.48f,0.72f,0.94f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    float projection[16],view[16];
    float aspect=(float)width/(float)std::max(height,1);

    perspective(projection,1.05f,aspect,0.1f,100.0f);
    lookAt(view,{0.0f,6.0f,12.0f},
                {0.0f,0.8f,-5.0f},{0.0f,1.0f,0.0f});
    multiply(viewProjection,projection,view);

    glUseProgram(program);
    glBindVertexArray(vao);

    drawCity();

    if (false && isGltfModelLoaded()) {
        drawGltfModel(viewProjection, playerX);
    } else {
    // Shoes
    drawPlayerPart(-0.13f + (playerMoving ? sinf(animationTime*2.0f)*0.085f : 0.0f),0.09f + (playerMoving ? fabsf(sinf(animationTime*2.0f))*0.055f : 0.0f),(playerMoving ? cosf(animationTime*2.0f)*0.065f : 0.0f),
             0.12f,0.09f,0.19f,0.08f,0.08f,0.09f);
    drawPlayerPart(+0.13f - (playerMoving ? sinf(animationTime*2.0f)*0.085f : 0.0f),0.09f + (playerMoving ? fabsf(sinf(animationTime*2.0f))*0.055f : 0.0f),(playerMoving ? -cosf(animationTime*2.0f)*0.065f : 0.0f),
             0.12f,0.09f,0.19f,0.08f,0.08f,0.09f);

    // Legs
    drawPlayerPart(-0.12f + (playerMoving ? sinf(animationTime*2.0f)*0.105f : 0.0f),0.39f + (playerMoving ? fabsf(sinf(animationTime*2.0f))*0.035f : 0.0f),(playerMoving ? cosf(animationTime*2.0f)*0.11f : 0.0f),
             0.095f,0.30f,0.10f,0.12f,0.20f,0.75f);
    drawPlayerPart(+0.12f - (playerMoving ? sinf(animationTime*2.0f)*0.105f : 0.0f),0.39f + (playerMoving ? fabsf(sinf(animationTime*2.0f))*0.035f : 0.0f),(playerMoving ? -cosf(animationTime*2.0f)*0.11f : 0.0f),
             0.095f,0.30f,0.10f,0.12f,0.20f,0.75f);

    // Torso
    drawPlayerPart(0.0f,0.91f,0.0f,
             0.25f,0.36f,0.15f,0.06f,0.13f,0.28f);

    // Shoulders
    drawPlayerPart(-0.28f,1.12f,0.0f,
             0.14f,0.14f,0.14f,0.06f,0.13f,0.28f);
    drawPlayerPart(+0.28f,1.12f,0.0f,
             0.14f,0.14f,0.14f,0.06f,0.13f,0.28f);

    // Arms
    drawPlayerPart(-0.32f,0.87f,(playerMoving ? -sinf(animationTime*2.0f)*0.14f : 0.0f),
             0.085f,0.27f,0.09f,0.76f,0.48f,0.32f);
    drawPlayerPart(+0.32f,0.87f,(playerMoving ? sinf(animationTime*2.0f)*0.14f : 0.0f),
             0.085f,0.27f,0.09f,0.76f,0.48f,0.32f);

    // Neck
    drawPlayerPart(0.0f,1.30f,0.0f,
             0.075f,0.10f,0.075f,0.76f,0.48f,0.32f);

    // Head
    drawPlayerPart(0.0f,1.48f,0.0f,
             0.16f,0.20f,0.15f,0.82f,0.60f,0.43f);

    // Hair
    drawPlayerPart(0.0f,1.64f,-0.005f,
             0.16f,0.065f,0.15f,0.10f,0.065f,0.035f);

    // Eyes
    drawPlayerPart(-0.06f,1.50f,0.139f,
             0.018f,0.023f,0.012f,0.06f,0.04f,0.03f);
    drawPlayerPart(+0.06f,1.50f,0.139f,
             0.018f,0.023f,0.012f,0.06f,0.04f,0.03f);

    }

    eglSwapBuffers(display,surface);
}

static int32_t handleInput(android_app*,AInputEvent* event){
    if(AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION)
        return 0;

    int action=AMotionEvent_getAction(event)&
               AMOTION_EVENT_ACTION_MASK;
    float x=AMotionEvent_getX(event,0);

    if(action==AMOTION_EVENT_ACTION_DOWN){
        touching=true;
        movedTouch=false;
        touchStartX=x;
        lastTouchX=x;
        playerMoving=false;
        return 1;
    }

    if(action==AMOTION_EVENT_ACTION_MOVE && touching){
        float dx=x-lastTouchX;
        if(fabsf(x-touchStartX)>8.0f)
            movedTouch=true;

        if(fabsf(dx)>0.5f){
            playerFacing=(dx<0.0f)?-1.0f:1.0f;
            playerMoving=true;
        }

        playerX+=dx*2.0f/(float)std::max(width,1);
        playerX=std::clamp(playerX,-2.0f,2.0f);
        lastTouchX=x;
        return 1;
    }

    if(action==AMOTION_EVENT_ACTION_UP ||
       action==AMOTION_EVENT_ACTION_CANCEL){
        if(action==AMOTION_EVENT_ACTION_UP && !movedTouch){
            if(jumpY<=0.001f && jumpVelocity<=0.0f)
                jumpVelocity=0.15f;
        }
        touching=false;
        playerMoving=false;
        return 1;
    }

    return 0;
}

static void shutdownGraphics(){
    if(display!=EGL_NO_DISPLAY){
        eglMakeCurrent(display,EGL_NO_SURFACE,
                       EGL_NO_SURFACE,EGL_NO_CONTEXT);

        destroyGltfModel();
        if(cubeVbo) glDeleteBuffers(1,&cubeVbo);
        if(cubeEbo) glDeleteBuffers(1,&cubeEbo);
        if(cubeVao) glDeleteVertexArrays(1,&cubeVao);
        if(program) glDeleteProgram(program);
        if(vbo) glDeleteBuffers(1,&vbo);
        if(ebo) glDeleteBuffers(1,&ebo);
        if(vao) glDeleteVertexArrays(1,&vao);

        if(context!=EGL_NO_CONTEXT)
            eglDestroyContext(display,context);
        if(surface!=EGL_NO_SURFACE)
            eglDestroySurface(display,surface);

        eglTerminate(display);
    }

    display=EGL_NO_DISPLAY;
    surface=EGL_NO_SURFACE;
    context=EGL_NO_CONTEXT;
    program=vao=vbo=ebo=0;
}

static void handleCommand(android_app* app,int32_t command){
    if(command==APP_CMD_INIT_WINDOW && app->window){
        if(!initGraphics(app->window, app->activity->assetManager))
            LOGE("Graphics initialization failed");
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
