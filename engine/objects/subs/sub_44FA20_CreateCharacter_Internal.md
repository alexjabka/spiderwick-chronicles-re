# sub_44FA20 -- CreateCharacter_Internal

## Identity

| Field | Value |
|---|---|
| Address | `0x44FA20` |
| Calling Convention | `__cdecl` |
| Parameters | `int assetRef` -- VM object reference for the character template |
| Return Value | `int` -- pointer to the new character object (vtable base), or 0 |
| Size | 300 bytes |
| Cyclomatic Complexity | 6 |
| Module | engine/objects |

## Purpose

Low-level character creation routine. Allocates a pool slot, sets up a memory tracking context, resolves the asset's classId, dispatches to the appropriate factory function, links the new object to the VM system, and updates pool counters. Called by `SpawnCharacter` (sub_44C600) and the duplicate-safe wrapper at `0x44FB50`.

## Decompiled (IDA)

```c
int __cdecl CreateCharacter_Internal(int a1)
{
  int v1;    // esi -- pool slot
  int v3;    // eax
  int v4;    // eax -- factory result (at base+4)
  int i;     // ecx
  int v6;    // edi -- adjusted object ptr (base+0, where vtable lives)
  int v7;    // eax -- memory used
  int v8;    // ecx
  int v10;   // ecx
  int v11;   // [esp+8h] [ebp-4h] BYREF -- memory tracking baseline

  v1 = dword_730798;                                // g_CharacterPoolSlot
  if ( !dword_730798 )
    return 0;                                        // no free pool slot

  sub_4E0FB0(dword_730798);                          // pre-init slot
  *(float *)(v1 + 40) = 0.0;                        // clear slot fields
  *(_DWORD *)(v1 + 12) = 0;
  *(_DWORD *)(v1 + 20) = 0;
  *(_DWORD *)(v1 + 24) = 0;
  *(_DWORD *)(v1 + 28) = 0;
  *(_DWORD *)(v1 + 32) = 0;
  *(_BYTE *)(v1 + 36) = 0;
  *(_BYTE *)(v1 + 37) = 0;
  *(_DWORD *)(v1 + 44) = 0;
  dword_7307E0 = v1;                                // g_CurrentCreatingChar = slot

  v11 = dword_7307AC;                               // memory tracking baseline
  sub_4DE2A0(*(_DWORD *)(v1 + 16), &v11, "Character Memory");  // setup memory context

  v3 = sub_55BDC0(a1);                              // GetAssetClassId(assetRef) → classId
  v4 = sub_4D6030(v3);                              // FactoryDispatch(classId) → object at base+4

  // Walk the chain at result+8 looking for off_6E2830 (character list sentinel)
  for ( i = *(_DWORD *)(v4 + 8); i; i = *(_DWORD *)(i + 4) )
  {
    if ( (char **)i == off_6E2830 )
      break;
  }

  *(_DWORD *)(v4 + 456) |= 0x100000u;              // set flag bit 20

  v6 = v4 - 4;                                      // *** ADJUST: factory returns base+4 ***
  VMInitObject(a1, v4 - 4);                         // link VM object to native object
  sub_4DE2E0();                                      // finalize memory context

  // Update pool counters
  v7 = dword_7307AC - v11;                           // memory used for this character
  --dword_7307BC;                                    // decrement available slots
  v8 = dword_7307B4 + 1;
  if ( dword_7307C4 > ++dword_7307B4 )               // increment active count
    v8 = dword_7307C4;
  dword_7307C4 = v8;                                 // update peak count
  v10 = dword_7307CC;
  if ( dword_7307CC <= v7 )
    v10 = dword_7307AC - v11;
  ++dword_7307D4;                                    // increment creation serial
  dword_7307CC = v10;                                // update peak memory

  // Store in slot structure
  *(_DWORD *)(v1 + 12) = v6;                        // slot[3] = object ptr
  *(_DWORD *)(v1 + 24) = v7;                        // slot[6] = memory used
  *(_DWORD *)(v1 + 44) = dword_7307D4;              // slot[11] = serial number
  *(_WORD *)(v6 + 1162) = dword_7307D4;             // character+0x48A = serial

  sub_4E0E90(v1);                                    // post-init (advance to next slot)
  dword_7307E0 = 0;                                  // clear g_CurrentCreatingChar
  return v6;                                         // return object base (vtable ptr)
}
```

