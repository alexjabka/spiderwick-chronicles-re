# sub_4DC090 — StartNewGame

**Address:** `Spiderwick.exe+DC090` (absolute: `004DC090`)
**Convention:** __cdecl (presumed)
**Returns:** void

## Signature
```c
void StartNewGame(void)
```

## Description
Initializes and starts a new game. Sets up the storage system, arms the loading flag, configures the spawn point to Chapter 1, and loads the Mansion world.

## Full Sequence
```c
void StartNewGame(void) {
    SetStorageFlag();               // 0x538080 — dword_7133B8 |= 1
    SetLoadFlag(1);                 // 0x488640 — byte_6E47D4 = 1
    SetSpawnPoint("nf01");          // 0x48AF00 — Chapter 1 spawn
    LoadWorld("MansionD", 0);       // 0x48B460 — load Mansion (Day)
}
```

## Key Details
- This is the complete new game entry point
- `SetStorageFlag` signals that save/storage should be reset for a fresh game
- Spawn point "nf01" is the Chapter 1 starting location in the Mansion
- `spawnId` of 0 passed to LoadWorld (default)
- The Mansion (Day) world is always the starting world for a new game

## Calls
- `SetStorageFlag` (0x538080) — set new game bit in storage flags
- `SetLoadFlag` (0x488640) — arm loading system
- `SetSpawnPoint` (0x48AF00) — set spawn to "nf01"
- `LoadWorld` (0x48B460) — load "MansionD" world

## Called By
- Main menu "New Game" handler

## Related
- [sub_538080_SetStorageFlag.md](sub_538080_SetStorageFlag.md) — storage flag
- [sub_488640_SetLoadFlag.md](sub_488640_SetLoadFlag.md) — load flag
- [sub_48AF00_SetSpawnPoint.md](sub_48AF00_SetSpawnPoint.md) — spawn point
- [sub_48B460_LoadWorld.md](sub_48B460_LoadWorld.md) — world loading
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
