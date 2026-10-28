from pathlib import Path
import shutil
import sys

p = Path("app/src/main/cpp/main.cpp")
s = p.read_text()

start = s.find("static void drawCity(){")
if start < 0:
    sys.exit("ERROR: drawCity() nahi mila; file change nahi hui.")

# Find the closing brace of drawCity() safely.
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
    sys.exit("ERROR: drawCity() ka end nahi mila; file change nahi hui.")

backup = Path("main_before_big_city.cpp")
if not backup.exists():
    shutil.copy2(p, backup)

new_city = r'''
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
'''

s = s[:start] + new_city + s[end:]
p.write_text(s)

print("SUCCESS: Big city map added.")
print("Backup saved as:", backup)
