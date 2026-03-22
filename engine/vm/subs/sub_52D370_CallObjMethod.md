# sub_52D370 -- Call Object Method with Bool Result

| Field | Value |
|-------|-------|
| Address | 0x52D370 |
| Size | 153 bytes (0x99) |
| Prototype | `int __cdecl CallObjMethod(int (*funcPtr)(int), int argCount)` |
| Called by | VMInterpreter opcode 0x3E (CALL_OBJ) |
| Category | VM Native Call Dispatch |

---

## Purpose

Calls a native function that operates on a VM object and returns a boolean. The function receives the native object pointer and its return value is coerced to 0 or 1 and pushed to the eval stack.

---

## Pseudocode

```c
int __cdecl CallObjMethod(int (*funcPtr)(int), int argCount) {
    int oldStackTop = E5616C;

    // Get "self" from arg stack
    E56200 = argCount;
    int selfRef = *(E56168 - 4 * argCount);
    E56200 = argCount - 1;

    int vmObj;
    if (selfRef)
        vmObj = E56160[0] + 4 * (selfRef / 4);
    else
        vmObj = 0;

    // Set type tag from vtable
    *(vmObj + 24) = *(*(vmObj + 20) + 12);

    // Call function with native object
    int result = funcPtr(vmObj);

    // Push boolean result
    if (result)
        *E5616C = 1;
    else
        *E5616C = 0;
    E5616C += 4;

    return (E5616C - oldStackTop) >> 2;
}
```

---

## Key Details

- Unlike CALL_METHOD (0x03), this:
  - Passes the VM object descriptor directly (not just the native pointer)
  - Coerces the return value to a boolean (0 or 1)
  - Sets the type tag at `vmObj+24` from the vtable before calling
- The type tag update: `*(vmObj + 24) = *(*(vmObj + 20) + 12)` reads a WORD from the class vtable's offset +12
