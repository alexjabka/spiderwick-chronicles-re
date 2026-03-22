# sub_52D920 --- VMResolveFunction

**Address:** 0x52D920 (Spiderwick.exe+12D920) | **Calling convention:** __cdecl

---

## Purpose

Resolves a script function name to a bytecode entry point. This is a `.kallis` thunk that delegates to `off_1C88E08` for the actual name lookup. On success, sets `dword_E5620C` to the bytecode entry point; on failure, leaves it at 0.

Only finds SCRIPT functions (bytecode). Does NOT resolve native methods or native-registered functions --- those are dispatched directly by the [VMInterpreter](sub_52D9C0_VMInterpreter.md) via separate opcode handlers.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `vmObject` | void* | VM object instance containing the function |
| `functionName` | const char* | Name of the function to resolve (e.g., `"EnterCoop"`) |

**Returns:** void (result is stored in `dword_E5620C`)

---

## Decompiled Pseudocode

```c
void __cdecl VMResolveFunction(void *vmObject, const char *functionName)
{
    // .kallis thunk — delegates to ROP dispatcher
    // Resolves functionName in vmObject's script table
    // Sets dword_E5620C = bytecode entry point (or 0 if not found)
    off_1C88E08(vmObject, functionName);
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x52D920` | Entry point (thunk) |
| `off_1C88E08` | `.kallis` indirect call target (name resolution logic) |
| `dword_E5620C` | Output: resolved bytecode entry point (0 = not found) |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_52EB40` ([VMCall](sub_52EB40_VMCall.md)) | Resolves function before execution |
| `sub_52EB60` ([VMCallWithArgs](sub_52EB60_VMCallWithArgs.md)) | Resolves function before arg push + execution |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `off_1C88E08` | `.kallis` name resolver | Looks up function name in script tables, sets dword_E5620C |

---

## Notes / Caveats

1. **This is a `.kallis` thunk.** The name resolution logic runs in the `.kallis` segment via ROP dispatch. The actual lookup presumably searches the VM object's function table (compiled from script source) for a matching name.

2. **Only resolves SCRIPT functions.** Functions defined in bytecode (compiled from `.sau` or similar script files) are findable. Native functions registered via the native handler table (like `sauSetPlayerType`, `sauSubstitutePlayer`, etc.) are NOT resolvable through this path --- they are dispatched directly by the VMInterpreter's CALL_NATIVE opcode (0x02).

3. **The output `dword_E5620C`** is a global, making this function inherently non-reentrant. Back-to-back calls will overwrite the previous result.

4. **Related functions:**
   - [VMCall](sub_52EB40_VMCall.md) (sub_52EB40) --- calls this then VMExecute
   - [VMCallWithArgs](sub_52EB60_VMCallWithArgs.md) (sub_52EB60) --- calls this then pushes args then VMExecute
   - [VMExecute](sub_52EA70_VMExecute.md) (sub_52EA70) --- consumes dword_E5620C
   - [VMInterpreter](sub_52D9C0_VMInterpreter.md) (sub_52D9C0) --- dispatches native calls via separate opcodes
