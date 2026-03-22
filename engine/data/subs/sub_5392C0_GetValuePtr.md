# sub_5392C0 — GetValuePtr

**Address:** `Spiderwick.exe+1392C0` (absolute: `005392C0`)
**Convention:** __thiscall (ecx = data store object at 0xE57F68)
**Returns:** int* (pointer to variable value)

## Signature
```c
int* __thiscall GetValuePtr(DataStore *this, int index)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| this (ecx) | DataStore* | Data store object (0xE57F68) |
| index | int | Entry index returned by HashLookup |

## Description
Returns a pointer to the integer value of a data store variable, given its index (as returned by `HashLookup`). The index is used to look up the value offset from the entry array, then the offset is applied to the data store object base to get the value pointer.

## Key Details
- `this` pointer is always the data store object at `0xE57F68`
- Uses the entry at `dword_E57F64 + index * 16`, reading the value offset from entry+12
- Returns a direct pointer to the stored integer value
- DO NOT call from EndScene during world transitions (data store may be in invalid state)

## Called By
- SpiderMod ASI (for reading game state — only safe during stable gameplay)
- Game systems that query variable values

## Related
- [sub_41E830_HashLookup.md](sub_41E830_HashLookup.md) — provides the index
- [../DATA_STORE.md](../DATA_STORE.md) — data store overview
