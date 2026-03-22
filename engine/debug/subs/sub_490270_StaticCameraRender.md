# sub_490270 -- Static Camera Render Pipeline

## Address
`0x00490270`

## Signature
```c
void sub_490270();
```

## Purpose
Render pipeline for the static/debug camera mode. Sets up ambient lighting, handles debug light modification controls, and drives the static camera update. Only active when the game is in **VIEWER mode** (game mode 7, selected by `sub_4DDA00`).

## Logic Flow

### 1. Ambient Setup
- Sets ambient light to `(0x20, 0x20, 0x20)` -- a dim base ambient.

### 2. Debug Light Modification
When `byte_D41906` (DEBUG_LIGHT_MODIFY) is non-zero:
- Adjusts `flt_6E55A0` (ambient light intensity) via input.
- Adjusts `flt_6E55A4` (sun light intensity) via input.
- Rotates the sun direction vector via input.
- Displays on-screen text labels:
  - `"Ambient Light"` -- shows current ambient intensity.
  - `"Sun Light"` -- shows current sun intensity.
- Text rendering via `sub_4DF660`.

### 3. Camera Update
- Calls `StaticCamera__Update` (`0x48FE90`) with `g_StaticRenderCamera` (`0xD41928`).

## Key Globals

| Address | Name | Type | Description |
|---------|------|------|-------------|
| `byte_D41906` | DEBUG_LIGHT_MODIFY | `uint8` | Enables debug light adjustment controls |
| `flt_6E55A0` | AmbientIntensity | `float` | Current ambient light intensity |
| `flt_6E55A4` | SunIntensity | `float` | Current sun/directional light intensity |
| `0xD41928` | g_StaticRenderCamera | `StaticCamera*` | Global static camera instance used in viewer mode |

## Callers
- Main render dispatch -- called when game mode is 7 (VIEWER).
- Game mode selection via `sub_4DDA00`.

## Callees
- `StaticCamera__Update` (`sub_48FE90`) -- updates camera position/orientation from input.
- `sub_4DF660` -- renders debug text ("Ambient Light", "Sun Light") on screen.
- Ambient light setter (sets RGB to 0x20 each).
- Light direction rotation functions.

## Notes
- This pipeline is specifically for the VIEWER game mode (mode 7), which is a developer tool for inspecting scenes with a free-flying camera and adjustable lighting.
- The low ambient (0x20 = 32/255 per channel) provides a base illumination that prevents complete darkness when sun intensity is zeroed out.
- Debug light controls allow real-time adjustment of both ambient and directional light, useful for art review and debugging shadow/lighting issues.
- In normal gameplay (mode 5), this function is never called.
