# Spiderwick Chronicles — Reverse Engineering Index

**Engine:** "Ogre" by Stormfront Studios (proprietary, NOT open-source OGRE3D)
**Binary:** Spiderwick.exe (32-bit x86, MSVC)
**IDA DB:** Spiderwick.exe.i64 (209 MB)
**ASI Mod:** `mods/spidermod/` (9 hooks, ImGui overlay, 340+ addresses)

---

## Documentation Statistics

| Category | Files | Sub Docs |
|----------|-------|----------|
| Camera & Rendering | 56 | 47 |
| VM / Scripting | 39 | 26 |
| Objects & Characters | 37 | 32 |
| World & Sectors | 12 | 11 |
| Data & Hash | 3 | 3 |
| Events | 3 | 3 |
| Debug & Cheats | 7 | 6 |
| Fade System | 3 | 3 |
| Time System | 5 | 5 |
| Services | 2 | 2 |
| Formats | 6 | 0 |
| **Total** | **227** | **163** |

---

## Engine Subsystems

### Camera & Rendering (`engine/camera/`)
- `ARCHITECTURE.md` — Camera system overview
- `CAMERA_RENDERING.md` — Render pipeline
- `WORLD_TO_SCREEN.md` — 3D→2D projection
- `PORTAL_SYSTEM.md` — Portal-based visibility
- `ROOM_CLIPPING.md` — Room/sector clipping
- `FREECAM_PLAN.md` — Free camera implementation
- `MONOCLE_INVESTIGATION.md` — Mouse look system
- `structs/camera_struct.md` — Camera object layout (+0x6B8 pos, +0x788 sector, +0x790 view matrix)
- `structs/camera_settings.md` — FOV, clip planes, aspect ratio
- `structs/position_accessors.md` — Position read/write patterns
- **47 sub docs** in `camera/subs/` including:
  - `sub_488DD0_CameraSectorUpdate.md` — Main update hook point
  - `sub_562760_RenderDispatch.md` — Draw call dispatch loop
  - `sub_562D30_AddToRenderQueue.md` — Render queue submission
  - `sub_5A8B20_Frustum__BuildProjectionMatrix.md`
  - `sub_5AA1B0_BuildViewMatrix.md`

### Kallis VM (`engine/vm/`)
- `KALLIS_VM.md` — Complete VM architecture
- `VM_BYTECODE_FORMAT.md` — SCT file format v13
- `VM_INTERPRETER.md` — All 64 opcodes documented
- `VM_FUNCTION_TABLE.md` — Function/class tables
- `VM_STACK_SYSTEM.md` — Dual stack system
- `VM_OBJECT_SYSTEM.md` — VM↔Native object binding
- `VM_DISPATCH_RESEARCH.md` — ROP-style dispatch analysis
- `VM_FUNCTION_REGISTRATION.md` — Native function registration
- `VM_ROP_DISPATCHERS.md` — Anti-analysis chains
- `VM_STACK_OPERATIONS.md` — Stack manipulation
- `SCRIPT_DEBUGGER.md` — Remote debugger enable/disable
- **26 sub docs** in `vm/subs/` including:
  - `sub_52D9C0_VMInterpreter.md` — Main interpreter (2810 bytes, 64 opcodes)
  - `sub_52EA70_VMExecute.md` — Script execution entry
  - `sub_52EB40_VMCall.md` — Call VM function by name
  - `sub_52EC30_VMInitObject.md` — Object initialization
  - `sub_52C2A0_ScriptLoader.md` — SCT file loader

### Objects & Characters (`engine/objects/`)
- **32 sub docs** in `objects/subs/` including:
  - `sub_44C600_SpawnCharacter.md` — Character spawning
  - `sub_44C6C0_sauCreateCharacter.md` — VM create character
  - `sub_44C730_sauSpawnObj.md` — VM spawn object
  - `sub_463880_SetPlayerType.md` — Player hot-switch
  - `sub_4D6030_FactoryDispatch.md` — Object factory
  - `sub_55BDC0_GetAssetClassId.md` — Asset class resolution

