# Approach: Work WITH the Engine, Not Against It

## Principle

Never fight the game's rendering pipeline. Instead, understand how the engine
builds its camera state and feed it OUR data through its own systems.

The engine knows its own DX9 conventions, coordinate handedness, matrix layout,
sign conventions, and rendering pipeline order. If we let it build the view
matrix from our inputs, everything works naturally. If we try to replicate
its math externally, we get sign errors, inverted controls, broken rendering.

## What This Means in Practice

**BAD: Writing raw matrix values from Lua**
- Manually computing rotation matrix → sign guessing, convention mismatch
- Manually computing view translation (t = -R*p) → wrong signs for DX9
- Skipping engine functions and replacing with our math → fragile, incomplete

**GOOD: Replacing engine INPUTS and letting engine functions run**
- Find what data the engine reads (eye position, target position, up vector)
- Replace that data with our values
- Let the engine's own functions build the view matrix

## Lessons Learned

### v7: Hooking generic memcpy (sub_5A7DC0) → BROKE RENDERING
Blocked ALL copies to camera struct. The engine couldn't update view/projection
data it needed for rendering. Screen stretched, then went blank.

**Lesson:** Generic utility functions serve many purposes. Never block them broadly.

### v8: Hooking specific caller (sub_4356F0) → MOVEMENT WORKS but INVERTED
Found the exact caller via diagnostic script. Camera moves, but forward=backward,
left=right. Because we computed view matrix translation manually with wrong
sign convention for DX9.

**Lesson:** Don't replicate engine math. You'll get sign conventions wrong.

### v9 (planned): Feed data to sub_5AA1B0 → LET ENGINE BUILD MATRIX
Discovered sub_5299A0 is a thin wrapper that calls sub_5AA1B0 (the real
LookAt/BuildViewMatrix function) with eye, target, and up={0,0,1}.

Instead of skipping sub_5299A0, we replace its input arguments (eye/target
pointers) with our own position and computed target, then let it run normally.
The engine builds a perfect view matrix with correct DX9 conventions.

**Lesson:** This is the right way. Understand the pipeline, intercept at the
INPUT level, let the engine do the heavy lifting.

## Architecture Rule

```
USER INPUT  →  our Lua/ASM updates position & angles
                       ↓
ENGINE INPUTS  →  we write eye/target to where the engine reads them
                       ↓
ENGINE PIPELINE  →  sub_5AA1B0 builds view matrix (UNTOUCHED)
                       ↓
RENDERER  →  reads view matrix, renders frame (UNTOUCHED)
```

We only touch the top layer. Everything below runs as the developers intended.

## v10-v14: Monocle Forcing — What Worked, What Didn't

### v10: Monocle with RMB held → WORKS
When user holds RMB, the game naturally sets monocle flags. The monocle
code path runs with real mouse input. Our shifted-target approach preserves
the monocle's rotation while changing position. Mouse rotation works perfectly.

### v13: Force monocle flag → PARTIAL (crashes + no mouse without RMB)
Set `[ecx+0xB9]=1` to force monocle Path 3 every frame. Two problems:
1. **Crash:** Missing Path 1 initialization (vtable[16] uses uninitialized data)
2. **Extra NOP byte:** Corrupted instruction at +3DA55

### v14: Fix crash + delta tracking → PARTIAL (RMB required)
Fixed crash (init + NOP removal). Added game direction delta capture.
But forced monocle without RMB produces "noise" — vtable[16] runs but
without real mouse input, its output drifts/jitters. Delta capture picks
up this noise → camera spins during WASD.

**Fix:** Only apply delta when RMB held. Works but requires RMB for mouse.

### Lesson: Forcing the flag is not enough
The monocle FLAG (`this+0xB9`) only tells sub_43DA50 to enter Path 3.
Path 3 calls vtable[16] which reads mouse input from *somewhere*.
When RMB isn't held, the game's input handler doesn't feed mouse data
to wherever vtable[16] reads it → output is noise/stale.

### Next approach: Understand vtable[16]'s input source
To get mouse rotation without RMB, we need one of:
1. **Find where vtable[16] reads mouse dx/dy** and force-feed it mouse data
2. **Find the input handler that gates mouse data** and bypass the RMB check
3. **Skip monocle entirely** and read raw mouse deltas in Lua, applying
   them directly to camYaw/camPitch (requires DirectInput or WM_INPUT hook)
4. **Use the component system** instead of monocle — identify which
   component handles orbit mouse rotation and feed it our data

Option 2 is likely simplest — find the "if RMB pressed, send mouse to monocle"
check and NOP/bypass it. Use `monocle_dump.lua` → decompile vtable[16] in IDA
→ trace the mouse input read → find the gate.

## General RE Approach

When encountering a game function, follow this order:
1. **Identify** — what does it do? (CE + IDA decompile)
2. **Map inputs** — what arguments/data does it read?
3. **Map outputs** — what does it write/return?
4. **Find the gate** — what condition controls whether it runs?
5. **Hook at the input level** — replace inputs, let function run naturally
6. **Never hook at output level** — don't overwrite results, don't skip functions
