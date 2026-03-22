# sub_52D1F0 -- VM Return Handler

| Field | Value |
|-------|-------|
| Address | 0x52D1F0 |
| Size | 74 bytes (0x4A) |
| Prototype | `int __cdecl VMReturnHandler(int popCount, int returnCount)` |
| Called by | VMInterpreter (0x52D9C0) -- opcodes RET, CALL_SCRIPT through CALL_OBJ |
| Category | VM Control Flow |

---

## Purpose

Handles function return cleanup in the VM. Pops `popCount` argument values from the frame, copies `returnCount` return values to the caller's stack, and restores the previous call frame.

---

## Pseudocode

```c
int __cdecl VMReturnHandler(int popCount, int returnCount) {
    int src = E5616C - 4 * returnCount;      // Where return values currently are
    int newBase = E56168 - 4 * popCount;      // New stack top after popping args
    E5616C = newBase;                          // Set stack top

    int savedFrame = E56168;
    E56168 = *E56168;                          // Restore previous frame (follow linked list)

    memcpy(newBase, src, 4 * returnCount);    // Copy return values down
    E5616C += 4 * returnCount;                // Advance past copied values

    return result;
}
```

---

## Stack Layout

Before return:
```
[frame link] [args...] [locals...] [return values] <- E5616C
^                                   ^
E56168                              src = E5616C - 4*retCount
```

After return:
```
[previous frame data] [return values] <- E5616C
^
newBase = old E56168 - 4*popCount
```

---

## Called From

- RET opcode (0x00): tail-called via `jmp sub_52D1F0`
- CALL_SCRIPT (0x01): after recursive interpreter call
- CALL_NATIVE (0x02): after sub_52D240
- CALL_METHOD (0x03): after sub_52D280
- CALL_STATIC (0x04): after sub_52D300
- CALL_VIRT (0x05): after sub_52D340
- CALL_OBJ (0x3E): after sub_52D370
