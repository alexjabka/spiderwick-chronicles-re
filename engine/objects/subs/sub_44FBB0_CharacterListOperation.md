# sub_44FBB0 -- CharacterListOperation (RemoveFromCharacterList)

**Address:** `0x44FBB0` | **Size:** `0xDF` (223 bytes) | **Calling convention:** __cdecl

## Purpose

Removes a character from the global character linked list. Before removal, performs player-awareness logic: if the character being removed is associated with the current player character, it notifies via `nullsub_336` (likely a stripped notification/callback).

The function checks bit 2 of `flags2` at offset `+0x1CC` before proceeding. If that bit is not set, the function is a no-op. After removal, the bit is cleared and the next pointer (`+0x5A0`) is zeroed.

## Decompiled

```c
void __cdecl CharacterListOperation(char** a1)  // a1 = character to remove
{
    // Check bit 2 of flags at +0x1CC
    // (a1[115] = *(a1 + 460) = offset 0x1CC when char** indexing)
    if ( ((unsigned int)a1[115] & 4) == 0 )     // bit 2 not set?
        return;                                   // nothing to do

    // --- Player awareness phase ---
    char** playerChar = (char**)GetPlayerCharacter();  // sub_44F890
    if (playerChar)
    {
        // Iterate through all player-controlled characters
        while (1)
        {
            if (a1 != playerChar)
            {
                // Check if playerChar's class-name linked list contains "ClPlayerObj"
                char* node = (char*)playerChar[3];     // +0x0C = class name ptr
                if (node)
                {
                    while (node != off_6E2C58)         // off_6E2C58 -> "ClPlayerObj"
                    {
                        node = *(char**)(node + 4);    // next in name chain
                        if (!node) goto skip_notify;
                    }
                    nullsub_336(playerChar, a1);       // notify player about removal
                }
            }
        skip_notify:
            // Advance to next player-controlled character
            playerChar = (char**)playerChar[360];      // +0x5A0 = next
            if (!playerChar) break;
            // vtable[19] check (IsPlayerControlled)
            if ( (*(unsigned __int8 (__thiscall**)(char**))(*playerChar + 0x4C))(playerChar) )
                continue;                               // found another player char
            // else keep scanning
            // ... (inner loop to find next player-controlled)
        }
    }

    // --- Linked list removal phase ---
    char* head = (char*)g_CharacterListHead;           // [0x7307D8]

    if (a1 == (char**)g_CharacterListHead)
    {
        // Removing the head node
        g_CharacterListHead = (int)a1[360];            // head = a1->next
        a1[115] = (char*)((unsigned int)a1[115] & ~4); // clear bit 2 at +0x1CC
        a1[360] = 0;                                   // clear next pointer
    }
    else
    {
        // Removing a non-head node: find predecessor
        char* prev = head;
        while (*(char***)(prev + 0x5A0) != a1)
        {
            prev = *(char**)(prev + 0x5A0);            // advance
        }
        // Unlink: prev->next = a1->next
        *(int*)(prev + 0x5A0) = (int)a1[360];
        a1[115] = (char*)((unsigned int)a1[115] & ~4); // clear bit 2 at +0x1CC
        a1[360] = 0;                                   // clear next pointer
    }
}
```

## Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `a1` | `ClCharacterObj*` | Character to remove from the linked list |

## Returns

Technically returns a `char**` in the decompiler output, but the return value is not meaningful -- callers do not use it.

## Called by / Calls

### Called by (2 callsites)

| Address | Name | Context |
|---------|------|---------|
| `0x451050` | sub_451050 | Character teardown/destruction |
| `0x46A460` | sub_46A460 | Character cleanup |

### Calls

| Address | Name | Notes |
|---------|------|-------|
| `0x44F890` | GetPlayerCharacter | Finds current player for notification |
| `0x462290` | nullsub_336 | Player notification (stripped/empty in release build) |

## Key Offsets

- `[0x7307D8]` -- `g_CharacterListHead`, head of linked list
- `character+0x0C` -- class name string pointer
- `character+0x1CC` -- flags2 (bit 2 = "in character list" flag)
- `character+0x5A0` -- next pointer in linked list
- `off_6E2C58` -- pointer to "ClPlayerObj" string

## Algorithm

### Phase 1: Player Notification

1. Checks bit 2 of `a1+0x1CC` -- if not set, the character is not in the list, so return immediately
2. Calls `GetPlayerCharacter()` to find the active player
3. For each player-controlled character (iterating with vtable[19]):
   - If the player character is NOT the one being removed:
     - Walks the class-name chain at `+0x0C` looking for `"ClPlayerObj"`
     - If found, calls `nullsub_336(playerChar, removedChar)` -- in the release build this is a no-op, but in debug builds it likely logged or notified the player system

### Phase 2: Linked List Removal

Standard singly-linked list removal:
- **If removing head:** update `g_CharacterListHead` to `a1->next`
- **If removing non-head:** walk list to find predecessor, set `prev->next = a1->next`
- In both cases: clear bit 2 of `+0x1CC` and zero the next pointer at `+0x5A0`

## Flags at +0x1CC (flags2)

| Bit | Mask | Meaning |
|-----|------|---------|
| 2 | `0x04` | Character is in the linked list (set on insertion, cleared on removal) |

The function reads this bit at entry (`shr eax, 2; test al, 1`) and clears it on removal (`and [edi+1CCh], 0FFFFFFFBh`).

## Notes

- `nullsub_336` at `0x462290` is empty in the shipping binary -- the notification logic exists but does nothing. In debug builds it likely performed validation or logging.
- The class-name chain walk (checking for `"ClPlayerObj"` via `off_6E2C58`) suggests the engine supports a form of runtime type introspection through linked class-name entries.
- The function is safe to call if the character is not in the list (bit 2 check at entry).
- After removal, the character object itself is NOT freed -- that happens in the calling teardown functions.

## Related

- [CHARACTER_SYSTEM.md](CHARACTER_SYSTEM.md) -- Character system overview
- [sub_44F890_GetPlayerCharacter.md](sub_44F890_GetPlayerCharacter.md) -- Called to find player before removal
- [sub_44FA20_CreateCharacterInternal.md](sub_44FA20_CreateCharacterInternal.md) -- Counterpart: adds characters to the list
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) -- Object field layout (flags at +0x1CC)
- [subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md](subs/sub_462E00_ClPlayerObj_IsPlayerControlled.md) -- vtable[19] used in notification phase
