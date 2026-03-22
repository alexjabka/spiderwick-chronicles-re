# Spiderwick Freecam - Session Log

## Session: 2026-03-15 — Finding the Position Overwriter

### Goal
Find why WASD movement doesn't work (position writes at +3B8/3BC/3C0 getting overwritten).

### Key Discovery: sub_5A7DC0 (Spiderwick.exe+1A7DC0)

**How found:** CE "Find what writes to this address" on `[pCamStruct]+3B8` while freecam active.

**Result:** Single instruction found:
```
005A7DF7 - D9 58 EC - fstp dword ptr [eax-14]    (5504 writes)
```

This instruction is inside `sub_5A7DC0` — a generic 16-float memcpy function.

### sub_5A7DC0 Analysis (IDA pseudocode)

```c
float __thiscall sub_5A7DC0(char *this, int a2)
{
    // this (ECX) = destination struct
    // a2 = source data address
    int v2 = a2 + 0xC;
    float *result = (float *)(this + 4);
    int v4 = a2 - (DWORD)this;
    int v5 = 2;           // loop counter
    do {
        result += 8;       // advance by 32 bytes per iteration
        // copy 8 floats from source to dest
        result[-9] = *(float *)(v2 - 0xC);
        result[-8] = *(float *)((char *)result + v4 - 0x20);
        result[-7] = *(float *)(v2 - 0x24);
        result[-6] = *(float *)(v2 - 0x20);
        result[-5] = *(float *)(v2 - 0x1C);
        result[-4] = *(float *)(v2 - 0x18);
        result[-3] = *(float *)(v2 - 0x14);   // <-- THIS is fstp [eax-14] at +1A7DF7
        result[-2] = *(float *)(v2 - 0x10);
        v2 += 0x20;
        v5--;
    } while (v5);
    return result;
}
```

**Key facts:**
- `__thiscall` convention: ECX = destination pointer
- Copies 2 iterations x 8 floats = **16 floats (64 bytes)** total
- Writes to dest offsets: `this+0x00` through `this+0x3C`
- Called for MANY game objects (5504 writes during measurement)
- When called with dest pointing into camera struct area containing position offsets, it overwrites our Lua-written position every frame

**ASM bytes at entry (for hook):**
```
005A7DC0: 56              push esi
005A7DC1: 8B 74 24 08     mov esi, [esp+8]    ; esi = arg a2 (source)
005A7DC5: 8D 56 0C        lea edx, [esi+0Ch]
005A7DC8: 8D 41 04        lea eax, [ecx+4]    ; eax = this + 4
005A7DCB: 2B F1           sub esi, ecx
005A7DCD: B9 02 00 00 00  mov ecx, 2          ; loop count
```

### Solution Implemented: v7 Hook

Added hook at `sub_5A7DC0` entry. Logic:
1. Check `fc_enabled == 1` (freecam active?) — if not, normal flow (fast path)
2. Compare ECX (destination) against `[pCamStruct]` (camera base address)
3. If `(ECX - camBase)` is within 0x400 bytes — it's the camera struct → `ret 4` (skip)
4. Otherwise → normal copy

```asm
cpyHook:
  cmp dword ptr [fc_enabled], 1
  jne cpy_original
  push eax
  push ebx
  mov eax, [pCamStruct]        // camera base address
  mov ebx, ecx                 // ebx = destination (this)
  sub ebx, eax                 // ebx = dest - camBase
  cmp ebx, 0x400               // within camera struct range?
  pop ebx
  pop eax
  ja cpy_original              // outside range -> normal copy
  ret 4                        // inside camera struct + freecam on -> skip
cpy_original:
  push esi
  mov esi, [esp+8]
  jmp cpy_return
```

**Why unsigned comparison works:**
- If dest < camBase: `sub` underflows → huge unsigned value → `ja` taken → normal copy
- If dest > camBase + 0x400: value > 0x400 → `ja` taken → normal copy
- If dest within [camBase, camBase+0x400]: value ≤ 0x400 → falls through → skip

