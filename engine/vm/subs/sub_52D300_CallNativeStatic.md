# sub_52D300 -- Call Native Static Method

| Field | Value |
|-------|-------|
| Address | 0x52D300 |
| Size | 64 bytes (0x40) |
| Prototype | `int __cdecl CallNativeStatic(void (*func)(int), int framePtr, int argCount)` |
| Called by | VMInterpreter opcode 0x04 (CALL_STATIC) |
| Category | VM Native Call Dispatch |

---

## Purpose

Calls a native static function, passing the current frame's native object pointer. Unlike CALL_METHOD which uses vtable dispatch, this calls the function directly.

---

## Pseudocode

```c
int __cdecl CallNativeStatic(void (*func)(int), int framePtr, int argCount) {
    int oldStackTop = E5616C;
    E56200 = argCount;
    int savedFunc = E56204;
    E56204 = (int)func;
    func(*(framePtr + 16));    // Pass native object from frame context
    E56204 = savedFunc;
    return (E5616C - oldStackTop) >> 2;
}
```

---

## Key Details

- `framePtr` is `E56164` (the current frame context, which is a VM object descriptor)
- `*(framePtr + 16)` is the native object pointer stored at offset +0x10 of the VM object
- The interpreter sets `*(E56164 + 0x1A) |= 1` before calling (marks frame as in-use)
