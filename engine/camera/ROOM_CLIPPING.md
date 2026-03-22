# Room Clipping / Sector Visibility — Investigation
**Goal:** Disable room clipping for freecam screenshots
**Status:** SOLVED (v20.1) — Lua AABB sector lookup + direct sector write

## Architecture: Three-Layer Visibility System

Room visibility has **three independent layers**:

| Layer | What it controls | Address/Function | Mechanism | Status |
|-------|-----------------|------------------|-----------|--------|
| **Sector Loading** | Which sectors are in memory | Bitmask at `0x01340080` | Set 0xFFFFFFFF = all loaded | **SOLVED** — bitmask freeze |
| **Sector Culling** | Which rooms pass per-frame visibility | `sub_564950` (PerformRoomCulling) via `sub_488B30` (UpdateVisibility) | Per-frame culling by camera sector | **SOLVED** — jump patches in UpdateVisibility |
| **Portal Rendering** | Which rooms are actually drawn | `sub_562650` → `sub_51ABE0` (Kallis VM) | Renders only rooms visible through portals based on pre-computed frustum | **SOLVED** — frustum override before VM computation |

**Key discovery:** Room culling is driven by **camera position**, NOT player position.
When freecam flies away from the player, the engine recomputes which sector the
camera is in and hides rooms not in that sector. The player's position is irrelevant.

**Portal rendering discovery:** Even with layers 1 and 2 fully defeated, rooms still
appear/disappear based on **camera angle**. Looking through a doorway = that room renders.
Turn the camera away from the doorway = room disappears. This is a classic portal/PVS
(Potentially Visible Set) system that is completely independent of the sector loading
and sector culling systems.

## Teleport Observation (Session 3)

Teleporting the player to a new position (e.g., middle-mouse teleport in freecam) does **NOT** trigger
`SectorTransition` (sub_4391D0). Sector loading requires **physical traversal** through transition
zone triggers -- simply writing new coordinates to the player's position offsets is insufficient.
This means rooms will not load/unload based on teleported position alone; the player must physically
walk through sector boundary triggers for the engine to register a sector change.

## Experiments Log (v1 through v5)

### v1: NOP sub_564950 call
- **Patch:** NOP the `call sub_564950` at `+88B69` (5 bytes -> 5x NOP)
- **Result:** Rooms froze in their last visibility state. The camera's own room became invisible when moving between sectors.
- **Conclusion:** sub_564950 is responsible for visibility updates, but NOPing it freezes state rather than enabling all rooms.

### v2: Freeze sector + bitmask
- **Patch:** Freeze `0x01340080` to `0xFFFFFFFF` (all sectors loaded)
- **Result:** All sectors loaded into memory, but rooms still culled by sub_564950 per-frame.
- **Conclusion:** Sector loading and sector culling are independent layers. Bitmask alone is insufficient.

### v3: Patch IsSectorSystemActive to return false
- **Patch:** Made `sub_516B10` (IsSectorSystemActive) always return false
- **Result:** Crash. The sector system active flag is read by other engine systems, not just visibility.
- **Conclusion:** Cannot disable the gate functions globally — too many side effects.

### v4: Patch jumps in UpdateVisibility
- **Patch:** Patched conditional jumps in `sub_488B30` to skip the culling block entirely
- **Result:** sub_564950 no longer called, but `sub_488970` (second per-frame pass) still culls rooms.
- **Conclusion:** There are multiple culling call sites; patching one is not enough.

### v5: v4 + freeze sector + bitmask
- **Patch:** Combined jump patches in UpdateVisibility + bitmask freeze `0x01340080 = 0xFFFFFFFF`
- **Result:** Sector loading solved, sector culling solved, BUT portal rendering still hides rooms based on camera angle. Looking through doorways = rooms visible, turn away = hidden.
- **Conclusion:** The three-layer system is confirmed. Portal rendering is the remaining blocker and is NOT controlled by the sector system at all.

