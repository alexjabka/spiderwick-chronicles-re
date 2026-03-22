# sub_487D70 -- SectorDistanceCheck
**Address:** 0x487D70 (Spiderwick.exe+87D70)
**Status:** FULLY REVERSED (Session 5, updated from partial)

## Signature

```c
void __thiscall SectorDistanceCheck(int this, int a2);
// ECX = ClPlayerProximityService object (64 bytes)
// a2 = unused parameter
```

## Purpose

Per-frame distance-based sector loading/unloading. Part of the `ClPlayerProximityService` vtable (0x62F440). Iterates all sectors, computes squared 3D distance from the camera to each sector entity, and fires load/unload events based on threshold comparison.

## Algorithm

```
1. If not enabled (+0x38 == 0): return
2. Copy camera position from reference object:
     [+0x3C]+0x68 -> +0x14 (X)
     [+0x3C]+0x6C -> +0x18 (Y)
     [+0x3C]+0x70 -> +0x1C (Z)
3. For each sector (index 0 to sub_53A010()):
   a. Get sector entity:
        sub_53A020(index+1) -> sub_539AE0 -> sub_450DA0 -> entity ptr
   b. Compute distSq:
        dx = entity[+0x68] - this[+0x14]
        dy = entity[+0x6C] - this[+0x18]
        dz = entity[+0x70] - this[+0x1C]
        distSq = dx*dx + dy*dy + dz*dz
   c. If sector bit IS set in loaded bitmask (+0x34):
        if distSq > unload_threshold_sq (+0x28):
          clear bit in +0x34
          fire unload event via sub_52EB60
   d. If sector bit NOT set:
        if distSq < load_threshold_sq (+0x24):
          set bit in +0x34
          fire load event via sub_52EB60
```

## ClPlayerProximityService Layout (64 bytes)

| Offset | Type | Field | Notes |
|--------|------|-------|-------|
| +0x00 | dword | vtable (0x62F440) | |
| +0x04 | dword | flags (bit 17 set) | `& 0xFFFCFFFF | 0x20000` |
| +0x08 | dword | 0 | |
| +0x0C | dword | 0 | |
| +0x10 | dword | parent reference | From Init(a10) |
| +0x14 | float | **camera position X** | Updated per-frame from [+0x3C] |
| +0x18 | float | **camera position Y** | |
| +0x1C | float | **camera position Z** | |
| +0x24 | float | **load threshold squared** | `loadRadius * loadRadius` |
| +0x28 | float | **unload threshold squared** | `unloadRadius * unloadRadius` |
| +0x2C | dword | load event hash | |
| +0x30 | dword | unload event hash | |
| +0x34 | dword | **sector loaded bitmask** | Bit N = sector N loaded |
| +0x38 | byte | **enabled flag** | 1 = active |
| +0x3C | dword* | reference object pointer | Object providing camera position |

## Event Dispatch

When a sector crosses a threshold, `sub_52EB60` is called to dispatch an event:
- Load event: `sub_52EB60([parent+168], loadHash, 1, entity_data[5])`
- Unload event: `sub_52EB60([parent+168], unloadHash, 1, entity_data[5])`

The entity data (5 DWORDs from entity+168) is copied via a constructor pattern.

## Size

355 bytes. 15 basic blocks, cyclomatic complexity 10.

## Callers

Called via vtable at 0x62F440 (ClPlayerProximityService virtual function table).

## Callees

- `sub_53A010` -- get sector count
- `sub_53A020` -- get sector by index
- `sub_539AE0` -- resolve entity reference
- `sub_450DA0` -- get entity data
- `sub_52EB60` -- dispatch event
- `std::locale::facet::facet` -- copy entity data (5 DWORDs)

## Freecam Relevance

This system can cause sectors to unload when the freecam moves far from the player. The bitmask at +0x34 may influence `g_SectorLoadBitmask` (0x01340080) through the event dispatch chain. Force-loading all sectors (patch at 0x57F16B) overrides this system's decisions.

## Related

- [sub_487C70_ClPlayerProximityService_Init.md](sub_487C70_ClPlayerProximityService_Init.md) -- Initializer
- [sub_487EE0_CreatePlayerProximityService.md](sub_487EE0_CreatePlayerProximityService.md) -- Factory
- [../../world/SECTOR_CULLING.md](../../world/SECTOR_CULLING.md) -- System overview
- [../../sectors/SECTOR_LOADING.md](../../sectors/SECTOR_LOADING.md) -- Bitmask-based loading
