# sub_55C7B0 --- ActivateAsPlayer

**Address:** 0x55C7B0 (Spiderwick.exe+15C7B0) | **Size:** 14 bytes | **Calling convention:** __cdecl

---

## Purpose

Activates (or deactivates) a character object as a player. This is a `.kallis` thunk that loads the character pointer into a register and jumps to the `.kallis` handler at `off_1C82C34` for the actual activation logic.

Called at the end of `SetPlayerType`'s native path with `flag=1` to make the target character the active player after slot/input/camera operations are complete.

---

## Prototype

```c
int __cdecl ActivateAsPlayer(int charObj, int flag)
```

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `charObj` | int (ClCharacterObj*) | The character object to activate/deactivate |
| `flag` | int | 1 = activate as player, 0 = deactivate (presumed) |

**Returns:** int (delegated to `.kallis`, return value unknown)

---

## Decompiled Pseudocode

```c
int __cdecl ActivateAsPlayer(int charObj, int flag)
{
    // Load charObj into register, jump to .kallis thunk
    // Note: flag is on the stack but only charObj is forwarded
    return off_1C82C34(charObj);
}
```

The actual implementation uses register-based calling for the `.kallis` dispatch:
```asm
mov  eax, [esp+arg_0]   ; eax = charObj
mov  ecx, [esp+arg_4]   ; ecx = flag
jmp  ds:off_1C82C34      ; .kallis thunk
```

---

## Key Operations

1. **Load arguments into registers** for the `.kallis` calling convention (eax = charObj, ecx = flag).
2. **Indirect jump** to `off_1C82C34` --- the `.kallis` ROP dispatcher handles the actual activation logic.

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x55C7B0` | Entry point (thunk) |
| `off_1C82C34` | `.kallis` indirect jump target (activation logic) |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | `ActivateAsPlayer(this, 1)` at 0x463933 --- activates new player |
| `sub_465AD0` | Alternate switching path at 0x465B1D |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `off_1C82C34` | `.kallis` activation handler | Actual character activation/deactivation logic |

---

## Notes / Caveats

1. **This is a `.kallis` thunk** that passes arguments via registers (eax = charObj, ecx = flag) rather than the stack. This is the non-standard calling convention used by the `.kallis` ROP dispatch mechanism.

2. **The flag parameter** is always 1 in `SetPlayerType`'s native path. The presumed semantics are 1=activate, 0=deactivate, but only activation has been observed.

3. **Cannot be safely called from arbitrary contexts** due to the `.kallis` delegation. However, in practice it appears robust when called from within the engine's normal execution flow (SetPlayerType calls it directly from the native path).

4. **Only 14 bytes total** --- this is a pure forwarding thunk with no logic of its own.

5. **Related functions:**
   - [SetPlayerType](sub_463880_SetPlayerType.md) (sub_463880) --- primary caller
   - [DeactivateAllPlayers](sub_462170_DeactivateAllPlayers.md) (sub_462170) --- resets characters before activation
   - [TransferInputController](sub_539CF0_TransferInputController.md) (sub_539CF0) --- input is transferred before activation
