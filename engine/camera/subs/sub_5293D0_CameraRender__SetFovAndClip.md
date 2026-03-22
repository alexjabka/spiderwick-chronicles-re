# sub_5293D0 -- CameraRender__SetFovAndClip
**Address:** 0x5293D0 (Spiderwick.exe+1293D0)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
int __thiscall CameraRender__SetFovAndClip(
    float *this,    // camera render object
    float fovRad,   // FOV in radians
    float aspectH,  // horizontal aspect ratio (e.g. 1.333)
    float aspectV,  // vertical aspect ratio (e.g. 1.0)
    float nearClip, // near clip distance
    float farClip   // far clip distance
);
```

## Purpose

Writes FOV and clip plane values to the camera render object, then rebuilds the frustum corners and projection matrix. This is the central entry point for updating the camera's rendering parameters.

## Behavior

1. Write parameters to object fields:
   - `this[0]` (+0x00) = nearClip
   - `this[1]` (+0x04) = farClip
   - `this[3]` (+0x0C) = fovRad
   - `this[4]` (+0x10) = aspectH
   - `this[5]` (+0x14) = aspectV

2. Call `Frustum__BuildCorners(fovRad, aspectH, aspectV, nearClip, farClip)` at this+0x20
   - Builds 8 frustum corners used by portal traversal

3. Call `Frustum__BuildProjectionMatrix(fovRad, aspectH, aspectV, nearClip, 10000.0)` at this+0x5C8
   - **Note:** Far plane is HARDCODED to 10000.0 (constant at 0x63EB64), NOT the farClip parameter

4. Call `IDirect3DDevice9::SetTransform(3, this+0x5C8)` via `[dword_E36E8C]+176`
   - Sets the projection matrix on the D3D device
   - Transform type 3 = D3DTS_PROJECTION

## Size

155 bytes (0x9B). Single basic block, cyclomatic complexity 1.

## Callers

| Address | Function | Context |
|---------|----------|---------|
| 0x436376 | CameraSystem__UpdateRender | Per-frame camera update (main caller) |
| 0x48F442 | StaticCamera__Init | Static camera initialization |
| 0x48FF1D | StaticCamera__Update | Static camera update |
| 0x49D6C9 | sub_49D640 | Unknown caller |
| 0x4B071E | sub_4B06A0 | Unknown caller |
| 0x4BE638 | sub_4BE5C0 | Unknown caller |
| 0x4C5118 | sub_4C4A80 | Unknown caller |
| 0x52A0BB | CameraRender__SetNearClip | Near clip setter |
| 0x52A11B | CameraRender__SetFarClip | Far clip setter |

## Callees

- `Frustum__BuildCorners` (0x5AABD0)
- `Frustum__BuildProjectionMatrix` (0x5A8B20)

## Key Observation

The projection matrix far plane is hardcoded to 10000.0, creating an intentional mismatch:
- Portal traversal frustum corners use the actual `farClip` parameter (default 1024.0)
- The GPU projection matrix uses 10000.0

This means geometry will render at distances up to 10000 units, but the portal system will only traverse portals within the logical far clip distance.

## Related

- [../CAMERA_RENDERING.md](../CAMERA_RENDERING.md) -- Pipeline overview
- [sub_5AABD0_Frustum__BuildCorners.md](sub_5AABD0_Frustum__BuildCorners.md) -- Corner builder
- [sub_5A8B20_Frustum__BuildProjectionMatrix.md](sub_5A8B20_Frustum__BuildProjectionMatrix.md) -- Projection builder
