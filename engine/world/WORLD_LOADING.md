# World Loading System

**Status:** Reversed (core pipeline mapped, all key functions identified)

---

## Overview

The Spiderwick engine loads self-contained "worlds" from `.zwd` wad files. A world transition involves fully unloading the current level (sectors, objects, resources) and loading the new one. The pipeline is orchestrated by several layers:

1. **Game logic** decides which world to load (chapter progression, door transitions, save data, menu actions)
2. **SetSpawnPoint** sets where the player will appear after loading
3. **SetLoadFlag** arms the loading system
4. **LoadWorld** triggers the actual transition via a Kallis VM thunk

The world loading system is distinct from the **sector streaming system** (see [../sectors/SECTOR_LOADING.md](../sectors/SECTOR_LOADING.md)), which manages loading/unloading of individual rooms *within* an already-loaded world.

---

## Architecture

```
Game Logic Layer
├── StartNewGame (0x4DC090)
│   └── SetStorageFlag → SetLoadFlag(1) → SetSpawnPoint("nf01") → LoadWorld("MansionD", 0)
│
├── ClLoadChapterAction (0x4C8F90)
│   └── Maps chapter ID → world name + spawn point → LoadWorld
│
├── ClLoadLevelAction (0x4C9AC0)
│   └── Reads world/spawn from action data → LoadWorld
│
├── LoadFromSaveData (0x48AA10)
│   └── Reads "LEVEL" + "RESPAWNID" from save → LoadWorld
│
├── QuitToShell (0x4A25C0)
│   └── LoadWorld("Shell", 0)
│
└── Door transitions (various callers)
    └── SetSpawnPoint → LoadWorld

Core Loading
├── LoadWorld (0x48B460) — VM thunk, triggers full world transition
├── SetSpawnPoint (0x48AF00) — VM thunk, sets spawn name
├── SetLoadFlag (0x488640) — arms loading flag at byte_6E47D4
└── UnloadLevel (0x488C10) — tears down current level

Initialization
└── InitLevelSystem (0x488330) — clears all level state on startup
```

---

## World Names

All worlds are stored as `.zwd` files in `na/Wads/`.

| Internal Name | Description | ZWD File | Used In |
|---------------|-------------|----------|---------|
| MansionD | Mansion (Day) | MansionD.zwd | Chapters 1-3 |
| MnAttack | Mansion (Attack) | MnAttack.zwd | Chapter 7 |
| GroundsD | Grounds (Day) | GroundsD.zwd | Chapters 4, 8, 43, 60 |
| GoblCamp | Goblin Camp | GoblCamp.zwd | Chapters 6, 58, 59 |
| ThimbleT | Thimbletack's Lair | ThimbleT.zwd | Chapter 5 |
| FrstRoad | Forest Road | FrstRoad.zwd | — |
| DeepWood | Deep Woods | DeepWood.zwd | — |
| Tnl2Town | Tunnel to Town | Tnl2Town.zwd | — |
| MGArena1 | Minigame Arena 1 | MGArena1.zwd | Minigames |
| MGArena2 | Minigame Arena 2 | MGArena2.zwd | Minigames |
| MGArena3 | Minigame Arena 3 | MGArena3.zwd | Minigames |
| MGArena4 | Minigame Arena 4 | MGArena4.zwd | Minigames |
| Shell | Main Menu | Shell.zwd | Menu/title |
| Common | Shared Assets | Common.zwd | Always loaded |

---

## Chapter-to-World Mapping

From `ClLoadChapterAction` (0x4C8F90):

| Chapter ID | World | Spawn Point |
|------------|-------|-------------|
| 1 | MansionD | nf01 |
| 2 | MansionD | nf02 |
| 3 | MansionD | nf03 |
| 4 | GroundsD | nf04 |
| 5 | ThimbleT | nf05 |
| 6 | GoblCamp | nf06 |
| 7 | MnAttack | nf07 |
| 8 | GroundsD | nf08 |
| 43 | GroundsD | — |
| 58 | GoblCamp | — |
| 59 | GoblCamp | — |
| 60 | GroundsD | — |

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0x6E4784` | byte | clear_color_flag | Clear color flag, cleared on init |
| `0x6E4780` | dword | clear_color_argb | Clear color ARGB value |
| `0x6E4785` | char[32] | g_CurrentLevelName | Current level name, printed on unload |
| `0x6E47A5` | char[32] | g_SecondaryLevelName | Secondary level name buffer |
| `0x6E47D4` | byte | g_LoadFlag | Level loading flag, set to 1 before LoadWorld |
| `0xE57F68` | char[] | g_SaveWorldName | Save/chapter world name buffer |
| `0x7133B8` | dword | g_StorageFlags | Storage system flags (bit 0 = new game) |

---

## Loading Sequence (New Game)

```
StartNewGame (0x4DC090)
│
├── SetStorageFlag (0x538080)
│   └── dword_7133B8 |= 1
│
├── SetLoadFlag(1) (0x488640)
│   └── byte_6E47D4 = 1
│
├── SetSpawnPoint("nf01") (0x48AF00)
│   └── VM thunk → sets spawn location
│
└── LoadWorld("MansionD", 0) (0x48B460)
    └── VM thunk → unloads current world → loads MansionD.zwd → spawns at nf01
