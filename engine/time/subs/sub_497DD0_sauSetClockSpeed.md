# sub_497DD0 — sauSetClockSpeed handler

**Address:** `Spiderwick.exe+97DD0` (absolute: `00497DD0`)
**Convention:** __cdecl (VM thunk)
**Returns:** void

## Signature
```c
void __cdecl sauSetClockSpeed_handler(void)
```

## Description
Script API handler for the `sauSetClockSpeed` Kallis script function. Reads a float parameter from the script stack, then performs the canonical clock update pattern: remove the old clock entry, set the new speed, and store the returned entry ID.

## Key Details
- Reads the speed float from the Kallis VM parameter stack
- Performs the full cleanup-then-set pattern:
  ```c
  RemoveClockEntry(dword_D42D6C);
  dword_D42D6C = SetClockSpeed(speed);
  ```
- Registered as `"sauSetClockSpeed"` via `RegisterScriptFunction` (0x52EA10)
- Registration happens in `RegisterTimeScriptFunctions` (0x498180)

## Called By
- Kallis VM — when a script calls `sauSetClockSpeed(speed)`

## Calls
- `RemoveClockEntry` (0x4DF070) — cleanup old entry
- `SetClockSpeed` (0x4DF360) — set new speed

## Related
- [sub_4DF360_SetClockSpeed.md](sub_4DF360_SetClockSpeed.md) — native SetClockSpeed
- [sub_4DF070_RemoveClockEntry.md](sub_4DF070_RemoveClockEntry.md) — RemoveClockEntry
- [sub_498180_RegisterTimeScriptFunctions.md](sub_498180_RegisterTimeScriptFunctions.md) — registration
- [../CLOCK_SYSTEM.md](../CLOCK_SYSTEM.md) — clock system overview
