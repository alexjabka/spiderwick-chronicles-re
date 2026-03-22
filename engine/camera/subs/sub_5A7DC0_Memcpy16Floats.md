# sub_5A7DC0 — Generic 16-Float Memcpy
**Address:** `Spiderwick.exe+1A7DC0` (absolute: `005A7DC0`)

## Purpose
Copies 16 floats (64 bytes) from source to destination.
Used by many game systems — not camera-specific.
Called ~5500 times during brief measurement period.

## Signature
```c
float* __thiscall sub_5A7DC0(char *this, int a2)
// ECX = this = destination struct
// a2  = source data (stack arg, cleaned by callee)
// Returns: pointer past last written float
```

## Pseudocode (IDA)
```c
float *__thiscall sub_5A7DC0(char *this, int a2)
{
    int v2 = a2 + 0xC;
    float *result = (float *)(this + 4);
    int v4 = a2 - (DWORD)this;
    int v5 = 2;                          // 2 iterations
    do {
        result += 8;                     // advance 32 bytes
        result[-9] = *(float *)(v2 - 0xC);      // this+0x00
        result[-8] = *(float *)((char *)result + v4 - 0x20);
        result[-7] = *(float *)(v2 - 0x24);
        result[-6] = *(float *)(v2 - 0x20);
        result[-5] = *(float *)(v2 - 0x1C);
        result[-4] = *(float *)(v2 - 0x18);
        result[-3] = *(float *)(v2 - 0x14);     // <-- 005A7DF7 fstp [eax-14]
        result[-2] = *(float *)(v2 - 0x10);
        v2 += 0x20;
        v5--;
    } while (v5);
    return result;
}
```

## Write Range
Per call: writes 16 floats to `this+0x00` through `this+0x3C` (64 bytes).

## Original Bytes
```
005A7DC0: 56              push esi
005A7DC1: 8B 74 24 08     mov esi, [esp+8]
005A7DC5: 8D 56 0C        lea edx, [esi+0Ch]
005A7DC8: 8D 41 04        lea eax, [ecx+4]
005A7DCB: 2B F1           sub esi, ecx
005A7DCD: B9 02 00 00 00  mov ecx, 2
```

## Key Instruction (position write)
```
005A7DF7: D9 58 EC    fstp dword ptr [eax-14]    // result[-3]
```
CE "Find what writes" confirms this is the ONLY instruction writing to
camera position `[pCamStruct]+3B8` during freecam.

## Register Snapshot (at 005A7DF7, writing camera position)
```
EAX = camBase + 0x3CC    (21490624 when camBase=21490258)
EBX = 76F9E802
ECX = 00000000            (loop counter exhausted → last iteration)
EDX = 024FEF50            (source data, on stack)
ESI = E106E924            (computed: a2 - this)
EDI = 024FEF04
ESP = 024FEED4
```

## Failed Hook Attempt (v7)
Hooked function entry, checked if ECX (dest) within 0x400 of camBase → `ret 4`.

**Result: BROKE RENDERING.** Screen stretched, then world disappeared.

**Root cause:** Function is called for ALL game objects. Blocking camera
copies also blocked view/projection matrix updates needed by the renderer.
The 0x400 range was too broad — it caught matrix copies at camBase+0x00
that are essential for rendering, not just position copies at camBase+0x384.

## Correct Approach
Do NOT hook this generic function. Instead:
1. Find the **specific caller** that passes camera position area as ECX
2. Hook or NOP **that call instruction** specifically

### How to find the caller:
- **IDA:** Press X on `sub_5A7DC0` → list of xrefs → find the one that sets ECX to camera struct
- **CE:** "More information" → look at stack for return address (need [ESP+4] at function entry)

## Status: NOT HOOKED (reverted)
Waiting for caller identification.

## Related
- Writes to position area: see [camera_struct.md](../structs/camera_struct.md) offsets +3B8/+3BC/+3C0
- Called from unknown caller (TODO: find via IDA xrefs)
