# Character System

**Status:** Reversed (linked list, identification, player detection, creation, removal, widget hashing, hot-switch)

---

## Overview

The Spiderwick engine manages all in-game characters (player characters, NPCs, creatures) through a global singly-linked list. Each node is a `ClCharacterObj` (or subclass `ClPlayerObj`) with a next pointer at offset `+0x5A0`. The engine identifies the active player character by calling a virtual method (`IsPlayerControlled`, vtable[19]) on each node during iteration.

Characters are identified at runtime by **widget hashes** computed via `HashString` (sub_405380). Name strings like `"jared"`, `"simon"`, `"mallory"` are hashed to produce fast lookup keys used throughout the event, data store, and character switching systems.

---

## Character Linked List

| Property | Value |
|----------|-------|
| Head pointer | `g_CharacterListHead` at `0x7307D8` |
| Link type | Singly linked |
| Next pointer offset | `+0x5A0` (decimal 1440) in each character object |
| Terminator | `NULL` (0) |

### Iteration Pattern

Every function that needs to find a character uses the same pattern:

```c
ClCharacterObj* cur = g_CharacterListHead;  // [0x7307D8]
while (cur) {
    // ... check condition on cur ...
    cur = *(ClCharacterObj**)(cur + 0x5A0);  // cur->next
}
```

The list contains ALL character types: player characters (`ClPlayerObj`), NPCs, creatures, and companions. Only `ClPlayerObj` instances return `true` from `vtable[19]` (IsPlayerControlled).

### Runtime Example

```
g_CharacterListHead (0x7307D8)
  |
  +-> FireSalamander  vtable=0x62D934  (creature)
  |     +0x5A0 ->
  +-> WillOWisp       vtable=0x62C89C  (creature)
  |     +0x5A0 ->
  +-> SproutSprite    vtable=0x62D0CC  (creature)
  |     +0x5A0 ->
  +-> Mallory         vtable=0x62B9EC  (ClPlayerObj)
  |     +0x5A0 ->
  +-> NULL
```

---

## Character Object Layout (ClCharacterObj / ClPlayerObj)

**Full layout:** [ClCharacterObj_layout.md](ClCharacterObj_layout.md)

### Key Offsets Summary

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 0 | 0x00 | ptr | **vtable** | constructor |
| 4 | 0x04 | word | flags/type | constructor |
| 12 | 0x0C | ptr | class name string ptr ("ClPlayerObj"/"ClCharacterObj") | constructor |
| 40 | 0x28 | float[12] | 3x4 rotation matrix | SetTransform |
| 104 | 0x68 | float | **position X** | SetTransform, FindClosestCharacter |
| 108 | 0x6C | float | **position Y** | SetTransform, FindClosestCharacter |
| 112 | 0x70 | float | **position Z** | SetTransform, FindClosestCharacter |
| 172 | 0xAC | ptr | secondary vtable | constructor |
| 308 | 0x134 | ptr | render/mesh object | sub_451050 |
| 312 | 0x138 | ptr | animation object | sub_451050 |
| 316 | 0x13C | ptr | physics/collision object | sub_451050 |
| 440 | 0x1B8 | ptr | AI/controller object | sub_460050, sub_459070 |
| 448 | 0x1C0 | ptr | **name struct** (+4 = char* name) | sub_4628C0 |
| 456 | 0x1C8 | int | **state flags** (bitfield) | sub_451010 |
| 460 | 0x1CC | int | **state flags 2** (bitfield) | sub_451010, CharacterListOperation |
| 504 | 0x1F8 | int | **current health** | sub_4628C0 |
| 508 | 0x1FC | int | **max health** | sub_4628C0 |
| 872 | 0x368 | ptr[6] | **weapon/item slots** | sub_4628C0 |
| 1440 | 0x5A0 | ptr | **next character in linked list** | GetPlayerCharacter, all iterators |

### ClPlayerObj-Specific

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 1488 | 0x5D0 | byte | player type flag | ClPlayerObj_Constructor |
| 1580 | 0x62C | int | sentinel/invalid ID (-1) | ClPlayerObj_Constructor |

### Vtable Layout (Key Entries)

| Index | Offset | ClCharacterObj (0x62A0E4) | ClPlayerObj (0x62B9EC) |
|-------|--------|---------------------------|------------------------|
| [19] | +0x4C | sub_454330 (always FALSE) | sub_462E00 (IsPlayerControlled) |
| [20] | +0x50 | (base impl) | IsPlayableCharacter |

---

## Character Identification

### Widget Hash (HashString at 0x405380)

