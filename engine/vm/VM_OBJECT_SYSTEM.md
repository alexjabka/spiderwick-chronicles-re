# Kallis VM Object System

**Status:** Reversed from interpreter, stack ops, and object init code

---

## Overview

The Kallis VM has a rich object system that bridges VM-managed objects (script objects) with engine-native objects (C++ game objects). Every game entity -- characters, triggers, items, etc. -- has both a VM representation and a native representation linked through a pointer chain.

---

## Object Reference Base

| Address | Name | Purpose |
|---------|------|---------|
| 0xE56160 | g_ObjectBase[4] | 16-byte array of 4 base pointers |
| 0xE56164 | g_FrameContext | Current frame's VM object pointer |

The 4-entry base array at `E56160` is selected by the low 2 bits of each opcode byte (the "register bank"):
- `regBank = opcodeByte & 3`
- `base = E56160[regBank * 4]` (i.e., the globals at 0xE56160, 0xE56164, 0xE56168, 0xE5616C)

**However**, in practice:
- `E56160[0]` (0xE56160 itself) = **primary object base** -- the root of the VM object memory pool
- `E56160[1]` (0xE56164) = **current frame context** -- points to the active VM object for the current call
- `E56160[2]` (0xE56168) = **arg stack base** (also used as object base selector)
- `E56160[3]` (0xE5616C) = **eval stack top** (also used as object base selector)

The primary object base (`E56160[0]`) is the most critical -- all VM object references are byte offsets from this address.

---

## VM Object Descriptor

Each VM object has a descriptor in the object pool. Based on analysis of the interpreter, stack ops, and init code:

### Layout (at least 36 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| +0x00 | 4 | name_hash_1 | Primary name hash (for VMRegisterMethod lookup) |
| +0x04 | 4 | name_hash_2 | Secondary name hash |
| +0x08 | 4 | name_id | String table ID (for sub_58C190 name resolution) |
| +0x0C | 4 | debug_name | Cached resolved name string pointer |
| +0x10 | 4 | native_obj | Pointer to native C++ object (set by sub_52EC30) |
| +0x14 | 4 | vtable_ptr | Pointer to class vtable / function table |
| +0x18 | 2 | type_tag | Type tag (set by SET_EXCEPT opcode 0x31) |
| +0x1A | 2 | flags | Status flags (see below) |
| +0x1C | 4 | method_table | Pointer to method dispatch table |
| +0x20 | 4 | save_data | Pointer to save/load state data |

### Flags Field (+0x1A)

| Bit | Mask | Meaning |
|-----|------|---------|
| 0 | 0x0001 | In-use / active (set by CALL_STATIC/CALL_VIRT) |
| 1 | 0x0002 | Alive flag (if clear AND 0x4000 clear -> object is dead) |
| 14 | 0x4000 | Initialized (set by sub_52EC30 after Init script runs) |

The flags are checked by `VMPopObject` (0x52D820):
```c
short flags = *(vmObj + 26);  // +0x1A
if ((flags & 0x4000) == 0 && (flags & 2) != 0) {
    // Object not initialized but marked alive -> treat as dead
    *out = 0;
    return;
}
```

---

## Object Reference Encoding

VM object references on the stack are **byte offsets** from `E56160[0]`, not raw pointers.

### Encoding: Native Pointer -> VM Reference

```c
// sub_52CE00 (VMReturnObjDirect): Direct encoding
if (nativePtr) {
    vmRef = nativePtr - E56160[0];
} else {
    vmRef = 0;  // null
}
*E5616C = vmRef;
E5616C += 4;

// sub_52CE40 (VMReturnObj): Via game object's +0xA8 field
if (gameObj && *(gameObj + 0xA8)) {
    vmObjAddr = *(gameObj + 0xA8);
    vmRef = vmObjAddr - E56160;
} else {
    vmRef = 0;
}
*E5616C = vmRef;
E5616C += 4;
```

### Decoding: VM Reference -> Native Pointer

```c
// sub_52C860 (VMPopObjRef): Basic decode
vmRef = pop_arg_stack();
if (vmRef) {
    nativePtr = E56160 + 4 * (vmRef / 4);  // aligned to 4 bytes
} else {
    nativePtr = NULL;
}

// sub_52D820 (VMPopObject): Validated decode with class chain walk
vmRef = pop_arg_stack();
vmObj = E56160[0] + 4 * (vmRef / 4);
// ... validate flags ...
nativeObj = *(vmObj + 16);  // Follow to native object
// ... walk class chain ...
```

### The /4 * 4 Alignment

The `vmRef / 4` operation uses signed integer division:
```asm
cdq                  ; sign-extend EAX into EDX
and     edx, 3       ; mask: keeps sign correction
add     eax, edx     ; adjust for negative values
sar     eax, 2       ; divide by 4
```
Then `E56160 + result * 4` effectively rounds to the nearest 4-byte boundary. This handles both positive and negative offsets correctly.

---

## Object Lifecycle

### 1. Creation (sub_52EC30 at 0x52EC30)

Called when a new game object is created (e.g., character creation):

