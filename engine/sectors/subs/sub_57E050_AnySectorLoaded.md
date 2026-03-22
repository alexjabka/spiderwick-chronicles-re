# sub_57E050 -- AnySectorLoaded
**Address:** 0x57E050 (Spiderwick.exe+17E050)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
char AnySectorLoaded(void);
```

## Purpose

Scans the global sector state array and returns 1 (true) if ANY sector has state == 3 (loaded). Returns 0 if no sectors are loaded or if the sector count is zero.

## Implementation

```c
char AnySectorLoaded() {
    if (g_SectorCount <= 0)
        return 0;
    for (int i = 0; i < g_SectorCount; i++) {
        if (g_SectorStateArray[3 * i] == 3)
            return 1;
    }
    return 0;
}
```

## Size

38 bytes. 6 basic blocks, cyclomatic complexity 3.

## Key Globals

| Address | Name | Purpose |
|---------|------|---------|
| 0x133FEF0 | g_SectorCount | Number of sectors in current world |
| 0x133FEF8 | g_SectorStateArray | 12 bytes per sector |

## Callers

| Address | Function | Context |
|---------|----------|---------|
| 0x5173F0 | (unnamed) | Sector system gate check |

## Notes

- This is a "readiness" check -- used to determine if the world has finished initial loading
- During world transitions, all sectors start at state 0; this function returns 0 until at least one finishes streaming

## Related

- [sub_57E0D0_IsSectorLoaded.md](sub_57E0D0_IsSectorLoaded.md) -- Per-sector check
- [../SECTOR_LOADING.md](../SECTOR_LOADING.md) -- Sector streaming system
