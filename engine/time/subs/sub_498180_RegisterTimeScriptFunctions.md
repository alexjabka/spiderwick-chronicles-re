# sub_498180 — RegisterTimeScriptFunctions

**Address:** `Spiderwick.exe+98180` (absolute: `00498180`)
**Convention:** __cdecl
**Returns:** void

## Signature
```c
void __cdecl RegisterTimeScriptFunctions(void)
```

## Description
Registers all time-related native functions with the Kallis script system. Called during engine initialization to make time control functions available to scripts.

## Key Details
- Calls `RegisterScriptFunction` (0x52EA10) for each time-related function
- Registered functions:
  - `"sauSetClockSpeed"` -> handler at 0x497DD0
  - `"sauDelay"` -> handler TBD
  - `"sauSetTimer"` -> handler TBD
  - `"sauPauseGame"` -> handler at 0x497FB0
  - `"sauUnPauseGame"` -> handler TBD

## Called By
- Engine initialization (exact caller TBD)

## Calls
- `RegisterScriptFunction` (0x52EA10) — called once per function

## Related
- [sub_497DD0_sauSetClockSpeed.md](sub_497DD0_sauSetClockSpeed.md) — sauSetClockSpeed handler
- [sub_497FB0_sauPauseGame.md](sub_497FB0_sauPauseGame.md) — sauPauseGame handler
- [../CLOCK_SYSTEM.md](../CLOCK_SYSTEM.md) — clock system overview
- [../../events/subs/sub_52EA10_RegisterScriptFunction.md](../../events/subs/sub_52EA10_RegisterScriptFunction.md) — registration API
