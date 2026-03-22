# Kallis VM Interpreter - Full Opcode Reference

**Address:** 0x52D9C0 (sub_52D9C0)
**Size:** 2,810 bytes (0xAFA)
**Prototype:** `int* __cdecl VMInterpreter(int bytecodeBase, int initialOffset)`
**Status:** Fully reversed -- all 64 opcodes documented

---

## Architecture

The Kallis VM is a **stack-based bytecode interpreter** with a main dispatch loop at 0x52D9C0.

### Instruction Format

Each instruction starts with a single opcode byte:
```
Bits 7-2: opcode (0x00 - 0x3F, 64 opcodes)
Bits 1-0: register bank selector (0-3)
```

The register bank selector (low 2 bits) indexes into `dword_E56160[0..3]` -- a set of 4 base pointers (16 bytes at 0xE56160). This is used by memory access opcodes (LOAD, STORE, PUSH_IMM, etc.) to select which base address to use.

### Variable-Length Encoding

Operands follow the opcode byte. The encoding depends on the opcode:
- **1-byte instructions:** Opcode only (math, logic, conversions)
- **3-byte instructions:** Opcode + 2-byte operand (jumps, POP_N, PUSH_N, LOAD, STORE, SET_EXCEPT, ADDR_CALC)
- **5-byte instructions:** Opcode + 4-byte operand (CALL, PUSH_IMM, COPY, SET_DEBUG, TRACE, SAVE_STATE)

### Operand Byte Order

For 2-byte operands (bytes at offsets +1, +2):
```
value = (byte[2] << 8) | byte[1]    // big-endian-ish: high byte at +2
```
This is sign-extended via `movsx` for use as signed offsets.

For 4-byte operands (bytes at offsets +1, +2, +3, +4):
- Bytes 1-2: typically a function/entry index (16-bit)
- Bytes 3-4: typically an argument count (16-bit)
Some opcodes use all 4 bytes as a single 32-bit immediate.

### Instruction Pointer

- `edi` register holds the current IP (offset from bytecodeBase)
- `[esp+14h+arg_0]` (a1) holds the bytecodeBase
- Effective address of current instruction: `bytecodeBase + edi`
- IP advances by the instruction length (1, 3, or 5 bytes) after each opcode

### VM Stacks

The interpreter uses two stacks:

1. **Evaluation/Return Stack** (dword_E5616C): Grows upward. Used for computation, local storage, and return values. `esi` register caches the current stack top pointer.

2. **Call Frame Stack** (dword_E56168): A linked list of frames. Each frame saves the previous arg stack base. When calling a function, the current E5616C is pushed to the frame, and E56168 is set to the new frame.

### Register Summary

| Register | Purpose |
|----------|---------|
| `edi` | Instruction pointer (offset from base) |
| `esi` | Cached stack top (mirrors dword_E5616C) |
| `ecx` | Points to current instruction bytes |
| `ebx` | Register bank selector (opcode & 3) |
| `ebp` | Saved frame context (dword_E56164) |

---

## Opcode Table

### 0x00 - RET (Return)
**Size:** 5 bytes | **Format:** `[00] [popLo] [popHi] [retLo] [retHi]`

Returns from the current VM function. Calls `sub_52D1F0(popCount, returnCount)`:
- `popCount` = bytes[1] | (bytes[2] << 8) -- number of args to pop
- `returnCount` = bytes[3] | (bytes[4] << 8) -- number of return values

`sub_52D1F0` restores the previous stack frame by:
1. Computing new stack top: `E5616C = E56168 - 4 * popCount`
2. Saving source pointer: `src = E5616C - 4 * returnCount` (where return values are)
3. Restoring arg stack: `E56168 = *E56168` (follow linked list)
4. Copying return values via memcpy from old position to new top
5. Advancing E5616C by `4 * returnCount`

This is also a function epilog -- pops saved registers and returns from sub_52D9C0.

---

### 0x01 - CALL_SCRIPT (Call Bytecode Function)
**Size:** 5 bytes | **Format:** `[04] [funcIdxLo] [funcIdxHi] [selfRefLo] [selfRefHi]`

Calls another bytecode function by index into the function table.

