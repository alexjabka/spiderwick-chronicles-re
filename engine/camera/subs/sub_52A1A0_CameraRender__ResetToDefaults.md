# sub_52A1A0 -- CameraRender__ResetToDefaults
**Address:** 0x52A1A0 (Spiderwick.exe+12A1A0)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
int __thiscall CameraRender__ResetToDefaults(int this);
```

## Purpose

Resets the camera render object to default state: clears frustum corners, resets FOV to 45 degrees, and loads aspect ratio from global variables. Called by `PortalTraversal_Native` during recursive portal traversal to restore camera state after processing sub-portals.

## Behavior

1. `sub_5AB260(this + 32)` -- clear/reset frustum corners at +0x20
2. `*(WORD*)(this + 1742) = 0` -- clear status flags at +0x6CE
3. `sub_4E93E0(dword_6ED394, dword_6ED414, 0, 0, 2)` -- viewport reset
4. `CameraRender__RebuildMatrices(this)` -- sub_529790
5. `*(float*)(this + 12) = g_Deg2Rad * 45.0` -- FOV = 45 degrees in radians
6. `*(float*)(this + 16) = g_AspectRatioH` -- from global at 0xD6F314
7. `*(float*)(this + 20) = g_AspectRatioV` -- from global at 0xD6F31C
8. `CameraRender__InitDefaults(this)` -- additional default init

## Size

101 bytes. Single basic block.

## Callers

| Address | Function | Context |
|---------|----------|---------|
| 0x51A145 | PortalTraversal_Native | During recursive traversal (early reset) |
| 0x51A59F | PortalTraversal_Native | During recursive traversal (mid-processing) |
| 0x51A8FA | PortalTraversal_Native | During recursive traversal (post-processing) |
| 0x49DC65 | sub_49DC10 | Unknown |
| 0x4B082B | sub_4B0770 | Unknown |
| 0x4BE6F7 | sub_4BE6D0 | Unknown |
| 0x4C4B70 | sub_4C4A80 | Unknown |
| + 6 more | Various | Portal/rendering systems |

## Key Detail

This function reads `g_AspectRatioH` (0xD6F314) and `g_AspectRatioV` (0xD6F31C) directly, ensuring the camera always resets to the current display aspect ratio. The FOV default of 45 degrees matches `CameraSettings__InitDefaults`.

## Related

- [../CAMERA_RENDERING.md](../CAMERA_RENDERING.md) -- Pipeline overview
- [sub_5293D0_CameraRender__SetFovAndClip.md](sub_5293D0_CameraRender__SetFovAndClip.md) -- Rebuild entry
- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) -- Main caller
