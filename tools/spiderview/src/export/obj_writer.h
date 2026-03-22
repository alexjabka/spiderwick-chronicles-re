#pragma once
// ============================================================================
// obj_writer.h — OBJ+MTL world exporter for SpiderView
// ============================================================================
//
// Exports the currently loaded PCWB world as OBJ+MTL with textures.
// Uses pre-transformed scene data (props and instances already placed).
// Groups objects by texture (one OBJ object per unique texture).
// Exports vertex colors as extended OBJ format: v x y z r g b
// Coordinate conversion: engine Z-up → Blender Y-up at export time.

#include "../scene.h"
#include "../formats.h"
#include <string>

// Export the loaded scene as OBJ+MTL+DDS textures.
// Creates: <outputDir>/<levelName>.obj, <levelName>.mtl, textures/*.dds
// Returns true on success.
bool ExportWorldOBJ(const char* outputDir,
                    const std::string& levelName,
                    const Scene& scene,
                    const PCWBFile& pcwb);
