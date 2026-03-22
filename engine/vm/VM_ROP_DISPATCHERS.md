# VM ROP Dispatchers

**Status:** Confirmed (Session 3)

---

## Overview

The `.kallis` PE section uses **ROP-style dispatch chains** to obfuscate control flow. These chains manipulate the stack and use `retn` instructions to transfer control, making static analysis extremely difficult. Understanding the difference between ROP chains and simple thunks is critical for reverse engineering this binary.

---

## ROP Dispatch Pattern

The canonical ROP dispatcher pattern:

```asm
push    <target_addr>           ; address or data
push    <value>                 ; intermediate value
push    <encrypted_target>      ; encrypted jump target
pushf                           ; save flags
sub     [esp+offset], value     ; decrypt: modify stack value
popf                            ; restore flags
retn                            ; jump to decrypted address
```

The `sub [esp+offset], value` instruction modifies a previously-pushed stack value, effectively decrypting the real dispatch target. The `retn` then pops this decrypted address and jumps to it. The `pushf`/`popf` pair preserves the CPU flags across the subtraction so it doesn't corrupt the execution state.

### Example: sub_463880 at 0x1CD7430

```asm
1CD7430  push    offset off_1CD744A      ; continuation data
1CD7435  push    401FF6h                 ; intermediate value
1CD743A  push    offset dword_1419690    ; encrypted target
1CD743F  pushf                           ; save flags
1CD7440  sub     [esp+1Ch-18h], 1F000h   ; decrypt stack value
1CD7448  popf                            ; restore flags
1CD7449  retn                            ; jump to decrypted address
```

The `sub` modifies the value at `[esp+4]` (the `401FF6h` that was pushed second). After subtraction of `0x1F000`, the effective jump target becomes `401FF6h - 1F000h = 3E2FF6h`. The `retn` pops whatever is at the current stack top (which after pushf/popf manipulation is the decrypted address) and jumps to it.

---

## Why ROP Chains Cannot Be Called From Native Code

ROP dispatch chains expect a **very specific stack layout** established by the VM dispatch loop. When entering a ROP chain:

1. The stack contains VM state (return addresses, frame pointers, VM context)
2. The chain manipulates specific stack offsets that correspond to VM-managed values
3. The `retn` at the end jumps to an address computed from VM stack contents

If you call a ROP chain directly from native C code:
- The stack layout is wrong (C calling convention, not VM layout)
- The `sub [esp+offset]` modifies the wrong data (your return address or locals)
- The `retn` jumps to garbage
- **Crash or undefined behavior**

This is a deliberate anti-analysis and anti-tampering measure.

---

## Simple .kallis Thunks (Callable)

In contrast to ROP chains, simple `.kallis` thunks ARE callable from native code. These are short `.text` section functions that use `jmp ds:off_XXXX` to transfer control through a function pointer in the `.kallis` section.

### Example: sub_55C7B0

```asm
55C7B0  mov     eax, [esp+arg_4]
55C7B4  mov     ecx, [esp+arg_0]
55C7B8  jmp     ds:off_1C82C34      ; indirect jump to .kallis entry point
```

This sets up arguments in registers, then performs a standard indirect jump. The `.kallis` entry point at `off_1C82C34` eventually calls back into native code. The stack layout is a normal C/thiscall convention.

### Example: sub_438E70

```asm
438E70  jmp     ds:off_1C899E8      ; simple indirect jump to .kallis
```

A single-instruction thunk -- just an indirect jump. Fully compatible with normal calling conventions.

---

## Critical Difference: Thunks vs. ROP Chains

| Property | Simple Thunk | ROP Chain |
|----------|-------------|-----------|
| Location | `.text` section | `.kallis` section |
| Pattern | `jmp ds:off_XXXX` | `push/push/pushf/sub/popf/retn` |
| Callable from native | YES | NO |
| Stack expectation | Normal C/thiscall | VM dispatch loop layout |
| Static analysis | Easy (IDA follows jump) | Difficult (encrypted targets) |
| Purpose | Bridge native <-> VM | Internal VM control flow |

