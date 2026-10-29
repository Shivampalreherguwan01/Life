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
static float playerX=0.0f, playerZ=0.0f;
static float lastTouchX=0.0f, lastTouchY=0.0f;
static float playerYaw=0.0f;
static bool touching=false;
static bool playerMoving=false;
static bool movedTouch=false;
static float touchStartX=0.0f;
static float playerFacing=1.0f;
static float jumpY=0.0f;
static float jumpVelocity=0.0f;
static float animationTime=0.0f;
static bool joystickActive=false;
static int joystickPointerId=-1;
static float joystickX=0.0f;
static float joystickY=0.0f;
static bool playerRunning=false;
static float cameraYaw=0.0f;
static float cameraPitch=0.12f;
static bool cameraTouchActive=false;
static int cameraPointerId=-1;
static float cameraLastX=0.0f;
static float cameraLastY=0.0f;
static float cameraTouchStartX=0.0f;
static float cameraTouchStartY=0.0f;
static bool cameraTouchMoved=false;

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
uniform float objectAlpha;

out vec4 vColor;

void main(){
    vec3 n=normalize(mat3(model)*normal);
    vec3 light=normalize(vec3(-0.5,1.0,0.7));
    float diffuse=max(dot(n,light),0.0);
    float lighting=0.32+0.68*diffuse;
    vColor=vec4(tint*lighting,objectAlpha);
    gl_Position=mvp*vec4(position,1.0);
}
)";

static const char* fragmentShader=R"(
#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 outputColor;

void main(){
    outputColor=vColor;
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
    glUniform1f(
        glGetUniformLocation(program,"objectAlpha"),1.0f);

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
    glUniform1f(glGetUniformLocation(program,"objectAlpha"),1.0f);
    glDrawElements(GL_TRIANGLES,cubeIndexCount,
                   GL_UNSIGNED_INT,nullptr);
}


static void drawHudPart(float x,float y,float z,
                         float sx,float sy,float sz,
                         float r,float g,float b,float alpha){
    float model[16],mvp[16];
    glBindVertexArray(vao);
    modelMatrix(model,x,y,z,sx,sy,sz);
    multiply(mvp,viewProjection,model);

    glUniformMatrix4fv(glGetUniformLocation(program,"model"),
                       1,GL_FALSE,model);
    glUniformMatrix4fv(glGetUniformLocation(program,"mvp"),
                       1,GL_FALSE,mvp);
    glUniform3f(glGetUniformLocation(program,"tint"),r,g,b);
    glUniform1f(glGetUniformLocation(program,"objectAlpha"),alpha);
    glDrawElements(GL_TRIANGLES,indexCount,GL_UNSIGNED_INT,nullptr);
}

static void drawHudBlock(float x,float y,float z,
                         float sx,float sy,float sz,
                         float r,float g,float b,float alpha){
    float model[16],mvp[16];
    glBindVertexArray(cubeVao);
    modelMatrix(model,x,y,z,sx,sy,sz);
    multiply(mvp,viewProjection,model);

    glUniformMatrix4fv(glGetUniformLocation(program,"model"),
                       1,GL_FALSE,model);
    glUniformMatrix4fv(glGetUniformLocation(program,"mvp"),
                       1,GL_FALSE,mvp);
    glUniform3f(glGetUniformLocation(program,"tint"),r,g,b);
    glUniform1f(glGetUniformLocation(program,"objectAlpha"),alpha);
    glDrawElements(GL_TRIANGLES,cubeIndexCount,
                   GL_UNSIGNED_INT,nullptr);
}

