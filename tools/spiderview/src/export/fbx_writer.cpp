// ============================================================================
// fbx_writer.cpp — ASCII FBX 7.4 writer for skinned NM40 models
// ============================================================================

#include "fbx_writer.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <set>

// FBX uses unique 64-bit IDs for all objects. We generate them sequentially.
static int64_t s_nextId = 1000000;
static int64_t NewId() { return s_nextId++; }

// Convert engine Z-up to FBX/Blender Y-up: (x, z, -y)
static void ToYUp(float ex, float ey, float ez, float& ox, float& oy, float& oz) {
    ox = ex; oy = ez; oz = -ey;
}

// Decompose a 4x4 row-major matrix into translation + rotation (Euler XYZ degrees)
// For FBX LimbNode Lcl Translation/Rotation properties.
static void DecomposeMatrix(const float* m, float outT[3], float outR[3]) {
    // Translation from row 3
    outT[0] = m[12]; outT[1] = m[13]; outT[2] = m[14];

    // Rotation: extract Euler angles from 3x3 rotation part
    // Assuming no scale (or uniform scale) for bone local transforms
    float sy = -m[2]; // -m[0][2]
    if (sy > 0.998f) {
        outR[0] = atan2f(m[4], m[5]) * 57.2957795f; // x
        outR[1] = 90.0f;                              // y
        outR[2] = 0.0f;                                // z
    } else if (sy < -0.998f) {
        outR[0] = atan2f(-m[4], m[5]) * 57.2957795f;
        outR[1] = -90.0f;
        outR[2] = 0.0f;
    } else {
        outR[0] = atan2f(m[6], m[10]) * 57.2957795f;
        outR[1] = asinf(sy) * 57.2957795f;
        outR[2] = atan2f(m[1], m[0]) * 57.2957795f;
    }
}

// Write a 4x4 matrix as comma-separated FBX values (row-major → FBX column-major)
static void WriteMatrix(FILE* f, const float* m) {
    // FBX stores matrices in column-major order
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            if (c > 0 || r > 0) fprintf(f, ",");
            fprintf(f, "%.6f", m[r * 4 + c]);
        }
    }
}

