# sub_52D9C0 --- VMInterpreter

**Address:** 0x52D9C0 (Spiderwick.exe+12D9C0) | **Size:** ~2810 bytes | **Calling convention:** __cdecl

---

## Purpose

THE main bytecode interpreter for the Kallis VM. Implements a `while(1)` loop with a `switch` on 64 opcodes. Each bytecode instruction is decoded as: low 2 bits = register bank selector, upper 6 bits = operation code. This function is the heart of the Kallis scripting engine.

The interpreter is recursive --- script-to-script calls (opcode 0x01) result in nested invocations of this function.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `bytecodeBase` | void* | Pointer to the start of the bytecode stream |
| `initialOffset` | int | Initial instruction pointer offset (typically 0) |

**Returns:** void

---

## Decompiled Pseudocode (Simplified)

```c
void __cdecl VMInterpreter(void *bytecodeBase, int initialOffset)
{
    int ip = initialOffset;

    while (1)
    {
        unsigned char instruction = bytecodeBase[ip++];
        int regBank  = instruction & 0x03;       // low 2 bits: register bank
        int opcode   = instruction >> 2;          // upper 6 bits: operation

        switch (opcode)
        {
        case 0x00:  // RET — return from function
            return;

        case 0x01:  // CALL_SCRIPT — call another script function
            // Resolve target, save state, recurse into VMInterpreter
            VMInterpreter(targetBytecode, targetOffset);  // recursive!
            break;

        case 0x02:  // CALL_NATIVE — call native (cdecl) function
            sub_52D240(nativeFuncPtr);    // cdecl dispatcher
            break;

        case 0x03:  // CALL_METHOD — call native method on object
            sub_52D280(methodPtr, obj);   // method dispatcher
            break;

        // ... arithmetic, comparison, stack ops ...

        case 0x2E:  // COND_JMP_FALSE — conditional jump (if false)
            if (!condition)
                ip = target;
            break;

        case 0x2F:  // COND_JMP_TRUE — conditional jump (if true)
            if (condition)
                ip = target;
            break;

        case 0x30:  // JMP — unconditional jump
            ip = target;
            break;

        // ... ~60 more opcodes for math, string ops, object access, etc.
        }
    }
}
```

---

## Key Opcodes

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0 | 0x00 | RET | Return from current function |
| 1 | 0x01 | CALL_SCRIPT | Call another script function (recursive) |
| 2 | 0x02 | CALL_NATIVE | Call native __cdecl function |
| 3 | 0x03 | CALL_METHOD | Call native method on object |
| 46 | 0x2E | COND_JMP_F | Conditional jump if false |
| 47 | 0x2F | COND_JMP_T | Conditional jump if true |
| 48 | 0x30 | JMP | Unconditional jump |

### Native Call Dispatchers

| Address | Function | Convention | Description |
|---------|----------|------------|-------------|
| `0x52D240` | NativeCallCdecl | __cdecl | Dispatches native function calls |
| `0x52D280` | NativeCallMethod | __thiscall | Dispatches native method calls on objects |
| `0x52D300` | NativeCallStatic | static | Dispatches static native calls |
| `0x52D340` | NativeCallVirtual | virtual | Dispatches virtual method calls |

---

## Instruction Encoding

```
Byte layout:  [OOOOOO RR]
               ^^^^^^ ^^
               |      |
               |      +-- Register bank (2 bits, 0-3)
               +--------- Operation code (6 bits, 0-63)
```

- **Register bank (bits 0-1):** Selects which register set the instruction operates on. The VM appears to have 4 register banks, possibly for different data types (int, float, object, string).
- **Operation (bits 2-7):** The actual instruction (0-63, giving 64 possible opcodes).

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x52D9C0` | Entry point |
| ~0x52E480 | End of function (~2810 bytes total) |
| `0x52D240` | NativeCallCdecl dispatcher |
| `0x52D280` | NativeCallMethod dispatcher |
| `0x52D300` | NativeCallStatic dispatcher |
| `0x52D340` | NativeCallVirtual dispatcher |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_52EA70` ([VMExecute](sub_52EA70_VMExecute.md)) | Called with `(entry+12, 0)` to begin execution |
| `sub_52D9C0` (self) | Recursive: CALL_SCRIPT opcode causes nested invocation |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x52D240` | NativeCallCdecl | Dispatch native __cdecl calls (opcode 0x02) |
| `0x52D280` | NativeCallMethod | Dispatch native method calls (opcode 0x03) |
| `0x52D300` | NativeCallStatic | Dispatch native static calls |
| `0x52D340` | NativeCallVirtual | Dispatch native virtual calls |
| `0x52D9C0` | VMInterpreter (self) | Recursive call for script-to-script invocations |

---

## Notes / Caveats

1. **2810 bytes is a MASSIVE function** for a game from this era. The switch statement handles 64 opcodes, making this the single most complex function in the engine. Full decompilation of every opcode would be a major undertaking.

2. **The register bank selector (low 2 bits)** is an unusual design. Most bytecode VMs use a flat register file or pure stack machine. The 4-bank approach may optimize for type-specific operations (avoiding runtime type checks in the interpreter loop).

3. **Recursive execution** for CALL_SCRIPT means deeply nested script call chains consume native stack space. There appears to be no explicit recursion depth limit, so pathological scripts could cause a stack overflow.

4. **Native call dispatchers** (sub_52D240, sub_52D280, sub_52D300, sub_52D340) handle the bridge between the VM and native C++ code. They read arguments from the VM stack, convert types as needed, call the native function, and push the return value back onto the VM stack. These are how native handlers like `sauSetPlayerType` get invoked from scripts.

5. **The interpreter is the execution context** for `.kallis` ROP chains. When a native handler thunks to a `.kallis` address, it's because the bytecode's execution flow entered native code (via CALL_NATIVE) which then needs to delegate back to `.kallis` implementation code.

6. **Related functions:**
   - [VMCall](sub_52EB40_VMCall.md) (sub_52EB40) --- high-level entry point
   - [VMCallWithArgs](sub_52EB60_VMCallWithArgs.md) (sub_52EB60) --- entry point with arguments
   - [VMExecute](sub_52EA70_VMExecute.md) (sub_52EA70) --- sets up state then calls this
   - [VMResolveFunction](sub_52D920_VMResolveFunction.md) (sub_52D920) --- resolves function name before execution
   - [PopObjectValidated](sub_52D820_PopObjectValidated.md) (sub_52D820) --- used by native handlers called FROM this interpreter