static void drawCity(){
    // Large green terrain.
    drawBlock(0,-0.30f,-35,180,0.6f,180,
              0.20f,0.38f,0.16f);

    // Main roads running through the city.
    for(int road=-1; road<=1; road++){
        float x=road*24.0f;
        drawBlock(x,0.015f,-35,7.5f,0.07f,150,
                  0.13f,0.14f,0.16f);

        // Sidewalks.
        drawBlock(x-4.2f,0.09f,-35,0.8f,0.18f,150,
                  0.53f,0.52f,0.48f);
        drawBlock(x+4.2f,0.09f,-35,0.8f,0.18f,150,
                  0.53f,0.52f,0.48f);

        // Broken lane markings.
        for(int i=0;i<25;i++){
            drawBlock(x,0.065f,25.0f-i*5.0f,
                      0.12f,0.025f,2.1f,
                      0.90f,0.88f,0.76f);
        }
    }

    // Cross streets and junctions.
    for(int i=0;i<6;i++){
        float z=12.0f-i*22.0f;

        drawBlock(0,0.025f,z,82,0.08f,6.5f,
                  0.14f,0.15f,0.17f);

        // Sidewalks along both sides of each cross street.
        drawBlock(0,0.09f,z-3.8f,82,0.18f,0.65f,
                  0.52f,0.51f,0.47f);
        drawBlock(0,0.09f,z+3.8f,82,0.18f,0.65f,
                  0.52f,0.51f,0.47f);

        // Pedestrian crossing stripes.
        for(int k=0;k<8;k++){
            drawBlock(-2.5f+k*0.7f,0.075f,z-4.0f,
                      0.35f,0.025f,1.3f,
                      0.88f,0.87f,0.80f);
        }
    }

    // Buildings: varied heights and widths on both sides.
    for(int row=0;row<6;row++){
        float z=20.0f-row*22.0f;

        for(int side=-1;side<=1;side+=2){
            for(int lane=0;lane<2;lane++){
                float x=side*(8.5f+lane*9.0f);
                float h=3.0f+float((row*3+lane*2+(side>0?1:0))%5)*0.85f;
                float w=5.0f+float((row+lane)%3)*1.1f;
                float d=6.0f;

                // Different facade colours.
                float r,g,b;
                int style=(row+lane+(side>0?1:0))%5;

                if(style==0){r=0.72f;g=0.58f;b=0.43f;}
                else if(style==1){r=0.72f;g=0.73f;b=0.69f;}
                else if(style==2){r=0.65f;g=0.48f;b=0.36f;}
                else if(style==3){r=0.76f;g=0.68f;b=0.54f;}
                else {r=0.53f;g=0.61f;b=0.65f;}

                // Main building.
                drawBlock(x,h*0.5f,z,w,h,d,r,g,b);

                // Roof rim.
                drawBlock(x,h+0.08f,z,w+0.22f,0.16f,d+0.22f,
                          0.35f,0.35f,0.34f);

                // Front windows and dark frames.
                int floors=(int)(h/1.25f);
                for(int floor=0;floor<floors;floor++){
                    for(int col=-1;col<=1;col++){
                        float wx=x+col*(w*0.28f);
                        float wy=0.85f+floor*1.15f;

                        drawBlock(wx,wy,z+d*0.5f+0.035f,
                                  0.88f,0.67f,0.07f,
                                  0.17f,0.20f,0.22f);
                        drawBlock(wx,wy,z+d*0.5f+0.08f,
                                  0.70f,0.49f,0.025f,
                                  0.28f,0.57f,0.68f);
                    }
                }

                // Entrance door.
                drawBlock(x,0.85f,z+d*0.5f+0.07f,
                          0.85f,1.55f,0.10f,
                          0.24f,0.17f,0.12f);

                // Shop awning on selected buildings.
                if((row+lane)%2==0){
                    drawBlock(x,1.85f,z+d*0.5f+0.30f,
                              w*0.72f,0.16f,0.65f,
                              0.68f,0.25f,0.18f);
                }

                // Small rooftop utility structure.
                if(row%2==0){
                    drawBlock(x, h+0.40f,z,
                              1.25f,0.55f,1.3f,
                              0.45f,0.45f,0.43f);
                }
            }
        }
    }

    // Park grounds and paths.
    drawBlock(35,-0.03f,-25,16,0.12f,22,
              0.12f,0.34f,0.12f);
    drawBlock(35,0.045f,-25,2.0f,0.035f,20,
              0.68f,0.61f,0.46f);
    drawBlock(35,0.05f,-25,14,0.035f,1.5f,
              0.68f,0.61f,0.46f);

    // Park benches.
    for(int i=0;i<3;i++){
        float z=-17.0f-i*7.0f;
        drawBlock(31,0.38f,z,1.8f,0.12f,0.55f,
                  0.40f,0.22f,0.10f);
        drawBlock(31,0.62f,z-0.22f,1.8f,0.45f,0.12f,
                  0.40f,0.22f,0.10f);
        for(int leg=-1;leg<=1;leg+=2){
            drawBlock(31+leg*0.65f,0.18f,z,
                      0.10f,0.38f,0.12f,
                      0.18f,0.18f,0.17f);
        }
    }

    // Trees with multiple leafy clusters.
    // Existing animationTime makes the crowns sway slightly in the wind.
    for(int i=0;i<18;i++){
        float z=18.0f-i*7.0f;
        for(int side=-1;side<=1;side+=2){
            float x=side*(5.5f+(i%2)*0.35f);
            float sway=sinf(animationTime*1.35f+i*0.73f)*0.10f;

            // Trunk.
            drawBlock(x,0.90f,z,0.30f,1.8f,0.30f,
                      0.30f,0.18f,0.09f);

            // Main crown and layered leaf clusters.
            drawPart(x+sway,2.10f,z,0.78f,0.88f,0.78f,
                     0.10f,0.31f,0.09f);
            drawPart(x-0.40f+sway,2.00f,z+0.10f,
                     0.52f,0.60f,0.52f,
                     0.13f,0.39f,0.11f);
            drawPart(x+0.38f+sway,2.20f,z-0.08f,
                     0.55f,0.63f,0.55f,
                     0.15f,0.42f,0.12f);
            drawPart(x+sway,2.62f,z+0.12f,
                     0.48f,0.53f,0.48f,
                     0.18f,0.46f,0.14f);
        }
    }

    // Street lights beside the roads.
    for(int i=0;i<12;i++){
        float z=18.0f-i*12.0f;
        for(int side=-1;side<=1;side+=2){
            float x=side*4.9f;
            drawBlock(x,1.45f,z,0.10f,2.9f,0.10f,
                      0.17f,0.18f,0.19f);
            drawBlock(x+side*0.20f,2.92f,z,
                      0.48f,0.12f,0.30f,
                      0.94f,0.85f,0.57f);
        }
    }
}



