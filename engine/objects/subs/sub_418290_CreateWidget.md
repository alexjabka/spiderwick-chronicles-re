# sub_418290 -- CreateWidget

**Address:** 0x418290 (Spiderwick.exe+18290) | **Calling convention:** __thiscall (ECX = widget descriptor*)

---

## Purpose

Initializes a widget descriptor by hashing its name string and storing the hash at `this[0]`. Sets a flag byte at `this[4]` to 1. The original name string is **NOT stored** -- only the hash is retained.

This is the fundamental widget identification function. Every engine entity (characters, cameras, UI elements, objects) is identified by a widget hash computed by this function. The hash algorithm is identical to `HashString` (sub_405380).

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | BYTE* | Widget descriptor to initialize |
| `name` | char* | Name string to hash (e.g., "jared", "simon", "mallory") |

**Returns:** void

---

## Decompiled Pseudocode

```c
void __thiscall CreateWidget(BYTE *this, char *name)
{
    int hash = 0;
    if (name)
    {
        char c = *name;
        if (c)
        {
            char *p = name;
            do
            {
                ++p;
                int shifted = c + (hash << (c & 7));
                c = *p;
                hash += shifted;
            }
            while (*p);
        }
    }

    *(DWORD*)this = hash;   // this[0] = computed hash
    *(this + 4) = 1;        // this[4] = initialized flag
}
```

---

## Hash Algorithm

The hash algorithm is character-by-character:

```c
hash = 0;
for each character c in name:
    hash += c + (hash << (c & 7));
```

This is the same algorithm as `HashString` (sub_405380) but inlined into the widget creation code. The shift amount is the low 3 bits of the character value (range 0-7), making the hash sensitive to character ordering and case.

### Example Hashes

| Name | Hash |
|------|------|
| `"jared"` | `0xEA836` |
| `"mallory"` | `0xB283C70` |
| `"simon"` | `0x5E4B5EC` |

---

## Key Behavior

1. **Name string is NOT stored.** Only the hash at `this[0]` is retained. This means there is no way to recover the original name from a widget descriptor -- all comparisons are done by hash.

2. **Flag at this[4]** is set to 1, indicating the widget has been initialized. This is checked by other systems before using the hash value.

3. **Null name handling:** If `name` is NULL or empty, the hash is 0.

---

## Called By (Selected)

This function has an extremely large number of callers (250+). Key ones include:

| Caller | Context |
|--------|---------|
| `sub_493A80` (sauResolvePlayer) | Hashes "mallory", "simon", "jared" for character resolution |
| `sub_489100` (sauRespawn) | Hashes character names during respawn |
| `sub_463F80` | Player object initialization |
| `sub_48AC50` | Level loading -- creates widgets for level objects |
| `sub_48B390` | World loading -- creates widgets for world objects |
| `sub_48B5D0` | World loading variant |
| Many `.kallis` callers | Widget creation from VM scripts |

---

## Notes

1. The widget hash is the engine's primary identification mechanism. It is used in the character switching system, data store key lookups, event matching, and entity resolution.

2. Because the hash function does not handle collisions, two different strings could theoretically produce the same hash. In practice, the engine's name space is small enough that collisions are unlikely.

3. The hash is stored at offset 0 of the widget descriptor, which means `widget->hash == *(DWORD*)widget`. This is the value compared during character lookups.

---

## Related Documentation

- [../sub_405380_HashString.md](../sub_405380_HashString.md) -- Standalone hash function (same algorithm)
- [../CHARACTER_SYSTEM.md](../CHARACTER_SYSTEM.md) -- Widget hash usage in character identification
- [../HOT_SWITCH_SYSTEM.md](../HOT_SWITCH_SYSTEM.md) -- Hash-to-type conversion during switching
- [sub_44FEC0_WidgetHashToType.md](sub_44FEC0_WidgetHashToType.md) -- Converts widget hash to player type
