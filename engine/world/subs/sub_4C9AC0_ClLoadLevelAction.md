# sub_4C9AC0 — ClLoadLevelAction

**Address:** `Spiderwick.exe+C9AC0` (absolute: `004C9AC0`)
**Convention:** __thiscall (action object)
**Returns:** void

## Signature
```c
void __thiscall ClLoadLevelAction(void *this)
```

## Description
Action handler that performs a direct level load from action data fields. Unlike `ClLoadChapterAction` which maps chapter IDs to worlds, this action reads the world name and spawn point directly from the action object's data.

## Action Object Layout
| Offset | Type | Field |
|--------|------|-------|
| this+0x08 | char* | Spawn point name |
| this+0x18 (24) | char* | World name |
| this+0x28 (40) | int | Spawn index |

## Key Details
- Reads spawn name from `this+8`
- Reads world name from `this+24` (0x18)
- Reads spawn index from `this+40` (0x28)
- Calls `SetSpawnPoint` with the spawn name, then `LoadWorld` with world name and spawn index
- Used for direct level transitions (e.g. door interactions, scripted transitions)

## Sequence
```
ClLoadLevelAction
├── Read spawn name from this+0x08
├── Read world name from this+0x18
├── Read spawn index from this+0x28
├── SetSpawnPoint(spawnName)         (0x48AF00)
└── LoadWorld(worldName, spawnIndex) (0x48B460)
```

## Called By
- Kallis VM action system (door triggers, scripted events)

## Calls
- `SetSpawnPoint` (0x48AF00)
- `LoadWorld` (0x48B460)

## Related
- [sub_4C8F90_ClLoadChapterAction.md](sub_4C8F90_ClLoadChapterAction.md) — chapter-based loading
- [sub_48B460_LoadWorld.md](sub_48B460_LoadWorld.md) — world loading
- [sub_48AF00_SetSpawnPoint.md](sub_48AF00_SetSpawnPoint.md) — spawn point
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
