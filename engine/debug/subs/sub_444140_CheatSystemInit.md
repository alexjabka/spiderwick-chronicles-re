# sub_444140 — CheatSystemInit

## Identity

| Field | Value |
|---|---|
| Address | `0x444140` |
| Calling Convention | `cdecl` |
| Module | engine/debug |

## Purpose

One-shot initialization routine for the cheat system. Performs three tasks:

1. Registers the cheat system object with the subsystem table via `sub_443D70`.
2. Zeros the per-player cheat flag array at `dword_6E1494` for as many player slots as the game reports.
3. Reads two game-data variables to apply startup overrides:
   - `"draw_game_state_info"` — if truthy, immediately enables the debug overlay (flag bit 4, player 1).
   - `"display_hud"` — if absent or falsy, immediately hides the HUD (flag bit 2, player 1).

## Key References

| Symbol | Role |
|---|---|
| `dword_6E1494` | Per-player cheat flag array (zeroed here) |
| `byte_6E1498` | Additional cheat byte, cleared to 0 on init |
| `sub_443D70` | Registers the cheat system object with `off_6E1490` |
| `sub_53A010` | Returns total player count (loop bound) |
| `sub_4E0AE0` | `ReadGameData(key)` — reads a named game data variable |
| `sub_443B10` | `kallis` — `(playerIndex, flagBit, value)` — sets a cheat flag |

## Startup Flag Effects

| Game Data Key | Condition | Effect |
|---|---|---|
| `"draw_game_state_info"` | value is non-zero | `sub_443B10(1, 4, 1)` — enable debug overlay |
| `"display_hud"` | value is zero or missing | `sub_443B10(1, 2, 1)` — hide HUD |

## Decompiled Pseudocode

```c
void CheatSystemInit(void)
{
    int i;
    int *result;

    // Register cheat system object
    sub_443D70(&off_6E1490);

    // Zero per-player cheat flags
    for (i = 0; i < sub_53A010(); i++)
        *(DWORD *)(dword_6E1494 + 4 * i) = 0;

    byte_6E1498 = 0;

    // Apply game-data startup overrides
    result = sub_4E0AE0("draw_game_state_info");
    if (result && *result)
        sub_443B10(1, 4, 1);   // enable debug overlay (kallis)

    result = sub_4E0AE0("display_hud");
    if (!result || !*result)
        sub_443B10(1, 2, 1);   // hide HUD (kallis)
}
```

## Notes

- Player index `1` is passed to `sub_443B10` for both startup overrides, meaning these are applied to player slot 1 specifically (1-based indexing assumed).
- `byte_6E1498` purpose is not fully resolved but is part of the cheat state block adjacent to `dword_6E1494`.
- `sub_443D70` sets up the object pointer at `off_6E1490`, which is used by other cheat functions to reach the system instance.