```c
int __cdecl VMInitObject(int vmTypeInfo, int nativeObject) {
    // Set type tag from class vtable
    *(vmTypeInfo + 24) = *(*(vmTypeInfo + 20) + 12);  // type_tag from vtable

    // Link native object
    *(vmTypeInfo + 16) = nativeObject;

    // Resolve debug name
    if (E561E4)
        name = sub_58C190(*(vmTypeInfo + 8));
    else
        name = NULL;
    *(vmTypeInfo + 12) = name;

    // If native object exists, set up bidirectional link
    if (nativeObject) {
        *(nativeObject + 0xA8) = vmTypeInfo;  // Native -> VM link
        sub_441140(nativeObject + 4, debugName);
        E561D4(nativeObject);  // Object dispatch (sub_4D7790 -> sub_552800)
    }

    // Mark as initialized
    *(vmTypeInfo + 26) |= 0x4000;

    // Call script "Init" function
    sub_52D920(vmTypeInfo, "Init");
    sub_52EA70();  // Execute
}
```

### 2. The Bidirectional Link

```
Game Object (native C++)          VM Object Descriptor
+0x00: vtable                     +0x00: name hashes
+0x04: name                       +0x10: native_obj -----> Game Object
...                               +0x14: vtable_ptr
+0xA8: vm_obj -----------------> +0x18: type_tag
...                               +0x1A: flags
```

- `gameObject + 0xA8` -> VM object descriptor address
- `vmObject + 0x10` -> native game object address

### 3. Calling VM Functions on Objects

```c
// From native code:
int vmObj = *(gameObject + 0xA8);
sub_52EB40(vmObj, "OnActivated");  // VMCall

// What happens internally:
// 1. sub_52D920(vmObj, "OnActivated") resolves the function
// 2. Sets E5620C to the bytecode address
// 3. sub_52EA70() saves state, sets up frame
// 4. sub_52D9C0(E5620C + 12, 0) runs the bytecode
```

### 4. Object Method Dispatch (in interpreter)

When opcode 0x03 (CALL_METHOD) executes:
```c
// Get "self" from stack
vmObj = E56160[0] + 4 * (argStackValue >> 2);

// Get native counterpart
nativeObj = *(vmObj + 16);
if (!nativeObj) {
    const char* name = sub_58C190(*(vmObj + 8));
    printf("Object type had no native counterpart: %s\n", name);
}

// Call through vtable
(**funcVtable)(funcVtable, nativeObj);
```

### 5. Object Destruction

Objects are not explicitly destroyed through a single function. Instead:
- The flag at `+0x1A` is used to mark objects as dead (clear bit 0x4000 or set bit 0x2)
- `VMPopObject` (0x52D820) checks these flags and returns NULL for dead objects
- The VM's garbage collection (inside .kallis) handles actual memory cleanup

---

## Object Base Initialization

### VMExecute (sub_52EA70) State Save/Restore

Before executing bytecode, VMExecute saves and restores the 16-byte object base:

```c
char saved[16];
memcpy(saved, &E56170, 16);         // Save current state (E56170 = E56160 + 16 offset area)
// ... execute bytecode ...
memcpy(E56160, saved, 16);          // Restore
```

Wait -- looking more carefully: the save area is `unk_E56170` (16 bytes starting at 0xE56170), which is AFTER the base array. The copy is:
```c
memcpy(localSave, &unk_E56170, 16);  // Save bytes at E56170-E5617F
// ... execute ...
memcpy(E56160, localSave, 16);       // Restore into E56160-E5616F
```

This means the 16 bytes at `E56170` are a backup area, and they get restored INTO `E56160[0..3]` after execution. This ensures the object base is clean after each VMExecute call.

---

## The Object Dispatch Function

| Address | Name | Purpose |
|---------|------|---------|
| 0xE561D4 | g_ObjectDispatch | Function pointer for object initialization |

Set during startup: `sub_52C540(sub_4D7790)` at 0x431B21

```c
// sub_4D7790:
int __cdecl ObjectDispatch(int nativeObject) {
    return sub_552800(nativeObject);  // thunk to .kallis
}
```

Called by `sub_52EC30` after linking native and VM objects. The function in `.kallis` handles registering the native object with the VM's object manager.

---

## VMState Save/Restore (sub_52E780)

The debug/E&C handler at 0x52E780 provides insight into VM state during execution:

| Case | Action |
|------|--------|
| 0 | Connect -- print "Connect", clear debugger flag |
| 1 | Step -- call sub_52D500 with execution state |
| 2 | Read stack data -- follows frame chain, copies stack data |
| 3 | Read frame data -- copies from E56164 + offset |
| 4 | Pause -- set step flags |
| 5-7 | Break -- set break flags |
| 9 | Disconnect -- call sub_52D450 (reset state), print "Disconnect" |

The "read stack data" case (2) reveals that stack frames form a linked list through `E56168`:
```c
int* frame = E56168;
int depth = *(a3 + 12);  // How many frames deep
for (int i = 0; i < depth; i++) {
    frame = *frame;  // Follow link
}
// Read data: memcpy(buf, frame + *(a3+4)/4, *(a3+8))
```

---

## Related

- [VM_INTERPRETER.md](VM_INTERPRETER.md) -- Opcodes that use objects
- [VM_STACK_SYSTEM.md](VM_STACK_SYSTEM.md) -- Object encoding on stack
- [VM_FUNCTION_TABLE.md](VM_FUNCTION_TABLE.md) -- Class table structure
- [KALLIS_VM.md](KALLIS_VM.md) -- Architecture overview