Characters are identified by hashing their name strings with `HashString`:

```c
int HashString(char* str) {
    int result = 0;
    for (char c = *str; c; c = *++str)
        result += c + (result << (c & 7));
    return result;
}
```

This hash is used for:
- Character switching (sub_493A80 hashes "jared", "simon", "mallory")
- Data store key lookups (e.g., "/Player/Character")
- Event matching and widget identification
- Cheat system input detection

See [sub_405380_HashString.md](sub_405380_HashString.md) for full analysis.

### CreateWidget (sub_418290)

The widget initialization function. Takes a name string, computes the hash, and stores it at `this[0]`. Sets a flag byte at `this[4]` to 1. **The name string is NOT stored** -- only the hash is retained. All subsequent comparisons use the hash value.

```c
void __thiscall CreateWidget(BYTE *this, char *name)
{
    int hash = 0;
    for each character c in name:
        hash += c + (hash << (c & 7));
    *(DWORD*)this = hash;  // store hash
    *(this + 4) = 1;       // initialized flag
}
```

See [subs/sub_418290_CreateWidget.md](subs/sub_418290_CreateWidget.md) for full analysis.

### WidgetHashToType (sub_44FEC0)

A `.kallis` thunk that converts a widget name hash to a player type number. Called during `SetPlayerType` to determine the type value for the data store. Dispatches to ROP code at `off_1C88E74`.

See [subs/sub_44FEC0_WidgetHashToType.md](subs/sub_44FEC0_WidgetHashToType.md) for full analysis.

### Character Names

| Character | Display Name | Hash Key (lowercase) | String Address |
|-----------|-------------|---------------------|----------------|
| Jared Grace | "Jared" (0x6230F8) | "jared" (0x62B7F4) | 0x62B7F4 |
| Simon Grace | "Simon" (0x6230D0) | "simon" (0x62B7E4) | 0x62B7E4 |
| Mallory Grace | "Mallory" (0x629CAC) | "mallory" (0x62B7EC) | 0x62B7EC |
| ThimbleTack | "ThimbleTack" (0x629CA0) | "ThimbleTack" (0x629CA0) | 0x629CA0 |

---

## IsPlayerControlled Mechanism

The engine does NOT store a simple boolean flag for "is this the player." Instead, it resolves through the data/script system at runtime.

### Call Chain

```
GetPlayerCharacter (0x44F890)
  for each character in linked list:
    call vtable[19] (offset +0x4C)
      |
      +-- ClCharacterObj::IsPlayerControlled (0x454330)
      |     -> always returns FALSE
      |
      +-- ClPlayerObj::IsPlayerControlled (0x462E00)
            1. facet::facet(this) -> create query from character ptr
            2. sub_53A2B0(query) -> resolve "component" via off_627800
            3. sub_539AC0(component) -> return component+0x31 (byte flag)
               This byte is TRUE only for the ACTIVE player character
```

### Key Insight

The "player controlled" state is managed externally by the Kallis script/data system, not stored directly in the character object. The component at `off_627800` only exists (and returns true) for the one character currently under player control.

---

## List Operations

### Query Functions

| Address | Name | Purpose | Vtable Check |
|---------|------|---------|--------------|
| `0x44F890` | [GetPlayerCharacter](sub_44F890_GetPlayerCharacter.md) | Return first player-controlled character | vtable[19] (+0x4C) |
| `0x44F920` | [NextPlayableCharacter](sub_44F920_NextPlayableChar.md) | Return next playable character after given one | vtable[20] (+0x50) |
| `0x44F950` | [CountPlayerCharacters](sub_44F950_CountPlayerCharacters.md) | Count all player-controlled characters | vtable[19] (+0x4C) |
| `0x44F7C0` | [FindClosestCharacter](sub_44F7C0_FindClosestCharacter.md) | Find nearest character to 3D position | (none, checks all) |
| `0x44F820` | FindClosestCharacterInRadius | Same with max radius | (none, checks all) |

### Mutation Functions

| Address | Name | Purpose |
|---------|------|---------|
| `0x44FA20` | [CreateCharacter_Internal](sub_44FA20_CreateCharacterInternal.md) | Allocate + init + register new character |
| `0x44FB50` | CreateCharacter (wrapper) | Duplicate-safe wrapper around Internal |
| `0x44FBB0` | [CharacterListOperation](sub_44FBB0_CharacterListOperation.md) | Remove character from linked list |

### Manager

