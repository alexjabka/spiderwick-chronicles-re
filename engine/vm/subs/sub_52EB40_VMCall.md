# sub_52EB40 --- VMCall

**Address:** 0x52EB40 (Spiderwick.exe+12EB40) | **Calling convention:** __cdecl

---

## Purpose

Primary entry point for calling VM script functions from native code. Takes a VM object and a function name string, resolves the function to bytecode via [VMResolveFunction](sub_52D920_VMResolveFunction.md) (sub_52D920), then executes it via [VMExecute](sub_52EA70_VMExecute.md) (sub_52EA70).

This function is used **hundreds of times** throughout the engine to invoke script callbacks like `"OnAlerted"`, `"ItemWasGiven"`, `"EnterCoop"`, `"ExitCoop"`, etc.

**Critical limitation:** Only resolves SCRIPT (bytecode) functions. Cannot call native methods or native-registered functions.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `vmObject` | void* | VM object instance to call the function on |
| `functionName` | const char* | Name of the script function to call (e.g., `"EnterCoop"`) |

**Returns:** `char` (bool) --- 0 = function not found, 1 = function executed successfully

---

## Decompiled Pseudocode

```c
char __cdecl VMCall(void *vmObject, const char *functionName)
{
    // Resolve function name to bytecode entry point
    // Sets dword_E5620C to bytecode offset if found
    VMResolveFunction(vmObject, functionName);     // sub_52D920

    // Execute the resolved function
    return VMExecute();                            // sub_52EA70
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x52EB40` | Entry point |
| `dword_E5620C` | Set by VMResolveFunction --- bytecode entry point (0 = not found) |

### Known Function Names Called via VMCall

| Name | Context |
|------|---------|
| `"OnAlerted"` | NPC AI state transition |
| `"ItemWasGiven"` | Inventory event callback |
| `"EnterCoop"` | Co-op mode activation (see [CallEnterCoopAll](../../objects/subs/sub_44C3E0_CallEnterCoopAll.md)) |
| `"ExitCoop"` | Co-op mode deactivation |
| `"OnDeath"` | Character death handler |
| `"OnSpawn"` | Character spawn callback |
| (many more) | Script callbacks throughout the engine |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_44C3E0` ([CallEnterCoopAll](../../objects/subs/sub_44C3E0_CallEnterCoopAll.md)) | Calls `"EnterCoop"` on each registered coop object |
| `sub_44C420` (CallExitCoopAll) | Calls `"ExitCoop"` on each registered coop object |
| Hundreds of engine functions | Script callbacks for game events |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x52D920` | [VMResolveFunction](sub_52D920_VMResolveFunction.md) | Resolves function name --> bytecode entry point |
| `0x52EA70` | [VMExecute](sub_52EA70_VMExecute.md) | Executes the resolved bytecode |

---

## Notes / Caveats

1. **Only resolves SCRIPT functions.** VMResolveFunction (sub_52D920) only finds functions defined in `.kallis` bytecode. It will NOT find native methods (registered via the native function table) or native-registered functions. For those, the VM uses opcode dispatchers (CALL_NATIVE, CALL_METHOD) in the [VMInterpreter](sub_52D9C0_VMInterpreter.md).

2. **Thread-unsafe.** VMCall writes to global state (`dword_E5620C`) and the shared VM stack. Multiple simultaneous VMCall invocations will corrupt each other's state.

3. **The return value** reflects whether VMExecute found a valid entry point. If VMResolveFunction fails (function not found), `dword_E5620C` is 0, and VMExecute returns 0 without executing anything.

4. **For calls with arguments**, use [VMCallWithArgs](sub_52EB60_VMCallWithArgs.md) (sub_52EB60) instead, which pushes typed arguments onto the VM stack before execution.

5. **Related functions:**
   - [VMCallWithArgs](sub_52EB60_VMCallWithArgs.md) (sub_52EB60) --- extended version with argument passing
   - [VMResolveFunction](sub_52D920_VMResolveFunction.md) (sub_52D920) --- function name resolution
   - [VMExecute](sub_52EA70_VMExecute.md) (sub_52EA70) --- bytecode execution
   - [VMInterpreter](sub_52D9C0_VMInterpreter.md) (sub_52D9C0) --- the main bytecode interpreter loop