### Rule of Thumb

- If a function is in `.text` and does `jmp ds:off_XXXX` where the target is in `.kallis` -- it is a **thunk** and is safe to call.
- If code is in `.kallis` and uses `push/push/pushf/sub/popf/retn` -- it is an **ROP chain** and must NOT be called from native code.

---

## Encrypted Dispatch Variants

Not all ROP chains use the exact same pattern. Observed variants include:

1. **Sub-based decryption** (most common): `sub [esp+offset], imm` to decrypt target
2. **Add-based decryption**: `add [esp+offset], imm` (same principle, opposite direction)
3. **XOR-based decryption**: theoretically possible but not yet confirmed
4. **Chained dispatchers**: one ROP chain's `retn` lands in another ROP chain, forming multi-hop dispatch

The `.kallis` section contains thousands of these dispatchers, forming a dense web of obfuscated control flow that implements the VM's instruction dispatch loop.

---

## Implications for Modding

- **Cannot hook ROP chains directly** -- they don't have stable entry points
- **Can hook thunks** -- the `.text` section thunks have stable addresses and normal calling conventions
- **Can hook handlers** -- the native handler functions (called after the VM dispatches) are normal functions
- **Script debugger wait loop is in ROP code** -- cannot be patched, must be disabled via the flag (see [SCRIPT_DEBUGGER.md](SCRIPT_DEBUGGER.md))

---

## Key Addresses

| Address | Type | Description |
|---------|------|-------------|
| `0x1CD7430` | .kallis ROP | Example ROP dispatcher (sub_463880 entry) |
| `0x55C7B0` | .text thunk | Example callable thunk (2-arg, jmp to .kallis) |
| `0x438E70` | .text thunk | Example callable thunk (1-instruction jmp) |
| `0x52EA10` | .text thunk | RegisterScriptFunction (callable VM thunk) |
| `0x52EBE0` | .text thunk | DispatchEvent (callable VM thunk) |

---

## Related

- [KALLIS_VM.md](KALLIS_VM.md) -- VM architecture overview
- [VM_FUNCTION_REGISTRATION.md](VM_FUNCTION_REGISTRATION.md) -- Registration uses ROP chains in .kallis
- [SCRIPT_DEBUGGER.md](SCRIPT_DEBUGGER.md) -- Wait loop is ROP code (unpatchable)
# Kallis VM Dispatch Research

## Overview

The Kallis VM is a bytecode interpreter embedded in the `.kallis` PE section. It executes
scripts compiled to "SCT" format (version 13) with a stack-based architecture. Native code
can call VM functions through well-defined entry points in the `.text` section.

---

## 1. ROP Chain Trace from 0x1CD7430

### The VM Switch Function (sub_463880 -> off_1C867CC -> 0x1CD7430)

The address `0x1CD7430` is where sub_463880's `off_1C867CC` thunk resolves. IDA labels it as
being inside sub_463880 but it's physically in `.kallis`.

**Disassembly at 0x1CD7430:**
```
1cd7430  push    offset 0x1CD744A       ; next ROP address
1cd7435  push    0x401FF6               ; encrypted return address
1cd743a  push    offset 0x1419690       ; encrypted target
1cd743f  pushf                           ; save flags
1cd7440  sub     [esp+4], 0x1F000       ; decrypt: 0x1419690 - 0x1F000 = 0x13FA690
1cd7448  popf                            ; restore flags
1cd7449  retn                            ; jump to 0x13FA690
```

**Stack after pushes (top to bottom):**
- [esp+0]: EFLAGS
- [esp+4]: 0x01419690 (will be decrypted)
- [esp+8]: 0x00401FF6
- [esp+C]: 0x01CD744A

**After `sub [esp+4], 0x1F000`:** Value at [esp+4] = 0x013FA690

