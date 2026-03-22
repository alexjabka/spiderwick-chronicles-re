# sub_55BDC0 -- GetAssetClassId

## Identity

| Field | Value |
|---|---|
| Address | `0x55BDC0` |
| Calling Convention | `__thiscall` |
| this | VM object (asset reference) |
| Parameters | none |
| Return Value | `int` -- classId (hash used for factory lookup) |
| Size | 7 bytes |
| Module | engine/objects |

## Purpose

Reads the classId from a VM object's asset data. This is the value that gets passed to `FactoryDispatch` (sub_4D6030) to determine which type of object to construct. Extremely simple -- just a double dereference.

## Decompiled (IDA)

```c
int __thiscall sub_55BDC0(_DWORD *this)
{
  return *(_DWORD *)(*(this + 5) + 4);
}
```

## Access Pattern

```
this          -- VM object base
  this[5]     -- pointer at offset +0x14 (field 5, asset data block)
    [5]+4     -- DWORD at offset +4 within asset data = classId
```

Example: for a `ClPlayerObj` asset reference, `this[5]+4` contains `0xF6EA786C`.

## Key Details

- Only 7 bytes of code -- the simplest function in the spawn pipeline
- `this` is the VM object that represents the asset/template reference
- `this[5]` (offset +20) points to an asset descriptor block
- The classId at `descriptor+4` is a hash that matches entries in the factory table at `0x6E8868`
- The "Dump VM Objects" button in SpiderMod reads this same path to display asset classIds

## Called By

- `CreateCharacter_Internal` (sub_44FA20) -- reads classId before calling FactoryDispatch
- `sub_47E040` -- another creation-related function (3 xrefs)

## Related

- [sub_44FA20_CreateCharacter_Internal.md](sub_44FA20_CreateCharacter_Internal.md) -- caller
- [sub_4D6030_FactoryDispatch.md](sub_4D6030_FactoryDispatch.md) -- consumes the classId
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Full spawn pipeline overview
