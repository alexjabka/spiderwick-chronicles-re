# Config.txt (Launch.txt) Developer Configuration Keys

Complete reference for all keys read by the game's developer configuration system.

## System Overview

- **File**: `Config.txt` (or `Launch.txt` -- same format, game loads whichever exists)
- **Format**: One key-value pair per line, space-separated: `KEY value`
- **Loader**: `ClDevConfiguration::LoadConfigTxt` at `0x4E0D70` -> `ClDevConfiguration__LoadFile_Thunk` at `0x4E0CA0`
- **Storage**: `ClParser` object at `0xD6CDD0`
- **Reader**: `ReadGameData` at `0x4E0AE0` -- looks up key in parser, returns pointer to value or NULL
- **Type checker**: `sub_4E0AC0` -- returns value type (e.g., 3 = array of 3 ints)

## All 50 Config Keys

### Game Mode / Level Loading

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `MODE` | string | `0x4DDA05` | `GameMode_Select` | Game mode selection. `test` = test mode (returns state 8). If unset, falls through to `VIEWER` check | (none) |
| `VIEWER` | string | `0x4DDA57` | `GameMode_Select` | Viewer mode name. Must match one of: `audio`, `bard`, `fmv`, `font`, `image`, `input`, `noam`, `world`. If matched, enters viewer mode (state 7) instead of normal gameplay (state 5) | (none) |
| `LEVEL` | string | `0x48A811` | `sub_48A800` | Level/world file to load directly (bypasses shell/menus). Checked in save data setup and level transition. E.g., `GroundsD` | (none) |
| `RESPAWNID` | int | `0x48AA6A` | `sub_48AA10` | Respawn point ID within the level. Used when `LEVEL` is set to specify spawn location | 0 |
| `BARD` | string | `0x48D133` | `sub_48D0F0` | Bard script file for bard viewer mode. If not set, prints error `{rERROR}: No Bard File specified` | (none) |
| `DIFFICULTY` | int | `0x50532F` | `sub_5051D0` | Initial difficulty level. Clamped to range 1-10; default 6 if unset | 6 |

