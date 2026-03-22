# sub_517E50 -- PortalDistancePreTest
**Address:** 0x517E50 (Spiderwick.exe+117E50)
**Status:** FULLY REVERSED (Session 5, updated from partial)

## Signature

```c
void __thiscall PortalDistancePreTest(float *this, int context);
// ECX = portal geometry data (float array with face vertices)
// context = traversal context structure
```

## Purpose

Per-portal distance gate in `PortalTraversal_Native`. Tests if the portal's closest point to the camera is within rendering range. If the portal passes, the caller skips the expensive frustum clip test and proceeds directly to portal processing.

## Algorithm

Iterates 3 times (3 faces/edges of the portal geometry):

```
for face in 0..2:
    v1 = portal_face_vertex_1
    v2 = portal_face_vertex_2
    v3 = portal_face_vertex_3

    closestPoint = sub_51BC50(v3, v2, v1, context, &result, ...)

    dx = result.x - context[0]   // camera X
    dy = result.y - context[1]   // camera Y
    dz = result.z - context[2]   // camera Z
    distSq = dx*dx + dy*dy + dz*dz

    if distSq <= context[36]:    // context+0x90 = threshold squared
        context[34] = 1          // context+0x88 = result flag (passed)
```

The portal face data is read from `this` at stride 19 floats (76 bytes) per face:
- Face 0: this[24..34] (offsets -10,-9,-8 / -6,-5,-4 / -2,-1,0 relative to v3)
- Face 1: this[43..53]
- Face 2: this[62..72]

## Context Structure

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | float | Camera position X |
| +0x04 | float | Camera position Y |
| +0x08 | float | Camera position Z |
| +0x88 | int | **Result flag** (0=failed, 1=passed) |
| +0x90 | float | **Distance threshold squared** (from portal geometry) |

## Call Site in PortalTraversal_Native

```asm
51A3A8  call  PortalDistancePreTest     ; ECX = portal data
51A3AD  cmp   [esp+18h+arg_120], 0      ; check context+0x88
51A3B5  jnz   loc_51A8CC               ; passed -> skip clip test -> LABEL_65
```

The `jnz -> jmp` patch at 0x51A3B5 is the freecam portal bypass (forces all portals to pass).

## Size

211 bytes. 5 basic blocks, cyclomatic complexity 3.

## Callers

| Address | Function |
|---------|----------|
| 0x51A3A8 | PortalTraversal_Native (0x51A130) |

## Callees

- `sub_51BC50` -- closest point on triangle/face to a point

## Dynamic Threshold

The threshold at context+0x90 is **per-portal**, NOT a global constant. It comes from the portal's geometry data, so larger portals (e.g., wide doorways) have larger distance thresholds than small portals (e.g., windows). This means the engine naturally renders more detail through large openings.

## Gate Sequence in PortalTraversal_Native

```
1. PortalDistancePreTest  -- portal within dynamic range?
   YES -> skip to LABEL_65 (process portal)
   NO  -> continue to step 2

2. Portal flag bit check   -- [esi+4] & 2 == 0?
   YES -> skip to LABEL_65
   NO  -> continue to step 3

3. ClipPortalPolygon       -- frustum clip (Sutherland-Hodgman)
   vertices > 0 -> process portal
   vertices = 0 -> skip portal
```

## Related

- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) -- Caller
- [sub_5299E0_ClipPortalPolygon.md](sub_5299E0_ClipPortalPolygon.md) -- Frustum clip (step 3)
- [../../world/SECTOR_CULLING.md](../../world/SECTOR_CULLING.md) -- Distance culling overview
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) -- Full portal system
