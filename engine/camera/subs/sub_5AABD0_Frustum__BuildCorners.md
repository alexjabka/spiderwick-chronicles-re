# sub_5AABD0 -- Frustum__BuildCorners
**Address:** 0x5AABD0 (Spiderwick.exe+1AABD0)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
void __thiscall Frustum__BuildCorners(
    float *this,    // destination buffer (camObj+0x20)
    float fovRad,   // FOV in radians
    float aspectH,  // horizontal aspect ratio
    float aspectV,  // vertical aspect ratio
    float nearClip, // near clip distance
    float farClip   // far clip distance
);
```

## Purpose

Builds 8 frustum corners (4 near-plane + 4 far-plane) plus a center point from the camera's FOV, aspect ratio, and clip distances. These corners are used by the portal traversal system to determine which portals are visible.

## Algorithm

```
halfTan = tan(fovRad / 2)

Near plane:
  halfW_near = halfTan * nearClip * aspectH
  halfH_near = halfTan * nearClip * aspectV

Far plane:
  halfW_far = halfTan * farClip * aspectH
  halfH_far = halfTan * farClip * aspectV

Corners (view-space, Y-forward):
  near[0] = (-halfW_near, nearClip, +halfH_near)   top-left
  near[1] = (+halfW_near, nearClip, +halfH_near)   top-right
  near[2] = (+halfW_near, nearClip, -halfH_near)   bottom-right
  near[3] = (-halfW_near, nearClip, -halfH_near)   bottom-left

  far[0]  = (-halfW_far, farClip, +halfH_far)      top-left
  far[1]  = (+halfW_far, farClip, +halfH_far)      top-right
  far[2]  = (+halfW_far, farClip, -halfH_far)      bottom-right
  far[3]  = (-halfW_far, farClip, -halfH_far)      bottom-left

Center:
  center  = (0, (nearClip + farClip) / 2, 0)

Saved params:
  this[110] = fovRad
  this[111] = aspectH
  this[112] = aspectV
```

## Output Layout

Each corner is 4 floats (16 bytes). Total output: 35 floats.

| Offset (from this) | Content |
|---------------------|---------|
| +0x00 (float[0]) | Near top-left X |
| +0x04 (float[1]) | Near top-left Y (= nearClip) |
| +0x08 (float[2]) | Near top-left Z |
| ... | ... (3 more near corners) |
| +0x40 (float[16]) | Far top-left X |
| ... | ... (3 more far corners) |
| +0x80 (float[32]) | Center X (0.0) |
| +0x84 (float[33]) | Center Y (midpoint) |
| +0x88 (float[34]) | Center Z (0.0) |
| +0x1B8 (float[110]) | Saved FOV |
| +0x1BC (float[111]) | Saved aspect H |
| +0x1C0 (float[112]) | Saved aspect V |

## Size

248 bytes. Single basic block, cyclomatic complexity 1.

## Callers

| Address | Function |
|---------|----------|
| 0x529416 | CameraRender__SetFovAndClip |

## Callees

- `_tan` (CRT math)

## Notes

- Coordinates are in **view space** (Y-forward, Z-up, X-right), not world space
- The portal traversal system transforms these corners to world space using the view matrix
- Uses `tan(fov/2)` to compute the half-extents, which is the standard perspective projection formula

## Related

- [sub_5293D0_CameraRender__SetFovAndClip.md](sub_5293D0_CameraRender__SetFovAndClip.md) -- Caller
- [sub_5A8B20_Frustum__BuildProjectionMatrix.md](sub_5A8B20_Frustum__BuildProjectionMatrix.md) -- Sibling (projection matrix)
- [../CAMERA_RENDERING.md](../CAMERA_RENDERING.md) -- Pipeline overview
