# sub_4391D0 — SectorTransition
**Address:** Spiderwick.exe+391D0 (absolute: 0x4391D0)
**Status:** FULLY REVERSED

## Purpose
Detects when camera enters a new sector. Builds a ray from the previous camera
position to the current position and calls RaycastFindSector to determine which
sector the camera is in. Compares with current sector and updates if changed.

## CRITICAL: Matrix Offsets
The function decompiles **+0x790 and +0x7D0** matrices (NOT +0x834/+0x874 as
previously documented). These are VIEW matrices.

```
this+0x790 — current view matrix (destination position)
this+0x7D0 — previous view matrix (source position)
```

## Key Behavior
1. Reads camera position from **this+0x790** and **this+0x7D0** (view matrices)
2. Builds a ray from +0x7D0 position (previous) to +0x790 position (current)
3. Calls sub_519700 (RaycastFindSector) to determine which sector the ray enters
4. RaycastFindSector only checks immediate portal neighbors of the current sector
5. Compares new sector with current: [this+0x788]
6. If different: prints "Sector Changed: was %d now %d\n"
7. Writes new sector to [this+0x788]

## Limitation
Because RaycastFindSector (sub_519700) only iterates the current sector's portal
neighbors, this function **cannot handle position jumps**. If the camera teleports
more than one sector away, the raycast will fail to find the correct sector since
it only checks direct portal neighbors.

## Debug String
"Sector Changed: was %d now %d\n" at 0x439342 — confirms sector transition detection.

## Hardware Breakpoint Results
- Read at 004392E8: mov ecx,[esi+788h] — ~6500/frame (most frequent reader)
- Write at 00439356: mov [esi+788h],eax — 15 hits per room change

## Related
- [sub_519700](sub_519700_RaycastFindSector.md) (RaycastFindSector): portal-based sector detection, iterates portal neighbors only
- this+0x788 = camera sector index (same as camera_obj+0x788)
- this+0x790 = current view matrix
- this+0x7D0 = previous view matrix
