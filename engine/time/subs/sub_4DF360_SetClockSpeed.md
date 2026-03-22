# sub_4DF360 — SetClockSpeed

**Address:** `Spiderwick.exe+9F360` (absolute: `004DF360`)
**Convention:** __cdecl
**Returns:** int (clock entry ID)

## Signature
```c
int __cdecl SetClockSpeed(float speed)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| speed | float | Clock speed factor, clamped to [0.0, 1.0] |

## Description
Sets the global game clock speed. Takes a float parameter clamped between 0.0 (freeze) and 1.0 (normal). Internally sets `ecx` to `dword_D57810` (the clock system object) and creates a new clock entry. Returns an integer entry ID that must be stored and later passed to `RemoveClockEntry` for cleanup.

This is the same underlying API called by the `sauSetClockSpeed` Kallis script function.

## Key Details
- `ecx` is set internally to `dword_D57810` (clock system object), not passed by caller
- Speed is clamped: values below 0.0 become 0.0, values above 1.0 become 1.0
- The returned entry ID is stored globally in `dword_D42D6C` by the script handler
- `0.0` = freeze time, `1.0` = normal speed
- Must call `RemoveClockEntry(oldId)` before calling this again to avoid leaking entries

## Called By
- `sauSetClockSpeed handler` (0x497DD0) — script system handler

## Calls
- Internal clock system methods via the object at `dword_D57810`

## Related
- [sub_4DF070_RemoveClockEntry.md](sub_4DF070_RemoveClockEntry.md) — removes a clock entry
- [sub_497DD0_sauSetClockSpeed.md](sub_497DD0_sauSetClockSpeed.md) — script handler that calls this
- [../CLOCK_SYSTEM.md](../CLOCK_SYSTEM.md) — clock system overview
