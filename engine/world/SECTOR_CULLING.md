# Sector Distance Culling
**Status:** Fully reversed (Session 5)
**Last updated:** 2026-03-18

---

## Overview

The Spiderwick engine uses two distance-based culling systems that operate independently:

1. **ClPlayerProximityService** -- service-level system that loads/unloads sectors based on player-to-sector distance (squared thresholds)
2. **PortalDistancePreTest** -- per-portal gate in the recursive portal traversal that skips distant portals

These are distinct from the bitmask-based sector loading system documented in [../sectors/SECTOR_LOADING.md](../sectors/SECTOR_LOADING.md). The proximity service operates at a higher level (deciding which sectors should exist in memory), while the portal distance test operates at the render level (deciding which portals to traverse this frame).

---

## ClPlayerProximityService

A service object (64 bytes) that iterates all sectors each frame, computes squared distance from the camera to each sector's entity position, and fires load/unload events based on threshold comparison.

### Architecture

```
CreatePlayerProximityService (0x487EE0)
  |
  +-- allocate 64 bytes                    via sub_551600
  |
  +-- ClPlayerProximityService_Init (0x487C70)
  |     |
  |     +-- store initial position         +0x14/+0x18/+0x1C
  |     +-- store load threshold squared   a6 * a6 -> +0x24
  |     +-- store unload threshold squared a7 * a7 -> +0x28
  |     +-- set vtable to 0x62F440
  |     +-- set enabled flag (+0x38 = 1)
  |
  +-- sub_5513D0(service, parent)          register with service system

SectorDistanceCheck (0x487D70)             called per-frame via vtable
  |
  +-- if not enabled (+0x38 == 0): return
  |
  +-- copy camera position from [+0x3C]    to +0x14/+0x18/+0x1C
  |   (reads [obj+0x68], [obj+0x6C], [obj+0x70] = entity X/Y/Z)
  |
  +-- for each sector (0 to count):
        |
        +-- get sector entity via sub_53A020 -> sub_539AE0 -> sub_450DA0
        |
        +-- compute distSq = (entityX - camX)^2 + (entityY - camY)^2 + (entityZ - camZ)^2
        |
        +-- if sector IS loaded (bit set in +0x34):
        |     if distSq > unload_threshold_sq (+0x28):
        |       clear bit, fire unload event via sub_52EB60
        |
        +-- if sector NOT loaded:
              if distSq < load_threshold_sq (+0x24):
                set bit, fire load event via sub_52EB60
```

### Object Layout (64 bytes)

| Offset | Type | Field | Set By |
|--------|------|-------|--------|
| +0x00 | dword | vtable (0x62F440) | Init |
| +0x04 | dword | flags (bit 17 set) | Init |
| +0x08 | dword | 0 | Init |
| +0x0C | dword | 0 | Init |
| +0x10 | dword | parent reference | Init (a10) |
| +0x14 | float | camera position X | Init (a2), updated per-frame |
| +0x18 | float | camera position Y | Init (a3), updated per-frame |
| +0x1C | float | camera position Z | Init (a4), updated per-frame |
| +0x24 | float | **load threshold squared** (a6*a6) | Init |
| +0x28 | float | **unload threshold squared** (a7*a7) | Init |
| +0x2C | dword | event hash for load | Init (a8) |
| +0x30 | dword | event hash for unload | Init (a9) |
| +0x34 | dword | **sector loaded bitmask** | Updated per-frame |
| +0x38 | byte | **enabled flag** (1=active) | Init |
| +0x3C | dword* | camera/reference object pointer | Init |

### Threshold Behavior

The thresholds use **squared distances** to avoid expensive square root operations:

- **Load threshold** (+0x24): When `distSq < loadThresholdSq`, the sector is loaded. Stored as `inputRadius * inputRadius`.
- **Unload threshold** (+0x28): When `distSq > unloadThresholdSq`, the sector is unloaded. Stored as `inputRadius * inputRadius`.

The unload threshold should be >= load threshold to avoid load/unload oscillation (hysteresis).

### Event Dispatch

When a sector crosses a threshold, `sub_52EB60` is called with:
- The parent object's event handler (from `[+0x10]+168`)
- The event hash (+0x2C for load, +0x30 for unload)
- The sector entity's data (5 DWORDs from entity+168, copied via `std::locale::facet::facet` constructor pattern)

This connects into the engine's event/callback system, allowing scripts to react to sectors entering/leaving proximity.

