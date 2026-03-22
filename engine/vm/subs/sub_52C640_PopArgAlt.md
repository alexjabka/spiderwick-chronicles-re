# sub_52C640 -- PopArgAlt

**Address:** `0x52C640` (`Spiderwick.exe+0EC640`) | **Calling convention:** cdecl

## Purpose

Pops a value from the Kallis VM argument stack and writes it to an output pointer. This is an inline variant of PopArg (sub_52C610) -- the stack manipulation logic is directly in the function body rather than via a thunk. Unlike PopArg, this function does NOT return the popped value in `eax`; it returns the output pointer instead.

## Decompiled

```c
_DWORD* __cdecl PopArgAlt(_DWORD *out) {
    int value = *(int *)(dword_E56168 - 4 * dword_E56200);
    dword_E56200--;
    *out = value;
    return out;
}
```

## Disassembly

```asm
52C640  mov     eax, dword_E56200       ; load current index
52C645  mov     edx, dword_E56168       ; load base address
52C64B  lea     ecx, ds:0[eax*4]        ; ecx = index * 4
52C652  sub     edx, ecx                ; edx = base - 4*index (slot address)
52C654  mov     ecx, [edx]              ; ecx = value at slot
52C656  sub     eax, 1                  ; index--
52C659  mov     dword_E56200, eax       ; store decremented index
52C65E  mov     eax, [esp+arg_0]        ; eax = output pointer
52C662  mov     [eax], ecx              ; *out = value
52C664  retn
```

## Key Addresses

| Address | Description |
|---------|-------------|
| `0x52C640` | Function entry point |
| `0xE56168` | `dword_E56168` -- arg stack base address |
| `0xE56200` | `dword_E56200` -- arg stack index |

## Notes

- Only 1 known caller: `sub_4626B0` (sauSetPlayerType handler on ClPlayerObj).
- The difference from PopArg (sub_52C610) is subtle but important:
  - **PopArg** returns the popped value in `eax` (useful for direct use in expressions)
  - **PopArgAlt** returns the output pointer in `eax` (useful for pass-by-reference patterns)
- Both perform identical stack operations: read `*(base - 4*idx)`, then decrement `idx`.
- Used in `__thiscall` method handlers where the popped value is passed to a virtual method via a local variable.

## Called By

- `sub_4626B0` (sauSetPlayerType handler) -- pops player type, calls vtable method at +464

## Related

- [sub_52C610_PopArg.md](sub_52C610_PopArg.md) -- PopArg (thunk variant, returns value)
- [sub_52C860_PopObjRef.md](sub_52C860_PopObjRef.md) -- PopObjRef (pop + resolve)
- [sub_52CC70_PushResult.md](sub_52CC70_PushResult.md) -- PushResult (return stack push)
- [../VM_STACK_OPERATIONS.md](../VM_STACK_OPERATIONS.md) -- Stack operations overview
- [../KALLIS_VM.md](../KALLIS_VM.md) -- VM architecture overview
