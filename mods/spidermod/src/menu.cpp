// ============================================================================
// ImGui menu — main Render() loop
// Sub-modules: menu_common, menu_characters, menu_debug, menu_engine
// ============================================================================

#include "menu.h"
#include "menu_common.h"
#include "freecam.h"
#include "addresses.h"
#include "memory.h"
#include "log.h"
#include <cstring>
#include <cstdio>

// Deferred freeze — triggers on the first EndScene after world load
static bool pendingFreeze = false;
// Deferred script debugger restore — re-enable after world load if it was on
static bool pendingDebugRestore = false;

namespace menu {

bool visible = false;
static DWORD startTime = 0;

// World list for selector
static const char* worldList[] = {
    "MansionD", "GroundsD", "GoblCamp", "ThimbleT",
    "MnAttack", "FrstRoad", "DeepWood", "Tnl2Town",
    "MGArena1", "MGArena2", "MGArena3", "MGArena4",
    "Shell"
};
static const int worldCount = sizeof(worldList) / sizeof(worldList[0]);

// Helper: safely disable script debugger before world load
static void DisableScriptDebuggerForLoad() {
    uintptr_t ssPtr = mem::Read<uintptr_t>(addr::pScriptState);
    if (ssPtr && ssPtr > 0x10000 && ssPtr < 0x7FFFFFFF) {
        uint8_t dbgByte = mem::Read<uint8_t>(ssPtr + addr::SCRIPT_DEBUG_OFFSET);
        if (dbgByte & 1) {
            mem::Write<uint8_t>(ssPtr + addr::SCRIPT_DEBUG_OFFSET, dbgByte & ~1);
            pendingDebugRestore = true;
        }
    }
}

// Helper: load world with proper cleanup
static void DoLoadWorld(const char* world, int spawnId, bool freeExplore) {
    log::Write("LoadWorld: world=%s spawnId=%d", world, spawnId);
    if (freecam::enabled) freecam::Toggle();
    DisableScriptDebuggerForLoad();

    typedef void (__cdecl *SetLoadFlagFn)(char);
    typedef int  (__cdecl *LoadWorldFn)(int, int);
    auto setFlag  = reinterpret_cast<SetLoadFlagFn>(addr::fn_SetLoadFlag);
    auto loadWorld = reinterpret_cast<LoadWorldFn>(addr::fn_LoadWorld);

    if (freeExplore) pendingFreeze = true;
    setFlag(1);
    loadWorld(reinterpret_cast<int>(world), spawnId);
}

// Data store helpers
static int DsRead(const char* path) {
    typedef int (__cdecl *HashLookupFn)(const char*);
    typedef int* (__thiscall *GetValueFn)(void*, int);
    auto hashLookup = reinterpret_cast<HashLookupFn>(addr::fn_HashLookup);
    auto getValue   = reinterpret_cast<GetValueFn>(addr::fn_GetValuePtr);
    int idx = hashLookup(path);
    if (idx < 0) return -1;
    int* p = getValue(reinterpret_cast<void*>(addr::pDataStore), idx);
    return p ? *p : -1;
}
static bool DsWrite(const char* path, int val) {
    typedef int (__cdecl *HashLookupFn)(const char*);
    typedef int* (__thiscall *GetValueFn)(void*, int);
    auto hashLookup = reinterpret_cast<HashLookupFn>(addr::fn_HashLookup);
    auto getValue   = reinterpret_cast<GetValueFn>(addr::fn_GetValuePtr);
    int idx = hashLookup(path);
    if (idx < 0) return false;
    int* p = getValue(reinterpret_cast<void*>(addr::pDataStore), idx);
    if (!p) return false;
    *p = val;
    return true;
}

// ============================================================================
// Main render
// ============================================================================
void Render() {
    // --- Deferred actions after world load ---
    if (pendingFreeze && IsWorldValid()) {
        if (!worldPaused) SetWorldPaused(true);
        pendingFreeze = false;
    }
    if (pendingDebugRestore && IsWorldValid()) {
        uintptr_t ssPtr = mem::Read<uintptr_t>(addr::pScriptState);
        if (ssPtr && ssPtr > 0x10000 && ssPtr < 0x7FFFFFFF) {
            uint8_t dbgByte = mem::Read<uint8_t>(ssPtr + addr::SCRIPT_DEBUG_OFFSET);
            mem::Write<uint8_t>(ssPtr + addr::SCRIPT_DEBUG_OFFSET, dbgByte | 1);
        }
        pendingDebugRestore = false;
    }

    // --- Startup notification (fades after 5 sec) ---
    if (!startTime) startTime = GetTickCount();
    DWORD elapsed = GetTickCount() - startTime;
    if (elapsed < 5000) {
        float alpha = elapsed > 4000 ? (5000.0f - elapsed) / 1000.0f : 1.0f;
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.5f * alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::Begin("##status", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "SpiderMod loaded! INSERT = menu");
        ImGui::End();
        ImGui::PopStyleVar();
    }

    // --- FreeCam HUD (always visible when active) ---
    if (freecam::enabled && IsWorldValid()) {
        uintptr_t camObj = mem::Deref(addr::pCameraObj);
        int sector = camObj ? mem::Read<int>(camObj + addr::CAM_SECTOR) : -1;
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(10, io.DisplaySize.y - 30), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin("##fchud", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
            "FreeCam | (%.1f, %.1f, %.1f) | sector %d | speed %.2f",
            freecam::posX, freecam::posY, freecam::posZ, sector, freecam::speed);
        ImGui::End();
    }

    // --- Always-on overlays ---
    Draw3DOverlay();
    DrawDebugOverlay();
    DrawConsoleOverlay();

    if (!visible) return;

    // =====================================================================
    // MAIN MENU
    // =====================================================================
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("SpiderMod", &visible, ImGuiWindowFlags_AlwaysAutoResize);

    // -----------------------------------------------------------------
    // CAMERA — FreeCam + FOV/Clip override
    // -----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        // FreeCam
        bool fc = freecam::enabled;
        if (ImGui::Checkbox("FreeCam##fc", &fc))
            freecam::Toggle();

        if (freecam::enabled) {
            ImGui::SameLine();
            ImGui::SliderFloat("Speed##fc", &freecam::speed, 0.01f, 10.0f, "%.2f");
            ImGui::SliderFloat("Mouse Sens##fc", &freecam::mouseSens, 0.005f, 0.2f, "%.3f");
            ImGui::Text("Pos: %.1f, %.1f, %.1f  Yaw: %.1f  Pitch: %.1f",
                freecam::posX, freecam::posY, freecam::posZ, freecam::yaw, freecam::pitch);
        }

        ImGui::Separator();

        // FOV / Clip override
        uintptr_t camObj = mem::Deref(addr::pCameraObj);

        if (camObj && !g_cameraOverride) {
            float curNear = mem::Read<float>(camObj + 0x00);
            float curFar  = mem::Read<float>(camObj + 0x04);
            float curFovR = mem::Read<float>(camObj + 0x0C);
            if (curNear > 0.001f) g_cameraNear = curNear;
            if (curFar > 1.0f)    g_cameraFar  = curFar;
            if (curFovR > 0.01f)  g_cameraFov  = curFovR * 180.0f / 3.14159265f;
        }

        ImGui::Checkbox("Override##cam", &g_cameraOverride);
        if (g_cameraOverride) {
            ImGui::SliderFloat("FOV##cam", &g_cameraFov, 10.0f, 150.0f, "%.1f");
            ImGui::SliderFloat("Near##cam", &g_cameraNear, 0.01f, 50.0f, "%.2f");
            ImGui::SliderFloat("Far##cam",  &g_cameraFar, 100.0f, 50000.0f, "%.0f");
            if (ImGui::Button("Reset##cam")) { g_cameraFov = 45.0f; g_cameraNear = 2.0f; g_cameraFar = 1024.0f; }
            ImGui::SameLine();
            if (ImGui::Button("Ultra Far##cam")) g_cameraFar = 50000.0f;
            ImGui::SameLine();
            if (ImGui::Button("Wide##cam")) g_cameraFov = 90.0f;
        }

        if (camObj) {
            float fov = mem::Read<float>(camObj + 0x0C) * 180.0f / 3.14159265f;
            float n = mem::Read<float>(camObj + 0x00);
            float f = mem::Read<float>(camObj + 0x04);
            ImGui::TextDisabled("Live: FOV=%.1f  Near=%.2f  Far=%.0f", fov, n, f);
        }
    }

