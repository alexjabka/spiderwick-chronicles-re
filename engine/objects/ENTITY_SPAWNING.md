# Entity Spawning System

**Status:** Reversed (full spawn pipeline, factory system, allocator, VM Steal working pipeline)

---

## Overview

The Spiderwick engine spawns characters (player characters, NPCs, creatures) through a pipeline that originates in Kallis VM script calls and terminates in a native factory system. Two VM entry points exist: `sauSpawnObj` (arbitrary position/rotation) and `sauCreateCharacter` (spawner-relative). Both converge on `SpawnCharacter` (sub_44C600), which calls `CreateCharacter_Internal` (sub_44FA20) to allocate from the character pool, dispatch through the factory table, link the VM object, and register the character with the engine.

---

## Architecture

```
Script Layer (Kallis VM)
├── sauSpawnObj(objRef, pos, rot)       — sub_44C730: pops 3 args, calls SpawnCharacter
└── sauCreateCharacter(assetRef)        — sub_44C6C0: __thiscall on ClSpawnerObj, uses spawner pos/rot

Native Spawn Pipeline
├── sub_44C600 (SpawnCharacter)         — creates character, sets pos/rot, calls Activate + setup
│   ├── sub_44FA20 (CreateCharacter_Internal)
│   │   ├── sub_55BDC0 (GetAssetClassId)    — reads field[5]+4 from VM object → classId
│   │   ├── sub_4D6030 (FactoryDispatch)    — looks up classId in 50-entry table → calls factory
│   │   │   └── e.g. sub_4D6110 (ClPlayerObj_Factory)
│   │   │       ├── sub_4DE530 (GamePoolAllocator) — allocates 1592 bytes (16-aligned)
│   │   │       └── sub_463BF0 (ClPlayerObj_Constructor) — zeroes fields, sets vtable + RTTI
│   │   ├── VMInitObject(assetRef, obj)     — links VM script object to native object
│   │   └── Updates pool counters (7307B4, 7307BC, 7307C4, 7307D4)
│   ├── vtable[1] (SetPosition)             — sets world position from pos vec3
│   ├── sub_51AE30 (SetRotation)            — sets rotation from rot vec3
│   ├── vtable[2] (Activate)                — render system registration
│   └── vtable[10] (Setup)                  — passes spawner field to character
└── VMReturnBool(1)                         — push success to VM return stack
```

---

## VM Entry Points

### sauSpawnObj (sub_44C730)

Native handler registered in the VM function dispatch table. Called from Kallis scripts to spawn a character at an explicit position and rotation.

```c
void sauSpawnObj_Handler(void)
{
    int objRef;  VMPopObjRef(&objRef);     // asset reference
    float pos[3]; VMPopVec3(pos);          // world position
    float rot[3]; VMPopVec3(rot);          // euler rotation
    SpawnCharacter(objRef, pos, rot);      // sub_44C600
    VMReturnBool(1);                       // always succeeds
}
```

See [subs/sub_44C730_sauSpawnObj.md](subs/sub_44C730_sauSpawnObj.md)

### sauCreateCharacter (sub_44C6C0)

VM native method on `ClSpawnerObj` (a world-placed spawner entity). Pops only 1 argument (the asset reference) and uses the spawner's own position/rotation:

```c
BOOL __thiscall sauCreateCharacter(int this)
{
    int assetRef; VMPopObjRef(&assetRef);
    float pos[3] = { *(this+0x68), *(this+0x6C), *(this+0x70) };  // spawner world pos
    float rot[3] = { 0.0f, 0.0f, *(this+0x80) };                  // spawner Y rotation only
    SpawnCharacter(this, assetRef, pos, rot);
    VMReturnBool(1);
}
```

See [subs/sub_44C6C0_sauCreateCharacter.md](subs/sub_44C6C0_sauCreateCharacter.md)

---

## SpawnCharacter (sub_44C600)

The actual spawn function. `__thiscall` on the spawner object. Orchestrates the full sequence:

1. **CreateCharacter_Internal(assetRef)** -- allocate and construct via factory
2. **Store spawner backref** at `character[139]` (offset 0x22C)
3. **vtable[1](pos)** -- SetPosition
4. **sub_51AE30(rot)** -- SetRotation
5. **vtable[2]()** -- Activate (render registration)
6. **vtable[10](spawner[9])** -- Setup with spawner data
7. Optional animation setup if `spawner[44]` is non-null (calls sub_5360F0, sub_535D60, sub_5274D0)

See [subs/sub_44C600_SpawnCharacter.md](subs/sub_44C600_SpawnCharacter.md)

---

## CreateCharacter_Internal (sub_44FA20)

Core creation routine. `__cdecl`, takes `int assetRef`. Returns character object pointer (adjusted by -4 from factory result).

### Steps

