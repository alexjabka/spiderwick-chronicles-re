# sub_52D5A0 -- VMStackPush

**Address:** 0x52D5A0 (Spiderwick.exe+12D5A0) | **Calling convention:** __thiscall (ECX = variant struct*)

---

## Purpose

Pushes a typed value from a variant structure onto the VM return stack at `dword_E5616C`. Implements a 9-case type switch handling int, float, bool, vec3, vec4, string, and other types. The return stack grows upward.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | void* | Pointer to a variant structure: `[+0]=type, [+4..]=value` |

**Returns:** `float*` (return value is not semantically meaningful)

---

## Decompiled Pseudocode

```c
void __thiscall VMStackPush(void *this)
{
    int type = *(DWORD*)this;  // type tag at offset 0

    switch (type)
    {
    case 0:  // INT
        *(DWORD*)dword_E5616C = *((DWORD*)this + 1);  // push int value
        dword_E5616C += 4;
        break;

    case 1:  // FLOAT
        *(float*)dword_E5616C = *((float*)this + 1);   // push float value
        dword_E5616C += 4;
        break;

    case 2:  // BOOL
        sub_52CA80(*((BYTE*)this + 4));  // push bool via helper
        break;

    case 3:  // (default/unused)
        break;

    case 4:  // VEC3 (variant A)
        *(float*)dword_E5616C = *((float*)this + 1);   // X
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 2);   // Y
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 3);   // Z
        dword_E5616C += 4;
        break;

    case 5:  // VEC3 (variant B)
        // Same as case 4: push 3 floats
        *(float*)dword_E5616C = *((float*)this + 1);
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 2);
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 3);
        dword_E5616C += 4;
        break;

    case 6:  // VEC4 (variant A)
        *(float*)dword_E5616C = *((float*)this + 1);   // X
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 2);   // Y
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 3);   // Z
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 4);   // W
        dword_E5616C += 4;
        break;

    case 7:  // VEC4 (variant B)
        // Same as case 6: push 4 floats
        *(float*)dword_E5616C = *((float*)this + 1);
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 2);
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 3);
        dword_E5616C += 4;
        *(float*)dword_E5616C = *((float*)this + 4);
        dword_E5616C += 4;
        break;

    case 8:  // STRING
        sub_52CBF0(*((DWORD*)this + 1));  // push string via helper
        break;
    }
}
```

---

## Type Tags

| Type | Value | Size on Stack | Description |
|------|-------|---------------|-------------|
| 0 | INT | 4 bytes | 32-bit integer |
| 1 | FLOAT | 4 bytes | 32-bit float |
| 2 | BOOL | (via helper) | Boolean value |
| 3 | -- | 0 bytes | Unused/no-op |
| 4 | VEC3_A | 12 bytes | 3-component float vector (variant A) |
| 5 | VEC3_B | 12 bytes | 3-component float vector (variant B) |
| 6 | VEC4_A | 16 bytes | 4-component float vector (variant A) |
| 7 | VEC4_B | 16 bytes | 4-component float vector (variant B) |
| 8 | STRING | (via helper) | String reference |

The A/B variants for vec3 and vec4 may represent different source types (e.g., color vs. position) that are pushed identically.

---

## Return Stack Operation

The return stack grows **upward**:

```c
*(DWORD*)dword_E5616C = value;
dword_E5616C += 4;   // advance pointer up
```

This is the opposite direction from the argument stack, which grows downward.

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_52EB60` (VMCallWithArgs) | Pushes arguments to the return stack before calling VM functions |

---

## Key Global Variables

| Address | Type | Purpose |
|---------|------|---------|
| `dword_E5616C` | DWORD | VM return stack pointer (grows upward) |

---

## Notes

1. The variant structure is 20 bytes maximum: 4 bytes for the type tag + up to 16 bytes for a vec4 value.

2. Helper functions `sub_52CA80` (bool push) and `sub_52CBF0` (string push) handle the more complex push operations that require encoding.

3. Cases 4/5 and 6/7 have identical code -- the decompiler shows them as sharing the same implementation via fall-through labels.

---

## Related Documentation

- [../KALLIS_VM.md](../KALLIS_VM.md) -- VM stack system overview
- [../VM_STACK_OPERATIONS.md](../VM_STACK_OPERATIONS.md) -- Stack operations deep dive
- [sub_52D820_VMStackPop.md](sub_52D820_VMStackPop.md) -- Pop from arg stack (inverse operation)
- [sub_52CC70_PushResult.md](sub_52CC70_PushResult.md) -- Simpler push (bool only)
- [sub_52EB60_VMCallWithArgs.md](sub_52EB60_VMCallWithArgs.md) -- Primary caller
