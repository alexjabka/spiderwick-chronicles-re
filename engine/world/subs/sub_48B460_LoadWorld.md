# sub_48B460 — LoadWorld

**Address:** `Spiderwick.exe+8B460` (absolute: `0048B460`)
**Convention:** __cdecl (VM thunk, calls `off_1C88ED0`)
**Returns:** void

## Signature
```c
void __cdecl LoadWorld(const char *worldName, int spawnId)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| worldName | const char* | World name string (e.g. "MansionD", "GroundsD", "Shell") |
| spawnId | int | Spawn point index (0 = default) |

## Description
Triggers a full world transition: unloads the current level and loads the specified world from its `.zwd` wad file. This is a Kallis VM thunk that pushes parameters and calls into the VM interpreter via `off_1C88ED0`.

The world name corresponds to a `.zwd` file in `na/Wads/` (e.g. "MansionD" loads `MansionD.zwd`).

## Key Details
- VM thunk pattern: pushes bytecode descriptor, calls VM interpreter
- The actual world transition logic lives in Kallis VM bytecode
- `spawnId` of 0 means use the default spawn; non-zero values select specific spawn points
- Callers typically call `SetLoadFlag(1)` and `SetSpawnPoint(name)` before invoking this function

## Called By
- `ClLoadChapterAction` (0x4C8F90) — chapter progression
- `ClLoadLevelAction` (0x4C9AC0) — direct level load from action data
- `StartNewGame` (0x4DC090) — new game: LoadWorld("MansionD", 0)
- `QuitToShell` (0x4A25C0) — menu return: LoadWorld("Shell", 0)
- `LoadFromSaveData` (0x48AA10) — save file load
- Door transition handlers (various)

## Calls
- VM interpreter chain (`off_1C88ED0` -> VM bytecode)
- Internally triggers `UnloadLevel` (0x488C10) for the current world

## Related
- [sub_48AF00_SetSpawnPoint.md](sub_48AF00_SetSpawnPoint.md) — sets spawn before load
- [sub_488640_SetLoadFlag.md](sub_488640_SetLoadFlag.md) — arms loading flag
- [sub_488C10_UnloadLevel.md](sub_488C10_UnloadLevel.md) — unloads current level
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
