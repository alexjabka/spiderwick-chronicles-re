# Engine Debug Systems -- Overview

High-level documentation of the debug and development systems found in The Spiderwick Chronicles engine binary. These systems were used during development and remain in the retail executable, though most are disconnected from normal game flow.

---

## Table of Contents

1. [Cheat System](#cheat-system)
2. [Engine Console](#engine-console)
3. [Sector Debug HUD](#sector-debug-hud)
4. [Render Globals (Fog / Snow / Rim Light)](#render-globals)
5. [Debug Camera](#debug-camera)
6. [Debug Lights](#debug-lights)
7. [Static Camera Pipeline](#static-camera-pipeline)
8. [Game Modes](#game-modes)

---

## Cheat System

A per-player bitmask-based cheat flag system with input-driven toggling.

| Component | Address | Doc |
|-----------|---------|-----|
| CheckCheatFlag | `0x443160` | [sub_443160_CheckCheatFlag.md](sub_443160_CheckCheatFlag.md) |
| CheatInputHandler | `0x443EC0` | [sub_443EC0_CheatInputHandler.md](sub_443EC0_CheatInputHandler.md) |
| CheatSystemInit | `0x444140` | [sub_444140_CheatSystemInit.md](sub_444140_CheatSystemInit.md) |

**Architecture:** Cheat flags are stored as a bitmask per player in an array at `dword_6E1494`. Each bit controls a specific cheat (invulnerability, hide HUD, combat, heal, ammo, field guide). The input handler hashes cheat name strings and checks input actions each frame. Sprite debug cheats are gated behind a dev mode flag (`byte_6E1470`). Initial flags can be set from game data config (`draw_game_state_info`, `display_hud`).

---

## Engine Console

A ring-buffer debug printf system with color code support.

| Component | Address | Doc |
|-----------|---------|-----|
| EngineConsole | `0x4E02D0` | [sub_4E02D0_EngineConsole.md](sub_4E02D0_EngineConsole.md) |

**Architecture:** 256-line ring buffer at `byte_D5CB98` (256 chars per line). Write head at `dword_D6CDA4`, display start at `dword_D6CD9C`. Supports inline color codes: `{g}`=green, `{r}`=red, `{y}`=yellow, `{v}`=violet, `{o}`=orange. Called by world loader, script system, and many other subsystems. Color codes are stored raw and parsed at render time.

---

## Sector Debug HUD

Native debug overlay showing streaming system diagnostics.

| Component | Address | Doc |
|-----------|---------|-----|
| DebugDrawSectorInfo | `0x599760` | [sub_599760_DebugDrawSectorInfo.md](sub_599760_DebugDrawSectorInfo.md) |

**Architecture:** Gated by `sub_516940()==1` AND `dword_1345C48!=0`. Displays streaming pool memory bar (via `sub_5996C0`), sector states (Drawing/Loaded/Loading/Unloading/Throttling), sector names, load times, and memory usage. Text rendered via Kallis thunk `sub_599360` (requires font system init). The progress bar works standalone. Called from thunk at `0x555F00` but **never registered in the retail render loop** -- must be manually patched in.

---

## Render Globals

Engine-wide rendering toggle globals for fog, snow, and rim lighting.

| Component | Address | Doc |
|-----------|---------|-----|
| Render Toggle Globals | Various | [sub_596500_RenderGlobals.md](sub_596500_RenderGlobals.md) |

### Rim Light
- Enable: `byte_728BA0` (via `sub_596500`)
- Modulation: `byte_728BA1` (via `sub_596510`)
- Z/X Offsets: `flt_728BA4` / `flt_1345648`
- Color RGB: `dword_728BA8` / `728BAC` / `728BB0` (via `sub_596540`)

### Fog (SOFTWARE -- not D3D hardware fog)
- Enable: `byte_E794F9` (via `sub_551090`)
- Color RGB: `dword_E794D8` / `E794AC` / `E79498`
- Start distance: `flt_E794A8` (via `sub_551060`)
- Density: `flt_E794A0` (via `sub_5510A0`)
- Type: `dword_E794B0` (0=linear, 1=exp, 2=exp2, 3=custom)
- LUT built by `sub_550BB0` -- must be called after changing fog params.

### Snow
- Enable: `byte_726F78` (via `sub_563EA0`)

---

## Debug Camera

Free-flying debug camera used in VIEWER mode.

| Component | Address | Doc |
|-----------|---------|-----|
| StaticCamera__Update | `0x48FE90` | [sub_48FE90_StaticCameraUpdate.md](sub_48FE90_StaticCameraUpdate.md) |

**Architecture:** Reads pitch/yaw from `flt_D417F4` / `flt_D41908`. Movement computed via orientation vectors at `this+16..36` (forward/right/up). Activation via `byte_D417F3` triggers `StaticCamera__Init`. Input processed by `sub_48F460` (read) and `sub_48F500` (apply). Slow mode via `byte_D417F2`.

---

## Debug Lights

Debug light controls active in VIEWER mode when `byte_D41906` is set.

| Component | Address | Doc |
|-----------|---------|-----|
| StaticCameraRender (light section) | `0x490270` | [sub_490270_StaticCameraRender.md](sub_490270_StaticCameraRender.md) |

**Debug Input Flags:**

| Address | Flag | Action |
|---------|------|--------|
| `byte_D417F0` | DEBUG_LIGHT_REMOVE | Remove light |
| `byte_D417F1` | DEBUG_DIRECTION | Change light direction |
| `byte_D417F2` | DEBUG_TELEPORT_SLOWMODE | Teleport / slow mode |
| `byte_D417F3` | DEBUG_CAMERA_ACTIVATE | Reset camera |
| `byte_D41905` | DEBUG_LIGHT_ADD | Add light |
| `byte_D41906` | DEBUG_LIGHT_MODIFY | Enable light adjustment mode |

**Light Globals:**
- `flt_6E55A0` -- ambient light intensity
- `flt_6E55A4` -- sun light intensity
- Sun direction vector rotatable via input

---

## Static Camera Pipeline

The complete render pipeline for VIEWER mode (game mode 7).

| Component | Address | Doc |
|-----------|---------|-----|
| StaticCameraRender | `0x490270` | [sub_490270_StaticCameraRender.md](sub_490270_StaticCameraRender.md) |
| StaticCamera__Update | `0x48FE90` | [sub_48FE90_StaticCameraUpdate.md](sub_48FE90_StaticCameraUpdate.md) |

**Flow:** Sets ambient to `(0x20, 0x20, 0x20)` -> if `DEBUG_LIGHT_MODIFY`: adjusts ambient/sun intensity and sun direction, renders debug text -> calls `StaticCamera__Update` with `g_StaticRenderCamera` (`0xD41928`).

---

## Game Modes

Mode selection determines which render pipeline and camera system is active.

| Component | Address | Doc |
|-----------|---------|-----|
| GameModeSelect | `0x4DDA00` | [sub_4DDA00_GameModeSelect.md](sub_4DDA00_GameModeSelect.md) |

| Mode | ID | Pipeline | Camera | Access |
|------|----|----------|--------|--------|
| Normal Gameplay | 5 | Standard renderer | Gameplay cameras | Default |
| Viewer | 7 | StaticCameraRender (`0x490270`) | Free debug camera (`0x48FE90`) | Config: `VIEWER` key |
| Test | 8 | Unknown | Unknown | Config: `MODE = "test"` |

Mode is selected at startup by reading `"MODE"` and `"VIEWER"` keys from game data configuration. Viewer mode enables the full debug camera and lighting pipeline.

---

## Key Data Regions

| Address Range | Contents |
|---------------|----------|
| `0x6E1470 - 0x6E1494` | Cheat system (dev flag, struct, flags array ptr) |
| `0x6E55A0 - 0x6E55A4` | Light intensity globals (ambient, sun) |
| `0x726F78` | Snow enable |
| `0x728BA0 - 0x728BB0` | Rim light globals |
| `0xD417F0 - 0xD41928+` | Debug camera state, input flags, camera object |
| `0xD5CB98 - 0xD6CDA4` | Engine console ring buffer and indices |
| `0xE794A0 - 0xE794F9` | Fog system globals |
| `0x1345C48` | Sector debug HUD enable |