**Hop 1: retn -> 0x013FA690**
- This is a simple thunk: `jmp ds:off_1A561F4`
- off_1A561F4 contains 0x01E4B000
- Stack still has 0x401FF6, 0x1CD744A pending

**Hop 2: jmp -> 0x1E4B000 (.kallis anti-tamper stub)**
```
1e4b000  jmp short +0x12          ; skip embedded string "< space for rent >"
1e4b014  pusha
1e4b015  pushf
1e4b016  call $+5                  ; push EIP
1e4b01b  call sub_1E4B022
```
At 0x1E4B022: `pop edx` then a spinlock pattern (`lock dec byte [edx+0x79]` / `cmp [edx], 0` / `pause` / `jle`).

This is a **self-modifying code / anti-tamper spinlock**. The .kallis VM stubs use runtime
decryption -- the actual code is encrypted in the binary and decrypted in place when needed.
Static analysis cannot follow these chains beyond the first hop.

**Conclusion on ROP tracing:** The ROP chains in .kallis are designed to resist static
analysis. Each hop decrypts the next target address, and the deeper stubs use self-modifying
code with spinlocks. **These chains are NOT meant to be called directly from native code.**

### What off_1C867CC Actually Does

In sub_463880, `off_1C867CC(a2)` is called when certain conditions are met (the current
level has scripts loaded, and a specific check passes). The pointer at 0x1C867CC resolves to
0x1CDBA30, which is encrypted data in .kallis. At runtime, this would be decrypted to a
function that handles level transitions or teleportation.

---

## 2. The VM Bytecode Interpreter (THE DISPATCH LOOP)

### Location: sub_52D9C0 at 0x52D9C0 (size: 0xAFA / 2,810 bytes)

**This is the main VM interpreter.** It is a large function with a `while(1)` loop containing
a switch statement that dispatches on bytecode opcodes.

### Signature
```c
int* __cdecl sub_52D9C0(int bytecodeBase, int initialOffset)
```

### How It Works

1. `a1` = base address of bytecode stream
2. `a2` = initial offset into the stream (usually 0)
3. The loop reads bytes at `bytecodeBase + offset`, decodes the opcode, executes it
4. The low 2 bits of the opcode byte select a "register bank" (v6 = byte & 3)
5. The upper 6 bits select the operation (byte >> 2 = opcode 0x00-0x3F)

### VM State Globals

| Address    | Name           | Purpose                                              |
|------------|----------------|------------------------------------------------------|
| 0xE56160   | dword_E56160   | Object base pointer array (4 entries, indexed by v6) |
| 0xE56164   | dword_E56164   | Current frame/context pointer                        |
| 0xE56168   | dword_E56168   | VM arg stack base (linked list of frames)            |
| 0xE5616C   | dword_E5616C   | VM stack top pointer (grows upward)                  |
| 0xE561CC   | dword_E561CC   | Stack depth counter                                  |
| 0xE561D0   | dword_E561D0   | Object lookup function pointer                       |
| 0xE561D4   | dword_E561D4   | Object dispatch/init function pointer                |
| 0xE561D8   | dword_E561D8   | Script header pointer                                |
| 0xE561DC   | dword_E561DC   | String table base                                    |
| 0xE561E0   | dword_E561E0   | Class/type table base                                |
| 0xE561E4   | dword_E561E4   | String resolve table (for debug names)               |
| 0xE561E8   | dword_E561E8   | Type list pointer                                    |
| 0xE561EC   | dword_E561EC   | Save state data                                      |
| 0xE561F0   | dword_E561F0   | Save state flag                                      |
| 0xE561F4   | dword_E561F4   | Save state buffer                                    |
| 0xE561F8   | dword_E561F8   | Script loaded flag (0 = no script)                   |
| 0xE561FC   | dword_E561FC   | Function table base (bytecode entry points)          |
| 0xE56200   | dword_E56200   | VM arg count / current arg index                     |
| 0xE56204   | dword_E56204   | Current native function pointer                      |
| 0xE56208   | dword_E56208   | Pending execution flag                               |
| 0xE5620C   | dword_E5620C   | Resolved bytecode entry (set by sub_52D920)          |
| 0xE56210   | dword_E56210   | Execution active flag                                |
| 0xE56218   | dword_E56218   | Debug/trace value                                    |

