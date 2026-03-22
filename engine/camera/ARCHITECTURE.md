# Spiderwick Camera Architecture
**Last updated:** 2026-03-17 (v20.1 session)

## Overview

The camera system is component-based. A central **Camera Manager** object
processes an array of **Camera Components**, each modifying camera state
(position, direction, zoom, etc.). After all components run, the final
eye/target are passed to **LookAt** (sub_5AA1B0) to build the 4x4 view matrix.

## Camera Update Flow (confirmed v20 session)

```
1. MainCameraComponent (0x43E2B0)
   ├── Calls MonocleUpdate (sub_43DA50)
   │   └── Returns 0 (normal camera) or 1 (handled — skip orbit)
   ├── If MonocleUpdate returned 0 → orbit camera code runs
   └── Final eye/target → LookAt → view matrix

2. MonocleUpdate (0x43DA50)
   ├── Path 1: this+0xBB set → first-frame init
   ├── Path 2: this+0xB8 set → transition
   └── Path 3: this+0xB9 set → active monocle mode
   In freecam v20: returns 1 WITHOUT setting any flags
   → orbit camera skipped, no monocle side-effects

3. CameraTick (0x439410)
   ├── VM function with native block gated by bit 0x20 in flags (+0x78C)
   ├── When bit 0x20 set:
   │   ├── GetPlayerCharacter → player position stamp → +0x864
   │   ├── Write player sector to +0x788
   │   └── Call SectorTransition (sub_4391D0)
   └── When bit 0x20 NOT set (monocle/freecam): entire block SKIPPED
```

## Rendering Flow (confirmed v20 session)

```
1. CameraSectorUpdate (0x488DD0) — main per-frame entry
   ├── Reads camera_obj+0x788 (sector index)
   └── Calls UpdateVisibility + SectorVisibilityUpdate2

2. UpdateVisibility (0x488B30)
   └── PerformRoomCulling (sub_564950) — room objects, particles

3. SectorVisibilityUpdate2 (0x488970)
   ├── FrustumSetup_VM (sub_562650) — compute frustum planes
   ├── PortalTraversal_VM (sub_51ABE0) → PortalTraversal_Native (sub_51A130)
   │   └── Recursive portal walk, marks sectors visible in dword_E416CC
   ├── PostTraversal_VM (sub_5564D0)
   │   └── SectorRenderSubmit (sub_1C93F90) — checks dword_E416CC, submits geometry
   └── SectorSubmitLoop (sub_5814A0) → AddToRenderQueue (sub_562D30)

4. RenderDispatch (sub_562760)
   └── Processes render queue (dword_ECC2B0) → MeshRenderer → D3D calls
```

## Camera Rendering Pipeline (Session 5)

The projection/frustum pipeline runs AFTER the view matrix is built:

```
CameraSystem__UpdateRender (0x436190)
  └── CameraRender__SetFovAndClip (0x5293D0)
        ├── Frustum__BuildCorners (0x5AABD0) → camObj+0x20  (portal traversal)
        └── Frustum__BuildProjectionMatrix (0x5A8B20) → camObj+0x5C8  (D3D projection)
              └── Projection far plane HARDCODED to 10000.0 (constant at 0x63EB64)
```

**See [CAMERA_RENDERING.md](CAMERA_RENDERING.md) for full pipeline documentation.**

## Sector Detection (confirmed v20 session)

- **PlayerSectorTransition** reads `+0x790`/`+0x7D0` (VIEW matrices, NOT `+0x834`!)
- **RaycastFindSector** (sub_519700): portal-incremental only, CANNOT jump sectors
- **AABB bounds**: `sector_data+0x10` (min XYZ) / `sector_data+0x20` (max XYZ)
- **v20.1 solution**: Lua AABB lookup writes sector directly to `camera_obj+0x788`

## Key Global Data

| Address | Type | Purpose |
|---------|------|---------|
| `0x00E416C4` | ptr | **g_WorldState** — sector system root pointer |
| `[0x0072F670]` | ptr | **camera_obj** — sector camera singleton |
| `0x00E416C8` | int | **Sector count** (used by render loops) |
| `0x00E416CC` | ptr | **Visibility array** (8 bytes/sector: byte[0]=visible flag) |
| `0x00ECC2B0` | array | **Render queue** (7752 bytes/entry, up to 62 slots) |

## Object Hierarchy

There are **two distinct camera objects** — do not confuse them:

