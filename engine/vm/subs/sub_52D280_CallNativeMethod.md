# sub_52D280 -- Call Native Method (thiscall via vtable)

| Field | Value |
|-------|-------|
| Address | 0x52D280 |
| Size | 127 bytes (0x7F) |
| Prototype | `int __cdecl CallNativeMethod(vtable*** obj, int argCount)` |
| Called by | VMInterpreter opcode 0x03 (CALL_METHOD) |
| Category | VM Native Call Dispatch |

---

## Purpose

Dispatches a call to a native method through a VM object's vtable. This is used when script code calls a method on a VM object that has a native C++ counterpart.

---

## Pseudocode

```c
int __cdecl CallNativeMethod(vtable*** obj, int argCount) {
    int oldStackTop = E5616C;

    // Get "self" object from arg stack
    int selfRef = *(E56168 - 4 * argCount);
    int vmObj = E56160[0] + 4 * (selfRef >> 2);

    if (vmObj == 0) return 0;

    int nativeObj = *(vmObj + 16);  // Get native counterpart
    if (nativeObj == 0) {
        // Error: no native object linked
        const char* name;
        if (E561E4)
            name = sub_58C190(*(vmObj + 8));  // Resolve name for error message
        else
            name = NULL;
        printf("Object type had no native counterpart: %s\n", name);
    }

    E56200 = argCount - 1;              // Set arg count (minus self)
    (**obj)(obj, nativeObj);             // Call through vtable: thiscall(obj, nativePtr)
    return (E5616C - oldStackTop) >> 2;  // Return value count
}
```

---

## Error String

`"Object type had no native counterpart: %s\n"` -- printed when a script tries to call a method on a VM object that has no native C++ object linked at `vmObj+16`.

---

## vtable Call Convention

The call `(**obj)(obj, nativeObj)` is a thiscall through a vtable:
- `obj` = the vtable pointer pointer (function descriptor)
- `nativeObj` = the native C++ object to pass as the "this" or first argument
