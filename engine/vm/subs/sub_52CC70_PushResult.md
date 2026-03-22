# sub_52CC70 -- PushResult

**Address:** `0x52CC70` (`Spiderwick.exe+0ECC70`) | **Calling convention:** cdecl

## Purpose

Pushes a boolean result (0 or 1) onto the Kallis VM **return stack**. Used by native script handlers to return success/failure status to the calling script. The input is coerced to a strict boolean: any non-zero byte becomes 1.

## Decompiled

```c
BOOL __cdecl PushResult(char value) {
    BOOL result = (value != 0);      // setnz: coerce to 0 or 1
    *(int *)dword_E5616C = result;   // write to return stack
    dword_E5616C += 4;               // advance pointer
    return result;
}
```

## Disassembly

```asm
52CC70  mov     ecx, dword_E5616C       ; load return stack pointer
52CC76  xor     eax, eax                ; eax = 0
52CC78  cmp     [esp+arg_0], al         ; compare input byte to 0
52CC7C  setnz   al                      ; eax = (input != 0) ? 1 : 0
52CC7F  mov     [ecx], eax              ; *return_stack_ptr = result
52CC81  add     dword_E5616C, 4         ; advance return stack pointer
52CC88  retn
```

## Key Addresses

| Address | Description |
|---------|-------------|
| `0x52CC70` | Function entry point |
| `0xE5616C` | `dword_E5616C` -- return stack pointer (grows upward) |

## Notes

- The return stack grows **upward**: value is written at the current pointer, then the pointer is incremented by 4.
- The input parameter is a `char` (single byte), not an `int`. The `setnz` instruction ensures the output is strictly 0 or 1 regardless of the input byte value.
- This is the standard way for script handlers to return a boolean result to Kallis scripts.
- Contrast with `PushObjRef` (sub_52CE40) which also pushes to the return stack but handles object references.
- Many handlers call `PushResult(1)` after successful execution (e.g., after spawning an object).

## Called By

- Widely used across native script handlers to indicate operation success/failure
- Example: spawn functions push `1` after successful object creation

## Related

- [sub_52CE40_PushObjRef.md](sub_52CE40_PushObjRef.md) -- PushObjRef (also pushes to return stack)
- [sub_52C610_PopArg.md](sub_52C610_PopArg.md) -- PopArg (reads from arg stack)
- [sub_52C640_PopArgAlt.md](sub_52C640_PopArgAlt.md) -- PopArgAlt (reads from arg stack)
- [../VM_STACK_OPERATIONS.md](../VM_STACK_OPERATIONS.md) -- Stack operations overview
- [../KALLIS_VM.md](../KALLIS_VM.md) -- VM architecture overview
