# Kallis VM Stack System - Complete Reference

**Status:** Fully reversed (all stack functions documented)

---

## Overview

The Kallis VM uses a **dual-stack architecture** with additional support structures:

1. **Evaluation Stack** (aka Return Stack) -- used by the interpreter for computation and return values
2. **Argument Stack** -- used for passing arguments from scripts to native handlers
3. **Call Frame Chain** -- a linked list of saved arg stack bases for function calls
4. **Object Reference Base** -- for converting between VM references and native pointers

---

## Global Variables

| Address | Name | Purpose |
|---------|------|---------|
| 0xE56168 | g_ArgStackBase | Current argument stack base (linked list head) |
| 0xE5616C | g_EvalStackTop | Evaluation stack top pointer (grows upward) |
| 0xE56200 | g_ArgStackIndex | Current argument index (decremented on pop) |
| 0xE56160 | g_ObjectBase[4] | Object reference base array (4 entries, 16 bytes) |
| 0xE56164 | g_FrameContext | Current frame context pointer |
| 0xE561CC | g_StackDepth | Stack depth counter (incremented by PUSH_N, decremented by POP_N) |
| 0xE561F8 | g_ScriptsLoaded | Scripts loaded flag (guard for push operations) |
| 0xE56204 | g_CurrentFunc | Current native function pointer |

---

## Evaluation Stack (dword_E5616C)

Grows **upward** in memory. The interpreter uses `esi` to cache `E5616C` for performance.

### Push Operation
```c
*(int*)E5616C = value;
E5616C += 4;
```

### Pop Operation (implicit)
```c
E5616C -= 4;
value = *(int*)E5616C;  // i.e., stack[-1] = *(esi-4)
```

The interpreter accesses stack values relative to `esi`:
- `[esi-4]` = top of stack (TOS)
- `[esi-8]` = second element
- Binary ops typically read `[esi-4]` and `[esi-8]`, store result in `[esi-8]`, then pop one

---

## Argument Stack (dword_E56168 / dword_E56200)

Used for passing arguments **from script to native handler**. Grows **downward** from a base address.

### Pop Argument
```c
value = *(E56168 - 4 * E56200);
E56200--;
```

Higher index = lower address. The index starts at the arg count and decrements toward 0.

### Push Argument (reverse, for setting up calls)
```c
E56200++;
*(E56168 - 4 * E56200) = value;
```

---

## Call Frame Chain (dword_E56168)

When calling a function, a new frame is created:

```c
// Before call:
*E5616C = E56168;        // Save old arg stack base on eval stack
E56168 = E5616C;         // New arg stack base = current eval stack top
E5616C += 4;             // Advance past saved value
```

This creates a linked list: each frame's first DWORD points to the previous frame's base.

On return (sub_52D1F0):
```c
newTop = E56168 - 4 * popCount;  // Pop args
E56168 = *E56168;                 // Follow link to previous frame
memcpy(newTop, returnValues, 4 * returnCount);
E5616C = newTop + 4 * returnCount;
```

---

## Stack Functions - Complete Reference

### Pop Functions (Argument Stack -> Native Code)

| Address | Name | Prototype | Description |
|---------|------|-----------|-------------|
| 0x52C610 | VMPopArg | `int __cdecl(int* out)` | Pop int (thunk to 0x4CE3CD) |
| 0x52C640 | VMPopArgAlt | `int __cdecl(int* out)` | Pop int (inline variant) |
| 0x52C670 | VMPopInt | `int __cdecl(int* out)` | Pop int (another variant) |
| 0x52C6A0 | VMPopFloat | `float* __cdecl(float* out)` | Pop float |
| 0x52C6D0 | VMPopBool | `bool __cdecl(bool* out)` | Pop bool (converts to true/false) |
| 0x52C700 | VMPopFloat3 | `float* __cdecl(float out[3])` | Pop 3 consecutive floats |
| 0x52C770 | VMPopVec3 | `float* __cdecl(float out[3])` | Pop vec3 (wrapper around PopFloat3) |
| 0x52C7A0 | VMPopFloat4 | `float* __cdecl(float out[4])` | Pop 4 consecutive floats |
| 0x52C830 | VMPopVec4 | `float* __cdecl(float out[4])` | Pop vec4 (wrapper around PopFloat4) |
| 0x52C860 | VMPopObjRef | `int __cdecl(int* out)` | Pop + resolve object reference |
| 0x52D7D0 | VMPopString | `int __cdecl(int strHandle)` | Pop string ref (thunk to 0x445061) |
| 0x52D820 | VMPopObject | `int __cdecl(int* out)` | Pop + validate object with class chain walk |

