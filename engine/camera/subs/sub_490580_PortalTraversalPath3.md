# sub_490580 — PortalTraversalPath3
**Address:** `Spiderwick.exe+90580` (absolute: `00490580`)
**Size:** 365 bytes (0x16D)
**Status:** FULLY REVERSED

## Purpose
Portal traversal orchestrator for **Pipeline 3** (third render path). Unlike Pipelines 1
and 2 where the sector is passed as an argument, this function obtains the camera via
GetCameraObject() and reads the sector index directly from `camera+0x788`.

Uses `sub_55F6D0` instead of `sub_55DF50` for animation updates (alternate animation
path). After the traversal sequence, checks IsSectorSystemActive and conditionally
calls sub_564F40 (PostSectorRoomProcess) for additional room processing.

## Signature
```c
void __userpurge PortalTraversalPath3(int camera<esi>, int unknown)
// camera passed via ESI register
// unknown = stack argument
// userpurge calling convention (IDA: esi = camera, int on stack)
```

## Pseudocode
```c
void PortalTraversalPath3(int camera /*esi*/, int unknown) {
    // --- Get Camera & Sector ---
    CameraObject *cam = GetCameraObject();     // sub_4368B0 → [0x0072F670]
    int sector = cam->sectorIndex;             // cam+0x788

    // --- Viewport & Frustum ---
    SetupViewport(camera + 0x6C8, 1);         // sub_4E9670 → D3D viewport setup
    FrustumSetup_VM(camera, 1);                // sub_562650 → Kallis VM frustum planes

    // --- Sector Updates (alternate path) ---
    sub_55F6D0(dt);                            // alternate anim update (NOT sub_55DF50)
    // NOTE: No UpdateFadeEffect, no UpdateRoomTimer, no PreTraversal_VM

    // --- Portal Traversal ---
    SetSectorState_VM(sector);                 // sub_516900
    PortalTraversal_VM(sector, camera, 0);     // sub_51ABE0 → sub_51A130 (native)
    PostTraversal_VM(&off_7147C0);             // sub_5564D0

    // --- Finalize ---
    // ... lighting/color/render steps ...
    FinalizeRender_VM();                       // sub_519540

    // --- Post-render room processing ---
    if (IsSectorSystemActive()) {              // sub_516B10
        sub_564F40();                          // PostSectorRoomProcess
    }
}
```

## Key Call Sites (for portal patches)

These are the two critical call sites inside this function relevant for portal
frustum bypass approaches:

| Address | Target | Purpose | Bytes |
|---------|--------|---------|-------|
| `0x49059E` | `sub_562650` (FrustumSetup_VM) | Computes frustum planes for portal testing | `E8 AD 20 0D 00` |
| `0x4905DB` | `sub_51ABE0` (PortalTraversal_VM) | Entry to recursive portal walk | `E8 00 A6 08 00` |

The FrustumSetup_VM call at `0x49059E` computes the frustum planes. The
PortalTraversal_VM call at `0x4905DB` enters the recursive portal walk through the
Kallis VM thunk into native sub_51A130.

## Call Sequence (in order)

| # | Address | Function | Purpose |
|---|---------|----------|---------|
| 1 | — | `sub_4368B0` | GetCameraObject () → camera singleton |
| 2 | — | read `cam+0x788` | Get sector index from camera object |
| 3 | — | `sub_4E9670` | SetupViewport (camera+0x6C8, 1) |
| 4 | `0x49059E` | `sub_562650` | FrustumSetup_VM (camera, 1) |
| 5 | — | `sub_55F6D0` | Alternate anim update (dt) |
| 6 | — | `sub_516900` | SetSectorState_VM (sector) |
| 7 | `0x4905DB` | `sub_51ABE0` | PortalTraversal_VM (sector, camera, 0) |
| 8 | — | `sub_5564D0` | PostTraversal_VM (&off_7147C0) |
| 9 | — | `sub_519540` | FinalizeRender_VM |
| 10 | — | `sub_516B10` | IsSectorSystemActive check |
| 11 | — | `sub_564F40` | PostSectorRoomProcess (conditional) |

## Differences from Pipeline 1 and Pipeline 2

| Feature | Pipeline 1 (sub_488970) | Pipeline 2 (sub_48CBE0) | Pipeline 3 (sub_490580) |
|---------|------------------------|------------------------|------------------------|
| Calling convention | cdecl | stdcall | userpurge (esi=camera) |
| Size | 299 bytes | 144 bytes | 365 bytes |
| Gets camera via | argument | argument | GetCameraObject() |
| Gets sector via | argument | argument | reads camera+0x788 |
| Anim update | sub_55DF50 | sub_55DF50 | sub_55F6D0 (alternate) |
| UpdateFadeEffect | Yes | No | No |
| UpdateRoomTimer | Yes | Yes | No |
| PreTraversal_VM | Yes | Yes | No |
| PostSectorRoomProcess | No | No | Yes (conditional) |
| IsSectorSystemActive check | No | No | Yes (for sub_564F40) |

## Context in Pipeline 3

```
RenderPipeline3 (sub_490CE0)          vtable at 0x6301C0
├── dt = sub_4DE830()
├── Game subsystem updates
├── if (IsSectorSystemActive): PerformRoomCulling(dt, camera)
├── Sector transition handling
└── PortalTraversalPath3 (sub_490580, userpurge)  ← THIS FUNCTION
    ├── GetCameraObject() → read sector from cam+0x788
    ├── FrustumSetup_VM                            ← frustum planes computed here
    ├── sub_55F6D0 (alternate anim)
    ├── PortalTraversal_VM → sub_51A130            ← portal walk uses those planes
    └── if IsSectorSystemActive: sub_564F40        ← post-render room processing
```

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full portal system overview and all patch points
- [sub_488970_SectorVisibilityUpdate2.md](sub_488970_SectorVisibilityUpdate2.md) — Pipeline 1 equivalent
- [sub_48CBE0_PortalTraversalPath2.md](sub_48CBE0_PortalTraversalPath2.md) — Pipeline 2 equivalent
- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) — recursive portal walk (native)
- [sub_4368B0_GetCameraObject.md](sub_4368B0_GetCameraObject.md) — camera singleton accessor
- [sub_516B10_IsSectorSystemActive.md](sub_516B10_IsSectorSystemActive.md) — sector system gate check
- [sub_488DD0_CameraSectorUpdate.md](sub_488DD0_CameraSectorUpdate.md) — Pipeline 1 entry (for comparison)
