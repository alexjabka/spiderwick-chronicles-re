# sub_4356F0 — Camera Struct Block Copy (Position Writer)
**Address:** `Spiderwick.exe+356F0` (absolute: `004356F0`)

## Purpose
Copies a ~0xA2 byte block from source to destination struct.
When called with `ECX = camBase + 0x388`, this is the function that
**overwrites camera position** (+3B8/+3BC/+3C0) every frame.

**This is the caller we were looking for.** Found via `find_caller.lua` diagnostic.

## Signature
```c
void __thiscall sub_4356F0(void *this, void *source)
// ECX = this = destination
// arg_0 = source (callee cleans, retn 4)
```

## How Found
1. CE "Find what writes" on `[pCamStruct]+3B8` → found `005A7DF7 fstp [eax-14]` in sub_5A7DC0
2. sub_5A7DC0 has 187 callers (too many to check manually)
3. Used `find_caller.lua` diagnostic hook: temporarily hooks sub_5A7DC0 entry,
   checks if ECX is in camera position range, logs return address
4. Result: return address `004356FE` → call at `004356F9` → inside `sub_4356F0`

## Assembly
```asm
sub_4356F0:
  push    esi
  push    edi
  mov     edi, [esp+0Ch]        // edi = source (arg_0)
  push    edi                    // arg for sub_5A7DC0
  mov     esi, ecx               // esi = destination (this)
  call    sub_5A7DC0             // Copy 16 floats: this+0x00..0x3C   ← WRITES POSITION

  lea     eax, [edi+40h]
  push    eax
  lea     ecx, [esi+40h]
  call    sub_5A7DC0             // Copy 16 floats: this+0x40..0x7C

  fld/fstp pairs:               // Copy individual floats: this+0x80..0x98
  mov cl/dl:                    // Copy individual bytes:  this+0xA0..0xA1

  pop     edi
  pop     esi
  retn    4
```

## Write Map (when this = camBase + 0x388)
```
camBase+0x388..0x3C4  ← First sub_5A7DC0 call (includes position X/Y/Z!)
camBase+0x3C8..0x404  ← Second sub_5A7DC0 call
camBase+0x408..0x420  ← Individual float copies
camBase+0x428..0x429  ← Individual byte copies
```

## Original Bytes
```
004356F0: 56              push esi
004356F1: 57              push edi
004356F2: 8B 7C 24 0C     mov edi, [esp+0Ch]
```
Total: 6 bytes (5 for jmp + 1 nop)

## Our Hook
**Strategy:** Skip entire function when `fc_enabled == 1` AND `ECX == camBase + 0x388`.

```asm
posHook:
  cmp dword ptr [fc_enabled], 1
  jne pos_original
  push eax
  mov eax, [pCamStruct]
  add eax, 0x388
  cmp ecx, eax               // is this the camera position copy?
  pop eax
  jne pos_original
  ret 4                       // skip camera copy, freecam has control
pos_original:
  push esi
  push edi
  mov edi, [esp+0Ch]
  jmp pos_return
```

**Why this is safe:**
- Only skips when ECX == camBase + 0x388 (exact match, not range)
- Other callers of sub_4356F0 with different ECX values pass through normally
- Unlike the failed sub_5A7DC0 hook, this doesn't affect generic memcpy calls
- sub_4356F0 is camera-struct-specific, not a generic utility function

## Status: HOOKED (v8) — NEEDS TESTING

## Related
- Contains calls to [sub_5A7DC0](sub_5A7DC0_Memcpy16Floats.md) (generic 16-float memcpy)
- Writes position in [camera_struct.md](../structs/camera_struct.md) at +3B8/+3BC/+3C0
- Found by [find_caller.lua](../find_caller.lua) diagnostic tool
