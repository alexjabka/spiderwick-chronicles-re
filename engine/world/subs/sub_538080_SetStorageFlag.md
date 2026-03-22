# sub_538080 — SetStorageFlag

**Address:** `Spiderwick.exe+138080` (absolute: `00538080`)
**Convention:** __cdecl
**Returns:** void

## Signature
```c
void __cdecl SetStorageFlag(void)
```

## Description
Sets bit 0 of the storage system flags global (`dword_7133B8 |= 1`). Called before new game world loads to signal the storage system that a fresh game state is being initialized.

## Key Details
- Bitwise OR operation: `dword_7133B8 |= 1`
- Only sets bit 0; does not clear other bits
- Called exclusively in the new game path, not during chapter loads or save loads

## Global Variables
| Address | Type | Purpose |
|---------|------|---------|
| `0x7133B8` | dword | Storage system flags (`g_StorageFlags`), bit 0 = new game |

## Called By
- `StartNewGame` (0x4DC090) — first call in the new game sequence

## Related
- [sub_4DC090_StartNewGame.md](sub_4DC090_StartNewGame.md) — new game sequence
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