### Opcode Table (byte >> 2)

| Opcode | Mnemonic     | Description                                          |
|--------|-------------|------------------------------------------------------|
| 0x00   | RET         | Return from VM function (sub_52D1F0)                 |
| 0x01   | CALL_SCRIPT | Call VM script function (recursive sub_52D9C0)       |
| 0x02   | CALL_NATIVE | Call native function (sub_52D240 - cdecl)            |
| 0x03   | CALL_METHOD | Call native method on vtable object (sub_52D280)     |
| 0x04   | CALL_STATIC | Call native static method with frame (sub_52D300)    |
| 0x05   | CALL_VIRT   | Call native virtual method with frame (sub_52D340)   |
| 0x06   | POP_N       | Pop N values from stack                              |
| 0x07   | PUSH_N      | Reserve N slots on stack                             |
| 0x08   | PUSH_FRAME  | Push current frame offset                            |
| 0x09   | SAVE_STATE  | Save execution state (for coroutines)                |
| 0x0A   | COPY_TO_STK | Copy data to stack                                   |
| 0x0B   | COPY_FROM   | Copy data from stack to memory                       |
| 0x0C   | PUSH_IMM    | Push 4-byte immediate value                          |
| 0x0D   | LOAD_OFFSET | Load from base + offset + stack top                  |
| 0x0E   | STORE_OFF   | Store to base + offset + stack top                   |
| 0x0F   | ADD_I       | Integer add                                          |
| 0x10   | SUB_I       | Integer subtract                                     |
| 0x11   | MUL_I       | Integer multiply                                     |
| 0x12   | DIV_I       | Integer divide                                       |
| 0x13   | MOD_I       | Integer modulo                                       |
| 0x14   | NEG_I       | Integer negate                                       |
| 0x15   | INC_I       | Integer increment                                    |
| 0x16   | DEC_I       | Integer decrement                                    |
| 0x17   | ADD_F       | Float add                                            |
| 0x18   | SUB_F       | Float subtract                                       |
| 0x19   | MUL_F       | Float multiply                                       |
| 0x1A   | DIV_F       | Float divide                                         |
| 0x1C   | NEG_F       | Float negate                                         |
| 0x1D   | INC_F       | Float increment (+1.0)                               |
| 0x1E   | DEC_F       | Float decrement (-1.0)                               |
| 0x1F   | NOT         | Logical NOT (result = value == 0)                    |
| 0x20   | AND         | Logical AND                                          |
| 0x21   | OR          | Logical OR                                           |
| 0x22   | NEQ_I       | Integer not-equal                                    |
| 0x23   | EQ_I        | Integer equal                                        |
| 0x24   | LT_I        | Integer less-than                                    |
| 0x25   | LE_I        | Integer less-or-equal                                |
| 0x26   | GT_I        | Integer greater-than                                 |
| 0x27   | GE_I        | Integer greater-or-equal                             |
| 0x28   | NEQ_F       | Float not-equal                                      |
| 0x29   | EQ_F        | Float equal                                          |
| 0x2A   | LT_F        | Float less-than                                      |
| 0x2B   | LE_F        | Float less-or-equal                                  |
| 0x2C   | GT_F        | Float greater-than                                   |
| 0x2D   | GE_F        | Float greater-or-equal                               |
| 0x2E   | JNZ         | Jump if non-zero (conditional branch)                |
| 0x2F   | JZ          | Jump if zero (conditional branch)                    |
| 0x30   | JMP         | Unconditional jump                                   |
| 0x31   | SET_EXCEPT  | Set exception handler                                |
| 0x32   | F2I         | Float to int conversion                              |
| 0x33   | I2F         | Int to float conversion                              |
| 0x34   | ADDR_CALC   | Address calculation (base + offset)                  |
| 0x35   | SET_DEBUG   | Set debug/trace value (dword_E56218)                 |
| 0x36   | NOP5        | 5-byte NOP (skip 5 bytes)                            |
| 0x37   | NOP1        | 1-byte NOP                                           |
| 0x38   | BREAKPOINT  | Debugger breakpoint (sub_4E0E10)                     |
| 0x39   | TRACE       | Debug trace call (sub_52C0A0)                        |
| 0x3A   | LOAD        | Load dword from address                              |
| 0x3B   | STORE       | Store dword to address                               |
| 0x3C   | LOAD_IND    | Load indirect (address from stack + offset)          |
| 0x3D   | STORE_IND   | Store indirect (address from stack + offset)         |
| 0x3E   | CALL_OBJ    | Call object method with result (sub_52D370)          |
| 0x3F   | NOP5b       | Another 5-byte NOP                                   |

