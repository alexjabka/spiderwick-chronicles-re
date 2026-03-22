# sub_44FEC0 -- WidgetHashToType

**Address:** 0x44FEC0 (Spiderwick.exe+4FEC0) | **Calling convention:** __cdecl | **Type:** .kallis thunk

---

## Purpose

Converts a widget name hash to a player type number. This is the bridge between the character identification system (which uses hashes) and the player slot system (which uses type indices). Called during `SetPlayerType` to write the correct type value to the `/Player/Character` data store variable.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `hash` | int | Widget name hash (from HashString on the character name) |
| `flag` | int | Unknown flag (always 1 in observed calls) |

**Returns:** `int` -- player type number (1=Jared, 2=Mallory widget, 3=Simon widget)

---

## Decompiled Pseudocode

```c
// .kallis thunk -- dispatches to VM handler
int __cdecl WidgetHashToType(int hash, int flag)
{
    return off_1C88E74(hash, flag);  // .kallis ROP dispatcher
}
```

The actual hash-to-type mapping is implemented inside the `.kallis` VM. The thunk at `off_1C88E74` resolves to ROP code at `0x1CFE9F0`.

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | At 0x463953: converts character's widget hash to type for data store write |
| `sub_452D80` | Another switching-related function |

---

## Notes

1. This is a `.kallis` thunk -- the 6-byte `.text` stub just does `jmp ds:[off_1C88E74]`.

2. The hash values correspond to lowercase character names:
   - `"jared"` -> hash -> type 1
   - `"mallory"` -> hash -> type 2 (note: Mallory's widget type is 2, but visually shows Simon)
   - `"simon"` -> hash -> type 3 (note: Simon's widget type is 3, but visually shows Mallory)

3. The type value returned by this function is what gets written to `/Player/Character` in the data store, controlling which character the level loading system spawns on next load.

---

## Related Documentation

- [sub_463880_SetPlayerType.md](sub_463880_SetPlayerType.md) -- Primary consumer
- [../HOT_SWITCH_SYSTEM.md](../HOT_SWITCH_SYSTEM.md) -- Hot-switch system overview
- [../CHARACTER_SYSTEM.md](../CHARACTER_SYSTEM.md) -- Widget hash system
- [../../data/DATA_STORE.md](../../data/DATA_STORE.md) -- Data store system
