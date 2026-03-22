# sub_44F950 -- CountPlayerCharacters

**Address:** `0x44F950` | **Size:** `0x60` (96 bytes) | **Calling convention:** __cdecl (no args)

## Purpose

Iterates the global character linked list starting from `g_CharacterListHead` (`0x7307D8`). Counts the number of characters where `vtable[19]` (IsPlayerControlled, offset `+0x4C`) returns `TRUE`.

In practice, this should return 0 or 1 since only one character is player-controlled at a time. However, the function is written generically to handle the theoretical case of multiple player-controlled characters.

## Decompiled

```c
int CountPlayerCharacters(void)
{
    _DWORD* cur = (_DWORD*)g_CharacterListHead;  // [0x7307D8]
    int count = 0;

    if (!cur)
        return 0;

    // Phase 1: find first player-controlled character
    do
    {
        if ( (*(unsigned __int8 (__thiscall**)(_DWORD*))(*cur + 0x4C))(cur) )
            break;                          // vtable[19] = IsPlayerControlled
        cur = (_DWORD*)cur[360];            // +0x5A0 = next
    }
    while (cur);

    // Phase 2: count all player-controlled characters from here
    while (cur)
    {
        ++count;
        // Advance to next player-controlled character
        do
        {
            cur = (_DWORD*)cur[360];        // +0x5A0 = next
            if (!cur)
                goto done;
        }
        while ( !(*(unsigned __int8 (__thiscall**)(_DWORD*))(*cur + 0x4C))(cur) );
        //         vtable[19] = IsPlayerControlled
    }

done:
    return count;
}
```

### Assembly (key section)

```asm
CountPlayerCharacters:
    push    esi
    mov     esi, g_CharacterListHead    ; [0x7307D8]
    push    edi
    xor     edi, edi                    ; count = 0
    test    esi, esi
    jz      short .ret
    ; ... find first match via vtable[0x4C] ...
    ; ... then count loop via vtable[0x4C] ...
.ret:
    mov     eax, edi                    ; return count
    pop     edi
    pop     esi
    retn
```

## Parameters

None.

## Returns

`int` -- number of characters in the linked list where `vtable[19]` (IsPlayerControlled) returns `TRUE`. Expected values:
- `0` -- no player character in list (e.g., during level transition)
- `1` -- normal gameplay (one active player)

## Called by / Calls

### Called by (1 callsite)

| Address | Name | Context |
|---------|------|---------|
| `0x4B7D20` | sub_4B7D20 | Likely a validation/assert or UI query |

### Calls

| Address | Name | Notes |
|---------|------|-------|
| (indirect) | `vtable[19]` | IsPlayerControlled (offset `+0x4C` in vtable) |

## Key Offsets

- `[0x7307D8]` -- `g_CharacterListHead`, start of linked list
- `character+0x5A0` -- next pointer (DWORD index [360])
- `*character + 0x4C` -- vtable[19], IsPlayerControlled virtual call

## Algorithm Detail

The function uses a two-phase approach:

1. **Phase 1:** Linear scan to find the first player-controlled character. Exits early if none found.
2. **Phase 2:** From the first match, continues scanning and counting. For each match, increments count, then advances to find the next match.

This is equivalent to a single-pass count but the decompiler splits it due to the `break` in phase 1 followed by the counting loop in phase 2.

## Notes

- Uses the same `vtable[19]` check as `GetPlayerCharacter` (sub_44F890)
- Only 1 callsite suggests this is rarely used -- possibly a debug/validation function
- The two-phase structure is an optimization: if no player character exists, the function exits without entering the counting loop

## Related

- [CHARACTER_SYSTEM.md](CHARACTER_SYSTEM.md) -- Character system overview
- [sub_44F890_GetPlayerCharacter.md](sub_44F890_GetPlayerCharacter.md) -- Same vtable[19] check, returns first match
- [sub_44F920_NextPlayableChar.md](sub_44F920_NextPlayableChar.md) -- Uses vtable[20] instead
- [sub_44F7C0_FindClosestCharacter.md](sub_44F7C0_FindClosestCharacter.md) -- Distance-based search
- [subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md](subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md) -- vtable[19] implementation
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) -- Object field layout