### World & Levels (`engine/world/`)
- `WORLD_LOADING.md` — Level loading pipeline
- `SECTOR_CULLING.md` — Distance-based streaming
- `WORLD_GEOMETRY_INSTANCES.md` — **Multi-layer discovery, 487 instances**
- **11 sub docs** in `world/subs/` including:
  - `sub_48B460_LoadWorld.md` — World loading entry
  - `sub_488330_InitLevelSystem.md` — Level system init
  - `sub_4C9AC0_ClLoadLevelAction.md` — Level load action
  - `sub_5176E0_PrintSectorLoadInfo.md` — Sector load logging

### Data & Hash (`engine/data/`)
- `DATA_STORE.md` — Engine data store system
- `sub_41E830_HashLookup.md` — Hash-based lookup
- `sub_5392C0_GetValuePtr.md` — Value retrieval
- `sub_539630_BinarySearchByHash.md` — Binary search

### Events (`engine/events/`)
- `sub_488660_PostWorldLoadInit.md` — Post-load initialization
- `sub_52EA10_RegisterScriptFunction.md` — Script function registration
- `sub_52EBE0_DispatchEvent.md` — Event dispatch

### Debug & Cheats (`engine/debug/`)
- `sub_443160_CheckCheatFlag.md` — Cheat flag checking
- `sub_443EC0_CheatInputHandler.md` — Cheat input
- `sub_444140_CheatSystemInit.md` — Cheat initialization
- `sub_43C4E0_DebugCameraManager_CameraUpdate.md`
- `sub_440790_DebugCameraManager_InputHandler.md`
- `sub_440910_DebugCameraManager_Constructor.md`

### Fade System (`engine/fade/`)
- `sub_55DAA0_SubmitFadeRequest.md`
- `sub_55DB30_CreateFadeRequest.md`
- `sub_59DA70_IsFadeSlotAvailable.md`

### Time System (`engine/time/`)
- `sub_497DD0_sauSetClockSpeed.md`
- `sub_497FB0_sauPauseGame.md`
- `sub_498180_RegisterTimeScriptFunctions.md`
- `sub_4DF070_RemoveClockEntry.md`
- `sub_4DF360_SetClockSpeed.md`

### Services (`engine/services/`)
- `sub_537D10_ServiceLookup.md`

### Sectors (`engine/sectors/`)
- `sub_57E050_AnySectorLoaded.md`
- `sub_57E0B0_IsSectorThrottling.md`
- `sub_57E0D0_IsSectorLoaded.md`
- `sub_57E0F0_IsSectorUnloading.md`

### File Formats (`engine/formats/`)
- `ZWD_FORMAT.md` — SFZC/AWAD archive format
- `PCW_FORMAT.md` — PCWB world format
- `ASSET_LOADING.md` — Full I/O pipeline

---

## IDA Database Summary

### addresses.h — 377 lines
All reversed addresses used by the ASI mod:
- Function addresses (fn_*)
- Global variable addresses (p*)
- Structure offsets (CAM_*, CHAR_*, PLAYER_*)
- Original bytes for hook targets

### Renamed Functions (IDA)
**VM (50+):** VMInterpreter, VMExecute, VMCall, VMCallWithArgs, VMResolveFunction, VMInitObject, VMReturnHandler, VMCallNativeCdecl/Method/Static/Virtual, VMCallObjMethod, VMStackPush, VMPopObject, VMPopObjRef, VMReturnInt/Bool/String/Float/ObjDirect/Obj, VMRegisterMethod/Function, VMLoadScript, VMStateInit, sauSetPosition, sauSetRotation, sauGetPosition, sauGetRotation, VMPopVec3, VMReturnVec3Fmt

