#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cmath>

struct CameraController {
    Camera3D camera;

    // Modes
    bool  freeCam    = false;
    float flySpeed   = 50.0f;
    float flyYaw     = 0.0f;
    float flyPitch   = 0.0f;

    // Orbit
    float   camDist   = 80.0f;
    float   camYaw    = 0.45f;
    float   camPitch  = 0.35f;
    Vector3 camTarget = {0, 5, 0};

    void Init() {
        camera.position   = {60, 40, 60};
        camera.target     = {0, 5, 0};
        camera.up         = {0, 1, 0};
        camera.fovy       = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
    }

    void Update(float dt, bool imguiWantsMouse);
    void ToggleFreeCam();
    void ExitFreeCam();
    void FocusOn(BoundingBox bbox);
    void FrameScene(BoundingBox sceneBounds); // after world load
    void ResetView(BoundingBox sceneBounds);  // Home key
};
