# Character / Object Creation System

**Status:** Reversed (spawn pipeline, VM stack interface, creation internals identified)

---

## Overview

Characters in the Spiderwick engine are created through a pipeline that originates from Kallis script calls (`sauSpawnObj`, `sauCreateCharacter`) and terminates in native code that allocates, initializes, and registers character objects. The pipeline reads arguments from the VM stack and uses the engine's asset resolution system to instantiate characters from template definitions.

---

## Architecture

```
Script Layer (Kallis VM)
├── sauSpawnObj(objRef, pos, rot)      — VM calls native handler sub_44C730
└── sauCreateCharacter(template)       — VM-registered (handler in .kallis)

Native Spawn Pipeline
├── sub_44C730 (sauSpawnObj handler)   — reads VM stack, calls SpawnCharacter
│   ├── PopObjectRef (sub_52C860)      — pop asset/object reference
│   ├── PopVec3 (sub_52C770)           — pop position vec3
│   ├── PopVec3 (sub_52C770)           — pop rotation vec3
│   └── sub_44C600 (SpawnCharacter)    — __thiscall on spawner object
│       └── sub_44FA20 (CreateCharacter_Internal)
│           ├── sub_55BDC0 (ResolveAssetRef)  — asset ref → definition ptr
│           ├── sub_4D6030 (InstantiateCharacter) — definition → object
│           ├── Links to character list (off_6E2830)
│           └── sub_4DE2A0 ("Character Memory") — memory tracking
└── sub_52CC70 (PushResult)            — push success (1) to return stack

Character List
├── g_CharacterListHead (0x7307D8)     — singly linked list head
├── Next pointer at offset +0x5A0      — 1440 bytes into character object
└── Iteration functions:
    ├── GetPlayerCharacter (0x44F890)   — vtable[19] check
    ├── GetPlayerCharacter2 (0x44F8F0)  — vtable[20] check
    ├── FindClosestCharacter (0x44F7C0)
    ├── FindClosestCharacterInRadius (0x44F820)
    ├── CountPlayerCharacters (0x44F950)
    └── CharacterListOperation (0x44FBB0) — removes from list
```

---

## sauSpawnObj Flow (sub_44C730)

The native handler for the `sauSpawnObj` script function:

1. **Pop arguments from VM stack** (reverse order of script push):
   - Object reference via `PopObjectRef` (sub_52C860)
   - Position vec3 via `PopVec3` (sub_52C770)
   - Rotation vec3 via `PopVec3` (sub_52C770)
2. **Call `SpawnCharacter`** (sub_44C600) on the spawner object
3. **Push success result** via `PushResult(1)` (sub_52CC70)

---

## SpawnCharacter (sub_44C600)

`__thiscall` on the spawner object. Steps:

1. Calls `CreateCharacter_Internal(assetRef)` (sub_44FA20) to create the character
2. Sets spawner reference at `character[139]` (offset 0x22C)
3. Calls vtable methods to initialize the character
4. Sets position via `sub_51AE30`

---

## CreateCharacter_Internal (sub_44FA20)

`__cdecl`, takes `int assetRef`. Core creation routine:

1. Checks `dword_730798` (character pool slot pointer) — returns 0 if null
2. Calls `sub_55BDC0(assetRef)` — resolves asset reference to character definition pointer
3. Calls `sub_4D6030(definition)` — instantiates the character object from definition
4. Links new character to the character list (`off_6E2830`)
5. Updates tracking globals:
   - `dword_7307BC`, `dword_7307B4`, `dword_7307C4`, `dword_7307CC`, `dword_7307D4`
6. Tracks memory allocation via `sub_4DE2A0` with `"Character Memory"` label
7. Returns character object pointer (`result - 4` due to hidden header)

---

## ResolveAssetRef (sub_55BDC0)

`__thiscall`, simple dereference:

```c
void* ResolveAssetRef(void *this, int ref)
{
    return *(this[5] + 4);  // asset definition pointer from reference
}
```

---

## SwitchPlayerCharacter (sub_493A80)

Handles switching between playable characters:

1. Creates name hashes for Mallory, Simon, Jared via `sub_418290` (name hash function)
2. Character type mapping:
   - `1` = Jared
   - `2` = Mallory
   - `3` = Simon
   - `4+` = ThimbleTack
3. Iterates character list to find matching character by name hash
4. Uses `vtable[19]` (`IsPlayerControlled`) check

### Character Tracking Globals

| Address | Purpose |
|---------|---------|
| `dword_D42CFC` | Jared character reference |
| `dword_D42D04` | Simon character reference |
| `dword_D42D0C` | Mallory character reference |
| `dword_D42D14` | Creation bitmask (bit 0 = Mallory, bit 1 = Simon, bit 2 = Jared) |

