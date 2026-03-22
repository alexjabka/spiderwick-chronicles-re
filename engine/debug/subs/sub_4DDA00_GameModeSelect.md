# sub_4DDA00 -- GameModeSelect

## Address
`0x004DDA00`

## Signature
```c
int __stdcall sub_4DDA00(int a1);
```

## Purpose
Selects the game mode based on configuration data. Reads mode strings from game data and returns a numeric mode ID that determines which systems and render pipelines are active.

## Mode Selection Logic

| Data Key | Value | Return | Mode Name | Description |
|----------|-------|--------|-----------|-------------|
| `"MODE"` | `"test"` | 8 | Test Mode | Testing/QA mode |
| `"VIEWER"` | (present) | 7 | Viewer Mode | Static camera render pipeline, debug lights, free camera |
| (default) | -- | 5 | Normal Gameplay | Standard game mode with player control |

## Logic Flow
1. Reads the `"MODE"` key from game data.
2. If value is `"test"`, returns **8** (test mode).
3. Reads the `"VIEWER"` key from game data.
4. If present/truthy, returns **7** (viewer mode).
5. Otherwise, returns **5** (normal gameplay).

## Game Modes Summary

| Mode ID | Name | Render Pipeline | Camera |
|---------|------|----------------|--------|
| 5 | Normal | Standard gameplay renderer | Gameplay cameras (follow, scripted, etc.) |
| 7 | Viewer | `sub_490270` (StaticCameraRender) | Static/debug free camera (`sub_48FE90`) |
| 8 | Test | Unknown (likely subset of normal) | Unknown |

## Callers
- Engine initialization -- called once during startup to determine which mode to boot into.

## Callees
- Game data read functions -- reads `"MODE"` and `"VIEWER"` string keys from configuration.

## Notes
- Viewer mode (7) activates the static camera render pipeline (`sub_490270`), which includes debug light controls and the free-flying camera (`sub_48FE90`). This is a developer tool for scene inspection.
- The mode selection is data-driven, meaning it can be changed by modifying game configuration files without recompiling.
- Mode 5 is the standard retail gameplay mode. Modes 7 and 8 are developer/QA modes not accessible through normal game UI.
- The `__stdcall` convention means parameters are passed on the stack and the callee cleans up.
