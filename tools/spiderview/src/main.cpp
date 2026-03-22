// ============================================================================
// main.cpp — SpiderView: Spiderwick Chronicles Game Research Tool
// ============================================================================

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "rlImGui.h"
#include "imgui.h"
#include "app.h"
#include "ui/panels.h"

int main(int argc, char* argv[]) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1920, 1080, "SpiderView — Spiderwick Game Research Tool");
    SetTargetFPS(60);

    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f; style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f; style.ScrollbarRounding = 3.0f;
    style.FramePadding = ImVec2(6, 4); style.ItemSpacing = ImVec2(8, 4);
    style.Colors[ImGuiCol_WindowBg].w = 0.92f;

    App app;
    app.Init(argc, argv);

    while (!WindowShouldClose()) {
        app.cam.Update(GetFrameTime(), io.WantCaptureMouse);

        // Object picking
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !io.WantCaptureMouse) {
            Ray ray = GetScreenToWorldRay(GetMousePosition(), app.cam.camera);
            float cd = 1e9f; int co = -1;
            for (int i = 0; i < (int)app.scene.objects.size(); i++) {
                auto& obj = app.scene.objects[i];
                if (!obj.visible || obj.meshIndex < 0 || obj.meshIndex >= (int)app.scene.meshes.size()) continue;
                BoundingBox bb = obj.bbox;
                BoundingBox dbb;
                dbb.min={fminf(bb.min.x,bb.max.x),fminf(bb.min.z,bb.max.z),fminf(-bb.min.y,-bb.max.y)};
                dbb.max={fmaxf(bb.min.x,bb.max.x),fmaxf(bb.min.z,bb.max.z),fmaxf(-bb.min.y,-bb.max.y)};
                if (!GetRayCollisionBox(ray, dbb).hit) continue;
                auto& m = app.scene.meshes[obj.meshIndex];
                if (!m.vertices || !m.indices) continue;
                for (int t = 0; t < m.triangleCount; t++) {
                    int i0=m.indices[t*3],i1=m.indices[t*3+1],i2=m.indices[t*3+2];
                    Vector3 v0={m.vertices[i0*3],m.vertices[i0*3+2],-m.vertices[i0*3+1]};
                    Vector3 v1={m.vertices[i1*3],m.vertices[i1*3+2],-m.vertices[i1*3+1]};
                    Vector3 v2={m.vertices[i2*3],m.vertices[i2*3+2],-m.vertices[i2*3+1]};
                    RayCollision rc = GetRayCollisionTriangle(ray, v0, v1, v2);
                    if (rc.hit && rc.distance < cd) { cd = rc.distance; co = i; }
                }
            }
            app.view.selectedObject = co;
        }
        if (!app.cam.freeCam && IsKeyPressed(KEY_F) && app.view.selectedObject >= 0 &&
            app.view.selectedObject < (int)app.scene.objects.size())
            app.cam.FocusOn(app.scene.objects[app.view.selectedObject].bbox);

        BeginDrawing();
        ClearBackground({30, 33, 40, 255});

        // 3D viewport
        rlSetClipPlanes(0.1, 10000.0);
        BeginMode3D(app.cam.camera);
        if (app.renderSettings.showGrid) DrawGrid(200, 5.0f);
        if (app.renderSettings.showWireframe) rlEnableWireMode();
        app.worldShader.Apply(app.renderSettings, app.cam.camera);
        app.worldShader.Begin();
        rlDisableBackfaceCulling();
        app.scene.Draw();
        rlEnableBackfaceCulling();
        app.worldShader.End();
        if (app.renderSettings.showWireframe) rlDisableWireMode();

        // Selection wireframe (yellow)
        if (app.view.selectedObject >= 0 && app.view.selectedObject < (int)app.scene.objects.size()) {
            auto& obj = app.scene.objects[app.view.selectedObject];
            if (obj.visible && obj.meshIndex >= 0 && obj.meshIndex < (int)app.scene.meshes.size()) {
                auto& m = app.scene.meshes[obj.meshIndex];
                if (m.vertices && m.indices) {
                    rlEnableWireMode(); rlBegin(RL_TRIANGLES);
                    for (int t = 0; t < m.triangleCount; t++)
                        for (int v = 0; v < 3; v++) {
                            int idx = m.indices[t*3+v];
                            rlColor4ub(255,255,0,255);
                            rlVertex3f(m.vertices[idx*3], m.vertices[idx*3+2], -m.vertices[idx*3+1]);
                        }
                    rlEnd(); rlDisableWireMode();
                }
            }
        }

        // Armature overlay — Blender "Stick" display, X-ray mode
        // Must draw OUTSIDE shader mode for proper depth/blend control
        if (app.renderSettings.showArmature && !app.scene.bones.empty()) {
            // Force flush any pending batches, then disable depth for X-ray
            rlDrawRenderBatchActive();
            rlDisableDepthTest();
            rlSetTexture(0); // unbind textures

            auto& bones = app.scene.bones;

            // Stick bones: lines from parent to child
            rlBegin(RL_LINES);
            for (int bi = 0; bi < (int)bones.size(); bi++) {
                auto& bone = bones[bi];
                if (bone.vertexCount == 0 || bone.parentIndex < 0) continue;
                auto& par = bones[bone.parentIndex];
                if (par.vertexCount == 0) continue;
                rlColor4ub(100, 210, 255, 255);
                rlVertex3f(par.position[0], par.position[2], -par.position[1]);
                rlVertex3f(bone.position[0], bone.position[2], -bone.position[1]);
            }
            rlEnd();
            rlDrawRenderBatchActive(); // flush lines

            // Joint dots — tiny octahedra (Blender stick-bone joint style)
            float r = 0.025f;
            for (int bi = 0; bi < (int)bones.size(); bi++) {
                auto& bone = bones[bi];
                if (bone.vertexCount == 0) continue;
                float x = bone.position[0], y = bone.position[2], z = -bone.position[1];
                bool root = (bone.parentIndex < 0);
                unsigned char cr = root ? 255 : 160, cg = root ? 160 : 235, cb = 255;
                float v[6][3] = {{x+r,y,z},{x-r,y,z},{x,y+r,z},{x,y-r,z},{x,y,z+r},{x,y,z-r}};
                int f[8][3] = {{0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5}};
                rlBegin(RL_TRIANGLES);
                for (int fi = 0; fi < 8; fi++) {
                    rlColor4ub(cr,cg,cb,255);
                    rlVertex3f(v[f[fi][0]][0],v[f[fi][0]][1],v[f[fi][0]][2]);
                    rlVertex3f(v[f[fi][1]][0],v[f[fi][1]][1],v[f[fi][1]][2]);
                    rlVertex3f(v[f[fi][2]][0],v[f[fi][2]][1],v[f[fi][2]][2]);
                }
                rlEnd();
            }
            rlDrawRenderBatchActive(); // flush joints

            rlEnableDepthTest();
        }

        EndMode3D();

        DrawFPS(GetScreenWidth()-100,10);

        // Hotkey overlay (toggle with H)
        static bool showHelp = false;
        if (IsKeyPressed(KEY_H) && !ImGui::GetIO().WantCaptureKeyboard) showHelp = !showHelp;
        if (showHelp) {
            const char* lines[] = {
                "=== NAVIGATION (Blender-style) ===",
                "MMB Drag         Orbit",
                "Shift+MMB Drag   Pan",
                "Ctrl+MMB Drag    Zoom (vertical)",
                "Scroll Wheel     Zoom",
                "Shift+`          Fly Mode (WASD+Mouse)",
                "ESC              Exit Fly Mode",
                "",
                "=== NUMPAD VIEWS ===",
                "Numpad 1         Front",
                "Numpad 3         Right",
                "Numpad 7         Top",
                "Numpad 5         Ortho / Perspective",
                "Numpad .         Reset View",
                "Home             Reset View",
                "",
                "=== FLY MODE ===",
                "W/S              Forward / Back",
                "A/D              Strafe Left / Right",
                "E / Space        Up",
                "Q / Ctrl         Down",
                "Scroll           Adjust Speed",
                "",
                "=== OTHER ===",
                "H                Toggle This Help",
                "Click Object     Select (yellow wireframe)",
            };
            int lineCount = sizeof(lines) / sizeof(lines[0]);
            int boxW = 320, boxH = lineCount * 18 + 20;
            int boxX = (GetScreenWidth() - boxW) / 2;
            int boxY = (GetScreenHeight() - boxH) / 2;
            DrawRectangle(boxX, boxY, boxW, boxH, {0, 0, 0, 220});
            DrawRectangleLines(boxX, boxY, boxW, boxH, {100, 200, 255, 200});
            for (int i = 0; i < lineCount; i++) {
                Color c = (lines[i][0] == '=') ? Color{100,200,255,255} :
                          (lines[i][0] == 0)   ? Color{0,0,0,0} :
                                                  Color{200,200,200,255};
                DrawText(lines[i], boxX + 12, boxY + 10 + i * 18, 14, c);
            }
        }

        rlImGuiBegin();
        ui::DrawLeftPanel(app);
        ui::DrawRightPanel(app);
        rlImGuiEnd();

        // Status bar — drawn AFTER ImGui so it's on top of everything
        const char* mode = app.cam.freeCam ? "FLY" : (app.cam.camera.projection == CAMERA_ORTHOGRAPHIC ? "ORTHO" : "PERSP");
        char statusBar[128];
        snprintf(statusBar, sizeof(statusBar), "SpiderView v1.0 | %s | H=Help", mode);
        DrawRectangle(0, GetScreenHeight()-24, GetScreenWidth(), 24, {0,0,0,200});
        DrawText(statusBar, 10, GetScreenHeight()-19, 14, {180,180,180,255});

        EndDrawing();
    }

    app.Shutdown();
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
