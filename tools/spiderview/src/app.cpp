// ============================================================================
// app.cpp — Application logic: world loading, VM execution, asset operations
// ============================================================================

#include "app.h"
#include "formats.h"
#include "core/format_registry.h"
#include <cstdio>
#include <cstring>
#include <csignal>
#include <csetjmp>
#include <algorithm>
#include <unordered_set>
#ifdef _WIN32
#include <direct.h>
#include <sys/stat.h>
#endif

// ============================================================================
// Init / Shutdown
// ============================================================================

void App::Init(int argc, char* argv[]) {
    RegisterAllFormats();
    cam.Init();
    fs.Discover(GetWorkingDirectory());
    worldShader.Load();
    if (argc > 1) LoadWorld(argv[1]);
}

void App::Shutdown() {
    scene.Clear();
    view.ClearPreview();
    worldShader.Unload();
    cache.Clear();
}

// ============================================================================
// World loading
// ============================================================================

static std::string ExtractLevelName(const char* path) {
    std::string s(path);
    size_t sep = s.find_last_of("/\\");
    if (sep != std::string::npos) s = s.substr(sep + 1);
    size_t dot = s.rfind('.');
    if (dot != std::string::npos) s = s.substr(0, dot);
    return s;
}

void App::LoadWorld(const char* path) {
    scene.Clear();
    view.selectedObject = -1;
    vm = KallisVM();
    view.ClearPreview();
    renderSettings.normalLighting = false; // World uses baked vertex colors, not normals

    // Load prop position overrides from JSON before PCWB parse
    std::string levelName = ExtractLevelName(path);
    view.worldName = levelName;
    scene.pcwb.LoadPropPositions(levelName.c_str());

    if (!scene.LoadPCWB(path)) { view.statusMsg = "Failed to load"; return; }

    // Auto-load SCT from matching ZWD
    std::string worldName = ExtractLevelName(path);
    std::vector<std::string> candidates = { worldName };
    std::string base = worldName;
    while (!base.empty() && base.back() >= '0' && base.back() <= '9') base.pop_back();
    if (!base.empty() && base != worldName) { candidates.push_back(base + "D"); candidates.push_back(base); }

    std::string gd = fs.GetGameDir();
    const char* dirs[] = {"/ww/Wads/", "/na/Wads/", "/us/Wads/"};
    for (auto& n : candidates) { if (vm.loaded) break;
        for (auto& d : dirs) { if (vm.loaded) break;
            std::string p = gd + d + n + ".zwd";
            if (!FileExists(p.c_str())) continue;
            const auto* blob = cache.GetBlob(p);
            if (!blob) continue;
            for (uint32_t i = 0; i + 52 < (uint32_t)blob->size(); i++) {
                if ((*blob)[i]=='S'&&(*blob)[i+1]=='C'&&(*blob)[i+2]=='T'&&(*blob)[i+3]==0) {
                    uint32_t ver = (*blob)[i+4]|((*blob)[i+5]<<8)|((*blob)[i+6]<<16)|((*blob)[i+7]<<24);
                    if (ver == 13) {
                        uint32_t ds = (*blob)[i+8]|((*blob)[i+9]<<8)|((*blob)[i+10]<<16)|((*blob)[i+11]<<24);
                        uint32_t total = ds + 52;
                        if (i + total <= (uint32_t)blob->size()) {
                            vm.LoadSCTFromBuffer(blob->data() + i, total);
                            printf("[App] Auto-loaded SCT from %s\n", p.c_str());
                        }
                        break;
                    }
                }
            }
        }
    }

    cam.FrameScene(scene.GetSceneBounds());

    if (scene.headerInfo.valid) {
        float f0=scene.headerInfo.fogParams[0], f1=scene.headerInfo.fogParams[1], f2=scene.headerInfo.fogParams[2];
        if (f0>=0&&f0<=1&&f1>=0&&f1<=1&&f2>=0&&f2<=1) {
            renderSettings.fogColor[0]=f0; renderSettings.fogColor[1]=f1; renderSettings.fogColor[2]=f2;
        }
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "%d objs, %d tex, %dK tris%s",
             scene.GetObjectCount(), scene.GetTextureCount(),
             scene.GetTotalTriangles()/1000, vm.loaded ? ", SCT loaded" : "");
    view.statusMsg = buf;
}

// ============================================================================
// SCT loading from archive
// ============================================================================

void App::LoadSCTFromArchive(const std::string& zwdPath, uint32_t offset, uint32_t size) {
    vm = KallisVM();
    const auto* blob = cache.GetBlob(zwdPath);
    if (blob && offset + size <= (uint32_t)blob->size())
        vm.LoadSCTFromBuffer(blob->data() + offset, size);
}

// ============================================================================
// Script text view
// ============================================================================

