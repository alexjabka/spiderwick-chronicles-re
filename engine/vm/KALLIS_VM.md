# Kallis VM Architecture

**Status:** FULLY reversed (Session 3: stack ops, ROP dispatchers, function registration, debugger; Session 4: method registration, execution flow, stack push typed, class table; Session 5: complete interpreter with all 64 opcodes, bytecode format, object system, function table, all stack functions)

---

## Overview

The Spiderwick engine uses a custom virtual machine called **Kallis** to drive game logic, mission scripts, and character management. The VM bytecode and supporting native code are stored in a dedicated `.kallis` PE section within `Spiderwick.exe`. The VM is deeply integrated with the engine — many engine features are only accessible through VM thunks, and the VM has its own built-in script debugger.

---

## .kallis PE Section

The `.kallis` section is a non-standard PE section containing:

1. **Function descriptors** — metadata for VM-callable functions
2. **Obfuscated dispatchers** — ROP-style trampolines that resist static analysis
3. **Native code blocks** — real x86 code that CAN be decompiled in IDA
4. **Encrypted bytecode** — VM instructions that cannot be statically analyzed

### Dispatcher Anti-Analysis

The dispatchers use ROP-style chains to obscure control flow:

```asm
push <addr>
sub [esp], <offset>
retn
```

This modifies the return address on the stack before returning, making it difficult for IDA and other tools to follow the control flow. The technique is a deliberate anti-analysis measure.

---

## VM Thunk Pattern

VM functions are called from native code via **thunks** in the `.text` section:

```c
// Typical .text thunk
sub_XXXX(args) {
    return off_YYYY(args);  // off_YYYY points into .kallis
}
```

The thunk is a simple indirect call through a function pointer stored in the `.text` section, where the pointer target resides in `.kallis`. This pattern is used for:

- `DispatchEvent` (0x52EBE0) — event dispatch
- `RegisterScriptFunction` (0x52EA10, 0x52EA30) — function registration
- `LoadWorld` (0x48B460) — world loading
- Many character/object management functions

### Identifying VM Thunks

A function is a VM thunk if:
1. It is very short (just an indirect call + return)
2. The indirect call target (`off_YYYY`) points into the `.kallis` section
3. It passes all arguments through unchanged

---

## VM Stack System

The Kallis VM uses two separate stacks for argument passing and return values.

### Argument Stack

| Global | Purpose |
|--------|---------|
| `dword_E56168` | Arg stack base address |
| `dword_E56200` | Arg stack index (current position) |

**Pop operation:** `value = *(base - 4 * index); index--;`
**Push operation:** `index++; *(base - 4 * index) = value;`

The stack grows downward from the base address — higher index values correspond to lower memory addresses.

### Return Stack

| Global | Purpose |
|--------|---------|
| `dword_E5616C` | Return stack pointer |

**Push result:** `*(ptr) = value; ptr += 4;`

The return stack grows upward. Results are pushed by native handlers after execution.

### Object Reference Base

| Global | Purpose |
|--------|---------|
| `dword_E56160` | Object reference resolution base |

Used by `PopObjRef` (sub_52C860) to resolve object references popped from the argument stack.

### Stack Functions (Native)

All stack manipulation functions are native x86 code (not VM thunks) and can be fully decompiled:

| Address | Name | Purpose |
|---------|------|---------|
| `0x52C610` | PopArg | Pop value from arg stack (thunk to 0x4CE3CD) |
| `0x52C640` | PopArgAlt | Pop value from arg stack (inline variant) |
| `0x52C6A0` | PopFloat | Pop float from arg stack |
| `0x52C700` | PopFloat3 | Pop 3 floats (vec3) from arg stack |
| `0x52C770` | PopVec3 | Wrapper -- pops vec3 into output buffer |
| `0x52C860` | PopObjRef | Pop object reference, resolve via dword_E56160 |
| `0x52CC70` | PushResult | Push bool result to return stack via dword_E5616C |
| `0x52CE40` | PushObjRef | Push object reference to return stack (encodes via dword_E56160) |
| `0x52D7D0` | PopString | Pop string reference from arg stack |
| `0x52D820` | PopVariant | Pop variant value from arg stack |