### Native Call Dispatchers (called from interpreter)

**sub_52D240 (0x52D240) - Call Native Function (cdecl)**
```c
int __cdecl sub_52D240(void (*funcPtr)(void), int argCount)
```
- Saves dword_E56200 = argCount, dword_E56204 = funcPtr
- Calls funcPtr() directly
- Returns (new_stack_top - old_stack_top) / 4 (number of return values)

**sub_52D280 (0x52D280) - Call Native Method (thiscall via vtable)**
```c
int __cdecl sub_52D280(vtable** obj, int argCount)
```
- Gets "this" object from VM stack
- Resolves native counterpart via obj+16 pointer
- Calls (**obj)(obj, nativeObj) through vtable
- Error: "Object type had no native counterpart: %s"

**sub_52D300 (0x52D300) - Call Static Native Method**
```c
int __cdecl sub_52D300(void (*funcPtr)(int), int framePtr, int argCount)
```
- Calls funcPtr(*(framePtr + 16)) -- passes native object pointer

**sub_52D340 (0x52D340) - Call Virtual Native Method**
```c
int __cdecl sub_52D340(vtable** obj, int framePtr, int argCount)
```
- Calls (**obj)(obj, *(framePtr + 16)) through vtable

**sub_52D370 (0x52D370) - Call Object Method with Bool Return**
```c
int __cdecl sub_52D370(int (*funcPtr)(int), int argCount)
```
- Gets object from VM stack, resolves native pointer
- Calls funcPtr(nativeObj), pushes result as 0/1

**sub_52D1F0 (0x52D1F0) - VM Return Handler**
```c
int __cdecl sub_52D1F0(int popCount, int returnCount)
```
- Pops popCount args, copies returnCount values, restores stack frame

---

## 3. VM Entry Points (How to Call VM Functions from Native Code)

### Primary Entry: sub_52EB40 (0x52EB40) - Call VM Function by Name

```c
char __cdecl sub_52EB40(int vmObject, int functionName)
// vmObject: pointer to VM object (this+42 in game objects, or +168 for native)
// functionName: const char* - the script function name
```

**Call chain:**
1. `sub_52EB40(obj, "FuncName")` -- top-level entry
2. Calls `sub_52D920(obj, "FuncName")` -- resolves function name to bytecode address (thunk to .kallis via off_1C88E08). Sets `dword_E5620C` to the bytecode entry point.
3. Calls `sub_52EA70()` -- the VM execution driver

### VM Execution Driver: sub_52EA70 (0x52EA70)

```c
char sub_52EA70()
```
- Checks `dword_E561F8` (script loaded flag) -- returns 1 if no script
- Saves VM state (dword_E56160 block, 16 bytes)
- If `dword_E5620C != 0`: sets up stack frame, calls `sub_52D9C0(dword_E5620C + 12, 0)`
- The `+12` offset skips a function header (likely: name hash, arg count, local count)
- Restores VM state after execution

### Extended Entry: sub_52EB60 (0x52EB60) - Call VM Function with Arguments

