# sub_52E660 -- VMRegisterMethod

**Address:** 0x52E660 (Spiderwick.exe+12E660) | **Calling convention:** __cdecl

---

## Purpose

Registers a native function as a method on a VM class. Hashes both the class name and method name strings, then searches the global class table at `dword_E561E0` for a matching class (by hash). Within the matched class, searches the method table for a matching method entry (by hash) and writes the function address into the entry's slot.

If the method slot is already occupied (not -1), prints a warning: `"{yWARNING}: Reregistration of the method "%s::%s"."`.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `className` | const char* | Name of the VM class (e.g., "ClPlayerObj", "ClCharacterObj") |
| `methodName` | const char* | Name of the method (e.g., "sauSetPlayerType") |
| `funcAddr` | int | Address of the native handler function |

**Returns:** `char` -- 1 if registration succeeded, 0 if not

---

## Decompiled Pseudocode

```c
char __cdecl VMRegisterMethod(const char *className, const char *methodName, int funcAddr)
{
    // Hash the class name (same algorithm as HashString/CreateWidget)
    int classHash = 0;
    if (className)
    {
        for (char *p = className; *p; p++)
            classHash += *p + (classHash << (*p & 7));
    }

    // Hash the method name
    int methodHash = 0;
    if (methodName)
    {
        for (char *p = methodName; *p; p++)
            methodHash += *p + (methodHash << (*p & 7));
    }

    // Search class table
    char registered = 0;
    int classTable = dword_E561E0;
    int classCount = *(DWORD*)(classTable + 4);   // class count at +4

    for (int i = 0; i < classCount; i++)
    {
        DWORD *classPtr = *(DWORD**)(classTable + 8 + i * 4);  // class ptrs at +8

        // Match by either hash slot (class has two name hashes)
        if (classPtr[0] == classHash || classPtr[1] == classHash)
        {
            // Get method table from class
            int methodTable = classPtr[7];  // class[+0x1C] = method table ptr
            int methodCount = *(DWORD*)(methodTable + 4);  // method count at +4

            // Search method entries (12 bytes each, starting at methodTable+16)
            DWORD *entry = (DWORD*)(methodTable + 16);  // first entry at +16 (+8 aligned to +16)

            for (int j = 0; j < methodCount; j++, entry += 3)
            {
                if (entry[-2] == methodHash)  // hash at entry-8 relative (entry[0] of 12-byte struct)
                {
                    if (entry[0] == -1)  // slot unoccupied (0xFFFFFFFF)
                    {
                        entry[0] = funcAddr;  // write function address
                        registered = 1;
                    }
                    else
                    {
                        // Already registered
                        printf("{yWARNING}: Reregistration of the method \"%s::%s\".\n",
                               className, methodName);
                    }
                }
            }
        }
    }
    return registered;
}
```

---

## Class Table Structure

The global class table at `dword_E561E0` has the following layout:

```
dword_E561E0 --> class table base:
  [+0x00] DWORD   (unknown / flags)
  [+0x04] DWORD   class_count         -- number of registered classes
  [+0x08] DWORD*  class_ptrs[count]   -- array of pointers to class descriptors
```

### Class Descriptor Layout

Each class descriptor pointed to from the table:

```
class[+0x00]  DWORD  name_hash_1   -- primary name hash
class[+0x04]  DWORD  name_hash_2   -- secondary name hash (alternate casing?)
class[+0x08]  ...    (unknown fields)
class[+0x1C]  DWORD  method_table  -- pointer to method table
```

The function checks BOTH `class[0]` and `class[1]` against the computed class hash. This dual-hash scheme may support case-insensitive or alternate-name matching.

### Method Table Layout

The method table pointed to from `class[+0x1C]`:

```
method_table[+0x00]  DWORD  (unknown)
method_table[+0x04]  DWORD  method_count    -- number of method entries
method_table[+0x08]  ...    (padding/alignment)

Method entries start at method_table + 8, each 12 bytes:
  entry[+0x00]  DWORD  method_name_hash  -- hash of the method name
  entry[+0x04]  DWORD  (unknown)         -- flags or metadata
  entry[+0x08]  DWORD  func_addr         -- native function address (-1 if unregistered)
```

---

## Called By

This function has many callers, both from `.text` and `.kallis`:

| Source | Address | Context |
|--------|---------|---------|
| Native `.text` | `0x44499C` | Early game initialization |
| Native `.text` | `0x5515DF` | System registration |
| Native `.text` | `0x5B385C` | Render system registration |
| Native `.text` | `0x5B94D8` | Audio system registration |
| Native `.text` | `0x5DCA8D` | Physics registration |
| Native `.text` | `0x5DEE04` | Collision registration |
| `.kallis` | `0x1C99F7C` | VM class method registration |
| `.kallis` | `0x1CABCA0` | VM class method registration |
| `.kallis` | `0x1CAEBD1` | VM class method registration |
| `.kallis` | `0x1CB4453` | VM class method registration |
| `.kallis` | Multiple | Many more registration sites |

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `dword_E561E0` | DWORD | g_VMClassTable | Pointer to the class table base |

---

## Notes

1. The `-1` sentinel (0xFFFFFFFF) in the function address slot indicates an unregistered method. When a `.kallis` script defines a method, it pre-populates the method table entries with hashes and -1 for the function address. Later, `VMRegisterMethod` fills in the actual native function addresses.

2. The warning string `"Reregistration of the method"` uses the `{y}` color prefix, which is the engine's debug console yellow coloring.

3. The hash algorithm used here is identical to `HashString` (sub_405380) and `CreateWidget` (sub_418290) -- the same inline code appears in all three.

4. This function differs from `RegisterScriptFunction` (sub_52EA10) which registers global (non-class) functions. `VMRegisterMethod` specifically targets methods on classes within the VM type system.

---

## Related Documentation

- [../KALLIS_VM.md](../KALLIS_VM.md) -- VM architecture overview
- [../VM_FUNCTION_REGISTRATION.md](../VM_FUNCTION_REGISTRATION.md) -- Function registration patterns
- [sub_52EA70_VMExecute.md](sub_52EA70_VMExecute.md) -- VM execution (uses registered methods)
- [sub_52D9C0_VMInterpreter.md](sub_52D9C0_VMInterpreter.md) -- VM interpreter
- [../../objects/sub_405380_HashString.md](../../objects/sub_405380_HashString.md) -- Hash algorithm