### v6 (portal_frustum_bypass v1): FOV override before FrustumSetup_VM
- **Patch:** Hooked 6 call sites — overrode FOV to 3.1 rad + direction to (0,-1,0) BEFORE `sub_562650`, restored AFTER `sub_51ABE0`
- **Result:** Rooms half-loaded, character invisible. Direction override at +0x568 killed character rendering but VM reads direction from elsewhere.
- **Conclusion:** FOV override partially worked but direction override was wrong offset for VM frustum.

### v7 (portal_frustum_bypass v3): jnz→jmp in PortalTraversal_Native — SUCCESS
- **Patch:** Single 6-byte patch at `0x51A3B5` — `jnz → jmp` in native `sub_51A130`
- **Result:** All portals bypass frustum clip test. Rooms render regardless of camera angle.
- **Conclusion:** The actual frustum check is in **native x86** code (sub_51A130), not in Kallis VM. Direct branch patch is the clean solution.

### v8 (portal_frustum_bypass v4): v7 + sector freeze + all-pipeline NOP
- **Patch:** v7 + NOP sector transition write + NOP PerformRoomCulling in all 3 pipelines
- **Result:** Worse than v7 — sector freeze broke things
- **Conclusion:** Sector freeze (NOP at +39356) causes issues

### v9 (IsSectorVisible always true)
- **Patch:** sub_5198D0 → `mov al,1; ret` (3 bytes)
- **Result:** Objects visible (lamps, curtains) but rooms DARK — no wall/floor geometry
- **Key finding:** sub_5198D0 is **DEBUG ONLY** (called from debug overlay sub_599760). Does NOT control rendering.

### v15 (Hook player position stamp at 0x439449) — BEST RESULT
- **Patch:** Replace player position write with freecam position (fc_eye) in CameraTick
- **Result:** Significantly more rooms render! House visible from outside with lit rooms.
- **Key finding:** SectorTransition reads from camera_obj+0x864. Freecam froze this field.
  Hook writes freecam position → correct sector → portal traversal starts from right room.

### v17 (NOP render gate in sub_1C93F90)
- **Patch:** NOP jz at 0x1C93FB8 in SectorRenderSubmit (native x86 in .kallis section)
- **Result:** No effect
- **Why:** Sectors need frustum data beyond just visibility flag

### v18 (Force visibility before PostTraversal_VM)
- **Patch:** Hook at 0x4889DF, force all dword_E416CC[i*8]=1 before render submit
- **Result:** No effect — sub_1C93F90 returns early after first sector

### v20 (Graceful freecam — no monocle mode)
- **Approach:** Removed monocle entirely. MonocleUpdate (Hook 3) returns 1 WITHOUT setting monocle flags. CameraTick native block skipped (bit 0x20 not set in flags). Sector written from Lua AABB lookup.
- **Result:** Sector tracking WORKS — Lua correctly identifies camera sector from AABB bounds. BUT rooms still disappeared when flying between sectors because SectorTransition (sub_4391D0) couldn't handle sector jumps (portal-incremental only).
- **Conclusion:** Sector tracking via AABB is correct, but the engine's SectorTransition is not designed for arbitrary jumps.

### v20.1 (Lua AABB sector lookup — SOLUTION)
- **Approach:** Removed ALL matrix hacks. Removed Hook 6 (force visibility). Added Lua AABB sector lookup that reads bounding boxes from `sector_data+0x10` (min XYZ) and `sector_data+0x20` (max XYZ). Writes sector index directly to `camera_obj+0x788`, bypassing SectorTransition entirely.
- **Result:** WORKS. Rooms render correctly when flying into them. No crashes, no corruption.
- **Why it works:** By writing sector directly to +0x788, we bypass the incremental portal-based SectorTransition (sub_4391D0) which can't handle jumps. The AABB lookup gives correct sector for any arbitrary camera position. The portal traversal then starts from the correct sector and renders connected rooms properly.
- **Key:** This is the WORKING SOLUTION for sector tracking in freecam.