void App::BuildScriptView() {
    view.scriptText.clear();
    if (!vm.loaded) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "=== SCT: %d classes, %d funcs, %d names ===\n\n",
        (int)vm.classes.size(), (int)vm.functions.size(), (int)vm.nameResolve.size());
    view.scriptText += buf;
    for (auto& cls : vm.classes) {
        snprintf(buf, sizeof(buf), "class %s [%08X]\n",
            cls.name.empty() ? "?" : cls.name.c_str(), cls.nameHash);
        view.scriptText += buf;
        for (auto& me : cls.methods) {
            auto it = vm.nameResolve.find(me.nameHash);
            snprintf(buf, sizeof(buf), "  %s [%08X] bc=0x%X%s\n",
                (it != vm.nameResolve.end()) ? it->second.c_str() : "?",
                me.nameHash, me.bcOffset, me.isNative ? " native" : "");
            view.scriptText += buf;
        }
    }
    view.scriptText += "\n=== Names ===\n";
    for (auto& [h, n] : vm.nameResolve) {
        snprintf(buf, sizeof(buf), "  %08X = %s\n", h, n.c_str());
        view.scriptText += buf;
    }
}

// ============================================================================
// Texture preview
// ============================================================================

void App::PreviewTexture(const std::string& zwdPath, uint32_t pcimOffset, const FSNode& parent, int childIdx) {
    view.ClearPreview();
    const auto* blob = cache.GetBlob(zwdPath);
    if (!blob || pcimOffset + 0xC1 + 128 >= (uint32_t)blob->size()) return;

    const uint8_t* dds = blob->data() + pcimOffset + 0xC1;
    uint32_t ddsSize = (uint32_t)blob->size() - (pcimOffset + 0xC1);
    for (int k = childIdx + 1; k < (int)parent.children.size(); k++) {
        if (parent.children[k].offset > pcimOffset) {
            ddsSize = parent.children[k].offset - (pcimOffset + 0xC1);
            break;
        }
    }
    Image img = LoadImageFromMemory(".dds", dds, ddsSize);
    if (img.data) {
        view.previewTex = LoadTextureFromImage(img);
        view.previewW = img.width; view.previewH = img.height;
        view.previewName = std::to_string(pcimOffset);
        UnloadImage(img);
    }
}

// ============================================================================
// 3D Model Preview — loads NM40 into viewport
// ============================================================================

void App::PreviewModel(const std::string& zwdPath, uint32_t nm40Offset, uint32_t nm40Size,
                       const FSNode& archiveNode) {
    // Store archive context for the Materials tab texture picker
    view.lastArchivePath = zwdPath;
    view.lastArchiveNode = const_cast<FSNode*>(&archiveNode);

    const auto* blob = cache.GetBlob(zwdPath);
    if (!blob || nm40Offset + 0x40 >= (uint32_t)blob->size()) {
        view.statusMsg = "Failed to read archive blob";
        return;
    }

    // Use AWAD TOC-provided size (includes header + PCRD + vertex/index data)
    uint32_t avail = (uint32_t)blob->size() - nm40Offset;
    uint32_t sz = (nm40Size > 0 && nm40Size <= avail) ? nm40Size : avail;

    printf("[PreviewModel] NM40 at 0x%X, size=%u\n", nm40Offset, sz);

    // Parse NM40 into individual sub-batches for multi-material rendering
    NM40MeshResult nm40Result = ParseNM40Batches(blob->data() + nm40Offset, sz);
    lastNM40Result = nm40Result; // retain for FBX export
    if (!nm40Result.Valid()) {
        const char* err = GetNM40ParseError();
        char buf[320];
        snprintf(buf, sizeof(buf), "NM40 failed: %s", err[0] ? err : "unknown");
        view.statusMsg = buf;
        return;
    }

    // Load into viewport — one scene object per sub-batch
    scene.Clear();
    view.selectedObject = -1;
    view.materialSlots.clear();
    view.selectedSlot = 0;

    BoundingBox totalBB = {{1e9f,1e9f,1e9f},{-1e9f,-1e9f,-1e9f}};
    auto& batches = nm40Result.batches.empty()
        ? std::vector<ParsedMesh>{nm40Result.merged}  // fallback to merged
        : nm40Result.batches;

    // Build material slots from unique submesh indices
    // Entry 0 = "Body", entry 1+ = "Head / Face / Hair"
    std::map<int, int> submeshToSlot; // submeshIndex → slot index
    for (auto& mesh : batches) {
        if (!mesh.Valid()) continue;
        int si = (mesh.submeshIndex <= 0) ? 0 : 1; // group entries 1+ together
        if (submeshToSlot.find(si) == submeshToSlot.end()) {
            int slotIdx = (int)view.materialSlots.size();
            submeshToSlot[si] = slotIdx;
            MaterialSlot slot;
            slot.name = (si == 0) ? "Body" : "Head";
            // Create a default material for this slot
            Material mat = LoadMaterialDefault();
            mat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            slot.materialIndex = (int)scene.materials.size();
            scene.materials.push_back(mat);
            view.materialSlots.push_back(slot);
        }
    }

    for (int bi = 0; bi < (int)batches.size(); bi++) {
        auto& mesh = batches[bi];
        if (!mesh.Valid()) continue;

        int si = (mesh.submeshIndex <= 0) ? 0 : 1;
        int slotIdx = submeshToSlot[si];
        int matIdx = view.materialSlots[slotIdx].materialIndex;

        int meshIdx = scene.UploadParsedMesh(mesh);
        if (meshIdx < 0) continue;

        SceneObject obj;
        char nameBuf[96];
        snprintf(nameBuf, sizeof(nameBuf), "Batch %d (sub=%d, %d bones, %dv)",
                 bi, mesh.submeshIndex, mesh.boneCount, mesh.vertexCount);
        obj.name = nameBuf;
        obj.meshIndex = meshIdx;
        obj.materialIndex = matIdx;
        obj.transform = MatrixIdentity();

        // Compute per-batch bounding box and extend total
        BoundingBox bb = {{1e9f,1e9f,1e9f},{-1e9f,-1e9f,-1e9f}};
        for (int i = 0; i < mesh.vertexCount; i++) {
            float x = mesh.positions[i*3], y = mesh.positions[i*3+1], z = mesh.positions[i*3+2];
            bb.min.x = fminf(bb.min.x, x); bb.max.x = fmaxf(bb.max.x, x);
            bb.min.y = fminf(bb.min.y, y); bb.max.y = fmaxf(bb.max.y, y);
            bb.min.z = fminf(bb.min.z, z); bb.max.z = fmaxf(bb.max.z, z);
            totalBB.min.x = fminf(totalBB.min.x, x); totalBB.max.x = fmaxf(totalBB.max.x, x);
            totalBB.min.y = fminf(totalBB.min.y, y); totalBB.max.y = fmaxf(totalBB.max.y, y);
            totalBB.min.z = fminf(totalBB.min.z, z); totalBB.max.z = fmaxf(totalBB.max.z, z);
        }
        obj.bbox = bb;

        int objIdx = (int)scene.objects.size();
        scene.objects.push_back(obj);
        view.materialSlots[slotIdx].objectIndices.push_back(objIdx);
    }

    cam.FrameScene(totalBB);

    // Store bone data for armature visualization
    scene.bones = nm40Result.bones;

    // NM40 lighting — match game's character lighting (bright ambient + soft directional)
    renderSettings.normalLighting    = true;
    renderSettings.dirLightEnable    = true;
    renderSettings.dirLightIntensity = 0.5f;
    renderSettings.dirLightYaw       = 0.8f;
    renderSettings.dirLightPitch     = 0.5f;
    renderSettings.ambientLevel      = 0.7f;
    renderSettings.layerTextures     = true;

    char buf[256];
    snprintf(buf, sizeof(buf), "NM40: %d batches, %d slots",
             (int)nm40Result.batches.size(), (int)view.materialSlots.size());
    view.statusMsg = buf;

    // --- Auto-resolve texture: engine replica pipeline ---
    AutoResolveTexture(zwdPath, nm40Offset, archiveNode);
}

