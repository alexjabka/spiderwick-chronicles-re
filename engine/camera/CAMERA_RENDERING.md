# Camera Rendering Pipeline
**Status:** Fully reversed (Session 5)
**Last updated:** 2026-03-18

---

## Overview

The camera rendering pipeline converts logical camera state (FOV, aspect ratio, clip planes) into GPU-ready data: frustum corners for portal traversal and a projection matrix for Direct3D rendering. The pipeline is split into two distinct stages:

1. **Frustum corner generation** -- 8 corners of the view frustum, used by portal traversal to determine sector visibility
2. **Projection matrix build** -- 4x4 perspective projection matrix, passed to `IDirect3DDevice9::SetTransform`

Both stages are triggered by a single function (`CameraRender__SetFovAndClip`) which is called per-frame from the camera update loop.

---

## Per-Frame Flow

```
CameraSystem__UpdateRender (0x436190)           per-frame camera update
  |
  +-- CameraSettings__InitDefaults (0x4353C0)   init defaults: FOV=45, near=2.0, far=1024.0
  |
  +-- [component loop]                          run camera components (orbit, monocle, etc.)
  |
  +-- BuildLookAtMatrix                         build view matrix from eye/target
  |
  +-- FOV deg->rad conversion                   multiply by g_Deg2Rad (0x7130D0)
  |
  +-- CameraRender__SetFovAndClip (0x5293D0)    <-- main entry
        |
        +-- write FOV/aspect/clip to camObj     fields at +0x00, +0x04, +0x0C, +0x10, +0x14
        |
        +-- Frustum__BuildCorners (0x5AABD0)    8 frustum corners -> camObj+0x20
        |
        +-- Frustum__BuildProjectionMatrix (0x5A8B20)  proj matrix -> camObj+0x5C8
        |                                              hardcoded far=10000.0 (0x63EB64)
        |
        +-- IDirect3DDevice9::SetTransform(3)   set projection matrix on device
```

---

## Camera Render Object Layout

The camera render object (`this` for `CameraRender__SetFovAndClip`) stores clip planes, FOV, frustum geometry, and the final projection matrix.

| Offset | Type | Default | Field | Written By |
|--------|------|---------|-------|-----------|
| +0x00 | float | 2.0 | **Near clip distance** | SetFovAndClip, SetNearClip |
| +0x04 | float | 1024.0 | **Far clip distance** | SetFovAndClip, SetFarClip |
| +0x0C | float | pi/4 | **FOV in radians** | SetFovAndClip, ResetToDefaults |
| +0x10 | float | 1.333 | **Aspect ratio H** | SetFovAndClip, ResetToDefaults |
| +0x14 | float | 1.0 | **Aspect ratio V** | SetFovAndClip, ResetToDefaults |
| +0x20 | float[32] | -- | **Frustum corners** (8 corners x 4 floats) | Frustum__BuildCorners |
| +0x408 | float[16] | -- | **View matrix** (4x4) | RebuildMatrices |
| +0x448 | float[16] | -- | **ViewProjection matrix** (View*Scale*Proj) | RebuildMatrices |
| +0x5C8 | float[16] | -- | **Projection matrix** (4x4) | Frustum__BuildProjectionMatrix |
| +0x648 | float[16] | -- | **View * GlobalScale matrix** | RebuildMatrices |
| +0x6B8 | float | -- | **Camera position X** | sub_529160 |
| +0x6BC | float | -- | **Camera position Y** | sub_529160 |
| +0x6C0 | float | -- | **Camera position Z** | sub_529160 |
| +0x6C8 | __int16 | -- | **Viewport width** (pixels) | SetupViewport |
| +0x6CA | __int16 | -- | **Viewport height** (pixels) | SetupViewport |

See [WORLD_TO_SCREEN.md](WORLD_TO_SCREEN.md) for full extended layout.

### Frustum Corner Layout (+0x20)

Built by `Frustum__BuildCorners` (0x5AABD0). 8 corners in groups of 4 (near plane, then far plane), each corner is 4 floats (X, Y, Z, pad):

```
Near plane (4 corners):
  +0x20: near top-left      (-halfW_near, nearClip, +halfH_near)
  +0x30: near top-right     (+halfW_near, nearClip, +halfH_near)
  +0x40: near bottom-right  (+halfW_near, nearClip, -halfH_near)
  +0x50: near bottom-left   (-halfW_near, nearClip, -halfH_near)

Far plane (4 corners):
  +0x60: far top-left       (-halfW_far, farClip, +halfH_far)
  +0x70: far top-right      (+halfW_far, farClip, +halfH_far)
  +0x80: far bottom-right   (+halfW_far, farClip, -halfH_far)
  +0x90: far bottom-left    (-halfW_far, farClip, -halfH_far)

Center point:
  +0xA0: frustum center     (0, midpoint, 0)

Saved parameters (at end of corner array):
  +0x1B8: FOV (radians)
  +0x1BC: aspect H
  +0x1C0: aspect V
```

Where `halfW = tan(fov/2) * clipDist * aspectH` and `halfH = tan(fov/2) * clipDist * aspectV`.