## Pool Slot Structure

The pool slot (pointed to by `dword_730798`) has these known fields:

| Offset | Type | Purpose |
|--------|------|---------|
| +12 | ptr | Character object pointer (set after creation) |
| +16 | int | Value passed to memory context setup |
| +20 | int | Cleared during init |
| +24 | int | Memory used (set after creation) |
| +28 | int | Cleared during init |
| +32 | int | Cleared during init |
| +36 | byte | Cleared during init |
| +37 | byte | Cleared during init |
| +40 | float | Cleared to 0.0 during init |
| +44 | int | Creation serial number (set after creation) |

## Key Globals

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0x730798` | ptr | g_CharacterPoolSlot | Current pool slot (null = no free slot) |
| `0x7307AC` | int | g_MemoryTracker | Memory allocation counter |
| `0x7307B4` | int | g_ActiveCharCount | Active character count |
| `0x7307BC` | int | g_AvailableSlots | Available slot count |
| `0x7307C4` | int | g_PeakCharCount | Peak active count |
| `0x7307CC` | int | g_PeakMemUsage | Peak memory usage per character |
| `0x7307D4` | int | g_TotalCreated | Monotonic creation serial |
| `0x7307E0` | ptr | g_CurrentCreatingChar | Set during creation, cleared after |

## Critical Details

1. **Factory returns base+4:** The factory dispatch (`sub_4D6030`) returns a pointer at `base+4`, not the true object base. This function compensates with `v6 = v4 - 4`. The vtable lives at offset 0 of `v6`.

2. **Character serial at +0x48A:** Each created character gets a 16-bit serial number written at offset `+0x48A` (decimal 1162).

3. **VMInitObject:** Links the VM script object (asset reference) to the native C++ object. This is essential for the script system to interact with the character.

4. **Flag bit 20:** `result[456] |= 0x100000` sets bit 20 of a flags DWORD at offset +456 relative to the factory result (or +452 relative to the true base). Purpose unclear but likely marks the object as "spawned" or "active".

## Wrapper at 0x44FB50

The public wrapper guards against duplicate creation:
```c
"Character %s was asked to be created, but it already is"
```

## Calls

| Address | Name | Purpose |
|---------|------|---------|
| `0x4E0FB0` | SlotPreInit | Clear/prepare pool slot |
| `0x4DE2A0` | SetupMemoryContext | "Character Memory" allocation tracking |
| `0x55BDC0` | GetAssetClassId | Read classId from VM object field[5]+4 |
| `0x4D6030` | FactoryDispatch | Look up classId, call matching factory |
| `0x52EC30` | VMInitObject | Link VM object to native object |
| `0x4DE2E0` | FinalizeMemoryContext | End memory tracking |
| `0x4E0E90` | SlotPostInit | Advance pool to next slot |

## Called By

- `SpawnCharacter` (sub_44C600)
- `CreateCharacter` wrapper (sub_44FB50)

## Related

- [sub_44C600_SpawnCharacter.md](sub_44C600_SpawnCharacter.md) -- caller (spawn orchestration)
- [sub_55BDC0_GetAssetClassId.md](sub_55BDC0_GetAssetClassId.md) -- reads classId from asset ref
- [sub_4D6030_FactoryDispatch.md](sub_4D6030_FactoryDispatch.md) -- factory table lookup
- [sub_4D6110_ClPlayerObj_Factory.md](sub_4D6110_ClPlayerObj_Factory.md) -- ClPlayerObj factory
- [sub_4DE530_GamePoolAllocator.md](sub_4DE530_GamePoolAllocator.md) -- memory allocator
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Full spawn pipeline overview
- [../CHARACTER_CREATION.md](../CHARACTER_CREATION.md) -- Character creation overview
