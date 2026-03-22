# SpiderMod Entity Spawning Implementation

**Status:** Experimental (factory spawn works, clone works, both invisible -- render registration needed)

---

## Overview

SpiderMod includes experimental entity spawning functionality in the Debug Tools section of the ImGui menu (`menu.cpp`). Three approaches have been tested:

1. **Direct factory call** -- allocates and constructs a raw `ClPlayerObj` via the factory
2. **Clone (memcpy)** -- copies an existing player character's full memory block
3. **Diagnostic tools** -- dump the factory table and VM object details

Results: both factory spawn and clone produce characters that exist in the engine's linked list and respond to gameplay queries, but are **invisible** because render system registration (vtable[2] Activate) is not called.

---

## Buttons

### [Spawn ClPlayerObj]

Calls `ClPlayerObj_Factory` (0x4D6110) directly, bypassing the full `CreateCharacter_Internal` pipeline. This creates a raw, unlinked character object.

**Implementation** (menu.cpp, line ~681):

```cpp
// 1. Get current player position (offset spawn +3 on X)
uintptr_t player = GetPlayerCharacter();
float px = ReadFloat(player + CHAR_POS_X) + 3.0f;

// 2. Temporarily enable allocator (disabled during gameplay)
uint8_t wasDisabled = ReadByte(0xD577F4);
WriteByte(0xD577F4, 0);

// 3. Call factory directly
typedef uintptr_t (__cdecl *FactoryFn)();
auto factory = reinterpret_cast<FactoryFn>(0x4D6110);
uintptr_t rawResult = factory();
uintptr_t newObj = rawResult ? (rawResult - 4) : 0;  // factory returns base+4

// 4. Restore allocator disabled flag
WriteByte(0xD577F4, wasDisabled);

// 5. Set position
WriteFloat(newObj + CHAR_POS_X, px);
WriteFloat(newObj + CHAR_POS_Y, py);
WriteFloat(newObj + CHAR_POS_Z, pz);

// 6. Insert into character linked list (prepend)
uintptr_t oldHead = Read<uintptr_t>(pCharacterListHead);
Write<uintptr_t>(newObj + CHAR_NEXT, oldHead);
Write<uintptr_t>(pCharacterListHead, newObj);
```

**Key details:**
- `byte_D577F4` (allocator disabled flag) must be temporarily set to 0
- Factory returns `alloc+4`; subtract 4 to get the real object base (where vtable lives)
- Manual linked list insertion (prepend to head at 0x7307D8)
- No VM object linking (no `VMInitObject` call)
- No render registration (no vtable[2] call)

**Result:** Object appears in the linked list, has correct vtable (0x62B9EC for ClPlayerObj), but is **invisible** -- no model, no collision, no animation.

### [Clone Player]

Allocates memory via `GamePoolAllocator` (0x4DE530) and performs a raw `memcpy` of 1592 bytes from the current player character.

**Implementation** (menu.cpp, line ~767):

```cpp
// 1. Temporarily enable allocator
WriteByte(0xD577F4, 0);

// 2. Allocate via game pool
typedef uintptr_t (__cdecl *AllocFn)(int, unsigned int);
auto gameAlloc = reinterpret_cast<AllocFn>(0x4DE530);
uintptr_t rawMem = gameAlloc(1592, 16);  // ClPlayerObj size, 16-byte aligned

// 3. Restore allocator
WriteByte(0xD577F4, 1);

// 4. Copy entire player object
memcpy((void*)rawMem, (void*)player, 1592);

// 5. Clear VM object link to prevent double-linking
Write<uintptr_t>(rawMem + 0xA8, 0);

// 6. Offset position +5 on X
WriteFloat(rawMem + CHAR_POS_X, px + 5.0f);

// 7. Clear player flags (make NPC)
Write<uint32_t>(rawMem + CHAR_FLAGS, 0);

// 8. Insert into linked list
uintptr_t oldHead = Read<uintptr_t>(pCharacterListHead);
Write<uintptr_t>(rawMem + CHAR_NEXT, oldHead);
Write<uintptr_t>(pCharacterListHead, rawMem);
```

