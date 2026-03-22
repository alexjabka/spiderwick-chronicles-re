# Player Slot System

**Status:** Reversed (table layout, lookup, swap mechanics, control byte identified)

---

## Overview

The engine maintains a global **player slot table** at `dword_E58D70` that tracks playable character assignments. Each character type (Jared=1, Mallory=2, Simon=3) has a 56-byte slot entry. The slot stores a character reference, a control byte that determines which character is player-controlled, and activation state managed through `.kallis` callbacks.

This system is the backbone of character switching: `SetPlayerType` (sub_463880) manipulates slots to swap the active character, and `IsPlayerControlled` (sub_462E00/vtable[19]) reads the control byte from this table to determine which character responds to input.

---

## Table Layout

```
dword_E58D70 --> pointer to table:

+0x00  count (DWORD)     Number of slot entries
+0x04  entries_base       Start of slot array

Entry = base + 56 * (type - 1)     (type is 1-based)
```

### Slot Entry Layout (56 bytes)

| Offset | Hex | Type | Field | Description |
|--------|-----|------|-------|-------------|
| 0 | 0x00 | DWORD* | vtable/descriptor | Slot type descriptor (used by virtual calls) |
| 4 | 0x04 | DWORD | charRef | Character reference (entity ref for assigned character) |
| ... | | | | Internal state fields |
| 49 | 0x31 | BYTE | **controlByte** | **IsPlayerControlled flag.** Non-zero = this slot's character is player-controlled. |
| ... | | | | Remaining fields to offset 55 |

---

## Key Functions

### sub_53A020 --- GetPlayerSlot

**Address:** 0x53A020 | **Convention:** __cdecl

Looks up a slot by character type. Type is 1-based (1=Jared, 2=Mallory, 3=Simon). Passing type 0 is treated as type 1.

```c
int __cdecl GetPlayerSlot(unsigned int type)
{
    if (!dword_E58D70)
        return 0;

    unsigned int t = type;
    if (t == 0)
        t = 1;                    // clamp to 1

    if (t <= *(DWORD*)dword_E58D70)   // t <= count
        return *(DWORD*)(dword_E58D70 + 4) + 56 * t - 56;
    else
        return 0;
}
```

**Formula:** `slot_ptr = entries_base + 56 * (type - 1)`

Where `entries_base = *(dword_E58D70 + 4)`.

---

### sub_539CF0 --- SwapSlotChar

**Address:** 0x539CF0 | **Convention:** __thiscall (ECX = slot*)

Swaps the character reference assigned to a slot. Calls deactivate callback on the old character and activate callback on the new one. Only swaps if the new reference differs from the current one.

```c
int __thiscall SwapSlotChar(PlayerSlot *this, void *newCharRef)
{
    if (!newCharRef)
        newCharRef = &unk_E58D6C;        // fallback/null sentinel

    int oldRef = this->charRef;          // this[1], offset +0x04
    int newEntity = newCharRef->vtable[2](newCharRef);   // resolve entity ID
    int oldEntity = oldRef->vtable[2](oldRef);

    if (newEntity != oldEntity)
    {
        // Deactivate old character if conditions met
        sub_539BC0(this->controlByte, newCharRef, this->descriptor);

        // Activate new character if conditions met
        sub_539C20(this->controlByte, newCharRef, this->descriptor);

        // Update slot reference
        this->charRef = newCharRef;      // this[1] = newCharRef
    }
}
```

---

### sub_539CB0 --- SetSlotState

**Address:** 0x539CB0 | **Convention:** __stdcall

A `.kallis` thunk that enables or disables a slot. Dispatches to ROP code at `off_1C8D810`.

```c
int __stdcall SetSlotState(int flag)
{
    return off_1C8D810(flag);    // .kallis ROP dispatcher
}
```

In `SetPlayerType`, called as `sub_539CB0(1)` to enable the target slot before the swap.

---

### sub_539AC0 --- GetControlByte

**Address:** 0x539AC0 | **Convention:** __thiscall (ECX = slot*)

Returns the IsPlayerControlled flag byte from the slot.

```c
char __thiscall GetControlByte(BYTE *this)
{
    return *(this + 49);      // offset 0x31
}
```

This is what `IsPlayerControlled` (vtable[19]) ultimately reads.

---

### sub_539BC0 --- DeactivateCallback

**Address:** 0x539BC0 | **Convention:** __thiscall (ECX = slot byte ptr)