static void drawPlayerPart(float x,float y,float z,
                           float sx,float sy,float sz,
                           float r,float g,float b){
    // Rotate each body-part position around the character's Y axis.
    float c=cosf(playerYaw);
    float sn=sinf(playerYaw);

    float worldX=playerX+x*c+z*sn;
    float worldZ=playerZ-x*sn+z*c;

    float bob=playerMoving
        ? fabsf(sinf(animationTime*2.0f))*0.025f
        : 0.0f;

    drawPart(worldX,y+jumpY+bob,worldZ,sx,sy,sz,r,g,b);
}


static void drawControls(){
    float saved[16];
    for(int i=0;i<16;i++) saved[i]=viewProjection[i];

    identity(viewProjection);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    float aspect=(float)std::max(height,1)/
                 (float)std::max(width,1);

    // Minimal dark joystick base.
    drawHudPart(-0.70f,-0.57f,0.0f,
                0.245f*aspect,0.245f,0.018f,
                0.035f,0.045f,0.055f,0.66f);

    // One subtle grey inner ring.
    drawHudPart(-0.70f,-0.57f,0.025f,
                0.195f*aspect,0.195f,0.014f,
                0.30f,0.33f,0.36f,0.72f);

    // Dark center under the movable knob.
    drawHudPart(-0.70f,-0.57f,0.045f,
                0.160f*aspect,0.160f,0.014f,
                0.075f,0.085f,0.095f,0.94f);

    // Large neutral joystick knob.
    drawHudPart(
        -0.70f+joystickX*0.145f,
        -0.57f+joystickY*0.145f,
        0.075f,
        0.105f*aspect,0.105f,0.026f,
        joystickActive ? 0.78f : 0.64f,
        joystickActive ? 0.81f : 0.68f,
        joystickActive ? 0.84f : 0.72f,
        0.98f
    );

    // Dark jump button, with a restrained pressed/pulse effect.
    float pulse=0.5f+0.5f*sinf(animationTime*4.0f);
    float pressScale=(jumpY>0.001f || jumpVelocity>0.0f)
                     ? 0.90f : 1.0f;
    float bx=0.72f;
    float by=-0.56f;
    float br=0.165f*pressScale;

    drawHudPart(bx,by,0.0f,
                br*aspect,br,0.018f,
                0.035f,0.045f,0.055f,0.72f);

    drawHudPart(bx,by,0.025f,
                br*0.82f*aspect,br*0.82f,0.014f,
                0.25f,0.28f,0.31f,0.90f);

    // White jumping-person silhouette: head, torso, arms and legs.
    float iconY=by+0.005f+0.008f*pulse;
    float white=0.94f;

    // Head.
    drawHudPart(bx,iconY+0.060f,0.060f,
                0.027f*aspect,0.027f,0.020f,
                white,white,white,1.0f);

    // Torso.
    drawHudPart(bx,iconY+0.012f,0.060f,
                0.025f*aspect,0.048f,0.018f,
                white,white,white,1.0f);

    // Arms angled outward, represented by compact rounded shapes.
    drawHudPart(bx-0.038f*aspect,iconY+0.035f,0.060f,
                0.035f*aspect,0.012f,0.016f,
                white,white,white,1.0f);

    drawHudPart(bx+0.038f*aspect,iconY+0.035f,0.060f,
                0.035f*aspect,0.012f,0.016f,
                white,white,white,1.0f);

    // Legs spread into a jumping pose.
    drawHudPart(bx-0.027f*aspect,iconY-0.040f,0.060f,
                0.014f*aspect,0.040f,0.016f,
                white,white,white,1.0f);

    drawHudPart(bx+0.027f*aspect,iconY-0.040f,0.060f,
                0.014f*aspect,0.040f,0.016f,
                white,white,white,1.0f);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    for(int i=0;i<16;i++) viewProjection[i]=saved[i];
    glEnable(GL_DEPTH_TEST);
}

