# sub_52CC30 -- VMReturnInt

| Field | Value |
|-------|-------|
| Address | 0x52CC30 |
| Size | 5 bytes (thunk) |
| Prototype | `int __cdecl VMReturnInt(int value)` |
| Target | sub_43C426 (inline at 0x52CC35) |
| Category | VM Stack Push |

---

## Purpose

Pushes an integer value onto the VM evaluation stack (return stack). This is a thunk -- the actual code is at 0x43C426.

---

## Pseudocode

```c
int __cdecl VMReturnInt(int value) {
    *(int*)E5616C = value;
    E5616C += 4;
    return old_E5616C;  // Returns the address where value was written
}
```

---

## Usage

Called by 45+ native handler functions to return integer results to scripts. Examples:
- Animation state queries
- Inventory item counts
- Timer values
- Entity property getters

---

## Notes

- No guard check (unlike VMReturnBoolByte which checks E561F8)
- Returns the address of the written value (pre-increment pointer)
