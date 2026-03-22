# Game Settings System

## Overview

The Spiderwick Chronicles stores its configuration in the Windows Registry under `HKCU\Software\Sierra Entertainment\The Spiderwick Chronicles\`. Three subkeys hold all user-facing settings: `video`, `audio`, and `localization`. A separate `HKLM` key stores locale/distribution data.

The game's config system is implemented in `ClDevConfiguration_PC`:
- **Load**: `sub_4EDC70` — reads values from registry
- **Save**: `sub_4EDE30` — writes values to registry
- Config loaded via Kallis VM through `sub_4E0CA0` (virtual thunk referencing "Config.txt", unused in retail)

## Registry Layout

### HKCU Keys

**`HKCU\Software\Sierra Entertainment\The Spiderwick Chronicles\video`**

| Key | Type | Default | Notes |
|-----|------|---------|-------|
| width | REG_SZ | "800" | Screen width in pixels |
| height | REG_SZ | "600" | Screen height in pixels |
| refresh_rate | REG_SZ | "60" | Monitor refresh rate |
| texture_lod_bias | REG_SZ | "0" | LOD bias value |
| texture_scale_factor | REG_SZ | "1" | Texture quality multiplier |
| trilinear | REG_SZ | "1" | Trilinear filtering (0/1) |
| contrast | REG_SZ | "0.500000" | Display contrast |
| brightness | REG_SZ | "0.500000" | Display brightness |
| gamma | REG_SZ | "0.500000" | Display gamma |
| draw_distance | REG_SZ | "1.000000" | View distance multiplier |
| wide_screen | REG_SZ | "0" | Widescreen mode (0/1) |
| high_res | REG_SZ | "0" | High resolution mode (0/1) |

**`HKCU\Software\Sierra Entertainment\The Spiderwick Chronicles\audio`**

| Key | Type | Default | Notes |
|-----|------|---------|-------|
| master_volume | REG_SZ | "1.000000" | Master volume (0.0-1.0) |
| music_volume | REG_SZ | "1.000000" | Music volume |
| sfx_volume | REG_SZ | "1.000000" | Sound effects volume |
| speech_volume | REG_SZ | "1.000000" | Speech/dialogue volume |
| ambience_volume | REG_SZ | "1.000000" | Ambient sound volume |

**`HKCU\Software\Sierra Entertainment\The Spiderwick Chronicles\localization`**

| Key | Type | Default | Notes |
|-----|------|---------|-------|
| time_format | REG_SZ | "0" | Time display format |
| date_format | REG_SZ | "0" | Date display format |

### HKLM Key

**`HKLM\Software\Sierra Entertainment\The Spiderwick Chronicles\`**

| Key | Type | Notes |
|-----|------|-------|
| Locales | REG_SZ | "us;na;ww" — supported locales |
| AudioLanguage | REG_SZ | "us" — audio language code |
| FMVAudioTrack | REG_DWORD | 0 — FMV audio track index |

Read by `sub_489A00` (locale detection).

## Kallis VM Bug: ANSI Data in Unicode Registry

The game uses the **Unicode** (W-suffix) registry APIs:
- `RegCreateKeyExW`
- `RegQueryValueExW`
- `RegSetValueExW`

However, the Kallis VM passes **raw ANSI byte strings** to these Unicode APIs. The result is that values are stored as ANSI bytes misinterpreted as wide strings:

**Example**: The string "800" is stored as:
```
Actual bytes:   38 30 30 00        (ANSI "800\0", 4 bytes)
Should be:      38 00 30 00 30 00 00 00  (UTF-16 L"800\0", 8 bytes)
```

This means:
- Windows `regedit` displays garbled text (interprets ANSI as UTF-16)
- Standard registry tools (PowerShell `Get-ItemProperty`, etc.) read incorrect values
- Any tool writing to these keys must use the **same raw ANSI format** to match what the game expects
- The config key table lives at address `0x6EE9B8` (pointers to key name strings)

## Startup Behavior

1. Game starts, `sub_4EDC70` (ClDevConfiguration_PC::Load) reads all config from registry
2. ~2 seconds into startup, `sub_4EDE30` (ClDevConfiguration_PC::Save) **overwrites** registry with current in-memory values
3. This means any external registry edits must happen **before** the game launches, or be hooked to intercept the save

Key startup functions:
- `sub_4EF0B0` — D3D/DirectInput init, reads config, creates device
- `sub_4E1AB0` — Display mode init, reads width/height from `dword_E36E78`/`dword_E36E7C` into `g_ScreenWidth`/`g_ScreenHeight`
- `sub_4FEA10` — D3D device creation wrapper
- `sub_4F1830` — Startup info print (version, build date, display mode)
- `sub_50DFE0` — Safe mode dialog (DirectX warning skip)

## Resolution Change Behavior

The game does **NOT** use `IDirect3DDevice9::Reset` to change resolution at runtime. Instead:

1. User selects new resolution in game menu
2. Game writes new width/height to registry
3. Game **spawns a second instance of itself** (new process)
4. Original process exits

This means:
- D3D Reset is never called during normal gameplay for resolution changes
- The new process reads the updated registry values and creates a fresh D3D device
- Any mod hooks survive because the new process loads the same ASI/DLL

## Save Data Location

Save files are stored in: `Documents\Sierra\The Spiderwick Chronicles\`

## SpiderMod settings.ini Redirect

SpiderMod implements a registry redirect system (`registry_redirect.cpp`) that moves config storage from the Windows Registry to a portable `settings.ini` file. This uses a 3-phase approach:

1. **Phase 1 (DllMain)**: `PrePopulate()` reads `settings.ini` and writes values to registry in raw ANSI format (matching the Kallis VM bug)
2. **Phase 2 (ModThread)**: `InstallHooks()` hooks `RegCreateKeyExW`, `RegSetValueExW`, and `RegCloseKey` to intercept writes and save them back to `settings.ini`
3. **Phase 3 (DLL_PROCESS_DETACH)**: `SaveToINI()` dumps final registry state to `settings.ini` as a safety net

Registry hooks are installed in ModThread (not DllMain) to avoid loader lock issues with Windows 7 compatibility mode shims.
