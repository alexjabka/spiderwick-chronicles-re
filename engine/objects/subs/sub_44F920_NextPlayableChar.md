# sub_44F920 -- NextPlayableCharacter

**Address:** `0x44F920` | **Size:** `0x2B` (43 bytes) | **Calling convention:** __cdecl

## Purpose

Given a character pointer, advances through the linked list (via `+0x5A0`) and returns the next character where `vtable[20]` (IsPlayableCharacter, offset `+0x50` in the vtable) returns `TRUE`.

This is used for iterating through playable characters -- for example, when cycling through available characters during character switching or when building the character roster in UI code.

Note the distinction from `GetPlayerCharacter` (sub_44F890): that function uses vtable[19] (IsPlayerControlled) to find the ONE character currently being controlled. This function uses vtable[20] (IsPlayableCharacter) to find characters that CAN be controlled, which may include multiple characters.

## Decompiled

```c
_DWORD* __cdecl NextPlayableCharacter(_DWORD* a1)
{
    _DWORD* cur = a1;

    if (!a1)
        return NULL;

    do
    {
        cur = (_DWORD*)cur[360];  // +0x5A0 = next character in list
    }
    while (cur && !(*(unsigned __int8 (__thiscall**)(_DWORD*))(*cur + 0x50))(cur));
    //                  vtable[20] = IsPlayableCharacter (offset +0x50)

    return cur;  // NULL if no more playable characters
}
```

### Assembly (annotated)

```asm
sub_44F920:
    push    esi
    mov     esi, [esp+8]           ; a1
    test    esi, esi
    jnz     short .loop
    xor     eax, eax               ; return NULL
    pop     esi
    retn
.loop:
    mov     esi, [esi+5A0h]        ; cur = cur->next
    test    esi, esi
    jz      short .done
    mov     eax, [esi]             ; vtable
    mov     edx, [eax+50h]         ; vtable[20]
    mov     ecx, esi               ; this
    call    edx
    test    al, al
    jz      short .loop            ; not playable, keep going
.done:
    mov     eax, esi
    pop     esi
    retn
```

## Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `a1` | `ClCharacterObj*` | Starting character (iteration begins AFTER this one) |

## Returns

`ClCharacterObj*` -- pointer to the next playable character in the linked list after `a1`. Returns `NULL` if:
- `a1` is `NULL`
- No playable character exists after `a1` in the list

## Called by / Calls

### Called by (5 callsites)

| Address | Name | Context |
|---------|------|---------|
| `0x462170` | sub_462170 | Character management |
| `0x4634C0` | sub_4634C0 | Character management |
| `0x491E70` | sub_491E70 | Gameplay system |
| `0x493A80` | SwitchPlayerCharacter | Cycles through playable characters |
| `0x4D54E0` | sub_4D54E0 | Object system |

### Calls

| Address | Name | Notes |
|---------|------|-------|
| (indirect) | `vtable[20]` | IsPlayableCharacter (offset `+0x50` in vtable) |

## Key Offsets

- `character+0x5A0` -- next pointer in linked list (DWORD index [360])
- `*character + 0x50` -- vtable[20], IsPlayableCharacter virtual call

## vtable[19] vs vtable[20]

| Vtable Slot | Offset | Name | Meaning |
|-------------|--------|------|---------|
| [19] | +0x4C | IsPlayerControlled | Is this the ONE character the player is controlling RIGHT NOW? |
| [20] | +0x50 | IsPlayableCharacter | CAN this character be player-controlled? (may be multiple) |

`GetPlayerCharacter` uses [19]; this function uses [20]. In practice, all `ClPlayerObj` instances may return `TRUE` from [20], while only the active one returns `TRUE` from [19].

## Notes

- Unlike `GetPlayerCharacter`, this function takes a starting point and returns the NEXT match, making it suitable for iteration
- The iteration skips the starting character itself (advances first, then tests)
- To iterate all playable characters: start from `g_CharacterListHead`, call this repeatedly until NULL
- Typically only one ClPlayerObj exists at runtime (the active player), so this often returns NULL

## Related

- [CHARACTER_SYSTEM.md](CHARACTER_SYSTEM.md) -- Character system overview
- [sub_44F890_GetPlayerCharacter.md](sub_44F890_GetPlayerCharacter.md) -- Uses vtable[19] instead
- [sub_44F950_CountPlayerCharacters.md](sub_44F950_CountPlayerCharacters.md) -- Counts vtable[19] matches
- [subs/sub_493A80_SwitchPlayerCharacter.md](subs/sub_493A80_SwitchPlayerCharacter.md) -- Caller: cycles playable characters
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) -- Object field layout
