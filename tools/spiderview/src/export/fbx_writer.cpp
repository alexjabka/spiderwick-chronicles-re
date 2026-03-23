// ============================================================================
// fbx_writer.cpp — ASCII FBX 7.4 writer for skinned NM40 models
// ============================================================================
//
// Proper FBX with: mesh geometry, bone armature, skin deformers, materials.
// All coordinates converted from engine Z-up to FBX/Blender Y-up.
// Matrices converted via basis change: M_yup = C * M_zup * C_inv
// where C swaps Y↔Z with Z negated.

#include "fbx_writer.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <set>

static int64_t s_nextId = 1000000;
static int64_t NewId() { return s_nextId++; }

// Convert a single point from engine Z-up to Y-up: (x, z, -y)
static void PtToYUp(float ex, float ey, float ez, float& ox, float& oy, float& oz) {
    ox = ex; oy = ez; oz = -ey;
}

// Convert a 4x4 row-major matrix from Z-up to Y-up coordinate system.
// Basis change: M' = C * M * C^-1
// C = swap Y↔Z with Z negated:
//   [1  0  0  0]       [1  0  0  0]
//   [0  0  1  0]  inv: [0  0 -1  0]
//   [0 -1  0  0]       [0  1  0  0]
//   [0  0  0  1]       [0  0  0  1]
static void MatToYUp(const float* in, float* out) {
    // M' = C * M * C^-1 expanded for row-major 4x4:
    // Row i, Col j of result = sum_k sum_l C[i][k] * M[k][l] * Cinv[l][j]
    // Since C and Cinv are permutation+sign matrices, this simplifies to:
    //
    // For row-major M[row*4+col]:
    //   Row 0 of M stays row 0, but columns swap: col1↔col2, col2 negated
    //   Row 1 of M becomes row 2 (negated)
    //   Row 2 of M becomes row 1
    //   Row 3 (translation) gets same column swap
    //
    // Direct formula (verified):
    out[0]  =  in[0];   out[1]  =  in[2];   out[2]  = -in[1];   out[3]  = in[3];
    out[4]  =  in[8];   out[5]  =  in[10];  out[6]  = -in[9];   out[7]  = in[7];
    out[8]  = -in[4];   out[9]  = -in[6];   out[10] =  in[5];   out[11] = in[11];
    out[12] =  in[12];  out[13] =  in[14];  out[14] = -in[13];  out[15] = in[15];
}

// Write a 4x4 row-major matrix as FBX comma-separated values (row-major, NOT transposed).
// Blender's ASCII FBX importer reads matrices in row-major order for Transform/TransformLink.
static void WriteMatrix(FILE* f, const float* m) {
    for (int i = 0; i < 16; i++) {
        if (i > 0) fprintf(f, ",");
        fprintf(f, "%.6f", m[i]);
    }
}

// Multiply 4x4 row-major matrices: out = A * B
static void MatMul(const float* A, const float* B, float* out) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            float s = 0;
            for (int k = 0; k < 4; k++)
                s += A[r*4+k] * B[k*4+c];
            out[r*4+c] = s;
        }
}

