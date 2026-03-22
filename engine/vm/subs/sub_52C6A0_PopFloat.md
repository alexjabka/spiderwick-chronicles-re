# sub_52C6A0 — PopFloat

## Identity

| Field | Value |
|---|---|
| Address | `0x52C6A0` |
| Calling Convention | `cdecl` |
| Parameters | none |
| Return Value | `float` — popped float value |
| Module | engine/vm |

## Purpose

Pops a float value from the Kallis VM argument stack. This is a native function (not a VM thunk) and can be fully decompiled.

## Stack Operation

```c
float PopFloat(void)
{
    float value = *(float*)(dword_E56168 - 4 * dword_E56200);
    dword_E56200--;
    return value;
}
```

- `dword_E56168` = arg stack base address
- `dword_E56200` = arg stack index (decremented after read)

## Called By

- `PopFloat3` (sub_52C700) — calls this 3 times for vec3
- Various native script handlers expecting float arguments

## Related

- [sub_52C610_PopInt.md](sub_52C610_PopInt.md) — PopInt (same pattern, int type)
- [sub_52C700_PopFloat3.md](sub_52C700_PopFloat3.md) — PopFloat3 (calls PopFloat x3)
- [../KALLIS_VM.md](../KALLIS_VM.md) — VM architecture overview
