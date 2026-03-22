# sub_488C10 — UnloadLevel

**Address:** `Spiderwick.exe+88C10` (absolute: `00488C10`)
**Convention:** __cdecl (presumed)
**Returns:** void

## Signature
```c
void UnloadLevel(void)
```

## Description
Tears down the currently loaded level. Prints a debug message with the current level name, then clears all level state including unregistering objects and freeing resources.

## Key Details
- Prints: `"ClLevel: Unloading {g%s}\n"` using `byte_6E4785` (current level name)
- Clears `byte_6E4785` (level name buffer) with `memset` — zeroes the 32-byte char array
- Unregisters all level objects from the engine
- Frees level resources (geometry, textures, etc.)
- Called internally during `LoadWorld` — the engine unloads the current level before loading the new one

## Debug Output
```
ClLevel: Unloading {gMansionD}
```
The `{g%s}` format suggests the engine's internal logging uses `{g...}` for "game" scope messages.

## Global Variables
| Address | Type | Purpose |
|---------|------|---------|
| `0x6E4785` | char[32] | Current level name (`g_CurrentLevelName`), cleared on unload |

## Called By
- `LoadWorld` (0x48B460) — during world transition, unloads before loading new

## Related
- [sub_48B460_LoadWorld.md](sub_48B460_LoadWorld.md) — triggers unload
- [sub_488330_InitLevelSystem.md](sub_488330_InitLevelSystem.md) — initial clear of level state
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
