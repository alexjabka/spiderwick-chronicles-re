# sub_488640 — SetLoadFlag

**Address:** `Spiderwick.exe+88640` (absolute: `00488640`)
**Convention:** __cdecl
**Returns:** void

## Signature
```c
void __cdecl SetLoadFlag(int flag)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| flag | int | Loading flag value (1 = arm loading) |

## Description
Sets the level loading flag at `byte_6E47D4`. Called with `flag = 1` before `LoadWorld` calls to arm the loading system.

## Key Details
- Simple setter: `byte_6E47D4 = flag`
- Always called with `1` in observed code paths
- The flag is cleared by `InitLevelSystem` (0x488330) during initialization
- Read by the loading pipeline to determine if a level transition is pending

## Global Variables
| Address | Type | Purpose |
|---------|------|---------|
| `0x6E47D4` | byte | Level loading flag (`g_LoadFlag`) |

## Called By
- `StartNewGame` (0x4DC090) — SetLoadFlag(1) before LoadWorld("MansionD", 0)
- `ClLoadChapterAction` (0x4C8F90) — SetLoadFlag(1) before chapter load
- Other world transition callers

## Related
- [sub_48B460_LoadWorld.md](sub_48B460_LoadWorld.md) — reads the flag during load
- [sub_488330_InitLevelSystem.md](sub_488330_InitLevelSystem.md) — clears the flag
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
