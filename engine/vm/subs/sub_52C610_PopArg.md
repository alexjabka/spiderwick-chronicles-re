# sub_52C610 -- PopArg

**Address:** `0x52C610` (`Spiderwick.exe+0EC610`) | **Calling convention:** cdecl

## Purpose

Pops a value from the Kallis VM argument stack. This is a single-instruction thunk (`jmp sub_4CE3CD`) where the actual logic resides at sub_4CE3CD. The popped value is both written to the output pointer parameter and returned in `eax`.

## Decompiled

```c
// sub_52C610 — thunk
int __cdecl PopArg(int *out) {
    return sub_4CE3CD(out);
}

// sub_4CE3CD — actual implementation
int __cdecl PopArg_Impl(int *out) {
    int *slot = (int *)(dword_E56168 - 4 * dword_E56200);
    int value = *slot;
    *out = value;
    dword_E56200--;
    return value;
}
```

## Disassembly

```asm
; sub_52C610 (thunk)
52C610  jmp     sub_4CE3CD

; sub_4CE3CD (implementation, mapped into sub_52C610's address range by IDA)
52C622  mov     edx, dword_E56168       ; base address
        sub     edx, [4 * dword_E56200] ; compute slot
52C624  mov     eax, [edx]              ; read value
52C62A  mov     [out], eax              ; write to output
52C62C  dec     dword_E56200            ; decrement index
52C633  retn
```

## Key Addresses

| Address | Description |
|---------|-------------|
| `0x52C610` | Thunk entry point (jmp to 0x4CE3CD) |
| `0x4CE3CD` | Actual implementation |
| `0xE56168` | `dword_E56168` -- arg stack base address |
| `0xE56200` | `dword_E56200` -- arg stack index |

## Notes

- This is the primary pop function used by most script handlers (e.g., `sauResolvePlayer` at 0x493A80).
- The thunk pattern (`jmp sub_4CE3CD`) means IDA may show the implementation code under sub_4CE3CD's address even though it logically belongs to PopArg.
- Compare with `sub_52C640` (PopArgAlt) which has identical stack behavior but inlined implementation and does NOT return the value in `eax`.
- Has 10+ xrefs -- widely used across native script handlers.

## Called By

- `sub_493A80` (sauResolvePlayer handler) -- pops player type argument
- `sub_40D670`, `sub_40D710`, `sub_40E900`, `sub_40F740` -- various handlers
- `sub_424460`, `sub_444E00`, `sub_4450C0` -- various handlers
- Many others (10+ callers, truncated)

## Related

- [sub_52C640_PopArgAlt.md](sub_52C640_PopArgAlt.md) -- PopArgAlt (inline variant)
- [sub_52C860_PopObjRef.md](sub_52C860_PopObjRef.md) -- PopObjRef (pop + resolve)
- [sub_52CC70_PushResult.md](sub_52CC70_PushResult.md) -- PushResult (return stack push)
- [../VM_STACK_OPERATIONS.md](../VM_STACK_OPERATIONS.md) -- Stack operations overview
- [../KALLIS_VM.md](../KALLIS_VM.md) -- VM architecture overview
