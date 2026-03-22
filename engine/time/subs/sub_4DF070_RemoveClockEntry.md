# sub_4DF070 — RemoveClockEntry

**Address:** `Spiderwick.exe+9F070` (absolute: `004DF070`)
**Convention:** __cdecl
**Returns:** void

## Signature
```c
void __cdecl RemoveClockEntry(int entryId)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| entryId | int | Clock entry ID returned by SetClockSpeed |

## Description
Removes a previously created clock speed entry. Must be called before setting a new clock speed to clean up the old entry and avoid leaking clock entries in the system.

## Key Details
- Takes the entry ID previously returned by `SetClockSpeed` (0x4DF360)
- The canonical usage pattern is: `RemoveClockEntry(dword_D42D6C)` followed by `dword_D42D6C = SetClockSpeed(newSpeed)`
- Safe to call with an invalid/already-removed entry ID (no crash observed)

## Called By
- `sauSetClockSpeed handler` (0x497DD0) — called before each new SetClockSpeed

## Related
- [sub_4DF360_SetClockSpeed.md](sub_4DF360_SetClockSpeed.md) — creates clock entries
- [sub_497DD0_sauSetClockSpeed.md](sub_497DD0_sauSetClockSpeed.md) — script handler
- [../CLOCK_SYSTEM.md](../CLOCK_SYSTEM.md) — clock system overview