### Push Functions (Native Code -> Evaluation Stack)

| Address | Name | Prototype | Description |
|---------|------|-----------|-------------|
| 0x52CA80 | VMReturnBoolByte | `BOOL __cdecl(char val)` | Push bool (guarded by E561F8) |
| 0x52CAB0 | VMReturnVec3 | `float* __cdecl(float in[3])` | Push vec3 (guarded) |
| 0x52CB00 | VMReturnVec3Int | `void __cdecl(int handle)` | Push vec3 from internal format (guarded) |
| 0x52CB40 | VMReturnVec4 | `float* __cdecl(float in[4])` | Push vec4 (guarded) |
| 0x52CBA0 | VMReturnVec4Int | `void __cdecl(int handle)` | Push vec4 from internal format (guarded) |
| 0x52CBF0 | VMReturnString | `int __cdecl(int strPtr)` | Push string (thunk to off_1C8D674) |
| 0x52CC30 | VMReturnInt | `int __cdecl(int val)` | Push int to eval stack (thunk to 0x43C426) |
| 0x52CC50 | VMReturnFloat | `int __cdecl(float val)` | Push float to eval stack |
| 0x52CC70 | VMReturnBool | `BOOL __cdecl(char val)` | Push bool (0/1, unguarded) |
| 0x52CCD0 | VMReturnVec3Fmt | `float* __cdecl(int handle)` | Push vec3 from sub_5AD550 internal format |
| 0x52CD80 | VMReturnVec4Fmt | `float* __cdecl(int handle)` | Push vec4 from sub_5AD570 internal format |
| 0x52CE00 | VMReturnObjDirect | `int __cdecl(int objPtr)` | Push obj ref (raw ptr -> VM offset) |
| 0x52CE40 | VMReturnObj | `int __cdecl(int nativeObj)` | Push obj ref (via +0xA8 indirection) |

### Typed Push (sub_52D5A0 - VMStackPush)

**Address:** 0x52D5A0 | **Size:** 233 bytes

Pushes a typed value from a VMArg structure onto the evaluation stack. The VMArg struct is 20 bytes (5 DWORDs):

```c
struct VMArg {
    int type;           // +0x00: type tag
    union {             // +0x04:
        int   intVal;
        float floatVal;
        char  boolVal;
        float vec3[3];  // for types 4,5
        float vec4[4];  // for types 6,7
        int   strPtr;   // for type 8
    };
};
```

Type dispatch table:

| Type | Name | Push Size | Action |
|------|------|-----------|--------|
| 0 | int | 4 bytes | Push int value directly |
| 1 | float | 4 bytes | Push float value |
| 2 | bool | N/A | Call VMReturnBoolByte (0x52CA80) |
| 3 | (unused) | - | Returns immediately |
| 4 | vec3 | 12 bytes | Push 3 floats |
| 5 | vec3 | 12 bytes | Push 3 floats (same as 4) |
| 6 | vec4 | 16 bytes | Push 4 floats |
| 7 | vec4 | 16 bytes | Push 4 floats (same as 6) |
| 8 | string | N/A | Call VMReturnString (0x52CBF0) |

### Validated Object Pop (sub_52D820 - VMPopObject)

**Address:** 0x52D820 | **Size:** 153 bytes

Enhanced object pop with validation:

```c
int __cdecl VMPopObject(int* out) {
    int ref = *(E56168 - 4 * E56200--);
    if (ref == 0) { *out = 0; return 0; }

    int vmObj = E56160[0] + 4 * (ref / 4);
    if (vmObj == 0) { *out = 0; return 0; }

    short flags = *(vmObj + 26);
    if ((flags & 0x4000) == 0 && (flags & 2) != 0) {
        *out = 0;  // Object marked as dead/invalid
        return 0;
    }

    int nativeObj = *(vmObj + 16);
    *out = nativeObj;
    if (nativeObj == 0) return 0;

    // Walk class chain to check if it's the right type
    int classInfo = *(nativeObj + 12);
    if (classInfo == 0) return 0;

    while (classInfo != off_727EB0) {  // sentinel
        classInfo = *(classInfo + 4);  // next in chain
        if (classInfo == 0) return 0;
    }

    // Type matched -- clone/upgrade the object
    int upgraded = sub_55C7A0(1);
    *out = upgraded;
    return upgraded;
}
```