// Invert a 4x4 affine matrix (rotation+translation, no perspective)
static bool MatInvert(const float* m, float* out) {
    // For affine: inverse of upper 3x3 + adjusted translation
    // Using cofactor expansion for full generality
    float inv[16];
    inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

    float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (fabsf(det) < 1e-10f) return false;
    float invDet = 1.0f / det;
    for (int i = 0; i < 16; i++) out[i] = inv[i] * invDet;
    return true;
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

    // Build bone world positions from vertex centroids (Y-up converted).
    // These are the same positions used for SpiderView's armature overlay
    // and are verified correct. We avoid using the raw engine matrices
    // (boneWorldMatrices/boneInvBindMatrices) because their coordinate
    // space doesn't match FBX expectations and causes scale issues.
    //
    // For skinning clusters: Transform = TransformLink = identity.
    // This tells Blender the mesh vertices are already in the correct space
    // and bones just define vertex groups without additional transforms.
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    // Bone positions in Y-up (from centroids)
    struct BonePosYUp { float x, y, z; };
    std::vector<BonePosYUp> bonePosYUp(numBones);
    for (int bi = 0; bi < numBones; bi++) {
        PtToYUp(meshResult.bones[bi].position[0],
                meshResult.bones[bi].position[1],
                meshResult.bones[bi].position[2],
                bonePosYUp[bi].x, bonePosYUp[bi].y, bonePosYUp[bi].z);
    }

    // Assign IDs
    int64_t rootModelId = NewId();
    std::vector<int64_t> boneModelIds(numBones);
    for (int i = 0; i < numBones; i++) boneModelIds[i] = NewId();
    std::vector<int64_t> boneAttrIds(numBones);
    for (int i = 0; i < numBones; i++) boneAttrIds[i] = NewId();

    struct MeshIds { int64_t geomId, modelId, matId, texId, videoId, skinId; };
    std::vector<MeshIds> meshIds(numMeshes);
    for (int i = 0; i < numMeshes; i++) {
        meshIds[i] = {NewId(), NewId(), NewId(), NewId(), NewId(), NewId()};
    }

    // Per-bone cluster IDs per mesh
    std::vector<std::vector<int64_t>> clusterIds(numMeshes);
    for (int mi = 0; mi < numMeshes; mi++) {
        clusterIds[mi].resize(numBones, 0);
        auto& batch = meshResult.batches[mi];
        if (batch.blendIndices.empty()) continue;
        std::set<int> usedBones;
        for (int vi = 0; vi < batch.vertexCount; vi++)
            for (int bi = 0; bi < 4; bi++) {
                int idx = batch.blendIndices[vi*4+bi];
                float w = batch.blendWeights[vi*4+bi];
                if (w > 0.001f && idx < numBones) usedBones.insert(idx);
            }
        for (int bone : usedBones)
            clusterIds[mi][bone] = NewId();
    }

    // ====================================================================
    // Header
    // ====================================================================
    fprintf(f, "; FBX 7.4.0 project file\n");
    fprintf(f, "; SpiderView NM40 Export\n\n");
    fprintf(f, "FBXHeaderExtension:  {\n");
    fprintf(f, "  FBXHeaderVersion: 1003\n");
    fprintf(f, "  FBXVersion: 7400\n");
    fprintf(f, "}\n\n");

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
    fprintf(f, "  }\n}\n\n");

    // ====================================================================
    // Objects
    // ====================================================================
    fprintf(f, "Objects:  {\n\n");

    // --- Geometry (one per submesh) ---
    for (int mi = 0; mi < numMeshes; mi++) {
        auto& batch = meshResult.batches[mi];
        if (!batch.Valid()) continue;

        fprintf(f, "  Geometry: %lld, \"Geometry::Mesh_%d\", \"Mesh\" {\n", meshIds[mi].geomId, mi);
        fprintf(f, "    GeometryVersion: 124\n");

        // Vertices (Y-up)
        fprintf(f, "    Vertices: *%d {\n      a: ", batch.vertexCount * 3);
        for (int vi = 0; vi < batch.vertexCount; vi++) {
            float bx, by, bz;
            PtToYUp(batch.positions[vi*3], batch.positions[vi*3+1], batch.positions[vi*3+2], bx, by, bz);
            if (vi > 0) fprintf(f, ",");
            fprintf(f, "%.6f,%.6f,%.6f", bx, by, bz);
        }
        fprintf(f, "\n    }\n");

        // Polygon indices (last index per triangle = -(idx+1))
        fprintf(f, "    PolygonVertexIndex: *%d {\n      a: ", batch.triangleCount * 3);
        for (int ti = 0; ti < batch.triangleCount; ti++) {
            if (ti > 0) fprintf(f, ",");
            fprintf(f, "%d,%d,%d", batch.indices[ti*3], batch.indices[ti*3+1], -(int)(batch.indices[ti*3+2] + 1));
        }
        fprintf(f, "\n    }\n");

        // Normals (by polygon vertex for proper FBX)
        if (!batch.normals.empty()) {
            fprintf(f, "    LayerElementNormal: 0 {\n");
            fprintf(f, "      Version: 102\n");
            fprintf(f, "      Name: \"\"\n");
            fprintf(f, "      MappingInformationType: \"ByVertice\"\n");
            fprintf(f, "      ReferenceInformationType: \"Direct\"\n");
            fprintf(f, "      Normals: *%d {\n        a: ", batch.vertexCount * 3);
            for (int vi = 0; vi < batch.vertexCount; vi++) {
                // NM40 normals are already stored in Y-up (converted in parser)
                float nx = batch.normals[vi*3], ny = batch.normals[vi*3+1], nz = batch.normals[vi*3+2];
                if (vi > 0) fprintf(f, ",");
                fprintf(f, "%.6f,%.6f,%.6f", nx, ny, nz);
            }
            fprintf(f, "\n      }\n    }\n");
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
            fprintf(f, "\n      }\n    }\n");
        }

        // Layer
        fprintf(f, "    Layer: 0 {\n      Version: 100\n");
        if (!batch.normals.empty())
            fprintf(f, "      LayerElement:  { Type: \"LayerElementNormal\" TypedIndex: 0 }\n");
        if (!batch.texcoords.empty())
            fprintf(f, "      LayerElement:  { Type: \"LayerElementUV\" TypedIndex: 0 }\n");
        fprintf(f, "    }\n");
        fprintf(f, "  }\n\n");
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
        fprintf(f, "    Shading: T\n    Culling: \"CullingOff\"\n");
        fprintf(f, "  }\n\n");
    }

    // --- Armature root ---
    fprintf(f, "  Model: %lld, \"Model::Armature\", \"Null\" {\n", rootModelId);
    fprintf(f, "    Version: 232\n");
    fprintf(f, "    Properties70:  {\n");
    fprintf(f, "    }\n  }\n\n");

    // --- Bone LimbNode models ---
    for (int bi = 0; bi < numBones; bi++) {
        fprintf(f, "  Model: %lld, \"Model::Bone_%d\", \"LimbNode\" {\n", boneModelIds[bi], bi);
        fprintf(f, "    Version: 232\n");
        fprintf(f, "    Properties70:  {\n");

        {
            // Local translation = world position minus parent's world position
            float lx = bonePosYUp[bi].x;
            float ly = bonePosYUp[bi].y;
            float lz = bonePosYUp[bi].z;
            int parentIdx = meshResult.bones[bi].parentIndex;
            if (parentIdx >= 0 && parentIdx < numBones) {
                lx -= bonePosYUp[parentIdx].x;
                ly -= bonePosYUp[parentIdx].y;
                lz -= bonePosYUp[parentIdx].z;
            }
            fprintf(f, "      P: \"Lcl Translation\", \"Lcl Translation\", \"\", \"A\", %.6f, %.6f, %.6f\n", lx, ly, lz);
        }

        fprintf(f, "    }\n  }\n\n");

        // NodeAttribute for bone (makes Blender recognize it as a bone)
        fprintf(f, "  NodeAttribute: %lld, \"NodeAttribute::Bone_%d\", \"LimbNode\" {\n", boneAttrIds[bi], bi);
        fprintf(f, "    TypeFlags: \"Skeleton\"\n");
        fprintf(f, "  }\n\n");
    }

    // --- Materials ---
    for (int mi = 0; mi < numMeshes; mi++) {
        if (!meshResult.batches[mi].Valid()) continue;
        const char* mname = (mi < (int)materials.size()) ? materials[mi].name.c_str() : "Material";
        fprintf(f, "  Material: %lld, \"Material::%s\", \"\" {\n", meshIds[mi].matId, mname);
        fprintf(f, "    Version: 102\n    ShadingModel: \"phong\"\n");
        fprintf(f, "    Properties70:  {\n");
        fprintf(f, "      P: \"DiffuseColor\", \"Color\", \"\", \"A\", 0.8, 0.8, 0.8\n");
        fprintf(f, "    }\n  }\n\n");
    }

    // --- Textures ---
    for (int mi = 0; mi < numMeshes; mi++) {
        if (!meshResult.batches[mi].Valid()) continue;
        if (mi >= (int)materials.size() || materials[mi].texturePath.empty()) continue;
        fprintf(f, "  Texture: %lld, \"Texture::%s_tex\", \"\" {\n", meshIds[mi].texId, materials[mi].name.c_str());
        fprintf(f, "    Type: \"TextureVideoClip\"\n");
        fprintf(f, "    FileName: \"%s\"\n", materials[mi].texturePath.c_str());
        fprintf(f, "    RelativeFilename: \"%s\"\n", materials[mi].texturePath.c_str());
        fprintf(f, "  }\n\n");
        fprintf(f, "  Video: %lld, \"Video::%s_vid\", \"Clip\" {\n", meshIds[mi].videoId, materials[mi].name.c_str());
        fprintf(f, "    Type: \"Clip\"\n");
        fprintf(f, "    FileName: \"%s\"\n", materials[mi].texturePath.c_str());
        fprintf(f, "    RelativeFilename: \"%s\"\n", materials[mi].texturePath.c_str());
        fprintf(f, "  }\n\n");
    }

    // --- Skin Deformers + Clusters ---
    if (hasSkinning) {
        for (int mi = 0; mi < numMeshes; mi++) {
            auto& batch = meshResult.batches[mi];
            if (!batch.Valid() || batch.blendIndices.empty()) continue;

            fprintf(f, "  Deformer: %lld, \"Deformer::Skin_%d\", \"Skin\" {\n", meshIds[mi].skinId, mi);
            fprintf(f, "    Version: 101\n    Link_DeformAcuracy: 50\n");
            fprintf(f, "  }\n\n");

            // Build per-bone vertex index+weight lists
            std::map<int, std::vector<std::pair<int,float>>> boneVerts;
            for (int vi = 0; vi < batch.vertexCount; vi++)
                for (int bi = 0; bi < 4; bi++) {
                    int boneIdx = batch.blendIndices[vi*4+bi];
                    float w = batch.blendWeights[vi*4+bi];
                    if (w > 0.001f && boneIdx < numBones)
                        boneVerts[boneIdx].push_back({vi, w});
                }

            for (auto& [boneIdx, verts] : boneVerts) {
                int64_t cid = clusterIds[mi][boneIdx];
                if (cid == 0) continue;

                fprintf(f, "  Deformer: %lld, \"SubDeformer::Cluster_Bone_%d\", \"Cluster\" {\n", cid, boneIdx);
                fprintf(f, "    Version: 100\n    UserData: \"\", \"\"\n");

                // Indices
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

                // TransformLink = bone world position (translation matrix at centroid)
                // Transform = inverse of TransformLink (negative translation)
                float transLink[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0,
                    bonePosYUp[boneIdx].x, bonePosYUp[boneIdx].y, bonePosYUp[boneIdx].z, 1};
                float trans[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0,
                    -bonePosYUp[boneIdx].x, -bonePosYUp[boneIdx].y, -bonePosYUp[boneIdx].z, 1};

                fprintf(f, "    Transform: *16 {\n      a: ");
                WriteMatrix(f, trans);
                fprintf(f, "\n    }\n");

                fprintf(f, "    TransformLink: *16 {\n      a: ");
                WriteMatrix(f, transLink);
                fprintf(f, "\n    }\n");

                fprintf(f, "  }\n\n");
            }
        }
    }

    fprintf(f, "}\n\n");

    // ====================================================================
    // Connections
    // ====================================================================
    fprintf(f, "Connections:  {\n");

    // Root → scene
    fprintf(f, "  C: \"OO\", %lld, 0\n", rootModelId);

    // Meshes → root, geometry → mesh, material → mesh
    for (int mi = 0; mi < numMeshes; mi++) {
        if (!meshResult.batches[mi].Valid()) continue;
        fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].modelId, rootModelId);
        fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].geomId, meshIds[mi].modelId);
        fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].matId, meshIds[mi].modelId);
    }

    // Texture → Material
    for (int mi = 0; mi < numMeshes; mi++) {
        if (!meshResult.batches[mi].Valid()) continue;
        if (mi >= (int)materials.size() || materials[mi].texturePath.empty()) continue;
        fprintf(f, "  C: \"OP\", %lld, %lld, \"DiffuseColor\"\n", meshIds[mi].texId, meshIds[mi].matId);
        fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].videoId, meshIds[mi].texId);
    }

    // Bone hierarchy: bone → parent bone (or → root)
    // NodeAttribute → bone model
    for (int bi = 0; bi < numBones; bi++) {
        int parent = meshResult.bones[bi].parentIndex;
        if (parent >= 0 && parent < numBones)
            fprintf(f, "  C: \"OO\", %lld, %lld\n", boneModelIds[bi], boneModelIds[parent]);
        else
            fprintf(f, "  C: \"OO\", %lld, %lld\n", boneModelIds[bi], rootModelId);
        // Attribute → model (makes Blender create actual bone)
        fprintf(f, "  C: \"OO\", %lld, %lld\n", boneAttrIds[bi], boneModelIds[bi]);
    }

    // Skin connections
    if (hasSkinning) {
        for (int mi = 0; mi < numMeshes; mi++) {
            auto& batch = meshResult.batches[mi];
            if (!batch.Valid() || batch.blendIndices.empty()) continue;
            fprintf(f, "  C: \"OO\", %lld, %lld\n", meshIds[mi].skinId, meshIds[mi].geomId);
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