    // -----------------------------------------------------------------
    // CHARACTERS — table + hot-switch
    // -----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Characters"))
        DrawCharacterList();

    // -----------------------------------------------------------------
    // WORLD — selector + game state + reload
    // -----------------------------------------------------------------
    if (ImGui::CollapsingHeader("World")) {
        const char* curLevel = GetCurrentLevelName();
        ImGui::Text("Current: %s (%s)", WorldFriendlyName(curLevel), curLevel);

        // Story state
        int chapter = DsRead("/Story/Chapter");
        int storyIdx = DsRead("/Story/Index");
        ImGui::Text("Chapter: %d  Index: %d", chapter, storyIdx);

        // Quick chapter presets
        for (int ch = 1; ch <= 8; ch++) {
            if (ch > 1) ImGui::SameLine();
            char lbl[16]; snprintf(lbl, sizeof(lbl), "Ch%d##gs", ch);
            if (ImGui::Button(lbl)) { DsWrite("/Story/Chapter", ch); DsWrite("/Story/Index", ch * 200); }
        }
        // Manual chapter + index with +/- buttons
        static int manualChapter = 7;
        static int manualIndex = 1200;
        if (ImGui::Button("-##chm2")) manualChapter--;
        ImGui::SameLine(); ImGui::Text("Ch:%d", manualChapter);
        ImGui::SameLine(); if (ImGui::Button("+##chp2")) manualChapter++;
        ImGui::SameLine(); if (ImGui::Button("-##idm")) manualIndex -= 100;
        ImGui::SameLine(); ImGui::Text("Idx:%d", manualIndex);
        ImGui::SameLine(); if (ImGui::Button("+##idp")) manualIndex += 100;
        ImGui::SameLine();
        if (ImGui::Button("Set##gs")) { DsWrite("/Story/Chapter", manualChapter); DsWrite("/Story/Index", manualIndex); }

        ImGui::Separator();

        // World selector
        static int selectedWorld = 0;
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##world", WorldFriendlyName(worldList[selectedWorld]))) {
            for (int i = 0; i < worldCount; i++) {
                bool selected = (i == selectedWorld);
                char label[64];
                snprintf(label, sizeof(label), "%s (%s)", WorldFriendlyName(worldList[i]), worldList[i]);
                if (ImGui::Selectable(label, selected))
                    selectedWorld = i;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        static int spawnId = 0;
        ImGui::SetNextItemWidth(40);
        ImGui::InputInt("##spn", &spawnId, 0, 0);
        if (spawnId < 0) spawnId = 0;

        static bool freeExplore = false;
        ImGui::Checkbox("Free Explore##world", &freeExplore);
        ImGui::SameLine();
        if (ImGui::Button("Load##world"))
            DoLoadWorld(worldList[selectedWorld], spawnId, freeExplore);
        ImGui::SameLine();
        if (ImGui::Button("Reload##world") && IsWorldValid())
            DoLoadWorld(GetCurrentLevelName(), 0, false);

    }

    // -----------------------------------------------------------------
    // CHEATS
    // -----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Cheats"))
        DrawCheatsSection();

    // -----------------------------------------------------------------
    // RENDERING — engine globals + D3D overrides
    // -----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Rendering"))
        DrawRenderSection();

    // -----------------------------------------------------------------
    // DEBUG — overlays + sector info + spawn + diagnostics + console
    // -----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Debug")) {
        // 3D overlay
        ImGui::Checkbox("3D Overlay##dbg", &dbg3d_enabled);
        if (dbg3d_enabled) {
            ImGui::SameLine();
            ImGui::Checkbox("Chars##3d", &dbg3d_characters);
            ImGui::SameLine();
            ImGui::Checkbox("Sectors##3d", &dbg3d_sectors);
            ImGui::SameLine();
            ImGui::Checkbox("Props##3d", &dbg3d_props);
            ImGui::SameLine();
            ImGui::Checkbox("Chunks##3d", &dbg3d_worldChunks);
            ImGui::SameLine();
            ImGui::Checkbox("VM##3d", &dbg3d_vmPlacements);
            ImGui::Checkbox("Coords##3d", &dbg3d_showCoords);
            ImGui::SameLine();
            ImGui::Checkbox("Addrs##3d", &dbg3d_showAddrs);
        }

        // Force all characters visible (overrides chapter-gated invisibility)
        static bool forceAllVisible = false;
        if (ImGui::Checkbox("Force All Visible##fav", &forceAllVisible)) {
            log::Write("Force All Visible: %s", forceAllVisible ? "ON" : "OFF");
        }
        if (forceAllVisible) {
            ImGui::SameLine();
            // Walk character linked list, call FullActivate on each
            typedef void (__thiscall *FullActivate_t)(void*);
            auto pActivate = reinterpret_cast<FullActivate_t>(addr::fn_FullActivate);
            uintptr_t charHead = mem::Read<uintptr_t>(addr::pCharacterListHead);
            int activated = 0;
            uintptr_t cur = charHead;
            while (cur && cur > 0x10000 && cur < 0x7FFFFFFF && activated < 200) {
                __try {
                    // Check if character has render components
                    uint32_t renderCount = mem::Read<uint32_t>(cur + addr::CHAR_RENDER_COUNT);
                    if (renderCount == 0) {
                        // No render components — try creating them
                        typedef void (__thiscall *CreateRenderComps_t)(void*);
                        auto pCreate = reinterpret_cast<CreateRenderComps_t>(addr::fn_CreateRenderComps);
                        pCreate(reinterpret_cast<void*>(cur));
                    }
                    pActivate(reinterpret_cast<void*>(cur));
                    activated++;
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                cur = mem::Read<uintptr_t>(cur + addr::CHAR_NEXT);
                if (cur == charHead) break; // circular list
            }
            ImGui::Text("%d chars", activated);
        }

        // Debug overlay + console toggles
        DrawDebugSectionToggles();
        DrawConsoleToggle();

        ImGui::Separator();

        // Sector table + raw data
        if (ImGui::TreeNode("Sector & Engine Info")) {
            DrawDebugSection();
            ImGui::TreePop();
        }
    }

    // -----------------------------------------------------------------
    // CONTROLS
    // -----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Controls")) {
        ImGui::BulletText("INSERT: Toggle menu");
        ImGui::BulletText("HOME: Toggle FreeCam");
        ImGui::BulletText("PAUSE: Freeze/Resume world");
        ImGui::BulletText("F12: Screenshot");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "FreeCam:");
        ImGui::BulletText("WASD + Mouse: Fly");
        ImGui::BulletText("Space/Ctrl: Up/Down  Shift: Fast");
        ImGui::BulletText("Numpad +/-: Speed  MMB: TP player");
    }

    ImGui::End();
}

} // namespace menu