1. Read pool slot from `dword_730798` -- return 0 if null (no free slot)
2. Call `sub_4E0FB0(slot)` -- pre-init, clear base fields
3. Zero out slot fields: +40(float), +12, +20, +24, +28, +32, +36(byte), +37(byte), +44
4. Set `dword_7307E0 = slot` (g_CurrentCreatingChar)
5. Call `sub_4DE2A0(slot[4], &v11, "Character Memory")` -- memory context setup
6. Call `sub_55BDC0(assetRef)` -- read classId from VM object's `field[5]+4`
7. Call `sub_4D6030(classId)` -- factory dispatch, returns allocated+constructed object at base+4
8. Walk chain at `result+8` looking for `off_6E2830` (character list sentinel)
9. Set `result[456] |= 0x100000` -- mark flags
10. **Adjust pointer: `v6 = result - 4`** -- this is the actual game object base (where vtable lives)
11. Call `VMInitObject(assetRef, v6)` -- link VM object to native object
12. Call `sub_4DE2E0()` -- finalize memory context
13. Update pool counters:
    - `dword_7307BC--` (decrement available)
    - `dword_7307B4++` (increment active count)
    - `dword_7307C4 = max(dword_7307C4, dword_7307B4)` (high water mark)
    - `dword_7307D4++` (total created serial)
    - `dword_7307CC = max(dword_7307CC, memUsed)` (memory high water mark)
14. Store in slot: `slot[3] = v6`, `slot[6] = memUsed`, `slot[11] = serial`
15. Write serial to character: `*(short*)(v6 + 1162) = serial`
16. Call `sub_4E0E90(slot)` -- post-init
17. Clear `dword_7307E0 = 0`

See [subs/sub_44FA20_CreateCharacter_Internal.md](subs/sub_44FA20_CreateCharacter_Internal.md)

---

## Factory System (sub_4D6030)

A table-driven factory dispatcher. Looks up a classId in a 50-entry table at `0x6E8868`, calls the matching factory function.

### Table Layout

```
dword_6E8848 = entry count (up to 50)
Table at 0x6E8868: 12 bytes per entry
  +0: data pointer (passed as arg to factory func)
  +4: classId (hash, e.g. 0xF758F803)
  +8: factory function pointer
```

### Dispatch Logic

```c
int FactoryDispatch(int classId)
{
    for (int i = 0; i < dword_6E8848; i++) {
        if (table[i].classId == classId)
            return table[i].factoryFunc(table[i].data);
    }
    return 0;  // not found
}
```

### Known Factories

| Index | classId | Factory | Object Type | Size | Constructor |
|-------|---------|---------|-------------|------|-------------|
| 2 | `0xF758F803` | sub_4D60E0 | ClCharacterObj | 1488 bytes | sub_454060 |
| 3 | `0xF6EA786C` | sub_4D6110 | ClPlayerObj | 1592 bytes | sub_463BF0 |

See [subs/sub_4D6030_FactoryDispatch.md](subs/sub_4D6030_FactoryDispatch.md)

---

## ClPlayerObj_Factory (sub_4D6110)

Factory function for `ClPlayerObj`:

```c
void* ClPlayerObj_Factory()
{
    void* mem = GamePoolAllocator(1592, 16);   // sub_4DE530
    if (!mem) return 0;
    void* obj = ClPlayerObj_Constructor(mem);   // sub_463BF0
    if (!obj) return 0;
    return obj + 4;  // IMPORTANT: returns base+4, caller must subtract 4
}
```

**Critical detail:** Factory returns `ptr+4`, not the actual object base. `CreateCharacter_Internal` compensates with `v6 = result - 4`. When calling the factory directly from a mod, you must perform the same adjustment.

See [subs/sub_4D6110_ClPlayerObj_Factory.md](subs/sub_4D6110_ClPlayerObj_Factory.md)

---

## GamePoolAllocator (sub_4DE530)

Stack-based pool allocator used for all game object allocation during level loading.

### Key Globals

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0xD577F0` | ptr | g_CustomAllocator | If non-null, delegates to custom allocator vtable[0] |
| `0xD577EC` | int | g_AllocStackDepth | Stack index for nested allocation contexts |
| `0xD577F4` | byte | **g_AllocDisabled** | **1 = disabled (normal gameplay), 0 = enabled (loading)** |
| `0xD57730` | int | g_DefaultPoolCurrent | Default pool current pointer |
| `0xD57738` | int | g_DefaultPoolRemaining | Default pool bytes remaining |

### Allocation Logic

```c
void* GamePoolAllocator(int size, unsigned int alignment)
{
    if (g_CustomAllocator)
        return g_CustomAllocator->vtable[0](size, alignment);

    int roundedSize = ALIGN_UP(size, 16);

    int *current, *remaining;
    if (g_AllocStackDepth > 0) {
        current   = &stack[g_AllocStackDepth].current;
        remaining = &stack[g_AllocStackDepth].remaining;
    } else {
        if (g_AllocDisabled)  // byte_D577F4
            return NULL;      // ALLOCATION BLOCKED
        current   = &g_DefaultPoolCurrent;
        remaining = &g_DefaultPoolRemaining;
    }

    int padding = (*current % alignment) ? (alignment - *current % alignment) : 0;
    int total = padding + roundedSize;
    if (*remaining < total)
        return NULL;  // not enough space

    *current  += total;
    *remaining -= total;
    return *current - roundedSize;
}
```

### KEY DISCOVERY: Allocator Disabled During Gameplay

The allocator is **disabled** (`byte_D577F4 = 1`) during normal gameplay. It is only enabled during level loading. To spawn entities at runtime, the disabled flag must be **temporarily cleared** (`byte_D577F4 = 0`), then restored after the factory call.

See [subs/sub_4DE530_GamePoolAllocator.md](subs/sub_4DE530_GamePoolAllocator.md)

---

## GetAssetClassId (sub_55BDC0)

Trivial accessor that reads the classId from a VM object's asset data:

```c
int __thiscall GetAssetClassId(DWORD* this)
{
    return *(*(this + 5) + 4);  // this[5] is ptr, read DWORD at offset +4
}
```

The classId is what gets passed to `FactoryDispatch` to determine which type of object to construct.

See [subs/sub_55BDC0_GetAssetClassId.md](subs/sub_55BDC0_GetAssetClassId.md)

---

## Character Pool

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0x730798` | ptr | g_CharacterPoolSlot | Current slot pointer (null = no free slot) |
| `0x7307B4` | int | g_ActiveCharCount | Active character count (incremented on create) |
| `0x7307BC` | int | g_AvailableSlots | Available slots (decremented on create) |
| `0x7307C4` | int | g_PeakCharCount | High water mark for active count |
| `0x7307CC` | int | g_PeakMemUsage | High water mark for memory usage |
| `0x7307D4` | int | g_TotalCreated | Monotonic serial number for character creation |
| `0x7307D8` | ptr | g_CharacterListHead | Head of the character linked list |
| `0x7307E0` | ptr | g_CurrentCreatingChar | Temporarily set during creation |

