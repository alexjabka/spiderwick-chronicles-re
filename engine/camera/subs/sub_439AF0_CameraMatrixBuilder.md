# sub_439AF0 — Camera Matrix Builder (vtable[16])
**Address:** `Spiderwick.exe+39AF0` (absolute: `00439AF0`)

## Purpose
Builds a camera transform matrix from a direction vector and eye position.
**Does NOT handle mouse rotation** — just constructs the matrix.

Previously assumed to be "mouse rotation handler" but decompilation shows
it only builds a basis (right/forward/up) from the input direction and
writes a 4x4 matrix. No mouse input is read.

## Signature
```c
// IDA shows __stdcall but it's __thiscall — ECX = monocle object (unused)
float *__stdcall sub_439AF0(float *output, float *direction, float *eye)
// output    = 4x4 camera matrix (16 floats, written)
// direction = look direction vector (3 floats, normalized internally)
// eye       = camera position (3 floats, copied to output[12..14])
// Returns:  eye pointer (a3)
```

## Decompiled Pseudocode
```c
sub_439AF0(float *output, float *direction, float *eye) {
    float up[3] = {0, 0, 1};
    float right[3], up2[3];

    // 1. Normalize direction in-place
    sub_4052E0(direction);          // __thiscall normalize(ECX = direction)

    // 2. Compute right = direction × up(0,0,1)
    right.x = direction.y * 1 - direction.z * 0;   // = direction.y
    right.y = direction.z * 0 - direction.x * 1;   // = -direction.x
    right.z = direction.x * 0 - direction.y * 0;   // = 0
    // Simplified: right = (dir.y, -dir.x, 0)

    // 3. Normalize right
    sub_4052E0(right);

    // 4. Compute up2 = right × direction
    up2.x = right.y * direction.z - right.z * direction.y;
    up2.y = right.z * direction.x - right.x * direction.z;
    up2.z = right.x * direction.y - right.y * direction.x;

    // 5. Normalize up2
    sub_4052E0(up2);

    // 6. Build rotation matrix from basis vectors
    sub_5A8260(right, direction, up2);  // writes 3x3 rotation to output
    // sub_5A8260 is __stdcall(_DWORD, _DWORD, _DWORD)
    // Likely __thiscall with ECX = output buffer (IDA missed it)

    // 7. Write eye position to translation row
    output[12] = eye[0];    // position X
    output[13] = eye[1];    // position Y
    output[14] = eye[2];    // position Z

    return eye;
}
```

## Key Finding: NO MOUSE ROTATION HERE

This function is purely mathematical:
1. Takes direction → builds orthonormal basis (right, forward, up)
2. Writes rotation matrix via sub_5A8260
3. Copies eye position to translation

**Mouse rotation happens UPSTREAM** — the `direction` parameter already
contains the rotated direction when mouse is active. Direction comes from
sub_43DA50 Path 3: `direction = a2 - eye`, where `a2` is the look-at
target passed from the camera pipeline (sub_436190).

## Helper Functions
```
sub_4052E0 — __thiscall vector normalize (ECX = float[3] pointer)
sub_5A8260 — build rotation matrix from 3 basis vectors
             __stdcall(right, forward, up) or __thiscall(ECX=output)
```

## VTable Context
```
vtable[16] (offset +0x40) = 00439AF0  ← this function (LAST vtable entry)
vtable has exactly 17 entries (0-16)
Data after vtable: float 1.8, ASCII "Bard Ler..." (class name / static data)
```

## Object Info (from monocle_dump.lua, runtime)
```
this       = 1FD529C8  (monocle object, NOT camera manager)
vtable     = 0062818C  (Spiderwick.exe+22818C, .rdata section)
```

## Related
- Called from [sub_43DA50](sub_43DA50_CameraModeHandler.md) Path 3
- Output saved to this+0x38 (internal matrix) and a3 (pipeline output)
- [sub_4052E0](../math/sub_4052E0.md) — vector normalize
- [sub_5A8260](../math/sub_5A8260.md) — build rotation matrix from basis
