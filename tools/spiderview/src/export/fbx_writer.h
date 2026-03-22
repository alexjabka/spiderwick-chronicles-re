#pragma once
// ============================================================================
// fbx_writer.h — ASCII FBX 7.4 writer for skinned NM40 character models
// ============================================================================
//
// Exports NM40 character models as ASCII FBX with:
//   - Mesh geometry (positions, normals, UVs, triangle indices)
//   - Skeleton (bone hierarchy as LimbNode chain)
//   - Skin weights (per-vertex bone influences)
//   - Materials + DDS texture references
//
// Coordinate conversion: engine Z-up → FBX/Blender Y-up at export time.
// No external library needed — pure C++ stdio output.

#include "../formats.h"
#include <string>
#include <vector>

struct FBXMaterial {
    std::string name;        // "Body", "Head"
    std::string texturePath; // relative path to DDS file, or empty
};

// Export an NM40 model as ASCII FBX 7.4.
// Creates: <path> (single .fbx file). DDS textures should be exported separately.
// Returns true on success.
bool ExportNM40FBX(const char* path,
                   const NM40MeshResult& meshResult,
                   const std::vector<FBXMaterial>& materials);
