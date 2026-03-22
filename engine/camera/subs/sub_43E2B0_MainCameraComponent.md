# sub_43E2B0 — Main Camera Component (vtable[3])
**Address:** `Spiderwick.exe+3E2B0` (absolute: `0043E2B0`)
**Status:** FULLY REVERSED

## Purpose
The main camera processing function. This is vtable[3] of the camera
mode handler object (the same object as the "monocle" object).
Contains BOTH monocle dispatch AND orbit camera logic.

Called every frame as a camera component via the pipeline.

## Signature
```c
void __thiscall sub_43E2B0(unsigned __int8 *this, int a2, int a3)
// this = camera mode handler (monocle object, 1FD529C8)
// a2   = camera data (contains camera transform, position at +0x30..+0x38)
// a3   = output/state struct (contains delta time at +0x80, output at +0x90)
```

## High-Level Flow
```
1. Compute target position (v154) from pAnotherCamera (+0x58/5C/60)
2. Apply interpolation/smoothing to target
3. Call MonocleUpdate(this, target, output, camData)
4. IF MonocleUpdate returned 1 → done (skip all normal camera)
5. IF MonocleUpdate returned 0 → run normal camera:
     a. Mouse rotation — orbital movement of eye around target
     b. Camera-behind-player snap
     c. Interpolation animation (this->BC)
     d. Distance/height clamping
     e. Collision check
     f. vtable[16] builds final matrix from (target - eye, eye)
     g. Copy result to output
```

## MonocleUpdate Gate
```c
// Target computed from pAnotherCamera position
v154 = *(float *)(pAnotherCam + 0x58);  // target X
v155 = *(float *)(pAnotherCam + 0x5C);  // target Y
v156 = *(float *)(pAnotherCam + 0x60) + 4.0;  // target Z + height offset

// Various interpolation/smoothing applied to v154...

// Write to output state
*(float *)(a3 + 0x90) = v154;
*(float *)(a3 + 0x94) = v155;
*(float *)(a3 + 0x98) = v156;

// Call monocle dispatcher
if (!MonocleUpdate(this, &v154, a3, a2)) {
    // MonocleUpdate returned 0 → run normal camera
    // ... mouse rotation, player follow, collision ...
}
// MonocleUpdate returned 1 → skip ALL of the above
```

When MonocleUpdate returns 0, the full normal camera pipeline runs:
- Mouse rotation (reads flt_72FC10/14 for deltas)
- Player follow (orbits eye around target)
- Collision detection
- Matrix construction via vtable[16]

When MonocleUpdate returns 1, ALL of this is skipped. The rendering
pipeline continues unaffected — only camera control is bypassed.

## Mouse Delta Addresses
**Mouse input is at STATIC addresses:**
```
0x0072FC10 (flt_72FC10) — mouse delta Y (pitch/vertical)
0x0072FC14 (flt_72FC14) — mouse delta X (yaw/horizontal)
0x0072FC0C (byte)       — axis inversion flag (horizontal)
0x0072FC0D (byte)       — axis inversion flag (vertical)
```

### Mouse Rotation Code (orbit camera, when MonocleUpdate returns 0)
```c
// Check if mouse has moved
float absX = flt_72FC14 < 0 ? -flt_72FC14 : flt_72FC14;
float absY = flt_72FC10 < 0 ? -flt_72FC10 : flt_72FC10;

if (absX + absY > 0.001) {
    // Mouse input present → compute orbital rotation

    // Apply axis inversion
    float signX = byte_72FC0C ? -1.0 : 1.0;
    float signY = byte_72FC0D ? 1.0 : -1.0;  // note: inverted logic

    // Scale by sensitivity and magic number
    float horizontal = signX * flt_72FC14 * this->float_A4 * 1.8;
    float vertical   = signY * flt_72FC10 * this->float_A4;

    // Transform rotation by current camera orientation
    sub_5A7C80(a2);                          // set transform context
    result = sub_5A83C0(&output, &rotation); // transform to world space

    // Apply to eye position (orbital movement)
    float dt = *(float *)(a3 + 0x80);
    eye.x += result.x * dt;
    eye.y += result.y * dt;
    eye.z += result.z * dt;
}
```

### Key Insight: Orbit vs First-Person

The orbit camera rotates by moving the EYE position around a fixed TARGET:
```
TARGET = character position (v154, from pAnotherCamera)
EYE = computed position orbiting around target
direction = TARGET - EYE
vtable[16](output, direction, eye)  → builds matrix
```

For freecam, we want FIRST-PERSON rotation:
```
EYE = our freecam position (fixed by WASD)
direction = from camYaw/camPitch
No need for target orbiting
```

## Camera-Behind-Player Feature
```c
// Input binding for camera reset
dword_72FC58 = sub_408240(2, "INP_CAMERA_BEHIND_PLAYER");

// Checked via input handler:
if (byte_72FC0E || input_handler(pAnotherCamera, INP_CAMERA_BEHIND_PLAYER)) {
    // Snap camera behind player character
    // Uses pAnotherCamera forward direction
}
```

## Object Offsets Used
```
this+0x78 (dword): pAnotherCamera pointer
this+0x7C (float): height offset (normal)
this+0x80 (float): height offset (alternative/v7)
this+0x84 (float): Z offset for monocle
this+0x88 (float): min height clamp
this+0x8C (float): max height clamp
this+0x90 (float): scale/distance
this+0x94 (float): min distance
this+0xA0 (float): movement speed factor
this+0xA4 (float): mouse sensitivity
this+0xA8 (float): max orbit distance
this+0xC0 (float): interpolation rate
this+0xC4 (float): interpolation progress
this+0xC8 (float): interpolation speed multiplier
this+0xCC..0xD4 (float[3]): saved position
this+0xDC..0xE4 (float[3]): target interpolation point
this+0xEC (float): timer
this+0xB8 (byte): transition flag
this+0xB9 (byte): monocle active flag
this+0xBA (byte): collision result
this+0xBB (byte): monocle init flag
this+0xBC (byte): interpolation active
this+0xBD (byte): interpolation phase
this+0xF0 (byte): character-related flag
this+0xF1 (byte): override flag
```

## Related
- Calls [MonocleUpdate (sub_43DA50)](sub_43DA50_CameraModeHandler.md) — monocle dispatcher
- Calls [sub_439AF0](sub_439AF0_CameraMatrixBuilder.md) via vtable[16] — matrix builder (BOTH paths)
- Calls [sub_437850](../math/sub_437850.md) — interpolation
- Calls [sub_43DE80](sub_43DE80.md) — collision check
- Calls [sub_4356F0](../copy/sub_4356F0_PositionWriter.md) — camera struct copy (HOOKED)
- Mouse deltas at static 72FC10/72FC14
- Input binding "INP_CAMERA_BEHIND_PLAYER" via sub_408240
