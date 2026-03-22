# sub_44F890 -- GetPlayerCharacter

**Address:** `0x44F890` | **Size:** `0x2B` (43 bytes) | **Calling convention:** __cdecl (no args)

## Purpose

Iterates the global character linked list starting from `g_CharacterListHead` (`0x7307D8`). For each character, calls `vtable[19]` (IsPlayerControlled, offset `+0x4C` in the vtable). Returns the first character where that call returns `TRUE`.

This is the engine's canonical way to obtain the currently player-controlled character. It is called from 44+ callsites across camera, gameplay, AI, UI, and debug systems.

## Decompiled

```c
_DWORD* GetPlayerCharacter(void)
{
    _DWORD* cur = (_DWORD*)g_CharacterListHead;  // [0x7307D8]

    if (cur)
    {
        do
        {
            // vtable[19] = IsPlayerControlled (offset +0x4C in vtable)
            if ( (*(unsigned __int8 (__thiscall**)(_DWORD*))(*cur + 0x4C))(cur) )
                break;
            cur = (_DWORD*)cur[360];  // +0x5A0 = next character
        }
        while (cur);
    }
    return cur;  // NULL if none found
}
```

## Parameters

None.

## Returns

`ClCharacterObj*` (or `ClPlayerObj*`) -- pointer to the currently player-controlled character object. Returns `NULL` if no character in the list has `IsPlayerControlled` returning `TRUE`.

## Called by / Calls

### Called by (44+ callsites, sample)

| Address | Name | Context |
|---------|------|---------|
| `0x43C4E0` | DebugCameraManager_CameraUpdate | Camera follows player |
| `0x43D860` | sub_43D860 | Camera thunk (`jmp GetPlayerCharacter` at 0x43D850) |
| `0x44FBB0` | CharacterListOperation | Finds player before removal logic |
| `0x463CD0` | sub_463CD0 | Player object post-init |
| `0x4699F0` | sub_4699F0 | Gameplay system |
| `0x490F60` | sub_490F60 | Gameplay system |
| `0x407B10` | sub_407B10 | System init |

### Calls

| Address | Name | Notes |
|---------|------|-------|
| (indirect) | `vtable[19]` | IsPlayerControlled -- ClPlayerObj: sub_462E00, ClCharacterObj: sub_454330 |

## Key Offsets

- `[0x7307D8]` -- `g_CharacterListHead`, start of character linked list
- `character+0x5A0` -- next pointer in linked list (DWORD index [360])
- `*character + 0x4C` -- vtable[19], IsPlayerControlled virtual call

## Vtable[19] Resolution

For `ClCharacterObj` (vtable `0x62A0E4`): calls `sub_454330`, which always returns `FALSE`.

For `ClPlayerObj` (vtable `0x62B9EC`): calls `sub_462E00`, which resolves through the Kallis data system:
1. `facet::facet(this)` -- create query from character pointer
2. `sub_53A2B0(query)` -- resolve component via `off_627800`
3. `sub_539AC0(component)` -- returns byte at `component+0x31`

The byte at `+0x31` is `TRUE` only for the one character currently under player control. See [subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md](subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md).

## Hook (pPlayer_CT_entry.cea)

The Cheat Engine script `pPlayer_CT_entry.cea` hooks this function. It saves the return value (EAX) to the `pPlayer` symbol on every call, giving stable access to the player object pointer. With 44+ calls per frame, the pointer is always current.

## Notes

- This function is a hot path -- 44+ callers mean it runs many times per frame
- A thunk exists at `0x43D850` (`jmp GetPlayerCharacter`) used by camera subsystem
- The linked list is NOT sorted; the player character can be at any position
- Only ONE character will return true from vtable[19] at any given time

## Related

- [CHARACTER_SYSTEM.md](CHARACTER_SYSTEM.md) -- Character system overview
- [sub_44F920_NextPlayableChar.md](sub_44F920_NextPlayableChar.md) -- Same pattern, vtable[20]
- [sub_44F950_CountPlayerCharacters.md](sub_44F950_CountPlayerCharacters.md) -- Counts vtable[19] matches
- [sub_44F7C0_FindClosestCharacter.md](sub_44F7C0_FindClosestCharacter.md) -- Distance-based character search
- [sub_44FBB0_CharacterListOperation.md](sub_44FBB0_CharacterListOperation.md) -- Calls GetPlayerCharacter before removal
- [subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md](subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md) -- vtable[19] implementation
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) -- Object field layout
