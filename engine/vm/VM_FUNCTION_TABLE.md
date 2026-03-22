# Kallis VM Function Table

**Status:** Reversed from script loading code and interpreter usage

---

## Overview

The VM function table stores entry points for all callable functions (both bytecode and native). It is loaded from compiled script files (".kallis" bytecode in SCT format) and used by the interpreter to dispatch function calls.

---

## Key Globals

| Address | Name | Purpose |
|---------|------|---------|
| 0xE561FC | g_FunctionTable | Base pointer to the function table (array of DWORDs) |
| 0xE561DC | g_StringTable | String/name table base |
| 0xE561E0 | g_ClassTable | Class/type table base |
| 0xE561E4 | g_StringResolve | String resolve table (for debug name lookups) |
| 0xE561E8 | g_TypeList | Type list pointer |
| 0xE561D8 | g_ScriptHeader | Pointer to the loaded script header |

---

## Script File Format (SCT v13)

### Loading Function: sub_52C2A0 (0x52C2A0)

This function loads a compiled script file for "Edit and Continue" mode. It reveals the complete file structure.

### File Header (52 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| +0x00 | 4 | magic | "SCT\0" (bytes: 0x53 0x43 0x54 0x00) |
| +0x04 | 4 | version | Must be 13 (decimal) |
| +0x08 | 4 | fileSize | Total file size in bytes |
| +0x0C | 4 | stringTableOffset | Offset to NTV (string/name) table |
| +0x10 | 4 | functionTableOffset | Offset to function table |
| +0x14 | 4 | classTableOffset | Offset to VTL (class) table |
| +0x18 | 4 | stringResolveOffset | Offset to string resolve table |
| +0x1C | 4 | typeListOffset | Offset to type list |
| +0x20 | 4 | saveStateOffset | Offset to save state data |
| +0x24 | 4 | (reserved) | - |
| +0x28 | 4 | saveStateSize | Size of save state data (at +0x28) |
| +0x2C | 2 | flags | Bit 0: debugger enable; Bit 1: E&C support |

All offsets are relative to the start of the loaded buffer (the base address where the file data is loaded).

### Validation Checks

1. `magic` must be "SCT\0" (checked at 0x52C2F4)
2. `version` must be 13 (error: "Incorrect script version. Got %d, expected %d")
3. `flags & 2` must be set for Edit and Continue mode
4. String table must start with "NTV\0" magic (validated by sub_52BE50)
5. Class table must start with "VTL\0" magic (validated by sub_52BF80)

### Loading Sequence (sub_52C2A0)

```
1. Open file (sub_4E5BE0)
2. Allocate memory: 0x100000 bytes at 0x4000000 ("Edit and Continue" label)
3. Allocate buffer (sub_4DE530): 0x80000 bytes, aligned to 128
4. Read header (52 bytes)
5. Validate magic "SCT\0" and version 13
6. Read rest of file: fileSize - 52 bytes
7. Close file
8. Set up table pointers:
   E561DC = base + stringTableOffset     // NTV table (validated)
   E561FC = base + functionTableOffset   // Function table
   E561E0 = base + classTableOffset      // VTL class table (validated)
9. Fixup function table entries (sub_52BFA0): add base address to each entry
10. Fixup type list entries (sub_52BFD0): add base to pointers, adjust sub-pointers
11. Set E561E4 = base + stringResolveOffset
12. Initialize string resolver (sub_58C0E0, sub_58C100)
```

---

## Function Table Structure

`E561FC` points to an array of DWORDs, where each DWORD is either:

1. **Bytecode function pointer** -- an address of compiled bytecode
2. **Native function pointer** -- address of a registered native function

### Function Table Entry

After fixup (step 9), each entry is an absolute address:
```
E561FC[i] = baseAddr + originalOffset
```

The fixup function `sub_52BFA0` iterates the table:
```c
void sub_52BFA0(int* table, int baseAddr) {
    for (int i = 0; i < table[1]; i++) {
        table[i + 2] += baseAddr;    // Fixup relative -> absolute
        sub_58C080(baseAddr);         // Fixup string references within entry
    }
}
```

Table layout:
```
table[0] = (flags/reserved)
table[1] = entry_count
table[2] = entry[0]   (first function pointer)
table[3] = entry[1]
...
table[1+N] = entry[N-1]
```

### Usage in Interpreter

The interpreter accesses functions by index:

```c
// CALL_SCRIPT (opcode 0x01):
int funcIdx = bytes[1] | (bytes[2] << 8);
int bytecodeBase = E561FC + 4 * funcIdx;  // NOT *(E561FC + 4*funcIdx)!
// For script calls, the value at this address IS the bytecode base
sub_52D9C0(bytecodeBase, 0);

// CALL_NATIVE (opcode 0x02):
int funcIdx = bytes[1] | (bytes[2] << 8);
int funcPtr = *(E561FC + funcIdx * 4);    // Dereference to get function pointer
sub_52D240(funcPtr, argCount);
```

Wait -- looking more carefully at the disassembly:
- Opcode 0x01: `ecx = E561FC + 4*funcIdx` then `call sub_52D9C0(ecx, 0)` -- passes the TABLE ADDRESS as bytecodeBase, not the dereferenced value. The bytecode is stored inline starting at that table slot.
- Opcodes 0x02-0x05 and 0x3E: `ecx = *(E561FC + funcIdx*4)` then pass `ecx` to the dispatcher -- dereferenced to get the native function pointer.

**Key insight:** For bytecode functions, the function table entry IS the start of the bytecode. For native functions, the entry is a pointer to the handler.

---

## Function Name Resolution (sub_52D920)

**Address:** 0x52D920 (thunk to .kallis via off_1C88E08)

```c
int __cdecl VMResolveFunction(int vmObject, const char* funcName);
```