---

## Key Discoveries

### 1. Allocator Disabled Flag

During normal gameplay, `byte_D577F4 = 1`, which causes `GamePoolAllocator` to return NULL for any allocation request when the stack depth is 0. The engine only enables allocation during level loading. To spawn at runtime:

```c
// Enable allocator
WriteByte(0xD577F4, 0);

// Call factory
uintptr_t result = ClPlayerObj_Factory();

// Restore disabled state
WriteByte(0xD577F4, 1);

// Adjust pointer (factory returns base+4)
uintptr_t obj = result - 4;
```

### 2. Factory Pointer Adjustment

All factory functions return `alloc + 4`, not the actual object base. The vtable lives at offset 0 of the real base, so `CreateCharacter_Internal` compensates: `v6 = factoryResult - 4`. Any code calling a factory directly MUST apply this adjustment.

### 3. Clone Experiment Results

A `memcpy` of 1592 bytes from an existing player character creates a **gameplay-functional but invisible** clone:
- The clone appears in the character linked list
- Engine iteration functions find it
- It responds to distance queries (FindClosestCharacter)
- **It has no visual representation** because render system registration is separate from the linked list

The render system uses `vtable[2]` (Activate) to register a character for rendering. Simply being in the linked list is not sufficient. The full spawn pipeline (SpawnCharacter) calls vtable[2] after creation, which is why engine-spawned characters are visible.

### 4. Missing Pieces for Full Runtime Spawn (SOLVED)

~~To achieve a fully functional runtime spawn (with visuals), the following are needed:~~
~~1. A valid **asset reference** (VM object with correct field[5]+4 classId)~~
~~2. Calling `vtable[2]` (Activate) after construction for render registration~~
~~3. Proper `VMInitObject` linking for the script system to recognize the entity~~
~~4. The character pool slot (`dword_730798`) must be valid (has a free entry)~~

**All of these have been solved via the VM Steal approach.** See section below and [ENTITY_SPAWN_PIPELINE.md](ENTITY_SPAWN_PIPELINE.md) for the full discovery journey and working pipeline.

### 5. VM Steal Breakthrough -- Working Runtime Spawn

The key discovery was that vtable[2] ("light" activate, sub_4546A0) is insufficient. It only registers existing render objects. Characters need vtable[14] ("full" activate, sub_458B30) which calls `RenderObjSetup` (sub_4584C0) to create render objects from the visual data that the VM Init script produces.

#### The Problem

- VM objects cannot be created at runtime (pool-resident, loaded at level init)
- All 1438 VM objects in a level are linked to native objects (0 free templates)
- Fake/cloned VM objects crash in VMInitObject

#### The Solution: VM Steal

Steal a VM object from an existing non-player character (donor):

```
1. Detach VM from donor:  donor+0xA8 = 0,  vmObj+0x10 = 0
2. Get classId:           GetAssetClassId(vmObj)  [__thiscall!]
3. Enable allocator:      byte_D577F4 = 0
4. Factory dispatch:      FactoryDispatch(classId) → result - 4 = newObj
5. Restore allocator:     byte_D577F4 = 1
6. VM Init:               VMInitObject(vmObj, newObj) → runs Init script → visuals created
7. Set position:          memcpy(newObj+0x68, &pos, 12)
8. Full Activate:         newObj->vtable[14](newObj) → sub_458B30 → VISIBLE!
```

#### Critical Requirements

- **Must execute in game update thread** (HookedCameraSectorUpdate), not EndScene thread
- **Destructive:** Each spawn sacrifices a donor NPC
- vtable[14] (not vtable[2]) is required for visibility

