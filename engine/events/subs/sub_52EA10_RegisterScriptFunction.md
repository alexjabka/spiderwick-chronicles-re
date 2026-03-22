# sub_52EA10 — RegisterScriptFunction

**Address:** `Spiderwick.exe+12EA10` (absolute: `0052EA10`)
**Convention:** __cdecl
**Returns:** void

## Signature
```c
void __cdecl RegisterScriptFunction(const char *name, void *handler)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| name | const char* | Script-visible function name (e.g., "sauSetClockSpeed") |
| handler | void* | Pointer to the native handler function |

## Description
Registers a named native function so it can be called from Kallis scripts. The function name and handler pointer are added to the VM's function lookup table. When a script calls the named function, the VM invokes the registered native handler.

## Key Details
- Called during engine initialization to populate the script function table
- Each call registers one function by name
- Known caller: `RegisterTimeScriptFunctions` (0x498180) registers 5 time-related functions
- The `"sau"` prefix is a naming convention for script-accessible utility functions

## Called By
- `RegisterTimeScriptFunctions` (0x498180) — registers sauSetClockSpeed, sauDelay, sauSetTimer, sauPauseGame, sauUnPauseGame
- Other registration functions (TBD)

## Related
- [sub_52EBE0_DispatchEvent.md](sub_52EBE0_DispatchEvent.md) — event dispatch (related VM API)
- [../../time/subs/sub_498180_RegisterTimeScriptFunctions.md](../../time/subs/sub_498180_RegisterTimeScriptFunctions.md) — caller
- [../EVENT_SYSTEM.md](../EVENT_SYSTEM.md) — event system overview