This function:
1. Takes a VM object pointer and a function name string
2. Searches the object's function list for a matching name
3. Sets `E5620C` (resolved_entry) to the function's bytecode address
4. Returns success/failure

The resolution happens inside `.kallis` code (encrypted), so the internal algorithm cannot be statically analyzed. However, the result is stored in `E5620C` and consumed by `sub_52EA70` (VMExecute).

### Name Lookup Helper (sub_52D190)

**Address:** 0x52D190 | **Size:** 57 bytes

```c
int __cdecl sub_52D190(int vmObj, int classIndex, int nameHash) {
    int classDesc = *(*(*(vmObj + 20) + 20) + 4 * classIndex);
    int methodCount = *(classDesc + 8);
    if (methodCount == 0) return 0;
    for (int i = 0; i < methodCount; i++) {
        int entry = *(classDesc + 12 + i * 4);  // array of pointers at +12
        if (*(entry + 4) == nameHash)
            return entry;
    }
    return 0;
}
```

This searches a class descriptor's method list by name hash. Each method entry has:
- `entry[0]` = method data
- `entry[1]` = name hash (compared against search key)

Called from .kallis code during function resolution.

---

## Bytecode Function Header

When `sub_52EA70` calls the interpreter, it does:
```c
sub_52D9C0(E5620C + 12, 0);
```

The `+12` skip means each bytecode function has a **12-byte header** before the actual bytecode:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| +0x00 | 4 | (unknown) | Likely function name hash or ID |
| +0x04 | 4 | (unknown) | Likely argument count or local count |
| +0x08 | 4 | (unknown) | Likely total frame size or return info |
| +0x0C | ... | bytecode | Actual VM instructions start here |

The interpreter starts execution at the first instruction (offset +0x0C from the function entry point).

---

## String Table (NTV)

The string table at `E561DC` stores function and variable names used by the VM:

### Magic: "NTV\0" (checked by sub_52BE50)
```c
BOOL sub_52BE50(char* ptr) {
    return ptr[0]=='N' && ptr[1]=='T' && ptr[2]=='V' && ptr[3]==0;
}
```

### String Resolver (sub_58C190)
```c
int __stdcall sub_58C190(int nameId);
// Thunk to .kallis (off_1C8DEDC)
// Resolves a name ID to a const char* string
// Used for debug messages ("Object type had no native counterpart: %s")
```

The resolver requires `E561E4` to be non-zero (the string resolve table must be loaded).

---

## Class Table (VTL)

The class table at `E561E0` stores VM class definitions:

### Magic: "VTL\0" (checked by sub_52BF80)
```c
BOOL sub_52BF80(char* ptr) {
    return ptr[0]=='V' && ptr[1]=='T' && ptr[2]=='L' && ptr[3]==0;
}
```

### Table Layout
```
E561E0 -> class table:
  [+0x00] DWORD  (flags/reserved)
  [+0x04] DWORD  class_count
  [+0x08] DWORD* class_ptrs[count]  -- pointers to class descriptors
```

### Class Descriptor
```
class[+0x00] DWORD  name_hash_1
class[+0x04] DWORD  name_hash_2
class[+0x08] DWORD  name_id (for string resolver)
class[+0x0C] DWORD  (reserved)
class[+0x10] DWORD  native_obj_ptr (set by sub_52EC30)
class[+0x14] DWORD  vtable_ptr (pointer to method/vtable table)
class[+0x18] WORD   type_tag (set by SET_EXCEPT opcode)
class[+0x1A] WORD   flags (bit 0: in-use, bit 1: alive, bit 14: initialized)
class[+0x1C] DWORD  method_table
class[+0x20] DWORD  save_data_ptr (fixup: +baseAddr if non-zero)
```

### Fixup (sub_52BFD0)
```c
void sub_52BFD0(int* typeList, int baseAddr) {
    for (int i = 0; i < *typeList; i++) {
        typeList[i+1] += baseAddr;           // Fixup class ptr
        *(typeList[i+1] + 20) += baseAddr;   // Fixup vtable ptr
        int savePtr = *(typeList[i+1] + 32);
        if (savePtr) {
            *(typeList[i+1] + 32) = baseAddr + savePtr;
            *(typeList[i+1] + 28) = *(typeList[i+1] + 32);
        }
    }
}
```

---

## Type List (E561E8)

```
E561E8 -> type list:
  [+0x00] DWORD  type_count
  [+0x04] DWORD* type_ptrs[count]  -- pointers to type descriptors
```

Accessor functions:
- `sub_52CF00()` -- returns type_count (E561E8[0])
- `sub_52CF10(int index)` -- returns type_ptr[index] (E561E8[1 + index])

---

## Name Hash Algorithm

Used by `sub_52E660` (VMRegisterMethod) for class and method name matching:

```c
int hashName(const char* name) {
    int hash = 0;
    if (name) {
        char c = *name;
        while (c) {
            hash += c + (hash << (c & 7));
            name++;
            c = *name;
        }
    }
    return hash;
}
```

The hash is computed by iterating each character:
1. Shift the running hash left by `(char & 7)` bits (0-7 positions)
2. Add the character value
3. Accumulate into hash

This produces a 32-bit hash used to match class names and method names in the class table.

---

## Related

- [VM_INTERPRETER.md](VM_INTERPRETER.md) -- How functions are called (opcodes 0x01-0x05, 0x3E)
- [VM_BYTECODE_FORMAT.md](VM_BYTECODE_FORMAT.md) -- Complete bytecode file structure
- [VM_OBJECT_SYSTEM.md](VM_OBJECT_SYSTEM.md) -- Object/class relationship
- [KALLIS_VM.md](KALLIS_VM.md) -- High-level architecture
