# sub_52CBF0 -- VMReturnString

| Field | Value |
|-------|-------|
| Address | 0x52CBF0 |
| Size | 6 bytes (thunk) |
| Prototype | `int __cdecl VMReturnString(int strPtr)` |
| Target | off_1C8D674 (inside .kallis) |
| Category | VM Stack Push |

---

## Purpose

Pushes a string value onto the VM evaluation stack. This is a thunk to code in the `.kallis` section that handles string interning/management.

---

## Pseudocode

```c
// Thunk:
int __cdecl VMReturnString(int strPtr) {
    return off_1C8D674(strPtr);  // Dispatches to .kallis
}
```

---

## Notes

- Called by VMStackPush (0x52D5A0) for type 8 (string)
- The .kallis target likely interns the string and pushes a handle/reference
- Only 1 direct caller from .text (VMStackPush)
- The actual string handling logic is inside the VM's encrypted code