#### Remaining Issues

- Sector culling causes angle-dependent visibility
- Some spawns land at world origin
- Need VM object allocator for non-destructive spawning

See [ENTITY_SPAWN_PIPELINE.md](ENTITY_SPAWN_PIPELINE.md) for comprehensive documentation of the full discovery journey, all failed approaches, and the working pipeline.

### 6. Activation Path Comparison

| Feature | vtable[2] ActivateBase | vtable[14] FullActivate |
|---------|----------------------|------------------------|
| Address | 0x4546A0 (via 0x462250) | 0x458B30 (via 0x4621D0) |
| VM event setup | No | Yes |
| Scene node creation | No | Yes |
| **Render object setup** | **No** | **Yes** (sub_4584C0) |
| Sector registration | Yes | Yes |
| Child activation | No | Yes (6 slots) |
| Makes char visible | Only if render exists | **Always** (creates render) |

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x44C730` | [sauSpawnObj](subs/sub_44C730_sauSpawnObj.md) | __cdecl | VM native handler, pops 3 args |
| `0x44C6C0` | [sauCreateCharacter](subs/sub_44C6C0_sauCreateCharacter.md) | __thiscall | VM method on spawner, pops 1 arg |
| `0x44C600` | [SpawnCharacter](subs/sub_44C600_SpawnCharacter.md) | __thiscall | Actual spawn orchestration |
| `0x44FA20` | [CreateCharacter_Internal](subs/sub_44FA20_CreateCharacter_Internal.md) | __cdecl | Pool + factory + VMInit |
| `0x4D6030` | [FactoryDispatch](subs/sub_4D6030_FactoryDispatch.md) | __cdecl | ClassId lookup in 50-entry table |
| `0x4D6110` | [ClPlayerObj_Factory](subs/sub_4D6110_ClPlayerObj_Factory.md) | __cdecl | Allocate 1592 + construct |
| `0x4DE530` | [GamePoolAllocator](subs/sub_4DE530_GamePoolAllocator.md) | __cdecl | Stack-based pool, disabled flag |
| `0x55BDC0` | [GetAssetClassId](subs/sub_55BDC0_GetAssetClassId.md) | __thiscall | Reads field[5]+4 from VM object |
| `0x463BF0` | [ClPlayerObj_Constructor](subs/sub_463BF0_ClPlayerObj_Constructor.md) | __thiscall | Zeroes fields, sets vtable + RTTI |
| `0x454060` | ClCharacterObj_Constructor | __cdecl | Base class constructor |
| `0x51AE30` | SetRotation | -- | Sets rotation from vec3 |
| `0x4546A0` | [ActivateBase](subs/sub_4546A0_ActivateBase.md) | __thiscall | "Light" activate (vtable[2]) |
| `0x458B30` | [FullActivate](subs/sub_458B30_FullActivate.md) | __thiscall | "Full" activate -- creates render objects |
| `0x4621D0` | ClPlayerObj_sauActivateObj | __thiscall | vtable[14] wrapper, calls FullActivate |
| `0x462250` | ClPlayerObj_Activate | __thiscall | vtable[2] wrapper, calls ActivateBase + CameraDirtyFlag |
| `0x4584C0` | RenderObjSetup | -- | Creates render objects from Init data |
| `0x52EC30` | VMInitObject | __cdecl | Links VM to native, runs Init script |

---

## Related Documentation

- [ENTITY_SPAWN_PIPELINE.md](ENTITY_SPAWN_PIPELINE.md) -- **Full spawn pipeline RE (VM Steal breakthrough, all failures documented)**
- [CHARACTER_SYSTEM.md](CHARACTER_SYSTEM.md) -- Character linked list, identification, vtable layout
- [CHARACTER_CREATION.md](CHARACTER_CREATION.md) -- Earlier creation pipeline overview
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) -- Full object field layout
- [../vm/KALLIS_VM.md](../vm/KALLIS_VM.md) -- VM stack system (arg passing, VMInitObject)
- [../../mods/spidermod/docs/ENTITY_SPAWNING_MOD.md](../../mods/spidermod/docs/ENTITY_SPAWNING_MOD.md) -- SpiderMod spawn implementation
# Entity Spawn Pipeline -- Full Reverse Engineering Documentation

**Status:** Fully reversed (working VM Steal pipeline, all failure modes documented)

---

## Overview

This document records the complete chronological reverse engineering journey of the Spiderwick Chronicles entity spawning system. The engine ("Ogre" by Stormfront Studios) uses a tightly coupled pipeline where character visibility depends on the Kallis VM Init script creating visual data (mesh, skeleton, materials) before the native render system can register the object. Direct native-only spawning is impossible without VM participation.

The final working approach -- **VM Steal** -- detaches a VM object from a donor NPC, creates a fresh native object via the factory, links the stolen VM object to it, and runs the full activation path to achieve a visible, functional character.

---

## Discovery Journey (Chronological)

### Phase 1: Direct Factory Call (Invisible Clones)

**Hypothesis:** Call `ClPlayerObj_Factory` (0x4D6110) directly with allocator enabled, then `VMInitObject` + `vtable[2]` to activate.

**Result:** Hollow shell. The factory allocates 1592 bytes and runs `ClPlayerObj_Constructor` (0x463BF0), which zeroes fields and sets the vtable. But without a valid VM Init script running, the object has no visual data -- no mesh, no skeleton, no materials. The character exists in memory but is completely invisible.

**Key insight:** The factory creates the native container; visual data comes from the VM Init script.

---

### Phase 2: Memcpy Clone (Visual Stealing)

**Hypothesis:** `memcpy` 1592 bytes from an existing player character to get a pre-populated object with visual data.

**Result:** The clone appears in the character linked list and responds to distance queries (`FindClosestCharacter`). However, it shares render component pointers with the source character -- both point to the same mesh/material data. This causes:
- Visual stealing (source loses its appearance)
- Render system corruption (two objects sharing state)
- The clone is invisible or flickers depending on camera angle

**Key insight:** Render components at +0x368 (child components) and +0x4D8 (render array) are pointers, not inline data. Copying them creates aliased references.

---

### Phase 3: Render System Discovery

**Hypothesis:** Player characters use the render component array at +0x4D8/+0x4DC for visibility.

**Result:** Wrong. Player characters have +0x4D8 (render component count) **always set to 0**. Their visual rendering goes through an entirely different path:
- **Child components** at +0x368 through +0x37C (6 slots)
- **Activation scene node** at +0x138
- **Render bounds object** at +0x13C

Calling `CreateRenderComponents` (sub_4510E0) returns 0 for player characters. Calling `RenderObjSetup` (sub_4584C0) on a character without Init data crashes on the next frame.

**Key insight:** The render pipeline is initialized by the VM Init script, not by native code.

---

### Phase 4: Activation Paths Discovered

Two distinct activation paths were found:

#### vtable[2] -- "Light" Activate (sub_4546A0 via 0x462250)

Called by `SpawnCharacter` (sub_44C600). Performs:
- Sector registration via +0x1B8 pointer
- Scene node creation
- Render component positioning loop (+0x4D8/+0x4DC)
- `EntityRegister_SceneNodes` (sub_51B220)
- Attachment visitor
- Position copy

This path assumes visual data already exists. On a character that has been through Init, it registers the existing visuals with the sector/scene system. On a character without Init, it creates empty scene nodes -- the character remains invisible.

#### vtable[14] -- "Full" Activate (sub_458B30 via 0x4621D0)

Called by `sauActivateObj` VM handler. The **critical** activation path that makes characters visible. Performs:
- vtable[16] validation check
- .kallis trampoline (VM event handler setup)
- VM event registration (sub_52EDB0, sub_52F300, sub_52F9A0 x4, sub_52F570)
- Scene node creation (sub_556170)
- **Render object setup** (sub_4584C0) -- the step that creates the visual representation
- Scene registration (sub_556200)
- Sector registration (sub_53A470 x2)
- Child activation (iterates 6 slots at +0x368)
- Flag updates

**Key insight:** vtable[14] is what makes characters VISIBLE. It runs the full render pipeline setup that vtable[2] skips.

---

### Phase 5: VM System Deep Dive

#### VM Object Pool

VM objects live in a contiguous pool at `g_VMObjectBase` (0xE56160). Each object has a fixed header:

```
Offset  Size  Field
+0x00   4     Magic: 0x004A424F ("OBJ\0")
+0x04   4     Class hash (from classDef+0)
+0x08   4     Unique instance identifier
+0x0C   4     Resolved name string pointer
+0x10   4     Native object pointer (0 = unlinked)
+0x14   4     Class definition pointer
+0x18   2     Flags DWORD (low word)
+0x1A   2     Flags word (bit 0x4000 = initialized)
```

#### VM-Native Linking

`VMInitObject` (0x52EC30) performs bidirectional linking:
1. Writes `nativeObj` pointer into vmObj+0x10
2. Writes `vmObj` pointer into nativeObj+0xA8
3. Runs the "Init" script on the VM object's class definition

The Init script is where all visual data is created: mesh loading, skeleton binding, material assignment, animation setup.

#### Pool Constraints

- VM objects **must** live in the pool -- external allocation crashes because `GetAssetClassId` (sub_55BDC0) uses pool-relative addressing
- Fake VM objects (VirtualAlloc outside pool) crash in `GetAssetClassId`
- Fake VM objects (heap-allocated, zeroed, placed in pool) crash in `VMInitObject`
- Cloned VM objects (memcpy from existing) crash in `VMInitObject`

#### Pool Survey (MansionD Level)

- 1438 VM objects found in the pool
- ALL are linked to native objects (vmObj+0x10 != 0)
- 0 free character templates available
- No unlinked VM objects suitable for spawning

---

### Phase 6: The Breakthrough -- VM Steal

**The working approach:** Steal a VM object from an existing non-player character (the "donor"), then use it to spawn a new character.

#### Pipeline

```
1. SELECT DONOR
   └── Find NPC character with linked VM object (donor+0xA8 != 0)