// ============================================================================
// Apply texture to current model — user clicks PCIM while model is loaded
// ============================================================================

void App::ApplyTextureToModel(const std::string& zwdPath, uint32_t pcimOffset, uint32_t pcimSize) {
    if (scene.objects.empty()) {
        view.statusMsg = "No model loaded — load an NM40 first";
        return;
    }

    const auto* blob = cache.GetBlob(zwdPath);
    if (!blob || pcimOffset + 0xC1 + 128 >= (uint32_t)blob->size()) {
        view.statusMsg = "Failed to read PCIM data";
        return;
    }

    // Extract DDS from PCIM — find "DDS " magic
    // PCIM header is typically 0xC1 (193) bytes, but validate by scanning for DDS magic
    const uint8_t* pcimData = blob->data() + pcimOffset;
    uint32_t ddsOff = 0;
    // Try PCIM+0x10 pointer first (relative offset to DDS within PCIM)
    if (pcimSize > 0x14) {
        uint32_t rel = pcimData[0x10]|(pcimData[0x11]<<8)|(pcimData[0x12]<<16)|(pcimData[0x13]<<24);
        if (rel > 0 && rel < pcimSize && pcimOffset + rel + 128 < (uint32_t)blob->size() &&
            memcmp(blob->data() + pcimOffset + rel, "DDS ", 4) == 0) {
            ddsOff = pcimOffset + rel;
        }
    }
    // Fallback: standard 0xC1 offset
    if (!ddsOff && pcimOffset + 0xC1 + 128 < (uint32_t)blob->size() &&
        memcmp(blob->data() + pcimOffset + 0xC1, "DDS ", 4) == 0) {
        ddsOff = pcimOffset + 0xC1;
    }
    if (!ddsOff) { view.statusMsg = "No DDS in PCIM"; return; }

    const uint8_t* dds = blob->data() + ddsOff;
    uint32_t ddsSize = pcimSize - (ddsOff - pcimOffset);
    if (ddsOff + ddsSize > (uint32_t)blob->size())
        ddsSize = (uint32_t)blob->size() - ddsOff;

    Image img = LoadImageFromMemory(".dds", dds, ddsSize);
    if (!img.data) {
        view.statusMsg = "Failed to load DDS texture";
        return;
    }

    // Create GPU texture
    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);

    int w = img.width, h = img.height;
    UnloadImage(img);

    if (tex.id == 0) {
        view.statusMsg = "Failed to upload texture to GPU";
        return;
    }

    // Apply to selected material slot (Blender-style), or all if no slots
    if (!view.materialSlots.empty()) {
        int si = view.selectedSlot;
        if (si < 0 || si >= (int)view.materialSlots.size()) si = 0;
        auto& slot = view.materialSlots[si];

        if (slot.materialIndex >= 0 && slot.materialIndex < (int)scene.materials.size()) {
            scene.materials[slot.materialIndex].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
            scene.materials[slot.materialIndex].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        }
        slot.texId = tex.id;
        slot.texWidth = w;
        slot.texHeight = h;

        // Store PCIM nameHash for mapping info display
        if (view.lastArchiveNode) {
            for (auto& child : view.lastArchiveNode->children)
                if (child.type == AssetType::PCIM && child.offset == pcimOffset)
                    { slot.pcimHash = child.nameHash; break; }
        }

        char buf[128];
        snprintf(buf, sizeof(buf), "Slot %d (%s): %dx%d", si, slot.name.c_str(), w, h);
        view.statusMsg = buf;
    } else {
        // Fallback: apply to all materials (no slots = world or unknown model)
        for (auto& mat : scene.materials) {
            mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
            mat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        }
        char buf[128];
        snprintf(buf, sizeof(buf), "Texture applied: %dx%d", w, h);
        view.statusMsg = buf;
    }
}