### Viewer Mode Configuration

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `VIEWER_WORLD` | string | `0x49078D` | `WorldViewer_Init` | World file for world viewer. Loads `Worlds\<value>.wld` | (none, required for world viewer) |
| `VIEWER_NOAM` | string | `0x48F9AE` | `sub_48F990` | NOAM (animated model) file for noam viewer. Prepends `actors\` | (none, required for noam viewer) |
| `VIEWER_NOAM_ANIMATION` | string | `0x48FA0B` | `sub_48F990` | Animation file for noam viewer. Prepends `actors\`. Falls back to `VIEWER_NOAM` value if unset | VIEWER_NOAM value |
| `VIEWER_FMV` | string | `0x48D218` | `sub_48D210` | FMV video file for fmv viewer. If not set, prints info message | (none) |
| `VIEWER_PLAYLIST` | string | `0x48C9A1` | `sub_48C900` | Audio playlist file for audio viewer. Required, prints error if missing | (none, required for audio viewer) |
| `VIEWER_TIME` | int (ms) | `0x48C906` | `sub_48C900` | Playback time in milliseconds for audio viewer. Stored as float seconds (value * 0.001) | 0 |
| `VIEWER_FIRST` | int | `0x48C947` | `sub_48C900` | First track index for audio viewer playlist | 0 |
| `VIEWER_NEXT` | int | `0x48C958` | `sub_48C900` | Next track index for audio viewer playlist | 0 |

### Camera System

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `INVERT_CAMERA_ROTATION_X` | int (bool) | `0x43A6F1` | `ClCamComponent_CharacterOffsetCam` ctor | Inverts camera X rotation axis. Read at camera component init and update | 0 (not inverted) |
| `INVERT_CAMERA_ROTATION_Y` | int (bool) | `0x43D89E` | `sub_43D860` | Inverts camera Y rotation axis | 0 (not inverted) |
| `CAMERA_TILT_MODE` | string | `0x43DE11` | `sub_43DD70` | Camera tilt behavior. Hash-compared against `"Moving"` -- if matching, enables tilt-while-moving mode | (none) |

### Rendering / Display

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `CAP_30FPS` | int (bool) | `0x48CC85` | `RenderPipeline2` | Caps frame rate at 30 FPS (delta time clamped to 0.0333s). Also read at `0x4883DF` during init | 1 (capped) |
| `CLEAR_COLOR` | int[3] (RGB) | `0x48834E` | `sub_488330` | Background clear color as RGB triplet (type must be array/3). Used in main render init and world viewer | (default clear) |
| `SOFT_FOCUS` | int (bool) | `0x56E475` | `sub_56E420` | Enables/disables soft focus post-processing effect. If set to 0, disables the effect | 1 (enabled) |

### Localization / Audio

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `LOCALES` | string | `0x489A2C` | `Locale_DetectFromRegistry` | Locale search path (semicolon-separated). Sets asset repository search path | `us;na;ww` (from registry) |
| `AUDIO_LOCALE` | string | `0x489A44` | `Locale_DetectFromRegistry` | Audio language locale code. E.g., `us`, `uk`, `fr` | `us` (from registry) |
| `FMV_AUDIO_TRACK` | int | `0x489A5C` | `Locale_DetectFromRegistry` | FMV audio track index for localized movies | 0 (from registry) |
| `AUDIO_PRINT_BANK_DB` | int (bool) | `0x49628C` | `sub_496270` | Enables audio bank debug printing (VM-accessible) | 0 |
| `AUDIO_PRINT_LEVEL` | int | `0x1CB222A` | Kallis VM (audio) | Audio system print verbosity level. Stored to `dword_1374E40` | 0 |
| `AUDIO_VERBOSE_PRINT_LEVEL` | int (bool) | `0x1CB228B` | Kallis VM (audio) | Enables verbose audio print level | 0 |

### Cheats / Debug

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `ENABLE_DEV_CHEATS` | int (bool) | `0x46322B` | `sub_4631A0` | Enables developer cheat system. When active, `GiveWeaponsCheat` VM call becomes available | 0 |
| `draw_game_state_info` | int (bool) | `0x444184` | `CheatSystemInit` | Shows game state debug info overlay (cheat flag 4) | 0 |
| `display_hud` | int (bool) | `0x4441A9` | `CheatSystemInit` | Controls HUD visibility. If set to 0, hides HUD (cheat flag 2) | 1 (shown) |
| `DRAW_WORLD_HUD` | int (bool) | `0x5994E2` | `SectorHUD_Init` | Toggles world/sector HUD debug display | 0 |
| `COMBAT_TRACE` | int (bool) | `0x408B0D` | `sub_408AA0` | Enables combat system trace/debug logging. Also read in Kallis VM at `0x1CA2939` | 0 |
| `STATE_TRACE` | int (bool) | `0x40C705` | state machine code | Enables state machine transition trace logging. Read at multiple state transition points | 0 |
| `UNLOCK_MULTIPLAYER` | int (bool) | `0x442C10` | (inline) | Unlocks multiplayer mode from the start | 0 |

### Intro / Cinematics

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `SKIP_INTRO_MOVIES` | int (bool) | `0x4898C8` | `sub_4898C0` | Skips all intro/splash movies at startup | 0 |
| `ALLOW_SKIP_ANY_IGC` | int (bool) | `0x445EE4` | (inline), `0x4E7A7A` | Allows skipping any in-game cinematic/cutscene, not just skippable ones | 0 |
| `CINE_VIDCAP` | int (bool) | `0x445AC3` | `sub_445AB0` | Enables video capture mode during cinematics | 0 |

### Input / UI

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `input_share_mouse` | int (bool) | `0x4EFF7E` | `sub_4EFF40` | Shares mouse input between game and OS (non-exclusive mode) | 0 |
| `INPUT_DISABLE_DIRECTX` | int (bool) | `0x5020AD` | `sub_502090` | Disables DirectInput, uses alternative input method | 0 |
| `UI_SCREENS_USE_TRUE_4x3_ASPECT_RATIO` | int (bool) | `0x4CCF6E` | `sub_4CCF30` | Forces true 4:3 aspect ratio for UI screens instead of stretched | 0 |
| `USER` | string | `0x499F78` | `sub_499F70` | User type identifier. If set to `artist`, enables artist-specific mode (VM function returns 1) | (none) |

### Video Capture / Debug Tools

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `VID_CAPTURE` | int (bool) | `0x508CAF` | obfuscated block | Enables video frame capture system. Sets `byte_D702CD` | 0 |
| `VID_NAME` | string | `0x508CD8` | obfuscated block | Output filename for video capture. Stored to `dword_D702C8` | (empty) |
| `VID_RENDER` | int | `0x508D28` | obfuscated block | Video render mode/quality setting. Stored to `dword_6EDAA8` | 0 |

### Engine / System

| Key | Type | Read At | Function | What It Controls | Default |
|-----|------|---------|----------|-----------------|---------|
| `LOAD_THROTTLE` | float | `0x4E6DCF` | `sub_4E6CE0` (engine init) | Asset loading throttle rate (seconds). Controls how aggressively assets are loaded per frame | 0.0 |
| `abort_after_load` | int (bool) | `0x489794` | (inline) | Immediately exits the process after level loading completes (for automated testing). Calls `_exit(0)` | 0 |
| `STOREFILE` | string | `0x1C99D82` | Kallis VM (file system) | Save/store file path override | (none) |
| `play_timer` | int (bool) | `0x5C44DA` | obfuscated block | Enables play time tracking/timer | 0 |
| `timeline` | int (bool) | `0x1C94480` | Kallis VM | Enables timeline recording/playback system | 0 |
| `file_led` | int (bool) | `0x1C94513` | Kallis VM | Enables file I/O activity LED indicator | 0 |
| `file_telemetry` | int (bool) | `0x1C9455A` | Kallis VM | Enables file system telemetry/performance logging | 0 |

## Example Config.txt

```
LEVEL GroundsD
SKIP_INTRO_MOVIES 1
ENABLE_DEV_CHEATS 1
display_hud 1
draw_game_state_info 0
CAP_30FPS 0
DIFFICULTY 6
```

## Viewer Mode Examples

```
MODE test
```

```
VIEWER world
VIEWER_WORLD GroundsD
CLEAR_COLOR 128 128 255
```

```
VIEWER noam
VIEWER_NOAM boggart_warrior
VIEWER_NOAM_ANIMATION boggart_warrior_idle
```

```
VIEWER audio
VIEWER_PLAYLIST music_level1
VIEWER_TIME 5000
VIEWER_FIRST 0
VIEWER_NEXT 1
```

```
VIEWER fmv
VIEWER_FMV intro_cinematic
```

```
VIEWER bard
BARD test_script
```

## Valid VIEWER Mode Names

Registered at `0x48BE20` via `ClTemplateSpawner` pattern:

| Mode | Class | Description |
|------|-------|-------------|
| `audio` | `ClGameModeAudioViewer` | Audio playlist playback viewer |
| `bard` | `ClGameModeBardViewer` | Bard script viewer |
| `fmv` | `ClGameModeFmvViewer` | Full-motion video viewer |
| `font` | `ClGameModeFontViewer` | Font/glyph atlas viewer |
| `image` | `ClGameModeImageViewer` | Image/texture viewer |
| `input` | `ClGameModeInputViewer` | Input device/mapping viewer |
| `noam` | `ClGameModeNoamViewer` | 3D animated model viewer |
| `world` | `ClGameModeWorldViewer` | Full world/level viewer |

## Notes

- Keys are **case-sensitive** in the parser. Most engine keys are UPPERCASE, some debug keys are lowercase.
- String values are raw (no quotes needed).
- Integer booleans: 0 = false, non-zero = true.
- `CLEAR_COLOR` is special: it stores 3 integers (RGB) and the engine checks type == 3 before reading.
- `LEVEL` is the most useful key for development -- it directly loads a world file, bypassing all menus.
- `MODE test` enters test mode (game state 8).
- Several keys (VID_*, play_timer, file_*, timeline, STOREFILE, AUDIO_*_LEVEL) are read from Kallis VM obfuscated code.
- `PLAYER_SKIP_IGC` at `0x62F51C` is an **input widget name** (used by `CreateWidget`), not a config key.