1. Reads 16-bit function index from bytes[1..2]
2. Reads 16-bit self-reference offset from bytes[3..4]
3. Resolves self-reference: `E56164 = E56160[0] + 4 * (stack[-selfRef] / 4)`
4. Saves current arg stack: `*E5616C = E56168`
5. Creates new frame: `E56168 = E5616C; E5616C += 4`
6. **Recursively calls** `sub_52D9C0(E561FC + 4*funcIdx, 0)`
7. Restores E56164 (frame context) after return
8. IP advances by 5

The function table at `E561FC` contains DWORDs -- each is a bytecode offset. `E561FC + 4*funcIdx` gives the base address for the target function's bytecode.

---

### 0x02 - CALL_NATIVE (Call Global Native Function - cdecl)
**Size:** 5 bytes | **Format:** `[08] [funcIdxLo] [funcIdxHi] [argCountLo] [argCountHi]`

Calls a registered global native function.

1. Reads function index from bytes[1..2], arg count from bytes[3..4]
2. Looks up function pointer: `funcPtr = *(E561FC + funcIdx*4)` (function table entry is a native function pointer)
3. Saves frame, creates new arg stack frame
4. Calls `sub_52D240(funcPtr, argCount)`:
   - Sets `E56200 = argCount` (so native code can pop args)
   - Sets `E56204 = funcPtr` (current function pointer)
   - Calls `funcPtr()` directly (cdecl, no arguments passed -- it reads from VM stack)
   - Returns `(newStackTop - oldStackTop) / 4` = number of return values pushed
5. Calls `sub_52D1F0(argCount, returnCount)` to clean up
6. IP advances by 5

---

### 0x03 - CALL_METHOD (Call Native Method via vtable - thiscall)
**Size:** 5 bytes | **Format:** `[0C] [funcIdxLo] [funcIdxHi] [argCountLo] [argCountHi]`

Calls a native method on a VM object through its vtable.

1. Same operand decode as CALL_NATIVE
2. Calls `sub_52D280(vtableObj, argCount)`:
   - Gets "this" VM object from stack: `vmObj = E56160[0] + 4 * (argStackValue >> 2)`
   - Validates vmObj has native counterpart at `vmObj+16`
   - If not: prints "Object type had no native counterpart: %s"
   - Sets `E56200 = argCount - 1`
   - Calls `(**vtableObj)(vtableObj, *(vmObj+16))` -- thiscall through vtable
3. Returns pushed value count; cleaned up by sub_52D1F0

---

### 0x04 - CALL_STATIC (Call Native Static with Frame - cdecl)
**Size:** 5 bytes | **Format:** `[10] [funcIdxLo] [funcIdxHi] [argCountLo] [argCountHi]`

Calls a native static function, passing the current frame's native object.

1. Same decode. Saves E56164.
2. Sets flag: `*(E56164 + 0x1A) |= 1` (marks frame as in-use)
3. Calls `sub_52D300(funcPtr, E56164, argCount)`:
   - Sets `E56200 = argCount`
   - Saves/restores `E56204`
   - Calls `funcPtr(*(E56164 + 16))` -- passes native object from frame
4. Cleans up via sub_52D1F0

---

### 0x05 - CALL_VIRT (Call Native Virtual with Frame - thiscall)
**Size:** 5 bytes | **Format:** `[14] [funcIdxLo] [funcIdxHi] [argCountLo] [argCountHi]`

Like CALL_STATIC but dispatches through a vtable.

1. Sets frame flag: `*(E56164 + 0x1A) |= 1`
2. Calls `sub_52D340(vtableObj, E56164, argCount)`:
   - Sets `E56200 = argCount`
   - Calls `(**vtableObj)(vtableObj, *(E56164 + 16))` -- thiscall via vtable
3. Cleans up via sub_52D1F0

---

### 0x06 - POP_N (Pop N Values)
**Size:** 3 bytes | **Format:** `[18] [countLo] [countHi]`

Pops N values from the evaluation stack:
- `count = bytes[1] | (bytes[2] << 8)`
- `E5616C -= 4 * count`
- Also decrements `E561CC` (stack depth counter)

---

### 0x07 - PUSH_N (Reserve N Stack Slots)
**Size:** 3 bytes | **Format:** `[1C] [countLo] [countHi]`

Reserves N slots on the evaluation stack:
- `count = bytes[1] | (bytes[2] << 8)`
- `E5616C += 4 * count`
- Increments `E561CC`

---

### 0x08 - PUSH_FRAME (Push Frame Offset)
**Size:** 1 byte | **Format:** `[20]`

