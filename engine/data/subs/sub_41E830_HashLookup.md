# sub_41E830 — HashLookup

**Address:** `Spiderwick.exe+1E830` (absolute: `0041E830`)
**Convention:** __cdecl
**Returns:** int (index, or -1 if not found)

## Signature
```c
int __cdecl HashLookup(const char *name)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| name | const char* | Variable name string (e.g., "/Story/Chapter") |

## Description
Hashes a variable name string and performs a binary search in the `dword_E57F64` entry array to find the corresponding data store index. Returns the index if found, or -1 if the variable does not exist.

## Key Details
- Computes a hash of the input string
- Binary search via `BinarySearchByHash` (0x539630) on the sorted array at `dword_E57F64`
- Entry array has `dword_E57F54` entries, each 16 bytes (hash at +0)
- Returns -1 on miss (no matching hash)
- DO NOT call from EndScene during world transitions (data store may be invalid)

## Called By
- SpiderMod ASI (for chapter/state queries — only safe during stable gameplay)
- Other game systems that need to read game state variables

## Calls
- `BinarySearchByHash` (0x539630)

## Related
- [sub_5392C0_GetValuePtr.md](sub_5392C0_GetValuePtr.md) — uses the returned index
- [sub_539630_BinarySearchByHash.md](sub_539630_BinarySearchByHash.md) — internal binary search
- [../DATA_STORE.md](../DATA_STORE.md) — data store overview
