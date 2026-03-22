# sub_52EB60 --- VMCallWithArgs

**Address:** 0x52EB60 (Spiderwick.exe+12EB60) | **Calling convention:** __cdecl (variadic)

---

## Purpose

Extended version of [VMCall](sub_52EB40_VMCall.md) that supports passing typed arguments to the script function. After resolving the function name, it pushes each argument onto the VM stack using `sub_52D5A0` before executing.

Each argument is passed as a 20-byte `VMArg` struct (5 DWORDs on the stack due to cdecl), containing a type tag and the value.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `vmObject` | void* | VM object instance to call the function on |
| `functionName` | const char* | Name of the script function to call |
| `argCount` | int | Number of VMArg arguments following |
| `...` | VMArg[] | Variable number of 20-byte VMArg structs (each passed as 5 DWORDs) |

**Returns:** `char` (bool) --- 0 = function not found, 1 = function executed successfully

---

## VMArg Structure (20 bytes)

```c
struct VMArg {
    int type;           // +0x00: type tag (see table below)
    union {
        int    intVal;  // +0x04: integer value (type 0)
        float  fltVal;  // +0x04: float value (type 1)
        int    boolVal; // +0x04: bool value (type 2)
        float  vec3[3]; // +0x04: xyz components (types 4/5)
        float  vec4[4]; // +0x04: xyzw components (types 6/7)
        char*  strVal;  // +0x04: string pointer (type 8)
    } value;
};
```

### VMArg Type Tags

| Type | Name | Size of Value | Description |
|------|------|---------------|-------------|
| 0 | INT | 4 bytes | Integer value |
| 1 | FLOAT | 4 bytes | Float value |
| 2 | BOOL | 4 bytes | Boolean (0/1) |
| 4 | VEC3_A | 12 bytes | 3D vector (variant A) |
| 5 | VEC3_B | 12 bytes | 3D vector (variant B) |
| 6 | VEC4_A | 16 bytes | 4D vector/quaternion (variant A) |
| 7 | VEC4_B | 16 bytes | 4D vector/quaternion (variant B) |
| 8 | STRING | 4 bytes (ptr) | String pointer |

---

## Decompiled Pseudocode

```c
char __cdecl VMCallWithArgs(void *vmObject, const char *functionName, int argCount, ...)
{
    // Resolve function name to bytecode entry point
    VMResolveFunction(vmObject, functionName);     // sub_52D920

    // Push each argument onto the VM stack
    va_list args;
    va_start(args, argCount);
    for (int i = 0; i < argCount; i++)
    {
        VMArg arg = va_arg(args, VMArg);           // 20 bytes = 5 DWORDs
        sub_52D5A0(&arg);                          // push typed arg onto VM stack
    }
    va_end(args);

    // Execute the resolved function
    return VMExecute();                            // sub_52EA70
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x52EB60` | Entry point |
| `0x52D5A0` | PushVMArg --- pushes a typed argument onto the VM stack |

---

## Called By

| Caller | Context |
|--------|---------|
| Various engine functions | Script calls that require passing data (positions, entity refs, etc.) |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x52D920` | [VMResolveFunction](sub_52D920_VMResolveFunction.md) | Resolves function name --> bytecode entry point |
| `0x52D5A0` | PushVMArg | Pushes a typed 20-byte arg onto the VM stack |
| `0x52EA70` | [VMExecute](sub_52EA70_VMExecute.md) | Executes the resolved bytecode |

---

## Notes / Caveats

1. **Each VMArg is 20 bytes / 5 DWORDs on the stack.** Due to __cdecl variadic calling, the compiler lays out each VMArg as 5 consecutive DWORD pushes. The total stack usage for args is `20 * argCount` bytes plus the three fixed parameters.

2. **Same script-only limitation as VMCall.** Only resolves bytecode functions, not native methods or native-registered functions.

3. **The vec3/vec4 type variants (4/5 and 6/7)** likely differ in coordinate space or interpretation (e.g., world vs local), but the exact distinction is not yet confirmed.

4. **PushVMArg (sub_52D5A0)** handles the type-specific encoding to convert the VMArg struct into the VM stack's internal representation. This is distinct from the simpler PopInt/PopFloat/PopString functions which operate on individual typed values.

5. **Related functions:**
   - [VMCall](sub_52EB40_VMCall.md) (sub_52EB40) --- simpler version without arguments
   - [VMResolveFunction](sub_52D920_VMResolveFunction.md) (sub_52D920) --- function name resolution
   - [VMExecute](sub_52EA70_VMExecute.md) (sub_52EA70) --- bytecode execution
   - [PopInt](sub_52C610_PopInt.md) / [PopFloat](sub_52C6A0_PopFloat.md) / [PopString](sub_52D7D0_PopString.md) --- corresponding pop operations in native handlers
