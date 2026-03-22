# sub_5176E0 — PrintSectorLoadInfo

**Address:** `Spiderwick.exe+1176E0` (absolute: `005176E0`)
**Convention:** __cdecl (presumed)
**Returns:** void

## Signature
```c
void PrintSectorLoadInfo(void *sector_data)
```

## Description
Debug printing function that outputs sector loading statistics. Prints the sector name, chunk count, geometry instance count, and hanging edge count. This function revealed that **sector_data+0x00** contains the sector name as a null-terminated string.

## Debug Output Format
Prints information including:
- Sector name (from `sector_data+0x00`)
- Chunk count (from `sector_data+0x78`, via `*(v2+120)` -> `*v3`)
- Geometry instance count (from `sector_data+0xAC`, via `v2+172`)
- Hanging edge count (from `sector_data+0x58`, via `v2+88`)

## Key Discovery
This function proved that `sector_data+0x00` is the **sector name string** (e.g. "Foyer", "Kitchen", "Library"). This was previously unknown in the per-sector data layout.

## Sector Data Fields Referenced
| Offset | Type | Field |
|--------|------|-------|
| +0x00 | char[] | Sector name (null-terminated) |
| +0x58 | int | Hanging edge count |
| +0x78 | int* -> int | Chunk count (pointer dereference) |
| +0xAC | int | Geometry instance count |

## Related
- [../WORLD_LOADING.md](../WORLD_LOADING.md) — world loading overview
- [../../camera/PORTAL_SYSTEM.md](../../camera/PORTAL_SYSTEM.md) — per-sector data structure (updated with +0x00 name field)