2. DETACH VM FROM DONOR
   ├── vmObj = *(donor + 0xA8)
   ├── *(donor + 0xA8) = 0          // donor loses VM link
   └── *(vmObj + 0x10) = 0          // VM object becomes "unlinked"

3. READ CLASS ID
   └── classId = GetAssetClassId(vmObj)    // sub_55BDC0, __thiscall!

4. ENABLE ALLOCATOR
   └── WriteByte(0xD577F4, 0)       // clear g_AllocDisabled

5. FACTORY DISPATCH
   ├── result = FactoryDispatch(classId)   // sub_4D6030
   └── newObj = result - 4                  // factory returns base+4

6. RESTORE ALLOCATOR
   └── WriteByte(0xD577F4, 1)

7. VM INIT
   └── VMInitObject(vmObj, newObj)   // sub_52EC30
       └── Runs Init script → creates mesh, skeleton, materials

8. SET POSITION
   └── memcpy(newObj+0x68, &pos, 12)  // XYZ floats

9. FULL ACTIVATE
   └── newObj->vtable[14](newObj)    // sub_4621D0 → sub_458B30
       └── Render setup, sector registration → CHARACTER IS VISIBLE

10. REGISTER IN CHARACTER LIST
    └── Check if FullActivate already inserted (it may auto-insert)
    └── If not: manually link into list at 0x7307D8
