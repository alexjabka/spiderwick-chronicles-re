# sub_5AA1B0 — BuildViewMatrix / LookAt
**Address:** `Spiderwick.exe+1AA1B0` (absolute: `005AA1B0`)

## Purpose
Builds a 4x4 view matrix from eye position, target position, and up vector.
This is the game's core camera matrix construction function.

## Signature
```c
int __thiscall sub_5AA1B0(float *this, float *eye, float *target, float *up)
// this = destination (16 floats = 4x4 matrix)
// eye  = float[3] camera position
// target = float[3] look-at position
// up = float[3] up vector (typically {0, 0, 1})
```

## Algorithm (from pseudocode)
```
1. forward = normalize(target - eye)
2. right = normalize(cross(forward, up))
3. up2 = cross(right, forward)          // recompute orthogonal up
4. Write rotation rows:
     this[0..2]  = right vector
     this[4..6]  = forward vector
     this[8..10] = up2 vector
     this[3,7,11] = 0.0
5. Write translation row:
     this[12] = -dot(right, eye)
     this[13] = -dot(forward, eye)
     this[14] = -dot(up2, eye)
     this[15] = 1.0
```

Uses `sub_4952E0` for vector normalization (called 3 times).

## Called By
- `sub_5299A0` (camera matrix rebuild wrapper)
  - Adds 0x480 to ECX before calling
  - Passes up = {0.0, 0.0, 1.0} (Z is vertical axis)

## Matrix Layout Written
```
this[0]  right.x    this[1]  fwd.x    this[2]  up.x    this[3]  0
this[4]  right.y    this[5]  fwd.y    this[6]  up.y    this[7]  0
this[8]  right.z    this[9]  fwd.z    this[10] up.z    this[11] 0
this[12] tx         this[13] ty       this[14] tz      this[15] 1
```
Where tx/ty/tz = -dot(axis, eye_position).

## Why This Matters for Freecam
Instead of manually computing the rotation matrix and translation (which
requires knowing DX9 sign conventions), we feed our eye/target positions
to this function through sub_5299A0 and let the engine build the view
matrix perfectly.

## Status: NOT HOOKED (engine runs it naturally with our data)

## Related
- Called by [sub_5299A0](sub_5299A0_ViewMatrixWrapper.md)
- Uses [sub_4952E0](sub_4952E0.md) for vector normalization
- Writes to camera view matrix at [pCamStruct](../structs/camera_struct.md)
