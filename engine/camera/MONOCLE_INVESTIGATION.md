# Monocle Mouse Rotation — Investigation Plan
**Goal:** Get mouse rotation working WITHOUT holding RMB
**Last updated:** 2026-03-17 (v20 final conclusion)

## FINAL CONCLUSION (v20 session)

**Monocle mode is the ROOT CAUSE of rendering failure in freecam.**

When freecam forces monocle mode (v15-v17), `camera_obj+0x78C` flags = 0x0.
The native block in CameraTick (0x439419) checks `test al, 20h` — when flags=0,
the ENTIRE sector tracking + SectorTransition block is SKIPPED. This means:
- Player position stamp at 0x439449 never fires
- Sector index at +0x788 never updates
- Portal traversal starts from wrong sector
- Rooms disappear

**Freecam v20 removes monocle entirely.** Hook 3 (sub_43DA50) returns 1 WITHOUT
setting any monocle flags (0xB8, 0xB9, 0xBB all stay clear). This gives us the
orbit-camera-skip behavior we need without the monocle side-effects that block
sector tracking. Combined with Lua AABB sector lookup writing directly to +0x788,
rooms now render correctly.

## Current Understanding (after decompilation)

**vtable[16] (sub_439AF0) does NOT handle mouse rotation.**
It's a pure math function: takes direction + eye → builds camera matrix.
No mouse input is read inside it.

**Mouse rotation changes `a2`** — the look-at target parameter passed
to sub_43DA50 from the camera pipeline (sub_436190). When RMB is held,
the component system updates `a2` based on mouse input. When RMB is
not held, `a2` stays static.

**The monocle is just a matrix builder.** It computes `direction = a2 - eye`
and builds a camera transform. All rotation comes from whatever feeds `a2`.

## Call Chain (confirmed)

```
sub_436190 (camera pipeline manager)
  │
  ├── Component processing loop
  │     component->vtable[11](deltaTime)    // update
  │     component->vtable[3](state, buf)     // process  ← MOUSE ROTATION HERE?
  │     component->vtable[4]()               // finalize
  │
  ├── ??? code that computes a2 (look-at target) ???
  │
  ├── call sub_43DA50(mono_obj, a2, output, camData)  // at +3E540
  │     └── Path 3: dir = a2 - eye; vtable[16](dir, eye) → matrix
  │
  ├── test al, al / jne +3ED9C    // skip orbit if monocle returned 1
  │
  └── sub_5299A0(eye, target)      // build view matrix ← HOOKED
```

## What We Still Need To Find

### Priority 1: Where does `a2` come from?

Look at code BEFORE the call at +3E540 in sub_436190.
`a2` is a float[3] pushed as the second arg to sub_43DA50.
It's the look-at target position. When RMB is held, mouse rotation
changes this position.

**How to find:** In IDA, go to `0x0043E540`, look at ~30 instructions
before the call. Find the `push` or `lea` that provides the second arg.
Trace it backward to its source.

### Priority 2: Which component handles mouse rotation?

The component system at camManager+0x88 processes input each frame.
One component applies mouse dx/dy to the camera direction, but only
when RMB is held.

**How to find:**
1. Dump component vtable addresses (monocle_dump.lua — need to fix
   to read from camera manager, not monocle object)
2. Decompile each component's vtable[3] (process function)
3. Find the one that reads mouse input and checks for RMB

### Priority 3: RMB gate location

Somewhere in the input pipeline, there's a check like:
```c
if (isButtonPressed(RMB)) {
    apply_mouse_to_camera(dx, dy);
}
```
Finding and bypassing this is the goal.

## Approaches To Fix (updated)

### Option A: Bypass the RMB gate (PREFERRED)
Find the conditional that checks RMB before applying mouse rotation.
NOP or bypass it when fc_enabled. Mouse always rotates camera.

### Option B: Feed `a2` ourselves
Instead of letting the pipeline compute `a2`, write our own `a2` value
that includes mouse rotation. We'd hook the call at +3E540 and replace
the `a2` argument with our computed look-at target.

### Option C: Read mouse in Lua directly
Skip the engine entirely. Read mouse dx/dy via Windows API or game's
internal buffer. Apply to camYaw/camPitch in Lua.

### Option D: Hook the component
Find the mouse rotation component, hook its process function (vtable[3]),
force it to always run regardless of RMB state.

## What We Ruled Out

- ~~vtable[16] reads mouse input~~ — WRONG. It's just a matrix builder.
- ~~Monocle has its own mouse system~~ — WRONG. Mouse comes from upstream.
- ~~Forcing monocle flags enables mouse~~ — WRONG. Flags only select
  the code path, they don't affect input processing.

## BREAKTHROUGH: Mouse Deltas Found (sub_43E2B0)

sub_43E2B0 (vtable[3]) is the main camera component. It contains
BOTH monocle dispatch AND orbit camera. Decompilation revealed:

**Mouse deltas at STATIC addresses:**
```
0x0072FC10 — flt_72FC10 — mouse delta Y (pitch)
0x0072FC14 — flt_72FC14 — mouse delta X (yaw)
0x0072FC0C — byte       — axis inversion (horizontal)
0x0072FC0D — byte       — axis inversion (vertical)
```

These are read by the orbit camera code (NOT monocle). The orbit camera
adds these deltas to the eye position to orbit around the character.

**For our freecam:** Read these values in Lua, apply as yaw/pitch
rotation instead of orbital movement. No monocle needed for rotation.

### Next Step: Verify Input

Need to check: are flt_72FC10/14 written when RMB is NOT held?
- If YES → read them directly, always have mouse rotation
- If NO → find the RMB gate in the input writer and bypass it

Use CE "Find what writes to address" on 0x0072FC14 while
moving mouse with and without RMB.

## Key Addresses

| Address | Function | Status |
|---------|----------|--------|
| 0x0072FC10 | Mouse delta Y (pitch) | **FOUND — static float** |
| 0x0072FC14 | Mouse delta X (yaw) | **FOUND — static float** |
| 0x0072FC0C | Axis inversion X | **FOUND — static byte** |
| 0x0072FC0D | Axis inversion Y | **FOUND — static byte** |
| Spiderwick.exe+3E2B0 | sub_43E2B0 — main camera component | **DECOMPILED** |
| Spiderwick.exe+39AF0 | vtable[16] — matrix builder | **DECOMPILED** — no mouse |
| Spiderwick.exe+3DA50 | sub_43DA50 — monocle dispatcher | **DECOMPILED** — no mouse |
| Spiderwick.exe+36190 | sub_436190 — camera pipeline | Parent caller |
| 72FC18 | Monocle config struct | Static, known |

## Diagnostic Tools

| Tool | Purpose |
|------|---------|
| `tools/monocle_dump.lua` | Dump monocle object, vtable, components |
| F6 key (freecam) | Show game direction + position |