### Projection Matrix Layout (+0x5C8)

Standard perspective projection matrix built by `Frustum__BuildProjectionMatrix` (0x5A8B20), then multiplied by a coordinate-swap matrix via `D3DXMatrixMultiply`. The far plane used is **hardcoded to 10000.0** (constant at 0x63EB64 in .rdata), NOT the logical far clip stored at +0x04.

```
proj[0]  = cot(fov/2) / aspectH       (X scale)
proj[5]  = cot(fov/2) * aspectV       (Y scale, note: multiplied, not divided)
proj[10] = far / (far - near)         (Z depth mapping)
proj[11] = 1.0                        (W=Z perspective divide)
proj[14] = -(far * near) / (far - near)  (Z offset)
proj[15] = 0.0
```

**Critical:** The projection matrix always uses far=10000.0 regardless of the logical far clip distance set by game scripts. This means:
- Geometry clipping (what disappears at distance) is at 10000.0 units
- Portal traversal uses the LOGICAL far clip (+0x04, default 1024.0) for frustum corners
- There is an intentional mismatch: portals cull at 1024 units, but geometry renders to 10000 units

---

## Hardcoded Projection Far Plane

**Address:** 0x63EB64 (.rdata section)
**Value:** 10000.0 (hex: 0x461C4000)
**Used by:** `Frustum__BuildProjectionMatrix` at 0x5A8B20

This constant is loaded as an immediate float operand. The projection matrix far plane is NOT configurable at runtime through the normal camera settings -- it is baked into the .rdata section. To modify it, you must either:
- Patch the bytes at 0x63EB64 (changes all cameras)
- Hook `Frustum__BuildProjectionMatrix` to replace the argument

---

## Setter Functions

### CameraRender__SetNearClip (0x52A090)

Sets near clip only, then rebuilds everything by calling `SetFovAndClip` with the existing FOV/aspect/far values from the object.

```c
void __thiscall CameraRender__SetNearClip(float *this, float nearClip) {
    *this = nearClip;                       // +0x00
    CameraRender__SetFovAndClip(this, this[3], this[4], this[5], nearClip, this[1]);
    CameraRender__RebuildMatrices(this);     // sub_529790
    // ... additional matrix operations
}
```

**Callers:** `sub_519F40` (called from PortalTraversal_Native)

### CameraRender__SetFarClip (0x52A0F0)

Sets far clip only, then rebuilds. Same pattern as SetNearClip.

```c
void __thiscall CameraRender__SetFarClip(float *this, float farClip) {
    this[1] = farClip;                      // +0x04
    CameraRender__SetFovAndClip(this, this[3], this[4], this[5], *this, farClip);
    CameraRender__RebuildMatrices(this);
}
```

**Callers:** None found (possibly dead code or called only from VM)

### CameraRender__ResetToDefaults (0x52A1A0)

Resets FOV to 45 degrees and aspect ratio to current global values. Called by PortalTraversal_Native during recursive traversal (restores camera state after processing a portal).

```c
void __thiscall CameraRender__ResetToDefaults(int this) {
    sub_5AB260(this + 32);                  // clear frustum corners
    *(WORD*)(this + 1742) = 0;              // clear flags
    sub_4E93E0(...);                        // viewport reset
    CameraRender__RebuildMatrices(this);
    *(float*)(this + 12) = g_Deg2Rad * 45.0;  // FOV = 45 degrees
    *(float*)(this + 16) = g_AspectRatioH;     // from 0xD6F314
    *(float*)(this + 20) = g_AspectRatioV;     // from 0xD6F31C
    CameraRender__InitDefaults(this);
}
```

**Callers:** `PortalTraversal_Native` (0x51A130, called 3 times during recursion), `sub_49DC10`, `sub_4B0770`, `sub_4BE6D0`, `sub_4C4A80`

---

## CameraSettings__InitDefaults (0x4353C0)

Initializes a camera settings buffer with default values. Called at the start of `CameraSystem__UpdateRender` to set up the per-frame working buffer.

```c
void __thiscall CameraSettings__InitDefaults(int this) {
    *(float*)(this + 128) = 0.0;           // +0x80: unknown
    *(float*)(this + 132) = 45.0;          // +0x84: FOV (degrees)
    *(float*)(this + 136) = 2.0;           // +0x88: near clip
    *(float*)(this + 140) = 1024.0;        // +0x8C: far clip
    *(float*)(this + 144) = 0.0;           // +0x90
    *(float*)(this + 148) = 0.0;           // +0x94
    *(float*)(this + 152) = 0.0;           // +0x98
    *(BYTE*)(this + 160) = 0;              // +0xA0
    *(BYTE*)(this + 161) = 0;              // +0xA1
    sub_5A8420(this);                       // init matrix at +0x00
    sub_5A8420(this + 64);                  // init matrix at +0x40
}
```

**Default Values:**
- FOV: 45.0 degrees (converted to radians before passing to SetFovAndClip)
- Near clip: 2.0 units
- Far clip: 1024.0 units

These defaults can be overridden by camera components during the per-frame update loop, and by camera settings loaded from data files (see `structs/camera_settings.md`).

---

