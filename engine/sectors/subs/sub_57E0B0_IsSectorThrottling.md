# sub_57E0B0 -- IsSectorThrottling
**Address:** 0x57E0B0 (Spiderwick.exe+17E0B0)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
bool __cdecl IsSectorThrottling(int sectorIndex);
```

## Purpose

Returns true if the specified sector is in state 2 (IO submitted, waiting for completion). Used by the debug sector display to show loading progress.

## Implementation

```c
bool IsSectorThrottling(int sectorIndex) {
    return g_SectorStateArray[3 * sectorIndex] == 2;
}
```

## Size

23 bytes. Single basic block, cyclomatic complexity 1.

## Callers

| Address | Function | Context |
|---------|----------|---------|
| 0x5998F9 | DebugDrawSectorInfo | Debug overlay display |

## Related

- [sub_57E0D0_IsSectorLoaded.md](sub_57E0D0_IsSectorLoaded.md) -- State == 3
- [sub_57E0F0_IsSectorUnloading.md](sub_57E0F0_IsSectorUnloading.md) -- State == 4
- [../SECTOR_LOADING.md](../SECTOR_LOADING.md) -- Sector streaming system
