# sub_52D340 -- Call Native Virtual Method

| Field | Value |
|-------|-------|
| Address | 0x52D340 |
| Size | 46 bytes (0x2E) |
| Prototype | `int __cdecl CallNativeVirtual(vtable*** obj, int framePtr, int argCount)` |
| Called by | VMInterpreter opcode 0x05 (CALL_VIRT) |
| Category | VM Native Call Dispatch |

---

## Purpose

Calls a native virtual method through a vtable, passing the frame's native object. Combines vtable dispatch (like CALL_METHOD) with frame context (like CALL_STATIC).

---

## Pseudocode

```c
int __cdecl CallNativeVirtual(vtable*** obj, int framePtr, int argCount) {
    int oldStackTop = E5616C;
    E56200 = argCount;
    (**obj)(obj, *(framePtr + 16));    // thiscall via vtable, passing native obj
    return (E5616C - oldStackTop) >> 2;
}
```

---

## Key Details

- Like CALL_STATIC, the interpreter sets `*(E56164 + 0x1A) |= 1` before calling
- The vtable dispatch pattern: `(**obj)(obj, nativeObj)` -- first DWORD of obj is the vtable pointer, first entry is the function to call