```

#### Why This Works

- The stolen VM object has a valid class definition with Init bytecode
- VMInitObject recognizes it as a legitimate VM object (correct magic, pool-resident)
- The Init script creates all visual data (mesh/skeleton/materials) on the new native object
- vtable[14] FullActivate registers the new visuals with the render/sector system
- The character becomes visible and interactive

#### Constraints

- **Destructive:** Each spawn sacrifices a donor character (the donor loses its VM link and becomes a hollow shell)
- **Must run in game update context:** Spawning from the EndScene hook thread crashes on the next frame. Must be deferred to `HookedCameraSectorUpdate` or equivalent game-thread callback.
- **Donor selection matters:** The donor's class determines what gets spawned (its Init script defines the visual type)

---

## Key Addresses

### Spawn Pipeline Functions

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x44FA20` | CreateCharacter_Internal | __cdecl | Pool + factory + VMInit |
| `0x44C600` | SpawnCharacter | __thiscall | Full spawn orchestration (position, activate, setup) |
| `0x52EC30` | VMInitObject | __cdecl | Links VM object to native, runs Init script |
| `0x4D6030` | FactoryDispatch | __cdecl | ClassId lookup in 50-entry table |
| `0x55BDC0` | GetAssetClassId | **__thiscall** | Reads classDef+4 from VM object |
| `0x4D6110` | ClPlayerObj_Factory | __cdecl | Allocates 1592 bytes, constructs ClPlayerObj |

### Activation Functions

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x4546A0` | ActivateBase (vtable[2]) | __thiscall | "Light" activate -- sector/scene registration |
| `0x462250` | ClPlayerObj_Activate (vtable[2] wrapper) | __thiscall | Calls ActivateBase + CameraDirtyFlag |
| `0x458B30` | FullActivate (sub_458B30) | __thiscall | "Full" activate -- VM events + render setup + sector |
| `0x4621D0` | ClPlayerObj_sauActivateObj (vtable[14]) | __thiscall | Calls FullActivate |
| `0x4584C0` | RenderObjSetup | -- | Creates render representation |
| `0x4510E0` | CreateRenderComponents | -- | Returns 0 for player characters |
| `0x51B220` | EntityRegister_SceneNodes | -- | Registers scene nodes with sector system |
| `0x436910` | CameraDirtyFlag | -- | Marks camera as needing update |

### VM System

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x52EC30` | VMInitObject | __cdecl | VM-native bidirectional link + Init script |
| `0x52EDB0` | VM event handler setup 1 | -- | Called by FullActivate |
| `0x52F300` | VM event handler setup 2 | -- | Called by FullActivate |
| `0x52F9A0` | VM event handler setup 3 | -- | Called by FullActivate (x4) |
| `0x52F570` | VM event handler setup 4 | -- | Called by FullActivate |

### Allocator & Pool

| Address | Name | Type | Purpose |
|---------|------|------|---------|
| `0x4DE530` | GamePoolAllocator | function | Stack-based pool allocator |
| `0xD577F4` | g_AllocDisabled | byte | 1 = disabled (gameplay), 0 = enabled (loading) |
| `0xE56160` | g_VMObjectBase | ptr | Base of VM object pool |
| `0x730798` | g_CharacterPoolSlot | ptr | Current character pool slot |
| `0x7307B4` | g_ActiveCharCount | int | Active character count |
| `0x7307D8` | g_CharacterListHead | ptr | Head of character linked list |

### Scene & Sector

| Address | Name | Purpose |
|---------|------|---------|
| `0x556170` | SceneNode_Create | Creates scene graph node |
| `0x556200` | SceneNode_Register | Registers node with scene |
| `0x53A470` | Sector_Register | Registers entity with sector system |

---