**Key details:**
- Copies ALL 1592 bytes including vtable, RTTI, internal pointers
- Clears VM link at +0xA8 to prevent script system double-references
- Clears player flags at +0x1CC so clone is treated as NPC
- Does NOT clear render/mesh/animation object pointers (they point to source's data)

**Result:** Clone appears in linked list, is **gameplay-functional** (distance queries find it, it has valid vtable, AI could theoretically target it), but **invisible**. The render system does not know about it because vtable[2] (Activate) was never called.

### [Dump Factories]

Reads the factory table at `0x6E8868` and logs all registered entity types.

```cpp
int factoryCount = Read<int>(0x6E8848);
for (int i = 0; i < factoryCount && i < 64; i++) {
    uintptr_t base = 0x6E8868 + i * 12;
    uint32_t data    = Read<uint32_t>(base + 0);   // data pointer
    uint32_t classId = Read<uint32_t>(base + 4);   // type hash
    uint32_t func    = Read<uint32_t>(base + 8);   // factory function
}
```

Output format: `[index] classId=0x... func=0x... data=0x...`

### [Dump VM Objects]

Walks the character linked list and reads VM object details for each character:

- VM object hash, flags, native pointer
- Asset classId (what `sub_55BDC0` reads: `vmObj[5]+4`)
- Character pool state (slot, count, max, total)

---

## Addresses Used

| Address | Symbol | Purpose |
|---------|--------|---------|
| `0x4D6110` | ClPlayerObj_Factory | Factory function (allocate + construct) |
| `0x4DE530` | GamePoolAllocator | Pool allocator (size, alignment) |
| `0xD577F4` | g_AllocDisabled | Allocator disabled flag (1=blocked, 0=enabled) |
| `0x7307D8` | g_CharacterListHead | Character linked list head |
| `0x730798` | g_CharacterPoolSlot | Pool slot pointer |
| `0x7307B4` | g_ActiveCharCount | Active character count |
| `0x7307C4` | g_PeakCharCount | Peak character count |
| `0x7307D4` | g_TotalCreated | Total characters created |
| `0x6E8848` | g_FactoryCount | Factory table entry count |
| `0x6E8868` | g_FactoryTable | Factory table base (12 bytes/entry) |
| `0x44F890` | GetPlayerCharacter | Returns current player object |

---

## Results Summary

| Approach | In Linked List | Has Vtable | Responds to Queries | Has Visual | Script-Linked |
|----------|---------------|------------|-----------------------|-----------|---------------|
| Factory spawn | Yes | Yes (correct) | Yes | **No** | No |
| Clone (memcpy) | Yes | Yes (shared) | Yes | **No** | No (cleared) |
| Engine spawn (reference) | Yes | Yes | Yes | Yes | Yes |

---

## Why Invisible?

The character's visual representation (mesh, textures, animation) is managed by the **render system**, which is separate from the character linked list. The full engine spawn pipeline (`SpawnCharacter` / sub_44C600) calls:

1. `vtable[2](character)` -- **Activate** -- registers the character with the render system
2. `vtable[10](character, spawnerData)` -- **Setup** -- configures character-specific data

Without the Activate call, the render system never learns about the new object and never draws it.

Additionally, the factory-spawned object has no associated **asset data** (model, skeleton, textures). Even if Activate were called, there would be no visual data to render. A proper spawn requires a valid asset reference that points to loaded model data.

---

## Next Steps

1. **Call vtable[2] (Activate)** after factory construction to test render registration
2. **Obtain a valid asset reference** -- find or fabricate a VM object with correct `field[5]+4` classId so the factory dispatch resolves to the correct model data
3. **Use `CreateCharacter_Internal`** instead of the factory directly -- this handles VMInitObject, pool tracking, and memory context properly
4. **Investigate the animation branch** in SpawnCharacter (sub_44C600) for proper animation binding
5. **Test spawning existing character types** by reusing their VM object references

---

## Related Documentation

- [../../../engine/objects/ENTITY_SPAWNING.md](../../../engine/objects/ENTITY_SPAWNING.md) -- Full reverse-engineered spawn pipeline
- [../../../engine/objects/CHARACTER_SYSTEM.md](../../../engine/objects/CHARACTER_SYSTEM.md) -- Character linked list and identification
- [../../../engine/objects/CHARACTER_CREATION.md](../../../engine/objects/CHARACTER_CREATION.md) -- Character creation overview
- [../../../engine/objects/subs/sub_4D6110_ClPlayerObj_Factory.md](../../../engine/objects/subs/sub_4D6110_ClPlayerObj_Factory.md) -- Factory function
- [../../../engine/objects/subs/sub_4DE530_GamePoolAllocator.md](../../../engine/objects/subs/sub_4DE530_GamePoolAllocator.md) -- Allocator
- [../../../engine/objects/subs/sub_44C600_SpawnCharacter.md](../../../engine/objects/subs/sub_44C600_SpawnCharacter.md) -- SpawnCharacter (reference implementation)
