# Kallis VM Bytecode Format

**Status:** Reversed from script loader (sub_52C2A0) and interpreter (sub_52D9C0)

---

## Overview

The Kallis VM executes compiled scripts stored in **SCT format** (version 13). These files are produced by the Kallis compiler and contain bytecode, function tables, class definitions, and string data. The bytecode is loaded into memory and executed by the interpreter at 0x52D9C0.

---

## SCT File Structure

### File Header (52 bytes at offset 0)

```c
struct SCTHeader {
    char     magic[4];              // +0x00: "SCT\0"
    uint32_t version;               // +0x04: 13
    uint32_t fileSize;              // +0x08: total file size
    uint32_t stringTableOffset;     // +0x0C: -> NTV table
    uint32_t functionTableOffset;   // +0x10: -> function table
    uint32_t classTableOffset;      // +0x14: -> VTL class table
    uint32_t stringResolveOffset;   // +0x18: -> string resolve table
    uint32_t typeListOffset;        // +0x1C: -> type list
    uint32_t saveDataOffset;        // +0x20: -> save state area
    uint32_t reserved;              // +0x24: unused
    uint32_t saveStateSize;         // +0x28: bytes for save state
    uint16_t flags;                 // +0x2C: see below
    uint8_t  padding[2];           // +0x2E: alignment
};
```

### Flags Field (+0x2C / +0x2E depending on packing)

| Bit | Meaning |
|-----|---------|
| 0 | Enable script debugger |
| 1 | Built for Edit and Continue |

---

## Section Tables

### NTV (Name/String Table)

**Magic:** "NTV\0" (validated by sub_52BE50 at 0x52BE50)
**Global:** Stored in `E561DC`

Contains null-terminated strings referenced by ID throughout the VM. Used for:
- Function names
- Class names
- Variable names
- Debug symbols

String IDs are resolved to `const char*` via `sub_58C190(nameId)`.

### Function Table

**Global:** Stored in `E561FC`

```c
struct FunctionTable {
    uint32_t flags;          // +0x00: reserved/flags
    uint32_t count;          // +0x04: number of entries
    uint32_t entries[];      // +0x08: array of function pointers/offsets
};
```

After loading, entries are **fixed up** by adding the buffer base address (sub_52BFA0). Each entry becomes an absolute memory address pointing to either:
- A bytecode function (12-byte header + instructions)
- A native function pointer (for registered natives)

### VTL (Class/Virtual Table)

**Magic:** "VTL\0" (validated by sub_52BF80 at 0x52BF80)
**Global:** Stored in `E561E0`

Contains class definitions with method dispatch tables. See [VM_FUNCTION_TABLE.md](VM_FUNCTION_TABLE.md) for detailed structure.

### Type List

**Global:** Stored in `E561E8`

```c
struct TypeList {
    uint32_t count;          // +0x00: number of types
    uint32_t typeDescs[];    // +0x04: array of pointers to type descriptors
};
```

Fixed up by sub_52BFD0 during loading.

### String Resolve Table

**Global:** Stored in `E561E4`

Used by `sub_58C190` to resolve name IDs to string pointers. Initialized by `sub_58C0E0` and populated by `sub_58C100`.

### Save State Area

**Global:** Stored in `E561EC` (data) and `E561F4` (runtime buffer)
**Size:** `*(E561D8 + 0x28)` bytes

Used for save/load game state. When saving:
1. `sub_52C550` allocates a buffer of `saveStateSize` bytes
2. Copies current state from `E561EC + 4` to the buffer
3. Sets `E561F0 = 1` (save state valid)

---

## Bytecode Function Format

Each bytecode function in the function table has a 12-byte header followed by instructions:

```
+0x00: [4 bytes] Header field 1 (likely function metadata/hash)
+0x04: [4 bytes] Header field 2 (likely arg count or local frame size)
+0x08: [4 bytes] Header field 3 (likely return info or total size)
+0x0C: [bytecode...] Instructions start here
```

The interpreter is called with `sub_52D9C0(funcEntry + 12, 0)` -- the +12 skips the header.

---

## Instruction Encoding

### Opcode Byte

```
Bit 7 6 5 4 3 2 | 1 0
    opcode (0-63) | regBank (0-3)
```

- `opcode = byte >> 2` (6 bits, 0x00-0x3F)
- `regBank = byte & 3` (2 bits, selects base pointer)

### Instruction Sizes

| Category | Size | Opcodes |
|----------|------|---------|
| 1-byte | 1 | Math (0x0F-0x1A), Logic (0x1C-0x21), Compare (0x22-0x2D), Convert (0x32-0x33), NOP1 (0x37), BREAKPOINT (0x38), PUSH_FRAME (0x08) |
| 3-byte | 3 | POP_N (0x06), PUSH_N (0x07), Branch (0x2E-0x30), SET_EXCEPT (0x31), ADDR_CALC (0x34), LOAD/STORE (0x3A-0x3D) |
| 5-byte | 5 | CALL (0x01-0x05, 0x3E), COPY (0x0A-0x0B), PUSH_IMM (0x0C), LOAD/STORE_OFFSET (0x0D-0x0E), SAVE_STATE (0x09), SET_DEBUG (0x35), NOP5 (0x36, 0x3F), TRACE (0x39), RET (0x00) |

