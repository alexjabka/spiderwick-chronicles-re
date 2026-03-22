# sub_4626B0 --- sauSetPlayerType

**Address:** 0x4626B0 (Spiderwick.exe+626B0) | **Calling convention:** __thiscall (ECX = ClPlayerObj*)

---

## Purpose

VM method handler registered on `ClPlayerObj` for the `sauSetPlayerType` script function. Pops the target character type from the VM stack, then calls `SetPlayerType` (vtable[116]) followed by `CommitPlayerState` (vtable[113]) on the current player object.

This is the entry point for all script-initiated character switches.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | ClPlayerObj* | The active player character (method target) |

VM stack input: `type` (int) --- 1=Jared, 2=Mallory, 3=Simon

**Returns:** int (result of vtable[113])

---

## Decompiled Pseudocode

```c
int __thiscall sauSetPlayerType(ClPlayerObj *this)
{
    int type = 0;
    PopInt(&type);                                   // sub_52C640 - pop from VM stack

    this->vtable[116](this, type);                   // SetPlayerType (offset 464)
    return this->vtable[113](this);                  // CommitPlayerState (offset 452)
}
```

---

## Assembly

```asm
4626B0  push    ecx
4626B1  push    esi
4626B2  mov     esi, ecx              ; esi = this (ClPlayerObj*)
4626B4  push    ebp
4626B5  xor     ebp, ebp
4626B7  mov     [esp+0Ch+var_4], ebp  ; type = 0
4626B9  lea     eax, [esp+0Ch+var_4]
4626BD  push    eax
4626BE  mov     ecx, esi
4626C0  push    ecx                   ; (unused arg? or this for PopInt context)
4626C1  call    sub_52C640            ; PopInt(&type)
4626C6  add     esp, 8
4626C9  mov     ecx, [esi]            ; ecx = vtable ptr
4626CB  mov     eax, [ecx+1D0h]      ; vtable[116] = offset 0x1D0 = 464
4626D1  push    [esp+0Ch+var_4]       ; push type
4626D5  mov     ecx, esi              ; this
4626D7  push    ecx
4626D8  call    eax                   ; vtable[116](this, type) = SetPlayerType
4626DA  mov     edx, [esi]            ; vtable ptr again
4626DC  add     esp, 8
4626DF  mov     eax, [edx+1C4h]      ; vtable[113] = offset 0x1C4 = 452
4626E5  mov     ecx, esi              ; this
4626E6  call    eax                   ; vtable[113](this) = CommitPlayerState
4626E8  pop     ebp
4626E9  pop     esi
4626EA  pop     ecx
4626EB  retn
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x4626C1` | `call sub_52C640` --- PopInt from VM stack |
| `0x4626D8` | `call eax` --- vtable[116] = SetPlayerType (sub_463880) |
| `0x4626E6` | `call eax` --- vtable[113] = CommitPlayerState (sub_462B80) |

### Vtable Offsets

| Offset | Index | Function |
|--------|-------|----------|
| 0x1D0 (464) | vtable[116] | SetPlayerType (sub_463880) |
| 0x1C4 (452) | vtable[113] | CommitPlayerState (sub_462B80) |

---

## Called By

| Caller | Context |
|--------|---------|
| `.kallis` VM | Script method dispatch on ClPlayerObj |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x52C640` | PopInt | Pop integer from VM argument stack |
| vtable[116] | SetPlayerType | Core switch function (sub_463880) |
| vtable[113] | CommitPlayerState | Health/weapons/input setup (sub_462B80) |

---

## Notes / Caveats

1. **This is a VM method handler**, meaning it is called through the Kallis VM's method dispatch mechanism on a ClPlayerObj instance. The `this` pointer is the current player character, and the argument (type) comes from the VM stack.

2. **The two vtable calls are sequential and unconditional.** SetPlayerType runs first (which may delegate to .kallis for ClPlayerObj targets), then CommitPlayerState restores health and weapons. If SetPlayerType delegates to .kallis, the CommitPlayerState call here may not execute --- the .kallis code handles the full transition internally.

3. **sub_52C640 (PopInt)** pops a single integer from the VM argument stack. The type value (1/2/3) was pushed by the calling script before invoking `sauSetPlayerType`.

4. **To invoke this from native code**, you would need to push the type onto the VM stack via `sub_52C600` (PushInt) and then call this method with the correct `this` pointer. However, the VM context may not be valid outside of script execution.
