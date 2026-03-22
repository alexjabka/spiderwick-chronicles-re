# SpiderView v1.0 — Complete Technical Documentation

## Overview

SpiderView is a standalone game research tool for **The Spiderwick Chronicles** (2008, Stormfront Studios). It provides interactive 3D visualization of all game assets: character models (NM40), world geometry (PCWB/PCRD), textures (PCIM), scripts (SCT/Kallis VM), databases (DBDB), and subtitles (STTL).

**Tech stack:** C++17, raylib 5.5, Dear ImGui (docking), zlib 1.3.1
**Build:** CMake 3.20+, MSVC (Visual Studio 2022)
**Deploy:** `SpiderView.exe` + `nm40_texmap.json` in game root directory

---

## Architecture

```
main.cpp (165 lines)
  Entry point, main loop, 3D rendering, object picking, armature overlay
    |
app.h / app.cpp (139 + 805 lines)
  App facade: orchestrates all subsystems
    |
    +-- Scene (scene.h/cpp) ............. GPU resources, mesh upload, Draw()
    +-- CameraController (camera.h/cpp) . Orbit + freecam modes
    +-- WorldShader (world_shader.h/cpp)  GLSL v330 shader, lighting, fog
    +-- KallisVM (vm.h/cpp) ............ SCT bytecode interpreter (64 opcodes)
    +-- GameFileSystem (game_fs.h/cpp) .. Disk + ZWD archive tree
    +-- AssetCache (asset_cache.h/cpp) .. LRU blob cache (512MB)
    +-- FormatRegistry (format_registry.h + format_handlers.cpp)
    |                                     Self-describing format handlers (14 types)
    +-- Formats (formats.h/cpp) ........ Binary parsers (PCWB, PCRD, PCIM, NM40, ZWD)
    +-- UI (panels.h/cpp) .............. ImGui panels (left tree, right inspector)
    +-- HexViewer (hex_viewer.h/cpp) ... Binary hex dump widget
```

### Source Files (23 files, ~6K LOC)

| File | Lines | Purpose |
|------|-------|---------|
| main.cpp | 165 | Window, main loop, picking, armature overlay |
| app.h | 139 | App facade, ViewerState, MaterialSlot structs |
| app.cpp | 805 | World/model loading, texture resolution, VM execution |
| formats.h | 208 | ParsedMesh, NM40MeshResult, NM40Bone, PCWBFile structs |
| formats.cpp | 800+ | PCWB/PCRD/NM40/ZWD binary parsers |
| scene.h | 130 | Scene, SceneObject, SceneTexture |
| scene.cpp | 300+ | DDS loader, mesh upload, immediate-mode Draw() |
| vm.h | 336 | KallisVM class, 64 opcodes, VMObject, VMCallFrame |
| vm.cpp | 1392 | VM interpreter, native function stubs |
| scene/camera.h | 35 | CameraController (orbit + freecam) |
| scene/camera.cpp | impl | Camera input handling |
| scene/world_shader.h | 47 | WorldShader, RenderSettings |
| scene/world_shader.cpp | 106 | GLSL shader, 13 uniforms |
| core/game_fs.h | 41 | FSNode, GameFileSystem |
| core/game_fs.cpp | impl | Directory scan, AWAD TOC parsing |
| core/asset_cache.h | 40 | AssetCache (LRU, 512MB) |
| core/asset_cache.cpp | impl | Blob loading, eviction |
| core/asset_types.h | 91 | AssetType enum, magic detection |
| core/format_registry.h | 114 | FormatHandler pattern |
| core/format_handlers.cpp | 350+ | 14 format handlers (detect, info, view, calcSize) |
| ui/panels.h | 8 | DrawLeftPanel, DrawRightPanel |
| ui/panels.cpp | 600+ | Archive tree, Materials tab, texture picker, tabs |
| ui/hex_viewer.h/cpp | ~50 | Hex dump with ASCII column |

---

## Key Data Structures

### ParsedMesh (formats.h)
CPU-side vertex data extracted from PCRD:
```cpp
struct ParsedMesh {
    vector<float> positions;    // [x,y,z,...] 3 per vertex, engine Z-up
    vector<float> normals;      // [nx,ny,nz,...] display-space Y-up
    vector<float> texcoords;    // [u,v,...] 2 per vertex
    vector<uint8_t> colors;     // [r,g,b,a,...] 4 per vertex
    vector<uint32_t> indices;   // triangle list (converted from strip)
    int vertexCount, triangleCount;
    int batchIndex;             // sub-batch index within NM40
    int boneCount;              // bone palette size
    int submeshIndex;           // mesh table entry (0=body, 1+=head)
};
```

