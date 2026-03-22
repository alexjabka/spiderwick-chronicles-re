# sub_462170 --- DeactivateAllPlayers

**Address:** 0x462170 (Spiderwick.exe+62170) | **Size:** 73 bytes | **Calling convention:** __cdecl

---

## Purpose

Iterates all playable characters via the `GetPlayerCharacter2` / `NextPlayableChar` linked list and resets each character's movement parameters to default (1.0). For each character where the component pointer at offset +0x138 is non-zero, calls `sub_551F50(1.0)` and `sub_551F80(1.0)` to reset movement speed/scale axes.

Called at the start of `SetPlayerType` (sub_463880) to ensure all characters are in a neutral movement state before a hot-switch occurs.

---

## Prototype

```c
_DWORD* DeactivateAllPlayers(void)
```

## Parameters

None.

**Returns:** `_DWORD*` --- last iteration result (not meaningful to callers)

---

## Decompiled Pseudocode

```c
_DWORD* DeactivateAllPlayers(void)
{
    _DWORD *result = GetPlayerCharacter2();         // sub_44F8F0 - vtable[20] check
    _DWORD *charObj = result;

    while (result)
    {
        if (charObj[78])                             // offset +0x138: component pointer
        {
            sub_551F50(1.0f);                        // reset movement param A (X axis)
            sub_551F80(1.0f);                        // reset movement param B (Y axis)
        }

        result = sub_44F920(charObj);                // NextPlayableChar
        charObj = result;
    }

    return result;
}
```

---

## Key Operations

1. **Get first playable character** via `GetPlayerCharacter2()` (sub_44F8F0), which uses vtable[20] (IsPlayableCharacter) to find ALL playable characters, not just the currently controlled one.
2. **Iterate linked list** via `sub_44F920` (NextPlayableChar) until NULL.
3. **Guard check:** Only resets characters where `charObj[78]` (offset +0x138) is non-zero, filtering out uninitialized or inactive characters.
4. **Reset movement:** Calls `sub_551F50(1.0)` and `sub_551F80(1.0)` to normalize both movement axes to default scale.

---

## Key Addresses and Data

| Address | Instruction | Description |
|---------|------------|-------------|
| `0x462170` | `call GetPlayerCharacter2` | Entry: get first playable character |
| `0x462180` | `cmp dword ptr [esi+138h], 0` | Guard: check component at +0x138 |
| `0x462195` | `call sub_551F50` | Reset movement param A with 1.0f (0x3F800000) |
| `0x4621A2` | `call sub_551F80` | Reset movement param B with 1.0f (0x3F800000) |
| `0x4621A8` | `call sub_44F920` | Advance to next playable character |

### Character Offsets

| Offset | Type | Description |
|--------|------|-------------|
| +0x138 | DWORD | Component/state pointer; must be non-zero for reset to apply |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | First action at 0x463892 before slot lookup |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x44F8F0` | GetPlayerCharacter2 | Get first playable character (vtable[20] check) |
| `0x44F920` | NextPlayableChar (sub_44F920) | Iterate to next playable character in linked list |
| `0x551F50` | ResetMovementParamA | Reset X movement parameter to 1.0 (delegates to sub_551D00) |
| `0x551F80` | ResetMovementParamB | Reset Y movement parameter to 1.0 (delegates to sub_551D90) |

---

## Notes / Caveats

1. **The `charObj+0x138` check** prevents resetting characters that have not been fully initialized. This field is a component/state pointer that is NULL for dormant characters.

2. **1.0f (0x3F800000) is the default/neutral movement scale.** Both reset functions normalize movement so the new character starts with default speed after the switch.

3. **GetPlayerCharacter2 uses vtable[20]** (IsPlayableCharacter), which returns true for ALL playable characters including non-active ones. This is different from GetPlayerCharacter (vtable[19] = IsPlayerControlled), which only finds the currently controlled character.

4. **In practice, usually only one character exists at runtime**, so this loop typically iterates once. In coop/ThimbleTack scenarios with multiple spawned characters, all get reset.

5. **Related functions:**
   - [IsCharacterDead](sub_457850_IsCharacterDead.md) (sub_457850) --- called just before this in SetPlayerType
   - [PlayerSlotLookup](sub_53A020_PlayerSlotLookup.md) (sub_53A020) --- called just after this in SetPlayerType
   - [ActivateAsPlayer](sub_55C7B0_ActivateAsPlayer.md) (sub_55C7B0) --- called later in the switch sequence
