# sub_539AE0 --- GetCharacterFromSlot

**Address:** 0x539AE0 (Spiderwick.exe+139AE0) | **Size:** 4 bytes | **Calling convention:** __thiscall (ECX = PlayerSlot*)

---

## Purpose

Retrieves the character object reference stored in a player slot. Returns the value at `this[1]` (offset +0x04), which is the character/entity reference that can be further resolved via [GetEntityFromCharacter](sub_450DA0_GetEntityFromCharacter.md) (sub_450DA0) to obtain the actual `ClCharacterObj*`.

This is a trivial 4-byte accessor: `return *(this + 1)`.

---

## Prototype

```c
int __thiscall GetCharacterFromSlot(_DWORD *this)
```

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | _DWORD* / PlayerSlot* | Pointer to a 56-byte player slot entry (from [PlayerSlotLookup](sub_53A020_PlayerSlotLookup.md)) |

**Returns:** `int` --- character/entity reference stored at slot offset +0x04

---

## Decompiled Pseudocode

```c
int __thiscall GetCharacterFromSlot(_DWORD *this)
{
    return *(this + 1);  // slot + 0x04: character reference
}
```

### Assembly

```asm
539AE0  mov eax, [ecx+4]
539AE3  retn
```

---

## Key Operations

1. Returns the DWORD at `this + 4` (slot offset +0x04). No validation, no null checks.

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x539AE0` | Entry point |
| `slot+0x04` | Character/entity reference field within the 56-byte PlayerSlot |

### PlayerSlot Offset

| Offset | Type | Description |
|--------|------|-------------|
| +0x04 | DWORD | Character/entity reference --- the value this function returns |

---

## Called By (selected)

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | At 0x4638AC: resolve target character from slot |
| `sub_440F60` | Character management at 0x440FA1 |
| `sub_455620` | Player validation at 0x45571D, 0x45579C |
| `SectorDistanceCheck` | Distance calculation at 0x487DC6 |
| `sub_4884C0` | Camera/sector at 0x488510, 0x488553, 0x488570 |
| `sub_491E10` | Player management at 0x491E35 |
| `sub_491F30` ([sauSubstitutePlayer](sub_491F30_sauSubstitutePlayer.md)) | At 0x491FAD |
| `sub_49A0B0` | Coop events at 0x49A102, 0x49A133, 0x49A164 |
| `sub_4BECD0` | At 0x4BED64, 0x4BED92 |
| `sub_4C6720` / `sub_4C69A0` | Character queries |
| `sub_53A250` | Slot iteration at 0x53A258 |

## Calls

None --- this is a leaf function (single instruction + ret).

---

## Notes / Caveats

1. **Only 4 bytes** (`mov eax, [ecx+4]; retn`). One of the smallest functions in the binary.

2. **The returned value is an entity reference, not a direct object pointer.** It must be passed through [GetEntityFromCharacter](sub_450DA0_GetEntityFromCharacter.md) (sub_450DA0) to resolve to an actual `ClCharacterObj*`. This is the standard pattern seen in SetPlayerType:
   ```c
   int entityRef = GetCharacterFromSlot(slot);    // sub_539AE0
   ClCharacterObj* charObj = GetEntityFromCharacter(entityRef);  // sub_450DA0
   ```

3. **This is the inverse of the write in [TransferInputController](sub_539CF0_TransferInputController.md)** (sub_539CF0), which writes to `this[1]` when updating the slot's character reference.

4. **19 cross-references** across the engine. Every function that needs to go from a slot to a character calls this accessor.

5. **Related functions:**
   - [PlayerSlotLookup](sub_53A020_PlayerSlotLookup.md) (sub_53A020) --- provides the slot pointer
   - [GetEntityFromCharacter](sub_450DA0_GetEntityFromCharacter.md) (sub_450DA0) --- resolves the returned reference
   - [TransferInputController](sub_539CF0_TransferInputController.md) (sub_539CF0) --- writes to the same field