```c
void sub_52EB60(int vmObject, int functionName, int argCount, ...)
// varargs: each arg is a 5-DWORD VMArg struct
```

**Call chain:**
1. Calls `sub_52D920(obj, "FuncName")` to resolve
2. For each argument, calls `sub_52D5A0(argStruct)` to push it onto the VM stack
3. Calls `sub_52EA70()` to execute

### VMArg Structure (20 bytes / 5 DWORDs)

```c
struct VMArg {
    int type;       // 0=int, 1=float, 2=bool, 4=vec3, 5=vec3, 6=vec4, 7=vec4, 8=string
    union {
        int   intVal;
        float floatVal;
        char  boolVal;
        float vec3[3];
        float vec4[4];
        int   stringPtr;
    };
};
```

**Constructors:**
- `sub_52C150(VMArg* out, float val)` -- type=1 (float)
- `sub_52C170(VMArg* out, bool val)` -- type=2 (bool)
- `sub_52C1D0(int objPtr)` -- type=8? (object, thunk to .kallis)

### Object Init Entry: sub_52EC30 (0x52EC30)

```c
int __cdecl sub_52EC30(int vmTypeInfo, int nativeObject)
```
- Sets up the native counterpart for a VM object type
- Stores native object pointer at vmTypeInfo+16
- Resolves debug name via sub_58C190
- Calls `dword_E561D4(nativeObject)` -- the object dispatch function (sub_4D7790 -> sub_552800)
- Sets flag 0x4000 on the type
- Calls `sub_52D920(vmTypeInfo, "Init")` then `sub_52EA70()` -- invokes the "Init" script function

---

## 4. Native Function Registration System

### sub_52EA10 (0x52EA10) - Register Native Function
```c
int __cdecl sub_52EA10(const char* name, void* funcPtr)
// Thunk to off_1C86CDC -> .kallis
```
Registers a global native function callable from VM scripts.

### sub_52EA30 (0x52EA30) - Register Native Coroutine
```c
int __cdecl sub_52EA30(const char* name, void* funcPtr)
// Thunk to off_1C8E398 -> .kallis
```
Registers a native function that supports yielding (used for "sauDelay", "sauLoadBankAsync").

### sub_52E660 (0x52E660) - Register Native Method
```c
char __cdecl sub_52E660(const char* className, const char* methodName, int funcAddr)
```
Registers a native method on a VM class. Works by:
1. Hashing className and methodName
2. Iterating the class table (dword_E561E0)
3. Finding the matching class/method slot
4. Storing funcAddr in the slot (replaces -1 sentinel)

Error on re-registration: "WARNING: Reregistration of the method \"%s::%s\"."

### Master Registration: sub_431B60 (0x431B60)
Calls all sub-registration functions in order:
- sub_49BA20 -- utility functions (sauPrint, sauRandRange, sauSendTrigger, etc.)
- sub_495710 -- AI functions
- sub_494270 -- animation functions
- sub_541FC0 -- audio functions (sauPlayCue, sauPlaySegment, etc.)
- sub_498D60 -- camera functions
- sub_497C10 -- particle/VFX functions
- sub_4D7670 -- unknown
- sub_498180 -- timer functions (sauSetTimer, sauPauseGame, etc.)
- sub_493BC0 -- navigation functions
- ... and many more

### Native Function Calling Convention

Native functions registered with the VM interact through these stack operations:

**Popping arguments (reading from VM stack):**
- `sub_52C610(int*)` / `sub_4CE3CD(int*)` -- pop int
- `sub_52C640(int*)` -- pop int (same as above)
- `sub_52C670(int*)` -- pop int variant
- `sub_52C6A0(float*)` -- pop float
- `sub_52C6D0(bool*)` -- pop bool
- `sub_52C700(float[3])` -- pop vec3
- `sub_52C770(float[3])` -- pop vec3 (wrapper)
- `sub_52C7A0(float[4])` -- pop vec4
- `sub_52C860(int*)` -- pop object reference (converts to native ptr)
- `sub_52D7D0(char**)` / `sub_52D820(int*)` -- pop string / pop object with validation