| Address | Name | Purpose |
|---------|------|---------|
| `0x44FE90` | [GetCharacterManager](subs/sub_44FE90_GetCharacterManager.md) | Singleton getter for ClCharacterManagerImpl at 0x730754 |

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0x7307D8` | ptr | g_CharacterListHead | Head of character linked list |
| `0x730798` | ptr | g_CharacterPoolSlot | Character pool slot pointer for creation |
| `0x7307E0` | ptr | g_CurrentCreatingChar | Temporarily set during CreateCharacter_Internal |
| `0x730754` | obj | g_CharacterManager | ClCharacterManagerImpl singleton |
| `0x730758` | dword | g_CharManagerInitFlag | Init flag (bit 0) |
| `0x7307AC` | dword | — | Memory allocation tracking |
| `0x7307BC` | dword | — | Character tracking counter (decremented on create) |
| `0x7307B4` | dword | — | Character tracking counter (incremented on create) |
| `0x7307C4` | dword | — | Character tracking counter (high water mark) |
| `0x7307CC` | dword | — | Character memory high water mark |
| `0x7307D4` | dword | — | Character creation serial number |

### Character-Specific Globals

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0xD42CFC` | ptr | g_JaredRef | Jared character reference |
| `0xD42D04` | ptr | g_SimonRef | Simon character reference |
| `0xD42D0C` | ptr | g_MalloryRef | Mallory character reference |
| `0xD42D14` | dword | g_CharCreatedMask | Bitmask: bit 0=Mallory, bit 1=Simon, bit 2=Jared |

---

## Key Functions Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x405380` | [HashString](sub_405380_HashString.md) | __cdecl | Hash string for widget/key identification |
| `0x44F7C0` | [FindClosestCharacter](sub_44F7C0_FindClosestCharacter.md) | __cdecl | Find nearest character to 3D position |
| `0x44F890` | [GetPlayerCharacter](sub_44F890_GetPlayerCharacter.md) | __cdecl | Find player-controlled character |
| `0x44F920` | [NextPlayableCharacter](sub_44F920_NextPlayableChar.md) | __cdecl | Find next playable character after given one |
| `0x44F950` | [CountPlayerCharacters](sub_44F950_CountPlayerCharacters.md) | __cdecl | Count player-controlled characters |
| `0x44FA20` | [CreateCharacter_Internal](sub_44FA20_CreateCharacterInternal.md) | __cdecl | Allocate and register new character |
| `0x44FBB0` | [CharacterListOperation](sub_44FBB0_CharacterListOperation.md) | __cdecl | Remove character from linked list |
| `0x462E00` | [IsPlayerControlled](subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md) | __thiscall | ClPlayerObj vtable[19] override |
| `0x454330` | IsPlayerControlled_Base | __thiscall | ClCharacterObj vtable[19] (always FALSE) |
| `0x463BF0` | [ClPlayerObj_Constructor](subs/sub_463BF0_ClPlayerObj_Constructor.md) | __thiscall | ClPlayerObj constructor |
| `0x493A80` | [SwitchPlayerCharacter](subs/sub_493A80_SwitchPlayerCharacter.md) | -- | Switch active player by type |
| `0x44FE90` | [GetCharacterManager](subs/sub_44FE90_GetCharacterManager.md) | __cdecl | Singleton getter |
| `0x418290` | [CreateWidget](subs/sub_418290_CreateWidget.md) | __thiscall | Hash name string, store at this[0], set flag at this[4] |
| `0x44FEC0` | [WidgetHashToType](subs/sub_44FEC0_WidgetHashToType.md) | __cdecl (.kallis) | Widget hash to player type number |
| `0x4537B0` | [GetInputController](subs/sub_4537B0_GetInputController.md) | __thiscall | Returns this+300, the input controller block |

---

## Spawning

The entity spawning system creates and registers characters at runtime. Two VM entry points exist:

- **sauSpawnObj** (sub_44C730) -- pops 3 args (objRef, pos, rot) from VM stack
- **sauCreateCharacter** (sub_44C6C0) -- method on ClSpawnerObj, pops 1 arg, uses spawner's position

Both converge on **SpawnCharacter** (sub_44C600), which:
1. Calls `CreateCharacter_Internal` (sub_44FA20) -- pool allocation + factory dispatch + VMInit
2. Sets position/rotation via vtable[1] and sub_51AE30
3. Calls **vtable[2] (Activate)** for render system registration
4. Calls vtable[10] for setup with spawner data

