# VM Function Registration

**Status:** Confirmed (Session 3)

---

## Overview

The Kallis VM registers native C/C++ functions so they can be called from script code. There are two registration paths:

1. **Native-registered** -- called from `.text` section code (e.g., `RegisterTimeScriptFunctions`)
2. **VM-registered** -- called from `.kallis` section code via ROP-style dispatchers

Both paths ultimately invoke the same registration function: **sub_52EA10**.

---

## Registration Function

| Field | Value |
|-------|-------|
| Address | `0x52EA10` |
| Type | VM thunk (indirect call into `.kallis`) |
| Variant | `0x52EA30` (alternate entry point) |

`sub_52EA10` is the core registration function. It is a VM thunk -- a short `.text` function that performs an indirect jump into the `.kallis` section.

---

## Native Registration Pattern

From `.text` section code, registration is a straightforward function call:

```c
// Example: RegisterTimeScriptFunctions (0x498180)
sub_52EA10("sauSetClockSpeed", handler_497DD0, ...);
sub_52EA10("sauPauseGame",     handler_497FB0, ...);
```

The handler functions are normal native code that use the VM stack operations (PopArg, PushResult, etc.) to exchange data with the script system.

---

## .kallis Registration Pattern (ROP-style)

Within the `.kallis` section, function registration uses a distinctive ROP chain pattern:

```asm
push    handler_addr        ; native handler function pointer
push    name_string_addr    ; pointer to function name string
push    return_addr         ; address to continue after registration
push    sub_52EA10          ; registration function
retn                        ; "calls" sub_52EA10 via return
```

The `retn` instruction pops `sub_52EA10` from the stack and jumps to it. The registration function consumes the name and handler arguments, then returns to `return_addr` to continue the registration chain.

### Example: sauResolvePlayer

```
Address: 0x1CB137B (.kallis)

1CB137B  push    offset sub_493A80          ; handler
1CB1380  push    offset aSauResolvePlayer   ; "sauResolvePlayer" (0x6304F0)
1CB1385  push    offset loc_1CB1394         ; return address (cleanup)
1CB138A  push    offset sub_52EA10          ; registration function
1CB138F  retn                               ; dispatch
         ...
1CB1394  add     esp, 8                     ; cleanup after registration
1CB1397  retn                               ; continue chain
```

The handler `sub_493A80` is a native function that:
1. Calls `sub_52C610` (PopArg) to get a player type argument
2. Resolves player characters by type (Mallory=1, Simon=2, Jared=3, ThimbleTack=other)
3. Calls `sub_52CE40` (PushObjRef) to return the resolved player object

---

## Method Registration on Classes

Class methods follow a similar pattern but include the class name string and are registered as methods on a specific class object:

### Example: sauSetPlayerType on ClPlayerObj

```
Address: 0x1CADF2C (.kallis)

Data layout at 0x1CADF2C:
  dd offset aSausetplayerty    ; "sauSetPlayerType"
  dd <encoded push handler>
  dd offset aClplayerobj       ; "ClPlayerObj"
  dd <method descriptor setup>
  ...
  dd offset sub_4626B0         ; handler function
```

The handler `sub_4626B0` is a `__thiscall` method that:
1. Calls `sub_52C640` (PopArgAlt) to get the player type value
2. Calls a virtual method at vtable offset `+464` to apply the type
3. Calls a virtual method at vtable offset `+452` to finalize

```c
int __thiscall sauSetPlayerType_Handler(void *this) {
    int v3 = 0;
    sub_52C640(&v3);                                    // pop player type
    (*(void**)(*this + 464))(this, v3);                 // apply type
    return (*(int**)(*this + 452))(this);               // finalize
}
```

### Method Registration Data Layout

The `.kallis` section stores method registrations as contiguous data blocks:

| Offset | Content |
|--------|---------|
| +0x00 | Pointer to function name string |
| +0x04 | Encoded push (handler addr) |
| +0x08 | Pointer to class name string |
| +0x0C | Method descriptor bytes |
| ... | Encoded registration ROP chain |
| +N | Pointer to native handler function |

Multiple methods on the same class are registered in sequence (e.g., `sauSetPlayerType`, `sauAddArmorPiece`, `sauShowArmorPiece`, `sauAddHeldProjectile`, `sauSetPosition`, `sauSetRotation` -- all on `ClPlayerObj`).

---

## Registration Sources Summary

| Source | Location | Example |
|--------|----------|---------|
| Native `.text` | `sub_498180` | `sauSetClockSpeed`, `sauPauseGame` |
| Native `.text` | `sub_49BA20` | Various utility script functions |
| `.kallis` ROP | `0x1CB137B` | `sauResolvePlayer` (global function) |
| `.kallis` ROP | `0x1CADF2C` | `sauSetPlayerType` (method on `ClPlayerObj`) |

---

## Key Addresses

| Address | Type | Description |
|---------|------|-------------|
| `0x52EA10` | VM thunk | Primary registration function |
| `0x52EA30` | VM thunk | Registration variant |
| `0x498180` | native | `RegisterTimeScriptFunctions` (registers 7 time functions) |
| `0x49BA20` | native | Registers utility script functions |
| `0x1CB137B` | .kallis | sauResolvePlayer registration site |
| `0x1CADF2C` | .kallis | sauSetPlayerType registration site (ClPlayerObj) |
| `0x493A80` | native | sauResolvePlayer handler |
| `0x4626B0` | native | sauSetPlayerType handler (__thiscall) |
| `0x6304F0` | .rdata | "sauResolvePlayer" string |

---

## VMRegisterMethod (sub_52E660)

In addition to the thunk-based registration (sub_52EA10), the engine has a dedicated **method registration** function at `0x52E660`. This function registers native functions as methods on specific VM classes by:

1. Hashing the class name and method name using the standard hash algorithm
2. Searching the class table at `dword_E561E0` for a matching class (by hash)
3. Searching the class's method table for a matching method entry (by hash)
4. Writing the function address into the method entry slot

If the slot is already occupied, it prints: `"{yWARNING}: Reregistration of the method "%s::%s"."`.

See [subs/sub_52E660_VMRegisterMethod.md](subs/sub_52E660_VMRegisterMethod.md) for full analysis.

---

## Related

- [KALLIS_VM.md](KALLIS_VM.md) -- VM architecture overview (includes class table structure)
- [VM_STACK_OPERATIONS.md](VM_STACK_OPERATIONS.md) -- Stack operations used by handlers
- [VM_ROP_DISPATCHERS.md](VM_ROP_DISPATCHERS.md) -- ROP dispatch mechanism used for .kallis registration
- [subs/sub_52E660_VMRegisterMethod.md](subs/sub_52E660_VMRegisterMethod.md) -- VMRegisterMethod (class method registration)
- [subs/sub_52C610_PopArg.md](subs/sub_52C610_PopArg.md) -- PopArg (used by sauResolvePlayer)
- [subs/sub_52C640_PopArgAlt.md](subs/sub_52C640_PopArgAlt.md) -- PopArgAlt (used by sauSetPlayerType)
- [subs/sub_52CE40_PushObjRef.md](subs/sub_52CE40_PushObjRef.md) -- PushObjRef (used by sauResolvePlayer)