### Current Hook Architecture (v7)

| # | Target | Hook Type | What it blocks |
|---|--------|-----------|----------------|
| 1 | `sub_5299A0` (+1299A0) | Entry hook → `ret 8` | Rotation matrix rebuild (killed flicker) |
| 2 | `sub_5A7DC0` (+1A7DC0) | Entry hook → `ret 4` | **Generic memcpy into camera struct (killed position overwrite)** |
| 3 | `+3E547` | Byte patch jne→jmp | Camera update pipeline (orbit, collision, player tracking) |

All three controlled by single `fc_enabled` flag (hooks 1 & 2 check it; hook 3 is always active while script enabled).

### Status After This Session

**Expected to work now (UNTESTED):**
- [ ] WASD movement — position writes should persist now that memcpy is blocked
- [ ] Arrow key rotation — was already working

**Still known issues:**
- [ ] Forward/backward flip after monocle (Shift) release
- [ ] No mouse look (only arrow keys)
- [ ] Monocle mode interaction needs more work

### v7 Hook FAILED — Broke Rendering

**What happened:** Screen stretched weirdly, then world stopped rendering entirely.

**Root cause:** sub_5A7DC0 is called ~5500 times for ALL game objects.
Our 0x400 range check blocked copies for view matrix, projection matrix,
and other rendering-critical data within the camera struct. Only position
copies should have been blocked.

**Lesson:** Never hook generic functions with broad range checks. Hook
the SPECIFIC caller instead.

**Fix applied:** Removed sub_5A7DC0 hook entirely. Reverted to v6 (rotation
hook + pipeline patch only).

### Next Step: Find Position Copy Caller

See `FREECAM_PLAN.md` for the full architecture plan.

### Caller Found: sub_4356F0

**Method:** `find_caller.lua` — temp ASM hook on sub_5A7DC0 entry, checks if
ECX is in camera position range, logs return address, auto-removes in 3 sec.

**Result:** Return address `004356FE` → call at `004356F9` → parent: `sub_4356F0`

sub_4356F0 is a specific struct copy function (~0xA2 bytes), NOT generic.
See [subs/sub_4356F0_PositionWriter.md](subs/sub_4356F0_PositionWriter.md) for full analysis.

### v8 Hook: sub_4356F0

Hooked entry: if `fc_enabled==1` AND `ECX == camBase+0x388` → `ret 4`.
Exact ECX match — only blocks camera position copy, nothing else.

| # | Target | What it blocks | Condition |
|---|--------|----------------|-----------|
| 1 | `sub_5299A0` (+1299A0) | Rotation matrix rebuild | fc_enabled==1 |
| 2 | `sub_4356F0` (+356F0) | **Camera struct copy (position)** | fc_enabled==1 AND ECX==camBase+0x388 |
| 3 | `+3E547` byte patch | Camera update pipeline | Always (while script enabled) |

**v8 result:** WASD moves camera but controls inverted (forward=backward, left=right).
Rendering is fine — no artifacts like v7.

---

### v9: Engine-Native Approach (Replace Args, Don't Skip)

**Key Discovery:** Decompiled sub_5299A0 → it's a thin wrapper:
```c
sub_5299A0(float *eye, float *target) {
    float up[] = {0, 0, 1};
    this += 0x480;
    return sub_5AA1B0(this, eye, target, up);  // LookAt!
}
```

**sub_5AA1B0 is a classic LookAt/BuildViewMatrix function:**
- Computes forward = normalize(target - eye)
- Computes right = normalize(cross(forward, up))
- Computes up2 = cross(right, forward)
- Writes 4x4 view matrix: rotation rows + translation = -dot(axis, eye)

See [subs/sub_5AA1B0_BuildViewMatrix.md](subs/sub_5AA1B0_BuildViewMatrix.md) for full analysis.

**New approach:** Instead of `ret 8` (skip function + manually compute matrix),
REPLACE the eye/target pointers on the stack with our data, then let the
function run normally. The engine builds a perfect view matrix.