### Complete Render Pipeline (discovered session 4)
```
PortalTraversal_VM → PortalTraversal_Native (sub_51A130)
    marks sectors visible in dword_E416CC
        ↓
PostTraversal_VM (sub_5564D0)
    → SectorRenderSubmit (sub_1C93F90, native x86 in .kallis)
        checks dword_E416CC[i*8] visibility flag
        → SectorSubmitLoop (sub_5814A0) — 6 sub-objects per sector
            → AddToRenderQueue (sub_562D30) → dword_ECC2B0
                ↓
RenderDispatch (sub_562760)
    → MeshGroupIterator (sub_559B30)
        → MeshRenderer (sub_55B160)
            → SetStreamSource + SetIndices (sub_4EA2E0)
                → IDirect3DDevice9::DrawIndexedPrimitive
```

## Portal Rendering — SOLVED

**See [PORTAL_SYSTEM.md](PORTAL_SYSTEM.md) for full reverse engineering.**

### Root Cause
The portal system is implemented in **Kallis VM bytecode**, not native x86.
The visibility pipeline in each render pass is:
```
sub_562650(camera, 1)           → VM: compute frustum planes from camera FOV/direction
sub_516900(sector)              → VM: set current sector state
sub_51ABE0(sector, camera, 0)   → VM: recursive portal traversal using pre-computed frustum
```

Previous bypass scripts overrode camera FOV/direction before `sub_51ABE0`, but the
frustum was already computed by `sub_562650` earlier in the pipeline. The VM traversal
uses the **pre-computed frustum**, not the raw camera fields.

### Fix: portal_frustum_bypass.cea
Override camera FOV (3.1 rad / 177 deg) and direction (0,-1,0 = look down) **BEFORE**
`sub_562650`, then restore **AFTER** `sub_51ABE0`. Hooks 6 call sites across all
3 render pipelines. Combined with sector bitmask freeze and PerformRoomCulling NOP.

### Why Previous Approaches Failed
| Script | Problem |
|--------|---------|
| portal_bypass.cea | Override at wrong pipeline point (after frustum computation) |
| portal_widefov*.cea | Same — overrode FOV before traversal, not before frustum |
| portal_allsectors_v5.cea | Frustum still limited each per-sector traversal |
| portal_nop_test.cea | Without traversal, no rooms render at all |

## Camera Object

`sub_4368B0` returns the camera singleton used by the sector system.

**Runtime thunk chain** (after .kallis patching):
```
sub_4368B0 @ 004368B0:  JMP 00447AD6
00447AD6:               MOV EAX, [0072F670]    ; load camera_obj pointer
00447ADB:               JMP 004368B5           ; back to RET
004368B5:               RET
```

**camera_obj = `[0x0072F670]`** (static global pointer, always valid in-game)

- `camera_obj + 0x788` = current camera sector index (int)
- **NOT the same object as pCamStruct!** pCamStruct is the view matrix destination.
  camera_obj is the sector/visibility camera object.

## Player Object

- Captured via **pPlayerAddy hook** at `+87DF6` (inside `sub_487D70` / SectorDistanceCheck)
- Position: `+0x68` (X), `+0x6C` (Y), `+0x70` (Z) — world coordinates
- Separate objects for indoor/outdoor scenes — addresses change on level transition
- **Teleporting player does not trigger room loading** — the sector system uses distance checks, not player position directly

## Per-Frame Visibility Pipeline