### NM40Bone (formats.h)
Bone joint data computed from vertex blend weights:
```cpp
struct NM40Bone {
    float position[3];   // weighted centroid of all vertices referencing this bone
    int parentIndex;     // from bonePal+12 (stride 4, uint16 parent + uint16 flags)
    int vertexCount;     // number of vertices weighted to this bone
};
```

### MaterialSlot (app.h)
Blender-style per-submesh material assignment:
```cpp
struct MaterialSlot {
    string name;                // "Body", "Head"
    int materialIndex;          // index into Scene::materials
    int texWidth, texHeight;
    unsigned int texId;         // GPU texture ID for ImGui preview
    uint32_t pcimHash;          // AWAD nameHash of applied PCIM
    vector<int> objectIndices;  // scene objects using this slot
};
```

### ViewerState (app.h)
All UI/presentation state:
```cpp
struct ViewerState {
    string dataText, scriptText;         // tab content
    vector<uint8_t> hexData;             // hex viewer
    Texture2D previewTex;                // inline PCIM preview
    int selectedObject;                  // picked scene object
    vector<MaterialSlot> materialSlots;  // NM40 material slots
    int selectedSlot;                    // active slot for assignment
    uint32_t nm40Hash;                   // loaded NM40's AWAD nameHash
    string lastArchivePath;              // for texture picker
    FSNode* lastArchiveNode;             // for texture picker
    unordered_map<uint32_t, unsigned int> thumbCache; // PCIM thumbnails
    string statusMsg;
};
```

---

## Binary Format Parsing

### AWAD Table of Contents (game_fs.cpp)
Two-level indirection, reversed from Kallis runtime:
```
+0:  "AWAD" magic (4 bytes)
+4:  version (4) = 1
+8:  entryCount (4)
+12: Index table: entryCount x {nameHash(4), recordPtr(4)}
     Each recordPtr -> {typeHash(4), dataOffset(4)}
```
**Type hashes:** NM40=0x0000BB12, PCIM=0x01F1096F, SCT=0x0006D8A6

### NM40 Header (formats.cpp)
```
+0x00: "NM40" magic
+0x04: version (2)
+0x08: numBones (uint16)
+0x0A: numSkelBones (uint16)
+0x0C: scaleX,Y,Z,W (4 x float)
+0x1C: lodDist (float)
+0x20: flags (uint8)
+0x24: numMeshTableEntries (uint16)
+0x26: numSubmeshes (uint16)
+0x28: idxDataOff (uint32, fixup'd by engine)
+0x2C: idxBufSize (uint32)
+0x30: vtxDeclOff (uint32, fixup'd)
+0x34: meshTblOff (uint32, fixup'd)
+0x38: bonePalOff (uint32, fixup'd)
+0x3C: subTblOff (uint32, fixup'd)
```

### NM40 Mesh Table Chain
```
meshTblOff -> mesh table entries (8 bytes each)
  +0: uint16 unknown
  +2: uint16 subBatchCount
  +4: uint32 subBatchArrayPtr (NM40-relative)

subBatchArray -> entries (16 bytes each)
  +4: uint16 boneCount
  +8: uint32 bonePalettePtr (NM40-relative, uint8 per bone)
  +12: uint32 pcrdPtr (NM40-relative)
```

### NM40 Vertex Format (stride=52)
```
+0:  POSITION  (3 x float, 12 bytes)
+12: NORMAL    (3 x float, 12 bytes)
+24: TEXCOORD  (2 x float, 8 bytes)
+32: BLENDIDX  (4 x uint8, 4 bytes) — bone palette indices
+36: BLENDWT   (4 x float, 16 bytes) — blend weights
```

### NM40 BonePalette Structure (at NM40 + bonePalOff)
Reversed from Kallis NM40_PointerFixup (0x20BC000) and accessor functions
(sub_53A650, sub_53A660, NM40_GetMeshHeader at 0x53A720):
```
+0:  uint16 unknown (0)
+2:  uint16 unknown
+4:  uint16 unknown
+6:  uint16 unknown
+8:  uint32 boneDataOff    (NM40-relative, fixup'd by engine)
+12: uint32 boneHierOff    (NM40-relative, fixup'd — PARENT INDICES)
+16: uint32 meshHdrExtOff  (NM40-relative, fixup'd)
```

### Bone Parent Index Format (at NM40 + boneHierOff)
```
Stride: 4 bytes per bone (numBones entries)
+0: uint16 parentIndex  (self-reference = root)
+2: uint16 flags        (purpose unknown)
```
Verified from live IDA debugger session: 1 root, anatomically correct tree.

