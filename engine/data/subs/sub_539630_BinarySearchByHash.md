# sub_539630 — BinarySearchByHash

**Address:** `Spiderwick.exe+139630` (absolute: `00539630`)
**Convention:** __cdecl
**Returns:** int (index, or -1 if not found)

## Signature
```c
int __cdecl BinarySearchByHash(DWORD hash)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| hash | DWORD | Pre-computed hash value to search for |

## Description
Performs a binary search in the sorted hash array at `dword_E57F64` to find the entry matching the given hash. The array is sorted by the hash field (offset +0 of each 16-byte entry), enabling O(log n) lookup.

## Key Details
- Searches `dword_E57F54` entries in the `dword_E57F64` array
- Each entry is 16 bytes; hash is at offset +0
- Standard binary search: compares target hash against midpoint, narrows range
- Returns entry index on match, -1 on miss

## Called By
- `HashLookup` (0x41E830) — primary caller

## Related
- [sub_41E830_HashLookup.md](sub_41E830_HashLookup.md) — caller
- [../DATA_STORE.md](../DATA_STORE.md) — data store overview