**Pushing return values (writing to VM stack):**
- `sub_52CA80(bool)` -- push bool (guarded by dword_E561F8)
- `sub_52CC50(float)` -- push float
- `sub_52CC70(bool)` -- push bool (unguarded)
- `sub_52CAB0(float[3])` -- push vec3
- `sub_52CB40(float[4])` -- push vec4
- `sub_52CE00(int)` -- push object reference (native ptr -> VM offset)
- `sub_52CE40(int)` -- push object reference (via +168 indirection)
- `sub_52CCD0(int)` -- push vec3 from internal format
- `sub_52CD80(int)` -- push vec4 from internal format

**Example native function (sauRandRange):**
```c
int sub_499050() {
    float min, max;
    sub_52C6A0(&max);    // pop float arg
    sub_52C6A0(&min);    // pop float arg
    float result = min + (max - min) * Random();
    return sub_52CC50(result);  // push float result
}
```

---

## 5. dword_E561D4 -- Object Dispatch Function Pointer

### Setup
- Set by `sub_52C540(funcPtr)` at address 0x52C540
- Called from initialization at 0x431B21: `sub_52C540(sub_4D7790)`

### The Dispatch Function: sub_4D7790 (0x4D7790)
```c
int __cdecl sub_4D7790(int nativeObject) {
    return sub_552800(nativeObject);
}
```
sub_552800 is a thunk to .kallis (off_1C8319C). It handles "object dispatch" -- likely
initializing the native object's connection to the VM object system (setting up vtable
pointers, registering with the object manager, etc.).

### Usage in sub_52EC30
```c
dword_E561D4(nativeObject);  // Initialize native object in VM system
```
This is called after setting the native counterpart pointer (vmTypeInfo+16 = nativeObject)
and before calling the VM "Init" function.

---

## 6. Functions Near VM Stack Area (0x52C600-0x52CF00)

All functions in this range are VM stack operations:

| Address  | Purpose                              | Category         |
|----------|--------------------------------------|-----------------|
| 0x52C600 | Clear save state flag                | State            |
| 0x52C610 | Pop int from VM stack (thunk)        | Stack Pop        |
| 0x52C640 | Pop int from VM stack                | Stack Pop        |
| 0x52C670 | Pop int from VM stack (variant)      | Stack Pop        |
| 0x52C6A0 | Pop float from VM stack              | Stack Pop        |
| 0x52C6D0 | Pop bool from VM stack               | Stack Pop        |
| 0x52C700 | Pop vec3 from VM stack               | Stack Pop        |
| 0x52C770 | Pop vec3 wrapper                     | Stack Pop        |
| 0x52C7A0 | Pop vec4 from VM stack               | Stack Pop        |
| 0x52C860 | Pop object ref from VM stack         | Stack Pop        |
| 0x52CA80 | Push bool to VM stack (guarded)      | Stack Push       |
| 0x52CAB0 | Push vec3 to VM stack (guarded)      | Stack Push       |
| 0x52CB00 | Push variant (guarded)               | Stack Push       |
| 0x52CB40 | Push vec4 to VM stack (guarded)      | Stack Push       |
| 0x52CBF0 | Push string (thunk)                  | Stack Push       |
| 0x52CC50 | Push float to VM stack               | Stack Push       |
| 0x52CC70 | Push bool to VM stack (unguarded)    | Stack Push       |
| 0x52CCD0 | Push vec3 from internal format       | Stack Push       |
| 0x52CD80 | Push vec4 from internal format       | Stack Push       |
| 0x52CE00 | Push object ref (ptr -> VM offset)   | Stack Push       |
| 0x52CE40 | Push object ref (via +168 field)     | Stack Push       |

---

## 7. Recommendations for Calling VM Functions from EndScene Hook

### Method 1: Use sub_52EB40 (Simple, No Args)

This is the safest and simplest approach. The game uses this pattern hundreds of times.

