# sub_462E00 --- IsPlayerControlled (ClPlayerObj)

**Address:** 0x462E00 (Spiderwick.exe+62E00) | **Calling convention:** __thiscall (ECX = ClPlayerObj*)

**vtable[19]** for ClPlayerObj (offset 76 from vtable base)

---

## Purpose

Determines if this character is the currently player-controlled character. This is the ClPlayerObj override of vtable[19]; the base class version (`sub_454330` / `ClCharacterObj_IsPlayerControlled_Base`) always returns 0.

The check resolves through the player slot system:
1. Creates a temporary query struct `{type_ptr, charObj}` via `std::locale::facet::facet`
2. Calls `sub_53A2B0` (VM lookup / .kallis thunk) to find the slot for this character
3. Calls `sub_539AC0` on the slot, which simply returns byte at `slot+0x31` (offset 49)

The **IsPlayerControlled flag** is therefore NOT stored on the character object itself --- it lives in the player slot table at `dword_E58D70`.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | ClPlayerObj* | The character to check |

**Returns:** BOOL (1 = this is the player-controlled character, 0 = not controlled)

---

## Decompiled Pseudocode

```c
BOOL __thiscall IsPlayerControlled(ClPlayerObj *this)
{
    void *queryStruct[2];   // stack: [type_descriptor, charObj]

    // Create query from character pointer
    int slotQuery = facet_facet(&queryStruct, (unsigned int)this);

    // Resolve to player slot via VM/slot system
    int slot = sub_53A2B0(slotQuery);                // .kallis thunk --> off_1C89708

    // Fixup type descriptor (probably for destructor safety)
    queryStruct[0] = &off_627800;

    // Check control byte
    if (slot)
        return GetControlByte(slot);                 // sub_539AC0: return *(slot + 49)
    else
        return FALSE;
}
```

---

## Resolution Chain

```
ClPlayerObj::IsPlayerControlled(this)
  |
  +-- facet::facet(&query, this)      ; create {type, charRef} pair
  |
  +-- sub_53A2B0(query)               ; .kallis thunk (off_1C89708)
  |     Resolves charObj --> player slot in dword_E58D70
  |
  +-- sub_539AC0(slot)                ; return *(slot + 49)  i.e. slot+0x31
        The IsPlayerControlled flag!
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x462E08` | `call facet::facet` --- create query struct |
| `0x462E0E` | `call sub_53A2B0` --- .kallis thunk, resolves to slot |
| `0x462E18` | `mov [esp], offset off_627800` --- fixup type descriptor |
| `0x462E31` | Return: `slot && GetControlByte(slot)` |

### sub_539AC0 --- GetControlByte

```c
char __thiscall GetControlByte(BYTE *this)
{
    return *(this + 49);   // this + 0x31
}
```

This is the simplest function in the chain: a single byte read at offset 49 (0x31) from the slot base.

### sub_454330 --- Base Class Version

```c
char ClCharacterObj_IsPlayerControlled_Base(void)
{
    return 0;   // always false
}
```

Non-player character classes (creatures, NPCs) inherit this base version, which unconditionally returns false. Only `ClPlayerObj` overrides it with the slot-based check.

### Key Constants

| Address | Name | Description |
|---------|------|-------------|
| `off_627800` | Type descriptor | Used in query struct cleanup |
| `off_1C89708` | .kallis thunk target | sub_53A2B0 dispatches here |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_44F890` (GetPlayerCharacter) | Iterates char list, calls vtable[19] on each |
| `sub_44F950` (CountPlayerCharacters) | Counts chars where vtable[19] returns true |
| Various gameplay systems | Any code that needs to know "is this the active player?" |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `std::locale::facet::facet` | facet constructor | Create query struct on stack |
| `0x53A2B0` | SlotLookup (.kallis thunk) | charObj --> player slot |
| `0x539AC0` | GetControlByte | Read slot+0x31 byte |

---

## Notes / Caveats

1. **The control state is external to the character object.** Unlike a simple flag on the object, IsPlayerControlled resolves through the slot system. This means changing the active player requires updating the slot table, not the character object.

2. **sub_53A2B0 is a .kallis thunk** (`off_1C89708`). It dispatches through the VM's ROP-style code. This is why the resolution chain cannot be trivially replicated from native code --- the slot lookup itself goes through the VM.

3. **The `facet::facet` call is misidentified by IDA** --- it is not actually the C++ standard library `std::locale::facet`. It is an engine function that creates a `{type_descriptor, object_reference}` pair used by the slot system for lookup.

4. **Performance note:** This function is called on every character in the linked list during `GetPlayerCharacter()`. The .kallis thunk adds overhead. With a typical list of 5-20 characters, this is called 5-20 times per player lookup.

5. **To manually check IsPlayerControlled** from a mod, you can bypass this entire chain and directly read the slot byte:
   ```c
   int slot = sub_53A020(type);  // 1=Jared, 2=Mallory, 3=Simon
   if (slot)
       bool controlled = *(BYTE*)(slot + 0x31);
   ```
   This avoids the .kallis thunk entirely.