// ============================================================================
// Apply detail texture as secondary material (desc[4]) for two-pass NM40
// ============================================================================
// Engine RENDERFUNC_NOAM_SKIN pass 2: renders all sub-batches again with the
// detail texture. Alpha test discards body regions, showing face/hair only.
// Z-func LESSEQUAL lets this pass overwrite pass 1 at same depth.

void App::ApplyDetailTexture(const std::string& zwdPath, uint32_t pcimOffset, uint32_t pcimSize) {
    if (scene.objects.empty()) return;

    const auto* blob = cache.GetBlob(zwdPath);
    if (!blob || pcimOffset + 0xC1 + 128 >= (uint32_t)blob->size()) return;

    // Extract DDS from PCIM
    const uint8_t* pcimData = blob->data() + pcimOffset;
    uint32_t ddsOff = 0;
    if (pcimSize > 0x14) {
        uint32_t rel = pcimData[0x10]|(pcimData[0x11]<<8)|(pcimData[0x12]<<16)|(pcimData[0x13]<<24);
        if (rel > 0 && rel < pcimSize && pcimOffset + rel + 128 < (uint32_t)blob->size() &&
            memcmp(blob->data() + pcimOffset + rel, "DDS ", 4) == 0)
            ddsOff = pcimOffset + rel;
    }
    if (!ddsOff && pcimOffset + 0xC1 + 128 < (uint32_t)blob->size() &&
        memcmp(blob->data() + pcimOffset + 0xC1, "DDS ", 4) == 0)
        ddsOff = pcimOffset + 0xC1;
    if (!ddsOff) return;

    const uint8_t* dds = blob->data() + ddsOff;
    uint32_t ddsSize = pcimSize - (ddsOff - pcimOffset);
    if (ddsOff + ddsSize > (uint32_t)blob->size())
        ddsSize = (uint32_t)blob->size() - ddsOff;

    Image img = LoadImageFromMemory(".dds", dds, ddsSize);
    if (!img.data) return;

    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    int w = img.width, h = img.height;
    UnloadImage(img);
    if (tex.id == 0) return;

    // Track for cleanup
    static unsigned int s_lastDetailTexId = 0;
    if (s_lastDetailTexId > 0) {
        Texture2D old = {s_lastDetailTexId, 0, 0, 0, 0};
        UnloadTexture(old);
    }
    s_lastDetailTexId = tex.id;

    // Create a secondary material with the detail texture
    Material detailMat = LoadMaterialDefault();
    detailMat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
    detailMat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    int detailMatIdx = (int)scene.materials.size();
    scene.materials.push_back(detailMat);

    // Assign as secondary material to all NM40 scene objects
    for (auto& obj : scene.objects)
        obj.secondaryMatIdx = detailMatIdx;

    printf("[DetailTex] Applied detail %dx%d as secondary material (idx=%d)\n", w, h, detailMatIdx);
}

// Forward declarations for crash-protected VM execution
static jmp_buf s_vmJmpBuf;
static bool s_vmCrashed = false;
static void VmCrashHandler(int) { s_vmCrashed = true; longjmp(s_vmJmpBuf, 1); }

// ============================================================================
// Auto-resolve texture for NM40 — engine replica approach
// ============================================================================
// Replicates the game engine's asset loading pipeline:
//   1. Load SCT script from the same WAD
//   2. Execute VM → sauCharacterInit captures (template, texA, texB) hashes
//   3. Match NM40 nameHash against captured templates
//   4. Look up texA/texB hashes in AWAD TOC → find PCIM entries
//   5. Apply as textures