Key discoveries:
- The **allocator is disabled** during gameplay (`byte_D577F4 = 1`); must temporarily enable for runtime spawns
- **Factory functions return `base+4`**; subtract 4 to get the vtable/object base
- The **factory table** at `0x6E8868` holds up to 50 entries (`{data, classId, factoryFunc}`)
- A **memcpy clone** is gameplay-functional but invisible -- render registration is separate from the linked list

Full documentation: [ENTITY_SPAWNING.md](ENTITY_SPAWNING.md)

### Spawning Functions

| Address | Name | Purpose |
|---------|------|---------|
| `0x44C730` | [sauSpawnObj](subs/sub_44C730_sauSpawnObj.md) | VM handler, pops 3 args |
| `0x44C6C0` | [sauCreateCharacter](subs/sub_44C6C0_sauCreateCharacter.md) | VM method on spawner |
| `0x44C600` | [SpawnCharacter](subs/sub_44C600_SpawnCharacter.md) | Actual spawn orchestration |
| `0x44FA20` | [CreateCharacter_Internal](subs/sub_44FA20_CreateCharacter_Internal.md) | Pool + factory + VMInit |
| `0x4D6030` | [FactoryDispatch](subs/sub_4D6030_FactoryDispatch.md) | ClassId lookup in factory table |
| `0x4D6110` | [ClPlayerObj_Factory](subs/sub_4D6110_ClPlayerObj_Factory.md) | Allocate 1592 + construct |
| `0x4DE530` | [GamePoolAllocator](subs/sub_4DE530_GamePoolAllocator.md) | Pool allocator with disabled flag |
| `0x55BDC0` | [GetAssetClassId](subs/sub_55BDC0_GetAssetClassId.md) | Reads field[5]+4 from VM object |

---

## Related Documentation

- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) -- Full object field layout
- [CHARACTER_CREATION.md](CHARACTER_CREATION.md) -- Character creation pipeline (spawn -> create -> register)
- [ENTITY_SPAWNING.md](ENTITY_SPAWNING.md) -- Entity spawning system (factory, allocator, discoveries)
- [CHARACTER_SWITCHING.md](CHARACTER_SWITCHING.md) -- Character switching system (/Player/Character state)
- [subs/sub_493A80_SwitchPlayerCharacter.md](subs/sub_493A80_SwitchPlayerCharacter.md) -- Uses name hashes for switching
- [subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md](subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md) -- Vtable[19] implementation
- [subs/sub_463BF0_ClPlayerObj_Constructor.md](subs/sub_463BF0_ClPlayerObj_Constructor.md) -- ClPlayerObj constructor
- [subs/sub_44FE90_GetCharacterManager.md](subs/sub_44FE90_GetCharacterManager.md) -- Manager singleton
- [HOT_SWITCH_SYSTEM.md](HOT_SWITCH_SYSTEM.md) -- Hot-switch mechanism (full native switching pipeline)
- [subs/sub_418290_CreateWidget.md](subs/sub_418290_CreateWidget.md) -- Widget creation (hash + flag)
- [subs/sub_44FEC0_WidgetHashToType.md](subs/sub_44FEC0_WidgetHashToType.md) -- Widget hash to type conversion
- [subs/sub_4537B0_GetInputController.md](subs/sub_4537B0_GetInputController.md) -- Input controller accessor
- [subs/sub_492030_sauInteractHandler.md](subs/sub_492030_sauInteractHandler.md) -- Interaction input handler
- [subs/sub_49A430_sauRegObjForCoopEvents.md](subs/sub_49A430_sauRegObjForCoopEvents.md) -- Coop events registration
- [subs/sub_44C3C0_ClearCoopEventsArray.md](subs/sub_44C3C0_ClearCoopEventsArray.md) -- Coop array clear
- [subs/sub_44C6C0_sauCreateCharacter.md](subs/sub_44C6C0_sauCreateCharacter.md) -- sauCreateCharacter (spawner method)
- [subs/sub_4D6030_FactoryDispatch.md](subs/sub_4D6030_FactoryDispatch.md) -- Factory table dispatcher
- [subs/sub_4D6110_ClPlayerObj_Factory.md](subs/sub_4D6110_ClPlayerObj_Factory.md) -- ClPlayerObj factory
- [subs/sub_4DE530_GamePoolAllocator.md](subs/sub_4DE530_GamePoolAllocator.md) -- Game pool allocator
- [subs/sub_55BDC0_GetAssetClassId.md](subs/sub_55BDC0_GetAssetClassId.md) -- Asset classId accessor
- [../vm/KALLIS_VM.md](../vm/KALLIS_VM.md) -- VM stack system (creation args, script interface)
