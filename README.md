# The Spiderwick Chronicles — Reverse Engineering & Modding Toolkit

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Game](https://img.shields.io/badge/Game-Spiderwick%20Chronicles%20PC%20(2008)-green.svg)](#)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](#)

> **Extract, view, and mod every asset from The Spiderwick Chronicles (PC, 2008).**
> 3D models, textures, worlds, scripts, animations — all formats reversed and documented.

**Game:** The Spiderwick Chronicles (PC, 2008) by Stormfront Studios / Sierra Entertainment
**Engine:** Proprietary "Ogre" engine, 32-bit, DirectX 9, Kallis VM scripting
**Status:** Engine substantially reversed — 150+ functions named, 21 asset types mapped, full render pipeline traced

<p align="center">
  <i>Built for game preservation, modding research, and fans of the Spiderwick universe.</i>
</p>

---

## What Is This?

A complete reverse engineering and modding toolkit for *The Spiderwick Chronicles* PC game (2008), based on the book series by Holly Black and Tony DiTerlizzi. This project provides everything needed to explore, extract, and understand the game's internals:

- **SpiderView** — A game research tool (like OpenIV for GTA) for browsing archives, viewing 3D character models with skeletons, loading full game worlds, and exporting to FBX/OBJ for Blender
- **SpiderMod** — An in-game ASI mod with freecam, debug overlays, entity spawning, and chapter control
- **Python Tools** — Asset extraction and format conversion (models, textures, databases, scripts, worlds)
- **Engine Documentation** — 160+ markdown docs covering every reversed engine subsystem
- **IDA Analysis** — 150+ named functions, complete type registry, VM interpreter, render pipeline

---

## Quick Start

### SpiderView (Game Research Tool)

```bash
cd tools/spiderview
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Place `SpiderView.exe` in the game directory and run. It auto-discovers `ww/Wads/` archives.

**Requirements:** CMake 3.20+, MSVC (Visual Studio 2019+), Windows SDK

### Python Asset Extractor

```bash
# Extract all assets from a ZWD archive (textures, models, scripts, databases)
python tools/spiderwick_unpack.py path/to/game/ww/Wads/GroundsD.zwd output_dir/

# Batch extract all archives
python tools/spiderwick_unpack.py path/to/game/ww/ output_dir/
```

**Requirements:** Python 3.8+, no external dependencies

### SpiderMod (ASI Mod)

```bash
cd mods/spidermod
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Place `spidermod.asi` and `dinput8.dll` in the game directory. Press `INSERT` for the debug menu, `HOME` for freecam.

---

## Project Structure

```
_Spiderwick_RE/
|
|-- docs/                              Project-level documentation
|   |-- NM40_FORMAT.md                 Skinned mesh format (complete)
|   |-- NM40_TEXTURE_PIPELINE.md       Character texture binding pipeline (complete)
|   |-- APPROACH.md                    RE philosophy & methodology
|   |-- SESSION_LOG.md                 Chronological findings log
|   |-- STRINGS_GUIDE.md              String recovery from hashed archives
|   +-- IDA_MCP_SETUP.md              IDA Pro + AI-assisted RE setup
|
|-- engine/                            Reversed engine systems
|   |-- formats/                       File format documentation
|   |   |-- ZWD_FORMAT.md             Archive format (AWAD TOC, 21 asset types)
|   |   |-- PCW_FORMAT.md             World geometry container
|   |   |-- SCT_FORMAT.md             Kallis VM bytecode format
|   |   |-- DBDB_FORMAT.md            Binary database format
|   |   |-- WF_FORMAT.md              Widget font / glyph atlas
|   |   |-- SKELETON_FORMAT.md        Bone hierarchy format
|   |   |-- WORLD_GEOMETRY.md         PCRD/PCIM rendering data
|   |   +-- ASSET_LOADING.md          Engine asset loading pipeline
|   |
|   |-- vm/                           Kallis VM runtime (fully reversed)
|   |   |-- KALLIS_VM.md              VM architecture overview
|   |   |-- VM_BYTECODE_FORMAT.md     64 opcodes, instruction encoding
|   |   |-- VM_INTERPRETER.md         Main interpreter loop
|   |   |-- VM_STACK_SYSTEM.md        Dual-stack system (args + returns)
|   |   |-- VM_OBJECT_SYSTEM.md       11 object classes
|   |   |-- VM_FUNCTION_TABLE.md      ~1100 function entries
|   |   +-- VM_FUNCTION_REGISTRATION.md  92 native functions
|   |
|   |-- camera/                       Camera system (fully reversed)
|   |-- sectors/                      Sector streaming (fully reversed)
|   |-- world/                        World loading pipeline
|   |-- objects/                      Character/object system
|   |-- rendering/                    D3D9 render pipeline, texture binding
|   |-- events/                      Script event dispatch
|   |-- time/                        Clock/pause system
|   +-- ...                          (debug, fade, data store, services)
|
|-- tools/
|   |-- spiderview/                   OpenIV-style research tool (C++)
|   |   |-- CMakeLists.txt
|   |   |-- src/
|   |   |   |-- main.cpp             Entry point, 3D viewport, ImGui
|   |   |   |-- app.h/.cpp           Application facade + viewer state
|   |   |   |-- formats.h/.cpp       PCWB/PCRD/PCIM/NM40 parsers
|   |   |   |-- scene.h/.cpp         3D scene, two-pass rendering
|   |   |   |-- vm.h/.cpp            Kallis VM interpreter (64 opcodes)
|   |   |   |-- core/                Asset cache, game filesystem, format registry
|   |   |   |-- scene/               Camera controller, world shader
|   |   |   |-- export/              FBX + OBJ exporters
|   |   |   +-- ui/                  ImGui panels, hex viewer
|   |   +-- data/                    nm40_texmap.json (texture mappings)
|   |
|   |-- spiderwick_unpack.py         Asset extractor (ZWD/AWAD archives)
|   |-- spiderwick_world_export.py   World geometry OBJ+MTL exporter
|   |-- spiderwick_glb.py            Skinned model glTF exporter
|   |-- sct_disasm.py                SCT bytecode disassembler
|   |-- fmt_spiderwick_pcwb.py       Noesis plugin for PCWB import
|   |-- match_prop_transforms.py     Prop placement matcher
|   |-- analysis/                    Format analysis & hex dump scripts
|   +-- ce_scripts/                  Cheat Engine Lua diagnostic scripts
|
|-- mods/
|   |-- spidermod/                   ASI mod (dinput8.dll proxy)
|   |   |-- src/
|   |   |   |-- dllmain.cpp          DLL entry, 10 engine hooks
|   |   |   |-- freecam.cpp/h        Freecam implementation
|   |   |   |-- menu.cpp/h           ImGui debug menu
|   |   |   |-- d3d_hook.cpp/h       D3D9 EndScene hook
|   |   |   |-- sector.cpp/h         Sector visibility control
|   |   |   +-- addresses.h          All engine addresses
|   |   +-- CMakeLists.txt
|   |-- freecam/                     Standalone CE freecam table
|   +-- debug/                       Portal bypass, test patches
|
+-- reference/                       External reference files
```

---

## Reversed Engine Systems

### Asset Type Registry (Complete — 21 Types)

Every asset type registered via `ClAssetTypeMap_Register` (0x52A340):

| Type | typeHash | Magic | Description |
|------|----------|-------|-------------|
| Mesh | 0x0000BB12 | NM40 | Skinned character/prop model |
| Image | 0x01F1096F | PCIM | DDS texture with 193-byte header |
| Prop | 0x00020752 | PCPB | Prop with embedded geometry + textures |
| Script | 0x0006D8A6 | SCT | Kallis VM compiled bytecode |
| Database | 0x04339C43 | DBDB | Binary key-value database |
| Animation | 0x44FE8920 | adat | Skeletal animation data |
| AnimationList | 0xFA5E717C | aniz | Compressed animation package |
| NavMesh | 0x194738A4 | NAVM | Navigation mesh for AI |
| ActionStateMachine | 0xBCBFB478 | ac0000ac | AI behavior state machine |
| BehaviorStateMachine | 0x5550C44A | a10000a1 | AI behavior tree |
| SubtitleTimeline | 0xA117D668 | STTL | Subtitle timing data |
| Font | 0x009A2807 | WF | Widget font glyph atlas |
| AssetMap | 0x2A7E6F30 | AMAP | Animation map |
| ControlMap | 0xB8D1C6C6 | Devi | Device/controller config |
| Playlist | 0x06572A64 | play | Audio playlist |
| AudioEffects | 0xE2F8E66E | EPC | Audio effect definitions |
| Strings | 0x690BE5E8 | STRI | Localized string table |
| Table | 0x000FD514 | --- | Generic data table |
| LoadedCinematic | 0x3072B942 | brxb | In-memory cinematic |
| StreamedCinematic | 0xEAB838D4 | --- | Streamed video |
| SaveGameIcon | 0x451A228E | --- | Save file thumbnail |

### Kallis VM (Fully Reversed)

The game's scripting VM with 64 opcodes, 4 register banks, dual-stack architecture:

- **64 opcodes** — arithmetic, control flow, field access, method dispatch
- **~1100 function table entries** — global NTV + per-class VTL + STE methods
- **92 native functions** — `sauSetPosition`, `sauCreateCharacter`, `sauSendTrigger`, etc.
- **11 object classes** — Character, Pickup, Camera, Trigger, etc.
- **SCT v13 format** — 52-byte header, NTV/VTL/STE tables, string resolve table

### NM40 Character Pipeline (Fully Reversed)

Complete path from script to screen:

```
SCT Script                    AWAD Archive
  sauCharacterInit()            NM40 (Mesh)
    | pops model name           PCIM (Texture A)
    | pops skin name            PCIM (Texture B)
    v                           adat (Animations)
  Character Descriptor [22 slots]
    [0]  = NM40 mesh
    [13] = PCIM texture A (primary)
    [14] = PCIM texture B (secondary)
    |
    v
  ClNoamActor_Init (0x527EC0)
    -> ModelInstance_Init (0x523A00)
    -> NoamFigure_Init (0x56E970)
    -> MaterialBuilder_Init (0x5861A0)
    -> SkinRenderer_Init (0x589760)
    |
    v
  RENDERFUNC_NOAM_DIFFUSE (0x571000)
    Pass 1: bind TEX_A, draw all material groups
    Pass 2: bind TEX_B, draw overlay groups
    -> D3D_SetTexture (0x4E9970)
```

### Render Pipeline

- Two-pass rendering: base textures (opaque) + shadow overlays (multiply blend)
- Batch sentinel pattern (3x FFFFFFFF) for PCRD-to-texture mapping
- 57+ render group types registered
- Deferred queue: `AddToRenderQueue` -> `RenderDispatch`

---

## SpiderView Features

| Feature | Status |
|---------|--------|
| Archive browser (AWAD TOC) | Working |
| NM40 3D model viewer (multi-material) | Working |
| PCWB world loading (with geometry instancing) | Working |
| FBX export (mesh + skeleton + skin weights) | Working |
| OBJ world export (grouped by texture + DDS) | Working |
| Texture preview (PCIM, inline thumbnails) | Working |
| Materials UI (Blender-style slots, texture picker) | Working |
| Armature visualization (stick bones, X-ray) | Working |
| Script viewer (SCT) | Working |
| Database viewer (DBDB) | Working |
| Subtitle viewer (STTL) | Working |
| Hex viewer | Working |
| Export All DDS from archive | Working |
| Blender-style camera (orbit, pan, zoom, fly mode) | Working |
| Hotkey overlay (H to toggle) | Working |
| Object picking (click to select) | Working |
| VM script execution | Working |
| 33+ auto texture mappings (nm40_texmap.json) | Working |

---

## SpiderMod Features

| Feature | Status |
|---------|--------|
| Freecam (WASD + mouse) | Working |
| Debug overlay (3D labels) | Working |
| Character/sector/prop overlay | Working |
| Entity spawning | Working |
| Character hot-switching | Working |
| Portal bypass (all rooms visible) | Working |
| Force All Visible (chapter-gated enemies) | Working |
| Chapter setter (quick Ch1-Ch8 buttons) | Working |
| Launch.txt config redirect | Working |
| Scene dump (geometry + VM placements) | Working |
| Prop positions JSON dump (per-level) | Working |

---

## Python Tools

| Tool | Purpose |
|------|---------|
| `spiderwick_unpack.py` | Extract all assets from ZWD archives (NM40->OBJ, PCIM->DDS, DBDB->JSON, STTL->JSON, WF->JSON) |
| `spiderwick_world_export.py` | Export world geometry as OBJ+MTL with textures |
| `spiderwick_glb.py` | Export skinned models as glTF |
| `sct_disasm.py` | Disassemble SCT bytecode to readable text |
| `fmt_spiderwick_pcwb.py` | Noesis plugin for PCWB world import |
| `match_prop_transforms.py` | Match prop placements between scene dump and PCWB |

---

## Key Engine Addresses

<details>
<summary>150+ named functions (click to expand)</summary>

### Asset System
| Address | Name | Purpose |
|---------|------|---------|
| 0x52A340 | ClAssetTypeMap_Register | Register asset type with factory |
| 0x58B770 | ClAssetTypeMap_Lookup | Look up asset factory by typeHash |
| 0x52B0E8 | SfAssetRepository_Load | Load ZWD/WAD archive |
| 0x405380 | HashString | Engine name hash function |

### NM40 Mesh
| Address | Name | Purpose |
|---------|------|---------|
| 0x56D970 | NM40_Register | Register "Mesh" type |
| 0x56D940 | NM40_Factory | Create NM40 asset object |
| 0x56D860 | NM40_Load | Validate magic + call fixup |
| 0x20BC000 | NM40_PointerFixup | Convert relative offsets to pointers |
| 0x53A640 | NM40_GetLODCount | LOD count accessor |
| 0x53A8D0 | NM40_GetSubmeshCount | Submesh count accessor |
| 0x53A8F0 | NM40_GetSubmeshDesc | Submesh descriptor accessor |

### Character System
| Address | Name | Purpose |
|---------|------|---------|
| 0x527EC0 | ClNoamActor_Init | Character init (22-slot descriptor) |
| 0x523A00 | ModelInstance_Init | Create model from NM40 (304 bytes) |
| 0x56E970 | NoamFigure_Init | Create render figure (TEX at +92/+96) |
| 0x5861A0 | MaterialBuilder_Init | Per-submesh material creation |
| 0x589760 | SkinRenderer_Init | Skin material renderer |
| 0x458170 | sauCharacterInit | VM native: hash names, init character |
| 0x44C6C0 | sauCreateCharacter | VM native: spawn character |
| 0x44C600 | SpawnCharacter | Internal character spawner |

### Rendering
| Address | Name | Purpose |
|---------|------|---------|
| 0x571000 | RENDERFUNC_NOAM_DIFFUSE | Character diffuse 2-pass render |
| 0x4E9970 | D3D_SetTexture | Bind D3D9 texture + sampler states |
| 0x59D000 | PropTextureBind_Callback | Prop pre-render texture bind |
| 0x562760 | RenderDispatch | Main deferred renderer |
| 0x562D30 | AddToRenderQueue | Add to render queue |
| 0x4EA160 | D3D_CreateVBIB | Create vertex/index buffers |
| 0x4ED400 | D3D_CreateTextureFromPCIM | Create D3D texture from PCIM |

### Kallis VM
| Address | Name | Purpose |
|---------|------|---------|
| 0x52D9C0 | VMInterpreter | Main bytecode interpreter loop |
| 0x52EA70 | VMExecute | Execute VM function |
| 0x52EB40 | VMCall | Call VM function by hash |
| 0x52EC30 | VMInitObject | Initialize VM object |
| 0x52EA10 | VMRegisterFunction | Register native function |
| 0x52E660 | VMRegisterMethod | Register native method |
| 0x49BA20 | RegisterGlobalVMFunctions | Register all 92 natives |

### Camera
| Address | Name | Purpose |
|---------|------|---------|
| 0x5293D0 | CameraRender_SetFovAndClip | Per-frame FOV/clip setup |
| 0x529790 | CameraRender_RebuildMatrices | Build view/projection matrices |
| 0x52A150 | CameraRender_WorldToScreen | World-to-screen projection |
| 0x5A8B20 | Frustum_BuildProjectionMatrix | 4x4 projection matrix |
| 0x4368B0 | GetCameraObject | Get active camera |

### World
| Address | Name | Purpose |
|---------|------|---------|
| 0x518C20 | ClWorld_LoadFromFile | Load world from file |
| 0x519090 | WorldStatic_Load | Static world initialization |
| 0x51A130 | PortalTraversal_Native | Portal visibility traversal |
| 0x488970 | SectorVisibilityUpdate | Sector streaming update |

</details>

---

## File Formats Reference

| Format | Magic | Description | Doc |
|--------|-------|-------------|-----|
| AWAD | `AWAD` | Hash-indexed asset archive (two-level TOC) | [ZWD_FORMAT.md](engine/formats/ZWD_FORMAT.md) |
| PCWB | `PCWB` | World geometry container (v10) | [PCW_FORMAT.md](engine/formats/PCW_FORMAT.md) |
| NM40 | `NM40` | Skinned mesh (52B vertices, embedded PCRD) | [NM40_FORMAT.md](docs/NM40_FORMAT.md) |
| PCRD | `PCRD` | Renderable geometry (tri-strip, v2) | [WORLD_GEOMETRY.md](engine/formats/WORLD_GEOMETRY.md) |
| PCIM | `PCIM` | Texture (193B header + DDS) | [WORLD_GEOMETRY.md](engine/formats/WORLD_GEOMETRY.md) |
| SCT | `SCT\0` | Kallis VM bytecode (v13) | [SCT_FORMAT.md](engine/formats/SCT_FORMAT.md) |
| DBDB | `DBDB` | Binary database (typed records) | [DBDB_FORMAT.md](engine/formats/DBDB_FORMAT.md) |
| STTL | `STTL` | Subtitle timeline | engine/formats/ |
| PCPB | `PCPB` | Prop (embedded PCRD + PCIM) | engine/formats/ |

---

## Building

### SpiderView

SpiderView auto-downloads its dependencies (raylib, imgui, zlib) via CMake FetchContent.

```bash
cd tools/spiderview
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### SpiderMod

SpiderMod auto-downloads its dependencies (MinHook, Dear ImGui) via CMake FetchContent.

```bash
cd mods/spidermod
cmake -B build
cmake --build build --config Release
# Copy spidermod.asi + dinput8.dll to game directory
```

### Python Tools

No dependencies beyond Python 3.8+ standard library.

```bash
python tools/spiderwick_unpack.py <file_or_dir> [output_dir]
```

---

## Contributing

This is a research project. Contributions welcome in:

- **Format RE** — any of the partially-reversed formats (PCPB internals, animation system, audio)
- **SpiderView** — new format viewers, auto texture resolution, UI improvements
- **SpiderMod** — new debug features, engine patches
- **Documentation** — corrections, additional findings

Please open an issue before starting major work to avoid duplication.

---

## Legal

This project is for educational and preservation purposes. It does not include any copyrighted game assets, executables, or proprietary code. All knowledge was obtained through clean-room reverse engineering of publicly available software.

*The Spiderwick Chronicles* is a trademark of Simon & Schuster. This project is not affiliated with or endorsed by Simon & Schuster, Stormfront Studios, or Sierra Entertainment.

---

## What Can You Do With This?

- **Extract all game assets** — every texture, 3D model, sound, script, and database from the game's archives
- **View characters in 3D** — load any NM40 character model with textures and bone armature
- **Explore game worlds** — load complete levels with all props and objects correctly placed
- **Export to Blender** — FBX with full skeleton + skin weights for characters, OBJ + textures for worlds
- **Free camera** — fly through the game world with no restrictions, see all rooms and hidden areas
- **Mod the game** — debug overlays, entity spawning, chapter control, force-load invisible enemies
- **Understand the engine** — 160+ pages of documentation on every engine system

---

## Downloads

Pre-built binaries are available on the [Releases](../../releases) page.

---

<sub>
Keywords: spiderwick chronicles pc game 2008 modding reverse engineering asset extractor 3d model viewer
stormfront studios sierra entertainment ogre engine kallis vm holly black tony diterlizzi
texture extractor world viewer blender export fbx obj dds game preservation
</sub>

---

## License

[MIT](LICENSE)
