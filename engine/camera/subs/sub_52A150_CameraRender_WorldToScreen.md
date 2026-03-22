# sub_52A150 -- CameraRender__WorldToScreen
**Address:** 0x52A150 (Spiderwick.exe+12A150)
**Status:** VM-PROTECTED BODY (Kallis), fully understood from callers + reimplementation

## Signature

```c
void __thiscall CameraRender__WorldToScreen(
    CameraRender* this,    // ECX: camera render object from [0x72F670]
    float* inout_vec3      // stack arg1: float[3], input = world XYZ, output = screen XYZ
);
```

## Purpose

Transforms a 3D world-space position to 2D screen-space pixel coordinates in-place. This is the engine's canonical projection function used by debug text, 3D sprites, and UI indicator systems. The function body is Kallis VM-protected but the math is fully understood.

## Input / Output

**Input:** `inout_vec3` contains world position `{X, Y, Z}`.

**Output:** The same buffer is overwritten:

| Index | Field | Description |
|-------|-------|-------------|
| [0] | screenX | Screen X in pixels (0 = left edge) |
| [1] | screenY | Screen Y in pixels (0 = top edge) |
| [2] | depth | Normalized depth (0.0 to 1.0 = visible / in front of camera) |

## Internal Math (VM Body)

The VM-protected body performs the following equivalent operations:

### Step 1: ViewProjection Transform

Multiply the world position by the pre-computed ViewProjection matrix at `CameraRender+0x448` (16 floats, row-major):

```
cx = wx*M[0] + wy*M[4] + wz*M[8]  + M[12]
cy = wx*M[1] + wy*M[5] + wz*M[9]  + M[13]
cz = wx*M[2] + wy*M[6] + wz*M[10] + M[14]
cw = wx*M[3] + wy*M[7] + wz*M[11] + M[15]
```

This is equivalent to `D3DXVec3Transform(&clipSpace, &worldPos, viewProj)`.

### Step 2: Perspective Divide

```
ndcX = cx / cw
ndcY = cy / cw
ndcZ = cz / cw     // this becomes the depth value in [2]
```

If `cw <= 0`, the point is behind the camera and projection is invalid.

### Step 3: NDC to Screen Pixels

Map from normalized device coordinates (NDC range [-1, +1]) to screen pixel coordinates using the viewport at `CameraRender+0x6C8`:

```
screenX = (ndcX + 1.0) * 0.5 * vpWidth
screenY = (1.0 - ndcY) * 0.5 * vpHeight
```

Where:
- `vpWidth` = `*(short*)(CameraRender + 0x6C8)`
- `vpHeight` = `*(short*)(CameraRender + 0x6CA)`

The Y axis is flipped (NDC +Y is up, screen +Y is down).

## Data Dependencies

### ViewProjection Matrix (CameraRender+0x448)

| Offset | Type | Description |
|--------|------|-------------|
| +0x448 | float[16] | Combined View * GlobalScale * Projection matrix |

This matrix is rebuilt each frame by `CameraRender__RebuildMatrices` (0x529790). It includes the engine's global scale factor (from `0xE55A28`), so world coordinates transform correctly without additional scaling.

### Viewport (CameraRender+0x6C8)

| Offset | Type | Description |
|--------|------|-------------|
| +0x6C8 | short | Viewport width in pixels |
| +0x6CA | short | Viewport height in pixels |
| +0x6D0 | short | Viewport X offset (typically 0) |
| +0x6D2 | short | Viewport Y offset (typically 0) |

## Assembly Preamble

```asm
sub_52A150:
  push  esi
  mov   esi, [esp+4+arg_0]    ; esi = float[3]* (inout_vec3)
  push  edi
  push  esi                    ; push for VM body
  mov   edi, ecx               ; edi = CameraRender* (this)
  call  CameraRender__WorldToScreen_VM_Inner  ; 0x52A159 -> enters Kallis VM
  jmp   ds:off_1C86828         ; continues in VM
```

**Size:** 20 bytes (tiny stub, real work is in VM).
**Cyclomatic complexity:** 1 (linear flow into VM).

## Callers

| Address | Function | Description |
|---------|----------|-------------|
| 0x407645 | sub_4075E0 | Debug text at world position |
| 0x40771E | sub_4076B0 | Debug text at entity position |
| 0x4C1431 | Sprite3D__Render | 3D sprite projection for billboards |
| 0x4C9FB1 | ControlIndicator__UpdateScreenPos | "Press E" prompt screen positioning |

All callers follow the same pattern:
1. Copy world position to local `float[3]`
2. Get CameraRender pointer via `GetCameraObject()` (0x4368B0) or `sub_436B70`
3. Call `WorldToScreen(this, &vec3)`
4. Read `vec3[0]` as screenX, `vec3[1]` as screenY
5. Check `vec3[2]` for visibility (0.0 to 1.0 = visible)

## Visibility Checks by Callers

| Caller | Check | Logic |
|--------|-------|-------|
| sub_4075E0 | `result[2] <= 1.0` | In front of camera |
| sub_4076B0 | `result[2] >= 0.0` | In front of camera |
| SpiderMod | `ndcZ >= 0.0 && ndcZ <= 1.0` | Both bounds (safe combined check) |

## SpiderMod Reimplementation

SpiderMod reimplements this function to avoid calling into the Kallis VM from the render thread. The mod reads the VP matrix and viewport each frame from the CameraSectorUpdate hook (game update context) and performs the projection math in its own `WorldToScreen()` in `menu.cpp`. See [SpiderMod 3D Debug Overlay](../../../mods/spidermod/docs/3D_DEBUG_OVERLAY.md).

## Related

- [../WORLD_TO_SCREEN.md](../WORLD_TO_SCREEN.md) -- Full projection pipeline documentation
- [sub_5293D0_CameraRender__SetFovAndClip.md](sub_5293D0_CameraRender__SetFovAndClip.md) -- FOV/clip setup
- [sub_5A8B20_Frustum__BuildProjectionMatrix.md](sub_5A8B20_Frustum__BuildProjectionMatrix.md) -- Projection matrix builder
- [sub_5299A0_ViewMatrixWrapper.md](sub_5299A0_ViewMatrixWrapper.md) -- View matrix setup
