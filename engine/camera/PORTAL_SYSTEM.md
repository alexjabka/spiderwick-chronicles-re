# Portal Rendering System — Definitive Reference
**Status:** SOLVED (Freecam v20) — full pipeline reversed, sector tracking via AABB lookup

---

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [Main Frame Render: CameraSectorUpdate](#main-frame-render-camerasectorupdate-0x488dd0)
3. [Portal Traversal Pipeline: SectorVisibilityUpdate2](#portal-traversal-pipeline-sectorvisibilityupdate2-0x488970)
4. [Sector Render Submit: SectorRenderSubmit](#sector-render-submit-sectorrendersubmit-0x1c93f90)
5. [Sector Detection: PlayerSectorTransition](#sector-detection-playersectortransition-0x4391d0)
6. [Portal-Based Raycasting: RaycastFindSector](#portal-based-raycasting-raycastfindsector-0x519700)
7. [Camera Components: MonocleUpdate and MainCameraComponent](#camera-components)
8. [Native Portal Traversal: PortalTraversal_Native](#native-portal-traversal-sub_51a130)
9. [VM Render Path](#vm-render-path-0x561a20)
10. [Render Queue System](#render-queue-system)
11. [Key Data Structures](#key-data-structures)
12. [Camera Object Layout](#camera-object-layout)
13. [Kallis VM Details](#kallis-vm-details)
14. [Freecam v20 Solution](#freecam-v20-solution)
15. [Function Reference](#function-reference)
16. [Global Data](#global-data)
17. [Related Files](#related-files)

---

## Architecture Overview

The Spiderwick engine uses a **portal-based sector rendering system**. The world is divided into sectors (rooms), connected by portals (doorways/openings). Each frame, the engine:

1. **Determines which sector the camera occupies** (via AABB or portal raycast)
2. **Traverses portals recursively** from that sector to find visible neighbors
3. **Submits visible sectors** to a render queue for D3D drawing

The system is orchestrated by the **Kallis VM** (a custom bytecode interpreter), which calls into **native x86** functions for the heavy math (polygon clipping, frustum tests, recursion). Despite the VM entry points, all critical portal logic is native code.

---

## Main Frame Render: CameraSectorUpdate (0x488DD0)

The top-level per-frame render function. Called every frame during gameplay.

```c
void CameraSectorUpdate(float dt) {
    CameraObject = GetCameraObject();
    UpdateVisibility(camera_obj+0x788, camera_obj, dt);  // room culling
    SectorVisibilityUpdate2(camera_obj+0x788, camera_obj, dt);  // portal traversal + render
    RenderDispatch();  // process render queue
    sub_55FFB0();
    sub_552420();
}
```

**Flow:**
- `UpdateVisibility` (0x488B30) gates `PerformRoomCulling` — object-level, distance-based culling
- `SectorVisibilityUpdate2` (0x488970) runs the full portal traversal pipeline
- `RenderDispatch` (0x562760) processes the render queue, dispatches D3D draw calls

---

## Portal Traversal Pipeline: SectorVisibilityUpdate2 (0x488970)

The core per-frame pipeline that sets up frustum, traverses portals, and submits sectors for rendering.

```c
void SectorVisibilityUpdate2(int sector, int camera_obj, float dt) {
    SetupViewport(1);                              // 0x4E9670 - D3D viewport
    FrustumSetup_VM(camera_obj, 1);                // 0x562650 - compute frustum planes
    UpdateSectorAnims(camera_obj, dt);             // 0x55DF50
    UpdateFadeEffect(dt);                          // 0x5620D0
    UpdateRoomTimer(dt);                           // 0x561DF0
    PreTraversal_VM();                             // 0x561B60
    SetSectorState_VM(sector);                     // 0x516900 - set starting sector
    PortalTraversal_VM(sector, camera_obj, 0);     // 0x51ABE0 -> 0x51A130 native
    PostTraversal_VM(&off_7147C0);                 // 0x5564D0 - calls SectorRenderSubmit via VM
    // ambient color setup...
    CheckVisMode_VM();                             // 0x517050
    VisPostProcess_VM(camera_obj);                 // 0x5193E0
    NotifyObservers(camera_obj);
    FinalizeRender_VM(camera_obj);                 // 0x519540
}
```

**Key insight:** `PostTraversal_VM` is what triggers `SectorRenderSubmit` (0x1C93F90) to iterate the visibility array and submit visible sectors. This call is the bridge between portal traversal results and actual geometry rendering.

---

## Sector Render Submit: SectorRenderSubmit (0x1C93F90)

The function that reads traversal results and submits sector geometry to the render queue. Lives in the `.kallis` section as native code callable by the VM.

**Calling convention:** `__usercall` — args in `ebx`, `ebp`, `esi` + 2 stack args

**Behavior:**
- Iterates the visibility array (`dword_E416CC`), 8 bytes per sector entry:
  - `byte[0]` = visibility flag
  - `byte[1]` = side/facing
  - `dword[4]` = frustum ID
- For each visible sector: calls `FrustumSetup_VM` and **returns**
- The VM calls this function **repeatedly** (once per visible sector) until all are processed
- Tracks processed sectors via internal arrays to avoid re-processing
- **NOT called in monocle mode** — the VM takes a different path entirely

---

## Sector Detection: PlayerSectorTransition (0x4391D0)

Determines which sector the camera is in by raycasting through portals.

```c
void PlayerSectorTransition(int *this) {
    // Decomposes FOUR matrices: +0x834, +0x874, +0x790, +0x7D0
    // Only +0x790 and +0x7D0 are used for the ray!
    // Builds ray from +0x7D0 position TO +0x790 position
    // These are VIEW MATRICES - translation is NOT world-space position
    RaycastFindSector(current_sector, ray_data, &hit_info, output);
    if (hit && new_sector >= 0) {
        if (new_sector != current_sector)
            print("Sector Changed: was %d now %d\n");
        this->sector = new_sector;
    }
}
```

**Critical detail:** The matrices at `+0x790` and `+0x7D0` are **view matrices** — their translation components are NOT world-space positions. This is why directly writing world coordinates to `+0x864` (which is `+0x834+0x30`, the translation row of the final matrix) did not work for freecam sector tracking.

---

## Portal-Based Raycasting: RaycastFindSector (0x519700)

The actual sector-finding logic called by `PlayerSectorTransition`.

- Takes the **current sector index** and iterates ONLY that sector's portals
- For each portal: performs ray-portal intersection test (`PortalRayIntersect`)
- Can **ONLY** find sectors directly connected via portals — **CANNOT handle jumps**
- Portal data: `g_WorldState+0x68` base, 328 bytes per portal

**Limitation:** This is fundamentally incremental — if the camera teleports across multiple sectors (as freecam does), it cannot find the correct sector. This is why AABB lookup was needed for freecam.

---

## Camera Components

### MonocleUpdate (0x43DA50) — Camera Mode Handler

```
__thiscall, returns char (0 or 1)
```

Three code paths based on flags:
- **Init path:** `this+0xBB == 1`
- **Rotate path:** `this+0xB9 == 1`
- **Copy path:** `this+0xB8 == 1 && this+0xB9 == 1`

**Return value semantics:**
- Return `1` = "handled" — `MainCameraComponent` skips all normal camera logic
- Return `0` = not handled — normal camera runs

**Side effect of monocle flags:** Setting monocle flags causes `camera_obj+0x78C` to be zeroed, which makes the VM skip the rendering path entirely. This is why the freecam solution hooks MonocleUpdate to return 1 **without** setting monocle flags.

### MainCameraComponent (0x43E2B0) — Camera Update

```c
void MainCameraComponent(this, a2, a3) {
    // ... player position tracking, elevation ...
    if (!MonocleUpdate(&pos, a3, a2)) {
        // Normal camera: mouse rotation, follow player, collision
        // Reads mouse input from flt_72FC10/72FC14
        // Uses INP_CAMERA_BEHIND_PLAYER input
    }
    // If MonocleUpdate returns 1 -> all normal camera logic skipped
}
```

---

## Native Portal Traversal: sub_51A130

**Address:** `0x51A130` (Spiderwick.exe+1A130)
**Size:** 2232 bytes (0x8B8)
**Called from:** Kallis VM via `PortalTraversal_VM` thunk (0x51ABE0)

The core recursive portal walk. Despite the VM entry point, all the heavy math is native x86.

### Per-Portal Decision Flow

```
For each portal in current sector:
|
+-- sub_517E50 (PortalDistancePreTest)
|   Tests if portal's closest point to camera is within range.
|   Sets arg_120 = 1 if in range, 0 if out of range.
|
+-- [0x51A3AD] cmp arg_120, 0
+-- [0x51A3B5] jnz loc_51A8CC          * PATCH POINT (jnz -> jmp)
|              If pre-test passed     |
|              -> skip clip test      |
|                                     |
+-- [0x51A3BB] test [esi+4], 2        |
+-- [0x51A3BF] jz loc_51A8CC          |
|              If flag bit clear      |
|              -> skip clip test      |
|                                     |
+-- [0x51A47A] call sub_5299E0        |  Frustum clip test
|              (ClipPortalPolygon)    |  Sutherland-Hodgman
|              Returns vertex count   |
|                                     |
+-- [0x51A592] jle loc_51A9C5         |  If 0 vertices -> skip portal
|              (portal not visible)   |
|                                     |
+-- [0x51A782] dot product face test  |  Front/back face check
|                                     |
+-- Recursive call to sub_51A130 <----+
|   for connected sector
|
+-- [0x51A8CC] LABEL_65: <-------------- All portals land here with patch
    mov ebp, [esi+8]                     Load connected sector
    ...process portal...                Recursive traversal
```

The recursion is limited by a "visited sectors" check that prevents infinite loops.

---

## VM Render Path (0x561A20)

Unnamed native function called by the VM for each visible sector.

- Called via VM callback, up to 5 entries tracked via `dword_EBD0A8`
- Calls `FrustumSetup_VM` + `AddToRenderQueue`
- Uses separate data arrays: `EBD058`, `EBD064`, `EBD0A8`, `EBD0B0`

---

## Render Queue System

| Component | Address | Details |
|-----------|---------|---------|
| Queue base | `dword_ECC2B0` | Each entry 7752 bytes (1938 DWORDs) |
| Entry count | `dword_F80090` | Current number of entries |
| AddToRenderQueue | `0x562D30` | Adds entries to the queue |
| RenderDispatch | `0x562760` | Processes queue, calls D3D draw |

---

## Key Data Structures

### World/Sector System

| Symbol | Address | Description |
|--------|---------|-------------|
| **g_WorldState** | `0xE416C4` | World/sector system pointer |
| Sector count | `dword_E416C8` | Number of sectors (14 for indoor levels) |
| Visibility array | `dword_E416CC` | 8 bytes/sector (see SectorRenderSubmit) |
| Sector data array | `g_WorldState+0x64` | DWORD pointer array, one per sector |
| Portal base | `g_WorldState+0x68` | 328 bytes per portal |

### Per-Sector Data

| Offset | Type | Field |
|--------|------|-------|
| `+0x00` | char[] | **Sector name** (null-terminated string, e.g. "Foyer") |
| `+0x10` | vec4 (float x4) | AABB min (XYZ + W=1.0) |
| `+0x20` | vec4 (float x4) | AABB max (XYZ + W=1.0) |
| `+0x58` | int | Hanging edge count |
| `+0x5C` | int | Portal count for this sector |
| `+0x68` | int* | Portal index array ptr |
| `+0x78` | int* -> int | Chunk count (ptr dereference) |
| `+0x80` | int | Render ID |
| `+0xAC` | int | Geometry instance count |

### Visibility Array Entry (8 bytes)

| Offset | Type | Field |
|--------|------|-------|
| `byte[0]` | byte | Visibility flag (1=visible) |
| `byte[1]` | byte | Side/facing |
| `dword[4]` | dword | Frustum ID |

---

## Camera Object Layout

Sector camera singleton at `[0x0072F670]`:

| Offset | Type | Field | Used By |
|--------|------|-------|---------|
| +0x124 | float | **FOV** (radians) | SetProjection, FrustumSetup_VM |
| +0x128 | float | Aspect ratio | SetProjection |
| +0x12C | float | Near plane | SetProjection |
| +0x130 | float | Far plane | SetProjection |
| +0x40C | float | Direction component 1 | PerformRoomCulling (stride 0x10) |
| +0x41C | float | Direction component 2 | PerformRoomCulling (stride 0x10) |
| +0x42C | float | Direction component 3 | PerformRoomCulling (stride 0x10) |
| +0x568 | float | **View forward X** | Character rendering, misc |
| +0x56C | float | **View forward Y** | Character rendering, misc |
| +0x570 | float | **View forward Z** | Character rendering, misc |
| +0x6B8 | float | **Position X** | GetCameraPosition, portal traversal |
| +0x6BC | float | **Position Y** | GetCameraPosition, portal traversal |
| +0x6C0 | float | **Position Z** | GetCameraPosition, portal traversal |
| +0x6C8 | 12 bytes | Viewport data | SetupViewport (0x4E9670) |
| +0x788 | int | **Current sector index** | SectorTransition, all pipelines |
| +0x78C | dword | **Flags** | Bit 0x20 gates player pos stamp, bit 0x2 disables it |
| +0x790 | 64 bytes | **Working camera matrix** (4x4) | Source for CopyPositionBlock, ray endpoint for SectorTransition |
| +0x7D0 | 64 bytes | Working matrix 2 | Ray origin for SectorTransition |
| +0x834 | 64 bytes | **Final camera matrix** (4x4) | Dest of CopyPositionBlock |
| +0x864 | float | **Matrix translation X** (+0x834+0x30) | Written by PlayerPosStamp |
| +0x868 | float | **Matrix translation Y** | Written by PlayerPosStamp |
| +0x86C | float | **Matrix translation Z** | Written by PlayerPosStamp |
| +0x874 | 64 bytes | Final matrix 2 | Read by SectorTransition |

### CameraTick Native Block (0x439419-0x439512)

```asm
439419: test al, 20h           ; check flag bit 0x20
43941C: jz  0x43946D           ; SKIP when flags=0 (monocle mode)
439422: call GetPlayerCharacter ; returns NULL in monocle!
439449: player pos stamp        ; writes player XYZ to +0x864/868/86C
439464: mov [esi+788h], [edi+24h] ; write player sector
439473: and [esi+78Ch], ~0x20   ; CLEAR flag (one-shot)
439500: call SectorTransition   ; 0x4391D0
439512: call CopyPositionBlock  ; copies +0x790 -> +0x834
```

In monocle mode (`flags=0x0`), the entire block is skipped — no sector tracking, no position stamp.

---

## Kallis VM Details

Portal system entry points are Kallis VM bytecode thunks.

**Thunk pattern (6 bytes):**
```asm
sub_XXXXXX:
  push    offset bytecode_ptr    ; address of VM bytecode descriptor
  call    sub_13FA690            ; VM interpreter thunk -> off_1A561F4 -> 0x1E4B000
; embedded:
  dd      offset bytecode_data   ; inline bytecode address
```

**VM interpreter chain:** `sub_13FA690` -> `off_1A561F4` -> `0x1E4B000` (actual interpreter)
**Bytecode section:** `.kallis` (addresses `0x1C80000` - `0x210xxxx`)

Despite the VM entry points, the actual portal traversal (`sub_51A130`) is native x86 code **called from within the VM**. The VM handles orchestration while the heavy math (polygon clipping, frustum recursion) is done natively.

---

## Freecam v20 Solution

The definitive freecam approach that keeps all portal rendering working.

### Architecture (3 hooks + Lua)

| Component | Address | What It Does |
|-----------|---------|--------------|
| **Hook 1** | sub_5299A0 | Replaces eye/target for view matrix |
| **Hook 2** | sub_4356F0 | Blocks CopyPositionBlock to camera struct |
| **Hook 3** | sub_43DA50 (MonocleUpdate) | `mov al, 1; ret $0C` — returns "handled" without monocle flags |
| **Lua timer** | every tick | AABB sector lookup, writes correct sector to `camera_obj+0x788` |

### Why It Works

1. **No monocle flags set** — VM takes the **normal rendering path**, so the full portal traversal pipeline (including `SectorRenderSubmit`) is active
2. **Bit 0x20 NOT set** — CameraTick native block is skipped, so `PlayerSectorTransition` does not overwrite the sector with stale data
3. **Lua writes sector from AABB lookup** — portal traversal starts from the correct sector for the freecam position
4. **`SectorRenderSubmit` (0x1C93F90)** is called by the VM as normal — sectors are rendered through portals from the correct starting sector

### The Key Insight

The fundamental blocker was that **monocle mode** (the engine's built-in first-person view) causes the VM to skip the entire render submission path. Every approach that relied on monocle mode was doomed:
- `SectorRenderSubmit` is never called
- Visibility array forcing has no effect
- Portal bypasses have no target to act on

The solution: hook `MonocleUpdate` to **claim the camera is handled** (return 1) **without** actually entering monocle mode. The engine stays on the normal rendering path, and Lua handles sector tracking via simple AABB containment testing.

### AABB Sector Lookup

Sector bounding boxes are at `sector_data+0x10` (min, vec4) and `sector_data+0x20` (max, vec4). The Lua timer reads the freecam position, iterates all sectors, and finds which AABB contains the camera. This replaces `RaycastFindSector` which can only find adjacent sectors via portal intersection — useless for a teleporting freecam.

---

## Function Reference

### Portal Core (Native x86)

| Address | Name | Purpose |
|---------|------|---------|
| `0x51A130` | PortalTraversal_Native | Recursive portal walk (2232 bytes), calls IsSectorLoaded + PortalDistancePreTest + ClipPortalPolygon |
| `0x5299E0` | ClipPortalPolygon | Portal polygon frustum clip wrapper |
| `0x5AC250` | SutherlandHodgmanClip | General polygon-vs-plane clipper |
| `0x517E50` | PortalDistancePreTest | Per-portal distance gate (dynamic threshold from portal geometry, context+0x90) |
| `0x57E0D0` | IsSectorLoaded | g_SectorStateArray[idx*3]==3, called by portal traversal to skip unloaded sectors |

### Portal Pipeline (Kallis VM Thunks)

| Address | Name | Thunk Target | Purpose |
|---------|------|-------------|---------|
| `0x562650` | FrustumSetup_VM | `0x1CF7AE0` | Compute frustum planes |
| `0x516900` | SetSectorState_VM | `0x20A5000` | Set sector for traversal |
| `0x51ABE0` | PortalTraversal_VM | `0x1CFDAE0` | Entry to portal walk |
| `0x5564D0` | PostTraversal_VM | `0x1CFB630` | Finalize traversal, triggers SectorRenderSubmit |
| `0x561B60` | PreTraversal_VM | — | Pre-traversal setup |
| `0x517050` | CheckVisMode_VM | — | Post-traversal mode check |
| `0x5193E0` | VisPostProcess_VM | — | Conditional post-processing |
| `0x519540` | FinalizeRender_VM | — | Render finalization |
| `0x5625E0` | FinalizeVisibility_VM | — | Visibility finalization |

### Render Pipeline Functions

| Address | Name | Prototype | Purpose |
|---------|------|-----------|---------|
| `0x488DD0` | CameraSectorUpdate | cdecl(float) | Main frame render entry |
| `0x488B30` | UpdateVisibility | stdcall(int,int,float) | PerformRoomCulling gate |
| `0x488970` | SectorVisibilityUpdate2 | cdecl(int,int,float) | Portal traversal pipeline |
| `0x48CBE0` | PortalTraversalPath2 | stdcall(int,int,float) | Alternate pipeline pass 2 |
| `0x490580` | PortalTraversalPath3 | userpurge(esi,int) | Third pipeline pass 2 |

### Render Submission Chain

| Address | Name | Purpose |
|---------|------|---------|
| `0x1C93F90` | SectorRenderSubmit | Iterates visibility array, submits geometry per sector |
| `0x561A20` | (unnamed) | VM-callable: FrustumSetup_VM + AddToRenderQueue per sector |
| `0x562D30` | AddToRenderQueue | Inserts into render queue at `dword_ECC2B0` |
| `0x562760` | RenderDispatch | Processes render queue, dispatches D3D draws |

### Camera / Sector Tracking

| Address | Name | Purpose |
|---------|------|---------|
| `0x43DA50` | MonocleUpdate | Camera mode handler (__thiscall, returns 0/1) |
| `0x43E2B0` | MainCameraComponent | Camera update, calls MonocleUpdate |
| `0x439410` | CameraTick | Camera tick with player pos stamp + SectorTransition |
| `0x4391D0` | PlayerSectorTransition | Sector detection via portal raycast |
| `0x519700` | RaycastFindSector | Portal-based sector finding (adjacent only) |

### Supporting Functions

| Address | Name | Purpose |
|---------|------|---------|
| `0x564950` | PerformRoomCulling | Object-level distance-based room culling |
| `0x5198D0` | IsSectorVisible | **DEBUG ONLY** — reads visibility for debug display, NOT for rendering |
| `0x599760` | DebugSectorDisplay | Debug overlay ("Drawing", "Loaded" etc.) |
| `0x4E9670` | SetupViewport | D3D viewport from camera+0x6C8 |
| `0x55DF50` | UpdateSectorAnims | 14 sector animation entries at `unk_EA22A0` |
| `0x4E1600` | SetAmbientColor | D3D ambient via `dword_E36E8C+172` |
| `0x5170A0` | GetSectorAmbientRGB | RGB from `[E416C4+7C]+108/109/110` |
| `0x529130` | GetCameraPosition | Reads camera+0x6B8/6BC/6C0 |
| `0x516B10` | IsSectorSystemActive_Mode0 | `[[E416C4]+0x80]+0x20 == 0` |
| `0x516E30` | IsSectorSystemActive_Mode1 | `[[E416C4]+0x80]+0x20 == 1` |

---

## Global Data

| Address | Type | Purpose |
|---------|------|---------|
| `0x0072F670` | ptr | camera_obj singleton |
| `0x006E4780` | obj | SectorVisibilityManager (this for UpdateVisibility) |
| `0x00E416C4` | ptr | **g_WorldState** — sector system (`+0x64`=sector ptrs, `+0x68`=portal base, `+0x78`=render data, `+0x80`=sector mgr) |
| `0x00E416C8` | int | **Sector count** (14 indoor, 2 outdoor) |
| `0x00E416CC` | ptr | **Visibility array** (8 bytes/sector) |
| `0x00E796EC` | ptr | Camera pointer cache |
| `0x01340080` | dword | Sector loading bitmask |
| `0x0133FEF0` | int | Sector count (for loading system) |
| `0x00ECC2B0` | array | **Render queue** (7752 bytes/slot, up to 62 slots) |
| `0x00F80090` | int | Render queue entry count |
| `0x00E36E8C` | ptr | **IDirect3DDevice9** pointer |
| `0x007147C0` | ptr | PostTraversal_VM this pointer |
| `0x00EA22A0` | array | 14 sector entries, 4992 bytes each (+0x133C=active flag) |
| `0x00E416B8` | dword | One-time init flag (bit 0, used in sub_51A130) |
| `0x00E412D0` | obj | Frustum data object |
| `0x00E45168` | dword | Current traversal sector ID |
| `0x00EBD058` | array | VM render path data |
| `0x00EBD064` | array | VM render path data |
| `0x00EBD0A8` | int | VM render path entry count (max 5) |
| `0x00EBD0B0` | array | VM render path data |
| `0x0072FC10` | float | Mouse input X |
| `0x0072FC14` | float | Mouse input Y |

---

## Related Files

### Documentation
- [ROOM_CLIPPING.md](ROOM_CLIPPING.md) — three-layer visibility overview + experiment log
- [subs/sub_488DD0_CameraSectorUpdate.md](subs/sub_488DD0_CameraSectorUpdate.md) — Pipeline 1 entry
- [subs/sub_488B30_UpdateVisibility.md](subs/sub_488B30_UpdateVisibility.md) — PerformRoomCulling gate
- [subs/sub_564950_PerformRoomCulling.md](subs/sub_564950_PerformRoomCulling.md) — Layer 2 culling
- [subs/sub_516B10_IsSectorSystemActive.md](subs/sub_516B10_IsSectorSystemActive.md) — Sector system gate
- [subs/sub_4391D0_SectorTransition.md](subs/sub_4391D0_SectorTransition.md) — Sector change detection
- [subs/sub_1C93F90_SectorRenderSubmit.md](subs/sub_1C93F90_SectorRenderSubmit.md) — Sector render submit
- [subs/sub_562D30_AddToRenderQueue.md](subs/sub_562D30_AddToRenderQueue.md) — Render queue insertion
- [subs/sub_562760_RenderDispatch.md](subs/sub_562760_RenderDispatch.md) — D3D dispatch
- [CAMERA_RENDERING.md](CAMERA_RENDERING.md) — Camera rendering pipeline (FOV, frustum, projection)
- [../world/SECTOR_CULLING.md](../world/SECTOR_CULLING.md) — Distance-based sector culling (proximity service + portal pre-test)
