# sub_52CA80 -- VMReturnBoolByte (Guarded)

| Field | Value |
|-------|-------|
| Address | 0x52CA80 |
| Size | 34 bytes (0x22) |
| Prototype | `BOOL __cdecl VMReturnBoolByte(char value)` |
| Called by | VMStackPush (0x52D5A0) for type 2 (bool) |
| Category | VM Stack Push |

---

## Purpose

Pushes a boolean value (0 or 1) onto the evaluation stack, but only if scripts are loaded. This is the "guarded" variant used by VMStackPush when pushing bool-typed VMArgs.

---

## Pseudocode

```c
BOOL __cdecl VMReturnBoolByte(char value) {
    if (E561F8) {  // Scripts loaded?
        int result = (value != 0) ? 1 : 0;
        *(int*)E5616C = result;
        E5616C += 4;
        return result;
    }
    // Silently does nothing if scripts not loaded
}
```

---

## Comparison with VMReturnBool (0x52CC70)

| Feature | VMReturnBoolByte (0x52CA80) | VMReturnBool (0x52CC70) |
|---------|----------------------------|------------------------|
| Guard | Checks E561F8 first | No guard |
| Caller | VMStackPush (typed push) | Native handlers directly |
| Effect when no scripts | Does nothing | Always pushes |
