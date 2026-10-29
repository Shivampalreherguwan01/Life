from pathlib import Path
import shutil
import sys

p = Path("app/src/main/cpp/main.cpp")
s = p.read_text()

def replace_once(old, new, label):
    global s
    count = s.count(old)
    if count != 1:
        sys.exit(
            "ERROR: " + label + " ka exact code nahi mila "
            "(matches=" + str(count) + "). File change nahi hui."
        )
    s = s.replace(old, new, 1)

# 1. Add world depth and body rotation.
replace_once(
    "static float playerX=0.0f,lastTouchX=0.0f;",
    """static float playerX=0.0f, playerZ=0.0f;
static float lastTouchX=0.0f, lastTouchY=0.0f;
static float playerYaw=0.0f;""",
    "player position"
)

# 2. Replace player-part placement with 3D rotation.
start = s.find("static void drawPlayerPart(")
end = s.find("\nstatic void drawFrame()", start)

if start < 0 or end < 0:
    sys.exit("ERROR: drawPlayerPart/drawFrame nahi mila. File change nahi hui.")

new_helper = r'''
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
'''
s = s[:start] + new_helper + s[end:]

# 3. Make the camera follow the player from behind.
replace_once(
    """    lookAt(view,{0.0f,6.0f,12.0f},
                {0.0f,0.8f,-5.0f},{0.0f,1.0f,0.0f});""",
    """    float camBackX=-sinf(playerYaw)*10.0f;
    float camBackZ=-cosf(playerYaw)*10.0f;
    float targetX=playerX+sinf(playerYaw)*2.0f;
    float targetZ=playerZ+cosf(playerYaw)*2.0f;

    lookAt(view,
        {playerX+camBackX,5.5f+jumpY,playerZ+camBackZ},
        {targetX,0.9f+jumpY,targetZ},
        {0.0f,1.0f,0.0f});""",
    "follow camera"
)

# 4. Replace the complete touch handler.
start = s.find("static int32_t handleInput(")
if start < 0:
    sys.exit("ERROR: handleInput nahi mila. File change nahi hui.")

brace = s.find("{", start)
depth = 0
end = -1

for i in range(brace, len(s)):
    if s[i] == "{":
        depth += 1
    elif s[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break

if end < 0:
    sys.exit("ERROR: handleInput ka end nahi mila. File change nahi hui.")

new_handler = r'''
static int32_t handleInput(android_app*,AInputEvent* event){
    if(AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION)
        return 0;

    int action=AMotionEvent_getAction(event)&
               AMOTION_EVENT_ACTION_MASK;
    float x=AMotionEvent_getX(event,0);
    float y=AMotionEvent_getY(event,0);

    if(action==AMOTION_EVENT_ACTION_DOWN){
        touching=true;
        movedTouch=false;
        touchStartX=x;
        lastTouchX=x;
        lastTouchY=y;
        playerMoving=false;
        return 1;
    }

    if(action==AMOTION_EVENT_ACTION_MOVE && touching){
        float dx=x-lastTouchX;
        float dy=y-lastTouchY;

        if(fabsf(x-touchStartX)>8.0f ||
           fabsf(y-lastTouchY)>8.0f)
            movedTouch=true;

        // Screen drag controls movement on the ground plane.
        // Up = forward (+Z); down = backward (-Z).
        float moveX=dx*0.025f;
        float moveZ=-dy*0.025f;

        playerX+=moveX;
        playerZ+=moveZ;

        // Keep the player inside the current city area.
        playerX=std::clamp(playerX,-42.0f,42.0f);
        playerZ=std::clamp(playerZ,-105.0f,35.0f);

        float distance=sqrtf(moveX*moveX+moveZ*moveZ);
        if(distance>0.0005f){
            playerYaw=atan2f(moveX,moveZ);
            playerMoving=true;
        }

        lastTouchX=x;
        lastTouchY=y;
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
'''
s = s[:start] + new_handler + s[end:]

# Save only after all replacements succeeded.
backup = Path("main_before_3d_movement.cpp")
if not backup.exists():
    shutil.copy2(p, backup)

p.write_text(s)
print("SUCCESS: 3D player movement and follow camera updated.")
print("Backup:", backup)