```

---

## Post-Load Initialization & MissionStart Event

After the world finishes loading (geometry, sectors, objects), `PostWorldLoadInit` (0x488660) runs the post-load sequence:

```
PostWorldLoadInit (0x488660)
├── sub_4DC6A0(flag)                 — pre-init step
├── DispatchEvent("MissionStart")    — 0x52EBE0, triggers all level scripts
└── sub_4DC330(2)                    — post-init step
```

The `"MissionStart"` event (string at `0x62F4C0`) is dispatched to all registered Kallis script handlers. This is where level scripts initialize — mission objectives, cutscene triggers, enemy spawns, NPC dialogue, etc.

### Free Explore Technique

By modifying the first byte of the `"MissionStart"` string at `0x62F4C0` from `'M'` (0x4D) to `'X'` (0x58), the event becomes `"XissionStart"`. No script handlers match this name, so **no mission scripts execute**. The world loads normally with all geometry, NPCs, and objects, but the player can explore freely without cutscenes, triggers, or mission state changes.

```c
*(BYTE*)0x62F4C0 = 'X';   // Free Explore: suppress mission scripts
*(BYTE*)0x62F4C0 = 'M';   // Restore: re-enable mission scripts
```

This is a single-byte, instantly-toggleable patch — the cleanest possible approach.

See [../events/EVENT_SYSTEM.md](../events/EVENT_SYSTEM.md) for the full event system documentation.

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x48B460` | LoadWorld | __cdecl (VM thunk) | Trigger full world transition |
| `0x48AF00` | SetSpawnPoint | __stdcall (VM thunk) | Set spawn location for next load |
| `0x488640` | SetLoadFlag | __cdecl | Arm the loading flag |
| `0x538080` | SetStorageFlag | __cdecl | Set storage system bit 0 |
| `0x488C10` | UnloadLevel | — | Tear down current level |
| `0x488660` | PostWorldLoadInit | — | Post-load init, dispatches MissionStart |
| `0x4C8F90` | ClLoadChapterAction | — | Map chapter ID to world+spawn |
| `0x4DC090` | StartNewGame | — | Full new game init sequence |
| `0x4C9AC0` | ClLoadLevelAction | — | Direct level load from action |
| `0x4A25C0` | QuitToShell | — | Load Shell world |
| `0x48AA10` | LoadFromSaveData | — | Load world from save file |
| `0x488330` | InitLevelSystem | — | Clear all level state |
| `0x5176E0` | PrintSectorLoadInfo | — | Debug print sector name + stats |
| `0x52EBE0` | DispatchEvent | __cdecl (VM thunk) | Dispatch named event to scripts |

---

## Related Documentation

- [subs/sub_48B460_LoadWorld.md](subs/sub_48B460_LoadWorld.md) — LoadWorld VM thunk
- [subs/sub_48AF00_SetSpawnPoint.md](subs/sub_48AF00_SetSpawnPoint.md) — SetSpawnPoint VM thunk
- [subs/sub_488640_SetLoadFlag.md](subs/sub_488640_SetLoadFlag.md) — SetLoadFlag
- [subs/sub_538080_SetStorageFlag.md](subs/sub_538080_SetStorageFlag.md) — SetStorageFlag
- [subs/sub_488C10_UnloadLevel.md](subs/sub_488C10_UnloadLevel.md) — UnloadLevel
- [subs/sub_4C8F90_ClLoadChapterAction.md](subs/sub_4C8F90_ClLoadChapterAction.md) — Chapter action handler
- [subs/sub_4DC090_StartNewGame.md](subs/sub_4DC090_StartNewGame.md) — New game init
- [subs/sub_4C9AC0_ClLoadLevelAction.md](subs/sub_4C9AC0_ClLoadLevelAction.md) — Direct level load
- [subs/sub_4A25C0_QuitToShell.md](subs/sub_4A25C0_QuitToShell.md) — Quit to Shell
- [subs/sub_48AA10_LoadFromSaveData.md](subs/sub_48AA10_LoadFromSaveData.md) — Load from save
- [subs/sub_488330_InitLevelSystem.md](subs/sub_488330_InitLevelSystem.md) — Level system init
- [subs/sub_5176E0_PrintSectorLoadInfo.md](subs/sub_5176E0_PrintSectorLoadInfo.md) — Sector load debug print
- [../sectors/SECTOR_LOADING.md](../sectors/SECTOR_LOADING.md) — Sector streaming (within a loaded world)
- [SECTOR_CULLING.md](SECTOR_CULLING.md) — Distance-based sector culling (proximity service + portal pre-test)
- [../camera/PORTAL_SYSTEM.md](../camera/PORTAL_SYSTEM.md) — Portal rendering system
- [../events/EVENT_SYSTEM.md](../events/EVENT_SYSTEM.md) — Event system (MissionStart dispatch)
- [../events/subs/sub_488660_PostWorldLoadInit.md](../events/subs/sub_488660_PostWorldLoadInit.md) — Post-world-load init
- [../events/subs/sub_52EBE0_DispatchEvent.md](../events/subs/sub_52EBE0_DispatchEvent.md) — DispatchEvent
- [../fade/FADE_SYSTEM.md](../fade/FADE_SYSTEM.md) — Fade system (loading screen fades)
- [../time/CLOCK_SYSTEM.md](../time/CLOCK_SYSTEM.md) — Clock/time system
