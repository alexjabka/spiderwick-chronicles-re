# sub_52D7D0 — PopString

## Identity

| Field | Value |
|---|---|
| Address | `0x52D7D0` |
| Calling Convention | `cdecl` |
| Parameters | none |
| Return Value | `const char*` — popped string reference |
| Module | engine/vm |

## Purpose

Pops a string reference from the Kallis VM argument stack. Returns a pointer to the string data. This is a native function (not a VM thunk).

## Called By

- Script handlers that receive string arguments (event names, asset paths, etc.)

## Related

- [sub_52C610_PopInt.md](sub_52C610_PopInt.md) — PopInt (same stack, different type)
- [sub_52C860_PopObjectRef.md](sub_52C860_PopObjectRef.md) — PopObjectRef (similar but resolves objects)
- [sub_52D820_PopVariant.md](sub_52D820_PopVariant.md) — PopVariant (another pop type)
- [../KALLIS_VM.md](../KALLIS_VM.md) — VM architecture overview
