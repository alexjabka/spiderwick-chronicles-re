# sub_443160 — CheckCheatFlag

**Address:** `Spiderwick.exe+43160` (absolute: `00443160`)
**Convention:** __cdecl
**Returns:** int (non-zero if flag is set)

## Signature
```c
int __cdecl CheckCheatFlag(int group, int bit)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| group | int | Flag group index (1-based) |
| bit | int | Bit position within the group |

## Description
Tests whether a specific cheat flag bit is set. Computes `(1 << bit) & *(DWORD*)(dword_6E1494 + 4*group - 4)` and returns the result. Non-zero means the flag is active.

## Key Details
- Flag table at `dword_6E1494`, indexed by group (1-based, so group 1 is at offset 0)
- Each group is a DWORD bitmask
- Returns the masked value (non-zero = set, zero = not set)
- Used to check invulnerability, HUD hide, combat cheats, etc.

## Pseudocode
```c
int __cdecl CheckCheatFlag(int group, int bit)
{
    return (1 << bit) & *(DWORD *)(dword_6E1494 + 4 * group - 4);
}
```

## Called By
- Various gameplay systems checking cheat state (damage, HUD, combat, ammo)

## Related
- [sub_443EC0_CheatInputHandler.md](sub_443EC0_CheatInputHandler.md) — sets cheat flags
- [sub_444140_CheatSystemInit.md](sub_444140_CheatSystemInit.md) — initializes flag table
- [../CHEAT_SYSTEM.md](../CHEAT_SYSTEM.md) — cheat system overview
