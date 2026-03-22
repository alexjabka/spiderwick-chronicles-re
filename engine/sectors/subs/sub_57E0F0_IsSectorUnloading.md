# sub_57E0F0 -- IsSectorUnloading
**Address:** 0x57E0F0 (Spiderwick.exe+17E0F0)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
bool __cdecl IsSectorUnloading(int sectorIndex);
```

## Purpose

Returns true if the specified sector is in state 4 (textures/draw data being freed). Used by the debug sector display.

## Implementation

```c
bool IsSectorUnloading(int sectorIndex) {
    return g_SectorStateArray[3 * sectorIndex] == 4;
}
```

## Size

23 bytes. Single basic block, cyclomatic complexity 1.

## Callers

| Address | Function | Context |
|---------|----------|---------|
| 0x59993E | DebugDrawSectorInfo | Debug overlay display |

## Related

- [sub_57E0D0_IsSectorLoaded.md](sub_57E0D0_IsSectorLoaded.md) -- State == 3
- [sub_57E0B0_IsSectorThrottling.md](sub_57E0B0_IsSectorThrottling.md) -- State == 2
- [../SECTOR_LOADING.md](../SECTOR_LOADING.md) -- Sector streaming system
