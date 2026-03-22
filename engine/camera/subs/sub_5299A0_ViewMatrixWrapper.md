# sub_5299A0 — Camera View Matrix Wrapper
**Address:** `Spiderwick.exe+1299A0` (absolute: `005299A0`)

## Purpose
Thin wrapper that calls `sub_5AA1B0` (LookAt/BuildViewMatrix) with
an up vector of {0, 0, 1}.

## Signature
```c
int __stdcall sub_5299A0(float *eye, float *target)
// eye = float[3] camera position (a1)
// target = float[3] look-at target (a2)
// ECX = camera object (adds +0x480 before calling sub_5AA1B0)
// Callee cleans: retn 8
```

## Pseudocode
```c
sub_5299A0(float *eye, float *target) {
    float up[] = {0.0, 0.0, 1.0};   // Z = vertical
    this += 0x480;                    // shift to view matrix location
    return sub_5AA1B0(this, eye, target, up);
}
```

## Original Bytes
```
005299A0: 83 EC 10    sub esp, 10
005299A3: D9 EE       fldz
```

## Our Hook (v9 — engine-native)
**Strategy:** Replace eye/target pointers on the stack with our own data,
then let the function run normally. The engine builds the view matrix perfectly.

```asm
fcCode:
  cmp dword ptr [fc_enabled], 1
  jne fc_normal
  mov dword ptr [esp+4], fc_eye      // replace eye pointer
  mov dword ptr [esp+8], fc_target   // replace target pointer
fc_normal:
  sub esp, 10              // original
  fldz                     // original
  jmp fc_return
```

**Result:** Engine's own LookAt function builds view matrix from our
eye/target. No manual matrix math, no sign convention issues.

## Hook Evolution
- **v6-v8:** `ret 8` (skip entirely), Lua wrote matrix manually → inverted controls
- **v9:** Replace args, let engine run → correct DX9 conventions automatically

## Status: v9 — NEEDS TESTING

## Related
- Calls [sub_5AA1B0](sub_5AA1B0_BuildViewMatrix.md) (the real BuildViewMatrix/LookAt function)
- Eye/target stored in `fcData` (fc_eye, fc_target symbols)
- Lua computes forward direction from camYaw/camPitch, writes eye + forward as target
