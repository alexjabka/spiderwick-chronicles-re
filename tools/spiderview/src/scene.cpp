// ============================================================================
// scene.cpp — Scene management implementation
// ============================================================================
// See scene.h for design rationale and coordinate system notes.

#include "scene.h"
#include "rlgl.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <set>

// ============================================================================
// TryLoadDDSFallback — direct GPU upload for DDS textures raylib rejects
// ============================================================================
// raylib's LoadImageFromMemory(".dds") can fail on certain DDS files
// (unusual header fields, edge cases in its parser). This function bypasses
// raylib's DDS parser and uploads compressed texture data directly to GPU
// via rlLoadTexture → glCompressedTexImage2D.

static Texture2D TryLoadDDSFallback(const uint8_t* dds, uint32_t ddsSize,
                                     std::string& outFormat) {
    Texture2D tex = {0};
    outFormat = "unknown";

    if (ddsSize < 128 || memcmp(dds, "DDS ", 4) != 0) return tex;

    uint32_t height, width, mipCount, pfFlags, fourCC, bitCount;
    memcpy(&height,   dds + 12, 4);
    memcpy(&width,    dds + 16, 4);
    memcpy(&mipCount, dds + 28, 4);
    memcpy(&pfFlags,  dds + 80, 4);
    memcpy(&fourCC,   dds + 84, 4);
    memcpy(&bitCount, dds + 88, 4);

    if (width == 0 || height == 0 || width > 4096 || height > 4096) return tex;
    if (mipCount == 0) mipCount = 1;

    const uint8_t* pixelData = dds + 128;
    uint32_t dataSize = ddsSize - 128;

    // Compressed formats (DXT1/3/5)
    if (pfFlags & 0x4) { // DDPF_FOURCC
        int format = -1;
        if      (fourCC == 0x31545844) { format = PIXELFORMAT_COMPRESSED_DXT1_RGBA; outFormat = "DXT1"; }
        else if (fourCC == 0x33545844) { format = PIXELFORMAT_COMPRESSED_DXT3_RGBA; outFormat = "DXT3"; }
        else if (fourCC == 0x35545844) { format = PIXELFORMAT_COMPRESSED_DXT5_RGBA; outFormat = "DXT5"; }
        else {
            char cc[5] = {0};
            memcpy(cc, &fourCC, 4);
            outFormat = std::string("FCC:") + cc;
            return tex;
        }

        tex.id = rlLoadTexture(pixelData, width, height, format, mipCount);
        if (tex.id > 0) {
            tex.width = width;
            tex.height = height;
            tex.format = format;
            tex.mipmaps = mipCount;
        }
        return tex;
    }

    // Uncompressed formats (A8R8G8B8, X8R8G8B8, R5G6B5, etc.)
    if (pfFlags & 0x40) { // DDPF_RGB
        uint32_t rMask, gMask, bMask, aMask;
        memcpy(&rMask, dds + 92, 4);
        memcpy(&gMask, dds + 96, 4);
        memcpy(&bMask, dds + 100, 4);
        memcpy(&aMask, dds + 104, 4);

        outFormat = "RGB" + std::to_string(bitCount);

        if (bitCount == 32) {
            // Convert BGRA → RGBA (D3D9 A8R8G8B8 layout: B,G,R,A in memory)
            // Engine uses alpha test (D3DRS_ALPHAREF=15) — shadow overlays rely on
            // alpha=0 to be discarded. For X8R8G8B8 (no alpha flag):
            //   Large textures (>=128px): force alpha=255 (base textures, opaque)
            //   Small textures (<128px): keep alpha from X byte (0 → overlay gets discarded)
            bool hasAlphaFlag = (pfFlags & 0x1) != 0;
            bool isSmallOverlay = (width < 128 || height < 128);
            std::vector<uint8_t> rgba(width * height * 4);
            uint32_t pixelCount = std::min(width * height, dataSize / 4);
            for (uint32_t i = 0; i < pixelCount; i++) {
                uint32_t pixel;
                memcpy(&pixel, pixelData + i * 4, 4);
                if (bMask == 0xFF && gMask == 0xFF00 && rMask == 0xFF0000) {
                    rgba[i*4+0] = (pixel >> 16) & 0xFF;
                    rgba[i*4+1] = (pixel >> 8)  & 0xFF;
                    rgba[i*4+2] = pixel & 0xFF;
                    if (hasAlphaFlag)
                        rgba[i*4+3] = (pixel >> 24) & 0xFF;
                    else if (isSmallOverlay)
                        rgba[i*4+3] = (pixel >> 24) & 0xFF; // keep X byte (usually 0)
                    else
                        rgba[i*4+3] = 255; // force opaque for base textures
                } else {
                    memcpy(&rgba[i*4], &pixel, 4);
                }
            }
            tex.id = rlLoadTexture(rgba.data(), width, height,
                                   PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        } else if (bitCount == 24) {
            // Convert BGR → RGB
            std::vector<uint8_t> rgb(width * height * 3);
            uint32_t pixelCount = std::min(width * height, dataSize / 3);
            for (uint32_t i = 0; i < pixelCount; i++) {
                rgb[i*3+0] = pixelData[i*3+2]; // R
                rgb[i*3+1] = pixelData[i*3+1]; // G
                rgb[i*3+2] = pixelData[i*3+0]; // B
            }
            tex.id = rlLoadTexture(rgb.data(), width, height,
                                   PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
        } else if (bitCount == 16) {
            // A1R5G5B5 or R5G6B5 → just use as-is, raylib can handle R5G6B5
            tex.id = rlLoadTexture(pixelData, width, height,
                                   PIXELFORMAT_UNCOMPRESSED_R5G6B5, 1);
        }

        if (tex.id > 0) {
            tex.width = width;
            tex.height = height;
            tex.format = (bitCount >= 32) ? PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
                       : (bitCount >= 24) ? PIXELFORMAT_UNCOMPRESSED_R8G8B8
                       : PIXELFORMAT_UNCOMPRESSED_R5G6B5;
            tex.mipmaps = 1;
        }
        return tex;
    }

    return tex;
}

// ============================================================================
// UploadParsedMesh — convert ParsedMesh to raylib Mesh + compute normals
// ============================================================================

int Scene::UploadParsedMesh(const ParsedMesh& parsed) {
    if (!parsed.Valid()) return -1;

    Mesh mesh = {0};
    mesh.vertexCount   = parsed.vertexCount;
    mesh.triangleCount = parsed.triangleCount;

    mesh.vertices = (float*)RL_MALLOC(parsed.positions.size() * sizeof(float));
    memcpy(mesh.vertices, parsed.positions.data(), parsed.positions.size() * sizeof(float));

    mesh.texcoords = (float*)RL_MALLOC(parsed.texcoords.size() * sizeof(float));
    memcpy(mesh.texcoords, parsed.texcoords.data(), parsed.texcoords.size() * sizeof(float));

    mesh.colors = (unsigned char*)RL_MALLOC(parsed.colors.size());
    memcpy(mesh.colors, parsed.colors.data(), parsed.colors.size());

    if (!parsed.normals.empty()) {
        mesh.normals = (float*)RL_MALLOC(parsed.normals.size() * sizeof(float));
        memcpy(mesh.normals, parsed.normals.data(), parsed.normals.size() * sizeof(float));
    }

    mesh.indices = (unsigned short*)RL_MALLOC(parsed.indices.size() * sizeof(unsigned short));
    for (size_t i = 0; i < parsed.indices.size(); i++)
        mesh.indices[i] = (unsigned short)parsed.indices[i];

    // NOTE: No UploadMesh — we render via rlBegin/rlEnd, not DrawMesh.
    // Uploading 2346 meshes (creating ~14000 VBOs) corrupts OpenGL state
    // and breaks the batch renderer's texture binding.

    int idx = (int)meshes.size();
    meshes.push_back(mesh);
    return idx;
}

// ============================================================================
// LoadSceneTexture — load a PCIM texture with fallback chain
// ============================================================================

int Scene::LoadSceneTexture(int texIndex) {
    auto it = textureMap.find(texIndex);
    if (it != textureMap.end()) return it->second;

    auto texIt = pcwb.texRef.find(texIndex);
    if (texIt == pcwb.texRef.end()) return -1;

    const uint8_t* ddsData;
    uint32_t ddsSize, w, h;
    if (!pcwb.ExtractDDS(texIt->second, &ddsData, &ddsSize, &w, &h))
        return -1;

    // Detect DDS format for diagnostics
    std::string fmtStr = "unknown";
    if (ddsSize > 0x58) {
        uint32_t pfFlags, fourCC, bitCount;
        memcpy(&pfFlags, ddsData + 80, 4);
        memcpy(&fourCC, ddsData + 84, 4);
        memcpy(&bitCount, ddsData + 88, 4);
        if (pfFlags & 0x4) {
            char cc[5] = {0};
            memcpy(cc, &fourCC, 4);
            fmtStr = cc;
        } else if (pfFlags & 0x40) {
            fmtStr = "RGB" + std::to_string(bitCount);
        }
    }

    // Check if DDS has a real alpha channel (DDPF_ALPHAPIXELS = 0x01)
    bool ddsHasAlpha = false;
    if (ddsSize > 0x6C) {
        uint32_t ddsPfFlags;
        memcpy(&ddsPfFlags, ddsData + 80, 4);
        ddsHasAlpha = (ddsPfFlags & 0x01) != 0;
    }

    // --- Attempt 1: raylib's built-in DDS loader ---
    Image img = LoadImageFromMemory(".dds", ddsData, (int)ddsSize);
    if (img.data != nullptr) {
        bool isOverlay = (w < 128 || h < 128);

        // For non-alpha textures loaded as R8G8B8 (no alpha channel):
        // Convert to R8G8B8A8 so we can control alpha for overlay handling
        if (!ddsHasAlpha && img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) {
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            // ImageFormat sets alpha=255 — fix it for overlays
            if (isOverlay) {
                unsigned char* px = (unsigned char*)img.data;
                for (int i = 0; i < img.width * img.height; i++) {
                    // Swap R/B (BGRA→RGBA) and set alpha=0 for overlay
                    unsigned char tmp = px[i*4+0];
                    px[i*4+0] = px[i*4+2];
                    px[i*4+2] = tmp;
                    px[i*4+3] = 0;
                }
            } else {
                // Large base texture: swap R/B, keep alpha=255
                unsigned char* px = (unsigned char*)img.data;
                for (int i = 0; i < img.width * img.height; i++) {
                    unsigned char tmp = px[i*4+0];
                    px[i*4+0] = px[i*4+2];
                    px[i*4+2] = tmp;
                }
            }
        }
        // For non-alpha textures loaded as R8G8B8A8:
        else if (!ddsHasAlpha && img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
            unsigned char* px = (unsigned char*)img.data;
            int pixelCount = img.width * img.height;

            bool hasBlendAlpha = false;
            for (int i = 0; i < pixelCount; i++) {
                if (px[i*4+3] != 0) { hasBlendAlpha = true; break; }
            }

            for (int i = 0; i < pixelCount; i++) {
                unsigned char tmp = px[i*4+0];
                px[i*4+0] = px[i*4+2];
                px[i*4+2] = tmp;
                if (!hasBlendAlpha && !isOverlay) px[i*4+3] = 255;
                // overlay with no blend alpha: alpha stays 0 → discarded by alpha test
            }
        }
        SceneTexture st;
        st.texture = LoadTextureFromImage(img);
        st.name    = "tex_" + std::to_string(texIndex);
        st.width   = img.width;
        st.height  = img.height;
        UnloadImage(img);

        SetTextureFilter(st.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(st.texture, TEXTURE_WRAP_REPEAT);

        int idx = (int)textures.size();
        textures.push_back(st);
        textureMap[texIndex] = idx;

        texDiagnostics.push_back({texIndex, fmtStr, st.width, st.height, true, "raylib"});
        texLoadOK++;
        return idx;
    }

    // --- Attempt 2: Direct GPU upload (bypass raylib DDS parser) ---
    std::string fallbackFmt;
    Texture2D fallbackTex = TryLoadDDSFallback(ddsData, ddsSize, fallbackFmt);
    if (fallbackTex.id > 0) {
        SceneTexture st;
        st.texture = fallbackTex;
        st.name    = "tex_" + std::to_string(texIndex);
        st.width   = fallbackTex.width;
        st.height  = fallbackTex.height;

        SetTextureFilter(st.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(st.texture, TEXTURE_WRAP_REPEAT);

        int idx = (int)textures.size();
        textures.push_back(st);
        textureMap[texIndex] = idx;

        texDiagnostics.push_back({texIndex, fallbackFmt, st.width, st.height, true, "gpu_fallback"});
        texLoadOK++;
        return idx;
    }

    // --- Attempt 3: Generate magenta placeholder ---
    {
        int pw = (w > 0 && w <= 4096) ? (int)w : 64;
        int ph = (h > 0 && h <= 4096) ? (int)h : 64;
        Image placeholder = GenImageColor(pw, ph, MAGENTA);

        SceneTexture st;
        st.texture = LoadTextureFromImage(placeholder);
        st.name    = "tex_" + std::to_string(texIndex) + "_MISSING";
        st.width   = pw;
        st.height  = ph;
        UnloadImage(placeholder);

        SetTextureFilter(st.texture, TEXTURE_FILTER_POINT);
        SetTextureWrap(st.texture, TEXTURE_WRAP_REPEAT);

        int idx = (int)textures.size();
        textures.push_back(st);
        textureMap[texIndex] = idx;

        texDiagnostics.push_back({texIndex, fmtStr, pw, ph, false, "placeholder"});
        texLoadFail++;
        return idx;
    }
}

// ============================================================================
// GetOrCreateMaterial — lazy material creation per texture index
// ============================================================================

int Scene::GetOrCreateMaterial(int texIndex) {
    auto it = materialMap.find(texIndex);
    if (it != materialMap.end()) return it->second;

    Material mat = LoadMaterialDefault();

    if (texIndex >= 0) {
        int texArrIdx = LoadSceneTexture(texIndex);
        if (texArrIdx >= 0)
            mat.maps[MATERIAL_MAP_DIFFUSE].texture = textures[texArrIdx].texture;
    }

    int idx = (int)materials.size();
    materials.push_back(mat);
    materialMap[texIndex] = idx;
    return idx;
}

// ============================================================================
// LoadPCWB — load and parse a world file
// ============================================================================
// Two passes:
//   Pass 1: Props — pre-transform vertices with world matrix
//   Pass 2: World geometry — remaining PCRDs at identity
//
// Props are NOT deduplicated (each instance has a unique transform).
// World geometry IS deduplicated (shared PCRDs uploaded once).

bool Scene::LoadPCWB(const char* path) {
    Clear();

    if (!pcwb.Load(path)) return false;

    // Extract header info for lighting/fog
    headerInfo = pcwb.GetHeaderInfo();

    // Build prop → PCRD lookup
    std::unordered_map<uint32_t, std::vector<int>> pcrdToPropIndices;
    for (int pi = 0; pi < (int)pcwb.props.size(); pi++) {
        for (uint32_t off : pcwb.props[pi].pcrdOffsets)
            pcrdToPropIndices[off].push_back(pi);
    }

    // --- Pass 1: Props with pre-transformed vertices ---
    // Each prop instance gets its own mesh copy (can't deduplicate across
    // different transforms). Vertices are multiplied by the prop's world matrix
    // during ParsePCRD, so they end up in engine world space.
    for (int pi = 0; pi < (int)pcwb.props.size(); pi++) {
        const PropInfo& prop = pcwb.props[pi];

        for (size_t mi = 0; mi < prop.pcrdOffsets.size(); mi++) {
            uint32_t pcrdOff = prop.pcrdOffsets[mi];

            // Pre-transform: pass prop's D3D9 world matrix
            int meshIdx = UploadParsedMesh(pcwb.ParsePCRD(pcrdOff, prop.matrix));
            if (meshIdx < 0) continue;

            int texIdx = pcwb.GetPCRDTexture(pcrdOff);
            int matIdx = GetOrCreateMaterial(texIdx);

            SceneObject obj;
            obj.name = std::string(prop.name);
            if (prop.pcrdOffsets.size() > 1)
                obj.name += "_sub" + std::to_string(mi);
            obj.meshIndex     = meshIdx;
            obj.materialIndex = matIdx;
            obj.texRefKey     = texIdx;
            obj.pcrdOffset    = pcrdOff;
            obj.transform     = MatrixIdentity(); // already in world space
            obj.bbox          = GetMeshBoundingBox(meshes[meshIdx]);
            obj.visible       = true;

            objects.push_back(obj);
        }
    }

    // --- Pass 2: World geometry (terrain/environment) ---
    // Streaming PCRDs may have world transforms from batch entries.
    // PCRDs with non-identity transforms get pre-transformed vertices
    // (like props), since each instance has unique world placement.
    // PCRDs with identity/zero transforms are already in world space.
    std::set<uint32_t> propPCRDs;
    for (auto& [off, _] : pcrdToPropIndices) propPCRDs.insert(off);

    std::unordered_map<uint32_t, int> pcrdMeshIndex; // only for identity-transform PCRDs

    for (size_t i = 0; i < pcwb.pcrdOffsets.size(); i++) {
        uint32_t pcrdOff = pcwb.pcrdOffsets[i];
        if (propPCRDs.count(pcrdOff)) continue;

        // Check for instance transform from batch entry
        const float* worldMatrix = nullptr;
        auto wmIt = pcwb.pcrdWorldMatrix.find(pcrdOff);
        bool hasTransform = false;
        if (wmIt != pcwb.pcrdWorldMatrix.end() && wmIt->second.size() == 16) {
            const auto& m = wmIt->second;
            // Check if transform has non-zero translation (not identity placement)
            if (std::abs(m[12]) > 0.01f || std::abs(m[13]) > 0.01f || std::abs(m[14]) > 0.01f) {
                worldMatrix = m.data();
                hasTransform = true;
            }
        }

        int meshIdx;
        if (hasTransform) {
            // Instanced PCRD: pre-transform vertices with world matrix (unique mesh per instance)
            meshIdx = UploadParsedMesh(pcwb.ParsePCRD(pcrdOff, worldMatrix));
        } else {
            // Identity/world-space PCRD: deduplicate mesh
            auto meshIt = pcrdMeshIndex.find(pcrdOff);
            if (meshIt != pcrdMeshIndex.end()) {
                meshIdx = meshIt->second;
            } else {
                meshIdx = UploadParsedMesh(pcwb.ParsePCRD(pcrdOff));
                pcrdMeshIndex[pcrdOff] = meshIdx;
            }
        }
        if (meshIdx < 0) continue;

        int texIdx = pcwb.GetPCRDTexture(pcrdOff);
        int matIdx = GetOrCreateMaterial(texIdx);

        SceneObject obj;
        obj.name          = "world_" + std::to_string(i);
        obj.meshIndex     = meshIdx;
        obj.materialIndex = matIdx;
        obj.texRefKey     = texIdx;
        obj.pcrdOffset    = pcrdOff;
        obj.transform     = MatrixIdentity(); // already in world space (pre-transformed)
        obj.bbox          = GetMeshBoundingBox(meshes[meshIdx]);
        obj.visible       = true;
        objects.push_back(obj);
    }

    // Sort objects: large textures (base) first, small textures (overlays) last.
    // Engine renders base geometry first, then shadow overlays with alpha test.
    std::stable_sort(objects.begin(), objects.end(), [this](const SceneObject& a, const SceneObject& b) {
        int aSize = 0, bSize = 0;
        if (a.texRefKey >= 0) {
            auto it = pcwb.texRef.find(a.texRefKey);
            if (it != pcwb.texRef.end() && it->second + 0xA4 < pcwb.data.size()) {
                aSize = pcwb.ReadU32(it->second + 0x9C) * pcwb.ReadU32(it->second + 0xA0);
            }
        }
        if (b.texRefKey >= 0) {
            auto it = pcwb.texRef.find(b.texRefKey);
            if (it != pcwb.texRef.end() && it->second + 0xA4 < pcwb.data.size()) {
                bSize = pcwb.ReadU32(it->second + 0x9C) * pcwb.ReadU32(it->second + 0xA0);
            }
        }
        return aSize > bSize;  // larger textures render first
    });

    // Audit: count objects with valid textures
    objsWithTexture = 0;
    objsWithoutTexture = 0;
    for (auto& obj : objects) {
        if (obj.materialIndex >= 0 && obj.materialIndex < (int)materials.size()) {
            if (materials[obj.materialIndex].maps[MATERIAL_MAP_DIFFUSE].texture.id > 0)
                objsWithTexture++;
            else
                objsWithoutTexture++;
        }
    }

    return true;
}

// ============================================================================
// Clear — release all GPU resources
// ============================================================================

void Scene::Clear() {
    for (auto& mat : materials) {
        for (int i = 0; i < 12; i++) // MAX_MATERIAL_MAPS
            mat.maps[i].texture.id = 0;
        UnloadMaterial(mat);
    }
    materials.clear();
    materialMap.clear();

    // Free CPU-side mesh data (no GPU resources to release — we don't call UploadMesh)
    for (auto& mesh : meshes) {
        RL_FREE(mesh.vertices);
        RL_FREE(mesh.texcoords);
        RL_FREE(mesh.colors);
        RL_FREE(mesh.normals);
        RL_FREE(mesh.indices);
    }
    meshes.clear();

    for (auto& tex : textures) {
        if (tex.texture.id > 0) UnloadTexture(tex.texture);
    }
    textures.clear();
    textureMap.clear();

    objects.clear();
    bones.clear();
    texDiagnostics.clear();
    texLoadOK = 0;
    texLoadFail = 0;
    headerInfo = {};
}

// ============================================================================
// Draw — render all visible objects via rlBegin immediate mode
// ============================================================================
// Coordinate conversion from engine Z-up to raylib Y-up applied inline:
//   display = (engine_x, engine_z, -engine_y)
// Normals use the same conversion.

void Scene::Draw() {
    drawTexBindCount = 0;

    // ========================================================================
    // Engine-replica multi-pass rendering
    // ========================================================================
    //
    // PCWB world geometry (two-pass):
    //   Pass 1: Base textures (>= 128px) — opaque, standard blend
    //   Pass 2: Shadow overlays (< 128px) — multiply blend (darkens base)
    //   Replicates RenderDispatch bucket ordering + blend states.
    //
    // NM40 character geometry (two-pass, RENDERFUNC_NOAM_SKIN replica):
    //   Pass 1: desc[2] main diffuse (1024x1024) — body, clothing
    //   Pass 2: desc[4] detail texture (512x512) — face, hair
    //   Both passes render ALL sub-batches. Alpha channel in each texture
    //   controls which regions are visible (alpha test compositing).
    //   Engine uses D3DRS_ZFUNC=LESSEQUAL so pass 2 overwrites pass 1
    //   at the same depth. Alpha ref lowered by 15 for pass 2.

    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            // Multiply blend for world shadow overlays
            // (NM40 objects skip this — they use standard alpha test)
            rlSetBlendFactors(0x0306, 0x0303, 0x8006);
            rlSetBlendMode(BLEND_CUSTOM);
            rlEnableColorBlend();
        }

        for (auto& obj : objects) {
            if (!obj.visible || obj.meshIndex < 0 || obj.materialIndex < 0) continue;

            // Classify object for pass filtering
            bool isOverlay = false;
            bool isNM40 = (obj.secondaryMatIdx >= 0);
            bool isWorldGeom = (obj.name.size() > 6 && obj.name.compare(0, 6, "world_") == 0);
            if (isWorldGeom && obj.texRefKey >= 0) {
                auto it = pcwb.texRef.find(obj.texRefKey);
                if (it != pcwb.texRef.end() && it->second + 0xA4 < pcwb.data.size()) {
                    uint32_t tw = pcwb.ReadU32(it->second + 0x9C);
                    uint32_t th = pcwb.ReadU32(it->second + 0xA0);
                    isOverlay = (tw <= 128 || th <= 128);
                }
            }

            // Pass 0 = base textures + NM40 primary, Pass 1 = overlays (skip NM40)
            if (pass == 0 && isOverlay) continue;
            if (pass == 1 && !isOverlay) continue;
            if (pass == 1 && isNM40) continue; // NM40 second pass handled below

            Mesh& m = meshes[obj.meshIndex];
            if (!m.vertices || !m.indices || m.triangleCount == 0) continue;

            Material& mat = materials[obj.materialIndex];
            if (mat.maps[MATERIAL_MAP_DIFFUSE].texture.id > 0) {
                rlSetTexture(mat.maps[MATERIAL_MAP_DIFFUSE].texture.id);
                drawTexBindCount++;
            }

            rlBegin(RL_QUADS);
            for (int t = 0; t < m.triangleCount; t++) {
                for (int vi = 0; vi < 4; vi++) {
                    int idx = (vi < 3) ? m.indices[t * 3 + vi] : m.indices[t * 3 + 2];

                    if (m.colors)
                        rlColor4ub(m.colors[idx*4], m.colors[idx*4+1],
                                   m.colors[idx*4+2], m.colors[idx*4+3]);
                    else
                        rlColor4ub(255, 255, 255, 255);

                    if (m.texcoords)
                        rlTexCoord2f(m.texcoords[idx*2], m.texcoords[idx*2+1]);

                    rlVertex3f(m.vertices[idx*3], m.vertices[idx*3+2], -m.vertices[idx*3+1]);
                }
            }
            rlEnd();
            rlSetTexture(0);
        }

        if (pass == 1) {
            rlSetBlendMode(BLEND_ALPHA);
        }
    }

    // ========================================================================
    // NM40 detail pass (desc[4]) — engine RENDERFUNC_NOAM_SKIN pass 2 replica
    // ========================================================================
    // Render ALL sub-batches again with the detail texture.
    // Alpha test discards pixels where detail alpha < threshold.
    // Z-func LEQUAL allows overwriting pass 1 at same depth.
    for (auto& obj : objects) {
        if (!obj.visible || obj.meshIndex < 0 || obj.secondaryMatIdx < 0) continue;
        if (obj.secondaryMatIdx >= (int)materials.size()) continue;

        Mesh& m = meshes[obj.meshIndex];
        if (!m.vertices || !m.indices || m.triangleCount == 0) continue;

        Material& mat2 = materials[obj.secondaryMatIdx];
        if (mat2.maps[MATERIAL_MAP_DIFFUSE].texture.id > 0) {
            rlSetTexture(mat2.maps[MATERIAL_MAP_DIFFUSE].texture.id);
            drawTexBindCount++;
        }

        rlBegin(RL_QUADS);
        for (int t = 0; t < m.triangleCount; t++) {
            for (int vi = 0; vi < 4; vi++) {
                int idx = (vi < 3) ? m.indices[t * 3 + vi] : m.indices[t * 3 + 2];

                if (m.colors)
                    rlColor4ub(m.colors[idx*4], m.colors[idx*4+1],
                               m.colors[idx*4+2], m.colors[idx*4+3]);
                else
                    rlColor4ub(255, 255, 255, 255);

                if (m.texcoords)
                    rlTexCoord2f(m.texcoords[idx*2], m.texcoords[idx*2+1]);

                rlVertex3f(m.vertices[idx*3], m.vertices[idx*3+2], -m.vertices[idx*3+1]);
            }
        }
        rlEnd();
        rlSetTexture(0);
    }
}

// ============================================================================
// Statistics
// ============================================================================

int Scene::GetTotalVertices() const {
    int total = 0;
    for (auto& m : meshes) total += m.vertexCount;
    return total;
}

int Scene::GetTotalTriangles() const {
    int total = 0;
    for (auto& m : meshes) total += m.triangleCount;
    return total;
}

BoundingBox Scene::GetSceneBounds() const {
    BoundingBox bounds = {{1e9f, 1e9f, 1e9f}, {-1e9f, -1e9f, -1e9f}};
    for (auto& obj : objects) {
        if (!obj.visible || obj.meshIndex < 0) continue;
        BoundingBox bb = GetMeshBoundingBox(meshes[obj.meshIndex]);
        bounds.min.x = std::min(bounds.min.x, bb.min.x);
        bounds.min.y = std::min(bounds.min.y, bb.min.y);
        bounds.min.z = std::min(bounds.min.z, bb.min.z);
        bounds.max.x = std::max(bounds.max.x, bb.max.x);
        bounds.max.y = std::max(bounds.max.y, bb.max.y);
        bounds.max.z = std::max(bounds.max.z, bb.max.z);
    }
    return bounds;
}
