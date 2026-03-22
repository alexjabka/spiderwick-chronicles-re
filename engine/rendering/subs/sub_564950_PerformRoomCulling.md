# sub_564950 — PerformRoomCulling
**Address:** `Spiderwick.exe+164950` (absolute: `00564950`)

## Purpose
The actual room culling function. Decides which rooms to render based on
camera sector. Called conditionally by UpdateVisibility (sub_488B30) when
the sector system is active.

## Signature
```c
void __cdecl PerformRoomCulling(float deltaTime, CameraObject *camera_obj)
// deltaTime  = frame delta (float, on stack)
// camera_obj = SectorCameraObject pointer
// Reads camera_obj+0x788 internally to get sector index
```

## What We Know
- Called from `sub_488B30` at `+88B69` every frame (when indoors)
- Reads `camera_obj+0x788` (camera sector) internally — the sector
  is NOT passed as a separate argument
- When this call is skipped (NOP'd), room culling should stop
- **Not yet decompiled** — internals unknown

## Patch
NOP the call site in sub_488B30:
```
Spiderwick.exe+88B69:  E8 xx xx xx xx  →  90 90 90 90 90
```

## Relationship to Portal System
This is **Layer 2** — object-level distance-based culling. It is **NOT** the portal
frustum system (Layer 3). The portal system is handled by:
- `sub_51A130` (PortalTraversal_Native) — recursive portal walk
- `sub_5299E0` (ClipPortalPolygon) — Sutherland-Hodgman frustum clip
- Entry via `sub_51ABE0` (PortalTraversal_VM) → Kallis VM → sub_51A130

PerformRoomCulling manages the F800xx global arrays (room positions, timers,
visibility counts). Portal traversal manages sector-level rendering via a
separate mechanism.

Both layers must be disabled for full room visibility. The portal_frustum_bypass.cea
script NOPs this function's call site AND patches the portal clip test.

See [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) for the complete system.

## Globals Used (F800xx Region)
| Address | Type | Purpose |
|---------|------|---------|
| `F800A0` | float | Time accumulator |
| `F800AC` | int | Visible room count |
| `F800C0/C4` | ptr pair | Room array 1 (start/end) |
| `F800C8/CC` | int pair | Counters |
| `F800E0/E4` | ptr pair | Room array 2 (start/end) |
| `F800F8/FC` | ptr pair | Room array 3 (start/end) |
| `F80118` | int | Mode (0 or 1) |

## Status: DECOMPILED (see PORTAL_SYSTEM.md for full pseudocode)
