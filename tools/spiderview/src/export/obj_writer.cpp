// ============================================================================
// obj_writer.cpp — OBJ+MTL world exporter
// ============================================================================
//
// Exports the loaded Scene as OBJ + MTL + DDS textures.
// Groups scene objects by texture (one OBJ group per unique texture).
// Vertices are converted from engine Z-up to Blender Y-up.
// Vertex colors exported as extended OBJ: v x y z r g b

#include "obj_writer.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

bool ExportWorldOBJ(const char* outputDir,
                    const std::string& levelName,
                    const Scene& scene,
                    const PCWBFile& pcwb) {

    // Create output directories
    std::string baseDir = std::string(outputDir) + "/" + levelName;
    std::string texDir  = baseDir + "/textures";
    MKDIR(outputDir);
    MKDIR(baseDir.c_str());
    MKDIR(texDir.c_str());

    std::string objPath = baseDir + "/" + levelName + ".obj";
    std::string mtlPath = baseDir + "/" + levelName + ".mtl";
    std::string mtlName = levelName + ".mtl";

    // Group objects by texRefKey (one texture = one OBJ group)
    struct TexGroup {
        int texRefKey;
        std::vector<int> objectIndices; // indices into scene.objects
    };
    std::map<int, TexGroup> groups; // texRefKey → group

    for (int oi = 0; oi < (int)scene.objects.size(); oi++) {
        auto& obj = scene.objects[oi];
        if (!obj.visible) continue;
        if (obj.meshIndex < 0 || obj.meshIndex >= (int)scene.meshes.size()) continue;
        int key = obj.texRefKey;
        groups[key].texRefKey = key;
        groups[key].objectIndices.push_back(oi);
    }

    // Export DDS textures and build material info
    struct MatInfo {
        std::string matName;
        std::string texFile; // relative path from OBJ
        int width, height;
        bool hasAlpha;
    };
    std::map<int, MatInfo> materials; // texRefKey → material

    for (auto& [key, group] : groups) {
        MatInfo mi;
        mi.width = mi.height = 0;
        mi.hasAlpha = false;
        mi.texFile = "";

        if (key >= 0) {
            auto trIt = pcwb.texRef.find(key);
            if (trIt != pcwb.texRef.end()) {
                uint32_t pcimOff = trIt->second;
                const uint8_t* ddsData = nullptr;
                uint32_t ddsSize = 0, w = 0, h = 0;
                if (pcwb.ExtractDDS(pcimOff, &ddsData, &ddsSize, &w, &h) && ddsData && ddsSize > 0) {
                    mi.width = (int)w;
                    mi.height = (int)h;

                    char texName[64];
                    snprintf(texName, sizeof(texName), "tex_%03d_%dx%d.dds", key, (int)w, (int)h);
                    mi.texFile = std::string("textures/") + texName;

                    std::string texPath = texDir + "/" + texName;
                    FILE* tf = fopen(texPath.c_str(), "wb");
                    if (tf) {
                        fwrite(ddsData, 1, ddsSize, tf);
                        fclose(tf);
                    }
                }
            }
        }

        char matNameBuf[32];
        snprintf(matNameBuf, sizeof(matNameBuf), "tex_%03d", key >= 0 ? key : 0);
        mi.matName = matNameBuf;
        materials[key] = mi;
    }

    // Write MTL file
    FILE* mtl = fopen(mtlPath.c_str(), "w");
    if (!mtl) return false;

    fprintf(mtl, "# SpiderView World Export — %s\n", levelName.c_str());
    fprintf(mtl, "# %d materials\n\n", (int)materials.size());

    for (auto& [key, mi] : materials) {
        fprintf(mtl, "newmtl %s\n", mi.matName.c_str());
        fprintf(mtl, "Ka 0.1 0.1 0.1\n");
        fprintf(mtl, "Kd 1.0 1.0 1.0\n");
        fprintf(mtl, "Ks 0.0 0.0 0.0\n");
        fprintf(mtl, "d 1.0\n");
        if (!mi.texFile.empty()) {
            fprintf(mtl, "map_Kd %s\n", mi.texFile.c_str());
        }
        fprintf(mtl, "\n");
    }
    fclose(mtl);

    // Write OBJ file
    FILE* obj = fopen(objPath.c_str(), "w");
    if (!obj) return false;

    fprintf(obj, "# SpiderView World Export — %s\n", levelName.c_str());
    fprintf(obj, "# %d objects, %d textures\n", (int)scene.objects.size(), (int)materials.size());
    fprintf(obj, "mtllib %s\n\n", mtlName.c_str());

    int globalVertOff = 1;  // OBJ indices are 1-based
    int globalUVOff   = 1;

    for (auto& [key, group] : groups) {
        auto& mi = materials[key];

        // Object header
        fprintf(obj, "o %s\n", mi.matName.c_str());
        fprintf(obj, "usemtl %s\n", mi.matName.c_str());

        int groupVertStart = globalVertOff;
        int groupUVStart   = globalUVOff;

        // Write all vertices and UVs for this texture group
        for (int oi : group.objectIndices) {
            auto& sceneObj = scene.objects[oi];
            auto& mesh = scene.meshes[sceneObj.meshIndex];
            int vc = mesh.vertexCount;

            for (int vi = 0; vi < vc; vi++) {
                // Engine Z-up → Blender Y-up: (x, z, -y)
                float ex = mesh.vertices[vi * 3 + 0];
                float ey = mesh.vertices[vi * 3 + 1];
                float ez = mesh.vertices[vi * 3 + 2];

                float bx = ex;
                float by = ez;
                float bz = -ey;

                // Vertex colors as extended OBJ (0-1 range)
                float r = 1.0f, g = 1.0f, b = 1.0f;
                if (mesh.colors) {
                    r = mesh.colors[vi * 4 + 0] / 255.0f;
                    g = mesh.colors[vi * 4 + 1] / 255.0f;
                    b = mesh.colors[vi * 4 + 2] / 255.0f;
                }

                fprintf(obj, "v %.6f %.6f %.6f %.4f %.4f %.4f\n", bx, by, bz, r, g, b);
            }

            // UVs — V flip for Blender (DDS is top-down, OBJ expects bottom-up)
            if (mesh.texcoords) {
                for (int vi = 0; vi < vc; vi++) {
                    float u = mesh.texcoords[vi * 2 + 0];
                    float v = 1.0f - mesh.texcoords[vi * 2 + 1];
                    fprintf(obj, "vt %.6f %.6f\n", u, v);
                }
            }
        }

        // Write faces for this texture group
        int objVertOff = groupVertStart;
        int objUVOff   = groupUVStart;

        for (int oi : group.objectIndices) {
            auto& sceneObj = scene.objects[oi];
            auto& mesh = scene.meshes[sceneObj.meshIndex];
            int tc = mesh.triangleCount;

            for (int ti = 0; ti < tc; ti++) {
                int i0 = mesh.indices[ti * 3 + 0];
                int i1 = mesh.indices[ti * 3 + 1];
                int i2 = mesh.indices[ti * 3 + 2];

                if (mesh.texcoords) {
                    fprintf(obj, "f %d/%d %d/%d %d/%d\n",
                        objVertOff + i0, objUVOff + i0,
                        objVertOff + i1, objUVOff + i1,
                        objVertOff + i2, objUVOff + i2);
                } else {
                    fprintf(obj, "f %d %d %d\n",
                        objVertOff + i0, objVertOff + i1, objVertOff + i2);
                }
            }

            objVertOff += mesh.vertexCount;
            objUVOff   += mesh.vertexCount;
        }

        globalVertOff = objVertOff;
        globalUVOff   = objUVOff;
        fprintf(obj, "\n");
    }

    fclose(obj);
    return true;
}
