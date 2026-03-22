# sub_4353C0 -- CameraSettings__InitDefaults
**Address:** 0x4353C0 (Spiderwick.exe+353C0)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
int __thiscall CameraSettings__InitDefaults(int this);
```

## Purpose

Initializes a camera settings work buffer with default values. Called at the start of `CameraSystem__UpdateRender` to set up the per-frame buffer before camera components modify it.

## Default Values

| Offset | Type | Value | Field |
|--------|------|-------|-------|
| +0x80 | float | 0.0 | Unknown |
| +0x84 | float | **45.0** | FOV (degrees) |
| +0x88 | float | **2.0** | Near clip distance |
| +0x8C | float | **1024.0** | Far clip distance |
| +0x90 | float | 0.0 | Unknown |
| +0x94 | float | 0.0 | Unknown |
| +0x98 | float | 0.0 | Unknown |
| +0xA0 | byte | 0 | Flag |
| +0xA1 | byte | 0 | Flag |

Also calls `sub_5A8420` twice to initialize 4x4 identity matrices at +0x00 and +0x40.

## Size

96 bytes. Single basic block, cyclomatic complexity 1.

## Callers

| Address | Function | Context |
|---------|----------|---------|
| 0x4361A0 | CameraSystem__UpdateRender | Per-frame init |
| 0x4363A8 | sub_436390 | Camera init |
| 0x43579D | sub_435E60 | Camera setup |
| 0x435CC1 | (inline) | Camera setup |
| 0x435EC1 | sub_435E60 | Camera setup |
| 0x435FAA | sub_435F36 | Camera setup |
| 0x447601 | sub_4475B0 | Unknown |
| 0x4E3B42 | (multiple) | Init contexts |

## Notes

- The FOV default (45.0 degrees) is the engine's base FOV before camera scripts or data files override it
- Camera components can modify the work buffer during the component loop in `CameraSystem__UpdateRender`
- Camera settings loaded from data files (via ClCameraDB) can override FOV and far clip -- see [../structs/camera_settings.md](../structs/camera_settings.md)

## Related

- [sub_436190_CameraPipeline.md](sub_436190_CameraPipeline.md) -- CameraSystem__UpdateRender (main caller)
- [../CAMERA_RENDERING.md](../CAMERA_RENDERING.md) -- Pipeline overview
- [../structs/camera_settings.md](../structs/camera_settings.md) -- Full camera settings structure
