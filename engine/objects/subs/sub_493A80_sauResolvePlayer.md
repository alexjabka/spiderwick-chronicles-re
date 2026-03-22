# sub_493A80 --- sauResolvePlayer

**Address:** 0x493A80 (Spiderwick.exe+93A80) | **Calling convention:** __cdecl

---

## Purpose

VM function handler for `sauResolvePlayer`. Despite the old name "SwitchPlayerCharacter" in earlier docs, this function does NOT perform the switch itself --- it **resolves** which character object corresponds to a given type and returns it to the VM.

The function ensures that character widgets have been created for Mallory, Simon, and Jared (tracked by a creation bitmask), then searches the character linked list for the character matching the requested type's widget hash. Returns the found character (or the current player) to the VM via `sub_52CE40` (PushObject).

---

## Parameters

VM stack input: `type` (int) --- 0=current, 1=Jared, 2=Mallory, 3=Simon, 4+=ThimbleTack

**Returns:** int (result of PushObject)

---

## Decompiled Pseudocode

```c
int sauResolvePlayer(void)
{
    int type;
    PopType(&type);                                  // sub_52C610 - pop type from VM stack

    int creationMask = dword_D42D14;

    // === ENSURE WIDGETS CREATED ===

    // Mallory (bit 0)
    if ((creationMask & 1) == 0)
    {
        creationMask |= 1;
        dword_D42D14 = creationMask;
        CreateWidget("Mallory");                     // creates/resolves Mallory widget
        byte_D42D10 = 1;                             // Mallory ready flag
    }

    // Simon (bit 1)
    if ((creationMask & 2) == 0)
    {
        creationMask |= 2;
        dword_D42D14 = creationMask;
        CreateWidget("Simon");
        byte_D42D08 = 1;                             // Simon ready flag
    }

    // Jared (bit 2)
    if ((creationMask & 4) == 0)
    {
        dword_D42D14 = creationMask | 4;
        CreateWidget("Jared");
        byte_D42D00 = 1;                             // Jared ready flag
    }

    // === RESOLVE CHARACTER ===

    ClCharacterObj *playableChar = GetPlayerCharacter2();   // sub_44F8F0 - vtable[20] check

    if (type == 0)
    {
        // Type 0: return current player character
        ClPlayerObj *current = GetPlayerCharacter();        // sub_44F890 - vtable[19] check
        return PushObject(current);                         // sub_52CE40
    }

    // Look up widget hash for requested type
    int targetWidgetHash;
    switch (type)
    {
        case 1:  targetWidgetHash = dword_D42CFC;  break;  // Jared widget global
        case 2:  targetWidgetHash = dword_D42D0C;  break;  // Mallory widget global
        case 3:  targetWidgetHash = dword_D42D04;  break;  // Simon widget global
        default:
            CreateWidget("ThimbleTack");
            targetWidgetHash = /* result from CreateWidget */;
            break;
    }

    // Check if current player already matches
    ClPlayerObj *currentPlayer = GetPlayerCharacter();
    if (currentPlayer && currentPlayer->widgetDesc->hash == targetWidgetHash)  // *(*(this+112)+36) == hash
    {
        return PushObject(currentPlayer);
    }

    // Search playable character list for matching widget hash
    while (playableChar)
    {
        if (*(*(playableChar + 0x1C0) + 0x24) == targetWidgetHash)
            break;
        playableChar = NextPlayableChar(playableChar);      // sub_44F920
    }

    return PushObject(playableChar);                        // sub_52CE40
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x493A8A` | `call sub_52C610` --- PopType from VM stack |
| `0x493AB0` | `CreateWidget("Mallory")` |
| `0x493AD4` | `CreateWidget("Simon")` |
| `0x493AF8` | `CreateWidget("Jared")` |
| `0x493B09` | `call GetPlayerCharacter2` --- get first playable char |
| `0x493B5A` | `CreateWidget("ThimbleTack")` --- type 4+ fallback |
| `0x493B63` | `call GetPlayerCharacter` --- get current player |
| `0x493B75` | `cmp` --- check if current player's widget hash matches target |
| `0x493B8B` | Loop: iterate playable chars searching for widget hash match |

### Widget Globals

| Address | Name | Character | Description |
|---------|------|-----------|-------------|
| `dword_D42CFC` | g_JaredWidget | Jared | Widget hash/reference for Jared |
| `dword_D42D04` | g_SimonWidget | Simon | Widget hash/reference for Simon |
| `dword_D42D0C` | g_MalloryWidget | Mallory | Widget hash/reference for Mallory |

### Creation Bitmask: `dword_D42D14`

| Bit | Character | When Set |
|-----|-----------|----------|
| 0 | Mallory | After `CreateWidget("Mallory")` |
| 1 | Simon | After `CreateWidget("Simon")` |
| 2 | Jared | After `CreateWidget("Jared")` |

### Ready Flags

| Address | Character |
|---------|-----------|
| `byte_D42D00` | Jared ready |
| `byte_D42D08` | Simon ready |
| `byte_D42D10` | Mallory ready |

---

## Widget Hash Matching

The character lookup compares `targetWidgetHash` against the hash stored at:
```
character + 0x1C0 --> widgetDesc ptr
widgetDesc + 0x24 --> hash (DWORD)
```

In the decompiled code this appears as `*(_DWORD *)(*(_DWORD *)(char + 448) + 36)`.

Known widget hashes (computed by `HashString` at 0x405380):
- Jared: `0xEA836`
- Mallory: `0xB283C70`
- Simon: `0x5E4B5EC`
- ThimbleTack: `0x5923C9C6`

---

## Called By

| Caller | Context |
|--------|---------|
| `.kallis` VM | Registered as VM function handler |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x52C610` | PopType | Pop type from VM stack |
| CreateWidget | CreateWidget | Ensure character widget exists |
| `0x44F8F0` | GetPlayerCharacter2 | First playable character (vtable[20]) |
| `0x44F890` | GetPlayerCharacter | Current player character (vtable[19]) |
| `0x44F920` | NextPlayableChar | Iterate to next playable character |
| `0x52CE40` | PushObject | Push result to VM return stack |

---

## Notes / Caveats

1. **This is NOT the switch function** --- it is a resolver. The actual switch is done by `sauSetPlayerType` (sub_4626B0) which calls `SetPlayerType` (vtable[116]). This function only finds the character object for a given type.

2. **The creation bitmask is persistent** (`dword_D42D14`). Once a widget is created, it is never recreated. The bitmask survives across calls within a session.

3. **`sub_52C610` (PopType)** is slightly different from `sub_52C640` (PopInt). It pops a VM "type" value which may have different encoding or validation.

4. **The widget globals (`dword_D42CFC`, etc.) are set by `CreateWidget`**, not by this function. This function only reads them after ensuring creation has happened.

5. **Type 0 is a special case** --- it returns the current player character without any widget hash lookup. This is used by scripts that need a reference to "whoever is active right now."

6. **For type 4+** (ThimbleTack and beyond), the function creates the widget on every call and uses the creation result directly, rather than caching in a global.
