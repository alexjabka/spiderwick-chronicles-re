# sub_5A8B20 -- Frustum__BuildProjectionMatrix
**Address:** 0x5A8B20 (Spiderwick.exe+1A8B20)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
int __thiscall Frustum__BuildProjectionMatrix(
    float *this,    // destination buffer (camObj+0x5C8)
    float fovRad,   // FOV in radians
    float aspectH,  // horizontal aspect ratio
    float aspectV,  // vertical aspect ratio
    float nearClip, // near clip distance
    float farClip   // far clip distance -- HARDCODED to 10000.0 by caller
);
```

## Purpose

Builds a 4x4 perspective projection matrix and stores it at `this`. The matrix is then multiplied by a coordinate-swap matrix using `D3DXMatrixMultiply` to convert from the engine's coordinate system to Direct3D's.

## Algorithm

```
cotHalfFov = 1.0 / tan(fovRad / 2)
fRange = farClip / (farClip - nearClip)

Projection matrix (before coordinate swap):
  [0]  = cotHalfFov / aspectH
  [5]  = cotHalfFov * aspectV    (note: multiplied, not divided)
  [10] = fRange
  [11] = 1.0                     (perspective W)
  [14] = -(fRange * nearClip)
  [15] = 0.0
  all other elements = 0.0

Coordinate swap matrix (local):
  Identity with [10] = -1.0, [14] = 1.0
  (swaps Z direction and shifts W)

Final = projection * coordinateSwap (via D3DXMatrixMultiply)
```

## Critical Detail: Hardcoded Far Plane

When called from `CameraRender__SetFovAndClip`, the `farClip` argument is **always 10000.0** (loaded from constant at 0x63EB64 in .rdata). The logical far clip stored in the camera object (+0x04) is NOT used for the projection matrix.

This means:
- Frustum corners (from `Frustum__BuildCorners`) use the logical far clip (e.g. 1024.0)
- Projection matrix uses 10000.0
- Portals are culled at 1024 units, but geometry renders to 10000 units

## Size

233 bytes. Single basic block, cyclomatic complexity 1.

## Callers

| Address | Function | Context |
|---------|----------|---------|
| 0x52944F | CameraRender__SetFovAndClip | Main per-frame call (far=10000.0) |
| 0x52976B | sub_529470 | Unknown |

## Callees

- `D3DXMatrixMultiply` (d3dx9)
- `_tan` (CRT math)

## Notes

- The `aspectV` multiplication (not division) in element [5] is unusual. Most projection matrices divide by aspect ratio for the Y component. This may be because the engine passes `aspectV = 1.0` in most cases, making the distinction irrelevant.
- The coordinate-swap matrix converts from the engine's Y-forward/Z-up system to D3D's Z-forward/Y-up system.

## Related

- [sub_5293D0_CameraRender__SetFovAndClip.md](sub_5293D0_CameraRender__SetFovAndClip.md) -- Caller
- [sub_5AABD0_Frustum__BuildCorners.md](sub_5AABD0_Frustum__BuildCorners.md) -- Sibling (frustum corners)
- [../CAMERA_RENDERING.md](../CAMERA_RENDERING.md) -- Pipeline overview
