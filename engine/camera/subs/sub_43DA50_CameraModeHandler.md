# sub_43DA50 — MonocleUpdate
**Address:** `Spiderwick.exe+3DA50` (absolute: `0043DA50`)
**Status:** FULLY REVERSED

## Purpose
Monocle camera mode update function. Called every frame from
MainCameraComponent (sub_43E2B0). Checks monocle flags and either
initializes monocle mode, computes monocle rotation, copies position
data, or returns 0 to let the orbit camera run.

## Signature
```c
char __thiscall MonocleUpdate(void *this, float *a2, char *a3, int a4)
// this = monocle object (NOT camera manager — separate object)
// a2   = look-at target position (float[3]) — from pipeline
// a3   = output buffer (char/float, 64+ bytes) — receives camera transform
// a4   = camera data source (int/pointer) — copied to this+0x38 and camera struct
// Returns: 1 = monocle handled camera (skip orbit), 0 = normal mode
// Cleanup: retn 0Ch (3 stack args, thiscall)
```

## Full Decompiled Pseudocode
```c
char __thiscall MonocleUpdate(void *this, float *a2, char *a3, int a4) {
    char result;

    if (this->byte_BB) {
        // === PATH 1: INITIALIZATION (first frame of monocle) ===
        // Copies position, sets FOV and speed parameters
        dword_72FC1C = this->dword_04;           // save state
        sub_5A7DC0(this + 0x38, a4);             // copy camera data to internal state
        sub_4356F0(a4);                           // copy to camera struct
        sub_43C880(&unk_72FC18);                  // init monocle config struct
        sub_43C8D0(30.0);                         // FOV = 30 degrees
        flt_72FC30 = 125.0;                       // speed = 125
        flt_72FC38 = 4.0;                         // zoom speed
        sub_43C900(8.0);                          // sensitivity
        sub_43C930(0.1);                          // mouse sensitivity
        byte_72FC4E = 0;                          // clear flag
        result = 1;
        // Falls through to clear flags and return 1
    }
    else {
        if (!this->byte_B8 && this->byte_B9) {
            // === PATH 2: ROTATION (B9 set, B8 NOT set) ===
            // vtable[16] computes orientation from mouse
            byte_72FC0E = 0;
            float *v6 = (float *)this->dword_78;  // pAnotherCamera
            this->byte_BD = 0;
            this->byte_BC = 0;

            // Get vtable[16] function pointer
            auto v7 = vtable[0x40/4];  // = sub_439AF0

            // Compute monocle eye position from pAnotherCamera
            float scale = this->float_90;
            float eyeX = v6[14]*scale + v6[22];   // pAnother+0x38 * scale + pAnother+0x58
            float eyeY = v6[15]*scale + v6[23];   // pAnother+0x3C * scale + pAnother+0x5C
            float eyeZ = v6[24] + this->float_84; // pAnother+0x60 + Z_offset

            // Compute direction = target - eye
            float dir[3];
            dir[0] = a2[0] - eyeX;
            dir[1] = a2[1] - eyeY;
            dir[2] = a2[2] - eyeZ;

            // Build camera matrix from direction + eye
            char output[64];
            float eye[3] = {eyeX, eyeY, eyeZ};
            v7(this, output, dir, eye);   // vtable[16] = sub_439AF0

            // Copy result to pipeline output
            sub_5A7DC0(a3, output);

            // Clear flags
            this->byte_B9 = 0;
            this->byte_BB = 0;

            // Save result to internal state
            sub_5A7DC0(this + 0x38, output);
            return 1;
        }
        else if (this->byte_B8 && this->byte_B9) {
            // === PATH 3: COPY (both B8 and B9 set) ===
            // Just copies position data, no rotation computation
            sub_5A7DC0(this + 0x38, a4);     // copy camera data
            sub_4356F0(a4);                   // copy to camera struct
        }
        result = 0;
    }
    // All paths clear flags at end
    this->byte_BB = 0;
    this->byte_B8 = 0;
    this->byte_B9 = 0;
    return result;
}
```

## Path Summary

| Path | Condition | What it does | Returns |
|------|-----------|-------------|---------|
| 1 (Init) | this+0xBB | Copy position, set FOV=30, speed=125, etc. | 1 |
| 2 (Rotation) | this+0xB9, NOT 0xB8 | vtable[16] computes orientation from mouse | 1 |
| 3 (Copy) | this+0xB8 AND 0xB9 | Just copies position data | 0 |
| 0 (None) | no flags | Nothing | 0 |

All paths clear flags (BB, B8, B9) at end. Flags are re-set each frame by input handler.

## Freecam v20 Approach
Return 1 WITHOUT setting any flags. This causes MainCameraComponent (sub_43E2B0)
to skip normal camera processing (mouse rotation, player follow, collision) while
keeping the rendering pipeline intact.

## Mode Selection Flags
```
this+0xB8 (byte): transition/copy state
this+0xB9 (byte): monocle ACTIVE (rotation mode)
this+0xBB (byte): monocle INITIALIZATION (first frame)
this+0xBC (byte): cleared in Path 2
this+0xBD (byte): cleared in Path 2
```

## Pipeline Context
```asm
+3E540: call MonocleUpdate           // camera mode handler
+3E545: test al, al
+3E547: jne +3ED9C                   // if returned 1, skip orbit camera
```

## Monocle Parameters (static, set in Path 1)
```
72FC0E (byte)  — flag (cleared in Path 2)
72FC18         — monocle config struct (initialized by sub_43C880)
72FC1C (dword) — saved this[1] state
72FC30 (float) — speed parameter (125.0)
72FC38 (float) — zoom speed (4.0)
72FC4E (byte)  — flag (cleared in Path 1)
```

## Related
- Called from [sub_43E2B0](sub_43E2B0_MainCameraComponent.md) (MainCameraComponent)
- Calls [sub_439AF0](sub_439AF0_CameraMatrixBuilder.md) (vtable[16]) — matrix builder
- Calls [sub_5A7DC0](../copy/sub_5A7DC0_Memcpy16Floats.md) — 16-float memcpy
- Calls [sub_4356F0](../copy/sub_4356F0_PositionWriter.md) — camera struct copy (HOOKED)
- Monocle init: sub_43C880, sub_43C8D0, sub_43C900, sub_43C930