**Function at `Spiderwick.exe+88DD0`** — called every frame:
```c
// sub_488DD0(float deltaTime)  — reconstructed
void __cdecl CameraSectorUpdate(float deltaTime) {
    camera_obj = GetCameraObject();           // sub_4368B0 -> [0x0072F670]
    int cameraSector = camera_obj[0x788];     // current sector index

    // 1. Update sector-based room visibility
    SectorVisibilityManager->UpdateVisibility(cameraSector, camera_obj, deltaTime);
    //   ECX = 0x006E4780  (SectorVisibilityManager singleton)
    //   sub_488B30 (stdcall: sector, camera_obj, float)

    // 2. Camera object updates
    camera_obj->sub_438E50([0x6ED394], [0x6ED414], 0, 0, 2);
    camera_obj->sub_438EE0();
    camera_obj->sub_438EA0(deltaTime);

    // 3. Second visibility pass
    cameraSector = camera_obj[0x788];         // re-read (may have changed)
    sub_488970(cameraSector, camera_obj, deltaTime);  // cdecl, 3 args

    // 4. Cleanup
    sub_562760();
    sub_55FFB0();
    jmp sub_552420();   // tail call
}
```

### Hardware Breakpoint Results for camera_obj+0x788

| Count | Address | Instruction | Role |
|-------|---------|-------------|------|
| ~6500/frame | `004392E8` | `mov ecx,[esi+788]` | Sector computation (every frame) |
| ~6500/frame | `00488DE0` | `mov eax,[esi+788]` | 1st read -> passed to `sub_488B30` |
| ~6500/frame | `00488E27` | `mov eax,[esi+788]` | 2nd read -> passed to `sub_488970` |
| 15 (room change) | `00439336` | `mov ecx,[esi+788]` | Sector transition: read old sector |
| 15 (room change) | `00439356` | `mov [esi+788],eax` | Sector transition: write new sector |
| 6 (enter house) | `0043942D` | `mov eax,[esi+788]` | Level transition read |
| 6 (enter house) | `00439467` | `mov [esi+788],eax` | Level transition write |
| 3 | `00440AC9` | `mov [eax+788],ecx` | Scripting write (sauSetCameraSector) |
| 2 | `00438E21` | `mov [esi+788],0` | Initialization (set to 0) |
| 1 | `004450DB` | `mov [eax+788],edi` | One-time write |

### Sector Transition Logic (at 00439332)
```asm
cmp  eax, edi            ; eax = new computed sector, edi = 0
jl   skip                ; if negative -> invalid, skip
mov  ecx, [esi+788h]     ; ecx = current camera sector
cmp  eax, ecx            ; new == current?
je   write_sector        ; if same -> skip update logic, just write
; ... (sector change handler: triggers loading/unloading) ...
write_sector:
mov  [esi+788h], eax     ; store new sector
```

## sub_488B30: UpdateVisibility (Sector Visibility Manager)

**Address:** Spiderwick.exe+88B30
**Calling convention:** stdcall, ECX = this (SectorVisibilityManager at `0x006E4780`)

```c
// Decompiled with renamed parameters
int __stdcall UpdateVisibility(int cameraSector, CameraObject *camera_obj, float deltaTime) {
    nullsub_182();                          // no-op
    sub_53E9C0();                           // setup (unknown)
    sub_519350(deltaTime);                  // time-based update

    if (IsSectorSystemActive() || IsSectorSystemActive2()) {
    //  sub_516B10()                sub_516E30()
        PerformRoomCulling(deltaTime, camera_obj);
    //  sub_564950 — the culling function
    }

    sub_55FD60(deltaTime);                  // post-update
    return sub_5625E0();                    // finalize
}
```

**Critical finding:** `cameraSector` (arg1) is **never used directly** by sub_488B30.
The sub-functions (especially `sub_564950`) read `camera_obj+0x788` internally.

### Assembly Listing
```asm
sub_488B30:
  call  nullsub_182
  call  sub_53E9C0
  fld   [esp+arg_8]           ; deltaTime
  push  ecx
  fstp  [esp]
  call  sub_519350            ; sub_519350(deltaTime)
  add   esp, 4
  call  sub_516B10            ; IsSectorSystemActive()?
  test  al, al
  jnz   short loc_488B5C      ; -> do culling
  call  sub_516E30            ; IsSectorSystemActive2()?
  test  al, al
  jz    short loc_488B71      ; -> skip culling

loc_488B5C:                    ; <- culling block entry
  mov   eax, [esp+arg_4]      ; eax = camera_obj
  fld   [esp+arg_8]           ; deltaTime
  push  eax                   ; arg: camera_obj
  push  ecx
  fstp  [esp]                 ; arg: deltaTime (float)
  call  sub_564950            ; PerformRoomCulling(deltaTime, camera_obj)
  add   esp, 8

loc_488B71:                    ; <- after culling block
  fld   [esp+arg_8]
  push  ecx
  fstp  [esp]
  call  sub_55FD60            ; post-update
  add   esp, 4
  call  sub_5625E0            ; finalize
  retn  0Ch                   ; stdcall: clean 3 args (12 bytes)
```