---

## VM Class/Method Registration System

The Kallis VM maintains a global class table at `dword_E561E0` for method dispatch. Native functions are registered as methods on VM classes via `VMRegisterMethod` (sub_52E660).

### Class Table Layout

```
dword_E561E0 --> class table base:
  [+0x00] DWORD   (flags/reserved)
  [+0x04] DWORD   class_count
  [+0x08] DWORD*  class_ptrs[count]   -- array of pointers to class descriptors
```

### Class Descriptor

```
class[+0x00] DWORD  name_hash_1    -- primary name hash
class[+0x04] DWORD  name_hash_2    -- secondary name hash
class[+0x1C] DWORD  method_table   -- pointer to method table
```

### Method Table

```
method_table[+0x00] DWORD  (reserved)
method_table[+0x04] DWORD  method_count
method_table[+0x08] ...    (alignment)

Methods at +8, each 12 bytes:
  [+0x00] DWORD  method_name_hash
  [+0x04] DWORD  (flags)
  [+0x08] DWORD  func_addr (-1 if unregistered)
```

Registration writes the native function address to `func_addr`, replacing the -1 sentinel. Attempting to re-register prints: `"{yWARNING}: Reregistration of the method "%s::%s"."`.

See [subs/sub_52E660_VMRegisterMethod.md](subs/sub_52E660_VMRegisterMethod.md) for full analysis.

---

## Script Debugger

The Kallis VM has a built-in **remote script debugger**.

### Enable Flag

```c
*(dword_E561D8 + 0x2E) & 1  // bit 0 of byte at offset +0x2E
```

- `dword_E561D8` = pointer to the script system state object
- When bit 0 of `[dword_E561D8 + 0x2E]` is set, the debugger is enabled

### Behavior When Enabled

1. Prints: `"Waiting for remote script debugger to connect (Press 's' to skip)"`
2. Then: `"Hit Run or Step in the script debugger to begin execution (Press 's' to skip)"`
3. Blocks execution until debugger connects or user presses 's'

### String References

| Address | String |
|---------|--------|
| `0x63EF28` | "Waiting for remote script debugger to connect (Press 's' to skip)" |
| `0x63EEC0` | "Hit Run or Step in the script debugger to begin execution (Press 's' to skip)" |

Referenced from `.kallis` at `0x20FC0A2`.

### Check Code (0x20FC072 — 0x20FC0BF)

```asm
test al, al          ; prior condition check
jz   skip
mov  eax, dword_E561D8
test byte ptr [eax+2Eh], 1   ; debugger enable bit
jz   skip
; ... debugger wait loop ...
```

### Protocol

The debugger protocol is **unknown**. No TCP/IP socket or named pipe references were found near the debugger code. It may use:
- File-based communication (shared files)
- Shared memory / memory-mapped files
- A proprietary IPC mechanism

Further investigation needed.

---

## Script Function Registration

Two registration thunks exist:

| Address | Name | Type |
|---------|------|------|
| `0x52EA10` | RegisterScriptFunction | VM thunk |
| `0x52EA30` | RegisterScriptFunction (variant) | VM thunk |

### Registration Sources

1. **Native-registered** — called from native `.text` code (e.g., `RegisterTimeScriptFunctions` at 0x498180 registers `sauSetClockSpeed`, `sauPauseGame`, etc.). These handlers are accessible native functions.

2. **VM-registered** — called from `.kallis` bytecode (e.g., `sauCreateCharacter`, `sauSpawnObj`). The registration call itself happens inside the VM, but the handlers may be native code blocks within `.kallis`.