Conditional deactivation logic. Checks the control byte and validates both old and new character references before allowing deactivation.

```c
BOOL __thiscall DeactivateCallback(BYTE *this, char controlByte, int newRef, int oldRef)
{
    bool slotActive = (*(this + 49) != 0);
    int currentRef = *(DWORD*)(this + 4);
    int descriptor = *(DWORD*)this;

    // Deactivate if:
    //   (slot not active OR old char invalid OR old descriptor invalid)
    //   AND controlByte is set
    //   AND new char is valid
    //   AND new descriptor is valid
    return (!slotActive || !currentRef->vtable[1](currentRef) || !sub_54FCC0(descriptor))
        && controlByte
        && newRef->vtable[1](newRef)
        && sub_54FCC0(oldRef);
}
```

---

### sub_539C20 --- ActivateCallback

**Address:** 0x539C20 | **Convention:** __thiscall (ECX = slot byte ptr)

Conditional activation logic. Inverse of DeactivateCallback --- activates the new character.

```c
BOOL __thiscall ActivateCallback(BYTE *this, char controlByte, int newRef, int oldRef)
{
    bool slotActive = (*(this + 49) != 0);
    int currentRef = *(DWORD*)(this + 4);
    int descriptor = *(DWORD*)this;

    // Activate if:
    //   slot IS active
    //   AND current char is valid
    //   AND current descriptor is valid
    //   AND (controlByte is clear OR new char is invalid OR new descriptor is invalid)
    return slotActive
        && currentRef->vtable[1](currentRef)
        && sub_54FCC0(descriptor)
        && (!controlByte || !newRef->vtable[1](newRef) || !sub_54FCC0(oldRef));
}
```

---

## How the Slot System Fits Into Switching

```
sauSetPlayerType (VM handler)
  |
  +-- SetPlayerType(this, type)           vtable[116]
  |     |
  |     +-- GetPlayerSlot(type)           sub_53A020: find 56-byte slot
  |     +-- Resolve target char from slot
  |     +-- IsClassType check at 0x4638DE
  |     |     |
  |     |     +-- ClPlayerObj? --> .kallis VM (full spawn/despawn)
  |     |     +-- Not ClPlayerObj? --> native path:
  |     |           |
  |     |           +-- SetSlotState(slot, 1)       sub_539CB0: enable slot
  |     |           +-- SwapSlotChar(slot, charRef)  sub_539CF0: deactivate old, activate new
  |     |           +-- Camera swap + ActivateCharacter
  |     |
  |     +-- Write /Player/Character to data store
  |     +-- vtable[14](this) -- FinalActivation
  |
  +-- CommitPlayerState(this)             vtable[113]
        +-- Load health, weapons, input mode from data store
```

---

## Global Variables

| Address | Type | Name | Description |
|---------|------|------|-------------|
| `0xE58D70` | DWORD | dword_E58D70 | Pointer to slot table base |
| `0xE58D6C` | -- | unk_E58D6C | Null/fallback character reference |

---

## Reading Slots from a Mod

To check which character type is currently player-controlled:

```c
for (int type = 1; type <= 3; type++)
{
    int slot = sub_53A020(type);   // or: *(dword_E58D70 + 4) + 56*(type-1)
    if (slot)
    {
        BYTE controlled = *(BYTE*)(slot + 0x31);
        if (controlled)
            printf("Type %d is player-controlled\n", type);
    }
}
```

To directly read the slot table without calling sub_53A020:

```c
DWORD table = *(DWORD*)0xE58D70;
if (table)
{
    DWORD count = *(DWORD*)table;
    DWORD base = *(DWORD*)(table + 4);
    for (DWORD i = 0; i < count; i++)
    {
        DWORD slot = base + 56 * i;
        BYTE controlled = *(BYTE*)(slot + 0x31);
        DWORD charRef = *(DWORD*)(slot + 0x04);
        // ...
    }
}
```

---

## Related Documentation

- [CHARACTER_SWITCHING.md](CHARACTER_SWITCHING.md) --- Full switching system overview
- [subs/sub_463880_SetPlayerType.md](subs/sub_463880_SetPlayerType.md) --- Uses slot system for switching
- [subs/sub_462E00_IsPlayerControlled.md](subs/sub_462E00_IsPlayerControlled.md) --- Reads controlByte from slot
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) --- Character object layout