## Character Object Layout (Key Offsets)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| +0x00 | 4 | ptr | vtable pointer |
| +0x28 | 64 | float[16] | Transform matrix (used by sector registration) |
| +0x68 | 4 | float | Position X |
| +0x6C | 4 | float | Position Y |
| +0x70 | 4 | float | Position Z |
| +0x9C | 4 | ptr | Scene graph node 1 |
| +0xA0 | 4 | ptr | Scene graph node 2 |
| +0xA8 | 4 | ptr | VM object link |
| +0x138 | 4 | ptr | Activation scene node |
| +0x13C | 4 | ptr | Render bounds object |
| +0x1B8 | 4 | ptr | Sector/scene data pointer |
| +0x1C0 | 4 | ptr | Widget/entity descriptor |
| +0x1CC | 4 | DWORD | Flags (bit 2 = player) |
| +0x22C | 4 | ptr | Spawner back-reference (char[139]) |
| +0x368 | 24 | ptr[6] | Child components (6 slots, through +0x37C) |
| +0x4D8 | 4 | int | Render component count (always 0 for players) |
| +0x4DC | 12 | ptr[3] | Render component array (+0x4DC, +0x4E0, +0x4E4) |
| +0x5A0 | 4 | ptr | Linked list next pointer |

---

## VM Object Structure (28+ bytes header)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| +0x00 | 4 | DWORD | Magic: `0x004A424F` ("OBJ\0") |
| +0x04 | 4 | DWORD | Class hash (from classDef+0) |
| +0x08 | 4 | DWORD | Unique instance identifier |
| +0x0C | 4 | ptr | Resolved name string pointer |
| +0x10 | 4 | ptr | Native object pointer (0 = unlinked) |
| +0x14 | 4 | ptr | Class definition pointer |
| +0x18 | 2 | WORD | Flags (low word) |
| +0x1A | 2 | WORD | Flags (bit 0x4000 = initialized) |

---

## Class Definition Structure

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| +0x00 | 4 | DWORD | Class hash |
| +0x04 | 4 | DWORD | classId (used by FactoryDispatch) |
| +0x08 | 4 | int | Count (methods?) |
| +0x0C | 4 | DWORD | `0xBBBB0001` (sentinel + flags) |
| +0x10 | 4 | int | Bytecode offset/size |
| +0x14 | 4 | ptr | Method table pointer |

### Known classIds

| classId | Class Name |
|---------|-----------|
| `0xF6EA786C` | ClPlayerObj |
| `0xF758F803` | ClCharacterObj |

---

## Documented Failures (What Does NOT Work)

### 1. Direct Factory Call

**Method:** `FactoryDispatch(classId)` with allocator enabled, no VM.

**Failure:** Invisible. No Init script runs, so no mesh/skeleton/materials are created. The object is a hollow native container.

### 2. Memcpy Clone

**Method:** `memcpy(newMem, existingChar, 1592)`.

**Failure:** Invisible or visual stealing. The clone shares render component pointers with the source. Both objects reference the same mesh/material instances. The source may lose its visual.

### 3. CreateRenderComponents (sub_4510E0)

**Method:** Call `CreateRenderComponents` on a factory-created character.

**Failure:** Returns 0 for player character class. This function is not the path used by players.

### 4. RenderObjSetup (sub_4584C0) Without Init

**Method:** Call `RenderObjSetup` on a character that never had Init script run.

**Failure:** Crash on next frame. The function expects mesh/skeleton data that Init creates.

### 5. ActivateBase (vtable[2]) Without Init

**Method:** Call vtable[2] (ActivateBase, sub_4546A0) on a character without Init.

**Failure:** Invisible. Creates empty scene nodes. The sector system registers the object but there is nothing to render.

### 6. Fake VM Object (VirtualAlloc Outside Pool)

**Method:** Allocate memory outside the VM pool, fill in the header fields, pass to `GetAssetClassId`.

**Failure:** Crash in `GetAssetClassId` (sub_55BDC0). This function is `__thiscall` and uses pool-relative addressing. Pointers outside the pool calculate invalid offsets.

### 7. Fake VM Object (Heap, Zeroed, In Pool Range)

**Method:** Allocate on the heap, zero-fill, manually place within the pool address range.

**Failure:** Crash in `VMInitObject`. The VM system validates internal structure beyond just the magic number.

### 8. Cloned VM Object (Memcpy From Existing)

**Method:** `memcpy` a valid VM object to create a second copy.

**Failure:** Crash in `VMInitObject`. The VM system detects duplicate instance IDs or inconsistent pool state.

### 9. Spawn From EndScene Thread

**Method:** Execute the spawn pipeline from within the D3D9 EndScene hook.

**Failure:** Crash on next frame. The engine's render and game update systems run on different contexts. Spawning must happen during the game update tick, not during the render pass.

---

## What WORKS: VM Steal + Deferred Execution

### Requirements

1. **Donor character:** An existing NPC with a linked VM object (nativeObj+0xA8 != 0)
2. **Execution context:** Must run in the game update thread (e.g., `HookedCameraSectorUpdate`)
3. **Allocator enabled:** Temporarily clear `g_AllocDisabled` (0xD577F4) for the factory call

### Full Pipeline (Pseudocode)