---

## Character List

The global character linked list is used by all character lookup operations.

| Property | Value |
|----------|-------|
| Head pointer | `g_CharacterListHead` at `0x7307D8` |
| Link type | Singly linked |
| Next pointer offset | `+0x5A0` (1440 bytes) |

### List Operations

| Address | Name | Purpose |
|---------|------|---------|
| `0x44F7C0` | FindClosestCharacter | Squared distance search |
| `0x44F820` | FindClosestCharacterInRadius | Distance search with max radius |
| `0x44F890` | GetPlayerCharacter | Iterate, vtable[19] check |
| `0x44F8F0` | GetPlayerCharacter2 | Iterate, vtable[20] check |
| `0x44F950` | CountPlayerCharacters | Count matching vtable[19] |
| `0x44FBB0` | CharacterListOperation | Remove character from list |

---

## Character Names in Binary

| Address | String | Context |
|---------|--------|---------|
| `0x6230D0` | "Simon" | Character name reference |
| `0x6230F8` | "Jared" | Character name reference |
| `0x629CAC` | "Mallory" | Character name reference |
| `0x629CA0` | "ThimbleTack" | Character name reference |
| `0x62B7E4` | "simon" (lowercase) | Hash/lookup key |
| `0x62B7F4` | "jared" (lowercase) | Hash/lookup key |
| `0x62B7EC` | "mallory" (lowercase) | Hash/lookup key |
| `0x62B414` | "ClCharacterObj" | Class name string |
| `0x62BC40` | "ClPlayerObj" | Class name string |

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0x730798` | dword | g_CharacterPoolSlot | Character pool slot pointer |
| `0x7307D8` | dword | g_CharacterListHead | Head of character linked list |
| `0x7307E0` | dword | g_CurrentCreatingChar | Character being created (set during CreateCharacter_Internal) |
| `0x7307BC` | dword | — | Character tracking counter |
| `0x7307B4` | dword | — | Character tracking counter |
| `0x7307C4` | dword | — | Character tracking counter |
| `0x7307CC` | dword | — | Character tracking counter |
| `0x7307D4` | dword | — | Character tracking counter |
| `0xD42CFC` | dword | — | Jared character reference |
| `0xD42D04` | dword | — | Simon character reference |
| `0xD42D0C` | dword | — | Mallory character reference |
| `0xD42D14` | dword | — | Character creation bitmask |

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x44C730` | sauSpawnObj handler | __cdecl | Native handler for sauSpawnObj script function |
| `0x44C600` | SpawnCharacter | __thiscall | Spawn character from asset ref + position + rotation |
| `0x44FA20` | CreateCharacter_Internal | __cdecl | Core character creation (allocate + init + register) |
| `0x55BDC0` | ResolveAssetRef | __thiscall | Resolve asset reference to definition pointer |
| `0x4D6030` | InstantiateCharacter | — | Create character object from definition |
| `0x493A80` | sauResolvePlayer | __cdecl | VM handler: ensure widgets created, resolve character by type |
| `0x44F7C0` | FindClosestCharacter | __cdecl | Find nearest character to position |
| `0x44F890` | GetPlayerCharacter | — | Find player-controlled character in list |
| `0x44FBB0` | CharacterListOperation | — | Remove character from linked list |

---

## Related Documentation

- [subs/sub_44C730_sauSpawnObj.md](subs/sub_44C730_sauSpawnObj.md) — sauSpawnObj native handler
- [subs/sub_44C600_SpawnCharacter.md](subs/sub_44C600_SpawnCharacter.md) — SpawnCharacter
- [subs/sub_44FA20_CreateCharacter_Internal.md](subs/sub_44FA20_CreateCharacter_Internal.md) — CreateCharacter_Internal
- [subs/sub_55BDC0_ResolveAssetRef.md](subs/sub_55BDC0_ResolveAssetRef.md) — ResolveAssetRef
- [subs/sub_493A80_sauResolvePlayer.md](subs/sub_493A80_sauResolvePlayer.md) --- sauResolvePlayer (character widget creation + resolution)
- [subs/sub_44F7C0_FindClosestCharacter.md](subs/sub_44F7C0_FindClosestCharacter.md) — FindClosestCharacter
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) — Object field layout
- [CHARACTER_SWITCHING.md](CHARACTER_SWITCHING.md) — Character switching system
- [../vm/KALLIS_VM.md](../vm/KALLIS_VM.md) — VM stack system (used for arg passing)
