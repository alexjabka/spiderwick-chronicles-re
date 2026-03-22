# sub_48AA10 — LoadFromSaveData

**Address:** `Spiderwick.exe+8AA10` (absolute: `0048AA10`)
**Convention:** __cdecl (presumed)
**Returns:** void

## Signature
```c
void LoadFromSaveData(void)
```

## Description
Loads a world from saved game data. Reads the "LEVEL" and "RESPAWNID" fields from the save data system (`ReadGameData`), then calls `LoadWorld` with the saved level name and respawn ID.

## Key Details
- Reads `"LEVEL"` key from save data — returns the world name string (e.g. "MansionD")
- Reads `"RESPAWNID"` key from save data — returns the spawn/respawn ID integer
- Calls `LoadWorld(levelName, respawnId)` with the values from the save
- Does not call `SetSpawnPoint` — the respawn ID is passed directly to `LoadWorld` as the `spawnId` parameter

## Pseudocode
```c
void LoadFromSaveData(void) {
    const char *level = ReadGameData("LEVEL");
    int respawnId = ReadGameDataInt("RESPAWNID");
    LoadWorld(level, respawnId);
}
```

## Called By
- Main menu "Continue" / "Load Game" handler

## Calls
- `ReadGameData` — reads string/int values from save file
- `LoadWorld` (0x48B460) — loads the saved world

## Related
- [sub_48B460_LoadWorld.md](sub_48B460_LoadWorld.md) — world loading
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