**Camera (20+):** CameraSectorUpdate, CameraMatrixBuilder, CameraTick, CameraModeHandler, MainCameraComponent, GetCameraObject, SetProjection, BuildViewMatrix, Frustum_BuildProjectionMatrix, Frustum_BuildCorners, CameraRender_WorldToScreen, CameraRender_SetFovAndClip/NearClip/FarClip/ResetToDefaults, SetAspectRatioMode

**World (15+):** LoadWorld, InitLevelSystem, UnloadLevel, SetLoadFlag, PrintSectorLoadInfo, RegisterSectorDrawData, StreamingIO_CalcAddress, RenderDispatch, AddToRenderQueue, PerformRoomCulling

**Objects (25+):** SpawnCharacter, sauCreateCharacter, sauSpawnObj, SetPlayerType, GetPlayerCharacter, FindClosestCharacter, CreateCharacter_Internal, FactoryDispatch, ClPlayerObj_Factory/Constructor/IsPlayerControlled, ActivateBase, FullActivate, CharacterDeath, DeactivateAllPlayers, GetAssetClassId

**Other (20+):** HashString, HashAndLookup, SetClockSpeed, sauPauseGame, RegisterTimeScriptFunctions, CheckCheatFlag, CheatInputHandler, SfAssetRepository_Load, PCWB_ValidateMagic/ValidateAlignment, EulerToMatrix4x4, ApplyRotation

---

## Tools Created

| Tool | Purpose |
|------|---------|
| `tools/spiderwick_unpack.py` v3 | Full extractor: all formats, 15+ extensions, DDS/OBJ/JSON conversion |
| `tools/spiderwick_world_export.py` v5 | World OBJ exporter: props + transforms + vertex colors |
| `tools/match_prop_transforms.py` | WVP capture → PCRD transform matcher |
| `tools/sct_disasm.py` | SCT bytecode disassembler (needs function table fix) |
| `tools/spiderwick_glb.py` | Skinned mesh glTF exporter |
| `tools/asset_names.py` | Hash→name dictionary (143 WAD names) |
| `tools/pcwb_analyze.py` | PCWB structure analyzer |

---

## ASI Mod Hooks (10 total)

| # | Hook | Address | Purpose |
|---|------|---------|---------|
| 1 | EndScene | vtable[42] | ImGui, hotkeys, freecam |
| 2 | CameraSectorUpdate | 0x488DD0 | Hot-switch, camera, VP capture, auto-load |
| 3 | DrawIndexedPrimitive | vtable[82] | Render overrides, WVP capture (5-frame) |
| 4 | Reset | vtable[16] | ImGui device invalidation |
| 5 | RegCreateKeyExW | advapi32 | Registry → settings.ini |
| 6 | RegSetValueExW | advapi32 | Registry → settings.ini |
| 7 | HashString | 0x405380 | Runtime name capture |
| 8 | sauSetPosition | 0x52EE50 | VM prop placement (obj+0x68) |
| 9 | sauSetRotation | 0x52EE80 | VM prop rotation (obj+0x78) |
| 10 | GeomInstance_Init | 0x5851D0 | World chunk transform capture (PCRD index + 4x4 matrix) |

## DLL_PROCESS_ATTACH Actions
- GeomInstance_Init hook (must be before level loads)
- Launch.txt patch (overwrites "Config.txt" at 0x63992C)
- Registry pre-populate from settings.ini

## Developer Config System (Launch.txt)
**Works on ALL retail copies — no mods needed for Config.txt!**
50 keys found. See `engine/debug/DEV_CONFIG.md` for full list.
Key entries: LEVEL, SKIP_INTRO_MOVIES, ENABLE_DEV_CHEATS, MODE, VIEWER, ALLOW_SKIP_ANY_IGC

## Scene Dump System (DUMP SCENE button)
Single-file dump: geometry instances (PCRD index + world matrix) + VM placements (name + pos + rot) + sector info. Used by Python exporter for world reconstruction.