### Operand Encoding (2-byte, in 3-byte instructions)

```
byte[1] = low byte
byte[2] = high byte
value = (byte[2] << 8) | byte[1]
```

Sign-extended via `movsx` for signed operations (jumps, offsets).

### Operand Encoding (4-byte, in 5-byte instructions)

Two 16-bit values:
```
field1 = byte[1] | (byte[2] << 8)    // typically function index
field2 = byte[3] | (byte[4] << 8)    // typically arg count / secondary param
```

For immediate values (PUSH_IMM, SET_DEBUG):
```
value = (byte[2] << 24) | (byte[3] << 16) | (byte[4] << 8) | byte[1]
```

---

## Execution Model

### Function Call Chain

```
Native Code
    |
    v
sub_52EB40(vmObj, "FuncName")     -- VMCall
    |
    v
sub_52D920(vmObj, "FuncName")     -- VMResolveFunction (thunk to .kallis)
    |                                 Sets E5620C = bytecode address
    v
sub_52EA70()                       -- VMExecute
    |  Saves 16-byte state from E56170
    |  Sets up stack frame
    v
sub_52D9C0(E5620C + 12, 0)       -- VMInterpreter
    |  Main dispatch loop
    |  Reads opcodes, dispatches
    |  May recursively call sub_52D9C0 (CALL_SCRIPT)
    |  May call native functions (CALL_NATIVE, etc.)
    v
Returns to VMExecute
    |  Restores state
    v
Returns to native caller
```

### Stack Frame Setup (in VMExecute)

```c
// Before calling interpreter:
*E5616C = E56168;       // Save old arg stack base
E56168 = E5616C;        // New frame starts at current eval stack top
E5616C += 4;            // Advance past saved link
```

### Coroutine Support

The SAVE_STATE opcode (0x09) allows functions to yield:
1. Saves current IP, stack data, and frame info to a save area
2. Returns from the interpreter
3. Later, execution can be resumed by restoring the saved state and calling the interpreter again

This is used by blocking script functions like `sauDelay(seconds)`.

---

## Memory Layout at Runtime

```
E56160 [16 bytes] -- Object base array (4 pointers)
E56170 [16 bytes] -- State backup area (saved/restored by VMExecute)
E56180 [64 bytes] -- VM register/scratch area (zeroed on init)
E561C0 [4 bytes]  -- Memory allocation size (set to 0x100000)
E561C5 [1 byte]   -- Debug step flag
E561C6 [1 byte]   -- Debug break flag
E561CC [4 bytes]   -- Stack depth counter
E561D0 [4 bytes]   -- Object lookup function pointer
E561D4 [4 bytes]   -- Object dispatch function pointer (sub_4D7790)
E561D8 [4 bytes]   -- Script header pointer
E561DC [4 bytes]   -- String table base (NTV)
E561E0 [4 bytes]   -- Class table base (VTL)
E561E4 [4 bytes]   -- String resolve table
E561E8 [4 bytes]   -- Type list pointer
E561EC [4 bytes]   -- Save state data
E561F0 [4 bytes]   -- Save state flag
E561F4 [4 bytes]   -- Save state buffer
E561F8 [4 bytes]   -- Scripts loaded flag
E561FC [4 bytes]   -- Function table base
E56200 [4 bytes]   -- Arg stack index
E56204 [4 bytes]   -- Current native function pointer
E56208 [4 bytes]   -- Pending execution flag
E5620C [4 bytes]   -- Resolved bytecode entry
E56210 [4 bytes]   -- Execution depth flag
E56218 [4 bytes]   -- Debug/trace value
E5621C [4 bytes]   -- (padding)
E56220 [12 bytes]  -- Debugger state 1
E5622C [...]       -- Debugger state 2
```

### Initialization (sub_52C420)

Sets all VM state to defaults:
- Object base registers (E56120-E5615C): all set to -1 (0xFFFFFFFF)
- Scratch area (E56180-E561BC): all zeroed
- All table pointers: zeroed
- Script loaded flag: 0
- All execution flags: 0
- `0x713118` (execution active): 1

---

## Related

- [VM_INTERPRETER.md](VM_INTERPRETER.md) -- Full opcode reference
- [VM_FUNCTION_TABLE.md](VM_FUNCTION_TABLE.md) -- Function and class table details
- [VM_STACK_SYSTEM.md](VM_STACK_SYSTEM.md) -- Stack operations
- [VM_OBJECT_SYSTEM.md](VM_OBJECT_SYSTEM.md) -- Object reference system
- [KALLIS_VM.md](KALLIS_VM.md) -- Architecture overview