static void drawFrame(){
    if(display==EGL_NO_DISPLAY || surface==EGL_NO_SURFACE)
        return;

    if(!eglMakeCurrent(display,surface,surface,context))
        return;

    // Continuous movement from the virtual joystick.
    float stickLength=sqrtf(
        joystickX*joystickX+joystickY*joystickY);

    playerMoving=joystickActive && stickLength>0.12f;
    playerRunning=playerMoving && stickLength>0.72f;

    if(playerMoving){
        float moveX=joystickX*cosf(cameraYaw)
                   +joystickY*sinf(cameraYaw);
        float moveZ=-joystickX*sinf(cameraYaw)
                   +joystickY*cosf(cameraYaw);
        float moveLength=sqrtf(moveX*moveX+moveZ*moveZ);

        if(moveLength>1.0f){
            moveX/=moveLength;
            moveZ/=moveLength;
        }

        float speed=playerRunning ? 0.105f : 0.052f;
        playerX+=moveX*speed;
        playerZ+=moveZ*speed;

        playerX=std::clamp(playerX,-42.0f,42.0f);
        playerZ=std::clamp(playerZ,-105.0f,35.0f);

        playerYaw=atan2f(moveX,moveZ);
    }

    animationTime+=playerRunning ? 0.15f :
                   (playerMoving ? 0.085f : 0.035f);

    // Jump physics.
    if(jumpY>0.0f || jumpVelocity>0.0f){
        jumpY+=jumpVelocity;
        jumpVelocity-=0.0065f;
        if(jumpY<=0.0f){
            jumpY=0.0f;
            jumpVelocity=0.0f;
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
    float camDistance=7.5f;
    float horizontalDistance=cosf(cameraPitch)*camDistance;
    float camBackX=-sinf(cameraYaw)*horizontalDistance;
    float camBackZ=-cosf(cameraYaw)*horizontalDistance;
    float camHeight=std::max(0.65f,
        1.7f+sinf(cameraPitch)*camDistance);
    float targetX=playerX;
    float targetZ=playerZ;

    lookAt(view,
        {playerX+camBackX,camHeight+jumpY,playerZ+camBackZ},
        {targetX,1.0f+jumpY,targetZ},
        {0.0f,1.0f,0.0f});
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

    drawControls();
    eglSwapBuffers(display,surface);
}



static int32_t handleInput(android_app*,AInputEvent* event){
    if(AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION)
        return 0;

    int action=AMotionEvent_getAction(event);
    int masked=action&AMOTION_EVENT_ACTION_MASK;
    int index=(action&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
              >>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    if(masked==AMOTION_EVENT_ACTION_DOWN ||
       masked==AMOTION_EVENT_ACTION_POINTER_DOWN){

        float x=AMotionEvent_getX(event,index);
        float y=AMotionEvent_getY(event,index);
        int id=AMotionEvent_getPointerId(event,index);

        // Left lower area: joystick.
        if(x<width*0.42f && y>height*0.48f &&
           !joystickActive){
            joystickActive=true;
            joystickPointerId=id;
            joystickX=0.0f;
            joystickY=0.0f;
            return 1;
        }

        // Right side: camera drag, or jump on a short tap.
        if(x>width*0.48f && !cameraTouchActive){
            cameraTouchActive=true;
            cameraPointerId=id;
            cameraLastX=x;
            cameraLastY=y;
            cameraTouchStartX=x;
            cameraTouchStartY=y;
            cameraTouchMoved=false;
            return 1;
        }
        return 1;
    }

    if(masked==AMOTION_EVENT_ACTION_MOVE){
        int count=AMotionEvent_getPointerCount(event);

        for(int i=0;i<count;i++){
            int id=AMotionEvent_getPointerId(event,i);
            float x=AMotionEvent_getX(event,i);
            float y=AMotionEvent_getY(event,i);

            if(joystickActive && id==joystickPointerId){
                float cx=width*0.16f;
                float cy=height*0.785f;
                float radius=std::max(height*0.125f,1.0f);

                float sx=(x-cx)/radius;
                float sy=(cy-y)/radius;
                float len=sqrtf(sx*sx+sy*sy);
                if(len>1.0f){
                    sx/=len;
                    sy/=len;
                }
                joystickX=sx;
                joystickY=sy;
            }

            if(cameraTouchActive && id==cameraPointerId){
                float dx=x-cameraLastX;
                float dy=y-cameraLastY;

                if(fabsf(x-cameraTouchStartX)>10.0f ||
                   fabsf(y-cameraTouchStartY)>10.0f)
                    cameraTouchMoved=true;

                if(cameraTouchMoved){
                    cameraYaw-=dx*0.006f;
                    cameraPitch+=dy*0.004f;

                    if(cameraPitch< -0.45f) cameraPitch= -0.45f;
                    if(cameraPitch>0.75f) cameraPitch=0.75f;
                }

                cameraLastX=x;
                cameraLastY=y;
            }
        }
        return 1;
    }

    if(masked==AMOTION_EVENT_ACTION_POINTER_UP ||
       masked==AMOTION_EVENT_ACTION_UP ||
       masked==AMOTION_EVENT_ACTION_CANCEL){

        int id=AMotionEvent_getPointerId(event,index);

        if(joystickActive &&
           (id==joystickPointerId ||
            masked==AMOTION_EVENT_ACTION_CANCEL)){
            joystickActive=false;
            joystickPointerId=-1;
            joystickX=joystickY=0.0f;
        }

        if(cameraTouchActive &&
           (id==cameraPointerId ||
            masked==AMOTION_EVENT_ACTION_CANCEL)){
            if(masked!=AMOTION_EVENT_ACTION_CANCEL &&
               !cameraTouchMoved &&
               jumpY<=0.001f && jumpVelocity<=0.0f){
                jumpVelocity=0.15f;
            }
            cameraTouchActive=false;
            cameraPointerId=-1;
        }

        if(masked==AMOTION_EVENT_ACTION_UP ||
           masked==AMOTION_EVENT_ACTION_CANCEL){
            playerMoving=false;
            playerRunning=false;
        }
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
