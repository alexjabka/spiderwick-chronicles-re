#pragma once
// ============================================================================
// scene.h — Scene management: world loading, GPU resources, rendering
// ============================================================================
//
// Owns all GPU resources (meshes, textures, materials) and manages the
// collection of placed objects that make up a loaded world.
//
// Rendering approach:
//   Uses raylib's immediate-mode rlBegin/rlEnd instead of DrawMesh.
//   DrawMesh (VAO/VBO path) produces rendering artifacts with our indexed
//   meshes for reasons not yet diagnosed. The rlBegin path sends vertices
//   directly through raylib's batch renderer and works correctly.
//
// Coordinate conversion:
//   Engine uses Z-up (X=right, Y=forward, Z=up).
//   Raylib uses Y-up (X=right, Y=up, Z=forward).
//   Conversion is applied during Draw(): rlVertex3f(eng_x, eng_z, -eng_y).
//   Vertex data is stored in engine coordinates; conversion happens at render time.

#include "raylib.h"
#include "raymath.h"
#include "formats.h"
#include <string>
#include <vector>
#include <unordered_map>

// ============================================================================
// SceneTexture — GPU-uploaded texture, shared across materials
// ============================================================================
struct SceneTexture {
    Texture2D   texture = {0};
    std::string name;
    int         width  = 0;
    int         height = 0;
};

// ============================================================================
// SceneObject — a placed mesh instance in the world
// ============================================================================
// References shared mesh and material by index. Props have transforms;
// world geometry uses identity. Transform is in engine space (Z-up).
struct SceneObject {
    std::string name;
    int         meshIndex     = -1;   // → Scene::meshes
    int         materialIndex = -1;   // → Scene::materials
    int         texRefKey     = -1;   // texRef key from batch sentinel
    uint32_t    pcrdOffset    = 0;    // PCRD offset in PCWB data
    Matrix      transform;            // D3D9 row-major, stored as raylib Matrix
    BoundingBox bbox;                 // local-space bounding box (engine coords)
    bool        visible  = true;
    bool        selected = false;

    // Two-pass NM40 rendering (engine replica):
    //   Pass 1: materialIndex    → desc[2] main diffuse (1024x1024)
    //   Pass 2: secondaryMatIdx  → desc[4] detail texture (512x512, face/hair)
    // Engine renders ALL sub-batches twice. Alpha channel in each texture
    // controls which body regions are visible per pass (alpha test compositing).
    int         secondaryMatIdx = -1; // → Scene::materials, -1 = no second pass
};

// ============================================================================
// TexDiagnostic — tracks texture loading results for debugging
// ============================================================================
struct TexDiagnostic {
    int         texIndex;
    std::string format;      // "DXT1", "DXT3", "DXT5", "RGB32", etc.
    int         width, height;
    bool        loaded;
    std::string method;      // "raylib", "gpu_fallback", "failed"
};

// ============================================================================
// Scene — owns world state and GPU resources
// ============================================================================
class Scene {
public:
    // GPU resources (shared across objects via index)
    std::vector<Mesh>         meshes;     // CPU+GPU mesh data
    std::vector<Material>     materials;  // materials with textures
    std::vector<SceneTexture> textures;   // GPU textures

    // World objects
    std::vector<SceneObject>  objects;

    // Texture diagnostics
    std::vector<TexDiagnostic> texDiagnostics;
    int texLoadOK   = 0;
    int texLoadFail = 0;

    // Render diagnostics (updated each frame in Draw)
    int objsWithTexture    = 0;
    int objsWithoutTexture = 0;
    int drawTexBindCount   = 0; // rlSetTexture calls per frame

    // PCWB header info (lighting, fog, environment)
    PCWBHeaderInfo headerInfo;

    // Armature (NM40 bone positions for visualization)
    std::vector<NM40Bone> bones;

    // Load a world file (.pcw or .pcwb)
    bool LoadPCWB(const char* path);

    // Release all GPU resources
    void Clear();

    // Render all visible objects (uses rlBegin immediate mode)
    void Draw();

    // Statistics
    int GetObjectCount()    const { return (int)objects.size(); }
    int GetTextureCount()   const { return (int)textures.size(); }
    int GetTotalVertices()  const;
    int GetTotalTriangles() const;

    // Scene bounding box (engine coordinates, not display-space)
    BoundingBox GetSceneBounds() const;

    PCWBFile pcwb;  // retains parsed PCWB data for texture extraction

    int UploadParsedMesh(const ParsedMesh& parsed);
private:
    int LoadSceneTexture(int texIndex);
    int GetOrCreateMaterial(int texIndex);

    std::unordered_map<int, int> materialMap;  // texIndex → materials[] index
    std::unordered_map<int, int> textureMap;   // texIndex → textures[] index
};
