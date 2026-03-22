#pragma once
// ============================================================================
// app.h — SpiderView application state and actions
// ============================================================================

#include "raylib.h"
#include "scene.h"
#include "vm.h"
#include "scene/camera.h"
#include "scene/world_shader.h"
#include "core/game_fs.h"
#include "core/asset_cache.h"

#include <string>
#include <vector>
#include <cstdint>

// ============================================================================
// MaterialSlot — Blender-style per-submesh material assignment
// ============================================================================
// Each NM40 model has material slots based on mesh table entries:
//   Slot 0 = body (mesh entry 0)  → typically desc[2] 1024x1024 diffuse
//   Slot 1 = head (mesh entry 1+) → typically desc[4] 512x512 detail
// Users can override by selecting a slot and clicking a PCIM in the tree.

struct MaterialSlot {
    std::string name;                // "Body", "Face/Hair"
    int         materialIndex = -1;  // → Scene::materials
    int         texWidth  = 0;
    int         texHeight = 0;
    unsigned int texId    = 0;       // GPU texture ID for ImGui preview
    uint32_t    pcimHash  = 0;       // AWAD nameHash of applied PCIM (for mapping)
    std::vector<int> objectIndices;  // scene object indices using this slot
};

// ============================================================================
// ViewerState — all UI/presentation state, separate from domain model
// ============================================================================

struct ViewerState {
    // Data tab
    std::string dataTitle;
    std::string dataText;

    // Script tab
    std::string scriptText;

    // Hex tab
    std::vector<uint8_t> hexData;
    std::string hexTitle;

    // Texture preview (inline in archive tree)
    Texture2D   previewTex = {0};
    std::string previewName;
    int previewW = 0, previewH = 0;

    // Selection
    int selectedObject = -1;

    // Material slots (NM40 mode)
    std::vector<MaterialSlot> materialSlots;
    int selectedSlot = 0;  // active slot for PCIM assignment
    uint32_t nm40Hash = 0; // AWAD nameHash of loaded NM40 (for mapping info)

    // Archive context for texture picker (set when loading model)
    std::string lastArchivePath;
    FSNode*     lastArchiveNode = nullptr;

    // Thumbnail cache for texture picker {pcimOffset → GPU texture ID}
    std::unordered_map<uint32_t, unsigned int> thumbCache;
    std::unordered_map<uint32_t, int> thumbW, thumbH;
    void ClearThumbs() {
        for (auto& [off, id] : thumbCache)
            if (id > 0) { Texture2D t = {id,0,0,0,0}; UnloadTexture(t); }
        thumbCache.clear(); thumbW.clear(); thumbH.clear();
    }

    // Status bar
    std::string statusMsg;

    // Loaded world name (e.g., "GroundsD")
    std::string worldName;

    void ClearPreview() {
        if (previewTex.id > 0) UnloadTexture(previewTex);
        previewTex = {0}; previewName.clear(); previewW = previewH = 0;
    }
};

// ============================================================================
// App — facade coordinating domain subsystems + viewer state
// ============================================================================

struct App {
    // --- Domain subsystems ---
    Scene            scene;
    KallisVM         vm;
    CameraController cam;
    WorldShader      worldShader;
    RenderSettings   renderSettings;
    GameFileSystem   fs;
    AssetCache       cache;

    // --- Presentation state ---
    ViewerState      view;

    // --- Lifecycle ---
    void Init(int argc, char* argv[]);
    void Shutdown();

    // --- Domain actions ---
    void LoadWorld(const char* path);
    void LoadSCTFromArchive(const std::string& zwdPath, uint32_t offset, uint32_t size);
    void RunAllInits();
    void BuildScriptView();

    // --- Asset operations (go through FormatRegistry) ---
    // Unified view: detects type, calls handler's view(), populates ViewerState
    void ViewAsset(const std::string& zwdPath, uint32_t offset, uint32_t size, AssetType type);
    // Unified export: detects type, uses handler's ext
    bool ExportAsset(const std::string& zwdPath, uint32_t offset, uint32_t size,
                     AssetType type, const std::string& suggestedName);
    // Hex view: loads raw bytes into ViewerState
    void HexViewAsset(const std::string& zwdPath, uint32_t offset, uint32_t size, AssetType type);
    // Texture preview
    void PreviewTexture(const std::string& zwdPath, uint32_t pcimOffset, const FSNode& parent, int childIdx);
    // 3D model preview — loads NM40 into viewport
    void PreviewModel(const std::string& zwdPath, uint32_t offset, uint32_t size,
                      const FSNode& archiveNode);
    // Apply a PCIM texture to the currently loaded model (desc[2] = primary diffuse)
    void ApplyTextureToModel(const std::string& zwdPath, uint32_t pcimOffset, uint32_t pcimSize);
    // Apply detail texture as secondary material for two-pass NM40 (desc[4])
    void ApplyDetailTexture(const std::string& zwdPath, uint32_t pcimOffset, uint32_t pcimSize);
    // Auto-resolve texture for NM40 from same WAD
    void AutoResolveTexture(const std::string& zwdPath, uint32_t nm40Offset, const FSNode& archiveNode);

    // --- Export ---
    void ExportCurrentWorldOBJ();  // exports loaded PCWB world as OBJ+MTL+textures
    void ExportCurrentModelFBX();  // exports loaded NM40 model as FBX with skeleton+skin

    // Retained NM40 mesh result for FBX export (populated during PreviewModel)
    NM40MeshResult lastNM40Result;
    std::string    lastNM40Name;
};

namespace ui {
    void DrawLeftPanel(App& app);
    void DrawRightPanel(App& app);
}
