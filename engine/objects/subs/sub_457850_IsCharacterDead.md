# sub_457850 --- IsCharacterDead

**Address:** 0x457850 (Spiderwick.exe+57850) | **Size:** 6 bytes | **Calling convention:** __thiscall (ECX = ClCharacterObj*)

---

## Purpose

Checks whether a character is currently dead, invalid, or in a switching/transition state. This is a `.kallis` thunk that delegates to `off_1C890B0` for the actual state check. Returns a boolean indicating whether the character is unavailable for switching.

Used as the **first guard** in [SetPlayerType](sub_463880_SetPlayerType.md) (sub_463880) --- if the current character fails this check, the switch is aborted immediately. Also checked on the resolved target character before proceeding with the switch.

---

## Prototype

```c
int __thiscall IsCharacterDead(void *this)
```

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | void* / ClCharacterObj* | Character object to check |

**Returns:** `int` (bool) --- non-zero = character is dead/switching/unavailable, 0 = character is alive and ready

---

## Decompiled Pseudocode

```c
// attributes: thunk
int __thiscall IsCharacterDead(void *this)
{
    return off_1C890B0(this);   // .kallis thunk delegation
}
```

### Assembly

```asm
457850  jmp ds:off_1C890B0     ; .kallis indirect jump
```

---

## Key Operations

1. **Indirect jump** to `.kallis` handler at `off_1C890B0`. The actual state logic (reading a death flag, transition state, or validity check) runs entirely in the `.kallis` ROP segment.

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x457850` | Entry point (thunk) |
| `off_1C890B0` | `.kallis` indirect call target (state check logic) |
| `0x1D03230` | Code-type xref from `.kallis` segment |

---

## Called By (selected)

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | First guard at 0x463885 --- bails if `this` is dead/switching |
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | Second check at 0x4638C2 --- bails if target is dead/switching |
| `sub_405AC0` | At 0x405AEB |
| `sub_4187F0` | At 0x418875 |
| `sub_41D7A0` | At 0x41DA95 |
| `sub_433220` | At 0x433260 |
| `sub_43B510` | At 0x43B54B |
| `sub_44AC60` | At 0x44AD50 |
| `sub_4578B0` | At 0x4578B3 (near-neighbor function) |
| `sub_459070` ([CharacterDeath](sub_459070_CharacterDeath.md)) | At 0x45907B |
| `sub_45A5B0` | At 0x45A5F7 |
| `sub_4728A0` | At 0x472A36 |
| `sub_49B050` | At 0x49B126 |
| `sub_49B160` | At 0x49B227 |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `off_1C890B0` | `.kallis` state check | Reads character state and returns bool |

---

## Notes / Caveats

1. **Called TWICE in SetPlayerType.** First on `this` (the current player character) at function entry (0x463885), then on the resolved target character (0x4638C2). Both must pass (return 0) for the switch to proceed.

2. **This is a `.kallis` thunk** --- the entire function is a single `jmp` instruction (6 bytes). The actual state check logic runs in the `.kallis` ROP segment.

3. **The checked state** likely covers multiple conditions: character death, mid-switch transition, and possibly invalid/unloaded state. The exact flag or state enum being read is inside the `.kallis` handler.

4. **15 cross-references** across the engine show this is a general-purpose character validity/liveness check, not just a switching guard. Functions like `CharacterDeath` (sub_459070) and various AI/interaction handlers all call it.

5. **Only 6 bytes** --- `jmp ds:off_1C890B0` with no prologue or epilogue.

6. **Related functions:**
   - [SetPlayerType](sub_463880_SetPlayerType.md) (sub_463880) --- primary consumer (double guard)
   - [DeactivateAllPlayers](sub_462170_DeactivateAllPlayers.md) (sub_462170) --- called after this check passes
   - [ActivateAsPlayer](sub_55C7B0_ActivateAsPlayer.md) (sub_55C7B0) --- activation only happens if this returns 0
