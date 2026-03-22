# sub_57E0D0 -- IsSectorLoaded
**Address:** 0x57E0D0 (Spiderwick.exe+17E0D0)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
bool __cdecl IsSectorLoaded(int sectorIndex);
```

## Purpose

Returns true if the specified sector is fully loaded (state == 3) by checking the global sector state array.

## Implementation

```c
bool IsSectorLoaded(int sectorIndex) {
    return g_SectorStateArray[3 * sectorIndex] == 3;
}
```

Each sector state entry is 12 bytes (3 DWORDs). The first DWORD is the state:
- 0 = unloaded
- 2 = throttling (IO in flight)
- 3 = **loaded** (renderable)
- 4 = unloading

## Size

23 bytes. Single basic block, cyclomatic complexity 1.

## Key Globals

| Address | Name | Purpose |
|---------|------|---------|
| 0x133FEF8 | g_SectorStateArray | 12 bytes per sector: [state, ?, tracking] |

## Callers

| Address | Function | Context |
|---------|----------|---------|
| 0x51A1B9 | PortalTraversal_Native | Skip unloaded sectors during portal walk |
| 0x51A7F9 | PortalTraversal_Native | Second check during recursion |
| 0x51A8E7 | PortalTraversal_Native | Third check during recursion |
| 0x517410 | TraceAllLoadedSectors | Debug/diagnostic |
| 0x517866 | (unnamed) | Unknown |
| 0x519AF9 | sub_519AC0 | Unknown |
| 0x519B79 | sub_519B40 | Unknown |
| 0x1E95020 | (.kallis) | VM code |

## Notes

- This is the most frequently called sector state function
- Portal traversal calls this 3 times per recursion level to skip portals leading to unloaded sectors
- Forcing this to always return true would render portals to unloaded sectors, causing crashes (no geometry data)

## Related

- [sub_57E0B0_IsSectorThrottling.md](sub_57E0B0_IsSectorThrottling.md) -- State == 2
- [sub_57E0F0_IsSectorUnloading.md](sub_57E0F0_IsSectorUnloading.md) -- State == 4
- [sub_57E050_AnySectorLoaded.md](sub_57E050_AnySectorLoaded.md) -- Scans all sectors
- [../SECTOR_LOADING.md](../SECTOR_LOADING.md) -- Sector streaming system
