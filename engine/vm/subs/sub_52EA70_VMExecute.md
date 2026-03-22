# sub_52EA70 --- VMExecute

**Address:** 0x52EA70 (Spiderwick.exe+12EA70) | **Calling convention:** (none --- no parameters)

---

## Purpose

Executes a previously resolved VM script function. Checks that scripts are loaded, saves the current VM state, sets up a stack frame, and if a valid bytecode entry point exists in `dword_E5620C`, invokes the [VMInterpreter](sub_52D9C0_VMInterpreter.md) (sub_52D9C0) to execute it. Restores VM state after execution completes.

This function is the execution half of the resolve-then-execute pattern used by [VMCall](sub_52EB40_VMCall.md) and [VMCallWithArgs](sub_52EB60_VMCallWithArgs.md).

---

## Parameters

None.

**Returns:** `char` (bool) --- 0 = no function was resolved (dword_E5620C == 0), 1 = execution completed

---

## Decompiled Pseudocode

```c
char VMExecute(void)
{
    // Check: are scripts loaded?
    if (!dword_E561F8)
        return 1;  // returns 1 (not 0) when scripts not loaded

    // Save 16 bytes of VM context from dword_E56170
    BYTE savedContext[16];
    memcpy(savedContext, &unk_E56170, 16);  // sub_5ADF00

    int resolvedEntry = dword_E5620C;

    // Clear execution state
    dword_E56208 = 0;
    dword_E5620C = 0;        // clear resolved entry
    dword_713118 = 1;         // execution flag

    if (resolvedEntry == 0)
    {
        // No function resolved: restore context and return 0
        memcpy(dword_E56160, savedContext, 16);
        return 0;
    }

    // Save and set VM depth flag
    int savedDepth = dword_E56210;
    dword_E56210 = 1;

    // Set up return stack: push current arg stack base, then advance
    *(DWORD*)dword_E5616C = dword_E56168;  // save arg stack base
    dword_E56168 = dword_E5616C;           // new arg base = return stack ptr
    dword_E5616C += 4;                      // advance return stack

    // Execute bytecode via interpreter
    sub_52D9C0(resolvedEntry + 12, 0);     // VMInterpreter(bytecodeBase, 0)

    // Restore VM context
    memcpy(dword_E56160, savedContext, 16);
    dword_E56210 = savedDepth;

    return 1;
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x52EA70` | Entry point |
| `dword_E561F8` | Scripts-loaded flag (0 = no scripts, skip execution) |
| `dword_E5620C` | Resolved bytecode entry point (0 = nothing resolved, set by VMResolveFunction) |
| `dword_E56208` | Execution state flag (cleared to 0 before execution) |
| `dword_E56210` | VM depth/recursion flag (saved and set to 1 during execution) |
| `dword_E56168` | VM arg stack base address (swapped during execution) |
| `dword_E5616C` | VM return stack pointer (advanced during execution) |
| `dword_713118` | Execution active flag (set to 1) |
| `unk_E56170` | 16-byte VM context block (saved/restored around execution) |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_52EB40` ([VMCall](sub_52EB40_VMCall.md)) | After VMResolveFunction |
| `sub_52EB60` ([VMCallWithArgs](sub_52EB60_VMCallWithArgs.md)) | After VMResolveFunction + argument push |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x52D9C0` | [VMInterpreter](sub_52D9C0_VMInterpreter.md) | Main bytecode interpreter (called with entry+12, 0) |

---

## Notes / Caveats

1. **State save/restore is critical.** VMExecute saves and restores the VM's internal state (stack pointer, frame pointer, instruction pointer, etc.) around execution. This allows nested VMCall invocations --- a script function called via VMCall can itself trigger native code that calls VMCall again.

2. **The `entry + 12` offset** suggests the function entry structure has a 12-byte header (likely containing metadata like argument count, local count, etc.) before the actual bytecode begins.

3. **`dword_E561F8` guard** prevents execution when scripts haven't been loaded yet (e.g., during early engine initialization). This is a safety check, not a normal control flow path.

4. **Returns 0 if nothing was resolved**, which is how VMCall's callers detect "function not found" vs "function executed." Many callers ignore this return value (fire-and-forget callbacks).

5. **Related functions:**
   - [VMCall](sub_52EB40_VMCall.md) (sub_52EB40) --- primary caller
   - [VMCallWithArgs](sub_52EB60_VMCallWithArgs.md) (sub_52EB60) --- primary caller with args
   - [VMResolveFunction](sub_52D920_VMResolveFunction.md) (sub_52D920) --- sets dword_E5620C before this is called
   - [VMInterpreter](sub_52D9C0_VMInterpreter.md) (sub_52D9C0) --- the actual bytecode execution loop
