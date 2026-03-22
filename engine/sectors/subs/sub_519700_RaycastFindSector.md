# sub_519700 — RaycastFindSector
**Address:** Spiderwick.exe+119700 (absolute: 0x519700)
**Status:** FULLY REVERSED

## Purpose
Portal-based sector detection. Given a ray (from previous camera position to current),
determines which sector the camera has moved into by iterating the current sector's
portal neighbors. This is a **local** search — it only checks direct portal neighbors,
not the entire world.

## Key Behavior
1. Takes current sector and a ray (start position, end position)
2. Reads portal data from `g_WorldState+0x68` (portal base address)
3. Each portal entry is **328 bytes** (0x148)
4. Iterates only the current sector's portals
5. Tests ray against each portal polygon
6. If ray intersects a portal, returns the sector on the other side
7. If no portal intersected, returns the current sector unchanged

## Limitation: Cannot Handle Position Jumps
Because this function only iterates the **immediate portal neighbors** of the current
sector, it cannot detect sector changes when the camera teleports more than one
sector away. The raycast will fail to intersect any portal if the destination is not
a direct neighbor.

This is the root cause of sector desync when freecam moves through multiple rooms
at once — the engine assumes camera movement is continuous and only checks adjacent
sectors.

## Portal Data Layout
```
g_WorldState+0x68 = portal array base
328 bytes (0x148) per portal entry:
  +0x00: portal polygon vertices
  +0x??: sector A index
  +0x??: sector B index
  +0x??: portal plane normal
  +0x??: bounding box
```

## Called By
- [sub_4391D0](sub_4391D0_SectorTransition.md) (SectorTransition) — builds ray from
  view matrices at +0x790 and +0x7D0, passes to this function

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full portal system overview
- [sub_4391D0](sub_4391D0_SectorTransition.md) — caller: sector transition detection
- `g_WorldState+0x68` — portal array base address
