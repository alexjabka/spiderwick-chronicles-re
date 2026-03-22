# sub_4D6030 -- FactoryDispatch

## Identity

| Field | Value |
|---|---|
| Address | `0x4D6030` |
| Calling Convention | `__cdecl` |
| Parameters | `int classId` -- hash identifying the object type |
| Return Value | `int` -- newly allocated+constructed object ptr (at base+4), or 0 on failure |
| Size | 68 bytes |
| Module | engine/objects |

## Purpose

Table-driven factory dispatcher. Looks up a classId in a global 50-entry table at `0x6E8868` and calls the matching factory function to allocate and construct the object. This is the central object creation dispatch point for the Spiderwick engine.

## Decompiled (IDA)

```c
int __cdecl sub_4D6030(int a1)
{
  int v1;       // eax -- loop index
  int *i;       // ecx -- pointer into classId column

  v1 = 0;
  if ( dword_6E8848 <= 0 )                       // entry count
    return 0;
  for ( i = &dword_6E886C; *i != a1; i += 3 )    // walk classId column (stride 12 bytes)
  {
    if ( ++v1 >= dword_6E8848 )
      return 0;                                    // classId not found
  }
  // Found: call factory function with data pointer as argument
  return ((int (__cdecl *)(char *))*(&off_6E8870 + 3 * v1))((&off_6E8868)[3 * v1]);
}
```

## Table Layout

```
dword_6E8848: int -- entry count (runtime value, up to 50)

Table base: 0x6E8868
Entry size: 12 bytes (3 DWORDs)

Per entry:
  +0x00: data pointer   (passed as argument to factory function)
  +0x04: classId        (hash, e.g. 0xF758F803)
  +0x08: factoryFunc    (function pointer)
```

The table is populated during level loading. The loop starts at `&dword_6E886C` (offset +4 from table base) to check classIds, with stride 3 (12 bytes).

## Known Factory Entries

| Index | classId | Factory Address | Object Type | Size | Constructor |
|-------|---------|-----------------|-------------|------|-------------|
| 2 | `0xF758F803` | sub_4D60E0 | ClCharacterObj | 1488 bytes | sub_454060 |
| 3 | `0xF6EA786C` | sub_4D6110 | ClPlayerObj | 1592 bytes | sub_463BF0 |

The full table contains up to 50 entries covering all entity types in the engine (creatures, items, triggers, etc.).

## Key Globals

| Address | Type | Purpose |
|---------|------|---------|
| `0x6E8848` | int | Entry count |
| `0x6E8868` | array | Table base (12 bytes/entry: data, classId, factoryFunc) |
| `0x6E886C` | int* | First classId (table_base + 4) |
| `0x6E8870` | ptr* | First factoryFunc (table_base + 8) |

## Important Notes

- **Factory functions return `alloc + 4`**, not the true object base. The caller (`CreateCharacter_Internal`) adjusts with `result - 4` to get the vtable pointer.
- ClassIds are hashes, not sequential indices. They are derived from asset type names.
- If no matching classId is found, returns 0 (null). `CreateCharacter_Internal` does not check for this, which could cause a crash if an invalid asset reference is used.
- The "Dump Factories" button in SpiderMod reads this table to enumerate all registered entity types.

## Called By

- `CreateCharacter_Internal` (sub_44FA20) -- the only known caller in the spawn pipeline

## Xrefs

- Code refs from: `0x44FA90` (CreateCharacter_Internal), `0x4D7775`
- Data refs from: `0x6E8848` (entry count global)

## Related

- [sub_44FA20_CreateCharacter_Internal.md](sub_44FA20_CreateCharacter_Internal.md) -- caller
- [sub_4D6110_ClPlayerObj_Factory.md](sub_4D6110_ClPlayerObj_Factory.md) -- ClPlayerObj factory (entry 3)
- [sub_4DE530_GamePoolAllocator.md](sub_4DE530_GamePoolAllocator.md) -- allocator used by factories
- [sub_55BDC0_GetAssetClassId.md](sub_55BDC0_GetAssetClassId.md) -- provides the classId argument
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Full spawn pipeline overview
