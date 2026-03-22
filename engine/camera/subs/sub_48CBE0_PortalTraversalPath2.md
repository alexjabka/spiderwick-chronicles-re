# sub_48CBE0 — PortalTraversalPath2
**Address:** `Spiderwick.exe+8CBE0` (absolute: `0048CBE0`)
**Size:** 144 bytes (0x90)
**Status:** FULLY REVERSED

## Purpose
Portal traversal orchestrator for **Pipeline 2** (alternate render path). Equivalent to
SectorVisibilityUpdate2 (sub_488970) but simpler — omits UpdateFadeEffect and
UpdateRoomTimer. Called from RenderPipeline2 (sub_48CC70) as the portal traversal pass.

Sets up the viewport, computes frustum planes, updates sector animations, then runs the
PreTraversal -> SetSectorState -> PortalTraversal -> PostTraversal sequence. Finishes
with render finalization.

## Signature
```c
void __stdcall PortalTraversalPath2(int sector, int camera, float dt)
// sector = camera's current sector index
// camera = CameraObject pointer
// dt     = frame delta time (float)
// Callee cleans stack (stdcall, retn 0Ch)
```

## Pseudocode
```c
void PortalTraversalPath2(int sector, int camera, float dt) {
    // --- Viewport & Frustum ---
    SetupViewport(camera + 0x6C8, 1);         // sub_4E9670 → D3D viewport setup
    FrustumSetup_VM(camera, 1);                // sub_562650 → Kallis VM frustum planes

    // --- Sector Updates ---
    UpdateSectorAnims(camera, dt);             // sub_55DF50 → 14 sector anim entries
    // NOTE: No UpdateFadeEffect or UpdateRoomTimer (unlike Pipeline 1)

    // --- Portal Traversal ---
    UpdateRoomTimer(dt);                       // sub_561DF0
    PreTraversal_VM();                         // sub_561B60
    SetSectorState_VM(sector);                 // sub_516900
    PortalTraversal_VM(sector, camera, 0);     // sub_51ABE0 → sub_51A130 (native)
    PostTraversal_VM(&off_7147C0);             // sub_5564D0

    // --- Finalize ---
    FinalizeRender_VM();                       // sub_519540
}
```

## Key Call Sites (for portal patches)

These are the two critical call sites inside this function relevant for portal
frustum bypass approaches:

| Address | Target | Purpose | Bytes |
|---------|--------|---------|-------|
| `0x48CBF6` | `sub_562650` (FrustumSetup_VM) | Computes frustum planes for portal testing | `E8 55 5A 0D 00` |
| `0x48CC2D` | `sub_51ABE0` (PortalTraversal_VM) | Entry to recursive portal walk | `E8 AE DF 08 00` |

The FrustumSetup_VM call at `0x48CBF6` computes the frustum planes. The
PortalTraversal_VM call at `0x48CC2D` enters the recursive portal walk through the
Kallis VM thunk into native sub_51A130.

## Call Sequence (in order)

| # | Address | Function | Purpose |
|---|---------|----------|---------|
| 1 | — | `sub_4E9670` | SetupViewport (camera+0x6C8, 1) |
| 2 | `0x48CBF6` | `sub_562650` | FrustumSetup_VM (camera, 1) |
| 3 | — | `sub_55DF50` | UpdateSectorAnims (camera, dt) |
| 4 | — | `sub_561DF0` | UpdateRoomTimer (dt) |
| 5 | — | `sub_561B60` | PreTraversal_VM () |
| 6 | — | `sub_516900` | SetSectorState_VM (sector) |
| 7 | `0x48CC2D` | `sub_51ABE0` | PortalTraversal_VM (sector, camera, 0) |
| 8 | — | `sub_5564D0` | PostTraversal_VM (&off_7147C0) |
| 9 | — | `sub_519540` | FinalizeRender_VM |

## Differences from Pipeline 1 (SectorVisibilityUpdate2)

| Feature | Pipeline 1 (sub_488970) | Pipeline 2 (sub_48CBE0) |
|---------|------------------------|------------------------|
| Calling convention | cdecl | stdcall |
| Size | 299 bytes | 144 bytes |
| UpdateFadeEffect | Yes (sub_5620D0) | No |
| Ambient lighting | Yes (GetSectorAmbientRGB + SetAmbientColor) | No |
| NotifyObservers | Yes (sub_432AD0) | No |
| Core portal sequence | Same | Same |

## Context in Pipeline 2

```
RenderPipeline2 (sub_48CC70)
├── dt = sub_4DE830()
├── Game subsystem updates
├── if (IsSectorSystemActive): PerformRoomCulling(dt, camera)
├── GetSectorAmbientRGB + SetAmbientColor
├── FinalizeVisibility_VM()
├── Camera updates
└── PortalTraversalPath2 (sub_48CBE0, stdcall)  ← THIS FUNCTION
    ├── FrustumSetup_VM                          ← frustum planes computed here
    └── PortalTraversal_VM → sub_51A130          ← portal walk uses those planes
```

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full portal system overview and all patch points
- [sub_488970_SectorVisibilityUpdate2.md](sub_488970_SectorVisibilityUpdate2.md) — Pipeline 1 equivalent
- [sub_490580_PortalTraversalPath3.md](sub_490580_PortalTraversalPath3.md) — Pipeline 3 equivalent
- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) — recursive portal walk (native)
- [sub_488DD0_CameraSectorUpdate.md](sub_488DD0_CameraSectorUpdate.md) — Pipeline 1 entry (for comparison)
