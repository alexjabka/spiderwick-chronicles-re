# Cheat / Debug Flag System

## Overview

The game has a built-in cheat flag system managed through `ClCheatManager`.
Flags are stored per-player at a global table. Some flags have gameplay effects
(invulnerability, heal), others control rendering (HUD hide, debug overlay).

## Flag Storage

```
dword_6E1494 → pointer to flag table
[dword_6E1494 + playerIndex*4] = flags bitmask (DWORD)
```

Player index is typically 0 for single-player.

## Known Flags

| Bit | Hex | Name | Effect | Status |
|-----|-----|------|--------|--------|
| 1 | 0x02 | Invulnerability | Player can't take damage | **Works** |
| 2 | 0x04 | HUD Hide | Hides all HUD elements | **Works** |
| 4 | 0x10 | draw_game_state_info | Debug info overlay | **STRIPPED from release** |
| 5 | 0x20 | Combat Cheat | Combat-related cheat | Untested |
| 6 | 0x40 | Heal | Heals player | Untested |
| 11 | 0x800 | Infinite Ammo | Unlimited ammo | Untested |
| 13 | 0x2000 | Field Guide | Unlocks field guide | Untested |

## How to Toggle (CE Lua)

```lua
local flagsPtr = readInteger(0x6E1494)
local flags = readInteger(flagsPtr)
-- Toggle invulnerability (bit 1 = 0x02)
writeInteger(flagsPtr, flags ~ 0x02)  -- XOR toggle
```

## Input Actions

Cheats are normally triggered through input actions:

| Input Action | Flag |
|-------------|------|
| `CHEAT_INVULNERABILITY` | bit 1 |
| `CHEAT_HEAL` | bit 6 |
| `CHEAT_COMBAT` | bit 5 |
| `CHEAT_AMMO` | bit 11 |
| `CHEAT_FIELD_GUIDE` | bit 13 |
| `CHEAT_SPRITE_A` | bit 8 (requires byte_6E1470) |
| `CHEAT_SPRITE_B` | bit 9 (requires byte_6E1470) |
| `CHEAT_SPRITE_C` | bit 10 (requires byte_6E1470) |

These inputs are NOT bound to keys in release build. Cheats can only
be activated through direct memory writes.

## Guard System

`CHEAT_GUARD_A` and `CHEAT_GUARD_B` are guard inputs — likely a button
combination (like Konami code) that must be held to enable the cheat widget.

## Key Functions

| Address | Name | Description |
|---------|------|-------------|
| sub_443160 | CheckCheatFlag | `(group, bit)` — returns `(1 << bit) & flags[group]` |
| sub_443EC0 | CheatInputHandler | Processes cheat input actions |
| sub_443B10 | SetCheatFlag | `(playerIdx, flagBit, value)` — through kallis |
| sub_444140 | CheatSystemInit | Reads `draw_game_state_info` and `display_hud` from game data |
| sub_4E0AE0 | ReadGameData | Reads game data variable by name |

## Game Data Variables

| Name | Description |
|------|-------------|
| `ENABLE_DEV_CHEATS` | Enables weapon cheat (GiveWeaponsCheat) |
| `draw_game_state_info` | Enables debug overlay (STRIPPED) |
| `display_hud` | Controls HUD visibility |

## Widget System

Widgets are created via `sub_418290(ecx=parentObj, widgetName)`.

| Widget Name | Address | Parent Object |
|------------|---------|---------------|
| "CheatWidget" | 0x632808 | dword_D437DC |
| "cheatwidget" | 0x6288B0 | unk_D43FD4 |

Both exist in the game — "CheatWidget" (capital C) is for the cheat code
input UI, "cheatwidget" (lowercase) is for the cheat state display.
