# World-to-Screen Projection
**Status:** Identified, VM-protected body, callable + reimplementable
**Last updated:** 2026-03-18

---

## Summary

The engine's WorldToScreen function is `sub_52A150` at **0x52A150**. It transforms a 3D world position to 2D screen pixel coordinates in-place. The function body is **Kallis VM-protected**, but it can be called directly at runtime or reimplemented using the known matrix layout.

---

## Engine Function: sub_52A150

**Address:** 0x52A150
**Convention:** `__thiscall`
**Prototype:**
```c
void __thiscall WorldToScreen(CameraRender* this, float* inout_vec3);
```

### Parameters

| Param | Register/Stack | Type | Description |
|-------|---------------|------|-------------|
| this | ECX | CameraRender* | Camera render object from `GetCameraObject()` or `[0x72F670]` |
| inout_vec3 | stack arg1 | float[3] | In-place: input = world XYZ, output = screen XYZ |

### Output Format

After the call, the float[3] buffer is overwritten:
- `[0]` = screen X (pixels, 0 = left edge)
- `[1]` = screen Y (pixels, 0 = top edge)
- `[2]` = depth flag (>= 0.0 and <= 1.0 means visible / in front of camera)

### Visibility Check

Callers check the depth flag differently:
- `sub_4075E0`: `if (result[2] <= 1.0)` -- point is in front of camera
- `sub_4076B0`: `if (result[2] >= 0.0)` -- point is in front of camera

Safe check: `result[2] >= 0.0 && result[2] <= 1.0`

---

## How to Call from ASI Mod

```cpp
// Function pointer type
typedef void (__thiscall* WorldToScreen_t)(void* cameraRender, float* inout_vec3);
WorldToScreen_t WorldToScreen = (WorldToScreen_t)0x52A150;

// Get camera render object
void* camRender = *(void**)0x72F670;

// Transform
float pos[3] = { worldX, worldY, worldZ };
WorldToScreen(camRender, pos);

float screenX = pos[0];
float screenY = pos[1];
bool visible  = (pos[2] >= 0.0f && pos[2] <= 1.0f);
```

**Important:** The Kallis VM must be initialized (it is during normal gameplay). This function is safe to call from any game thread context where the camera has been updated.

---

## Alternative: Reimplement Without VM

If calling the VM-protected function is unreliable, you can reimplement W2S using the engine's matrices:

### Required Data (from CameraRender object at `[0x72F670]`)

| Offset | Type | Field |
|--------|------|-------|
| +0x408 (1032) | float[16] | View matrix (4x4) |
| +0x448 (1096) | float[16] | ViewProjection matrix (View * GlobalScale * Projection) |
| +0x5C8 (1480) | float[16] | Projection matrix (4x4) |
| +0x648 (1608) | float[16] | View * GlobalScale matrix |
| +0x6C8 (1736) | __int16 | Viewport width (pixels) |
| +0x6CA (1738) | __int16 | Viewport height (pixels) |
| +0x6D0 (1744) | __int16 | Viewport X offset |
| +0x6D2 (1746) | __int16 | Viewport Y offset |

### Reimplementation

```cpp
#include <d3dx9.h>

bool WorldToScreenManual(const D3DXVECTOR3& worldPos, float& screenX, float& screenY)
{
    void* camRender = *(void**)0x72F670;
    if (!camRender) return false;

    // Get ViewProjection matrix at CameraRender+0x448
    D3DXMATRIX* viewProj = (D3DXMATRIX*)((BYTE*)camRender + 0x448);

    // Get viewport dimensions at CameraRender+0x6C8
    short vpWidth  = *(short*)((BYTE*)camRender + 0x6C8);
    short vpHeight = *(short*)((BYTE*)camRender + 0x6CA);
    short vpX      = *(short*)((BYTE*)camRender + 0x6D0);
    short vpY      = *(short*)((BYTE*)camRender + 0x6D2);

    // Transform world position by ViewProjection (with perspective divide)
    D3DXVECTOR3 ndc;
    D3DXVec3TransformCoord(&ndc, &worldPos, viewProj);

    // NDC is in [-1, 1] range after TransformCoord (which does /w internally)
    // Check if behind camera (TransformCoord clamps, but Z < 0 or > 1 = behind)
    // For a more robust check, use D3DXVec3Transform to get the W component:
    D3DXVECTOR4 clipSpace;
    D3DXVec3Transform(&clipSpace, &worldPos, viewProj);
    if (clipSpace.w <= 0.0f) return false;  // behind camera

    // Map NDC to screen pixels
    // Based on reverse of sub_529D30 (ScreenToWorld):
    //   ndcX = (screenX/width - (vpX/width + 0.5)) * 2
    //   ndcY = (screenY/height - (vpY/height + 0.5)) * -2
    // Inverse:
    screenX = (ndc.x / 2.0f + 0.5f + (float)vpX / (float)vpWidth) * (float)vpWidth;
    screenY = (-ndc.y / 2.0f + 0.5f + (float)vpY / (float)vpHeight) * (float)vpHeight;

    // Simplified (when vpX=0, vpY=0, which is typical):
    // screenX = (ndc.x + 1.0f) * 0.5f * vpWidth;
    // screenY = (1.0f - ndc.y) * 0.5f * vpHeight;

    return true;
}
```