## CameraSystem__UpdateRender (0x436190)

The per-frame camera update orchestrator. Runs all camera components, builds the view matrix, converts FOV from degrees to radians, and calls `CameraRender__SetFovAndClip`.

### Flow

```
1. CameraSettings__InitDefaults(workBuffer)     set FOV=45, near=2, far=1024
2. CopyPositionBlock(cameraState)               copy current state
3. For each component:
     component->vtable[11](deltaTime)           update
     component->vtable[3](cameraState, buf)     process
     component->vtable[4]()                      finalize
4. Build eye/target from pipeline output
5. BuildLookAtMatrix(eye, target)                view matrix -> pCamStruct+0x480
6. fovRad = fovDeg * g_Deg2Rad_2                 convert FOV degrees to radians
7. CameraRender__SetFovAndClip(fovRad, g_AspectRatioH, g_AspectRatioV, near, far)
```

The near/far clip values come from the camera state buffer at offsets +0x88 and +0x8C, which were initialized by `CameraSettings__InitDefaults` and potentially modified by camera components.

---

## SetAspectRatioMode (0x4E1680)

Sets the global aspect ratio based on a mode enum. Writes to `g_AspectRatioH` (0xD6F314) and `g_AspectRatioV` (0xD6F31C).

| Mode | Aspect | g_AspectRatioH | g_AspectRatioV |
|------|--------|---------------|---------------|
| 0, 1 | 4:3 | 1.3333334 | 1.0 |
| 2 | 16:9 (computed) | 1.7777778 | 1.0 |
| 3 | 16:9 | 1.7777778 | 1.0 |
| 4 | 4:3 (V scaled) | 1.3333334 | 1.3333334 |

After setting the values, if the mode changed, it notifies a linked list of observers (starting at `dword_D6FF88`) by calling each observer's vtable[0].

---

## Key Global Addresses

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| 0x63EB64 | float (rdata) | -- | Hardcoded projection far plane = 10000.0 |
| 0xD6F314 | float | g_AspectRatioH | Horizontal aspect ratio (1.333 for 4:3, 1.778 for 16:9) |
| 0xD6F31C | float | g_AspectRatioV | Vertical aspect ratio (typically 1.0) |
| 0x7130D0 | float | g_Deg2Rad | Degrees-to-radians conversion factor |
| 0x6E0B78 | float | g_Deg2Rad_2 | Second deg2rad instance (used by UpdateRender) |
| 0xE36E8C | dword* | -- | IDirect3DDevice9 pointer |
| 0xD6F928 | int | -- | Current aspect ratio mode |
| 0xD6FF88 | dword | -- | Aspect ratio observer linked list head |

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| 0x5293D0 | CameraRender__SetFovAndClip | __thiscall(this, fovRad, aspectH, aspectV, near, far) | Write clip/FOV, build frustum + projection |
| 0x52A090 | CameraRender__SetNearClip | __userpurge(ecx, esi, near) | Set near clip, rebuild |
| 0x52A0F0 | CameraRender__SetFarClip | __userpurge(ecx, esi, far) | Set far clip, rebuild |
| 0x52A1A0 | CameraRender__ResetToDefaults | __thiscall(this) | Reset FOV=45deg, aspect from globals |
| 0x436190 | CameraSystem__UpdateRender | __thiscall(this, dt, state, a4) | Per-frame camera orchestrator |
| 0x4353C0 | CameraSettings__InitDefaults | __thiscall(this) | Init defaults: FOV=45, near=2, far=1024 |
| 0x4E1680 | SetAspectRatioMode | __cdecl(mode) | Write g_AspectRatioH/V from mode enum |
| 0x5AABD0 | Frustum__BuildCorners | __thiscall(this, fov, aspectH, aspectV, near, far) | 8 frustum corners for portal traversal |
| 0x5A8B20 | Frustum__BuildProjectionMatrix | __thiscall(this, fov, aspectH, aspectV, near, far) | Proj matrix with hardcoded far=10000.0 |
| 0x529790 | CameraRender__RebuildMatrices | __thiscall(this) | Rebuild additional matrices after clip change |

---

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) -- Camera system overview and object hierarchy
- [WORLD_TO_SCREEN.md](WORLD_TO_SCREEN.md) -- WorldToScreen projection (3D->2D screen coordinates)
- [PORTAL_SYSTEM.md](PORTAL_SYSTEM.md) -- Portal traversal uses frustum corners from this pipeline
- [ROOM_CLIPPING.md](ROOM_CLIPPING.md) -- Room visibility investigation
- [structs/camera_settings.md](structs/camera_settings.md) -- Camera data settings (FOV, far plane from data files)
- [subs/sub_5293D0_CameraRender__SetFovAndClip.md](subs/sub_5293D0_CameraRender__SetFovAndClip.md) -- Main entry point
- [subs/sub_5AABD0_Frustum__BuildCorners.md](subs/sub_5AABD0_Frustum__BuildCorners.md) -- Corner builder
- [subs/sub_5A8B20_Frustum__BuildProjectionMatrix.md](subs/sub_5A8B20_Frustum__BuildProjectionMatrix.md) -- Projection builder
