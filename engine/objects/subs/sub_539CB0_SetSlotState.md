# sub_539CB0 --- SetSlotState

**Address:** 0x539CB0 (Spiderwick.exe+139CB0) | **Calling convention:** __stdcall

---

## Purpose

Enables or disables a player slot. Acts as a `.kallis` thunk, delegating to `off_1C8D810` for the actual state change. Called during character switching to enable the target slot before swapping characters into it.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `flag` | int | 1 = enable slot, 0 = disable slot |

**Returns:** unknown (delegated to `.kallis`)

---

## Decompiled Pseudocode

```c
void __stdcall SetSlotState(int flag)
{
    // .kallis thunk — delegates to ROP dispatcher
    off_1C8D810(flag);
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x539CB0` | Entry point (thunk) |
| `off_1C8D810` | `.kallis` indirect call target (slot state logic) |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | `SetSlotState(slot, 1)` at 0x4638F0 --- enables target slot in native path |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `off_1C8D810` | `.kallis` slot state handler | Actual enable/disable logic |

---

## Notes / Caveats

1. **This is a `.kallis` thunk.** The actual slot state management logic runs in the `.kallis` segment. Like other `.kallis` thunks, it uses ROP-style dispatch.

2. **Called in the native switch path of SetPlayerType** with flag=1 to ensure the target slot is active before [SwapSlotChar](sub_539CF0_SwapSlotChar.md) (sub_539CF0) installs the new character.

3. **__stdcall convention** means the callee cleans the stack. This is notable because most engine functions use __cdecl or __thiscall.

4. **Related functions:**
   - [GetPlayerSlot](sub_53A020_GetPlayerSlot.md) (sub_53A020) --- retrieves the slot this operates on
   - [SwapSlotChar](sub_539CF0_SwapSlotChar.md) (sub_539CF0) --- swaps character in the slot
   - [SetPlayerType](sub_463880_SetPlayerType.md) (sub_463880) --- orchestrates the full switch sequence
