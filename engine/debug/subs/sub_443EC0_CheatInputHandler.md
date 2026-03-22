# sub_443EC0 — CheatInputHandler

## Identity

| Field | Value |
|---|---|
| Address | `0x443EC0` |
| Calling Convention | `thiscall` (ECX = cheat system object) |
| Module | engine/debug |

## Purpose

Processes cheat input actions for all defined cheat codes. Called each frame (or on input event) to check whether any cheat key sequence has been completed. For each cheat, it lazily computes the input hash via `sub_405380` and tests it against the current input state, then toggles the corresponding flag bit via `sub_443B10`.

The CHEAT_SPRITE_A/B/C cheats additionally gate on `byte_6E1470` being set before they can activate.

## Key References

| Symbol | Role |
|---|---|
| `dword_6E1494` | Cheat flag table — one `DWORD` per player, indexed as `dword_6E1494 + 4 * playerIndex` |
| `byte_6E1470` | Guard flag — must be non-zero for CHEAT_SPRITE variants to fire |
| `sub_405380` | Hash / input-name lookup (lazy, result is cached per cheat slot) |
| `sub_443B10` | `kallis` — `(playerIndex, flagBit, toggle)` — sets or clears a cheat flag bit |

## Cheat Action Table

| Cheat Name | Flag Bit | Guard |
|---|---|---|
| CHEAT_INVULNERABILITY | bit 0 | none |
| CHEAT_HEAL | bit 1 | none |
| CHEAT_COMBAT | bit 2 | none |
| CHEAT_AMMO | bit 3 | none |
| CHEAT_FIELD_GUIDE | bit 4 | none |
| CHEAT_SPRITE_A | bit 5 | `byte_6E1470 != 0` |
| CHEAT_SPRITE_B | bit 6 | `byte_6E1470 != 0` |
| CHEAT_SPRITE_C | bit 7 | `byte_6E1470 != 0` |

## Decompiled Pseudocode

```c
void __thiscall CheatInputHandler(CheatSystem *this)
{
    int playerIndex;
    DWORD currentFlags;
    int inputHash;

    // For each defined cheat, lazily resolve its input hash then check input state.
    // sub_405380 returns a cached hash for the named input action string.

    // CHEAT_INVULNERABILITY
    inputHash = sub_405380("CHEAT_INVULNERABILITY");
    if (InputTriggered(inputHash)) {
        currentFlags = *(DWORD *)(dword_6E1494 + 4 * playerIndex);
        int newState = (currentFlags & 1) ? 0 : 1;
        sub_443B10(playerIndex, /*flagBit=*/1, newState);  // kallis
    }

    // CHEAT_HEAL
    inputHash = sub_405380("CHEAT_HEAL");
    if (InputTriggered(inputHash)) {
        sub_443B10(playerIndex, /*flagBit=*/2, 1);
    }

    // CHEAT_COMBAT
    inputHash = sub_405380("CHEAT_COMBAT");
    if (InputTriggered(inputHash)) {
        currentFlags = *(DWORD *)(dword_6E1494 + 4 * playerIndex);
        int newState = (currentFlags & 4) ? 0 : 1;
        sub_443B10(playerIndex, /*flagBit=*/4, newState);
    }

    // CHEAT_AMMO
    inputHash = sub_405380("CHEAT_AMMO");
    if (InputTriggered(inputHash)) {
        sub_443B10(playerIndex, /*flagBit=*/8, 1);
    }

    // CHEAT_FIELD_GUIDE
    inputHash = sub_405380("CHEAT_FIELD_GUIDE");
    if (InputTriggered(inputHash)) {
        currentFlags = *(DWORD *)(dword_6E1494 + 4 * playerIndex);
        int newState = (currentFlags & 0x10) ? 0 : 1;
        sub_443B10(playerIndex, /*flagBit=*/0x10, newState);
    }

    // CHEAT_SPRITE_A/B/C — gated on byte_6E1470
    if (byte_6E1470) {
        inputHash = sub_405380("CHEAT_SPRITE_A");
        if (InputTriggered(inputHash)) {
            currentFlags = *(DWORD *)(dword_6E1494 + 4 * playerIndex);
            sub_443B10(playerIndex, /*flagBit=*/0x20, (currentFlags & 0x20) ? 0 : 1);
        }

        inputHash = sub_405380("CHEAT_SPRITE_B");
        if (InputTriggered(inputHash)) {
            currentFlags = *(DWORD *)(dword_6E1494 + 4 * playerIndex);
            sub_443B10(playerIndex, /*flagBit=*/0x40, (currentFlags & 0x40) ? 0 : 1);
        }

        inputHash = sub_405380("CHEAT_SPRITE_C");
        if (InputTriggered(inputHash)) {
            currentFlags = *(DWORD *)(dword_6E1494 + 4 * playerIndex);
            sub_443B10(playerIndex, /*flagBit=*/0x80, (currentFlags & 0x80) ? 0 : 1);
        }
    }
}
```

## Notes

- `sub_443B10` is labelled `kallis` in the IDA database — it is the canonical cheat-flag setter for this system.
- The flag table at `dword_6E1494` is zeroed during `CheatSystemInit` (0x444140); each element maps to one player slot.
- Hash values computed by `sub_405380` are cached after first call, so repeated per-frame calls are cheap.
