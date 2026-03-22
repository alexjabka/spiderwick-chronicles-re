# sub_52C610 — PopInt

## Identity

| Field | Value |
|---|---|
| Address | `0x52C610` |
| Calling Convention | `cdecl` |
| Parameters | none |
| Return Value | `int` — popped integer value |
| Module | engine/vm |

## Purpose

Pops an integer value from the Kallis VM argument stack. This is a native function (not a VM thunk) and can be fully decompiled.

## Stack Operation

```c
int PopInt(void)
{
    int value = *(dword_E56168 - 4 * dword_E56200);
    dword_E56200--;
    return value;
}
```

- `dword_E56168` = arg stack base address
- `dword_E56200` = arg stack index (decremented after read)

## Called By

- Various native script handlers that expect integer arguments from Kallis scripts

## Related

- [sub_52C6A0_PopFloat.md](sub_52C6A0_PopFloat.md) — PopFloat (same pattern, float cast)
- [sub_52C700_PopFloat3.md](sub_52C700_PopFloat3.md) — PopFloat3 (pops 3 values)
- [sub_52CC70_PushResult.md](sub_52CC70_PushResult.md) — PushResult (return stack push)
- [../KALLIS_VM.md](../KALLIS_VM.md) — VM architecture overview
