# sub_48FE90 -- StaticCamera__Update

## Address
`0x0048FE90`

## Signature
```c
int __thiscall StaticCamera__Update(void* this, float dt);
```

## Purpose
Updates the static/debug camera each frame. Handles camera activation, reads input for pitch/yaw rotation and movement, and applies transforms. This is the core camera controller for VIEWER mode (game mode 7).

## Logic Flow

### 1. Camera Activation
When `byte_D417F3` (DEBUG_CAMERA_ACTIVATE) becomes non-zero:
- Calls `StaticCamera__Init` to reset camera state to defaults.
- Clears the activation flag after init.

### 2. Input Reading
- Calls `sub_48F460` -- reads input devices and writes pitch/yaw deltas to:
  - `flt_D417F4` -- pitch angle (up/down rotation)
  - `flt_D41908` -- yaw angle (left/right rotation) (note: `0xD41908` = `g_StaticRenderCamera + ??? offset` -- part of the camera struct or nearby global)

### 3. Movement Application
- Calls `sub_48F500` -- applies movement using the camera's orientation vectors stored at `this + 16` through `this + 36`:
  - Pitch, yaw angles
  - Forward vector
  - Right vector
  - Up vector
- Movement speed may be affected by `byte_D417F2` (slow mode flag).

## Debug Input Globals

| Address | Name | Type | Description |
|---------|------|------|-------------|
| `byte_D417F0` | DEBUG_LIGHT_REMOVE | `uint8` | Triggers light removal action |
| `byte_D417F1` | DEBUG_DIRECTION | `uint8` | Triggers direction change action |
| `byte_D417F2` | DEBUG_TELEPORT_SLOWMODE | `uint8` | Teleport to target / toggle slow camera mode |
| `byte_D417F3` | DEBUG_CAMERA_ACTIVATE | `uint8` | Triggers camera initialization/reset |
| `byte_D41905` | DEBUG_LIGHT_ADD | `uint8` | Triggers light addition action |
| `byte_D41906` | DEBUG_LIGHT_MODIFY | `uint8` | Enables light modification mode (see sub_490270) |

## Camera State Globals

| Address | Name | Type | Description |
|---------|------|------|-------------|
| `flt_D417F4` | CameraPitch | `float` | Current pitch angle (radians) |
| `flt_D41908` | CameraYaw | `float` | Current yaw angle (radians) |
| `0xD41928` | g_StaticRenderCamera | `StaticCamera` | Global static camera object instance |

## Camera Object Layout (partial)

| Offset | Type | Description |
|--------|------|-------------|
| `+0x10` (16) | `float` | Pitch component |
| `+0x14` (20) | `float` | Yaw component |
| `+0x18` (24) | `float[3]` | Forward vector |
| `+0x1C` (28) | `float[3]` | Right vector |
| `+0x24` (36) | `float[3]` | Up vector |

## Callers
- `sub_490270` (StaticCameraRender) -- called each frame in VIEWER mode.

## Callees
- `StaticCamera__Init` -- resets camera to default position/orientation.
- `sub_48F460` -- reads input and computes pitch/yaw from input devices.
- `sub_48F500` -- applies movement using orientation vectors and delta time.

## Notes
- The `__thiscall` convention means `this` (the camera object pointer) is passed in ECX.
- The debug input globals (`byte_D417F0` through `byte_D41906`) form a cluster of single-byte flags at the start of the static camera data region. They are likely set by input action handlers elsewhere.
- The slow mode toggle (`byte_D417F2`) allows precision camera positioning by reducing movement speed.
- Camera activation (`byte_D417F3`) acts as a "reset camera" function -- it reinitializes the camera to a default state, useful when the camera gets lost or oriented incorrectly.