void App::AutoResolveTexture(const std::string& zwdPath, uint32_t nm40Offset,
                              const FSNode& archiveNode) {
    // ========================================================================
    // Engine-replica two-pass texture resolution (RENDERFUNC_NOAM_SKIN)
    // ========================================================================
    //
    // The engine renders NM40 characters in TWO passes over ALL sub-batches:
    //   Pass 1: desc[2] = main diffuse (1024x1024) — body, clothing, hands
    //   Pass 2: desc[4] = detail texture (512x512) — face, hair
    //
    // Alpha channel in each texture controls which regions are visible:
    //   - desc[2] has alpha=0 on face/hair → discarded by alpha test
    //   - desc[4] has alpha=0 on body     → discarded by alpha test
    //   - D3DRS_ZFUNC=LESSEQUAL lets pass 2 overwrite pass 1 at same depth
    //   - Engine lowers alpha ref by 15 for pass 2
    //
    // Confirmed via IDA decompile of RENDERFUNC_NOAM_SKIN (0x5709C0):
    //   Pass 1: D3D_BindTexture(*(actor+108)+4) then DrawIndexedPrimitive all
    //   Pass 2: D3D_BindTexture(*(actor+112)+4) then DrawIndexedPrimitive all
    //
    // nameHash mapping captured from live runtime via IDA debugger + D3D hooks.
    // ========================================================================

    // Find the NM40's nameHash
    uint32_t nm40Hash = 0;
    for (auto& child : archiveNode.children)
        if (child.type == AssetType::NM40 && child.offset == nm40Offset)
            { nm40Hash = child.nameHash; break; }
    view.nm40Hash = nm40Hash;

    if (nm40Hash == 0) {
        printf("[AutoTex] NM40 nameHash not found\n");
        return;
    }

    // ========================================================================
    // Character texture resolution
    // ========================================================================
    // The engine builds the NM40→PCIM mapping at runtime via the Kallis
    // factory (not stored in the AWAD data). We use a known mapping table
    // for confirmed characters, expandable via nm40_texmap.json.
    //
    // Descriptor layout (from ClNoamActor_Init 0x527EC0):
    //   desc[2] = main diffuse PCIM (1024x1024 body)
    //   desc[4] = detail PCIM (512x512 face/hair)
    //
    // Mapping captured from live runtime via IDA debugger + D3D hooks.

    // Load texture mappings from data/nm40_texmap.json (data-driven)
    // Expandable by capturing more characters via IDA breakpoint at 0x527EC0.
    struct CharTexMap { uint32_t nm40Hash, diffuseHash, detailHash; };
    static std::vector<CharTexMap> s_charTexMap;
    static bool s_mapLoaded = false;
    if (!s_mapLoaded) {
        s_mapLoaded = true;
        const char* paths[] = {"nm40_texmap.json", "data/nm40_texmap.json"};
        for (auto* path : paths) {
            FILE* f = fopen(path, "rb");
            if (!f) continue;
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            std::string json(sz, 0);
            fread(&json[0], 1, sz, f); fclose(f);
            size_t pos = 0;
            while ((pos = json.find("\"nm40\"", pos)) != std::string::npos) {
                size_t vs = json.find("\"0x", pos + 6);
                if (vs == std::string::npos || vs > pos + 40) { pos += 6; continue; }
                uint32_t nm = (uint32_t)strtoul(json.c_str() + vs + 1, nullptr, 16);
                uint32_t dh = 0, th = 0;
                size_t dp = json.find("\"diffuse\"", vs);
                if (dp != std::string::npos && dp < vs + 200) {
                    size_t dv = json.find("\"0x", dp + 9);
                    if (dv != std::string::npos && dv < dp + 40)
                        dh = (uint32_t)strtoul(json.c_str() + dv + 1, nullptr, 16);
                }
                size_t tp = json.find("\"detail\"", vs);
                if (tp != std::string::npos && tp < vs + 300) {
                    size_t tv = json.find("\"0x", tp + 8);
                    if (tv != std::string::npos && tv < tp + 40)
                        th = (uint32_t)strtoul(json.c_str() + tv + 1, nullptr, 16);
                }
                if (nm && (dh || th)) s_charTexMap.push_back({nm, dh, th});
                pos = vs + 10;
            }
            printf("[AutoTex] Loaded %d mappings from %s\n", (int)s_charTexMap.size(), path);
            break;
        }
    }

    uint32_t diffuseHash = 0, detailHash = 0;
    for (auto& map : s_charTexMap) {
        if (map.nm40Hash == nm40Hash) {
            diffuseHash = map.diffuseHash;
            detailHash = map.detailHash;
            printf("[AutoTex] Matched NM40 0x%08X -> diffuse=0x%08X detail=0x%08X\n",
                   nm40Hash, diffuseHash, detailHash);
            break;
        }
    }

    // --- Fallback: Scan raw AWAD blob for NM40 hash near PCIM hashes ---
    // The Kallis factory references asset hashes as constants in compiled code
    // or data tables (stored as unrecognized AWAD entries). Scan the entire blob
    // for the NM40 nameHash and check nearby uint32s for PCIM nameHashes.
    if (!diffuseHash && !detailHash) {
        const auto* blob = cache.GetBlob(zwdPath);
        if (blob && blob->size() > 16) {
            // Build PCIM hash set
            std::unordered_set<uint32_t> pcimSet;
            std::unordered_map<uint32_t, uint32_t> pcimSizes; // hash → width
            for (auto& child : archiveNode.children) {
                if (child.type == AssetType::PCIM) {
                    pcimSet.insert(child.nameHash);
                    if (child.offset + 0xA0 < (uint32_t)blob->size())
                        pcimSizes[child.nameHash] = (*blob)[child.offset+0x9C]|
                            ((*blob)[child.offset+0x9D]<<8)|((*blob)[child.offset+0x9E]<<16)|
                            ((*blob)[child.offset+0x9F]<<24);
                }
            }

            // Scan entire blob for NM40 hash
            for (uint32_t off = 0; off + 4 <= (uint32_t)blob->size() && !diffuseHash; off += 4) {
                uint32_t v = (*blob)[off]|((*blob)[off+1]<<8)|((*blob)[off+2]<<16)|((*blob)[off+3]<<24);
                if (v != nm40Hash) continue;

                // Found! Check nearby uint32s (within ±40 bytes) for PCIM hashes
                uint32_t scanStart = (off > 40) ? off - 40 : 0;
                uint32_t scanEnd = (off + 44 < (uint32_t)blob->size()) ? off + 44 : (uint32_t)blob->size() - 4;
                for (uint32_t so = scanStart; so <= scanEnd; so += 4) {
                    uint32_t sv = (*blob)[so]|((*blob)[so+1]<<8)|((*blob)[so+2]<<16)|((*blob)[so+3]<<24);
                    if (!pcimSet.count(sv)) continue;
                    uint32_t w = pcimSizes.count(sv) ? pcimSizes[sv] : 0;
                    if (w >= 512 && !diffuseHash) {
                        diffuseHash = sv;
                        printf("[AutoTex] AWAD scan: diffuse=0x%08X (%dx) near NM40 at blob+0x%X\n", sv, w, off);
                    } else if (w >= 128 && !detailHash && sv != diffuseHash) {
                        detailHash = sv;
                        printf("[AutoTex] AWAD scan: detail=0x%08X (%dx) near NM40 at blob+0x%X\n", sv, w, off);
                    }
                }
            }
        }
    }

    if (!diffuseHash && !detailHash) {
        printf("[AutoTex] NM40 0x%08X: no texture mapping found. Click a PCIM to apply manually.\n", nm40Hash);
        return;
    }
    {
        // --- Per-submesh texture assignment ---
        struct TexSlot { uint32_t hash; int matIdx; };
        TexSlot slots[2] = {{diffuseHash, -1}, {detailHash, -1}};

        for (int ti = 0; ti < 2; ti++) {
            if (slots[ti].hash == 0) continue;
            for (auto& child : archiveNode.children) {
                if (child.type != AssetType::PCIM || child.nameHash != slots[ti].hash) continue;

                const auto* blob = cache.GetBlob(zwdPath);
                if (!blob || child.offset + 0xC1 + 128 >= (uint32_t)blob->size()) break;

                // Extract DDS from PCIM
                uint32_t ddsOff = 0;
                const uint8_t* pcimData = blob->data() + child.offset;
                if (child.size > 0x14) {
                    uint32_t rel = pcimData[0x10]|(pcimData[0x11]<<8)|(pcimData[0x12]<<16)|(pcimData[0x13]<<24);
                    if (rel > 0 && rel < child.size && child.offset+rel+128 < (uint32_t)blob->size() &&
                        memcmp(blob->data()+child.offset+rel, "DDS ", 4) == 0)
                        ddsOff = child.offset + rel;
                }
                if (!ddsOff && child.offset+0xC1+128 < (uint32_t)blob->size() &&
                    memcmp(blob->data()+child.offset+0xC1, "DDS ", 4) == 0)
                    ddsOff = child.offset + 0xC1;
                if (!ddsOff) break;

                const uint8_t* dds = blob->data() + ddsOff;
                uint32_t ddsSize = child.size - (ddsOff - child.offset);
                if (ddsOff + ddsSize > (uint32_t)blob->size())
                    ddsSize = (uint32_t)blob->size() - ddsOff;

                Image img = LoadImageFromMemory(".dds", dds, ddsSize);
                if (!img.data) break;
                Texture2D tex = LoadTextureFromImage(img);
                SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
                SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
                printf("[AutoTex] Loaded slot %d (%s): %dx%d PCIM 0x%08X\n",
                       ti, ti==0?"diffuse":"detail", img.width, img.height, slots[ti].hash);
                UnloadImage(img);
                if (tex.id == 0) break;

                Material mat = LoadMaterialDefault();
                mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
                mat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                slots[ti].matIdx = (int)scene.materials.size();
                scene.materials.push_back(mat);
                break;
            }
        }

        // Assign textures to material slots and update scene objects
        for (int si = 0; si < (int)view.materialSlots.size() && si < 2; si++) {
            auto& slot = view.materialSlots[si];
            if (slots[si].matIdx < 0) continue;

            // Update the slot's existing material with the loaded texture
            if (slot.materialIndex >= 0 && slot.materialIndex < (int)scene.materials.size()) {
                Material& mat = scene.materials[slot.materialIndex];
                Material& srcMat = scene.materials[slots[si].matIdx];
                mat.maps[MATERIAL_MAP_DIFFUSE].texture = srcMat.maps[MATERIAL_MAP_DIFFUSE].texture;
                mat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            }
            slot.texId = scene.materials[slot.materialIndex].maps[MATERIAL_MAP_DIFFUSE].texture.id;
            slot.texWidth = (si == 0) ? 1024 : 512; // approximate, from DDS header
            slot.texHeight = slot.texWidth;
            slot.pcimHash = slots[si].hash;

            // Read actual dimensions from loaded texture
            for (auto& stex : scene.textures) {
                if (stex.texture.id == slot.texId) {
                    slot.texWidth = stex.width;
                    slot.texHeight = stex.height;
                    break;
                }
            }
        }

        // Remove temporary materials (the texture was copied to slot materials)
        // Don't remove — they might hold the GPU texture reference

        printf("[AutoTex] Assigned %d material slots\n", (int)view.materialSlots.size());
        view.statusMsg += " | per-submesh textured";
        return;
    }

    // Fallback: no mapping table match — log and allow manual selection
    printf("[AutoTex] NM40 0x%08X not in mapping table. Click a PCIM to apply manually.\n",
           nm40Hash);
}

