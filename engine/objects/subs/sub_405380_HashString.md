# sub_405380 -- HashString

**Address:** `0x405380` | **Size:** `0x2F` (47 bytes) | **Calling convention:** __cdecl

## Purpose

Computes a hash value from a null-terminated string. Used extensively throughout the engine for fast string-based lookups: character identification, data store keys, event names, widget matching, cheat code detection, and more.

Called from 74+ callsites -- one of the most widely used utility functions in the engine.

## Decompiled

```c
int __cdecl HashString(unsigned char* a1)
{
    unsigned char* ptr = a1;
    int result = 0;

    if (a1)
    {
        for (unsigned char c = *ptr; c != 0; c = *++ptr)
        {
            result += c + (result << (c & 7));
        }
    }

    return result;
}
```

### Assembly

```asm
HashString:
    push    esi
    mov     esi, [esp+8]           ; a1 = string pointer
    xor     eax, eax               ; result = 0
    test    esi, esi
    jz      short .ret             ; NULL check
    mov     cl, [esi]              ; c = *ptr
    test    cl, cl
    jz      short .ret             ; empty string check
.loop:
    movzx   edx, cl                ; v4 = (unsigned)c
    mov     edi, eax               ; copy result
    and     cl, 7                  ; c & 7
    shl     edi, cl                ; result << (c & 7)
    add     esi, 1                 ; ++ptr
    add     edx, edi               ; c + (result << (c & 7))
    mov     cl, [esi]              ; next char
    add     eax, edx               ; result += ...
    test    cl, cl
    jnz     short .loop
.ret:
    pop     esi
    retn
```

## Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `a1` | `char*` (unsigned) | Null-terminated string to hash. NULL-safe (returns 0). |

## Returns

`int` -- 32-bit hash value. Returns `0` for NULL or empty strings.

## Hash Algorithm

For each byte `c` in the string:
```
result = result + c + (result << (c & 7))
```

Where `c & 7` produces a shift amount of 0-7 bits depending on the low 3 bits of the character. This creates position-dependent mixing: the same character at different positions yields different contributions because `result` changes.

### Properties

- **Deterministic:** Same input always produces same output
- **Case-sensitive:** "Jared" and "jared" produce different hashes
- **NULL-safe:** Returns 0 for NULL pointers
- **Not cryptographic:** Simple mixing, collisions possible but rare in practice
- **Position-dependent:** Character order matters (unlike simple additive hashes)

### Example Hashes (from the binary)

| String | Hash | Context |
|--------|------|---------|
| "/Player/Character" | `0xA488A96A` | Character switching state key |
| "jared" | (computed at runtime) | Character switching lookup |
| "simon" | (computed at runtime) | Character switching lookup |
| "mallory" | (computed at runtime) | Character switching lookup |

## Called by / Calls

### Called by (74+ callsites, sample)

| Address | Name | Context |
|---------|------|---------|
| `0x407B10` | sub_407B10 | System initialization |
| `0x4127E0` | sub_4127E0 | Data store key lookup |
| `0x416A90` | sub_416A90 | Event system |
| `0x4262F0` | sub_4262F0 | Widget system |
| `0x42AC60` | sub_42AC60 | UI system |
| `0x43A2D0` | sub_43A2D0 | Camera system |
| `0x43C740` | DebugCameraManager_ReadInput | Debug camera input hash |
| `0x43DD70` | sub_43DD70 | Camera system |
| `0x440790` | DebugCameraManager_InputHandler | Debug input hash |
| `0x443EC0` | CheatInputHandler | Cheat code string hashing |
| `0x454750` | sub_454750 | Character projectile system |
| `0x454820` | sub_454820 | Character combat system |
| `0x493A80` | SwitchPlayerCharacter | Hashes "jared", "simon", "mallory" |

### Calls

None (leaf function, pure computation).

## Usage Patterns

### Character Identification (sub_493A80)

```c
int hashMallory = HashString("mallory");
int hashSimon   = HashString("simon");
int hashJared   = HashString("jared");
// Then compared against character widget hashes for switching
```

### Data Store Keys (sub_41E830)

```c
int keyHash = HashString("/Player/Character");
// keyHash = 0xA488A96A, used to look up game state values
```

### Cheat Code Detection (sub_443EC0)

```c
int inputHash = HashString(inputBuffer);
// Compared against known cheat code hashes
```

### Event System

```c
int eventHash = HashString("EventName");
// Used as fast lookup key in event dispatch tables
```

## Notes

- The shift amount `c & 7` means only the low 3 bits of each character affect the shift. For ASCII letters (0x41-0x7A), this gives varied but deterministic shift amounts.
- The hash accumulates -- later characters are influenced by the hash of all previous characters, providing good distribution.
- This is the engine's primary string-to-integer mapping. Understanding this hash is essential for:
  - Matching strings in the data store
  - Computing widget hashes for CE scripts
  - Understanding event routing
  - Reverse engineering cheat codes

## Related

- [CHARACTER_SYSTEM.md](CHARACTER_SYSTEM.md) -- Character system (uses hashes for identification)
- [sub_44F890_GetPlayerCharacter.md](sub_44F890_GetPlayerCharacter.md) -- Player lookup (widget hash used in switching)
- [subs/sub_493A80_SwitchPlayerCharacter.md](subs/sub_493A80_SwitchPlayerCharacter.md) -- Hashes character names
- [../../debug/subs/sub_443EC0_CheatInputHandler.md](../../debug/subs/sub_443EC0_CheatInputHandler.md) -- Hashes cheat input
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) -- Object layout (name struct at +0x1C0)