Pushes the current frame context offset onto the stack:
- `*stack = E56164 - E56160` (relative offset of current frame from object base)
- `E5616C += 4`

---

### 0x09 - SAVE_STATE (Save Coroutine State)
**Size:** 5 bytes | **Format:** `[24] [b1] [b2] [b3] [b4]`

Saves execution state for coroutine suspension. Complex opcode:
1. Reads a 32-bit value from bytes (combined as a size)
2. Calculates stack data to save
3. Copies stack data to coroutine save area using memcpy (sub_5ADF00)
4. Stores IP, base pointer, and size into the save area
5. Pops registers and returns from the interpreter

This allows a function to yield and be resumed later (used by `sauDelay`, `sauWait` etc).

---

### 0x0A - COPY_TO_STK (Copy Memory to Stack, 10)
**Size:** 5 bytes | **Format:** `[28] [offsetLo] [offsetHi] [sizeLo] [sizeHi]`

Copies a block of memory onto the evaluation stack:
- `offset = bytes[1] | (bytes[2] << 8)` -- source offset from base[regBank]
- `size = bytes[3] | (bytes[4] << 8)` -- number of bytes
- Source: `E56160[regBank] + offset`
- Calls `memcpy(stackTop, source, size)` via sub_5B0650
- Advances stack top by `align4(size)` bytes

---

### 0x0B - COPY_FROM (Copy from Stack to Memory, 11)
**Size:** 5 bytes | **Format:** `[2C] [offsetLo] [offsetHi] [sizeLo] [sizeHi]`

Copies data from the evaluation stack to memory:
- `offset = bytes[1] | (bytes[2] << 8)` -- destination offset from base[regBank]
- `size = bytes[3] | (bytes[4] << 8)` -- number of bytes
- Destination: `E56160[regBank] + offset`
- Shrinks stack by `align4(size)` bytes first, then copies

---

### 0x0C - PUSH_IMM (Push Immediate, 12)
**Size:** 5 bytes | **Format:** `[30] [b1] [b2] [b3] [b4]`

Pushes a 32-bit immediate value to the evaluation stack:
- `value = (b4 << 24) | (b3 << 16) | (b2 << 8) | b1`
- Actually encoded as: `value = (b2 << 24) | (b3 << 16) | (b4 << 8) | b1`
- Writes to `*E56160[regBank]`; advances E5616C by 4

Note: This writes to the address pointed to by the regBank base, not to the eval stack. The regBank selects a target "register" area.

---

### 0x0D - LOAD_OFFSET (Load Indirect with Stack Offset, 13)
**Size:** 5 bytes | **Format:** `[34] [offsetLo] [offsetHi] [sizeLo] [sizeHi]`

Complex load: reads a base from the stack, adds an offset, and copies a block:
- Pops stack top value as additional base offset
- `addr = E56160[regBank] + offset + stackTopValue`
- Copies `size` bytes from addr to stack using sub_5B0650

---

### 0x0E - STORE_OFF (Store Indirect with Stack Offset, 14)
**Size:** 5 bytes | **Format:** `[38] [offsetLo] [offsetHi] [sizeLo] [sizeHi]`

Complex store: writes stack data to computed address:
- Pops stack top as base offset
- `addr = E56160[regBank] + offset + poppedValue`
- Copies `size` bytes from stack to addr
- Shrinks stack by the copied amount

---

### 0x0F - ADD_I (Integer Add, 15)
**Size:** 1 byte | **Format:** `[3C]`

Pops two integers, pushes their sum:
- `stack[-2] += stack[-1]; pop 1`

---

### 0x10 - SUB_I (Integer Subtract, 16)
**Size:** 1 byte | **Format:** `[40]`

- `stack[-2] -= stack[-1]; pop 1`

---

### 0x11 - MUL_I (Integer Multiply, 17)
**Size:** 1 byte | **Format:** `[44]`

- `stack[-2] = stack[-2] * stack[-1]; pop 1` (uses `imul`)

---

### 0x12 - DIV_I (Integer Divide, 18)
**Size:** 1 byte | **Format:** `[48]`

- `stack[-2] = stack[-2] / stack[-1]; pop 1` (uses `idiv`, signed)

---

### 0x13 - MOD_I (Integer Modulo, 19)
**Size:** 1 byte | **Format:** `[4C]`

- `stack[-2] = stack[-2] % stack[-1]; pop 1` (uses `idiv`, takes remainder)

