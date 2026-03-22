# sub_487C70 -- ClPlayerProximityService_Init
**Address:** 0x487C70 (Spiderwick.exe+87C70)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
_WORD *__thiscall ClPlayerProximityService_Init(
    _WORD *this,    // 64-byte service object
    float x,        // initial position X
    float y,        // initial position Y
    float z,        // initial position Z
    int a5,         // (unused/passed through)
    float loadR,    // load radius (squared internally)
    float unloadR,  // unload radius (squared internally)
    int evtLoad,    // event hash for load callback
    int evtUnload,  // event hash for unload callback
    int parent      // parent object reference
);
```

## Purpose

Initializes a `ClPlayerProximityService` object. Sets the vtable, stores position, computes squared distance thresholds, and stores event hashes.

## Behavior

1. Clear flags and status fields
2. Set vtable to `ClPlayerProximityService::vftable` (0x62F440)
3. Store initial position: `+0x14=x, +0x18=y, +0x1C=z`
4. Store parent ref: `+0x10=parent`
5. **Square the thresholds:**
   - `+0x24 = loadR * loadR` (load threshold squared)
   - `+0x28 = unloadR * unloadR` (unload threshold squared)
6. Store event hashes: `+0x2C=evtLoad, +0x30=evtUnload`
7. Clear loaded bitmask: `+0x34=0`
8. Set enabled flag: `+0x38=1`
9. Set internal type: `*(WORD*)(this+2) = 29`

## Key Detail

The input radii are stored as their **squares** to avoid per-frame square root operations in `SectorDistanceCheck`. The distance comparison is always `distSq vs thresholdSq`.

## Size

117 bytes. Single basic block, cyclomatic complexity 1.

## Callers

| Address | Function |
|---------|----------|
| 0x487F2D | CreatePlayerProximityService (0x487EE0) |

## Related

- [sub_487D70_SectorDistanceCheck.md](sub_487D70_SectorDistanceCheck.md) -- Per-frame check using this object
- [sub_487EE0_CreatePlayerProximityService.md](sub_487EE0_CreatePlayerProximityService.md) -- Creator
- [../../world/SECTOR_CULLING.md](../../world/SECTOR_CULLING.md) -- System overview
