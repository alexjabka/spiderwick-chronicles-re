# sub_538E60 — ResolveCharacterMode

## Identity

| Field | Value |
|---|---|
| Address | `0x538E60` |
| Calling Convention | `cdecl` |
| Module | engine/objects |

## Purpose

Resolves the `/Player/Character` game state to determine which character is currently active and what visibility (sight) mode they are using. The result is stored in `dword_7134DC` and consumed by gameplay and camera systems.

Logic:

1. If `dword_7134D8 == 3`: the game is in a state where character selection is active. Reads the `/Player/Character` game state value (integer index 1–3) and maps it to a character.
2. For each mapped character, reads `/Game/Characters/{name}/Sight` to determine the visibility mode (0–3).
3. Stores the resolved character pointer in `dword_7134DC`.
4. Default fallback (if `dword_7134D8 != 3` or index is out of range): `dword_7134DC = 2`.

## Character Index Mapping

| Index | Character | Lookup Table Entry |
|---|---|---|
| 1 | Jared | `off_63F4D0 + 0 * 64` |
| 2 | Simon | `off_63F4D0 + 1 * 64` |
| 3 | Mallory | `off_63F4D0 + 2 * 64` |

The lookup table `off_63F4D0` has a stride of 64 bytes per entry.

## Sight Mode Values

| Value | Meaning |
|---|---|
| 0 | Default / no special sight |
| 1 | Mode 1 |
| 2 | Mode 2 |
| 3 | Mode 3 (highest) |

## Key References

| Symbol | Role |
|---|---|
| `dword_7134D8` | State selector — must equal `3` for character resolution to run |
| `dword_7134DC` | Output — resolved character identifier / pointer |
| `off_63F4D0` | Character name/data lookup table (stride 64 bytes) |
| `/Player/Character` | Game state key — integer index 1–3 |
| `/Game/Characters/{name}/Sight` | Per-character sight mode game state key |

## Decompiled Pseudocode

```c
void ResolveCharacterMode(void)
{
    if (dword_7134D8 == 3)
    {
        int charIndex = ReadGameState("/Player/Character");  // returns 1, 2, or 3

        const char *charName = NULL;
        switch (charIndex)
        {
            case 1:  charName = *(const char **)(off_63F4D0 + 0 * 64);  break;  // Jared
            case 2:  charName = *(const char **)(off_63F4D0 + 1 * 64);  break;  // Simon
            case 3:  charName = *(const char **)(off_63F4D0 + 2 * 64);  break;  // Mallory
            default: charName = NULL; break;
        }

        if (charName)
        {
            char sightKey[128];
            snprintf(sightKey, sizeof(sightKey), "/Game/Characters/%s/Sight", charName);
            int sightMode = ReadGameState(sightKey);  // 0–3

            dword_7134DC = /* resolved character value based on charIndex / sightMode */;
            return;
        }
    }

    // Default fallback
    dword_7134DC = 2;
}
```

## Notes

- `dword_7134D8 == 3` is the gating condition; the significance of the value `3` in the broader state machine is not fully resolved but likely corresponds to an "in-game / character-active" state.
- The default fallback value of `2` corresponds to Simon in the character index mapping.
- Sight mode values from `/Game/Characters/{name}/Sight` affect how the character perceives the game world (e.g. Jared's Sight of the Second Kind); the exact effect on gameplay logic is resolved downstream.
