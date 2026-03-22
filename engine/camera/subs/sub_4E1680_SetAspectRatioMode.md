# sub_4E1680 -- SetAspectRatioMode
**Address:** 0x4E1680 (Spiderwick.exe+E1680)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
int __cdecl SetAspectRatioMode(int mode);
```

## Purpose

Sets the global aspect ratio values (`g_AspectRatioH` at 0xD6F314, `g_AspectRatioV` at 0xD6F31C) based on a mode enum. These globals are read by `CameraRender__ResetToDefaults` and `CameraSystem__UpdateRender` to configure the projection matrix.

## Mode Table

| Mode | g_AspectRatioH | g_AspectRatioV | Description |
|------|---------------|---------------|-------------|
| 0 | 1.3333334 | 1.0 | 4:3 standard |
| 1 | 1.3333334 | 1.0 | 4:3 standard (alias) |
| 2 | 1.7777778 | 1.0 | 16:9 (computed from display) |
| 3 | 1.7777778 | 1.0 | 16:9 |
| 4 | 1.3333334 | 1.3333334 | 4:3 with V scaling |

## Behavior

1. Switch on mode:
   - Mode 0/1: Set H=1.333, V=1.0, clear `dword_D6F930`
   - Mode 2: Compute from display metrics (`flt_6ED944 * flt_6ED964`), then fall through to mode 3
   - Mode 3: Set H=1.778, V=1.0
   - Mode 4: Set H=1.333, V=1.333, clear `dword_D6F930`

2. If mode changed (different from `dword_D6F928`):
   - Save new mode to `dword_D6F928`
   - Walk observer linked list at `dword_D6FF88`
   - Call `vtable[0](this)` on each observer to notify of aspect ratio change

## Size

145 bytes. 13 basic blocks, cyclomatic complexity 8 (switch statement).

## Callers

| Address | Function |
|---------|----------|
| 0x4EEF54 | sub_4EEE70 (video settings handler) |

## Key Globals

| Address | Name | Purpose |
|---------|------|---------|
| 0xD6F314 | g_AspectRatioH | Horizontal aspect ratio |
| 0xD6F31C | g_AspectRatioV | Vertical aspect ratio |
| 0xD6F928 | (unnamed) | Current mode (cached for change detection) |
| 0xD6F930 | (unnamed) | Display mode adjustment value |
| 0xD6FF88 | (unnamed) | Observer linked list head |

## Notes

- Mode 2 is the only mode that does a runtime computation from display metrics
- The observer notification pattern allows other systems (e.g., UI, HUD) to react to aspect ratio changes
- Mode 4 (V scaling) is unusual -- may be for anamorphic displays

## Related

- [../CAMERA_RENDERING.md](../CAMERA_RENDERING.md) -- Where aspect ratio is consumed
- [sub_52A1A0_CameraRender__ResetToDefaults.md](sub_52A1A0_CameraRender__ResetToDefaults.md) -- Reads g_AspectRatioH/V