**v9 result:** Forward/backward WASD fixed! But left/right still inverted,
and manual translation formula had sign issues. Also, RMB crashed the game
(executeCodeEx for Windows API mouse conflicted with game's DirectInput).

### v10: Fully Engine-Native (Shifted Target + Matrix-Read Movement)

**Final breakthrough:** Instead of computing target from manual yaw/pitch,
use the GAME'S OWN target shifted by our position offset:

```
new_target = game_target + (our_eye - game_eye)
```

And read forward/right vectors directly from the engine-built matrix for
WASD movement — no manual sin/cos at all.

**ASM hook:**
```asm
// For each axis (X, Y, Z):
fld [fc_eye]         // our position
fsub [original_eye]  // delta from game's eye
fadd [original_target] // shift game's target
fstp [fc_target]     // store shifted target

// Replace args
mov [esp+0C], fc_eye
mov [esp+10], fc_target
```

**Lua reads movement directions from engine matrix:**
```lua
local fwdX = readFloat(b + 0x04)   -- forward.x from engine matrix
local fwdY = readFloat(b + 0x14)   -- forward.y from engine matrix
local rightX = readFloat(b + 0x00) -- right.x from engine matrix
local rightY = readFloat(b + 0x10) -- right.y from engine matrix
```

**v10 result: WASD WORKS PERFECTLY.** All directions correct.
Mouse rotation works ONLY in monocle mode (Shift held).

### Current State: Mouse Rotation Gap

**Why mouse only works in monocle:**
- Our `+3E547` patch skips the camera update pipeline
- This means the game's normal camera orbit code doesn't run
- The eye/target args to sub_5299A0 become STALE (don't reflect mouse input)
- But monocle has a SEPARATE code path that computes fresh eye/target
- So monocle mouse rotation works, normal mouse rotation doesn't

**Solution: Force monocle ON + disable HUD darkening.**
This was the user's original idea from the very beginning of the project.
The monocle code path already provides mouse rotation through the engine.
We just need to:
1. Find the monocle state flag and force it ON when freecam active
2. Find and disable the HUD darkening overlay

### Current Hook Architecture (v10)

| # | Target | What it does | Condition |
|---|--------|-------------|-----------|
| 1 | `sub_5299A0` (+1299A0) | Shifts game's target by our position offset | fc_enabled==1 |
| 2 | `sub_4356F0` (+356F0) | Skips camera position copy | fc_enabled==1 AND ECX==camBase+0x388 |
| 3 | `+3E547` byte patch | Detaches camera from player | Always (while script enabled) |

### Next Step: Force Monocle + Remove HUD Darkening

---

## DQXI FlyCam Reference Analysis

Examined `EXAMPLE OF FLYCAM - Dragon Quest XI - FlyCam Script from Tutorial.CT`.
Architecture comparison with our Spiderwick freecam:

### DQXI Script Architecture

| Script | What it does |
|--------|-------------|
| Camera Structure | Injection at a movsd instruction, captures RCX into pCamStruct |
| Decouple Camera | **NOPs the specific XY and Z write instructions** — simplest possible approach |
| Flycam (Lua) | Timer-based movement using sin/cos of pitch/yaw angles |
| Pause Game World | NOPs a `call [rax+2E0]` to freeze world updates |
| NOP Roll | NOPs roll value write to prevent camera tilt |

### DQXI Camera Struct Offsets
```
+410 = X position
+414 = Y position
+418 = Z position
+41C = Pitch (degrees, float)
+420 = Yaw (degrees, float)
+424 = Roll (degrees, float)
```

### Key Differences: DQXI vs Spiderwick

| Aspect | DQXI | Spiderwick |
|--------|------|------------|
| Rotation storage | Simple Euler angles (degrees) as floats | 4x3 rotation matrix (12 floats) |
| Decouple method | NOP specific write instructions | Hook generic memcpy + patch camera pipeline |
| Movement axes | Forward uses pitch (moves in 3D direction) | Forward only moves on XY plane |
| Rotation input | Game handles mouse rotation natively | Arrow keys only (no mouse yet) |
| World pause | NOP a single call instruction | Not implemented |

### Lessons to Apply

1. **Forward movement should include pitch component** (HIGH PRIORITY)
   DQXI forward movement:
   ```lua
   camx + (cosy * speed)    -- horizontal forward
   camy + (siny * speed)    -- horizontal forward
   camz + (sinp * speed)    -- VERTICAL component based on pitch
   ```
   Our current code only moves on XY plane — pressing W while looking up
   should move the camera forward AND up, not just forward flat.

2. **Decouple by NOPing specific writes is simpler**
   DQXI found the exact `movsd [rdi+410]` instructions and NOPed them.
   We can't easily do this because Spiderwick's position is written by a
   generic memcpy function (sub_5A7DC0) shared by many objects. Our hook
   approach is correct for our case.

3. **World pause is useful for screenshots**
   DQXI NOPs a single call to freeze the game world. Finding a similar
   instruction in Spiderwick would allow freezing NPCs/particles while
   moving the camera — good for screenshot mode.

4. **NOP Roll prevents camera tilt**
   DQXI explicitly NOPs roll writes. We may need this if camera tilts
   unexpectedly during freecam.

### Code Pattern: Pitch-Aware 3D Movement (to implement)
```lua
-- Current (flat XY only):
if key(W) then camX += spd*fwdX; camY += spd*fwdY end

-- Improved (3D direction based on pitch):
if key(W) then
  camX += spd * cosp * fwdX    -- horizontal reduced by pitch
  camY += spd * cosp * fwdY    -- horizontal reduced by pitch
  camZ += spd * sinp            -- vertical based on pitch (look up = move up)
end
```
Where `sinp = math.sin(camPitch)`, `cosp = math.cos(camPitch)`.

---

## v13 → v14: Bug Analysis and Fixes (2026-03-15)

### v13 Test Results (reported by user)

**Main menu (flyby camera):**
- WASD works but left/right swapped
- Arrow keys rotate but feel like orbiting an invisible anchor
- Mouse does NOT control rotation (monocle forcing not working)

**Gameplay (player loaded):**
- Script crashes the game on enable (before INSERT is pressed)

### Bug 1: Crash — Extra NOP byte in Hook 3

**Root cause:** Hook 3 wrote `jmp monoHook` (5 bytes) + `nop` (1 byte) = 6 bytes,
but only 5 bytes needed replacing (`83 EC 60 53 56`). The NOP overwrote the byte
at `+3DA55` (first byte of the next instruction), and `mono_return` pointed to
`+3DA56` — potentially the middle of a multi-byte instruction.

```
Before (v13, BROKEN):
+3DA50: E9 xx xx xx xx   jmp monoHook
+3DA55: 90               NOP (corrupts original instruction!)
+3DA56:                   mono_return (wrong — mid-instruction?)

After (v14, FIXED):
+3DA50: E9 xx xx xx xx   jmp monoHook
+3DA55:                   mono_return (correct — next original instruction)
```

**Why it crashed in gameplay but not main menu:** Unclear — possibly the code
path through the corrupted instruction was only taken during gameplay, or the
camera object differs between states.

**Fix:** Removed the `nop` line from Hook 3.

### Bug 2: Crash — Missing monocle initialization

**Root cause:** Hook 3 set `this+0xB9=1` (Path 3: monocle rotation) without
ever triggering `this+0xBB=1` (Path 1: initialization). Path 3 uses data
structures initialized by Path 1. Without init, vtable[16] operates on
uninitialized/stale data → crash or garbage.

From sub_43DA50 docs:
- Path 1 (this+0xBB): copies camera data, inits monocle parameters
- Path 3 (this+0xB9): uses those parameters for mouse rotation

**Fix:** Added `fc_mono_init` flag. First frame sets `this+0xBB=1` (init),
subsequent frames set `this+0xB9=1` (rotation). Reset on toggle.

### Bug 3: Mouse rotation not captured

**Root cause:** Hook 1 replaced BOTH eye and target pointers with Lua's static
values (`fc_eye`, `fc_target`). The monocle's vtable[16] computes rotation from
mouse input and the pipeline feeds the result to sub_5299A0, but Hook 1
overwrites it with Lua's `fc_target` (computed from `camYaw`/`camPitch` which
only change via arrow keys).

The monocle rotation DID happen inside the engine, but we threw away the result.

**Fix:** Hook 1 now saves the game's eye/target to `fc_game_eye`/`fc_game_target`
BEFORE replacing them. Lua reads these each frame, computes the direction delta
(change in game's look direction between frames), and applies it to
`camYaw`/`camPitch`. This captures mouse rotation through the monocle.

Delta approach ensures arrow keys and mouse stack correctly:
```lua
delta_yaw = gameYaw - prevGameYaw   -- mouse rotation this frame
camYaw = camYaw + delta_yaw          -- add to our state (preserves arrow input)
```

### Bug 4: Left/right strafe swapped

**Root cause:** Right vector was `(-cos(yaw), sin(yaw))` — this is the LEFT
vector. For forward `(sin(yaw), cos(yaw))`, the right vector should be
`(cos(yaw), -sin(yaw))` (90° clockwise rotation in Z-up coordinate system).

**Fix:**
```lua
-- v13 (WRONG):
local rightX = -math.cos(camYaw)
local rightY = math.sin(camYaw)

-- v14 (FIXED):
local rightX = math.cos(camYaw)
local rightY = -math.sin(camYaw)
```

### Main menu "anchor rotation" (not fixed, documented)

Arrow keys in the main menu feel like orbiting around an anchor point.
Likely cause: Hook 2 blocks position writes only for `ECX==camBase+0x388`.
The main menu camera may use a different struct layout, so position writes
aren't blocked. Game overwrites our position each frame → camera snaps back
to game's position while our target changes → orbit appearance.

Not fixed in v14 (main menu is secondary; gameplay is priority).

### v14 Architecture

| # | Target | What it does | New in v14 |
|---|--------|-------------|-----------|
| 1 | `sub_5299A0` (+1299A0) | Saves game direction + replaces eye/target | Saves game eye/target |
| 2 | `sub_4356F0` (+356F0) | Skips camera position copy | (unchanged) |
| 3 | `sub_43DA50` (+3DA50) | Forces monocle (init→rotation) | Init Path 1, removed NOP |
| 4 | `+C27FD` | Disables monocle HUD overlay | (unchanged) |

### fcData Layout (v14)

```
+0x00  fc_enabled     (dd)    freecam on/off
+0x04  fc_eye         (3×dd)  our camera position
+0x10  fc_target      (3×dd)  our look-at target
+0x1C  fc_game_eye    (3×dd)  game's eye (saved by Hook 1)
+0x28  fc_game_target (3×dd)  game's target (saved by Hook 1)
+0x34  fc_mono_init   (dd)    monocle init flag (0=needs init, 1=initialized)
```

---

## v15: Direct Mouse Rotation (2026-03-15)

### Deep RE of Monocle System

Decompiled the full monocle pipeline to find mouse rotation:

**sub_439AF0 (vtable[16]):** NOT a mouse handler. Pure math: takes direction +
eye → builds camera matrix (right/forward/up basis + position). No mouse input.

**sub_43DA50 (monocle dispatcher):** Three paths, none read mouse. Path 3
computes `direction = a2 - eye` where `a2` is a look-at target from the pipeline.
vtable[16] builds a matrix from this direction. Mouse rotation must change `a2`.

**sub_43E2B0 (vtable[3], main camera component):** THE KEY FUNCTION.
Contains BOTH monocle dispatch AND orbit camera. Orbit camera reads mouse deltas
from static addresses:

```
0x72FC14 — mouse delta X (yaw), float
0x72FC10 — mouse delta Y (pitch), float
```

Written every frame by sub_43DC10 (vtable[7]) at address 0x0043DD47:
```asm
mov ecx, esi
call sub_5522F0              ; compute mouse delta
fstp dword ptr [0x72FC14]    ; store to static address
```

Count: 16684 writes observed — writes EVERY frame, regardless of RMB.
Values are rotation velocity (capped), not per-frame deltas.

### Key Insight: Mouse Rotation Is Upstream

The monocle has NO mouse processing. All mouse rotation happens in the
ORBIT CAMERA code inside sub_43E2B0 (moves eye around target using
flt_72FC10/14). The monocle just builds a matrix from whatever direction
the pipeline provides.

Forcing monocle (return 1) skips the orbit camera code, so flt_72FC10/14
are unused by the game. We read them directly in Lua.

### v15 Implementation

**Simple:** Read 72FC14 (yaw) and 72FC10 (pitch) in Lua, apply to camYaw/camPitch.
No monocle direction tracking, no game direction deltas, no RMB requirement.

```lua
local mouseDX = readFloat(0x72FC14) or 0
local mouseDY = readFloat(0x72FC10) or 0
camYaw = camYaw - mouseDX * MOUSE_SENS
camPitch = camPitch + mouseDY * MOUSE_SENS
```

Signs were inverted initially — both axes needed flipping (yaw: -, pitch: +).

### v15 Result: WORKING

Full mouse rotation without RMB. WASD movement correct. Arrow keys as additional.
Speed control via NUM+/-. No crashes.

### Subroutine Map (complete)

| Address | Name | vtable | Purpose |
|---------|------|--------|---------|
| +36190 | sub_436190 | — | Camera pipeline manager (calls components) |
| +3E2B0 | sub_43E2B0 | [3] | Main camera component (orbit + monocle dispatch) |
| +3DA50 | sub_43DA50 | — | Monocle mode dispatcher (3 paths) |
| +39AF0 | sub_439AF0 | [16] | Camera matrix builder (direction→basis→matrix) |
| +3DC10 | sub_43DC10 | [7] | Mouse input writer (→ 72FC10/14) |
| +1299A0 | sub_5299A0 | — | View matrix wrapper (calls LookAt) — HOOKED |
| +1AA1B0 | sub_5AA1B0 | — | LookAt / BuildViewMatrix |
| +356F0 | sub_4356F0 | — | Camera struct copy — HOOKED |
| +1A7DC0 | sub_5A7DC0 | — | Generic 16-float memcpy |
| +1522F0 | sub_5522F0 | — | Compute mouse delta (returns float on FPU) |
| +52E0 | sub_4052E0 | — | Vector normalize (__thiscall ECX=vec) |
| +1A8260 | sub_5A8260 | — | Build rotation matrix from basis vectors |

### Static Address Map

| Address | Type | Purpose | Written by |
|---------|------|---------|-----------|
| 72FC0C | byte | Axis inversion (horizontal) | Input handler |
| 72FC0D | byte | Axis inversion (vertical) | Input handler |
| 72FC0E | byte | Flag (cleared in monocle Path 3) | sub_43E2B0 |
| 72FC10 | float | Mouse delta Y (pitch) | sub_43DC10 |
| 72FC14 | float | Mouse delta X (yaw) | sub_43DC10 |
| 72FC18 | struct | Monocle config base | sub_43C880 |
| 72FC1C | dword | Saved state | sub_43DA50 Path 1 |
| 72FC30 | float | Zoom parameter (125.0) | sub_43DA50 Path 1 |
| 72FC38 | float | Zoom speed (4.0) | sub_43DA50 Path 1 |
| 72FC4E | byte | Flag | sub_43DA50 Path 1 |
| 6E77C4 | int | Monocle HUD overlay flag | +C27FD (NOPed) |

### Folder Restructured

Moved from category-based (`subs/monocle/`, `subs/pipeline/`) to
system-based (`engine/camera/subs/`). Ready for future RE areas
(input, rendering, audio, etc.).

---

## Room Clipping Investigation (2026-03-15)

### Goal
Disable room clipping so all rooms render when freecam flies outside normal bounds.

### Sector System (found, not the solution)
- **Static addresses found:**
  - `0x0133FEC0` — world object pointer
  - `0x01340080` — target sector loading bitmask
- `sub_57F140` calls `sub_57E2E0` (compute bitmask) then `sub_57ED60` (apply)
- Sectors are LARGE zones (indoor/outdoor), not individual rooms
- Bitmask = 0 outdoors, non-zero indoors, doesn't change between rooms
- Setting bitmask to 0xFFFFFFFF forces all sectors loaded but doesn't fix room rendering
- **Conclusion:** Sectors control ASSET LOADING, not room RENDERING

### draw_distance Setting (found, not the solution)
- Video settings at settings_object+0x458 — just a quality preset
- Loaded by sub_4EDC70 alongside resolution, gamma, etc.
- Not the runtime culling system

### Camera Settings Structure (found, documented)
- sub_426450 loads ALL camera parameters from data files
- Full offset map in `engine/camera/structs/camera_settings.md`
- Key parameters: CHAROFFSET_DISTFROMSUBJECT, STANDARD3RDPERSON_MAX_ALLOWABLE_DISTANCE,
  CAMERASETTINGS_FARPLANE, heights, radii, speeds, lerp settings

### What's Next
- `CAMERASETTINGS_FARPLANE` at 0x00623918 — far clip plane, controls max render distance
- Portal system — `sauSetPortalActive` might force all rooms visible
- `DrawDistance_controller` class — RTTI at 006E63B0
- See `engine/camera/ROOM_CLIPPING.md` for full investigation plan

---

## Portal / Sector Visibility Deep Dive (2026-03-15, session 2)

### Goal
Continue portal system RE to find room-level visibility control.

### Key Findings

#### 1. sauSetCameraSector (sub_490F40)
Camera sector stored at `camera_obj + 0x788`:
```c
sauSetCameraSector(sector_index) {
    camera_obj = sub_4368B0();      // singleton getter
    camera_obj[0x788] = sector_index;
}
```
But `sub_4368B0` chains through thunks → `nullsub_56` (patched at runtime by .kallis).
**Cannot trace statically in IDA.**

#### 2. Camera Manager ≠ pCamStruct - 0x480
Tested in CE: `pCamStruct - 0x480 + 0x788` = `0x3F800000` (float 1.0).
The camera object returned by sub_4368B0 is a **different singleton**, not related to pCamStruct.

#### 3. sauGetObjSectorIndex (sub_49A300)
**Object sector index = object + 0x24:**
```c
sauGetObjSectorIndex(object) {
    sub_52D820(&obj);   // get object from script
    return obj[0x24];   // sector index
}
```

#### 4. Portal scripting functions are visual effects
`sauSetPortalActive/RotationGoal/Scale/RippleParams/Texture` — these are magic portal **visual effects**, NOT sector visibility portals.

#### 5. sauSetSector has 26 xrefs
All are .kallis per-module registration chains. Dead end for finding the implementation.

#### 6. Sector count = 14 in house
14 sectors in the house level — could be **individual rooms**, not just "indoor/outdoor".
Need to investigate the 12-byte sector state array to confirm.

### Scripting API Pattern Documented
```
sub_52D820(&var) — get object arg from script
sub_52C610(&var) — get integer arg from script
sub_52CC30(val)  — return value to script
sub_52EA10       — registrar function (called in push chains)
```

### Next Session Priorities
1. **Sector state array** (0x01C8E82C, 12 bytes × sector_count) — walk between rooms, see what changes
2. **Far plane hook** on sub_50B760 — outdoor clipping fix (all info ready)
3. **IDA immediate search for 0x788** — find who reads camera sector
4. **sauIsVisible implementation** — find what controls the visibility flag
