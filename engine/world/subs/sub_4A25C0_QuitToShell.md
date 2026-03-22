# sub_4A25C0 — QuitToShell

**Address:** `Spiderwick.exe+A25C0` (absolute: `004A25C0`)
**Convention:** __cdecl (presumed)
**Returns:** void

## Signature
```c
void QuitToShell(void)
```

## Description
Returns to the main menu by loading the "Shell" world. Checks a state flag before initiating the transition.

## Key Details
- Checks `byte_E57F68` before proceeding (likely a guard against re-entrant quit)
- Calls `LoadWorld("Shell", 0)` to transition to the main menu world
- "Shell" is the engine's name for the main menu / title screen environment
- `spawnId` is 0 (default)

## Global Variables
| Address | Type | Purpose |
|---------|------|---------|
| `0xE57F68` | byte | Save/chapter world name buffer (checked as guard) |

## Called By
- Pause menu "Quit to Main Menu" handler
- Game over / fail state handlers

## Calls
- `LoadWorld` (0x48B460) — LoadWorld("Shell", 0)

## Related
- [sub_48B460_LoadWorld.md](sub_48B460_LoadWorld.md) — world loading
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
