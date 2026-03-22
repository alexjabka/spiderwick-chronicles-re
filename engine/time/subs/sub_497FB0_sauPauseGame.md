# sub_497FB0 — sauPauseGame handler

**Address:** `Spiderwick.exe+97FB0` (absolute: `00497FB0`)
**Convention:** __cdecl (VM thunk)
**Returns:** void

## Signature
```c
void __cdecl sauPauseGame_handler(void)
```

## Description
Script API handler for the `sauPauseGame` Kallis script function. Pauses the game by manipulating the clock system. Registered alongside `sauSetClockSpeed`, `sauUnPauseGame`, `sauDelay`, and `sauSetTimer` in `RegisterTimeScriptFunctions` (0x498180).

## Key Details
- Registered as `"sauPauseGame"` via `RegisterScriptFunction` (0x52EA10)
- Likely calls `SetClockSpeed(0.0)` or an equivalent pause mechanism internally
- Has a corresponding `sauUnPauseGame` handler

## Called By
- Kallis VM — when a script calls `sauPauseGame()`

## Related
- [sub_497DD0_sauSetClockSpeed.md](sub_497DD0_sauSetClockSpeed.md) — related clock script function
- [sub_498180_RegisterTimeScriptFunctions.md](sub_498180_RegisterTimeScriptFunctions.md) — registration
- [../CLOCK_SYSTEM.md](../CLOCK_SYSTEM.md) — clock system overview
