# sub_4D6110 -- ClPlayerObj_Factory

## Identity

| Field | Value |
|---|---|
| Address | `0x4D6110` |
| Calling Convention | `__cdecl` |
| Parameters | none (data pointer from table is unused in decompilation) |
| Return Value | `void*` -- allocated+constructed object at **base+4**, or NULL |
| Size | 37 bytes |
| Module | engine/objects |

## Purpose

Factory function for `ClPlayerObj`. Registered in the factory table at index 3 with classId `0xF6EA786C`. Allocates 1592 bytes (16-byte aligned) via `GamePoolAllocator`, calls the `ClPlayerObj` constructor, and returns `base + 4`.

## Decompiled (IDA)

```c
_BYTE *ClPlayerObj_Factory()
{
  _BYTE *v0;   // eax -- raw allocation
  _BYTE *v1;   // eax -- constructed object

  v0 = (_BYTE *)sub_4DE530(1592, 16);          // GamePoolAllocator(size=1592, align=16)
  if ( v0 && (v1 = ClPlayerObj_Constructor(v0)) != 0 )
    return v1 + 4;                               // RETURNS base+4 (NOT the vtable base!)
  else
    return 0;
}
```

## Key Details

- **Size 1592 bytes** -- this is the full `ClPlayerObj` object size (1488 base + 104 player-specific)
- **Alignment 16** -- required for SIMD-friendly matrix operations
- **Returns base+4** -- the factory return value is offset by 4 bytes from the true object base. The caller (`CreateCharacter_Internal`) compensates with `result - 4`. When calling this factory directly from a mod, you MUST apply the same adjustment.
- Constructor failure (returns NULL) causes the factory to return NULL, leaking the allocation

## Factory Table Entry

| Field | Value |
|-------|-------|
| Table index | 3 |
| Table offset | `0x6E8868 + 3*12 = 0x6E8890` region |
| classId | `0xF6EA786C` |
| Factory func | `0x4D6110` |
| Data ref | `0x6E8894` (xref) |

## Calls

| Address | Name | Purpose |
|---------|------|---------|
| `0x4DE530` | GamePoolAllocator | Allocate 1592 bytes, 16-byte aligned |
| `0x463BF0` | ClPlayerObj_Constructor | Initialize fields, set vtable + RTTI |

## Called By

- `FactoryDispatch` (sub_4D6030) when classId matches `0xF6EA786C`
- SpiderMod "Spawn ClPlayerObj" button (direct call)

## Related

- [sub_4D6030_FactoryDispatch.md](sub_4D6030_FactoryDispatch.md) -- factory table dispatcher
- [sub_4DE530_GamePoolAllocator.md](sub_4DE530_GamePoolAllocator.md) -- allocator
- [sub_463BF0_ClPlayerObj_Constructor.md](sub_463BF0_ClPlayerObj_Constructor.md) -- constructor
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Full spawn pipeline overview