bool ExportNM40FBX(const char* path,
                   const NM40MeshResult& meshResult,
                   const std::vector<FBXMaterial>& materials) {

    if (!meshResult.Valid()) return false;

    FILE* f = fopen(path, "w");
    if (!f) return false;

    s_nextId = 1000000;

    int numBones = meshResult.numBones;
    int numMeshes = (int)meshResult.batches.size();
    bool hasSkinning = numBones > 0 &&
                       !meshResult.boneWorldMatrices.empty() &&
                       (int)meshResult.boneWorldMatrices.size() >= numBones * 16;

    // Assign IDs
    int64_t rootModelId = NewId();
    std::vector<int64_t> boneModelIds(numBones);
    for (int i = 0; i < numBones; i++) boneModelIds[i] = NewId();

    struct MeshIds { int64_t geomId, modelId, matId, texId, videoId, skinId; };
    std::vector<MeshIds> meshIds(numMeshes);
    for (int i = 0; i < numMeshes; i++) {
        meshIds[i].geomId  = NewId();
        meshIds[i].modelId = NewId();
        meshIds[i].matId   = NewId();
        meshIds[i].texId   = NewId();
        meshIds[i].videoId = NewId();
        meshIds[i].skinId  = NewId();
    }

    // Per-bone cluster IDs (one per mesh×bone pair that has influences)
    struct ClusterKey { int mesh; int bone; };
    std::vector<std::vector<int64_t>> clusterIds(numMeshes);
    for (int mi = 0; mi < numMeshes; mi++)
        clusterIds[mi].resize(numBones, 0);

    // Pre-scan: which bones are used by which mesh
    for (int mi = 0; mi < numMeshes; mi++) {
        auto& batch = meshResult.batches[mi];
        if (batch.blendIndices.empty()) continue;
        std::set<int> usedBones;
        for (size_t vi = 0; vi < batch.blendIndices.size(); vi += 4) {
            for (int bi = 0; bi < 4; bi++) {
                int boneIdx = batch.blendIndices[vi + bi];
                float w = (vi/4*4+bi < batch.blendWeights.size()) ? batch.blendWeights[vi/4*4+bi] : 0.0f;
                // Fix: correct weight indexing
                w = batch.blendWeights[vi + bi];
                if (w > 0.001f && boneIdx < numBones)
                    usedBones.insert(boneIdx);
            }
        }
        for (int bone : usedBones)
            clusterIds[mi][bone] = NewId();
    }

    // ====================================================================
    // FBX Header
    // ====================================================================
    fprintf(f, "; FBX 7.4.0 project file\n");
    fprintf(f, "; SpiderView NM40 Export\n");
    fprintf(f, "FBXHeaderExtension:  {\n");
    fprintf(f, "  FBXHeaderVersion: 1003\n");
    fprintf(f, "  FBXVersion: 7400\n");
    fprintf(f, "}\n\n");

    // Global Settings
    fprintf(f, "GlobalSettings:  {\n");
    fprintf(f, "  Version: 1000\n");
    fprintf(f, "  Properties70:  {\n");
    fprintf(f, "    P: \"UpAxis\", \"int\", \"Integer\", \"\", 1\n");
    fprintf(f, "    P: \"UpAxisSign\", \"int\", \"Integer\", \"\", 1\n");
    fprintf(f, "    P: \"FrontAxis\", \"int\", \"Integer\", \"\", 2\n");
    fprintf(f, "    P: \"FrontAxisSign\", \"int\", \"Integer\", \"\", 1\n");
    fprintf(f, "    P: \"CoordAxis\", \"int\", \"Integer\", \"\", 0\n");
    fprintf(f, "    P: \"CoordAxisSign\", \"int\", \"Integer\", \"\", 1\n");
    fprintf(f, "    P: \"UnitScaleFactor\", \"double\", \"Number\", \"\", 1.0\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n\n");

    // ====================================================================
    // Objects
    // ====================================================================
    fprintf(f, "Objects:  {\n");

    // --- Geometry nodes (one per submesh batch) ---
    for (int mi = 0; mi < numMeshes; mi++) {
        auto& batch = meshResult.batches[mi];
        if (!batch.Valid()) continue;

        fprintf(f, "  Geometry: %lld, \"Geometry::Mesh_%d\", \"Mesh\" {\n", meshIds[mi].geomId, mi);
        fprintf(f, "    GeometryVersion: 124\n");

        // Vertices (Y-up converted)
        fprintf(f, "    Vertices: *%d {\n      a: ", batch.vertexCount * 3);
        for (int vi = 0; vi < batch.vertexCount; vi++) {
            float bx, by, bz;
            ToYUp(batch.positions[vi*3], batch.positions[vi*3+1], batch.positions[vi*3+2], bx, by, bz);
            if (vi > 0) fprintf(f, ",");
            fprintf(f, "%.6f,%.6f,%.6f", bx, by, bz);
        }
        fprintf(f, "\n    }\n");

        // Polygon indices (triangles, last index negative-1 per FBX convention)
        fprintf(f, "    PolygonVertexIndex: *%d {\n      a: ", batch.triangleCount * 3);
        for (int ti = 0; ti < batch.triangleCount; ti++) {
            int i0 = batch.indices[ti*3+0];
            int i1 = batch.indices[ti*3+1];
            int i2 = batch.indices[ti*3+2];
            if (ti > 0) fprintf(f, ",");
            fprintf(f, "%d,%d,%d", i0, i1, -(i2 + 1)); // last index = -(idx+1)
        }
        fprintf(f, "\n    }\n");

        // Normals
        if (!batch.normals.empty()) {
            fprintf(f, "    LayerElementNormal: 0 {\n");
            fprintf(f, "      Version: 102\n");
            fprintf(f, "      Name: \"\"\n");
            fprintf(f, "      MappingInformationType: \"ByVertice\"\n");
            fprintf(f, "      ReferenceInformationType: \"Direct\"\n");
            fprintf(f, "      Normals: *%d {\n        a: ", batch.vertexCount * 3);
            for (int vi = 0; vi < batch.vertexCount; vi++) {
                float nx, ny, nz;
                ToYUp(batch.normals[vi*3], batch.normals[vi*3+1], batch.normals[vi*3+2], nx, ny, nz);
                if (vi > 0) fprintf(f, ",");
                fprintf(f, "%.6f,%.6f,%.6f", nx, ny, nz);
            }
            fprintf(f, "\n      }\n");
            fprintf(f, "    }\n");
        }

        // UVs
        if (!batch.texcoords.empty()) {
            fprintf(f, "    LayerElementUV: 0 {\n");
            fprintf(f, "      Version: 101\n");
            fprintf(f, "      Name: \"UVMap\"\n");
            fprintf(f, "      MappingInformationType: \"ByVertice\"\n");
            fprintf(f, "      ReferenceInformationType: \"Direct\"\n");
            fprintf(f, "      UV: *%d {\n        a: ", batch.vertexCount * 2);
            for (int vi = 0; vi < batch.vertexCount; vi++) {
                if (vi > 0) fprintf(f, ",");
                fprintf(f, "%.6f,%.6f", batch.texcoords[vi*2], 1.0f - batch.texcoords[vi*2+1]);
            }
            fprintf(f, "\n      }\n");
            fprintf(f, "    }\n");
        }

        // Layer definition
        fprintf(f, "    Layer: 0 {\n");
        fprintf(f, "      Version: 100\n");
        if (!batch.normals.empty()) {
            fprintf(f, "      LayerElement:  {\n");
            fprintf(f, "        Type: \"LayerElementNormal\"\n");
            fprintf(f, "        TypedIndex: 0\n");
            fprintf(f, "      }\n");
        }
        if (!batch.texcoords.empty()) {
            fprintf(f, "      LayerElement:  {\n");
            fprintf(f, "        Type: \"LayerElementUV\"\n");
            fprintf(f, "        TypedIndex: 0\n");
            fprintf(f, "      }\n");
        }
        fprintf(f, "    }\n");

        fprintf(f, "  }\n");
    }

    // --- Mesh Model nodes ---
    for (int mi = 0; mi < numMeshes; mi++) {
        if (!meshResult.batches[mi].Valid()) continue;
        const char* mname = (mi < (int)materials.size()) ? materials[mi].name.c_str() : "Mesh";
        fprintf(f, "  Model: %lld, \"Model::%s\", \"Mesh\" {\n", meshIds[mi].modelId, mname);
        fprintf(f, "    Version: 232\n");
        fprintf(f, "    Properties70:  {\n");
        fprintf(f, "      P: \"DefaultAttributeIndex\", \"int\", \"Integer\", \"\", 0\n");
        fprintf(f, "    }\n");
        fprintf(f, "    Shading: T\n");
        fprintf(f, "    Culling: \"CullingOff\"\n");
        fprintf(f, "  }\n");
    }

    // --- Armature root ---
    fprintf(f, "  Model: %lld, \"Model::Armature\", \"Null\" {\n", rootModelId);
    fprintf(f, "    Version: 232\n");
    fprintf(f, "    Properties70:  {\n");
    fprintf(f, "    }\n");
    fprintf(f, "  }\n");

    // --- Bone LimbNode models ---
    for (int bi = 0; bi < numBones; bi++) {
        fprintf(f, "  Model: %lld, \"Model::Bone_%d\", \"LimbNode\" {\n", boneModelIds[bi], bi);
        fprintf(f, "    Version: 232\n");
        fprintf(f, "    Properties70:  {\n");

        if (hasSkinning) {
            // Use bone world matrix position, converted to Y-up
            float wx = meshResult.boneWorldMatrices[bi*16+12];
            float wy = meshResult.boneWorldMatrices[bi*16+13];
            float wz = meshResult.boneWorldMatrices[bi*16+14];
            float bx, by, bz;
            ToYUp(wx, wy, wz, bx, by, bz);

            // For child bones: compute local offset from parent
            int parentIdx = meshResult.bones[bi].parentIndex;
            if (parentIdx >= 0 && parentIdx < numBones) {
                float pwx = meshResult.boneWorldMatrices[parentIdx*16+12];
                float pwy = meshResult.boneWorldMatrices[parentIdx*16+13];
                float pwz = meshResult.boneWorldMatrices[parentIdx*16+14];
                float pbx, pby, pbz;
                ToYUp(pwx, pwy, pwz, pbx, pby, pbz);
                bx -= pbx; by -= pby; bz -= pbz;
            }

            fprintf(f, "      P: \"Lcl Translation\", \"Lcl Translation\", \"\", \"A\", %.6f, %.6f, %.6f\n",
                    bx, by, bz);
        } else {
            // Fallback: use centroid positions
            float bx, by, bz;
            ToYUp(meshResult.bones[bi].position[0], meshResult.bones[bi].position[1],
                  meshResult.bones[bi].position[2], bx, by, bz);
            fprintf(f, "      P: \"Lcl Translation\", \"Lcl Translation\", \"\", \"A\", %.6f, %.6f, %.6f\n",
                    bx, by, bz);
        }

        fprintf(f, "    }\n");
        fprintf(f, "  }\n");
    }

    // --- Materials ---
    for (int mi = 0; mi < numMeshes; mi++) {
        if (!meshResult.batches[mi].Valid()) continue;
        const char* mname = (mi < (int)materials.size()) ? materials[mi].name.c_str() : "Material";
        fprintf(f, "  Material: %lld, \"Material::%s\", \"\" {\n", meshIds[mi].matId, mname);
        fprintf(f, "    Version: 102\n");
        fprintf(f, "    ShadingModel: \"phong\"\n");
        fprintf(f, "    Properties70:  {\n");
        fprintf(f, "      P: \"DiffuseColor\", \"Color\", \"\", \"A\", 0.8, 0.8, 0.8\n");
        fprintf(f, "    }\n");
        fprintf(f, "  }\n");
    }

    // --- Textures ---
    for (int mi = 0; mi < numMeshes; mi++) {
        if (!meshResult.batches[mi].Valid()) continue;
        if (mi >= (int)materials.size() || materials[mi].texturePath.empty()) continue;

        fprintf(f, "  Texture: %lld, \"Texture::%s_tex\", \"\" {\n", meshIds[mi].texId,
                materials[mi].name.c_str());
        fprintf(f, "    Type: \"TextureVideoClip\"\n");
        fprintf(f, "    FileName: \"%s\"\n", materials[mi].texturePath.c_str());
        fprintf(f, "    RelativeFilename: \"%s\"\n", materials[mi].texturePath.c_str());
        fprintf(f, "  }\n");

        fprintf(f, "  Video: %lld, \"Video::%s_vid\", \"Clip\" {\n", meshIds[mi].videoId,
                materials[mi].name.c_str());
        fprintf(f, "    Type: \"Clip\"\n");
        fprintf(f, "    FileName: \"%s\"\n", materials[mi].texturePath.c_str());
        fprintf(f, "    RelativeFilename: \"%s\"\n", materials[mi].texturePath.c_str());
        fprintf(f, "  }\n");
    }

    // --- Skin Deformers + Clusters ---
    if (hasSkinning) {
        for (int mi = 0; mi < numMeshes; mi++) {
            auto& batch = meshResult.batches[mi];
            if (!batch.Valid() || batch.blendIndices.empty()) continue;

            fprintf(f, "  Deformer: %lld, \"Deformer::Skin_%d\", \"Skin\" {\n", meshIds[mi].skinId, mi);
            fprintf(f, "    Version: 101\n");
            fprintf(f, "    Link_DeformAcuracy: 50\n");
            fprintf(f, "  }\n");

            // Build per-bone vertex lists
            std::map<int, std::vector<std::pair<int,float>>> boneVerts; // bone → [(vertIdx, weight)]
            for (int vi = 0; vi < batch.vertexCount; vi++) {
                for (int bi = 0; bi < 4; bi++) {
                    int boneIdx = batch.blendIndices[vi*4+bi];
                    float w = batch.blendWeights[vi*4+bi];
                    if (w > 0.001f && boneIdx < numBones)
                        boneVerts[boneIdx].push_back({vi, w});
                }
            }

            for (auto& [boneIdx, verts] : boneVerts) {
                int64_t clusterId = clusterIds[mi][boneIdx];
                if (clusterId == 0) continue;

                fprintf(f, "  Deformer: %lld, \"SubDeformer::Bone_%d\", \"Cluster\" {\n",
                        clusterId, boneIdx);
                fprintf(f, "    Version: 100\n");
                fprintf(f, "    UserData: \"\", \"\"\n");

                // Vertex indices
                fprintf(f, "    Indexes: *%d {\n      a: ", (int)verts.size());
                for (size_t i = 0; i < verts.size(); i++) {
                    if (i > 0) fprintf(f, ",");
                    fprintf(f, "%d", verts[i].first);
                }
                fprintf(f, "\n    }\n");

                // Weights
                fprintf(f, "    Weights: *%d {\n      a: ", (int)verts.size());
                for (size_t i = 0; i < verts.size(); i++) {
                    if (i > 0) fprintf(f, ",");
                    fprintf(f, "%.6f", verts[i].second);
                }
                fprintf(f, "\n    }\n");

                // Transform (mesh-space to bone-space = inverse bind)
                fprintf(f, "    Transform: *16 {\n      a: ");
                WriteMatrix(f, &meshResult.boneInvBindMatrices[boneIdx * 16]);
                fprintf(f, "\n    }\n");

                // TransformLink (bone world matrix)
                fprintf(f, "    TransformLink: *16 {\n      a: ");
                WriteMatrix(f, &meshResult.boneWorldMatrices[boneIdx * 16]);
                fprintf(f, "\n    }\n");

                fprintf(f, "  }\n");
            }
        }
    }

    fprintf(f, "}\n\n");

    // ====================================================================
    // Connections
    // ====================================================================
    fprintf(f, "Connections:  {\n");

    // Armature root → scene root (id 0)
    fprintf(f, "  C: \"OO\", %lld, 0\n", rootModelId);

    // Mesh models → Armature
    for (int mi = 0; mi < numMeshes; mi++) {
        if (!meshResult.batches[mi].Valid()) continue;
        fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].modelId, rootModelId);
        // Geometry → Model
        fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].geomId, meshIds[mi].modelId);
        // Material → Model
        fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].matId, meshIds[mi].modelId);
    }

    // Texture → Material (DiffuseColor property)
    for (int mi = 0; mi < numMeshes; mi++) {
        if (!meshResult.batches[mi].Valid()) continue;
        if (mi >= (int)materials.size() || materials[mi].texturePath.empty()) continue;
        fprintf(f, "  C: \"OP\", %lld, %lld, \"DiffuseColor\"\n", meshIds[mi].texId, meshIds[mi].matId);
        fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].videoId, meshIds[mi].texId);
    }

    // Bone hierarchy
    for (int bi = 0; bi < numBones; bi++) {
        int parent = meshResult.bones[bi].parentIndex;
        if (parent >= 0 && parent < numBones)
            fprintf(f, "  C: \"OO\", %lld, %lld\n", boneModelIds[bi], boneModelIds[parent]);
        else
            fprintf(f, "  C: \"OO\", %lld, %lld\n", boneModelIds[bi], rootModelId);
    }

    // Skin deformer connections
    if (hasSkinning) {
        for (int mi = 0; mi < numMeshes; mi++) {
            auto& batch = meshResult.batches[mi];
            if (!batch.Valid() || batch.blendIndices.empty()) continue;

            // Skin → Geometry
            fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].skinId, meshIds[mi].geomId);

            // Clusters → Skin, and cluster → bone
            for (int bi = 0; bi < numBones; bi++) {
                int64_t cid = clusterIds[mi][bi];
                if (cid == 0) continue;
                fprintf(f, "  C: \"OO\", %lld, %lld\n", cid, meshIds[mi].skinId);
                fprintf(f, "  C: \"OO\", %lld, %lld\n", boneModelIds[bi], cid);
            }
        }
    }

    fprintf(f, "}\n");

    fclose(f);
    return true;
}