---

### 0x14 - NEG_I (Integer Negate, 20)
**Size:** 1 byte | **Format:** `[50]`

- `stack[-1] = -stack[-1]` (uses `neg`)

---

### 0x15 - INC_I (Integer Increment, 21)
**Size:** 1 byte | **Format:** `[54]`

- `stack[-1] += 1`

---

### 0x16 - DEC_I (Integer Decrement, 22)
**Size:** 1 byte | **Format:** `[58]`

- `stack[-1] -= 1`

---

### 0x17 - ADD_F (Float Add, 23)
**Size:** 1 byte | **Format:** `[5C]`

- `stack[-2] = (float)stack[-2] + (float)stack[-1]; pop 1`

Uses x87 FPU: `fld [esi-4]; fadd [esi-8]; fstp [esi-8]`

---

### 0x18 - SUB_F (Float Subtract, 24)
**Size:** 1 byte | **Format:** `[60]`

- `stack[-2] = (float)stack[-2] - (float)stack[-1]; pop 1`

---

### 0x19 - MUL_F (Float Multiply, 25)
**Size:** 1 byte | **Format:** `[64]`

- `stack[-2] = (float)stack[-2] * (float)stack[-1]; pop 1`

---

### 0x1A - DIV_F (Float Divide, 26)
**Size:** 1 byte | **Format:** `[68]`

- `stack[-2] = (float)stack[-2] / (float)stack[-1]; pop 1`

---

### 0x1B - (Default/NOP, 27)
**Size:** varies

Default case in the switch -- falls through to continue the loop without doing anything. The IP was already advanced by the previous instruction or by case-specific logic. This is the "do nothing" case.

---

### 0x1C - NEG_F (Float Negate, 28)
**Size:** 1 byte | **Format:** `[70]`

- `stack[-1] = -(float)stack[-1]` (uses `fchs`)

---

### 0x1D - INC_F (Float Increment, 29)
**Size:** 1 byte | **Format:** `[74]`

- `stack[-1] += 1.0f` (adds flt_6209BC which is 1.0f)

---

### 0x1E - DEC_F (Float Decrement, 30)
**Size:** 1 byte | **Format:** `[78]`

- `stack[-1] -= 1.0f`

---

### 0x1F - NOT (Logical NOT, 31)
**Size:** 1 byte | **Format:** `[7C]`

- `stack[-1] = (stack[-1] == 0) ? 1 : 0`

Uses `cmp/setz` to produce boolean result.

---

### 0x20 - AND (Logical AND, 32)
**Size:** 1 byte | **Format:** `[80]`

- `stack[-2] = (stack[-2] != 0 && stack[-1] != 0) ? 1 : 0; pop 1`

Short-circuit: if either operand is 0, result is 0.

---

### 0x21 - OR (Logical OR, 33)
**Size:** 1 byte | **Format:** `[84]`

- `stack[-2] = (stack[-2] != 0 || stack[-1] != 0) ? 1 : 0; pop 1`

---

### 0x22 - NEQ_I (Integer Not-Equal, 34)
**Size:** 1 byte | **Format:** `[88]`

- `stack[-2] = (stack[-2] != stack[-1]) ? 1 : 0; pop 1`

---

### 0x23 - EQ_I (Integer Equal, 35)
**Size:** 1 byte | **Format:** `[8C]`

- `stack[-2] = (stack[-2] == stack[-1]) ? 1 : 0; pop 1`

---

### 0x24 - LT_I (Integer Less-Than, 36)
**Size:** 1 byte | **Format:** `[90]`

- `stack[-2] = (stack[-2] < stack[-1]) ? 1 : 0; pop 1` (signed)

---

### 0x25 - LE_I (Integer Less-or-Equal, 37)
**Size:** 1 byte | **Format:** `[94]`

- `stack[-2] = (stack[-2] <= stack[-1]) ? 1 : 0; pop 1` (signed)

---

### 0x26 - GT_I (Integer Greater-Than, 38)
**Size:** 1 byte | **Format:** `[98]`

- `stack[-2] = (stack[-2] > stack[-1]) ? 1 : 0; pop 1` (signed)

---

### 0x27 - GE_I (Integer Greater-or-Equal, 39)
**Size:** 1 byte | **Format:** `[9C]`

- `stack[-2] = (stack[-2] >= stack[-1]) ? 1 : 0; pop 1` (signed)

