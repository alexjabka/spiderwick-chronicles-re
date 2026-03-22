# sub_487EE0 -- CreatePlayerProximityService
**Address:** 0x487EE0 (Spiderwick.exe+87EE0)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
_WORD *__cdecl CreatePlayerProximityService(
    float x,        // initial position X
    float y,        // initial position Y
    float z,        // initial position Z
    int a4,         // (passed through)
    float loadR,    // load radius
    float unloadR,  // unload radius
    int evtLoad,    // event hash for load
    int evtUnload,  // event hash for unload
    int parent      // parent object reference
);
```

## Purpose

Factory function that allocates a 64-byte `ClPlayerProximityService` object, initializes it, and registers it with the service system.

## Behavior

1. Allocate 64 bytes via `sub_551600(64)`
2. If allocation succeeded:
   a. Call `ClPlayerProximityService_Init(obj, x, y, z, ..., loadR, unloadR, evtLoad, evtUnload, parent)`
   b. Call `sub_5513D0(obj, parent)` -- register with service system
3. Return the service object pointer (or NULL if allocation failed)

## Size

99 bytes. 3 basic blocks, cyclomatic complexity 1 (null check only).

## Callers

| Address | Function | Context |
|---------|----------|---------|
| 0x4D6F5A | sub_4D6EE0 | Level/scene initialization |

## Related

- [sub_487C70_ClPlayerProximityService_Init.md](sub_487C70_ClPlayerProximityService_Init.md) -- Initializer
- [sub_487D70_SectorDistanceCheck.md](sub_487D70_SectorDistanceCheck.md) -- Per-frame vtable callback
- [../../world/SECTOR_CULLING.md](../../world/SECTOR_CULLING.md) -- System overview