Both use the same registration mechanism — the difference is only in where the registration call originates.

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0xE56168` | dword | g_VMArgStackBase | VM argument stack base address |
| `0xE56200` | dword | g_VMArgStackIndex | VM argument stack current index |
| `0xE5616C` | dword | g_VMReturnStackPtr | VM return stack pointer |
| `0xE56160` | dword | g_VMObjectRefBase | Object reference resolution base |
| `0xE561D8` | dword* | g_ScriptSystemState | Pointer to script system state object |
| `0xE561E0` | dword | g_VMClassTable | VM class table base (class registration) |
| `0xE561F8` | dword | g_ScriptsLoaded | Scripts-loaded flag (0 = no scripts) |
| `0xE5620C` | dword | g_VMResolvedEntry | Resolved bytecode entry point for VMExecute |
| `0xE56208` | dword | g_VMExecState | Execution state flag |
| `0xE56210` | dword | g_VMDepth | VM recursion depth flag |
| `0x713118` | dword | g_VMExecActive | Execution active flag |

---

## Function Reference

### Core Interpreter

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x52D9C0` | VMInterpreter | native | Main bytecode interpreter (64 opcodes, 2810 bytes) |
| `0x52EA70` | VMExecute | native | Execution driver (saves state, calls interpreter) |
| `0x52EB40` | VMCall | native | Call VM function by name (no args) |
| `0x52EB60` | VMCallWithArgs | native | Call VM function with typed arguments |
| `0x52D920` | VMResolveFunction | VM thunk | Resolve function name to bytecode address |
| `0x52EC30` | VMInitObject | native | Initialize VM object with native counterpart |

### Native Call Dispatchers (called by interpreter)

| Address | Name | Purpose |
|---------|------|---------|
| `0x52D240` | CallNativeCdecl | Call global native function (opcode 0x02) |
| `0x52D280` | CallNativeMethod | Call native method via vtable (opcode 0x03) |
| `0x52D300` | CallNativeStatic | Call static native with frame (opcode 0x04) |
| `0x52D340` | CallNativeVirtual | Call virtual native with frame (opcode 0x05) |
| `0x52D370` | CallObjMethod | Call object method with bool return (opcode 0x3E) |
| `0x52D1F0` | ReturnHandler | Pop args, copy returns, restore frame (opcode 0x00) |

### Stack Pop Functions (Arg Stack -> Native)

| Address | Name | Type | Purpose |
|---------|------|------|---------|
| `0x52C610` | VMPopArg | int | Pop int (thunk to 0x4CE3CD) |
| `0x52C640` | VMPopArgAlt | int | Pop int (inline variant) |
| `0x52C670` | VMPopInt | int | Pop int (another variant) |
| `0x52C6A0` | VMPopFloat | float | Pop float |
| `0x52C6D0` | VMPopBool | bool | Pop bool (converts to true/false) |
| `0x52C700` | VMPopFloat3 | vec3 | Pop 3 consecutive floats |
| `0x52C770` | VMPopVec3 | vec3 | Pop vec3 (wrapper) |
| `0x52C7A0` | VMPopFloat4 | vec4 | Pop 4 consecutive floats |
| `0x52C830` | VMPopVec4 | vec4 | Pop vec4 (wrapper) |
| `0x52C860` | VMPopObjRef | objref | Pop + resolve object reference |
| `0x52D7D0` | VMPopString | string | Pop string ref (thunk to .kallis) |
| `0x52D820` | VMPopObject | object | Pop + validate object with class chain walk |

### Stack Push Functions (Native -> Eval Stack)