### Note on GlobalScale Matrix

The ViewProjection matrix at +0x448 includes a global scale factor (multiplied by the matrix at global `0xE55A28`). This is part of the engine's coordinate system. When calling `D3DXVec3TransformCoord` with this matrix, the global scale is already baked in, so world positions transform correctly.

If you only use the raw View (+0x408) and Projection (+0x5C8) matrices separately, you must also include the global scale. The combined matrix at +0x448 is preferred.

---

## Internal Flow (VM-Protected)

Assembly preamble before entering VM:

```asm
sub_52A150:
  push esi
  mov  esi, [esp+4+arg_0]    ; esi = float[3]* (world position / output)
  push edi
  push esi                    ; push vec3 pointer (arg for VM body)
  mov  edi, ecx               ; edi = CameraRender* (this)
  call sub_529CC0             ; enters Kallis VM
  jmp  ds:off_1C86828         ; continues in VM
```

`sub_529CC0` (also VM-protected) receives:
- ESI = CameraRender*
- stack: output buffer pointer, vec3 pointer

The VM body performs the equivalent of:
1. Transform worldPos by ViewProjection matrix (CameraRender+0x448)
2. Perspective divide (÷ w)
3. Map NDC to screen pixels using viewport (CameraRender+0x6C8..0x6D2)
4. Write result back to the input float[3]

---

## Helper Functions

### GetCameraObject (0x4368B0)

Returns the CameraRender object pointer. VM-protected but trivially:
```c
void* GetCameraObject() { return *(void**)0x72F670; }
```
Returns value in EAX. Also sets ECX = EAX for subsequent thiscall.

### sub_436B70 (0x436B70)

Non-VM version, returns the same pointer:
```c
void* sub_436B70() { return *(void**)0x72F670; }
```

---

## Callers (Known Usage)

| Address | Function | Description |
|---------|----------|-------------|
| 0x4075E0 | Debug text at world pos | Draws debug text at projected 3D position |
| 0x4076B0 | Debug text at entity | Projects entity position, draws label |
| 0x4C13F0 | 3D sprite render | Projects 3D position, renders sprite/icon at screen pos |
| 0x4C9EE0 | Control indicator update | Projects interaction target to screen for "Press E" prompt |

All callers follow the same pattern:
```
1. Copy world position to local float[3]
2. Call GetCameraObject() or sub_436B70() -> ECX
3. Push &float[3]
4. Call sub_52A150
5. Use result[0] as screenX, result[1] as screenY
6. Check result[2] for visibility
```

---

## Related Functions

| Address | Name | Description |
|---------|------|-------------|
| 0x529D30 | ScreenToWorld (unproject) | Inverse: screen pixel coords -> world ray direction |
| 0x529E20 | ScreenToRay | Screen coords -> view-space ray |
| 0x529880 | RebuildMatrices_WithViewport | Builds ViewProj*Viewport combined matrix (alt path) |
| 0x529790 | CameraRender__RebuildMatrices | Rebuilds View/Proj matrix chain |
| 0x5A8360 | Matrix::TransformCoord | D3DXVec3TransformCoord wrapper (perspective divide) |
| 0x5A8320 | Matrix::Transform4 | D3DXVec3Transform wrapper (returns vec4 with W) |

---

## CameraRender Object Layout (Extended)

Extends the table from [CAMERA_RENDERING.md](CAMERA_RENDERING.md):

| Offset | Type | Field |
|--------|------|-------|
| +0x000 | float | Near clip distance |
| +0x004 | float | Far clip distance |
| +0x008 | float | Depth precision value |
| +0x00C | float | FOV (radians) |
| +0x010 | float | Aspect ratio H |
| +0x014 | float | Aspect ratio V |
| +0x01C | int | Matrix order flag |
| +0x020 | float[32] | Frustum corners (8 corners x 4 floats) |
| +0x408 | float[16] | **View matrix** |
| +0x448 | float[16] | **ViewProjection matrix** (View * GlobalScale * Projection) |
| +0x4C8 | float[16] | Combined matrix (workspace) |
| +0x508 | float[16] | Viewport/depth scale matrix |
| +0x548 | float[16] | ViewProjection copy (used in viewport path) |
| +0x588 | float[16] | ViewProj * Viewport combined matrix |
| +0x5C8 | float[16] | **Projection matrix** |
| +0x648 | float[16] | View * GlobalScale matrix |
| +0x688 | float[16] | Inverse view / view copy |
| +0x6B8 | float | Camera position X |
| +0x6BC | float | Camera position Y |
| +0x6C0 | float | Camera position Z |
| +0x6C8 | __int16 | **Viewport width** (pixels) |
| +0x6CA | __int16 | **Viewport height** (pixels) |
| +0x6D0 | __int16 | Viewport X offset |
| +0x6D2 | __int16 | Viewport Y offset |
| +0x6E8 | float[16] | Additional matrix (pre-viewport) |

---

## Related Documentation

- [CAMERA_RENDERING.md](CAMERA_RENDERING.md) -- Projection matrix build pipeline
- [ARCHITECTURE.md](ARCHITECTURE.md) -- Camera system overview