Key details:
- Checks flag 0x4000 (initialized) and flag 0x2 (alive/dead)
- Walks the class inheritance chain via `+4` links
- Compares against a sentinel class pointer at `off_727EB0`
- Called extensively by native handlers (200+ call sites)

---

## Object Reference Encoding

Object references on the VM stack are **relative byte offsets** from `dword_E56160[0]`, NOT raw pointers.

### Encoding (Native -> VM)
```c
// sub_52CE00: Direct encoding
vmRef = nativePtr - E56160;  // byte offset from base
*(E5616C) = vmRef;
E5616C += 4;

// sub_52CE40: Via +0xA8 indirection
vmObjAddr = *(nativeObj + 0xA8);
vmRef = vmObjAddr - E56160;
*(E5616C) = vmRef;
E5616C += 4;
```

### Decoding (VM -> Native)
```c
// sub_52C860: PopObjRef
vmRef = *(E56168 - 4 * E56200--);
if (vmRef != 0) {
    nativePtr = E56160 + 4 * (vmRef / 4);  // signed div, align to 4
} else {
    nativePtr = 0;  // null reference
}
```

The division by 4 then multiplication by 4 effectively performs a 4-byte alignment. The signed division uses the `cdq/and edx,3/add/sar` pattern.

---

## Internal Format Converters

Some push functions convert from engine-internal vector formats:

| Address | Function | Input | Output |
|---------|----------|-------|--------|
| 0x52CCD0 | VMReturnVec3Fmt | Handle (via sub_5AD550) | 3 floats on stack |
| 0x52CD80 | VMReturnVec4Fmt | Handle (via sub_5AD570) | 4 floats on stack |
| 0x52CB00 | VMReturnVec3Int | Handle (via sub_5AD550) | 3 floats on stack (guarded) |
| 0x52CBA0 | VMReturnVec4Int | Handle (via sub_5AD570) | 4 floats on stack (guarded) |

`sub_5AD550` and `sub_5AD570` are engine functions that resolve internal handles to `float*` arrays (3 and 4 elements respectively).

---

## Guard Pattern

Many push functions are guarded by the `E561F8` (scripts_loaded) flag:

```c
if (E561F8) {   // Scripts loaded?
    // ... perform the push ...
}
// Otherwise: silently do nothing
```

Functions with this guard: VMReturnBoolByte (0x52CA80), VMReturnVec3 (0x52CAB0), VMReturnVec4 (0x52CB40), VMReturnVec3Int (0x52CB00), VMReturnVec4Int (0x52CBA0).

Functions WITHOUT the guard (always push): VMReturnBool (0x52CC70), VMReturnInt (0x52CC30), VMReturnFloat (0x52CC50), VMReturnObjDirect (0x52CE00).

---

## Related

- [VM_INTERPRETER.md](VM_INTERPRETER.md) -- How the interpreter uses the stack
- [VM_OBJECT_SYSTEM.md](VM_OBJECT_SYSTEM.md) -- Object reference details
- [KALLIS_VM.md](KALLIS_VM.md) -- High-level architecture
# VM Stack Operations

**Status:** Confirmed (Session 3)

---

## Overview

The Kallis VM uses two distinct stacks for passing data between script code and native handler functions:

1. **Argument stack** — passes arguments from script to native handler (pop operations)
2. **Return stack** — passes results from native handler back to script (push operations)

All stack manipulation functions are native x86 code in the `.text` section and can be fully decompiled. They are NOT VM thunks.

---

## Argument Stack

| Global | Address | Purpose |
|--------|---------|---------|
| `dword_E56168` | `0xE56168` | Arg stack base address |
| `dword_E56200` | `0xE56200` | Arg stack index (current position) |

The argument stack grows **downward** from the base address. Higher index values correspond to lower memory addresses.

### Pop Operation

```c
value = *(dword_E56168 - 4 * dword_E56200);
dword_E56200--;
```

1. Read the value at `base - 4 * index`
2. Decrement the index

Implemented by:
- **sub_52C610** (PopArg) — pops via thunk to sub_4CE3CD, writes result to output pointer AND returns it
- **sub_52C640** (PopArgAlt) — inline pop, writes result to output pointer only

Both perform the same stack operation. sub_52C610 is a `jmp` thunk to sub_4CE3CD which contains the actual logic. sub_52C640 has the logic inlined.

### Push Operation