```cpp
// Type definitions
typedef char (__cdecl* VMCallFunc_t)(int vmObject, int functionName);
typedef void (__cdecl* VMCallWithArgs_t)(int vmObject, int functionName, int argCount, ...);

// Function pointers (direct addresses in .text section)
auto VMCall = (VMCallFunc_t)0x52EB40;
auto VMCallArgs = (VMCallWithArgs_t)0x52EB60;

// Calling a VM function (no args):
int vmObj = ...;  // Get from game object at this+0xA8 (offset 42*4)
VMCall(vmObj, (int)"OnAlerted");
```

### Method 2: Use sub_52EB60 (With Arguments)

```cpp
// VMArg struct (20 bytes)
struct VMArg {
    int type;
    union {
        int intVal;
        float floatVal;
        char boolVal;
        float vec[4];
    };
    int pad[3]; // padding to 20 bytes total
};

// Float arg constructor (sub_52C150 pattern)
VMArg MakeFloatArg(float val) {
    VMArg arg = {};
    arg.type = 1;
    arg.floatVal = val;
    return arg;
}

// Calling with arguments (each arg is passed as 5 DWORDs on stack):
VMArg arg1 = MakeFloatArg(1.0f);
VMCallArgs(vmObj, (int)"OnDamage", 1,
    *(int*)&arg1, ((int*)&arg1)[1], ((int*)&arg1)[2], ((int*)&arg1)[3], ((int*)&arg1)[4]);
```

### Method 3: Use sub_52EC30 to Initialize New VM Objects

```cpp
typedef int (__cdecl* VMInitObject_t)(int vmTypeInfo, int nativeObject);
auto VMInitObject = (VMInitObject_t)0x52EC30;
VMInitObject(vmType, nativeObj);
```

### Important Guards

1. **Check dword_E561F8 != 0** before calling -- scripts must be loaded
2. **Check dword_E56210** -- don't re-enter if already executing (though the engine does handle recursion in case 1 of the interpreter)
3. **sub_52D920 is a thunk to .kallis** -- it MUST be called through the thunk, not reimplemented
4. **All VM calls must happen on the main thread** -- the VM is not thread-safe
5. **The function name string must be a valid script function name** -- if it doesn't exist, sub_52D920 returns 0 and sub_52EA70 returns early

### Getting VM Object Pointers

Game objects store their VM object pointer at offset `+0xA8` (this+42 in the decompiler).
Some objects use `+168` (0xA8) of a sub-object. The pattern seen in game code:

```cpp
int vmObj = *(int*)(gameObject + 0xA8);  // this[42]
if (vmObj) {
    VMCall(vmObj, (int)"FunctionName");
}
```

### Summary of Key Addresses

| Address    | Function                | Use                                    |
|------------|------------------------|----------------------------------------|
| 0x52EB40   | VMCall                 | Call VM function by name (no args)     |
| 0x52EB60   | VMCallWithArgs         | Call VM function with typed arguments  |
| 0x52EC30   | VMInitObject           | Initialize VM object with native ptr   |
| 0x52EA70   | VMExecute              | Execute pending VM function            |
| 0x52D9C0   | VMInterpreter          | Main bytecode interpreter loop         |
| 0x52D920   | VMResolveFunction      | Resolve function name to bytecode      |
| 0x52EA10   | VMRegisterFunction     | Register native function               |
| 0x52EA30   | VMRegisterCoroutine    | Register native coroutine              |
| 0x52E660   | VMRegisterMethod       | Register native method on class        |
| 0x431B60   | VMRegisterAll          | Master registration (all natives)      |
| 0xE56168   | vm_arg_stack_base      | VM argument stack base                 |
| 0xE5616C   | vm_stack_top           | VM stack top pointer                   |
| 0xE56200   | vm_arg_count           | Current argument count/index           |
| 0xE561F8   | vm_script_loaded       | Script loaded flag                     |
| 0xE5620C   | vm_resolved_entry      | Resolved bytecode entry point          |