```c
void SpawnViaVMSteal(void* donor, float x, float y, float z)
{
    // 1. Detach VM object from donor
    DWORD* vmObj = *(DWORD**)(donor + 0xA8);
    *(DWORD*)(donor + 0xA8) = 0;     // donor loses VM link
    *(DWORD*)((BYTE*)vmObj + 0x10) = 0;  // VM object becomes unlinked

    // 2. Get class ID (__thiscall!)
    typedef int (__thiscall *GetAssetClassId_t)(DWORD*);
    auto GetAssetClassId = (GetAssetClassId_t)0x55BDC0;
    int classId = GetAssetClassId(vmObj);

    // 3. Enable allocator temporarily
    *(BYTE*)0xD577F4 = 0;

    // 4. Factory dispatch
    typedef int (__cdecl *FactoryDispatch_t)(int);
    auto FactoryDispatch = (FactoryDispatch_t)0x4D6030;
    int result = FactoryDispatch(classId);
    uintptr_t newObj = result - 4;  // factory returns base+4

    // 5. Restore allocator
    *(BYTE*)0xD577F4 = 1;

    // 6. VM Init (links VM to native, runs Init script → creates visuals)
    typedef void (__cdecl *VMInitObject_t)(DWORD*, uintptr_t);
    auto VMInitObject = (VMInitObject_t)0x52EC30;
    VMInitObject(vmObj, newObj);

    // 7. Set position
    *(float*)(newObj + 0x68) = x;
    *(float*)(newObj + 0x6C) = y;
    *(float*)(newObj + 0x70) = z;

    // 8. Full Activate (vtable[14]) → VISIBLE!
    DWORD vtable = *(DWORD*)newObj;
    typedef void (__thiscall *FullActivate_t)(uintptr_t);
    auto FullActivate = (FullActivate_t)(*(DWORD*)(vtable + 14 * 4));
    FullActivate(newObj);

    // 9. Check character list insertion (FullActivate may auto-insert)
    // If not in list, manually link at g_CharacterListHead (0x7307D8)
}
```

### Execution Context

The spawn **must** be deferred to the game update thread. In SpiderMod, this is done by:
1. Setting a flag in the EndScene hook (UI thread) when the user requests a spawn
2. Checking the flag in `HookedCameraSectorUpdate` (game thread)
3. Executing the full pipeline in the game thread callback
4. Clearing the flag after completion

---

## Known Issues

| Issue | Description | Cause |
|-------|-------------|-------|
| Angle-dependent visibility | Characters visible only from certain camera angles | Sector culling -- the spawned character may not be registered in all required sectors |
| World origin spawn | Some characters spawn at (0,0,0) instead of specified position | Position must be set before FullActivate, and transform matrix at +0x28 may also need updating |
| Destructive spawning | Each spawn sacrifices a donor NPC | VM objects cannot be created at runtime; they are loaded during level init |
| No runtime VM allocation | Cannot create new VM objects at runtime | Need to reverse the VM object allocator for non-destructive spawning |

---

## RE Methodology Notes

### SEH-Protected Logging

Step-by-step logging with `__try`/`__except` on each engine call was critical for isolating crashes. Example pattern:

```c
__try {
    LogFile("Calling GetAssetClassId...");
    classId = GetAssetClassId(vmObj);
    LogFile("GetAssetClassId returned: 0x%08X", classId);
} __except(EXCEPTION_EXECUTE_HANDLER) {
    LogFile("CRASH in GetAssetClassId! Exception: 0x%08X", GetExceptionCode());
    return;
}
```

This pattern revealed that `GetAssetClassId` (sub_55BDC0) is `__thiscall`, not `__cdecl`. Calling it with the wrong convention corrupts ECX and crashes. The decompiler shows `this` but it is easy to miss when calling from C.

### Iterative Narrowing

Each crash was isolated to a single function call using SEH, then that function was decompiled and analyzed in IDA to understand its preconditions. This progressive narrowing -- broad try/catch, then targeted decompilation -- was more efficient than trying to understand the full pipeline top-down.

---

## Related Documentation

- [ENTITY_SPAWNING.md](ENTITY_SPAWNING.md) -- Original spawn system documentation
- [CHARACTER_SYSTEM.md](CHARACTER_SYSTEM.md) -- Character linked list and identification
- [CHARACTER_CREATION.md](CHARACTER_CREATION.md) -- Earlier creation pipeline overview
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) -- Full object field layout
- [subs/sub_458B30_FullActivate.md](subs/sub_458B30_FullActivate.md) -- FullActivate function details
- [subs/sub_4546A0_ActivateBase.md](subs/sub_4546A0_ActivateBase.md) -- ActivateBase function details
- [subs/sub_44FA20_CreateCharacter_Internal.md](subs/sub_44FA20_CreateCharacter_Internal.md) -- CreateCharacter_Internal
- [subs/sub_4D6030_FactoryDispatch.md](subs/sub_4D6030_FactoryDispatch.md) -- Factory dispatch system
- [../vm/KALLIS_VM.md](../vm/KALLIS_VM.md) -- VM system (bytecode, opcodes, object system)
- [../../mods/spidermod/docs/ENTITY_SPAWNING_MOD.md](../../mods/spidermod/docs/ENTITY_SPAWNING_MOD.md) -- SpiderMod implementation
