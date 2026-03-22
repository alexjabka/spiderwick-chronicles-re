# sub_52D240 -- Call Native Function (cdecl)

| Field | Value |
|-------|-------|
| Address | 0x52D240 |
| Size | 53 bytes (0x35) |
| Prototype | `int __cdecl CallNativeCdecl(void (*funcPtr)(void), int argCount)` |
| Called by | VMInterpreter opcode 0x02 (CALL_NATIVE) |
| Category | VM Native Call Dispatch |

---

## Purpose

Dispatches a call to a global native function registered with the VM. The native function uses VM stack operations (PopArg, PushResult, etc.) to read arguments and write return values.

---

## Pseudocode

```c
int __cdecl CallNativeCdecl(void (*funcPtr)(void), int argCount) {
    int oldStackTop = E5616C;
    E56200 = argCount;                  // Set arg count for native to pop
    int savedFunc = E56204;
    E56204 = (int)funcPtr;              // Store current function pointer
    funcPtr();                          // Call native (reads/writes VM stack)
    E56204 = savedFunc;                 // Restore previous function pointer
    return (E5616C - oldStackTop) >> 2; // Return count of pushed values
}
```

---

## Key Details

- The native function receives NO C arguments -- it reads from the VM arg stack via PopArg/PopFloat/etc.
- The return value count is computed by measuring how much the eval stack grew during the call
- `E56204` acts as a "current function" register, saved/restored to support nested calls