---

### 0x28 - NEQ_F (Float Not-Equal, 40)
**Size:** 1 byte | **Format:** `[A0]`

- `stack[-2] = ((float)stack[-1] != (float)stack[-2]) ? 1 : 0; pop 1`

Uses `fcomp/fnstsw/test ah,44h/jnp` (x87 status word check).

---

### 0x29 - EQ_F (Float Equal, 41)
**Size:** 1 byte | **Format:** `[A4]`

- `stack[-2] = ((float)stack[-1] == (float)stack[-2]) ? 1 : 0; pop 1`

---

### 0x2A - LT_F (Float Less-Than, 42)
**Size:** 1 byte | **Format:** `[A8]`

- `stack[-2] = ((float)stack[-2] < (float)stack[-1]) ? 1 : 0; pop 1`

Uses `fcomp/test ah,41h/jnz` -- checks for unordered or equal.

---

### 0x2B - LE_F (Float Less-or-Equal, 43)
**Size:** 1 byte | **Format:** `[AC]`

- `stack[-2] = ((float)stack[-2] <= (float)stack[-1]) ? 1 : 0; pop 1`

---

### 0x2C - GT_F (Float Greater-Than, 44)
**Size:** 1 byte | **Format:** `[B0]`

- `stack[-2] = ((float)stack[-2] > (float)stack[-1]) ? 1 : 0; pop 1`

---

### 0x2D - GE_F (Float Greater-or-Equal, 45)
**Size:** 1 byte | **Format:** `[B4]`

- `stack[-2] = ((float)stack[-2] >= (float)stack[-1]) ? 1 : 0; pop 1`

---

### 0x2E - JNZ (Jump if Non-Zero / Branch True, 46)
**Size:** 3 bytes | **Format:** `[B8] [offsetLo] [offsetHi]`

Conditional branch -- jumps if top of stack is non-zero:
- Pops top of stack
- If value != 0: `IP += (int16)(bytes[1] | (bytes[2] << 8))`
- If value == 0: `IP += 3` (skip to next instruction)

The offset is **signed** -- allows both forward and backward jumps (loops).

---

### 0x2F - JZ (Jump if Zero / Branch False, 47)
**Size:** 3 bytes | **Format:** `[BC] [offsetLo] [offsetHi]`

Conditional branch -- jumps if top of stack is zero:
- Pops top of stack
- If value == 0: `IP += offset` (same encoding as JNZ)
- If value != 0: `IP += 3`

---

### 0x30 - JMP (Unconditional Jump, 48)
**Size:** 3 bytes | **Format:** `[C0] [offsetLo] [offsetHi]`

Unconditional branch:
- `IP += (int16)(bytes[1] | (bytes[2] << 8))`

Does NOT pop from the stack.

---

### 0x31 - SET_EXCEPT (Set Exception/Type Info, 49)
**Size:** 3 bytes | **Format:** `[C4] [valueLo] [valueHi]`

Stores a 16-bit value into the current frame's type field:
- `*(E56164 + 0x18) = (uint16)(bytes[1] | (bytes[2] << 8))`

Used for exception handling or type tagging on the current stack frame.

---

### 0x32 - F2I (Float to Integer, 50)
**Size:** 1 byte | **Format:** `[C8]`

Converts the float at top of stack to an integer:
- Uses `__ftol2_sse` for conversion
- `stack[-1] = (int)(float)stack[-1]`

---

### 0x33 - I2F (Integer to Float, 51)
**Size:** 1 byte | **Format:** `[CC]`

Converts the integer at top of stack to a float:
- Uses `fild/fstp`
- `stack[-1] = (float)(int)stack[-1]`

---

### 0x34 - ADDR_CALC (Address Calculation, 52)
**Size:** 3 bytes | **Format:** `[D0] [offsetLo] [offsetHi]`

Adjusts the top of stack by adding a base-relative offset:
- `offset = bytes[1] | (bytes[2] << 8)`
- `addr = E56160[regBank] + offset`
- `stack[-1] += (addr - E56160)` -- adds the offset relative to base[0]

Used for pointer arithmetic within VM objects.

---

### 0x35 - SET_DEBUG (Set Debug Value, 53)
**Size:** 5 bytes | **Format:** `[D4] [b1] [b2] [b3] [b4]`

Sets the debug/trace value:
- `E56218 = (b4 << 24) | (b3 << 16) | (b2 << 8) | b1`

