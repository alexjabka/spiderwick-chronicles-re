# sub_539CF0 --- TransferInputController

**Address:** 0x539CF0 (Spiderwick.exe+139CF0) | **Size:** 88 bytes | **Calling convention:** __thiscall (ECX = PlayerSlot*)

---

## Purpose

Transfers P1 input bindings from the slot's current character to a new character. Reads vtable[2] from both the old and new character references to get a comparable identity, and only performs the transfer if they differ. When a swap is needed, calls `sub_539BC0` to unbind the old character and `sub_539C20` to bind the new one, then updates the slot's character reference at `slot+4`.

This is the mechanism that routes player input to the correct character during a hot-switch.

---

## Prototype

```c
int __thiscall TransferInputController(PlayerSlot *this, void *newCtrl)
```

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | PlayerSlot* (DWORD*) | The 56-byte player slot (from [PlayerSlotLookup](sub_53A020_PlayerSlotLookup.md)) |
| `newCtrl` | void* | New character/controller reference to install in the slot |

**Returns:** int (last sub-call result, not meaningful)

---

## Decompiled Pseudocode

```c
int __thiscall TransferInputController(_DWORD *this, void *newCtrl)
{
    void *v2 = newCtrl;

    // Null guard: use sentinel if no controller provided
    if (!newCtrl)
        v2 = &unk_E58D6C;                      // null sentinel / default controller

    int oldCtrl = *(this + 1);                   // slot+0x04: current character reference

    // Compare identities via vtable[2] on both controllers
    int newIdentity = (*(int (**)(void *))(*(DWORD *)v2 + 8))(v2);       // v2->vtable[2](v2)
    int oldIdentity = (*(int (**)(int))(*(DWORD *)oldCtrl + 8))(oldCtrl); // old->vtable[2](old)

    if (newIdentity != oldIdentity)
    {
        // Different characters --- perform full transfer
        sub_539BC0(*((BYTE *)this + 49), v2, *this);   // unbind old: args = controlByte, newCtrl, descriptor
        sub_539C20(*((BYTE *)this + 49), v2, *this);   // bind new:   args = controlByte, newCtrl, descriptor
        *(this + 1) = v2;                               // update slot+0x04 to new controller
    }

    return result;
}
```

---

## Key Operations

1. **Null guard:** If `newCtrl` is NULL, substitutes `&unk_E58D6C` (a global null sentinel) to prevent crashes.
2. **Identity comparison via vtable[2]:** Calls virtual function at vtable offset +8 (vtable[2]) on both old and new controllers. This extracts a comparable identity value.
3. **Conditional swap:** Only swaps if the identities differ, preventing redundant unbind/bind cycles when the controller is already correct.
4. **Unbind old:** Calls `sub_539BC0(controlByte, newCtrl, descriptor)` to deactivate the old character's input bindings.
5. **Bind new:** Calls `sub_539C20(controlByte, newCtrl, descriptor)` to activate the new character's input bindings.
6. **Update slot reference:** Writes the new controller pointer to `slot+0x04`.

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x539CF0` | Entry point |
| `0x539D07` | Read old controller from `this[1]` (slot+0x04) |
| `0x539D0F` | vtable[2] call on new controller |
| `0x539D18` | vtable[2] call on old controller |
| `0x539D1E` | Comparison: `if (newIdentity != oldIdentity)` |
| `0x539D2B` | `call sub_539BC0` --- unbind old |
| `0x539D3B` | `call sub_539C20` --- bind new |
| `0x539D40` | Update `this[1]` = newCtrl |
| `unk_E58D6C` | Null sentinel / default controller reference |

### PlayerSlot Offsets (relevant to this function)

| Offset | Type | Description |
|--------|------|-------------|
| +0x00 | DWORD | `*this` --- slot descriptor |
| +0x04 | DWORD | `this[1]` --- character reference (updated on swap) |
| +0x31 | byte | `*(this+49)` --- control byte (passed to bind/unbind) |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | Native path at 0x4638FF: `TransferInputController(slot, GetInputController(this))` |
| `sub_465AD0` | Alternate switching path at 0x465AFA |
| `sub_44F320` | Player initialization at 0x44F3DF |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x539BC0` | DeactivateSlotChar | Unbinds old character from input (clears control) |
| `0x539C20` | ActivateSlotChar | Binds new character to input (sets control) |

---

## Notes / Caveats

1. **The vtable[2] comparison** determines if a swap is actually needed. If the old and new controllers have the same identity (same character type), no swap occurs. This prevents redundant deactivation/activation.

2. **The null sentinel at `unk_E58D6C`** is 4 bytes before the slot table base (`dword_E58D70`). This provides a safe default object with a valid vtable that the identity comparison can be called on without crashing.

3. **Offset +0x31 is the IsPlayerControlled byte.** This is the control byte passed to both `sub_539BC0` and `sub_539C20`. It indicates whether the slot is currently player-controlled.

4. **This function only handles input binding transfer.** Camera rebinding, data store updates, and character activation are handled separately by the caller ([SetPlayerType](sub_463880_SetPlayerType.md)).

5. **Related functions:**
   - [PlayerSlotLookup](sub_53A020_PlayerSlotLookup.md) (sub_53A020) --- retrieves the slot this operates on
   - [GetInputController](sub_4537B0_GetInputController.md) (sub_4537B0) --- provides the newCtrl argument
   - [SetPlayerType](sub_463880_SetPlayerType.md) (sub_463880) --- orchestrates the full switch
