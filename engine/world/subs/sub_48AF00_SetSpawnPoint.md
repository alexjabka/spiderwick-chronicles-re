# sub_48AF00 — SetSpawnPoint

**Address:** `Spiderwick.exe+8AF00` (absolute: `0048AF00`)
**Convention:** __stdcall (VM thunk, calls `off_1C8901C`)
**Returns:** void

## Signature
```c
void __stdcall SetSpawnPoint(const char *name)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| name | const char* | Spawn point name (e.g. "nf01" through "nf08") |

## Description
Sets the spawn location for the next world load. The spawn name is a string identifier that corresponds to a named spawn point placed in the world data. This is a Kallis VM thunk that forwards the call into VM bytecode via `off_1C8901C`.

## Known Spawn Point Names
| Name | Chapter | World |
|------|---------|-------|
| nf01 | Chapter 1 | MansionD |
| nf02 | Chapter 2 | MansionD |
| nf03 | Chapter 3 | MansionD |
| nf04 | Chapter 4 | GroundsD |
| nf05 | Chapter 5 | ThimbleT |
| nf06 | Chapter 6 | GoblCamp |
| nf07 | Chapter 7 | MnAttack |
| nf08 | Chapter 8 | GroundsD |

## Key Details
- Must be called **before** `LoadWorld` to take effect
- The spawn name is stored internally by the VM and consumed during the load process
- __stdcall convention: callee cleans up the stack argument

## Called By
- `ClLoadChapterAction` (0x4C8F90) — sets spawn based on chapter ID
- `StartNewGame` (0x4DC090) — SetSpawnPoint("nf01")
- `ClLoadLevelAction` (0x4C9AC0) — reads spawn name from action data at this+8

## Related
- [sub_48B460_LoadWorld.md](sub_48B460_LoadWorld.md) — consumes the spawn point
- [sub_4C8F90_ClLoadChapterAction.md](sub_4C8F90_ClLoadChapterAction.md) — chapter-to-spawn mapping
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