### PCIM Header
```
+0x00: "PCIM" magic
+0x9C: width (uint32)
+0xA0: height (uint32)
+0xC1: DDS data starts (193-byte header before DDS)
```

### PCRD Header (inside PCWB or NM40)
```
+0x00: "PCRD" magic
+0x04: version (2)
+0x08: stride hint
+0x0C: index count
+0x10: vertex count
+0x14: index buffer offset
+0x18: vertex buffer offset
```

---

## Rendering Pipeline

### Main Loop (main.cpp)
```
1. Camera update (orbit/freecam input)
2. Object picking (ray-triangle intersection)
3. BeginMode3D()
4.   WorldShader.Begin() → set 13 uniforms
5.   Scene.Draw() → immediate-mode rlBegin/rlEnd per object
6.   WorldShader.End()
7.   Selection wireframe (yellow)
8.   Armature overlay (X-ray: disable depth, draw bone lines + joints)
9. EndMode3D()
10. ImGui panels (left tree, right inspector)
```

### Scene.Draw() (scene.cpp)
- Uses rlBegin(RL_TRIANGLES)/rlEnd() — NOT VAO/VBO (would corrupt GL state at 2000+ meshes)
- Per-object: bind texture via rlSetTexture(), iterate triangles
- Coordinate conversion at render time: engine(x,y,z) -> raylib(x,z,-y)
- Two-pass for NM40: primary material then secondary (detail) if set

### WorldShader (world_shader.cpp)
GLSL v330 with 13 dynamic uniforms:
- layerTextures, layerVertColor, layerLighting
- lightBoost, ambientLevel
- dirLightEnable, dirLightYaw/Pitch/Intensity
- normalLighting (NM40 mode: vertex colors = encoded normals)
- fogEnable, fogColor, fogStart, fogEnd
- Alpha clipping: threshold 15/255 (matches engine D3DRS_ALPHAREF)

---

## NM40 Texture Resolution

### Architecture
The NM40->PCIM texture mapping is NOT stored in any game data file. It exists
only in the Kallis runtime's obfuscated character factory code.

**Factory chain (reversed via IDA):**
```
sub_4514E0 (factory wrapper)
  -> sub_55C3E0 (Kallis thunk, ecx=descriptor output buffer)
    -> 0x1EFF000 (Kallis runtime, obfuscated)
      -> sub_55C270 -> 0x47B550 -> sub_52A410 (hash table iterator)
        -> sub_58BC70 (reads [ecx+0Ch] for table indirection)
          -> sub_58B9E0 (binary search on sorted hash table)
```

### Descriptor Layout (ClNoamActor_Init 0x527EC0)
```
desc[0]  = ANIZ animation archive
desc[1]  = NM40 mesh
desc[2]  = MAIN DIFFUSE PCIM (1024x1024 body)
desc[3]  = SECONDARY PCIM (256x256 for goblins, NULL for players)
desc[4]  = DETAIL PCIM (512x512 face/hair for players)
desc[5-7]= additional assets (usually NULL)
desc[13] = texA (64x64 secondary, enemies only)
desc[14] = texB (tiny secondary)
```

### Auto-Discovery Pipeline
Since the mapping can't be emulated from raw data, we capture it from
the running game via SpiderMod's ClNoamActor_Init hook:

1. SpiderMod hooks ClNoamActor_Init, logs desc[1], desc[2], desc[4] data pointers
2. Writes to `spiderview_texmap.txt` (10K+ lines across all chapters)
3. Python matching script (`data/ida_capture_texmap.py`) computes offset deltas
4. Matches deltas against ALL WADs' AWAD TOCs to find nameHashes
5. Outputs `nm40_texmap.json`

### Per-Submesh Assignment
- Mesh table entry 0 sub-batches -> desc[2] (body diffuse)
- Mesh table entry 1+ sub-batches -> desc[4] (face/hair detail)
- Implemented in AutoResolveTexture() and PreviewModel()

### Manual Assignment (Materials Tab)
- Collapsible material slots with texture previews
- Inline texture picker: all PCIMs from current WAD, sorted by size, with thumbnails
- Click thumbnail to assign to selected slot
- NM40 hash + PCIM hashes displayed for easy mapping capture

---

## Bone Hierarchy System