### Gate Functions
- **`sub_516B10`** (IsSectorSystemActive) — returns true if `[[0xE416C4]+0x80]+0x20 == 0`
- **`sub_516E30`** (IsSectorSystemActive2) — returns true if `[[0xE416C4]+0x80]+0x20 == 1`
- If BOTH return false -> `sub_564950` is never called -> no room culling

## Static Addresses

| Address | Type | Purpose |
|---------|------|---------|
| `0x0072F670` | ptr | **camera_obj pointer** (read by sub_4368B0) |
| `camera_obj+0x788` | int | Camera sector index |
| `0x006E4780` | obj | **SectorVisibilityManager singleton** (this for sub_488B30) |
| `0x00E416C4` | ptr | **Sector system state pointer** (used by IsSectorSystemActive / IsSectorSystemActive2) |
| `0x0133FEC0` | ptr | World object (sector manager) |
| `0x01340080` | dword | Target sector bitmask (= world+0x1C0) |
| `0x0133FEF0` | int | Sector count (14 in house, 2 outdoors) |
| `0x006ED414` | dword | Global used by camera sector update |
| `0x006ED394` | dword | Global used by camera sector update |
| `0x00F800xx` | various | Globals used by PerformRoomCulling (sub_564950) |

## Functions

| Address | Name | Purpose |
|---------|------|---------|
| `sub_4368B0` | GetCameraObject | Returns `[0x0072F670]` (camera_obj singleton) |
| `sub_488DD0` | CameraSectorUpdate | Per-frame: reads sector, calls visibility pipeline |
| `sub_488B30` | UpdateVisibility | Conditionally calls room culling |
| `sub_564950` | **PerformRoomCulling** | The actual room culling function |
| `sub_488970` | SectorVisibilityUpdate2 | 2nd per-frame call with sector — also culls rooms |
| `sub_516B10` | IsSectorSystemActive | Gate: returns true if `[[E416C4]+80]+20 == 0` |
| `sub_516E30` | IsSectorSystemActive2 | Gate: returns true if `[[E416C4]+80]+20 == 1` |
| `sub_487D70` | **SectorDistanceCheck** | Distance-based sector loading (see details below) |
| `sub_4391D0` | **SectorTransition** | Sector transition detection (see details below) |
| `sub_51B2A0` | **SetTransform** | Generic transform copy: matrix + position (see details below) |
| `sub_50B760` | SetProjection | thiscall(fov, aspect, near, far) -> D3DXMatrixPerspectiveFovLH |
| `sub_519700` | SectorRaytest | Determines which sector a point is in |
| `sub_519350` | (unknown) | Time-based update in visibility pipeline |
| `sub_55FD60` | (unknown) | Post-update in visibility pipeline |
| `sub_5625E0` | (unknown) | Finalize in visibility pipeline |
| `sub_57F140` | SectorUpdateTick | Calls sub_57E2E0 then sub_57ED60 |
| `sub_57E2E0` | ComputeSectorBitmask | Computes loading bitmask from position |
| `sub_57ED60` | LoadUnloadSectors | Loads/unloads sectors to match bitmask |
| `sub_490F40` | sauSetCameraSector | Script API: writes camera_obj+0x788 |
| `sub_49A300` | sauGetObjSectorIndex | Script API: reads object+0x24 |
| `sub_51ABE0` | (kallis indirect) | `jmp [off_1C8A4E4]` — possibly portal setup |

