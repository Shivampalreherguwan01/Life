from pathlib import Path
import shutil
import sys

p = Path("app/src/main/cpp/main.cpp")
s = p.read_text()

old_helper = """static void drawPlayerPart(float x,float y,float z,
                            float sx,float sy,float sz,
                            float r,float g,float b){
    drawPart(playerX+x,y+jumpY,z,sx,sy,sz,r,g,b);
}"""

new_helper = """static void drawPlayerPart(float x,float y,float z,
                            float sx,float sy,float sz,
                            float r,float g,float b){
    // Turn the whole character when facing the opposite direction.
    float facing = playerFacing;
    float worldX = playerX + x * facing;

    // Gentle body bounce while walking.
    float bob = playerMoving ? fabsf(sinf(animationTime * 2.0f))*0.025f : 0.0f;

    drawPart(worldX,y+jumpY+bob,z*facing,sx,sy,sz,r,g,b);
}"""

if old_helper not in s:
    sys.exit("ERROR: drawPlayerPart nahi mila. Koi file change nahi ki gayi.")

# Backup before changing anything.
backup = Path("main_before_walk_upgrade.cpp")
if not backup.exists():
    shutil.copy2(p, backup)

s = s.replace(old_helper, new_helper, 1)

replacements = [
    (
        "sinf(animationTime)*0.055f",
        "sinf(animationTime*2.0f)*0.085f"
    ),
    (
        "fabsf(sinf(animationTime))*0.035f",
        "fabsf(sinf(animationTime*2.0f))*0.055f"
    ),
    (
        "cosf(animationTime)*0.045f",
        "cosf(animationTime*2.0f)*0.065f"
    ),
    (
        "sinf(animationTime)*0.075f",
        "sinf(animationTime*2.0f)*0.105f"
    ),
    (
        "fabsf(sinf(animationTime))*0.025f",
        "fabsf(sinf(animationTime*2.0f))*0.035f"
    ),
    (
        "cosf(animationTime)*0.07f",
        "cosf(animationTime*2.0f)*0.11f"
    ),
    (
        "-sinf(animationTime)*0.10f",
        "-sinf(animationTime*2.0f)*0.14f"
    ),
    (
        "sinf(animationTime)*0.10f",
        "sinf(animationTime*2.0f)*0.14f"
    ),
    (
        "0.139f*playerFacing",
        "0.139f"
    ),
]

for old, new in replacements:
    if old not in s:
        sys.exit("ERROR: expected animation code nahi mila: " + old
                 + "\nOriginal file ko change nahi kiya gaya.")
    s = s.replace(old, new)

p.write_text(s)
print("OK: walking animation upgrade applied.")
print("Backup: " + str(backup))
