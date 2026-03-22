# sub_52C860 -- PopObjRef

**Address:** `0x52C860` (`Spiderwick.exe+0EC860`) | **Calling convention:** cdecl

## Purpose

Pops an object reference from the Kallis VM argument stack and resolves it to an actual object pointer using the object reference base (`dword_E56160`). If the popped value is zero (null reference), the output pointer is set to NULL.

## Decompiled

```c
int __cdecl PopObjRef(_DWORD *out) {
    int ref = *(int *)(dword_E56168 - 4 * dword_E56200);
    dword_E56200--;

    if (ref != 0) {
        // Signed division by 4, then scale back — aligns to 4-byte boundary
        // Uses: cdq / and edx,3 / add eax,edx / sar eax,2
        int aligned = ref / 4;  // signed, rounds toward zero
        *out = dword_E56160 + 4 * aligned;
        return (int)out;
    } else {
        *out = 0;   // null reference
        return 0;
    }
}
```

## Disassembly

```asm
52C860  mov     ecx, dword_E56200       ; load index
52C866  mov     edx, dword_E56168       ; load base
52C86C  lea     eax, ds:0[ecx*4]        ; eax = index * 4
52C873  sub     edx, eax                ; edx = base - 4*index
52C875  mov     eax, [edx]              ; eax = popped ref value
52C877  sub     ecx, 1                  ; index--
52C87A  test    eax, eax                ; null check
52C87C  mov     dword_E56200, ecx       ; store decremented index
52C882  jz      short loc_52C89D        ; jump if null ref
; --- non-null path ---
52C884  mov     ecx, dword_E56160       ; load object ref base
52C88A  cdq                             ; sign-extend eax into edx
52C88B  and     edx, 3                  ; edx = eax<0 ? (eax%4 correction) : 0
52C88E  add     eax, edx                ; adjust for signed division
52C890  sar     eax, 2                  ; eax = ref / 4 (signed)
52C893  lea     edx, [ecx+eax*4]        ; edx = base + 4*(ref/4) = resolved ptr
52C896  mov     eax, [esp+arg_0]        ; eax = output pointer
52C89A  mov     [eax], edx              ; *out = resolved pointer
52C89C  retn
; --- null path ---
52C89D  mov     ecx, [esp+arg_0]        ; ecx = output pointer
52C8A1  mov     dword ptr [ecx], 0      ; *out = NULL
52C8A7  retn
```

## Key Addresses

| Address | Description |
|---------|-------------|
| `0x52C860` | Function entry point |
| `0xE56168` | `dword_E56168` -- arg stack base address |
| `0xE56200` | `dword_E56200` -- arg stack index |
| `0xE56160` | `dword_E56160` -- object reference resolution base |

## Notes

- The signed division by 4 (`cdq` / `and edx,3` / `add eax,edx` / `sar eax,2`) is the MSVC idiom for `eax / 4` with signed integers. This effectively aligns the reference value to a 4-byte boundary before using it as an offset into the object table.
- The resolution formula: `resolved_ptr = dword_E56160 + 4 * (ref / 4)` means the VM stores object references as byte offsets that get rounded to DWORD-aligned entries in the object table.
- The inverse operation is `PushObjRef` (sub_52CE40): `ref = *(obj + 0xA8) - dword_E56160`.
- When the reference is 0, the output is set to NULL (0) and `eax` returns 0.
- When the reference is non-null, `eax` returns the output pointer (not the resolved value).

## Called By

- Script handlers that receive object references as arguments (spawn functions, character manipulation, etc.)

## Related

- [sub_52CE40_PushObjRef.md](sub_52CE40_PushObjRef.md) -- PushObjRef (inverse operation)
- [sub_52C610_PopArg.md](sub_52C610_PopArg.md) -- PopArg (no resolution step)
- [sub_52C640_PopArgAlt.md](sub_52C640_PopArgAlt.md) -- PopArgAlt (no resolution step)
- [../VM_STACK_OPERATIONS.md](../VM_STACK_OPERATIONS.md) -- Stack operations overview
- [../KALLIS_VM.md](../KALLIS_VM.md) -- VM architecture overview