// ============================================================================
// VM execution with crash protection
// ============================================================================

void App::RunAllInits() {
    if (!vm.loaded) return;
    vm.execLog.clear();
    vm.totalOpsExecuted = 0;
    vm.totalCallsMade = 0;
    s_vmCrashed = false;
    auto p1 = signal(SIGSEGV, VmCrashHandler);
    auto p2 = signal(SIGABRT, VmCrashHandler);
    int ran = 0;
    if (setjmp(s_vmJmpBuf) == 0) {
        for (int ci = 0; ci < (int)vm.classes.size(); ci++) {
            auto& cls = vm.classes[ci];
            if (cls.states.empty()) continue;
            int obj = vm.CreateObject(cls.name.empty()?"obj":cls.name.c_str(), cls.name.c_str());
            if (!cls.states[0].methodOffsets.empty()) {
                vm.CallBytecodeAt(obj, cls.states[0].methodOffsets[0]);
                ran++;
            }
        }
    }
    signal(SIGSEGV, p1); signal(SIGABRT, p2);
    char buf[128];
    if (s_vmCrashed) snprintf(buf,sizeof(buf),"CRASHED after %d inits, %d ops",ran,vm.totalOpsExecuted);
    else snprintf(buf,sizeof(buf),"Ran %d inits, %d ops, %d calls",ran,vm.totalOpsExecuted,vm.totalCallsMade);
    vm.Log(s_vmCrashed?"ERROR":"RunAll", buf);
}

