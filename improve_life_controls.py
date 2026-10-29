from pathlib import Path
import re
import shutil

p = Path("app/src/main/cpp/main.cpp")
s = p.read_text()

# 1. Backup the original source.
backup = Path("main_before_controls_upgrade.cpp")
if not backup.exists():
    shutil.copy2(p, backup)

# 2. Add real alpha support while preserving existing RGB tint calls.
old_shader = '''uniform vec3 tint;

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
)";'''

new_shader = '''uniform vec3 tint;
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
)";'''

if old_shader not in s:
    raise SystemExit("STOP: Shader section did not match. No changes written.")

s = s.replace(old_shader, new_shader, 1)

# Add opaque alpha to the existing draw functions.
old_tint = '''glUniform3f(
        glGetUniformLocation(program,"tint"),r,g,b);'''

new_tint = '''glUniform3f(
        glGetUniformLocation(program,"tint"),r,g,b);
    glUniform1f(
        glGetUniformLocation(program,"objectAlpha"),1.0f);'''

if s.count(old_tint) != 1:
    raise SystemExit("STOP: drawPart tint section was not unique.")
s = s.replace(old_tint, new_tint, 1)

old_block_tint = '''glUniform3f(glGetUniformLocation(program,"tint"),r,g,b);
    glDrawElements(GL_TRIANGLES,cubeIndexCount,'''

new_block_tint = '''glUniform3f(glGetUniformLocation(program,"tint"),r,g,b);
    glUniform1f(glGetUniformLocation(program,"objectAlpha"),1.0f);
    glDrawElements(GL_TRIANGLES,cubeIndexCount,'''

if old_block_tint not in s:
    raise SystemExit("STOP: drawBlock tint section did not match.")
s = s.replace(old_block_tint, new_block_tint, 1)

# Add an alpha-capable helper for the HUD.
anchor = '''static void drawCity(){'''

alpha_helper = '''static void drawHudPart(float x,float y,float z,
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

static void drawCity(){'''

if s.count(anchor) != 1:
    raise SystemExit("STOP: Could not locate drawCity().")
s = s.replace(anchor, alpha_helper, 1)

# 3. Replace the HUD with a larger translucent joystick and animated jump arrow.
start = s.find("static void drawControls(){")
end = s.find("static void drawFrame(){", start)

if start < 0 or end < 0:
    raise SystemExit("STOP: Could not locate drawControls()/drawFrame().")

new_controls = '''static void drawControls(){
    float saved[16];
    for(int i=0;i<16;i++) saved[i]=viewProjection[i];

    identity(viewProjection);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    float aspect=(float)std::max(height,1)/
                 (float)std::max(width,1);

    // Large translucent joystick base and bright outer ring.
    drawHudPart(-0.70f,-0.57f,0.0f,
                0.205f*aspect,0.205f,0.018f,
                0.07f,0.11f,0.17f,0.48f);

    drawHudPart(-0.70f,-0.57f,0.025f,
                0.164f*aspect,0.164f,0.014f,
                0.28f,0.63f,0.94f,0.72f);

    drawHudPart(-0.70f,-0.57f,0.045f,
                0.128f*aspect,0.128f,0.014f,
                0.08f,0.16f,0.24f,0.90f);

    // Larger joystick knob; moves with the user's finger.
    drawHudPart(
        -0.70f+joystickX*0.135f,
        -0.57f+joystickY*0.135f,
        0.075f,
        0.092f*aspect,0.092f,0.025f,
        joystickActive ? 0.20f : 0.36f,
        joystickActive ? 0.78f : 0.53f,
        1.00f,0.98f
    );

    // Jump button: translucent blue disc with a pulsing upward arrow.
    float pulse=0.018f*(0.5f+0.5f*sinf(animationTime*4.0f));
    float jumpIconY=-0.56f+pulse;

    drawHudPart(0.72f,-0.56f,0.0f,
                0.158f*aspect,0.158f,0.018f,
                0.04f,0.20f,0.38f,0.58f);

    drawHudPart(0.72f,-0.56f,0.025f,
                0.132f*aspect,0.132f,0.014f,
                0.08f,0.56f,0.98f,0.88f);

    // Arrow shaft.
    drawHudBlock(0.72f,jumpIconY-0.005f,0.055f,
                 0.025f*aspect,0.065f,0.012f,
                 1.0f,1.0f,1.0f,1.0f);

    // Arrow head.
    drawHudBlock(0.72f,jumpIconY+0.050f,0.055f,
                 0.025f*aspect,0.045f,0.012f,
                 1.0f,1.0f,1.0f,1.0f);

    drawHudBlock(0.72f-0.026f*aspect,jumpIconY+0.033f,0.055f,
                 0.025f*aspect,0.040f,0.012f,
                 1.0f,1.0f,1.0f,1.0f);

    drawHudBlock(0.72f+0.026f*aspect,jumpIconY+0.033f,0.055f,
                 0.025f*aspect,0.040f,0.012f,
                 1.0f,1.0f,1.0f,1.0f);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    for(int i=0;i<16;i++) viewProjection[i]=saved[i];
    glEnable(GL_DEPTH_TEST);
}

'''

s = s[:start] + new_controls + s[end:]

# 4. Convert joystick movement relative to the camera.
pattern = re.compile(
    r'float moveX\s*=\s*joystickX\s*;\s*'
    r'float moveZ\s*=\s*joystickY\s*;\s*'
    r'float moveLength\s*=\s*sqrtf\(moveX\*moveX\+moveZ\*moveZ\);'
)

new_move = """float moveX=joystickX*cosf(cameraYaw)
                   +joystickY*sinf(cameraYaw);
        float moveZ=-joystickX*sinf(cameraYaw)
                   +joystickY*cosf(cameraYaw);
        float moveLength=sqrtf(moveX*moveX+moveZ*moveZ);"""

s, count = pattern.subn(new_move, s, count=1)
if count != 1:
    raise SystemExit("STOP: Movement pattern not found in main.cpp. No game source changes saved.")

# Increase touch radius to match the visually larger joystick.
old_radius = 'float radius=std::max(height*0.078f,1.0f);'
new_radius = 'float radius=std::max(height*0.105f,1.0f);'
if old_radius not in s:
    raise SystemExit("STOP: Joystick touch radius did not match.")
s = s.replace(old_radius, new_radius, 1)

p.write_text(s)
print("LIFE controls updated successfully.")
print("Backup saved as:", backup)