```
┌─ CameraManager (sub_436190's this) ──────────────────────────────┐
│  = pCamStruct - 0x480                                            │
│  Purpose: view matrix pipeline (components → monocle → LookAt)   │
│                                                                   │
│  ├── +0x00..+0x04: vtable, state                                 │
│  ├── +0x78: pAnotherCamera pointer (used by monocle)             │
│  ├── +0x84: Z offset float (monocle eye Z)                       │
│  ├── +0x88: component[] array pointer                            │
│  ├── +0x90: scale float (monocle distance)                       │
│  ├── +0xB8: transition flag (byte)                               │
│  ├── +0xB9: monocle active flag (byte)                           │
│  ├── +0xBB: monocle init flag (byte)                             │
│  ├── +0xC8: component count                                      │
│  │                                                                │
│  └── +0x480: pCamStruct (View Matrix destination)                │
│        ├── +0x00..+0x3C: 4x4 view matrix (written by LookAt)    │
│        ├── +0x388: position sub-structure (16 floats × 2)        │
│        │     +0x30 (+0x3B8 abs): Camera X                        │
│        │     +0x34 (+0x3BC abs): Camera Y                        │
│        │     +0x38 (+0x3C0 abs): Camera Z                        │
│        └── +0x394: camera identifier?                            │
└──────────────────────────────────────────────────────────────────┘

┌─ SectorCameraObject (sub_4368B0's return) ───────────────────────┐
│  = [0x0072F670]                                                   │
│  Purpose: sector/visibility system (which rooms to render)        │
│                                                                   │
│  ├── +0x788: camera sector index (int)                           │
│  │     Written by sector transition code (sub_4391D0)            │
│  │     Read every frame by visibility pipeline (sub_488DD0)      │
│  │                                                                │
│  └── (other fields unknown — large object, 0x788+ bytes)         │
└──────────────────────────────────────────────────────────────────┘

SectorVisibilityManager singleton at 0x006E4780
  └── UpdateVisibility(sector, camera_obj, dt) = sub_488B30
        └── PerformRoomCulling(dt, camera_obj)  = sub_564950
```

**Important:** These two camera objects are completely separate.
`pCamStruct` controls WHERE the camera looks (view matrix).
`SectorCameraObject` controls WHAT the camera sees (room visibility).

## Main Pipeline: sub_436190

**Address:** Spiderwick.exe+36190

```
sub_436190(this, deltaTime, cameraState, a4)
  │
  ├── for each component in this+0x88[]:
  │     component->vtable[0x2C/4](deltaTime)      // update
  │     component->vtable[0x0C/4](cameraState, buf) // process
  │     component->vtable[0x10/4]()                 // finalize
  │
  ├── call sub_43DA50(this, a2, a3, a4)   // at +3E540
  │     monocle/collision check
  │     returns 1 = monocle handled it
  │
  ├── if (returned 1) goto skip_orbit     // at +3E547
  │     ... orbit camera code (reads player pos, computes orbit) ...
  │
  ├── skip_orbit:                          // at +3ED9C
  │     direction = pipeline output
  │     eye = camera position
  │     target = eye + direction
  │
  ├── sub_4356F0(camera_data)              // copy state → HOOKED
  │
  └── sub_5299A0(eye, target)              // build view matrix → HOOKED
        └── sub_5AA1B0(this+0x480, eye, target, up={0,0,1})
```

## Component System

Components are stored as an array of pointers at `camManager+0x88`.
Count at `camManager+0xC8`. Each component has its own vtable.

Components process input (keyboard, mouse, gamepad) and modify camera state.
**Mouse rotation for normal orbit camera is handled by a component**, not
by sub_43DA50 directly.

### Component Virtual Functions
```
vtable[0x0C/4] = vtable[3]  — process(cameraState, buffer)
vtable[0x10/4] = vtable[4]  — finalize()
vtable[0x2C/4] = vtable[11] — update(deltaTime)
```

### TODO: Identify Components
Run `monocle_dump.lua` to list components and their vtables.
Then decompile the mouse input component to understand how
mouse deltas feed into camera rotation.

## Monocle System: sub_43DA50

**Address:** Spiderwick.exe+3DA50

The monocle is a SEPARATE code path from normal orbit camera.
However, it does NOT have its own mouse rotation — it receives
the look-at target (`a2`) from the pipeline, which already includes
any mouse rotation applied by the component system.

**vtable[16] (sub_439AF0) is just a matrix builder** — it takes
direction + eye and constructs a camera transform. No mouse input.

### Three Paths

| Path | Flag | Trigger | What it does |
|------|------|---------|-------------|
| 1 | this+0xBB | First frame of monocle | Initialize: copy camera state, set params |
| 2 | this+0xB8 | Transition | Simple state copy |
| 3 | this+0xB9 | Active mode | Compute dir from a2, build matrix via vtable[16] |

Flags are cleared at end of function each frame. Game's input handler
re-sets them each frame when RMB/Shift is held.

### Path 3 (Mouse Rotation) — Detailed

