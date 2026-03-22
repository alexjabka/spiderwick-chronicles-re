# Developer Configuration System (Config.txt)

**Status:** Discovered in shipped retail exe — works on ALL copies, no mods needed!
**Loader:** `ClDevConfiguration::LoadConfigTxt` at `0x4E0D70`
**Parser:** `ClDevConfiguration::LoadFile_Thunk` at `0x4E0CA0`
**Data store:** ClParser object at `0xD6CDD0` (accessed via `ReadGameData` at `0x4E0AE0`)

---

## Usage

Create `Config.txt` next to `Spiderwick.exe` with key-value pairs:

```
LEVEL GroundsD
SKIP_INTRO_MOVIES 1
```

**Works on retail CD copies — no mods, no patches, no hacks needed.**

---

## Known Keys

| Key | Values | Effect |
|-----|--------|--------|
| `LEVEL` | World name (MansionD, GroundsD, etc.) | Skip Shell menu, load directly into level |
| `RESPAWNID` | Integer (0 = default) | Spawn point within the level |
| `SKIP_INTRO_MOVIES` | 1 | Skip boot cinematics |
| `MODE` | `test` | Enter test mode (state 8, `ClGameModeTest`) |
| `VIEWER` | 1 | Enter viewer mode (state 7) |
| `VIEWER_WORLD` | World name | World to load in viewer mode |
| `LOAD_THROTTLE` | value | Control streaming load speed |

## Valid World Names

| Name | Description |
|------|-------------|
| `MansionD` | Spiderwick Mansion (chapters 1-3) |
| `GroundsD` | Mansion Grounds (chapters 4, 8) |
| `ThimbleT` | Thimbletack's Area (chapter 5) |
| `GoblCamp` | Goblin Camp (chapter 6) |
| `MnAttack` | Mansion Attack (chapter 7) |
| `Shell` | Main menu |

## Game State Machine

| State | Class | Purpose |
|-------|-------|---------|
| 1 | `ClGameModeBoot` | Intro movies (SKIP_INTRO_MOVIES check) |
| 2 | `ClGameModeInitialize` | Engine initialization |
| 3 | `ClGameModePlatformInit` | Platform-specific init |
| 4 | `ClGameModeProfileCheck` | Profile loading, **LEVEL check fork point** |
| 5 | `ClGameModePlay` | Shell menu (main menu) |
| 6 | `ClGameModeTransition` | Level loading/transition |
| 7 | Various viewers | Debug viewers (World, Noam, FMV) |
| 8 | `ClGameModeTest` | Test mode |

## How It Works

1. Engine loads `Config.txt` during early initialization via `ClDevConfiguration::LoadConfigTxt`
2. Key-value pairs stored in ClParser data store at `0xD6CDD0`
3. At state 4 (`ProfileCheck_Update` at `0x48AA10`):
   - Calls `ReadGameData("LEVEL")`
   - If non-empty → calls `LoadWorld(level, respawnId)` → sets state 6 (transition)
   - If empty → sets state 5 (Shell menu)
4. `GameMode_Select` at `0x4DDA00` checks `MODE` and `VIEWER` keys for alternate modes

## SpiderMod Integration

The ASI patches the filename string at `0x63992C` from `"Config.txt"` to `"Launch.txt"` in memory during `DLL_PROCESS_ATTACH` (before engine reads it). This way the dev config uses a clearer filename without conflicting with anything.

## Key Addresses

| Address | Function |
|---------|----------|
| `0x4E0D70` | `ClDevConfiguration::LoadConfigTxt` — loads Config.txt |
| `0x4E0CA0` | `ClDevConfiguration::LoadFile_Thunk` — parses key-value pairs |
| `0x4E0AE0` | `ReadGameData(key)` — looks up config value |
| `0x48AA10` | `ProfileCheck_Update` — LEVEL check fork point |
| `0x4DDA00` | `GameMode_Select` — MODE/VIEWER check |
| `0x4897F0` | `SetGameState(state)` — state machine transition |
| `0x63992C` | Hardcoded `"Config.txt"` string (10 chars + null) |
