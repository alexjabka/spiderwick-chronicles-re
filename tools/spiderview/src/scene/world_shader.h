#pragma once
#include "raylib.h"
#include <cmath>

struct RenderSettings {
    bool layerTextures  = true;
    bool layerVertColor = true;
    bool layerLighting  = false;
    float lightBoost    = 2.0f;
    float ambientLevel  = 0.15f;

    bool  dirLightEnable    = false;
    float dirLightYaw       = 0.8f;
    float dirLightPitch     = 0.6f;
    float dirLightIntensity = 0.4f;

    bool  fogEnable  = false;
    float fogColor[3] = {0.55f, 0.62f, 0.70f};
    float fogStart   = 300.0f;
    float fogEnd     = 800.0f;

    bool showGrid      = false;
    bool showWireframe  = false;
    bool showBBoxes     = false;
    bool showArmature   = false;  // NM40 bone skeleton overlay

    // NM40 mode: vertex colors contain encoded normals, use N·L lighting
    bool normalLighting = false;
};

class WorldShader {
public:
    bool Load();
    void Unload();
    void Apply(const RenderSettings& s, const Camera3D& cam);
    void Begin();
    void End();

private:
    Shader shader = {0};
    int locLightBoost = -1, locAmbientLevel = -1;
    int locEnableTex = -1, locEnableVC = -1, locEnableLight = -1, locAlphaClip = -1;
    int locEnableDirL = -1, locLightDir = -1, locDirLightInt = -1;
    int locEnableFog = -1, locFogColor = -1, locFogStart = -1, locFogEnd = -1;
    int locCameraPos = -1;
    int locNormalLighting = -1;
};