### sub_487D70: SectorDistanceCheck (NEW)

Distance-based sector loading. This is where the **pPlayerAddy hook** captures the player object.

| Offset | Purpose |
|--------|---------|
| `this+0x3C` | Player object pointer |
| `[edi+0x68]` | Player X position |
| `[edi+0x6C]` | Player Y position |
| `[edi+0x70]` | Player Z position |
| `this+0x34` | Sector loading bitmask |
| `this+0x28` | Distance threshold |

### sub_4391D0: SectorTransition (NEW)

Sector transition detection. Fires when the camera crosses a sector boundary.

- Calls `sub_519700` (SectorRaytest) to determine which sector the camera is in
- Prints debug string: `"Sector Changed: was %d now %d\n"`
- Writes new sector index to `camera_obj+0x788`

### sub_51B2A0: SetTransform (NEW)

Generic transform copy — copies a 4x4 matrix + position vector from a source buffer to an object.

- `[this+0x68]` = X position (from `[src+0x30]`)
- `[this+0x6C]` = Y position (from `[src+0x34]`)
- `[this+0x70]` = Z position (from `[src+0x38]`)

### sub_50B760: SetProjection

**Calling convention:** thiscall(fov, aspect, near, far) -> calls D3DXMatrixPerspectiveFovLH

| Offset | Purpose |
|--------|---------|
| `this+0x124` | FOV (field of view) |
| `this+0x128` | Aspect ratio |
| `this+0x12C` | Near plane |
| `this+0x130` | Far plane |

## Sector System Details

### Sector State Array (0x01C8E82C) — Inconclusive
- 12 bytes per sector, but **data looks like misaligned heap structures**
- Mix of pointers, small ints, FFFFFFFF sentinels, and garbage
- Address may be stale/reallocated between level loads
- **Not useful for the current approach** — culling is controlled by sub_564950, not by state flags

## Other Approaches Investigated

### Sector Bitmask Freeze (0x01340080 = 0xFFFFFFFF)
- Forces all sectors **loaded** in memory
- Room clipping persists — sectors control LOADING, not RENDERING
- Must be combined with culling patch for full fix

### Camera Sector Freeze (camera_obj+0x788)
- Prevents rooms from disappearing for the frozen sector
- **Breaks sector LOADING** for the player — player's room may not load
- Not viable as standalone fix

### IsSectorSystemActive Patch (sub_516B10 -> return false)
- Crashed the game — flag is used by other engine systems beyond visibility

### Scripting API Dead Ends
- `sauSetSector` — 26 xrefs, all .kallis registration chains, untraceable
- `sauSetPortalActive` etc. — visual magic portal effects, not sector portals
- `sauIsVisible` / `sauMarkAsVisibleByPlayer` — .kallis registrations, not investigated further

## Remaining Work

### Portal Rendering — DONE
All items completed. See [PORTAL_SYSTEM.md](PORTAL_SYSTEM.md).
- sub_51ABE0 = Kallis VM portal traversal thunk
- sub_562650 = Kallis VM frustum computation
- Portal visibility determined by pre-computed frustum, not raw camera fields
- Fix: override FOV+direction before sub_562650, restore after sub_51ABE0

### After Room Clipping Fix
1. **Far plane hook** on `sub_50B760` (+10B760) for outdoor world clipping
2. **FOV control** via same `sub_50B760` hook
8. World pause

## Related Files
- [ARCHITECTURE.md](ARCHITECTURE.md) — camera system overview
- [structs/camera_settings.md](structs/camera_settings.md) — full camera settings map
- [subs/sub_43E2B0_MainCameraComponent.md](subs/sub_43E2B0_MainCameraComponent.md) — main camera component
- `tools/sector_monitor.lua` — sector state array monitor (inconclusive)
- `tools/find_cam_obj.lua` — camera_obj discovery script
- `mods/freecam/nocull_farplane_CT_entry.cea` — v5 (sector patches + bitmask freeze)
