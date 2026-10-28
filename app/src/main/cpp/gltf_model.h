#pragma once

#include <android/asset_manager.h>
#include <GLES3/gl3.h>

bool initGltfModel(AAssetManager* assets);
bool isGltfModelLoaded();
void drawGltfModel(const float* viewProjection, float playerX);
void destroyGltfModel();