### Discovery Process
1. Reversed NM40_PointerFixup (Kallis 0x20BC000) — found bonePal+8/+12/+16 get fixup'd
2. Reversed accessor functions (0x53A650, 0x53A660, 0x53A720) — read bonePal sub-offsets
3. Dumped raw bytes at bonePal+12 — found uint16 pairs at stride 4
4. Validated from live IDA debugger: 1 root, parent < numBones, correct tree

### Bone Positions
Computed as weighted centroids of all vertices referencing each bone.
NOT from bind pose matrices (engine stores zeros until animation plays).

### Armature Visualization (main.cpp)
- X-ray mode: rlDisableDepthTest() + rlDrawRenderBatchActive()
- Stick bones: RL_LINES from parent to child (cyan)
- Joint dots: tiny octahedra at each bone position (root=bright, child=dim)

---

## Key Algorithms

### Triangle Strip to List Conversion
```cpp
for (int i = 0; i + 2 < indexCount; i++) {
    uint16_t a = indices[i], b = indices[i+1], c = indices[i+2];
    if (a == b || b == c || a == c) continue; // degenerate
    if (i & 1) swap(a, b); // alternate winding for odd triangles
    output.push_back(a); output.push_back(b); output.push_back(c);
}
```

### Object Picking (main.cpp)
1. GetMouseRay() from camera
2. BBox intersection on all visible objects (AABB pre-filter)
3. Per-triangle Moller-Trumbore intersection test
4. Track closest hit distance -> selectedObject

### DDS Texture Loading (scene.cpp)
Three-level fallback:
1. raylib LoadImageFromMemory(".dds") — handles most formats
2. TryLoadDDSFallback() — manual DXT1/3/5 + RGB32/24/16 with BGRA->RGBA
3. Magenta placeholder if both fail

### AWAD Blob Scanning (game_fs.cpp)
```cpp
// Two-level TOC: index table + record indirection
for (uint32_t i = 0; i < count; i++) {
    uint32_t nameHash = ReadU32(blob, 12 + i*8);
    uint32_t entPtr   = ReadU32(blob, 12 + i*8 + 4);
    uint32_t typeHash = ReadU32(blob, entPtr);
    uint32_t dataOff  = ReadU32(blob, entPtr + 4);
    // typeHash 0xBB12 = NM40, 0x01F1096F = PCIM
}
```

---

## Coordinate Systems

**Engine (Z-up, left-handed):** X=right, Y=forward, Z=up
**Raylib (Y-up, right-handed):** X=right, Y=up, Z=forward

Conversion at render time: `rlVertex3f(eng_x, eng_z, -eng_y)`
Applied in Scene::Draw() and armature overlay.

---

## Build & Deploy

```bash
# Build
cd _Spiderwick_RE/tools/spiderview/build
cmake --build . --config Release

# Deploy
cp build/Release/SpiderView.exe "H:\Games\.Archive\SPIDEWICK/"
cp data/nm40_texmap.json "H:\Games\.Archive\SPIDEWICK/"
```

### Dependencies (auto-fetched by CMake)
- raylib 5.5 — rendering, window, input, DDS loading
- Dear ImGui (docking branch) — all UI
- rlImGui — raylib+ImGui integration
- zlib 1.3.1 — SFZC/ZWD decompression

---

## nm40_texmap.json Format

```json
{
  "characters": [
    {
      "nm40": "0x2B5B0254",     // NM40 AWAD nameHash
      "bones": 56,              // bone count (informational)
      "diffuse": "0x687335CB",  // body texture PCIM nameHash
      "detail": "0x0E55B3AF"   // face/hair texture PCIM nameHash (optional)
    }
  ]
}
```
SpiderView searches for this file next to the exe, then in `data/` subfolder.
Currently contains 33+ entries covering all player characters, bosses, and enemies.

### Adding New Mappings
1. Load NM40 model in SpiderView
2. Materials tab shows `NM40: 0xXXXXXXXX` at the top
3. Use texture picker to find correct textures
4. After applying, `Body: 0xXXXXXXXX` shows the PCIM hash
5. Add entry to nm40_texmap.json with these hashes

---

## Known Limitations

1. **No VAO/VBO** — immediate-mode rlBegin/rlEnd (VAO path corrupts GL state at 2000+ meshes)
2. **Bone positions are approximate** — vertex centroids, not real joint locations
3. **Bone transforms are zeros** — engine only fills during animation playback
4. **NM40 texture mapping requires runtime capture** — not derivable from raw data
5. **14 NM40 models unmapped** — offset deltas didn't match any WAD in the scan
6. **PCIM thumbnail loading is lazy** — first scroll through picker may stutter
7. **No animation playback** — models shown in bind pose only

---

## Engine Subroutines Reference