Used for source-level debugging (likely stores line number or debug ID).

---

### 0x36 - NOP5a (cases 54, 63)
**Size:** 5 bytes | **Format:** `[D8]/[FC]`

No operation -- just advances IP by 5 bytes. Used as padding.
Cases 54 (0x36) and 63 (0x3F) both map here.

---

### 0x37 - NOP1 (No Operation, 55)
**Size:** 1 byte | **Format:** `[DC]`

No operation -- advances IP by 1.

---

### 0x38 - BREAKPOINT (Debugger Break, 56)
**Size:** 1 byte | **Format:** `[E0]`

Triggers a debugger breakpoint:
1. Calls `sub_4E0E10(dword_E56220)` -- checks if debugger is attached
2. If attached, calls `sub_4E0E90(dword_E5622C, result)` -- notifies debugger

---

### 0x39 - TRACE (Debug Trace, 57)
**Size:** 5 bytes | **Format:** `[E4] [b1] [b2] [b3] [b4]`

Emits a debug trace:
- Encodes a 32-bit value from bytes[1..4]
- Calls `sub_52C0A0(value)` -- a thunk to the trace handler in .kallis

---

### 0x3A - LOAD (Load from Address, 58)
**Size:** 3 bytes | **Format:** `[E8] [offsetLo] [offsetHi]`

Loads a DWORD from a computed address and pushes it:
- `offset = bytes[1] | (bytes[2] << 8)`
- `addr = E56160[regBank] + offset`
- `push *(DWORD*)addr`

---

### 0x3B - STORE (Store to Address, 59)
**Size:** 3 bytes | **Format:** `[EC] [offsetLo] [offsetHi]`

Pops a value and stores it to a computed address:
- `offset = bytes[1] | (bytes[2] << 8)`
- `addr = E56160[regBank] + offset`
- `*(DWORD*)addr = pop()`

---

### 0x3C - LOAD_IND (Load Indirect, 60)
**Size:** 3 bytes | **Format:** `[F0] [offsetLo] [offsetHi]`

Indirect load: pops a stack value as additional offset:
- Pops stack top as `indexOffset`
- `addr = E56160[regBank] + offset + indexOffset`
- Pushes `*(DWORD*)addr`

Used for array/field access where the index is computed at runtime.

---

### 0x3D - STORE_IND (Store Indirect, 61)
**Size:** 3 bytes | **Format:** `[F4] [offsetLo] [offsetHi]`

Indirect store: pops index offset AND value:
- Pops stack top as `indexOffset`
- `addr = E56160[regBank] + offset + indexOffset`
- Pops next value and stores: `*(DWORD*)addr = pop()`

---

### 0x3E - CALL_OBJ (Call Object Method with Bool Result, 62)
**Size:** 5 bytes | **Format:** `[F8] [funcIdxLo] [funcIdxHi] [argCountLo] [argCountHi]`

Calls a native object method that returns a boolean:
- Calls `sub_52D370(funcPtr, argCount)`:
  1. Gets VM object from stack
  2. Resolves native pointer from `vmObj+16`
  3. Sets type tag from `*(vmObj+20) + 12`
  4. Calls `funcPtr(nativeObj)`
  5. Pushes result as 0 or 1

---

### 0x3F - NOP5b (case 63)
**Size:** 5 bytes

See 0x36 -- shares the same handler. Advances IP by 5, does nothing.

---

## Opcode Quick Reference

