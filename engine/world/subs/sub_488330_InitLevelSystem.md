# sub_488330 — InitLevelSystem

**Address:** `Spiderwick.exe+88330` (absolute: `00488330`)
**Convention:** __cdecl (presumed)
**Returns:** void

## Signature
```c
void InitLevelSystem(void)
```

## Description
Initializes the level system by clearing all level-related global state. Called during engine startup or level system reset.

## Key Details
- Clears `byte_6E4784` — clear color flag
- Clears `byte_6E47D4` — loading flag (`g_LoadFlag`)
- Clears `byte_6E4785` — current level name buffer (`g_CurrentLevelName`, 32 bytes)
- Clears `byte_6E47A5` — secondary level name buffer (`g_SecondaryLevelName`, 32 bytes)
- All clears are zeroing operations (memset to 0 or direct byte write of 0)

## Global Variables Cleared
| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0x6E4784` | byte | clear_color_flag | Clear color flag |
| `0x6E47D4` | byte | g_LoadFlag | Level loading flag |
| `0x6E4785` | char[32] | g_CurrentLevelName | Current level name |
| `0x6E47A5` | char[32] | g_SecondaryLevelName | Secondary level name |

## Called By
- Engine initialization sequence (startup)

## Related
- [sub_488640_SetLoadFlag.md](sub_488640_SetLoadFlag.md) — sets the flag this clears
- [sub_488C10_UnloadLevel.md](sub_488C10_UnloadLevel.md) — also clears level name
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