### Creator

`CreatePlayerProximityService` (0x487EE0) is called from `sub_4D6EE0`, passing 9 arguments including position, thresholds, and event hashes. It allocates 64 bytes, calls Init, and registers the service.

---

## Portal Distance Pre-Test

A per-portal distance gate inside `PortalTraversal_Native` (0x51A130) that checks if a portal is close enough to the camera to be worth traversing. This is a performance optimization that avoids expensive frustum clip tests for distant portals.

### Location in PortalTraversal_Native

```
For each portal in current sector:
  |
  +-- PortalDistancePreTest (0x517E50)       ECX = portal data
  |     Computes closest point on portal geometry to camera
  |     If distSq <= context+0x90 (threshold): sets context+0x88 = 1
  |
  +-- [0x51A3AD] cmp context+0x88, 0         check if pre-test passed
  +-- [0x51A3B5] jnz LABEL_65               if passed -> process portal directly
  |                                          (skip frustum clip test)
  |
  +-- Portal flag bit check                  secondary gate
  +-- ClipPortalPolygon                      full frustum clip test
  +-- Recursive call to PortalTraversal_Native
```

### How It Works

`PortalDistancePreTest` (0x517E50) iterates 3 times (likely 3 faces/edges of the portal geometry), computing the closest point on each face to the camera position (via `sub_51BC50`). For each face:

1. Compute closest point on face to camera
2. Compute distance vector: `closest_point - camera_position`
3. Compute squared distance: `dx*dx + dy*dy + dz*dz`
4. If `distSq <= threshold` (context+0x90): set result flag (context+0x88 = 1)

The threshold at context+0x90 is **dynamic** -- it comes from the portal's geometry data, not a global constant. Larger portals have larger thresholds.

### Freecam Bypass

The portal distance pre-test bypass at 0x51A3B5 (`jnz -> jmp`) forces all portals to pass regardless of distance. This is part of the four-layer visibility bypass used by the freecam mod. See [../camera/PORTAL_SYSTEM.md](../camera/PORTAL_SYSTEM.md) for details.

---

## Relationship Between Systems

```
Sector Loading Pipeline:
  Kallis VM script writes g_SectorLoadBitmask (0x01340080)
    |
    +-- ClPlayerProximityService may influence this via events
    |
    v
  SectorUpdateTick (0x57F140)
    |
    +-- LoadUnloadSectors (0x57ED60) -- stream sectors in/out

Sector Rendering Pipeline:
  PortalTraversal_Native (0x51A130)
    |
    +-- IsSectorLoaded (0x57E0D0) -- skip unloaded sectors
    |
    +-- PortalDistancePreTest (0x517E50) -- skip distant portals
    |
    +-- ClipPortalPolygon (0x5299E0) -- frustum visibility test
```

The proximity service affects **which sectors are in memory** (streaming), while the portal distance test affects **which portals are traversed** (rendering). Both use squared distance comparisons for performance.

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| 0x487D70 | SectorDistanceCheck | __thiscall(this, a2) | Per-frame distance check, load/unload sectors |
| 0x487C70 | ClPlayerProximityService_Init | __thiscall(this, x, y, z, a5, loadR, unloadR, evt1, evt2, parent) | Initialize proximity service object |
| 0x487EE0 | CreatePlayerProximityService | __cdecl(x, y, z, a4, loadR, unloadR, evt1, evt2, parent) | Allocate + init + register service |
| 0x517E50 | PortalDistancePreTest | __thiscall(portal, context) | Per-portal distance gate in traversal |
| 0x51BC50 | (unnamed) | -- | Closest point on triangle/face to point |

---

## Related Documentation

- [../sectors/SECTOR_LOADING.md](../sectors/SECTOR_LOADING.md) -- Bitmask-based sector streaming system
- [../camera/PORTAL_SYSTEM.md](../camera/PORTAL_SYSTEM.md) -- Portal traversal pipeline
- [../camera/subs/sub_487D70_SectorDistanceCheck.md](../camera/subs/sub_487D70_SectorDistanceCheck.md) -- Detailed analysis
- [../camera/subs/sub_517E50_PortalDistancePreTest.md](../camera/subs/sub_517E50_PortalDistancePreTest.md) -- Portal pre-test
- [../camera/subs/sub_51A130_PortalTraversal_Native.md](../camera/subs/sub_51A130_PortalTraversal_Native.md) -- Recursive traversal
