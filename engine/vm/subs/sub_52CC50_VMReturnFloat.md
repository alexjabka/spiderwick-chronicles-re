# sub_52CC50 -- VMReturnFloat

| Field | Value |
|-------|-------|
| Address | 0x52CC50 |
| Size | 19 bytes (0x13) |
| Prototype | `int __cdecl VMReturnFloat(float value)` |
| Category | VM Stack Push |

---

## Purpose

Pushes a float value onto the VM evaluation stack.

---

## Pseudocode

```c
int __cdecl VMReturnFloat(float value) {
    *(float*)E5616C = value;
    E5616C += 4;
    return old_E5616C;
}
```

---

## Usage

Called by 18+ native handlers. Common uses:
- `sauRandRange` -- push random float result
- Camera distance queries
- Object position/rotation getters
- Timer value getters
- Audio volume queries

---

## Notes

- No guard check -- always pushes
- Float is stored as IEEE 754 single precision (4 bytes)
- Called from addresses like 0x40A929, 0x4473DA, 0x499082
