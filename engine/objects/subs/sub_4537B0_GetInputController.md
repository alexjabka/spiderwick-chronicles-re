# sub_4537B0 --- GetInputController

**Address:** 0x4537B0 (Spiderwick.exe+537B0) | **Size:** 7 bytes | **Calling convention:** __thiscall (ECX = ClCharacterObj*)

---

## Purpose

Returns a pointer to the character's input controller block, located at a fixed offset of 300 bytes (0x12C) from the start of the character object. This is a trivial accessor --- a 7-byte function that simply computes `this + 300`.

The returned pointer is the input state block used by the slot system to route player input to the active character during hot-switches.

---

## Prototype

```c
char* __thiscall GetInputController(char *this)
```

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | char* / ClPlayerObj* | Character object pointer |

**Returns:** `char*` (void*) --- pointer to the input controller at `this + 300` (0x12C)

---

## Decompiled Pseudocode

```c
char* __thiscall GetInputController(char *this)
{
    return this + 300;  // offset 0x12C
}
```

### Assembly

```asm
4537B0  lea eax, [ecx+12Ch]
4537B6  retn
```

---

## Key Operations

1. Returns `this + 0x12C` (300 decimal). No validation, no null checks --- pure offset calculation.

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x4537B0` | Entry point |
| `+0x12C` | Offset of input controller within ClCharacterObj/ClPlayerObj |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | Gets current char's input controller at 0x4638F7 before swap |
| `sub_465AD0` | Alternate switching path at 0x465AF2 |
| `sub_44FE00` | Input binding query at 0x44FE13 |
| `sub_44FE20` | Input binding query at 0x44FE3B |
| `sub_462370` | Player movement at 0x462371 |
| `sub_41EEE0` (approx) | Input system at 0x41EEE7 |

## Calls

None --- this is a leaf function (single instruction + ret).

---

## Notes / Caveats

1. **Only 7 bytes.** This is `lea eax, [ecx+12Ch]; retn` --- one of the smallest functions in the binary.

2. **Offset 0x12C (300)** places the input controller within the middle of the `ClCharacterObj` layout, between the mesh/animation pointers and the name/widget descriptors.

3. **The returned pointer is passed to [TransferInputController](sub_539CF0_TransferInputController.md)** (sub_539CF0) during character switches. The transfer uses vtable[2] on the controller to extract a comparable identity.

4. **This function is NOT a `.kallis` thunk** --- it is pure native code with no VM delegation.

5. **Related functions:**
   - [TransferInputController](sub_539CF0_TransferInputController.md) (sub_539CF0) --- consumes the returned controller
   - [SetPlayerType](sub_463880_SetPlayerType.md) (sub_463880) --- primary caller chain
   - [ClCharacterObj_layout](../ClCharacterObj_layout.md) --- character object field layout
