#pragma once
// ============================================================================
// Spiderwick Chronicles — Reversed Addresses
// Engine: Stormfront Studios proprietary (2008), DirectX 9, Kallis VM
// Base: 0x00400000 (no ASLR, 32-bit)
// ============================================================================

#include <cstdint>

namespace addr {

// ----------------------------------------------------------------------------
// Camera Object
// ----------------------------------------------------------------------------
// camera_obj = **(DWORD**)(0x72F670)  → main camera object
constexpr uintptr_t pCameraObj          = 0x0072F670;  // ptr to camera_obj

// camera_obj field offsets
constexpr uint32_t CAM_POS_X            = 0x6B8;       // float, world position X
constexpr uint32_t CAM_POS_Y            = 0x6BC;       // float, world position Y
constexpr uint32_t CAM_POS_Z            = 0x6C0;       // float, world position Z
constexpr uint32_t CAM_SECTOR           = 0x788;       // DWORD, current sector index
constexpr uint32_t CAM_FLAGS            = 0x78C;       // DWORD, bit 0x20 = sector update needed
constexpr uint32_t CAM_MATRIX_WORK      = 0x790;       // 4x4 float matrix (working)
constexpr uint32_t CAM_MATRIX_WORK2     = 0x7D0;       // 4x4 float matrix (working 2)
constexpr uint32_t CAM_MATRIX_COMMIT    = 0x834;       // 4x4 float matrix (committed)
constexpr uint32_t CAM_MATRIX_PREV      = 0x874;       // 4x4 float matrix (previous)

// ----------------------------------------------------------------------------
// Camera Struct (pCamStruct — view/projection data)
// ----------------------------------------------------------------------------
// pCamStruct is a pointer to the camera rendering struct
// Position within struct:
constexpr uint32_t CAMSTRUCT_POS_X      = 0x3B8;       // float
constexpr uint32_t CAMSTRUCT_POS_Y      = 0x3BC;       // float
constexpr uint32_t CAMSTRUCT_POS_Z      = 0x3C0;       // float

// ----------------------------------------------------------------------------
// Mouse Input (static addresses, written by vtable[7] sub_43DC10)
// ----------------------------------------------------------------------------
constexpr uintptr_t MOUSE_DX            = 0x0072FC14;  // float, yaw delta
constexpr uintptr_t MOUSE_DY            = 0x0072FC10;  // float, pitch delta

// ----------------------------------------------------------------------------
// World / Sector System
// ----------------------------------------------------------------------------
constexpr uintptr_t pCurrentWorldName   = 0x00E57F68;  // char[], save/chapter world name
constexpr uintptr_t pCurrentLevelName   = 0x006E4785;  // char[32], active level name (printed on unload)
constexpr uintptr_t pLevelLoadFlag      = 0x006E47D4;  // byte, set to 1 before loading
constexpr uintptr_t pWorldState         = 0x00E416C4;  // g_WorldState ptr
constexpr uintptr_t pSectorCount        = 0x00E416C8;  // DWORD, sector count (14 indoors)
constexpr uintptr_t pVisibilityArray    = 0x00E416CC;  // ptr to vis array (8 bytes/sector)
// g_WorldState offsets:
//   +0x64: ptr to sector data pointer array
//   +0x68: portal base address (328 bytes/portal)
// Sector data offsets:
//   +0x10: AABB min (vec4: X,Y,Z,1.0)
//   +0x20: AABB max (vec4: X,Y,Z,1.0)
//   +0x5C: portal count
//   +0x68: portal index array ptr
//   +0x80: render_id

// Sector loading state array
constexpr uintptr_t pSectorStateArray   = 0x0133FEF8;  // g_SectorStateArray
// IsSectorLoaded: state[sector*12] == 3

// Sector loading bitmask (Layer 1)
constexpr uintptr_t pSectorLoadBitmask  = 0x01340080;

// ----------------------------------------------------------------------------
// Direct3D 9
// ----------------------------------------------------------------------------
constexpr uintptr_t pD3DDevice          = 0x00E36E8C;  // IDirect3DDevice9*

// ----------------------------------------------------------------------------
// HUD / Cheat Flags / Debug
// ----------------------------------------------------------------------------
constexpr uintptr_t pCheatFlagsArray    = 0x006E1494;  // ptr to per-player cheat bitmask array
// Bits: 1=invuln, 2=hideHUD, 4=gameStateInfo, 5=combat, 6=heal, 11=ammo, 13=fieldGuide
constexpr uintptr_t pDevCheatsEnabled   = 0x006E1470;  // byte, 1=dev cheats unlocked (sprite/guard)

// Engine sector debug HUD (streaming overlay: memory bar, sector states, load times)
// Struct at 0x1345C28: { xPos, halfW, val, pos+3, screenW, halfW2, animTime, animState, enableFlag, opacity, ... }
constexpr uintptr_t pSectorDebugHUD     = 0x01345C48;  // DWORD, 0=off, 1/3=on
constexpr uintptr_t pSectorHUD_Opacity  = 0x01345C4C;  // float, must be >0 for visible (default 0.0!)
constexpr uintptr_t pSectorHUD_XPos     = 0x01345C28;  // int, X start position
constexpr uintptr_t pSectorHUD_ScreenW  = 0x01345C38;  // int, screen width (from dword_6ED394)
constexpr uintptr_t pSectorHUD_HalfH    = 0x01345C2C;  // int, half screen height
constexpr uintptr_t pSectorHUD_HalfH2   = 0x01345C3C;  // int, other half
constexpr uintptr_t pSectorHUD_YBase    = 0x01345C34;  // int, Y text start = halfH + 3
constexpr uintptr_t pSectorHUD_Val30    = 0x01345C30;  // int, from dword_D6F318
// Source globals for HUD dimensions (set by engine during graphics init)
constexpr uintptr_t pScreenWidth_6ED394 = 0x006ED394;  // screen width
constexpr uintptr_t pScreenHeight_6ED414= 0x006ED414;  // screen height
constexpr uintptr_t pHUDVal_D6F318      = 0x00D6F318;  // used for HUD positioning

// Engine debug console ring buffer (256 lines x 256 chars, color-coded {g} {r} {v} {o})
constexpr uintptr_t pConsoleBuffer      = 0x00D5CB98;  // char[256][256]
constexpr uintptr_t pConsoleWriteHead   = 0x00D6CDA4;  // int, current line index (0-255)
constexpr uintptr_t pConsoleDisplayStart= 0x00D6CD9C;  // int, display start line
constexpr uintptr_t pConsoleScrollPos   = 0x00D6CD98;  // int, scroll position

// ----------------------------------------------------------------------------
// Functions — Camera
// ----------------------------------------------------------------------------
// sub_5299A0: SetViewMatrix(eye*, target*) — Hook 1 target
constexpr uintptr_t fn_SetViewMatrix    = 0x005299A0;
constexpr uint8_t   fn_SetViewMatrix_orig[] = { 0x83, 0xEC, 0x10, 0xD9, 0xEE }; // 5 bytes

// sub_4356F0: CopyPositionBlock(src) — Hook 2 target (__thiscall, ecx=dest)
constexpr uintptr_t fn_CopyPositionBlock = 0x004356F0;
constexpr uint8_t   fn_CopyPositionBlock_orig[] = { 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C }; // 6 bytes

// sub_43DA50: MonocleUpdate(this, a2*, a3*, a4) — Hook 3 target
// __thiscall, returns char (0=normal cam, 1=handled), retn 0Ch
constexpr uintptr_t fn_MonocleUpdate    = 0x0043DA50;
constexpr uint8_t   fn_MonocleUpdate_orig[] = { 0x83, 0xEC, 0x60, 0x53, 0x56 }; // 5 bytes

// sub_439410: CameraTick (VM function, native block at +9)
constexpr uintptr_t fn_CameraTick       = 0x00439410;
// Native block: 0x439419 (test al, 0x20; jz skip)
// Position stamp: 0x439449 (fld [edi+68h]; fstp [esi+864h]; ...)
// Player sector write: 0x439464 (mov eax,[edi+24h]; mov [esi+788h],eax)

// sub_43E2B0: MainCameraComponent — calls MonocleUpdate, then normal camera
constexpr uintptr_t fn_MainCameraComponent = 0x0043E2B0;

// sub_44F890: GetPlayerCharacter — returns player object ptr
constexpr uintptr_t fn_GetPlayerCharacter = 0x0044F890;

// ----------------------------------------------------------------------------
// Functions — World Loading
// ----------------------------------------------------------------------------
// sub_488640: SetLoadFlag(flag) — __cdecl, sets byte_6E47D4
constexpr uintptr_t fn_SetLoadFlag      = 0x00488640;

// sub_48AF00: SetSpawnPoint(name) — __stdcall, VM thunk (sets spawn name e.g. "nf01")
constexpr uintptr_t fn_SetSpawnPoint    = 0x0048AF00;

// sub_48B460: LoadWorld(worldName, spawnId) — __cdecl, VM thunk
// Triggers full world transition (unload current → load new)
// worldName: "MansionD", "GroundsD", etc. (matches .zwd filenames)
// spawnId: 0 = default, or index from save data
constexpr uintptr_t fn_LoadWorld        = 0x0048B460;

// sub_538080: SetStorageFlag — sets dword_7133B8 |= 1 (called before new game load)
constexpr uintptr_t fn_SetStorageFlag   = 0x00538080;

// ----------------------------------------------------------------------------
// Functions — Clock / Time
// ----------------------------------------------------------------------------
// sub_4DF360: SetClockSpeed(speed) — __cdecl, clamps 0.0-1.0, ecx=dword_D57810
// Returns clock entry ID (store in dword_D42D6C for removal)
constexpr uintptr_t fn_SetClockSpeed    = 0x004DF360;

// sub_4DF070: RemoveClockEntry(entryId) — __cdecl
constexpr uintptr_t fn_RemoveClockEntry = 0x004DF070;

// dword_D42D6C: current clock entry ID (for sauSetClockSpeed)
constexpr uintptr_t pClockEntryId       = 0x00D42D6C;

// dword_D57810: clock system "this" object (used by SetClockSpeed internally)
constexpr uintptr_t pClockSystem        = 0x00D57810;

// ----------------------------------------------------------------------------
// Functions — Fade
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// VM Runtime
// ----------------------------------------------------------------------------
constexpr uintptr_t pVMStackBase        = 0x00E56168;  // VM arg stack base
constexpr uintptr_t pVMStackIndex       = 0x00E56200;  // VM arg stack index (pop: base - 4*idx--)
constexpr uintptr_t pVMReturnStack      = 0x00E5616C;  // VM return stack pointer (push: *ptr = val; ptr += 4)
constexpr uintptr_t pVMObjBase          = 0x00E56160;  // VM object reference base

// Script debugger: *(dword_E561D8 + 0x2E) bit 0 = enable
constexpr uintptr_t pScriptState        = 0x00E561D8;  // ptr to script system state object
constexpr uint32_t  SCRIPT_DEBUG_OFFSET  = 0x2E;       // byte offset for debugger flag (bit 0)

// Character creation & spawn pipeline
constexpr uintptr_t fn_CreateCharacter  = 0x0044FA20;  // CreateCharacter_Internal(assetRef)
constexpr uintptr_t fn_SpawnCharacter   = 0x0044C600;  // SpawnCharacter(this=spawner, vmRef, pos, rot)
constexpr uintptr_t fn_SpawnObjHandler  = 0x0044C730;  // sauSpawnObj native handler
constexpr uintptr_t pCharacterPool      = 0x00730798;  // current character slot ptr (must be non-null)

// Spawn — activation & render registration
constexpr uintptr_t fn_CreateRenderComps = 0x004510E0; // __thiscall, creates render comps from mesh at +0x1B8
constexpr uintptr_t fn_ActivateBase      = 0x004546A0; // __thiscall, sector reg + render pos + scene graph
constexpr uintptr_t fn_PlayerActivate    = 0x00462250; // __thiscall, = ActivateBase + camera dirty flag
constexpr uintptr_t fn_RenderObjSetup    = 0x004584C0; // __thiscall(this, widget), bounds/collision render obj
constexpr uintptr_t fn_FullActivate      = 0x00458B30; // __thiscall, full sauActivateObj (VM + render + sector)
constexpr uintptr_t fn_CameraDirtyFlag   = 0x00436910; // sets bit 0x20 on g_pCameraSystem+1932
constexpr uintptr_t fn_EntityRegister    = 0x0051B220; // __thiscall, creates scene graph nodes at +0x9C/+0xA0
constexpr uintptr_t fn_GamePoolAlloc     = 0x004DE530; // __cdecl(size, align), game pool allocator
constexpr uintptr_t pAllocDisabled       = 0x00D577F4; // byte, 1=allocation blocked, 0=enabled

// "MissionStart" event string in .rdata — dispatched after every world load
// Change first byte 'M'→'X' to prevent scripts from matching the event
constexpr uintptr_t pMissionStartString = 0x0062F4C0;

// sub_41E830: HashLookup(name) — __cdecl, hashes var name → binary search → returns index or -1
constexpr uintptr_t fn_HashLookup       = 0x0041E830;

// sub_5392C0: GetValuePtr(this, idx) — __thiscall, returns int* to variable value
// this = data store object at 0xE57F68
constexpr uintptr_t fn_GetValuePtr      = 0x005392C0;
constexpr uintptr_t pDataStore          = 0x00E57F68;  // data store "this" ptr for GetValuePtr

// ----------------------------------------------------------------------------
// Functions — Sector / Portal Traversal
// ----------------------------------------------------------------------------
// sub_4391D0: PlayerSectorTransition (reads +0x790/+0x7D0, NOT +0x834!)
constexpr uintptr_t fn_SectorTransition = 0x004391D0;

// sub_519700: RaycastFindSector — portal-based, incremental only
constexpr uintptr_t fn_RaycastFindSector = 0x00519700;

// sub_51A130: PortalTraversal_Native — recursive portal traversal (2232 bytes)
constexpr uintptr_t fn_PortalTraversal  = 0x0051A130;

// sub_517E50: PortalDistancePreTest
constexpr uintptr_t fn_PortalDistPreTest = 0x00517E50;

// sub_57E0D0: IsSectorLoaded — returns g_SectorStateArray[sector*12] == 3
constexpr uintptr_t fn_IsSectorLoaded   = 0x0057E0D0;
constexpr uint8_t   fn_IsSectorLoaded_orig[] = { 0x8B, 0x44, 0x24, 0x04, 0x8D, 0x04, 0x40 }; // 7 bytes

// ----------------------------------------------------------------------------
// Functions — Render Pipeline
// ----------------------------------------------------------------------------
// sub_488DD0: CameraSectorUpdate — main per-frame update entry
constexpr uintptr_t fn_CameraSectorUpdate = 0x00488DD0;
constexpr uint8_t   fn_CameraSectorUpdate_orig[] = { 0x56, 0xE8 }; // push esi; call ...

// sub_488970: SectorVisibilityUpdate2 — portal traversal + render pipeline
constexpr uintptr_t fn_SectorVisUpdate2 = 0x00488970;

// sub_1C93F90: SectorRenderSubmit — .kallis native, __usercall
constexpr uintptr_t fn_SectorRenderSubmit = 0x01C93F90;

// sub_562760: RenderDispatch — processes render queue
constexpr uintptr_t fn_RenderDispatch   = 0x00562760;

// sub_562D30: AddToRenderQueue
constexpr uintptr_t fn_AddToRenderQueue = 0x00562D30;

// sub_562650: FrustumSetup_VM (thunk)
constexpr uintptr_t fn_FrustumSetup_VM  = 0x00562650;

// 0x561A20: VM render submit (unnamed) — max 5 entries cap
constexpr uintptr_t fn_VMRenderSubmit   = 0x00561A20;
constexpr uintptr_t fn_VMRenderSubmit_cap = 0x00561A28; // byte: cmp ecx, 5

// sub_5AC250: SutherlandHodgmanClip — portal frustum clip (6 planes)
// Frustum planes at this+0x140, 6 planes × 16 bytes
constexpr uintptr_t fn_SHClip          = 0x005AC250;
constexpr uint8_t   fn_SHClip_orig[]   = { 0x83, 0xEC, 0x18, 0x8B, 0x44, 0x24, 0x1C }; // 7 bytes

// sub_5ABF30: Per-object frustum visibility test (returns 0/1/2)
constexpr uintptr_t fn_ObjVisTest      = 0x005ABF30;

// sub_556300: Scene graph traversal — calls FrustumSetup_VM + ObjVisTest per object
constexpr uintptr_t fn_SceneGraphTraversal = 0x00556300;

// ----------------------------------------------------------------------------
// Character System (ClCharacterObj / ClPlayerObj)
// ----------------------------------------------------------------------------
// Linked list: g_CharacterListHead → char_obj+0x5A0 → next → ...
constexpr uintptr_t pCharacterListHead     = 0x007307D8;  // g_CharacterListHead

// Character object field offsets
constexpr uint32_t CHAR_POS_X              = 0x68;        // float, world X
constexpr uint32_t CHAR_POS_Y              = 0x6C;        // float, world Y
constexpr uint32_t CHAR_POS_Z              = 0x70;        // float, world Z
constexpr uint32_t CHAR_VM_LINK            = 0xA8;        // uintptr_t, VM object reference
constexpr uint32_t CHAR_SCENE_GFX_1        = 0x9C;        // uintptr_t, scene graph node 1
constexpr uint32_t CHAR_SCENE_GFX_2        = 0xA0;        // uintptr_t, scene graph node 2
constexpr uint32_t CHAR_ACTIVATION_NODE    = 0x138;       // uintptr_t, activation scene node
constexpr uint32_t CHAR_RENDER_OBJ         = 0x13C;       // uintptr_t, render bounds object
constexpr uint32_t CHAR_SCENE_NODE         = 0x1B8;       // uintptr_t, sector/mesh data (read-only template)
constexpr uint32_t CHAR_FLAGS              = 0x1CC;       // DWORD, bit 2 = is player character
constexpr uint32_t CHAR_WIDGET_PTR         = 0x1C0;       // ptr to widget/entity descriptor
constexpr uint32_t CHAR_RENDER_COUNT       = 0x4D8;       // uint32_t, render component count (max 3)
constexpr uint32_t CHAR_RENDER_COMP_0      = 0x4DC;       // uintptr_t, render component 0
constexpr uint32_t CHAR_RENDER_COMP_1      = 0x4E0;       // uintptr_t, render component 1
constexpr uint32_t CHAR_RENDER_COMP_2      = 0x4E4;       // uintptr_t, render component 2
constexpr uint32_t CHAR_NEXT               = 0x5A0;       // ptr to next character in linked list

// Widget/entity descriptor offsets
constexpr uint32_t WIDGET_NAME_HASH        = 0x24;        // DWORD, HashString(name) result

// Character vtable function indices (multiply by 4 for offset)
constexpr uint32_t VTBL_IS_PLAYER          = 19;          // vtable[19] = IsPlayerCharacter() → bool
constexpr uint32_t VTBL_IS_PLAYABLE        = 20;          // vtable[20] = IsPlayableCharacter() → bool

// sub_405380: HashString(name) — __cdecl, engine's name hashing function
constexpr uintptr_t fn_HashString          = 0x00405380;

// sub_458170: sauCharacterInit — VM native, pops (texB, texA, template) strings
// Hashes texA/texB → PCIM nameHash, calls vtable[18] to create character
constexpr uintptr_t fn_sauCharacterInit    = 0x00458170;

// sub_52D7D0: VMPopString — pops string ref from VM stack, resolves via string table
constexpr uintptr_t fn_VMPopString         = 0x0052D7D0;

// sub_527EC0: ClNoamActor_Init — copies descriptor array, creates ModelInstance + NoamFigure
// a2 = DWORD[22] descriptor: [1]=NM40 asset, [13]=texA hash, [14]=texB hash
constexpr uintptr_t fn_ClNoamActorInit     = 0x00527EC0;

// sub_56DCF0: PCIM_Factory — creates texture asset object from raw PCIM data
// __cdecl(allocator, pcimDataPtr) → asset object {vtable, dataPtr}
constexpr uintptr_t fn_PCIMFactory         = 0x0056DCF0;

// sub_56D940: NM40_Factory — creates mesh asset object from raw NM40 data
constexpr uintptr_t fn_NM40Factory         = 0x0056D940;

// sub_56E970: NoamFigure_Init — creates rendering instance, stores texA/texB
constexpr uintptr_t fn_NoamFigureInit      = 0x0056E970;

// sub_571000: RENDERFUNC_NOAM_DIFFUSE — main entry for NM40 character rendering
// __cdecl(renderCmd*). renderCmd[5]=renderCtx, *(renderCtx+4)=obj with textures
constexpr uintptr_t fn_RenderFuncNoamDiffuse = 0x00571000;

// sub_4E9970: D3D_BindTexture — wrapper for IDirect3DDevice9::SetTexture
// Sets texture + sampler states. a6=texture info obj, *(a6+28)=IDirect3DTexture9*
constexpr uintptr_t fn_D3DBindTexture       = 0x004E9970;

// sub_52EE50: sauSetPosition — VM handler, pops vec3 from VM stack, calls vtable[1](this, &pos)
// Used by ALL objects: ClWorldPropObj, ClStaticPropObj, ClCharacterObj, etc.
constexpr uintptr_t fn_sauSetPosition      = 0x0052EE50;
constexpr uintptr_t fn_sauSetRotation      = 0x0052EE80;

// sub_52C770: VMPopVec3 — pops 3 floats from VM stack into float[4] buffer
constexpr uintptr_t fn_VMPopVec3           = 0x0052C770;

// sub_57DCC0: StreamingIO_CalcAddress(page_offset) — returns RAM address of loaded PCWB data
constexpr uintptr_t fn_StreamingCalcAddr   = 0x0057DCC0;

// sub_5851D0: GeomInstance_Init — called once per geometry instance during sector load
constexpr uintptr_t fn_GeomInstanceInit    = 0x005851D0;

// Level loading — additional functions (SetLoadFlag, LoadWorld, SetStorageFlag defined above)
constexpr uintptr_t fn_SetNarrativeFile    = 0x0048AF00;  // int __stdcall(const char*) — set spawn point
constexpr uintptr_t fn_StartNewGame        = 0x004DC090;  // int() — full new game sequence
constexpr uintptr_t fn_SetGameState        = 0x004897F0;  // int(int state) — game state machine
constexpr uintptr_t pGameState             = 0x0073EF30;  // current game state (1-8)

// sub_41E830: HashAndLookup(name) — __cdecl, hashes name + binary searches data store
constexpr uintptr_t fn_HashAndLookup       = 0x0041E830;

// sub_493A80: SwitchPlayerCharacter — VM thunk, pops type from VM stack
// 1=Jared, 2=Mallory, 3=Simon, default=ThimbleTack
constexpr uintptr_t fn_SwitchCharacter     = 0x00493A80;

// Character widget globals (set by SwitchPlayerCharacter on first call)
constexpr uintptr_t pJaredWidget           = 0x00D42CFC;  // Jared widget ptr
constexpr uintptr_t pSimonWidget           = 0x00D42D04;  // Simon widget ptr
constexpr uintptr_t pMalloryWidget         = 0x00D42D0C;  // Mallory widget ptr

// Aliases for backward compatibility
constexpr uint32_t PLAYER_POS_X            = CHAR_POS_X;
constexpr uint32_t PLAYER_POS_Y            = CHAR_POS_Y;
constexpr uint32_t PLAYER_POS_Z            = CHAR_POS_Z;

// ----------------------------------------------------------------------------
// Functions — Input / Debug
// ----------------------------------------------------------------------------
// sub_440910: DebugCam constructor (registers with "Debug" input group)
constexpr uintptr_t fn_DebugCamCtor    = 0x00440910;

// sub_438F40: Camera notify (called when camera near player)
constexpr uintptr_t fn_CameraNotify    = 0x00438F40;

// ----------------------------------------------------------------------------
// Render Queue Data
// ----------------------------------------------------------------------------
constexpr uintptr_t pRenderQueue       = 0x00ECC2B0;  // render queue base
constexpr uintptr_t pRenderQueueCount  = 0x00F80090;  // entry count
// Each entry: 7752 bytes (1938 DWORDs)

// VM render submit data arrays
constexpr uintptr_t pVMRenderEntries   = 0x00EBD058;  // 5 × 16-byte entries
constexpr uintptr_t pVMRenderPtrs      = 0x00EBD064;  // entry pointers
constexpr uintptr_t pVMRenderCount     = 0x00EBD0A8;  // entry count
constexpr uintptr_t pVMRenderData      = 0x00EBD0B0;  // 5 × 64-byte data blocks

// ----------------------------------------------------------------------------
// Render Feature Globals (set by VM sau handlers, read by native render code)
// ----------------------------------------------------------------------------
// Fog system — software fog via LUT texture, NOT hardware D3D fog
constexpr uintptr_t pFogEnable          = 0x00E794F9;  // byte, 0/1
constexpr uintptr_t pFogDensity         = 0x00E794A0;  // float
constexpr uintptr_t pFogStart           = 0x00E794A8;  // float
constexpr uintptr_t pFogColorR          = 0x00E794D8;  // DWORD (0-255)
constexpr uintptr_t pFogColorG          = 0x00E794AC;  // DWORD (0-255)
constexpr uintptr_t pFogColorB          = 0x00E79498;  // DWORD (0-255)
constexpr uintptr_t pFogType            = 0x00E794B0;  // DWORD (0=linear,1=exp,2=exp2,3=custom)

// Snow particle system
constexpr uintptr_t pSnowEnable         = 0x00726F78;  // byte, 0/1

// Rim light system
constexpr uintptr_t pRimLightEnable     = 0x00728BA0;  // byte, 0/1
constexpr uintptr_t pRimLightModulation = 0x00728BA1;  // byte, 0/1
constexpr uintptr_t pRimLightZOffset    = 0x00728BA4;  // float
constexpr uintptr_t pRimLightXOffset    = 0x01345648;  // float
constexpr uintptr_t pRimLightColorR     = 0x00728BA8;  // DWORD
constexpr uintptr_t pRimLightColorG     = 0x00728BAC;  // DWORD
constexpr uintptr_t pRimLightColorB     = 0x00728BB0;  // DWORD

// Ambient/sun light (static camera pipeline, but readable)
constexpr uintptr_t pAmbientIntensity   = 0x006E55A0;  // float (0.0-1.0)
constexpr uintptr_t pSunIntensity       = 0x006E55A4;  // float (0.0-1.0)

} // namespace addr
