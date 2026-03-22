# SpiderMod -- Camera Override System
**Module:** `menu.cpp` (UI) + `dllmain.cpp` (per-frame application)
**Status:** Implemented and working

---

## Overview

The camera override system allows real-time adjustment of the game camera's field of view, near clip distance, and far clip distance through ImGui sliders. When enabled, SpiderMod calls the engine's `SetFovAndClip` function every frame from the CameraSectorUpdate hook, overriding whatever values the game's camera scripts would normally set.

---

## Architecture

### Two-Part Design

1. **UI (menu.cpp)** -- The "Camera" collapsing header provides an "Override Camera" checkbox and parameter sliders. The UI writes to global variables declared in `dllmain.cpp`.

2. **Application (dllmain.cpp :: HookedCameraSectorUpdate)** -- Each frame, if `g_cameraOverride` is true, the hook reads the global parameters, converts FOV to radians, reads the engine's aspect ratios, and calls `SetFovAndClip` on the CameraRender object.

### Why CameraSectorUpdate?

Camera parameters must be set from the game update context, not the EndScene render context. The CameraSectorUpdate function (0x488DD0) runs once per game tick and is the correct place to inject camera changes -- the engine's own camera scripts set FOV/clip from this same call chain.

---

## Global Variables

Declared in `dllmain.cpp`, externed in `menu.cpp`:

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `g_cameraOverride` | bool | false | Master enable for camera overrides |
| `g_cameraFov` | float | 45.0 | FOV in degrees |
| `g_cameraNear` | float | 2.0 | Near clip distance |
| `g_cameraFar` | float | 1024.0 | Far clip distance |

---

## Per-Frame Application

From `HookedCameraSectorUpdate` in `dllmain.cpp`:

```cpp
if (g_cameraOverride) {
    uintptr_t camObj = mem::Deref(addr::pCameraObj);  // [0x72F670]
    if (camObj) {
        float fovRad = g_cameraFov * 3.14159265f / 180.0f;

        // Read engine's aspect ratios
        float aspectH = mem::Read<float>(0xD6F314);  // g_AspectRatioH
        float aspectV = mem::Read<float>(0xD6F31C);  // g_AspectRatioV
        if (aspectH < 0.1f) aspectH = 1.333f;  // fallback
        if (aspectV < 0.1f) aspectV = 1.0f;    // fallback

        // Call engine function
        typedef int (__thiscall *SetFovClipFn)(void*, float, float, float, float, float);
        auto setFovClip = reinterpret_cast<SetFovClipFn>(0x5293D0);
        setFovClip((void*)camObj, fovRad, aspectH, aspectV, g_cameraNear, g_cameraFar);
    }
}
```

### Aspect Ratio Globals

| Address | Type | Name | Typical Value |
|---------|------|------|---------------|
| 0xD6F314 | float | g_AspectRatioH | 1.333 (4:3) or 1.778 (16:9) |
| 0xD6F31C | float | g_AspectRatioV | 1.0 |

These are set by the engine at startup based on display resolution via `SetAspectRatioMode` (0x4E1680). SpiderMod reads them to preserve the current aspect ratio when overriding FOV/clip.

### Engine Function: SetFovAndClip (0x5293D0)

```c
int __thiscall CameraRender__SetFovAndClip(
    CameraRender* this,
    float fovRad,     // FOV in radians
    float aspectH,    // horizontal aspect ratio
    float aspectV,    // vertical aspect ratio
    float nearClip,   // near clip distance
    float farClip     // far clip distance
);
```

This function:
1. Writes FOV/clip/aspect to CameraRender fields (+0x00, +0x04, +0x0C, +0x10, +0x14)
2. Rebuilds frustum corners via `Frustum__BuildCorners` (0x5AABD0)
3. Rebuilds projection matrix via `Frustum__BuildProjectionMatrix` (0x5A8B20)
4. Sets the D3D projection transform via `IDirect3DDevice9::SetTransform(D3DTS_PROJECTION, ...)`

See [engine docs](../../../engine/camera/subs/sub_5293D0_CameraRender__SetFovAndClip.md) for full details.

---

## ImGui Interface

Located in the "Camera" collapsing header of the main SpiderMod menu:

### Live Value Display

When the override is disabled, the UI reads current values from the CameraRender object to show what the game is using:
- Near clip: `camObj+0x00`
- Far clip: `camObj+0x04`
- FOV (radians): `camObj+0x0C` (displayed in degrees)
- Aspect H/V: `camObj+0x10`, `camObj+0x14`

This allows seeing the game's current settings before enabling the override.

### Controls

| Control | Type | Range | Description |
|---------|------|-------|-------------|
| Override Camera | Checkbox | on/off | Master toggle |
| FOV (deg) | SliderFloat | 10.0 -- 150.0 | Field of view in degrees |
| Near Clip | SliderFloat | 0.01 -- 50.0 | Near clip plane distance |
| Far Clip | SliderFloat | 100.0 -- 50000.0 | Far clip plane distance |

### Preset Buttons

| Button | Action | Values |
|--------|--------|--------|
| Reset Defaults | Restore engine defaults | FOV=45, Near=2.0, Far=1024.0 |
| Ultra Far | Maximize far clip | Far=50000.0 |
| Wide FOV | Set wide-angle FOV | FOV=90.0 |

### Always-Visible Status

Regardless of override state, the UI shows live readback:
```
Live: FOV=45.0  Near=2.00  Far=1024
Aspect: 1.333 x 1.000
```

---

## Design Notes

### Graceful Override

The system works WITH the engine by calling `SetFovAndClip` every frame, not by patching memory or NOPing game code. When the override is disabled, the game's camera scripts resume control immediately -- no restoration needed.

### FOV Conversion

The UI works in degrees (human-readable) while the engine uses radians internally:
```
fovRad = fovDeg * PI / 180.0
```

### Projection Matrix Far Plane Quirk

`SetFovAndClip` has a hardcoded far plane of 10000.0 for the GPU projection matrix (regardless of the `farClip` parameter). The `farClip` parameter only affects frustum corner generation for portal traversal. This means:
- Setting Far Clip > 1024 extends portal traversal range (more rooms visible)
- Geometry always renders up to 10000 units from camera regardless of Far Clip setting
- Setting Far Clip > 10000 has no additional effect on rendering distance

### Override Disabled Behavior

When `g_cameraOverride` is false, the UI syncs its slider values from the CameraRender object each frame. This means enabling the override starts from the game's current values, not from stale slider positions.

---

## Related Documentation

- [../../../engine/camera/subs/sub_5293D0_CameraRender__SetFovAndClip.md](../../../engine/camera/subs/sub_5293D0_CameraRender__SetFovAndClip.md) -- Engine function details
- [../../../engine/camera/subs/sub_4E1680_SetAspectRatioMode.md](../../../engine/camera/subs/sub_4E1680_SetAspectRatioMode.md) -- Aspect ratio initialization
- [3D_DEBUG_OVERLAY.md](3D_DEBUG_OVERLAY.md) -- 3D overlay (uses same CameraSectorUpdate hook)
- [HOT_SWITCH.md](HOT_SWITCH.md) -- Hot-switch (uses same CameraSectorUpdate hook)