```c
dword_E56200++;
*(dword_E56168 - 4 * dword_E56200) = value;
```

1. Increment the index
2. Write the value at `base - 4 * index`

This is the inverse of pop — used to prepare arguments for VM function calls from native code.

---

## Return Stack

| Global | Address | Purpose |
|--------|---------|---------|
| `dword_E5616C` | `0xE5616C` | Return stack pointer |

The return stack grows **upward**. Results are written at the current pointer, then the pointer advances by 4.

### PushResult Operation (sub_52CC70)

```c
*(int*)dword_E5616C = (value != 0) ? 1 : 0;
dword_E5616C += 4;
```

Pushes a boolean result (0 or 1) to the return stack. The input byte is coerced to a strict boolean via `setnz`.

### PushObjRef Operation (sub_52CE40)

```c
if (obj && *(obj + 0xA8) != 0) {
    *(int*)dword_E5616C = *(obj + 0xA8) - dword_E56160;
    dword_E5616C += 4;
} else {
    *(int*)dword_E5616C = 0;
    dword_E5616C += 4;
}
```

Pushes an object reference onto the return stack. The reference is computed as the object's field at offset `+0xA8` minus the object reference base (`dword_E56160`). This is the inverse of PopObjRef's resolution.

---

## Object Reference System

| Global | Address | Purpose |
|--------|---------|---------|
| `dword_E56160` | `0xE56160` | Object reference resolution base |

Object references on the VM stack are **relative offsets** from `dword_E56160`, not raw pointers.

### PopObjRef Resolution (sub_52C860)

```c
int ref = *(dword_E56168 - 4 * dword_E56200);
dword_E56200--;
if (ref != 0) {
    // Signed division with rounding toward zero
    *out = dword_E56160 + 4 * (ref / 4);
} else {
    *out = 0;  // null reference
}
```

The `ref / 4` uses `cdq` + `and edx, 3` + `add eax, edx` + `sar eax, 2` — a signed division by 4 with rounding toward zero. The result is scaled back by 4 and added to the base, effectively aligning the reference to a 4-byte boundary.

### PushObjRef Encoding (sub_52CE40)

The inverse operation: takes a native object pointer, reads its field at offset `+0xA8`, and subtracts `dword_E56160` to produce the relative reference pushed to the return stack.

---

## Composite Operations

### PopVec3 (sub_52C770)

Pops 3 consecutive float values from the argument stack into a `float[3]` output buffer. Internally calls `PopFloat3` (sub_52C700) which performs 3 individual pops, then copies the results to the caller's buffer.

```c
void PopVec3(float *out) {
    float tmp[3];
    PopFloat3(tmp);       // sub_52C700: pops 3 floats
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}
```

---

## Stack Function Summary

| Address | Name | Stack | Direction | Type |
|---------|------|-------|-----------|------|
| `0x52C610` | PopArg | Arg | Pop | int/generic (thunk to 0x4CE3CD) |
| `0x52C640` | PopArgAlt | Arg | Pop | int/generic (inline) |
| `0x52C6A0` | PopFloat | Arg | Pop | float |
| `0x52C700` | PopFloat3 | Arg | Pop | 3x float |
| `0x52C770` | PopVec3 | Arg | Pop | vec3 wrapper |
| `0x52C860` | PopObjRef | Arg | Pop | object reference (resolved) |
| `0x52CC70` | PushResult | Return | Push | bool (0/1) |
| `0x52CE40` | PushObjRef | Return | Push | object reference (encoded) |
| `0x52D7D0` | PopString | Arg | Pop | string reference |
| `0x52D820` | PopVariant | Arg | Pop | variant value |

---

## Related

- [KALLIS_VM.md](KALLIS_VM.md) -- VM architecture overview
- [subs/sub_52C610_PopArg.md](subs/sub_52C610_PopArg.md) -- PopArg
- [subs/sub_52C640_PopArgAlt.md](subs/sub_52C640_PopArgAlt.md) -- PopArgAlt
- [subs/sub_52C770_PopVec3.md](subs/sub_52C770_PopVec3.md) -- PopVec3
- [subs/sub_52C860_PopObjRef.md](subs/sub_52C860_PopObjRef.md) -- PopObjRef
- [subs/sub_52CC70_PushResult.md](subs/sub_52CC70_PushResult.md) -- PushResult
- [subs/sub_52CE40_PushObjRef.md](subs/sub_52CE40_PushObjRef.md) -- PushObjRef