// ============================================================================
// ViewAsset — unified viewer through FormatRegistry
// ============================================================================

void App::ViewAsset(const std::string& zwdPath, uint32_t offset, uint32_t size, AssetType type) {
    const auto* blob = cache.GetBlob(zwdPath);
    if (!blob || offset >= (uint32_t)blob->size()) return;

    uint32_t avail = (size > 0) ? size : ((uint32_t)blob->size() - offset);
    if (offset + avail > (uint32_t)blob->size()) avail = (uint32_t)blob->size() - offset;

    const FormatHandler* handler = FormatRegistry::Instance().Get(type);
    if (handler && handler->view) {
        view.dataText = handler->view(blob->data() + offset, avail);
        char buf[128];
        snprintf(buf, sizeof(buf), "%s @ 0x%X", handler->name, offset);
        view.dataTitle = buf;
    } else {
        // No viewer — fall back to hex
        HexViewAsset(zwdPath, offset, size, type);
    }
}

// ============================================================================
// HexViewAsset — load raw bytes into hex viewer
// ============================================================================

void App::HexViewAsset(const std::string& zwdPath, uint32_t offset, uint32_t size, AssetType type) {
    const auto* blob = cache.GetBlob(zwdPath);
    if (!blob || offset >= (uint32_t)blob->size()) return;

    uint32_t vs = size > 0 ? size : 4096;
    if (offset + vs > (uint32_t)blob->size()) vs = (uint32_t)blob->size() - offset;
    view.hexData.assign(blob->data() + offset, blob->data() + offset + vs);

    const char* name = FormatRegistry::Instance().GetName(type);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s @ 0x%X", name, offset);
    view.hexTitle = buf;
}

// ============================================================================
// ExportAsset — unified export through FormatRegistry
// ============================================================================