| Op | Hex | Mnemonic | Size | Category |
|----|-----|----------|------|----------|
| 00 | 00 | RET | 5 | Control |
| 01 | 04 | CALL_SCRIPT | 5 | Control |
| 02 | 08 | CALL_NATIVE | 5 | Control |
| 03 | 0C | CALL_METHOD | 5 | Control |
| 04 | 10 | CALL_STATIC | 5 | Control |
| 05 | 14 | CALL_VIRT | 5 | Control |
| 06 | 18 | POP_N | 3 | Stack |
| 07 | 1C | PUSH_N | 3 | Stack |
| 08 | 20 | PUSH_FRAME | 1 | Stack |
| 09 | 24 | SAVE_STATE | 5 | Control |
| 0A | 28 | COPY_TO_STK | 5 | Memory |
| 0B | 2C | COPY_FROM | 5 | Memory |
| 0C | 30 | PUSH_IMM | 5 | Stack |
| 0D | 34 | LOAD_OFFSET | 5 | Memory |
| 0E | 38 | STORE_OFF | 5 | Memory |
| 0F | 3C | ADD_I | 1 | Math |
| 10 | 40 | SUB_I | 1 | Math |
| 11 | 44 | MUL_I | 1 | Math |
| 12 | 48 | DIV_I | 1 | Math |
| 13 | 4C | MOD_I | 1 | Math |
| 14 | 50 | NEG_I | 1 | Math |
| 15 | 54 | INC_I | 1 | Math |
| 16 | 58 | DEC_I | 1 | Math |
| 17 | 5C | ADD_F | 1 | Math |
| 18 | 60 | SUB_F | 1 | Math |
| 19 | 64 | MUL_F | 1 | Math |
| 1A | 68 | DIV_F | 1 | Math |
| 1B | 6C | (default) | - | NOP |
| 1C | 70 | NEG_F | 1 | Math |
| 1D | 74 | INC_F | 1 | Math |
| 1E | 78 | DEC_F | 1 | Math |
| 1F | 7C | NOT | 1 | Logic |
| 20 | 80 | AND | 1 | Logic |
| 21 | 84 | OR | 1 | Logic |
| 22 | 88 | NEQ_I | 1 | Compare |
| 23 | 8C | EQ_I | 1 | Compare |
| 24 | 90 | LT_I | 1 | Compare |
| 25 | 94 | LE_I | 1 | Compare |
| 26 | 98 | GT_I | 1 | Compare |
| 27 | 9C | GE_I | 1 | Compare |
| 28 | A0 | NEQ_F | 1 | Compare |
| 29 | A4 | EQ_F | 1 | Compare |
| 2A | A8 | LT_F | 1 | Compare |
| 2B | AC | LE_F | 1 | Compare |
| 2C | B0 | GT_F | 1 | Compare |
| 2D | B4 | GE_F | 1 | Compare |
| 2E | B8 | JNZ | 3 | Branch |
| 2F | BC | JZ | 3 | Branch |
| 30 | C0 | JMP | 3 | Branch |
| 31 | C4 | SET_EXCEPT | 3 | Control |
| 32 | C8 | F2I | 1 | Convert |
| 33 | CC | I2F | 1 | Convert |
| 34 | D0 | ADDR_CALC | 3 | Memory |
| 35 | D4 | SET_DEBUG | 5 | Debug |
| 36 | D8 | NOP5a | 5 | NOP |
| 37 | DC | NOP1 | 1 | NOP |
| 38 | E0 | BREAKPOINT | 1 | Debug |
| 39 | E4 | TRACE | 5 | Debug |
| 3A | E8 | LOAD | 3 | Memory |
| 3B | EC | STORE | 3 | Memory |
| 3C | F0 | LOAD_IND | 3 | Memory |
| 3D | F4 | STORE_IND | 3 | Memory |
| 3E | F8 | CALL_OBJ | 5 | Control |
| 3F | FC | NOP5b | 5 | NOP |

---

## Native Call Dispatch Functions

| Address | Name | Call Convention | Description |
|---------|------|----------------|-------------|
| 0x52D240 | CallNativeCdecl | cdecl, no args | `funcPtr()` -- native reads from VM stack |
| 0x52D280 | CallNativeMethod | thiscall via vtable | `(**vt)(vt, nativeObj)` |
| 0x52D300 | CallNativeStatic | cdecl(nativeObj) | `funcPtr(*(frame+16))` |
| 0x52D340 | CallNativeVirtual | thiscall via vtable + frame | `(**vt)(vt, *(frame+16))` |
| 0x52D370 | CallObjMethod | cdecl, bool return | `funcPtr(nativeObj)` -> push 0/1 |
| 0x52D1F0 | ReturnHandler | - | Pops args, copies returns, restores frame |

---

## Related

- [KALLIS_VM.md](KALLIS_VM.md) -- High-level VM architecture
- [VM_STACK_SYSTEM.md](VM_STACK_SYSTEM.md) -- Stack system details
- [VM_BYTECODE_FORMAT.md](VM_BYTECODE_FORMAT.md) -- Script file format
- [VM_FUNCTION_TABLE.md](VM_FUNCTION_TABLE.md) -- Function table structure
- [VM_OBJECT_SYSTEM.md](VM_OBJECT_SYSTEM.md) -- Object reference system