| Address | Name | Type | Guard | Purpose |
|---------|------|------|-------|---------|
| `0x52CA80` | VMReturnBoolByte | bool | Yes | Push bool (guarded) |
| `0x52CAB0` | VMReturnVec3 | vec3 | Yes | Push vec3 (guarded) |
| `0x52CB00` | VMReturnVec3Int | vec3 | Yes | Push vec3 from internal format (guarded) |
| `0x52CB40` | VMReturnVec4 | vec4 | Yes | Push vec4 (guarded) |
| `0x52CBA0` | VMReturnVec4Int | vec4 | Yes | Push vec4 from internal format (guarded) |
| `0x52CBF0` | VMReturnString | string | No | Push string (thunk to .kallis) |
| `0x52CC30` | VMReturnInt | int | No | Push int |
| `0x52CC50` | VMReturnFloat | float | No | Push float |
| `0x52CC70` | VMReturnBool | bool | No | Push bool (unguarded) |
| `0x52CCD0` | VMReturnVec3Fmt | vec3 | No | Push vec3 from sub_5AD550 |
| `0x52CD80` | VMReturnVec4Fmt | vec4 | No | Push vec4 from sub_5AD570 |
| `0x52CE00` | VMReturnObjDirect | objref | No | Push obj ref (raw ptr) |
| `0x52CE40` | VMReturnObj | objref | No | Push obj ref (via +0xA8) |
| `0x52D5A0` | VMStackPush | typed | No | Push typed VMArg (9-type switch) |