bool App::ExportAsset(const std::string& zwdPath, uint32_t offset, uint32_t size,
                      AssetType type, const std::string& suggestedName) {
    const auto* blob = cache.GetBlob(zwdPath);
    if (!blob || offset + size > (uint32_t)blob->size()) return false;

    const char* ext = FormatRegistry::Instance().GetExportExt(type);
    char fname[256];
    snprintf(fname, sizeof(fname), "export_%s_0x%X%s", suggestedName.c_str(), offset, ext);

    const uint8_t* data = blob->data() + offset;
    uint32_t writeSize = size;
    if (writeSize == 0) writeSize = 4096;
    if (offset + writeSize > (uint32_t)blob->size()) writeSize = (uint32_t)blob->size() - offset;

    // PCIM: export just the DDS data (skip 0xC1 header)
    if (type == AssetType::PCIM) {
        uint32_t ddsOff = offset + 0xC1;
        if (ddsOff >= (uint32_t)blob->size()) return false;
        data = blob->data() + ddsOff;
        writeSize = (size > 0xC1) ? size - 0xC1 : (uint32_t)blob->size() - ddsOff;
        if (writeSize > 16*1024*1024) writeSize = 16*1024*1024;
    }

    FILE* f = fopen(fname, "wb");
    if (!f) return false;
    fwrite(data, 1, writeSize, f);
    fclose(f);
    printf("[Export] Saved %s (%u bytes)\n", fname, writeSize);
    view.statusMsg = std::string("Exported: ") + fname;
    return true;
}

// ============================================================================
// World OBJ export
// ============================================================================

#include "export/obj_writer.h"

void App::ExportCurrentWorldOBJ() {
    if (scene.objects.empty() || scene.pcwb.data.empty()) {
        view.statusMsg = "No world loaded";
        return;
    }

    std::string levelName = view.worldName.empty() ? "world" : view.worldName;

    if (ExportWorldOBJ("export", levelName, scene, scene.pcwb)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Exported OBJ: export/%s/%s.obj (%d objects, %d textures)",
                 levelName.c_str(), levelName.c_str(),
                 (int)scene.objects.size(), (int)scene.textures.size());
        view.statusMsg = msg;
    } else {
        view.statusMsg = "OBJ export failed";
    }
}

// ============================================================================
// NM40 FBX export
// ============================================================================

#include "export/fbx_writer.h"

void App::ExportCurrentModelFBX() {
    if (!lastNM40Result.Valid()) {
        view.statusMsg = "No NM40 model loaded";
        return;
    }

    // Build model folder name from NM40 hash or archive entry name
    char folderName[64];
    if (view.nm40Hash)
        snprintf(folderName, sizeof(folderName), "NM40_%08X", view.nm40Hash);
    else
        snprintf(folderName, sizeof(folderName), "NM40_model");

    // Create directory structure: export/<NM40_hash>/textures/
    std::string baseDir = std::string("export/") + folderName;
    std::string texDir  = baseDir + "/textures";
    _mkdir("export");
    _mkdir(baseDir.c_str());
    _mkdir(texDir.c_str());

    // Build material info and export DDS textures
    std::vector<FBXMaterial> fbxMats;
    for (auto& slot : view.materialSlots) {
        FBXMaterial fm;
        fm.name = slot.name;

        if (slot.texId > 0 && slot.pcimHash != 0) {
            char texName[64];
            snprintf(texName, sizeof(texName), "%s_%dx%d.dds",
                     slot.name.c_str(), slot.texWidth, slot.texHeight);
            fm.texturePath = std::string("textures/") + texName;

            // Extract and save the DDS to textures subfolder
            if (view.lastArchiveNode) {
                const auto* blob = cache.GetBlob(view.lastArchivePath);
                if (blob) {
                    for (auto& child : view.lastArchiveNode->children) {
                        if (child.nameHash == slot.pcimHash) {
                            uint32_t off = child.offset;
                            if (off + 0xC1 + 4 < blob->size() && memcmp(&(*blob)[off], "PCIM", 4) == 0) {
                                uint32_t ddsOff = off + 0xC1;
                                uint32_t ddsSize = child.size > 0xC1 ? child.size - 0xC1 : 0;
                                if (ddsSize > 0 && ddsOff + ddsSize <= blob->size()) {
                                    std::string ddsPath = texDir + "/" + texName;
                                    FILE* df = fopen(ddsPath.c_str(), "wb");
                                    if (df) { fwrite(&(*blob)[ddsOff], 1, ddsSize, df); fclose(df); }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
        fbxMats.push_back(fm);
    }

    if (fbxMats.empty()) fbxMats.push_back({"Default", ""});

    std::string fbxPath = baseDir + "/" + folderName + ".fbx";
    if (ExportNM40FBX(fbxPath.c_str(), lastNM40Result, fbxMats)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Exported: %s (%d bones, %d meshes, %d textures)",
                 fbxPath.c_str(), lastNM40Result.numBones,
                 (int)lastNM40Result.batches.size(), (int)fbxMats.size());
        view.statusMsg = msg;
    } else {
        view.statusMsg = "FBX export failed";
    }
}
