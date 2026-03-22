#pragma once
// ============================================================================
// Shared declarations for all menu sub-modules
// ============================================================================

#include <imgui.h>
#include <cstdint>

// --- Externs from dllmain.cpp ---
extern bool worldPaused;
void SetWorldPaused(bool pause);
extern volatile int g_pendingSwitchType;
extern volatile uintptr_t g_pendingSwitchTarget;
extern bool  g_cameraOverride;
extern float g_cameraFov, g_cameraNear, g_cameraFar;
extern float g_vpMatrix[16];
extern bool  g_matricesValid;
extern short g_vpWidth, g_vpHeight;

// --- Utility functions (menu_common.cpp) ---
bool        IsWorldValid();
const char* GetSectorName(int sectorIdx);
const char* GetCurrentLevelName();
const char* WorldFriendlyName(const char* internal);
bool        WorldToScreen(float wx, float wy, float wz, float& sx, float& sy);
void        DrawText3D(ImDrawList* dl, float wx, float wy, float wz,
                       const char* text, ImU32 color = IM_COL32(255,255,255,200));
const char* IdentifyCharacter(uintptr_t charObj);
bool        IsPlayerCharacter(uintptr_t charObj);

// --- Section draw functions (from sub-modules) ---
void DrawCharacterList();       // menu_characters.cpp
void DrawDebugSection();        // menu_debug.cpp
void DrawDebugOverlay();        // menu_debug.cpp
void Draw3DOverlay();           // menu_debug.cpp
void DrawCheatsSection();       // menu_engine.cpp — cheat flag toggles
void DrawRenderSection();       // menu_engine.cpp — fog/snow/rim/wireframe
void DrawConsoleOverlay();      // menu_engine.cpp — always-on overlay
void CaptureConsoleLines();     // menu_engine.cpp — poll engine ring buffer
void DrawDebugSectionToggles(); // menu_debug.cpp — debug overlay toggle
void DrawConsoleToggle();       // menu_engine.cpp — console overlay toggle

// D3D wireframe toggle (menu_engine.cpp, applied from dllmain.cpp EndScene)
extern bool g_dbgWireframe;

// VM prop placements (dllmain.cpp sauSetPosition/sauSetRotation hooks)
struct PropPlacement {
    uintptr_t objPtr;
    float pos[3];
    float rot[3];
    bool hasPos;
    bool hasRot;
    char name[48];   // object name read from memory
};
constexpr int MAX_PROP_PLACEMENTS = 2048;
extern PropPlacement g_propPlacements[MAX_PROP_PLACEMENTS];
extern volatile long g_propPlacementCount;
extern bool g_propCaptureActive;

// Geometry instance captures (dllmain.cpp GeomInstance_Init hook)
struct GeomInstance {
    float worldMatrix[16];
    uintptr_t addr;
    uint32_t vc;       // vertex count at instance+0x70
    uint32_t ic;       // index/prim count at instance+0x74
    uint32_t flags;    // flags/offset at instance+0x7C
};
constexpr int MAX_GEOM_INSTANCES = 2048;
extern GeomInstance g_geomInstances[MAX_GEOM_INSTANCES];
extern volatile long g_geomInstanceCount;
extern uintptr_t g_pcwbBase;  // PCWB buffer base address (computed from first geom instance)

// --- 3D overlay state (shared between menu.cpp and menu_debug.cpp) ---
extern bool dbg3d_enabled;
extern bool dbg3d_characters;
extern bool dbg3d_sectors;
extern bool dbg3d_showCoords;
extern bool dbg3d_showAddrs;
extern bool dbg3d_props;
extern bool dbg3d_worldChunks;
extern bool dbg3d_vmPlacements;
