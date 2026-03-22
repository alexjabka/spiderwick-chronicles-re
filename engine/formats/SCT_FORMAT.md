# SCT — Kallis VM Compiled Script Format

**Status:** Partially reversed. Header, string tables, and bytecode documented. Disassembler at `tools/sct_disasm.py` (function table parsing needs fix).

---

## Header (52 bytes)

```c
struct SCTHeader {
    char     magic[4];              // "SCT\0"
    uint32_t version;               // 13
    uint32_t fileSize;              // total file size
    uint32_t stringTableOffset;     // -> NTV string table
    uint32_t functionTableOffset;   // -> function table
    uint32_t classTableOffset;      // -> VTL class table
    uint32_t stringResolveOffset;   // -> string resolve table
    uint32_t typeListOffset;        // -> type list
    uint32_t saveDataOffset;        // -> save state area
    uint32_t reserved;
    uint32_t saveStateSize;
    uint16_t flags;                 // bit0=debugger, bit1=edit-and-continue
    uint8_t  padding[2];
};
```

## Sections

### NTV String Table
- Magic: `"NTV\0"`
- Contains null-terminated strings referenced by ID
- Used for function names, class names, variable names, debug symbols
- String IDs resolved via `sub_58C190(nameId)`

### Function Table
- Flags (u32) + count (u32) + entry offsets (u32 array)
- Entries are byte offsets to bytecode functions within the file
- Each function has a 12-byte header before bytecode instructions
- After loading, entries are fixed up by adding buffer base address

### VTL Class Table
- Magic: `"VTL\0"`
- Contains class definitions with method dispatch tables
- Enables polymorphic method calls (CALL_VIRTUAL opcode)

### String Resolve Table
- Array of `{nameId(u32), stringOffset(u32)}` pairs
- Maps name hash IDs to readable strings within the file
- GroundsD SCT has 2971 entries

### Bytecode

64 opcodes, stack-based. See `engine/vm/VM_INTERPRETER.md` for full opcode reference.

Instruction format: `opcode_byte [operands]`
- Bits 7-2: opcode (0x00-0x3F)
- Bits 1-0: register bank selector (0-3)
- 1-byte: math, logic, conversions
- 3-byte: jumps, LOAD/STORE, POP_N/PUSH_N
- 5-byte: CALL, PUSH_IMM, RET

## Per-Level Script Content (GroundsD example)

Strings include: `sauSetPosition`, `PROP_BackGate`, `worldPropObjprop_FrontDoor`, `BirdHouse`, `SetPositionObj`, `ActivateAnimate_BackGateOpen`, `JaredOpensFrontDoorOutside`, `Init_MissionStart01_FromMansionFrontDoor`, garden/trigger/AI function names.

## Tool

`tools/sct_disasm.py` — SCT bytecode disassembler (function table offset parsing needs fix).