```c
// 1. Get secondary camera pointer
v6 = (float *)this[0x1E];    // this+0x78 = pAnotherCamera

// 2. Get scale
scale = this_float[0x24];     // this+0x90

// 3. Compute monocle eye position
eye.x = v6[14]*scale + v6[22];  // pAnother+0x38 * scale + pAnother+0x58
eye.y = v6[15]*scale + v6[23];  // pAnother+0x3C * scale + pAnother+0x5C
eye.z = v6[24] + this[0x84];    // pAnother+0x60 + Z_offset

// 4. Compute initial look direction
direction = a2 - eye;            // a2 = input position (player?)

// 5. VIRTUAL FUNCTION: apply mouse rotation
vtable[16](this, output, direction, eye);
// vtable[16] = vtable[0x40/4]
// Takes current direction, applies mouse dx/dy, outputs new camera transform

// 6. Save result
sub_5A7DC0(a3, output);          // copy to output parameter
sub_5A7DC0(this+0x38, output);   // save to internal state
return 1;
```

### Key Virtual Function: vtable[16]

```
Signature (inferred):
  __thiscall vtable16(this, float *output, float *direction, float *eye)

  this      = monocle object (same as sub_43DA50's this)
  output    = buffer for new camera transform (16 floats?)
  direction = current look direction (3 floats, NOT normalized?)
  eye       = current eye position (3 floats)

  Returns: modified camera transform in output
```

**This function is the ACTUAL mouse rotation engine.**
It reads mouse dx/dy from somewhere (DirectInput? input struct?)
and applies rotation to the direction vector.

**TODO:** Find vtable[16] address using `monocle_dump.lua`, then decompile in IDA.

### Monocle Parameters (static addresses)

Set during Path 1 initialization:
```
72FC0E (byte)  — flag (purpose unknown)
72FC18         — monocle config struct base (passed to sub_43C880)
72FC1C (dword) — saved state (this[1])
72FC30 (float) — zoom parameter (set to 125.0)
72FC38 (float) — zoom speed (set to 4.0)
72FC4E (byte)  — flag (purpose unknown)
```

### Initialization Functions
```
sub_43C880(&unk_72FC18)  — init monocle config struct
sub_43C8D0(30.0)         — set angle limit (degrees?)
sub_43C900(8.0)          — set sensitivity
sub_43C930(0.1)          — set mouse sensitivity
```

## View Matrix Layout

Built by sub_5AA1B0 (LookAt), stored at pCamStruct+0x00:

```
Column-major layout:
+0x00 right.x    +0x04 forward.x    +0x08 up.x    +0x0C  0
+0x10 right.y    +0x14 forward.y    +0x18 up.y    +0x1C  0
+0x20 right.z    +0x24 forward.z    +0x28 up.z    +0x2C  0
+0x30 tx         +0x34 ty           +0x38 tz      +0x3C  1.0

tx/ty/tz = -dot(axis, eye_position)
```

Angle extraction:
```lua
yaw   = atan2(right.y, right.x) = atan2([+0x10], [+0x00])
pitch = atan2(forward.z, up.z)  = atan2([+0x24], [+0x28])
```

## Key Insight: ONE Mouse Rotation System

There is only ONE mouse rotation system — the **Component System**.
Both normal orbit camera AND monocle receive their rotation from it.

The monocle does NOT have its own mouse processing. It receives `a2`
(look-at target) from the pipeline, which the component system already
updated with mouse input. vtable[16] just builds a matrix from `a2`.

**The RMB gate is in the component system or input handler**, not in
the monocle. When RMB is held, a component applies mouse dx/dy to the
camera direction → `a2` changes → monocle sees rotated direction.
When RMB is not held, components don't apply mouse → `a2` stays static.

**To get mouse rotation without RMB:** find and bypass the RMB check
in the component system (or input handler that feeds the components).

## Current Freecam Hooks (v20.1)

| # | Address | Function | What our hook does |
|---|---------|----------|-------------------|
| 1 | +1299A0 | sub_5299A0 | Save game eye/target, replace with ours |
| 2 | +356F0  | sub_4356F0 | Skip camera position copy (ECX==camBase+0x388) |
| 3 | +3DA50  | sub_43DA50 | Returns 1 WITHOUT monocle flags (graceful skip) |
| 4 | +C27FD  | (inline)   | NOP monocle HUD overlay write |
| 5 | +88B69  | sub_564950 call | NOP 5 bytes (disable room culling) |
| — | Lua     | tick()     | Mouse rotation, WASD, AABB sector lookup + write to +0x788 |

**Removed in v20.1:** Hook 6 (force visibility before PostTraversal_VM) — no longer needed.
**Removed in v20.1:** All matrix hacks — AABB sector lookup replaces them.

## Open Questions

1. ~~**vtable[16] address**~~ — resolved: mouse now reads from game input buffer directly
2. ~~**Mouse input source**~~ — resolved: static addresses 0x72FC14 (yaw), 0x72FC10 (pitch)
3. **Component identification** — which component handles orbit mouse rotation?
4. **Difference between main menu and gameplay camera objects** — same vtable?
5. **sub_488970** — 2nd per-frame sector call, purpose unknown (may need patching too)
