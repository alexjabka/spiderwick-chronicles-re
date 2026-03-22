#include "camera.h"
#include "imgui.h"

void CameraController::ToggleFreeCam() {
    freeCam = !freeCam;
    if (freeCam) {
        DisableCursor();
        Vector3 dir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        flyYaw   = atan2f(dir.x, dir.z);
        flyPitch = asinf(Clamp(dir.y, -0.99f, 0.99f));
    } else {
        ExitFreeCam();
    }
}

void CameraController::ExitFreeCam() {
    freeCam = false;
    EnableCursor();
    Vector3 dir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    camTarget = Vector3Add(camera.position, Vector3Scale(dir, camDist));
    camYaw   = atan2f(-dir.x, -dir.z);
    camPitch = asinf(Clamp(-dir.y, -0.99f, 0.99f));
}

void CameraController::FocusOn(BoundingBox bbox) {
    camTarget = {
        (bbox.min.x + bbox.max.x) * 0.5f,
        (bbox.min.y + bbox.max.y) * 0.5f,
        (bbox.min.z + bbox.max.z) * 0.5f
    };
    camDist = Vector3Length(Vector3Subtract(bbox.max, bbox.min)) * 1.2f;
    if (camDist < 2.0f) camDist = 2.0f;
}

void CameraController::FrameScene(BoundingBox bb) {
    float cx = (bb.min.x + bb.max.x) * 0.5f;
    float cy = (bb.min.y + bb.max.y) * 0.5f;
    float cz = (bb.min.z + bb.max.z) * 0.5f;
    camTarget = {cx, cy, cz};

    float dx = bb.max.x - bb.min.x;
    float dy = bb.max.y - bb.min.y;
    float dz = bb.max.z - bb.min.z;
    float diag = sqrtf(dx*dx + dy*dy + dz*dz);
    camDist = fmaxf(diag * 1.5f, 2.0f);

    camPitch  = 0.3f;
    camYaw    = 0.8f;
    freeCam   = false;
}

void CameraController::ResetView(BoundingBox bb) {
    FrameScene(bb);
}

void CameraController::Update(float dt, bool imguiWantsMouse) {
    ImGuiIO& io = ImGui::GetIO();

    // =====================================================================
    // Blender-style controls:
    //   MMB drag        = orbit (rotate around pivot)
    //   Shift+MMB drag  = pan (translate pivot in screen plane)
    //   Ctrl+MMB drag   = zoom (vertical drag = dolly in/out)
    //   Scroll wheel    = zoom
    //   Numpad .        = focus selected object
    //   Numpad 1/3/7    = front/right/top views
    //   Numpad 5        = toggle ortho/perspective
    //   Shift+`         = fly mode (WASD + mouse look)
    // =====================================================================

    // Shift+` toggle fly mode
    if (IsKeyPressed(KEY_GRAVE) && io.KeyShift)
        ToggleFreeCam();

    // ESC exits fly mode
    if (freeCam && IsKeyPressed(KEY_ESCAPE))
        ExitFreeCam();

    if (freeCam) {
        // --- Fly mode (Blender Shift+~ navigation) ---
        Vector2 md = GetMouseDelta();
        flyYaw   -= md.x * 0.002f;
        flyPitch -= md.y * 0.002f;
        flyPitch  = Clamp(flyPitch, -1.5f, 1.5f);

        Vector3 fwd = {
            cosf(flyPitch) * sinf(flyYaw),
            sinf(flyPitch),
            cosf(flyPitch) * cosf(flyYaw)
        };
        Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, {0, 1, 0}));

        float wheel = GetMouseWheelMove();
        if (wheel != 0) flySpeed = Clamp(flySpeed * (1 + wheel * 0.3f), 1.f, 2000.f);

        float spd = flySpeed * dt;
        Vector3& pos = camera.position;
        if (IsKeyDown(KEY_W)) pos = Vector3Add(pos, Vector3Scale(fwd, spd));
        if (IsKeyDown(KEY_S)) pos = Vector3Add(pos, Vector3Scale(fwd, -spd));
        if (IsKeyDown(KEY_A)) pos = Vector3Add(pos, Vector3Scale(right, -spd));
        if (IsKeyDown(KEY_D)) pos = Vector3Add(pos, Vector3Scale(right, spd));
        if (IsKeyDown(KEY_E) || IsKeyDown(KEY_SPACE)) pos.y += spd;
        if (IsKeyDown(KEY_Q) || IsKeyDown(KEY_LEFT_CONTROL)) pos.y -= spd;

        camera.target = Vector3Add(pos, fwd);
    } else {
        // --- Orbit mode (Blender viewport navigation) ---
        if (imguiWantsMouse) goto after_orbit;

        // MMB = orbit (no modifiers)
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) && !io.KeyShift && !io.KeyCtrl) {
            Vector2 d = GetMouseDelta();
            camYaw   -= d.x * 0.005f;
            camPitch += d.y * 0.005f;
            camPitch = Clamp(camPitch, -1.55f, 1.55f);
        }

        // Shift+MMB = pan (translate pivot in screen plane)
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) && io.KeyShift) {
            Vector2 d = GetMouseDelta();
            Vector3 fwd   = Vector3Normalize(Vector3Subtract(camera.position, camTarget));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(camera.up, fwd));
            Vector3 up2   = Vector3CrossProduct(fwd, right);
            float spd     = camDist * 0.002f;
            camTarget = Vector3Add(camTarget, Vector3Scale(right, -d.x * spd));
            camTarget = Vector3Add(camTarget, Vector3Scale(up2, d.y * spd));
        }

        // Ctrl+MMB = zoom (vertical mouse drag)
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) && io.KeyCtrl) {
            Vector2 d = GetMouseDelta();
            float zoomFactor = 1.0f + d.y * 0.005f;
            camDist *= zoomFactor;
            camDist = Clamp(camDist, 0.1f, 50000.0f);
        }

        // Scroll wheel = zoom (Blender-style: smooth, distance-proportional)
        {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                float zoomFactor = 1.0f - wheel * 0.08f;
                camDist *= zoomFactor;
                camDist = Clamp(camDist, 0.1f, 50000.0f);
            }
        }

        // Numpad views (Blender standard)
        if (IsKeyPressed(KEY_KP_1)) { // Front view
            camYaw = 0.0f; camPitch = 0.0f;
        }
        if (IsKeyPressed(KEY_KP_3)) { // Right view
            camYaw = -1.5708f; camPitch = 0.0f;
        }
        if (IsKeyPressed(KEY_KP_7)) { // Top view
            camYaw = 0.0f; camPitch = 1.55f;
        }
        if (IsKeyPressed(KEY_KP_5)) { // Toggle ortho/perspective
            if (camera.projection == CAMERA_PERSPECTIVE)
                camera.projection = CAMERA_ORTHOGRAPHIC;
            else
                camera.projection = CAMERA_PERSPECTIVE;
        }
        // Numpad . = focus (same as Home)
        if (IsKeyPressed(KEY_KP_DECIMAL) || IsKeyPressed(KEY_HOME)) {
            camPitch = 0.3f;
            camYaw   = 0.8f;
        }

        after_orbit:
        // Compute camera position from orbit parameters
        camera.target   = camTarget;
        camera.position = {
            camTarget.x + camDist * cosf(camPitch) * sinf(camYaw),
            camTarget.y + camDist * sinf(camPitch),
            camTarget.z + camDist * cosf(camPitch) * cosf(camYaw),
        };
    }
}