### Kallis Runtime (dynamically loaded, addresses vary)
| Address | Name | Purpose |
|---------|------|---------|
| 0x20BC000 | NM40_PointerFixup | Converts NM40-relative offsets to absolute pointers |
| 0x1EFF000 | DescriptorBuilder | Builds 22-DWORD character descriptor |

### Main Executable (fixed addresses, no ASLR)
| Address | Name | Purpose |
|---------|------|---------|
| 0x527EC0 | ClNoamActor_Init | Copies descriptor, creates ModelInstance + NoamFigure |
| 0x523A00 | ModelInstance_Init | Allocates bone matrices (128 bytes/bone, identity) |
| 0x53A650 | GetBoneTransforms | Returns bonePal + *(bonePal+0x0C), 192 bytes/bone |
| 0x53A660 | GetBoneRemap | Returns bonePal + *(bonePal+0x10), parent indices |
| 0x53A720 | NM40_GetMeshHeader | Returns bonePal + *(bonePal+0x14), submesh data |
| 0x56D860 | NM40_Load | Validates magic, calls NM40_PointerFixup |
| 0x56DB00 | PCIM_Load | Validates PCIM magic |
| 0x56DCF0 | PCIM_Factory | Creates PCIM asset wrapper |
| 0x56D940 | NM40_Factory | Creates NM40 asset wrapper |
| 0x4514E0 | CharacterFactory | Builds descriptor, calls ClNoamActor_Init |
| 0x52A410 | HashTableIterator | Iterates 16 hash tables for asset resolution |
| 0x58B9E0 | BinarySearchLookup | Binary search on sorted (hash, value) table |
| 0x58BC70 | TableIndirect | Reads [ecx+0Ch] then calls BinarySearchLookup |
| 0x405380 | HashString | Engine's name hash: `h += c + (h << (c & 7))` |
| 0x571000 | RENDERFUNC_NOAM_DIFFUSE | NM40 character render entry point |
| 0x562760 | RenderDispatch | Deferred two-pass renderer |
| 0x59D000 | TextureBindCallback | Pre-render: binds texture from texObj+0xAC |
| 0x488DD0 | CameraSectorUpdate | Per-frame camera + sector update |
| 0x519090 | WorldStatic_Load | Full world initialization from PCWB |

### Key Global Variables
| Address | Name | Purpose |
|---------|------|---------|
| 0xE416C4 | g_WorldState | Loaded PCWB pointer |
| 0xE55CC0 | dword_E55CC0[16] | Asset hash table registry (factory lookup) |
| 0x72F670 | g_pCameraSystem | Main camera object pointer |
| 0xE56160 | g_VMObjectBase | VM object reference base |

---

## Failures & Dead Ends (documented for future reference)

### NM40 Bone Hierarchy Scanning (FAILED)
- **Brute-force int16 parent scan** of entire NM40 file — found false positives
- **Nearest-neighbor MST** from vertex centroids — wrong connections for fingers/legs
- **Bone palette chain ordering** — palette order doesn't follow parent-child
- **Solution that worked:** Parse bonePal+12 from raw file (stride 4, uint16 parent)

### Kallis Runtime Emulation (FAILED)
- Factory code at 0x1EFF000 is XOR-obfuscated (computed jumps via `push eax; retn`)
- Hash table entries use encrypted internal IDs, not AWAD nameHashes
- AWAD header is destroyed after loading (can't find TOC in memory)
- **Solution that worked:** Runtime capture via SpiderMod hook + offset matching

### AWAD Data Scanning for Templates (FAILED)
- Scanned all AWAD entries for uint32 values matching NM40 nameHashes
- Character templates are NOT stored as AWAD entries
- NM40 and PCIM nameHashes have no relationship in the AWAD TOC ordering
- **Solution that worked:** Capture descriptors at runtime, match by offset delta

### Two-Pass NM40 Rendering (PARTIALLY FAILED)
- Initially implemented engine's RENDERFUNC_NOAM_SKIN two-pass (desc[13]/desc[14])
- Player characters use Kallis runtime path, not RENDERFUNC_NOAM_SKIN
- desc[13]/desc[14] are tiny secondary maps (64x64), NOT main textures
- **Solution that worked:** Per-submesh texture assignment (entry 0=body, 1+=head)

### Bone Transform Matrices (NOT AVAILABLE)
- ModelInstance_Init allocates 128 bytes/bone initialized to IDENTITY
- Live debugger confirmed ALL ZEROS for bone transforms
- Engine only fills transforms during animation playback
- **Workaround:** Use vertex centroid positions instead
