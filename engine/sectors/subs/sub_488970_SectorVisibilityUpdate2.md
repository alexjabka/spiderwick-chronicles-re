# sub_488970 — SectorVisibilityUpdate2
**Address:** `Spiderwick.exe+88970` (absolute: `00488970`)
**Size:** 299 bytes (0x12B)
**Status:** FULLY REVERSED

## Purpose
Portal traversal orchestrator for **Pipeline 1** (normal gameplay). This is where the
frustum is computed and portal traversal happens during the second visibility pass.
Called from CameraSectorUpdate (sub_488DD0) after the camera object updates have run
and the sector index has been re-read from `camera+0x788`.

Sets up the D3D viewport, computes frustum planes via the Kallis VM, runs sector
animation/fade/timer updates, then executes the full portal traversal pipeline.

## Signature
```c
void __cdecl SectorVisibilityUpdate2(int sector, int camera, float dt)
// sector = camera's current sector index (from camera+0x788)
// camera = CameraObject pointer
// dt     = frame delta time (float)
// Caller cleans stack (cdecl, 3 args = 0x0C bytes)
```

## Full Pipeline (decompiled sequence)
```c
void SectorVisibilityUpdate2(int sector, int camera, float dt) {
    // --- Viewport & Frustum ---
    SetupViewport(camera + 0x6C8, 1);         // sub_4E9670 → D3D viewport setup
    FrustumSetup_VM(camera, 1);                // sub_562650 → Kallis VM frustum planes

    // --- Sector Updates ---
    UpdateSectorAnims(camera, dt);             // sub_55DF50 → 14 sector anim entries
    UpdateFadeEffect(dt);                      // sub_5620D0 → room transition fade
    UpdateRoomTimer(dt);                       // sub_561DF0 → room animation timer

    // --- Portal Traversal ---
    PreTraversal_VM();                         // sub_561B60
    SetSectorState_VM(sector);                 // sub_516900
    PortalTraversal_VM(sector, camera, 0);     // sub_51ABE0 → sub_51A130 (native)
    PostTraversal_VM(&off_7147C0);             // sub_5564D0

    // --- Lighting & Ambient ---
    GetSectorAmbientRGB(&r, &g, &b);          // sub_5170A0
    SetAmbientColor(r, g, b);                 // sub_4E1600 → D3D ambient

    // --- Finalization ---
    CheckVisMode_VM();                         // visibility mode check
    NotifyObservers(...);                      // sub_432AD0
    FinalizeRender_VM();                       // sub_519540
}
```

## Complete Pipeline Summary
```
FrustumSetup_VM
  → PortalTraversal_VM
    → PostTraversal_VM
      → ambient
        → CheckVisMode_VM
          → FinalizeRender_VM
```

## Key Call Sites (for portal patches)

These are the two critical call sites inside this function that are relevant for
portal frustum bypass approaches:

| Address | Target | Purpose | Bytes |
|---------|--------|---------|-------|
| `0x488989` | `sub_562650` (FrustumSetup_VM) | Computes frustum planes for portal testing | `E8 C2 9C 0D 00` |
| `0x4889CC` | `sub_51ABE0` (PortalTraversal_VM) | Entry to recursive portal walk | `E8 0F 22 09 00` |

The FrustumSetup_VM call at `0x488989` is where frustum planes are computed that
later feed into PortalTraversal_Native's clip test. The PortalTraversal_VM call at
`0x4889CC` is the entry point into the recursive portal walk (via Kallis VM thunk
into native sub_51A130).

## Call Sequence (in order)

| # | Address | Function | Purpose |
|---|---------|----------|---------|
| 1 | — | `sub_4E9670` | SetupViewport (camera+0x6C8, 1) |
| 2 | `0x488989` | `sub_562650` | FrustumSetup_VM (camera, 1) |
| 3 | — | `sub_55DF50` | UpdateSectorAnims (camera, dt) |
| 4 | — | `sub_5620D0` | UpdateFadeEffect (dt) |
| 5 | — | `sub_561DF0` | UpdateRoomTimer (dt) |
| 6 | — | `sub_561B60` | PreTraversal_VM () |
| 7 | — | `sub_516900` | SetSectorState_VM (sector) |
| 8 | `0x4889CC` | `sub_51ABE0` | PortalTraversal_VM (sector, camera, 0) |
| 9 | — | `sub_5564D0` | PostTraversal_VM (&off_7147C0) |
| 10 | — | `sub_5170A0` | GetSectorAmbientRGB |
| 11 | — | `sub_4E1600` | SetAmbientColor |
| 12 | — | — | CheckVisMode_VM |
| 13 | — | `sub_432AD0` | NotifyObservers |
| 14 | — | `sub_519540` | FinalizeRender_VM |

## Context in Pipeline 1

```
CameraSectorUpdate (sub_488DD0)
├── Pass 1: UpdateVisibility (sub_488B30, stdcall)
│   └── PerformRoomCulling (sub_564950)        ← object-level culling
├── Camera object method updates
│   └── sub_438E50, sub_438EE0, sub_438EA0
└── Pass 2: SectorVisibilityUpdate2 (sub_488970, cdecl)  ← THIS FUNCTION
    ├── FrustumSetup_VM                        ← frustum planes computed here
    ├── PortalTraversal_VM → sub_51A130        ← portal walk uses those planes
    ├── PostTraversal_VM                       ← submit visible sectors to render
    ├── Ambient color                          ← set D3D ambient
    ├── CheckVisMode_VM                        ← visibility mode check
    └── FinalizeRender_VM                      ← finalize render state
```

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full portal system overview and all patch points
- [sub_488DD0_CameraSectorUpdate.md](sub_488DD0_CameraSectorUpdate.md) — Pipeline 1 entry (caller)
- [sub_488B30_UpdateVisibility.md](sub_488B30_UpdateVisibility.md) — Pipeline 1 Pass 1
- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) — recursive portal walk (native)
- [sub_48CBE0_PortalTraversalPath2.md](sub_48CBE0_PortalTraversalPath2.md) — Pipeline 2 equivalent
- [sub_490580_PortalTraversalPath3.md](sub_490580_PortalTraversalPath3.md) — Pipeline 3 equivalent
