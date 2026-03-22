# Data Store / Property System

**Status:** Investigated (API mapped, UNSTABLE to call from EndScene)

---

## Overview

The Spiderwick engine has a key-value data store for game state variables (e.g., `"/Story/Chapter"`). Variables are stored in a hash-indexed array, accessible via `HashLookup` (string to index) and `GetValuePtr` (index to value pointer). The system uses a binary search on sorted hashes for efficient lookup.

**WARNING:** Do NOT call these functions from the EndScene hook during world transitions. The data store may be in an invalid/partially-initialized state during loading, causing crashes or reading stale data.

---

## Architecture

```
HashLookup(name)     — 0x41E830  __cdecl, returns index or -1
├── Hashes the variable name string
└── BinarySearchByHash (0x539630) in dword_E57F64 array

GetValuePtr(idx)     — 0x5392C0  __thiscall, this=0xE57F68
└── Returns int* to the variable's value

Data Store Layout
├── dword_E57F64     — entry array (16 bytes per entry)
│   ├── +0: hash (DWORD)
│   ├── +4..+8: (unknown)
│   └── +12: value offset (DWORD)
├── dword_E57F54     — entry count
└── byte_E57F68      — data store object base (also contains world name from save data)
```

---

## Lookup Flow

```c
// 1. Hash the variable name to get an index
int idx = HashLookup("/Story/Chapter");  // returns index or -1
if (idx == -1) return;  // not found

// 2. Get pointer to the value
int *valuePtr = GetValuePtr((DataStore*)0xE57F68, idx);
if (valuePtr) {
    int chapter = *valuePtr;
}
```

---

## Instability Warning

The data store API is **NOT safe to call from the EndScene hook** during world transitions:
- During loading, the entry array at `dword_E57F64` may be reallocated or partially populated
- The data store object at `byte_E57F68` also contains save-related data (world name) that changes during transitions
- Reading at the wrong time returns garbage or crashes
- Only safe to call when the game is in a stable gameplay state (not loading)

---

## Entry Layout

Each entry in the `dword_E57F64` array is 16 bytes:

| Offset | Type | Description |
|--------|------|-------------|
| +0 | DWORD | Hash of the variable name |
| +4 | ... | Unknown (8 bytes) |
| +12 | DWORD | Value offset within data store |

Entries are sorted by hash for binary search.

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0xE57F64` | dword* | g_DataStoreEntries | Array of 16-byte entries (hash + value offset) |
| `0xE57F54` | dword | g_DataStoreEntryCount | Number of entries in the array |
| `0xE57F68` | byte[] | g_DataStoreObject | Data store object base / value storage |

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x41E830` | HashLookup | __cdecl | Hash variable name, binary search, return index |
| `0x5392C0` | GetValuePtr | __thiscall | Get pointer to variable value by index |
| `0x539630` | BinarySearchByHash | — | Binary search in sorted hash array |

---

## Known Data Store Variables

The following variables have been identified through reverse engineering:

### Story/Progression

| Path | Type | Values | Purpose |
|------|------|--------|---------|
| `/Story/Chapter` | int | 1-8 | Current chapter number |
| `/Story/Index` | int | 0-1030+ | Story progression index. Threshold of 1030 triggers endgame camera behavior. |

### Player State

| Path | Type | Values | Purpose |
|------|------|--------|---------|
| `/Player/Character` | int | 1=Jared, 2=Mallory(widget)/Simon(visual), 3=Simon(widget)/Mallory(visual) | Active player character type. Index 655, hash `0xA488A96A`. |
| `/Player/Jared/Health` | int | -- | Jared's current health |
| `/Player/Simon/Health` | int | -- | Simon's current health |
| `/Player/Mallory/Health` | int | -- | Mallory's current health |

### Level/Minigame

| Path | Type | Values | Purpose |
|------|------|--------|---------|
| `/Level/MGArenas/Player1Character` | int | -- | Player 1 character in minigame arenas |
| `/Level/MGArenas/Player2Character` | int | -- | Player 2 character in minigame arenas |
| `/Level/MGArenas/Player1Only` | int | 0/1 | Whether minigame is single-player only |
| `/Level/MGArenas/Cooperative` | int | 0/1 | Whether minigame is cooperative mode |

### Character Equipment

| Path Pattern | Type | Purpose |
|-------------|------|---------|
| `/Game/Characters/%s/MeleeWeapon` | int | Character's melee weapon ID |
| `/Game/Characters/%s/RangedWeapon` | int | Character's ranged weapon ID |
| `/Game/Characters/%s/ButterflyNet` | int | Character's butterfly net state |

**Note:** The `%s` is replaced with the character name (Jared/Simon/Mallory) at runtime by `CommitPlayerState` (sub_462B80).

---

## Related Documentation

- [subs/sub_41E830_HashLookup.md](subs/sub_41E830_HashLookup.md) -- HashLookup
- [subs/sub_5392C0_GetValuePtr.md](subs/sub_5392C0_GetValuePtr.md) -- GetValuePtr
- [subs/sub_539630_BinarySearchByHash.md](subs/sub_539630_BinarySearchByHash.md) -- Binary search
- [../world/WORLD_LOADING.md](../world/WORLD_LOADING.md) -- World loading (data store unsafe during transitions)
- [../objects/HOT_SWITCH_SYSTEM.md](../objects/HOT_SWITCH_SYSTEM.md) -- Hot-switch system (writes /Player/Character)
- [../objects/CHARACTER_SWITCHING.md](../objects/CHARACTER_SWITCHING.md) -- Character switching (data store approach)