### Registration

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x52E660` | VMRegisterMethod | native | Register native function as method on VM class |
| `0x52EA10` | RegisterScriptFunction | VM thunk | Register native function for script access |
| `0x52EA30` | RegisterScriptFunction2 | VM thunk | Registration variant (coroutine) |
| `0x52EBE0` | DispatchEvent | VM thunk | Dispatch named event to script handlers |

### Script Loading

| Address | Name | Purpose |
|---------|------|---------|
| `0x52C2A0` | ScriptLoader | Load SCT file for Edit and Continue |
| `0x52C420` | VMStateInit | Initialize all VM globals to defaults |
| `0x52D450` | VMStateReset | Reset scratch state (for debugger disconnect) |
| `0x52C550` | SaveStateAlloc | Allocate save state buffer |

---

## Related Documentation

### Deep-Dive Topic Docs
- [VM_INTERPRETER.md](VM_INTERPRETER.md) -- **Full opcode table** (all 64 opcodes with encoding, behavior, pseudocode)
- [VM_STACK_SYSTEM.md](VM_STACK_SYSTEM.md) -- Complete stack system (all pop/push functions, encoding, types)
- [VM_FUNCTION_TABLE.md](VM_FUNCTION_TABLE.md) -- Function table, class table, SCT format, name hash algorithm
- [VM_OBJECT_SYSTEM.md](VM_OBJECT_SYSTEM.md) -- Object lifecycle, bidirectional links, reference encoding
- [VM_BYTECODE_FORMAT.md](VM_BYTECODE_FORMAT.md) -- SCT file structure, instruction encoding, memory layout
- [VM_FUNCTION_REGISTRATION.md](VM_FUNCTION_REGISTRATION.md) -- Function registration patterns
- [VM_ROP_DISPATCHERS.md](VM_ROP_DISPATCHERS.md) -- ROP dispatch mechanism
- [VM_DISPATCH_RESEARCH.md](VM_DISPATCH_RESEARCH.md) -- Original dispatch research notes
- [SCRIPT_DEBUGGER.md](SCRIPT_DEBUGGER.md) -- Script debugger findings
- [VM_STACK_OPERATIONS.md](VM_STACK_OPERATIONS.md) -- Original stack ops notes

### Sub Docs (Interpreter Internals)
- [subs/sub_52D1F0_VMReturnHandler.md](subs/sub_52D1F0_VMReturnHandler.md) -- VM return/cleanup handler
- [subs/sub_52D240_CallNativeCdecl.md](subs/sub_52D240_CallNativeCdecl.md) -- Call native cdecl (opcode 0x02)
- [subs/sub_52D280_CallNativeMethod.md](subs/sub_52D280_CallNativeMethod.md) -- Call native method (opcode 0x03)
- [subs/sub_52D300_CallNativeStatic.md](subs/sub_52D300_CallNativeStatic.md) -- Call static native (opcode 0x04)
- [subs/sub_52D340_CallNativeVirtual.md](subs/sub_52D340_CallNativeVirtual.md) -- Call virtual native (opcode 0x05)
- [subs/sub_52D370_CallObjMethod.md](subs/sub_52D370_CallObjMethod.md) -- Call object method with bool (opcode 0x3E)

### Sub Docs (Stack Pop Functions)
- [subs/sub_52C610_PopArg.md](subs/sub_52C610_PopArg.md) -- PopArg
- [subs/sub_52C640_PopArgAlt.md](subs/sub_52C640_PopArgAlt.md) -- PopArgAlt
- [subs/sub_52C6A0_PopFloat.md](subs/sub_52C6A0_PopFloat.md) -- PopFloat
- [subs/sub_52C700_PopFloat3.md](subs/sub_52C700_PopFloat3.md) -- PopFloat3
- [subs/sub_52C770_PopVec3.md](subs/sub_52C770_PopVec3.md) -- PopVec3
- [subs/sub_52C860_PopObjRef.md](subs/sub_52C860_PopObjRef.md) -- PopObjRef
- [subs/sub_52D7D0_PopString.md](subs/sub_52D7D0_PopString.md) -- PopString
- [subs/sub_52D820_PopVariant.md](subs/sub_52D820_PopVariant.md) -- PopVariant

### Sub Docs (Stack Push Functions)
- [subs/sub_52CA80_VMReturnBoolByte.md](subs/sub_52CA80_VMReturnBoolByte.md) -- VMReturnBoolByte (guarded bool push)
- [subs/sub_52CBF0_VMReturnString.md](subs/sub_52CBF0_VMReturnString.md) -- VMReturnString (thunk to .kallis)
- [subs/sub_52CC30_VMReturnInt.md](subs/sub_52CC30_VMReturnInt.md) -- VMReturnInt
- [subs/sub_52CC50_VMReturnFloat.md](subs/sub_52CC50_VMReturnFloat.md) -- VMReturnFloat
- [subs/sub_52CC70_PushResult.md](subs/sub_52CC70_PushResult.md) -- VMReturnBool (unguarded)
- [subs/sub_52CE00_VMReturnObjDirect.md](subs/sub_52CE00_VMReturnObjDirect.md) -- VMReturnObjDirect
- [subs/sub_52CE40_PushObjRef.md](subs/sub_52CE40_PushObjRef.md) -- VMReturnObj (via +0xA8)
- [subs/sub_52D5A0_VMStackPush.md](subs/sub_52D5A0_VMStackPush.md) -- VMStackPush (9-type switch)

### Sub Docs (Registration & Registration)
- [subs/sub_52E660_VMRegisterMethod.md](subs/sub_52E660_VMRegisterMethod.md) -- VMRegisterMethod (class+method hash, table write)
- [subs/sub_52D820_VMStackPop.md](subs/sub_52D820_VMStackPop.md) -- VMStackPop (validated object pop with class chain walk)
- [subs/sub_52EA70_VMExecute.md](subs/sub_52EA70_VMExecute.md) -- VMExecute (execution driver)
- [subs/sub_52EB40_VMCall.md](subs/sub_52EB40_VMCall.md) -- VMCall (no args)
- [subs/sub_52EB60_VMCallWithArgs.md](subs/sub_52EB60_VMCallWithArgs.md) -- VMCallWithArgs

### Sub Docs (Initialization & Loading)
- [subs/sub_52C2A0_ScriptLoader.md](subs/sub_52C2A0_ScriptLoader.md) -- Script file loader (SCT format)
- [subs/sub_52EC30_VMInitObject.md](subs/sub_52EC30_VMInitObject.md) -- VM object initialization
- [subs/sub_52D920_VMResolveFunction.md](subs/sub_52D920_VMResolveFunction.md) -- Function name resolution

### Cross-References
- [../events/EVENT_SYSTEM.md](../events/EVENT_SYSTEM.md) -- Event dispatch system
- [../objects/CHARACTER_CREATION.md](../objects/CHARACTER_CREATION.md) -- Character creation (uses VM stack)
- [../objects/HOT_SWITCH_SYSTEM.md](../objects/HOT_SWITCH_SYSTEM.md) -- Hot-switch system (uses VM stack and thunks)
