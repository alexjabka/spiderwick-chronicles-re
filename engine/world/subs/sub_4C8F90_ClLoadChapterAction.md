# sub_4C8F90 — ClLoadChapterAction

**Address:** `Spiderwick.exe+C8F90` (absolute: `004C8F90`)
**Convention:** __thiscall (action object)
**Returns:** void

## Signature
```c
void __thiscall ClLoadChapterAction(void *this)
```

## Description
Action handler that maps a chapter ID to a world name and spawn point, then triggers a world load. Reads the chapter's string file and calls `SetSpawnPoint` + `LoadWorld` to transition to the appropriate world.

## Chapter-to-World Mapping

```c
switch (chapterId) {
    case 1:  world = "MansionD"; spawn = "nf01"; break;
    case 2:  world = "MansionD"; spawn = "nf02"; break;
    case 3:  world = "MansionD"; spawn = "nf03"; break;
    case 4:  world = "GroundsD"; spawn = "nf04"; break;
    case 5:  world = "ThimbleT"; spawn = "nf05"; break;
    case 6:  world = "GoblCamp"; spawn = "nf06"; break;
    case 7:  world = "MnAttack"; spawn = "nf07"; break;
    case 8:  world = "GroundsD"; spawn = "nf08"; break;
    case 43: world = "GroundsD"; break;
    case 58: world = "GoblCamp"; break;
    case 59: world = "GoblCamp"; break;
    case 60: world = "GroundsD"; break;
}
```

## Key Details
- Chapters 1-3 all load the Mansion (same world, different spawn points)
- Chapter 4 and 8 both load GroundsD (different spawn points)
- Chapters 43 and 60 are additional GroundsD entries (possibly cutscene/event variants)
- Chapters 58 and 59 are additional GoblCamp entries
- Reads the chapter string file before loading (for cutscene/dialogue data)
- Calls `SetLoadFlag(1)` before `LoadWorld`

## Sequence
```
ClLoadChapterAction
├── Read chapter string file
├── SetLoadFlag(1)                    (0x488640)
├── SetSpawnPoint("nfXX")            (0x48AF00)
└── LoadWorld("WorldName", 0)         (0x48B460)
```

## Called By
- Game progression system (chapter completion triggers)
- Chapter select menu (if applicable)

## Calls
- `SetLoadFlag` (0x488640)
- `SetSpawnPoint` (0x48AF00)
- `LoadWorld` (0x48B460)

## Related
- [sub_48B460_LoadWorld.md](sub_48B460_LoadWorld.md) — world loading
- [sub_48AF00_SetSpawnPoint.md](sub_48AF00_SetSpawnPoint.md) — spawn point setting
- [sub_488640_SetLoadFlag.md](sub_488640_SetLoadFlag.md) — load flag
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
