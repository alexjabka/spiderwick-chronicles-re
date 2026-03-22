# sub_52D820 --- PopObjectValidated

**Address:** 0x52D820 (Spiderwick.exe+12D820) | **Calling convention:** __cdecl

---

## Purpose

Pops an object reference from the VM stack with full validation. This is a more thorough version of the simple [PopObjectRef](sub_52C860_PopObjectRef.md) --- it performs reference resolution, flag checking, native object extraction, and class chain validation before returning the result.

Used by native handlers that need guaranteed-valid object pointers from the VM stack (e.g., [sauSubstitutePlayer](../../objects/subs/sub_491F30_sauSubstitutePlayer.md)).

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `out` | int* | Output pointer: receives the validated native object pointer |

**Returns:** void (result written to `*out`)

---

## Decompiled Pseudocode

```c
void __cdecl PopObjectValidated(int *out)
{
    // Pop raw value from VM stack
    int rawRef = PopRaw();

    // Convert raw reference to VM object pointer
    void *vmObj = *(void**)(dword_E56160 + rawRef * sizeof(void*));

    // Check flags at vmObj+26 (offset 0x1A)
    short flags = *(short*)(vmObj + 26);

    if (flags & 0x4000)     // bit 14: object is indirect?
    {
        // Resolve indirection
        // ...
    }

    if (flags & 0x0001)     // bit 0: object has native backing?
    {
        // Extract native object pointer from vmObj+16
        int nativeObj = *(int*)(vmObj + 16);

        // Validate class chain against off_727EB0 (base object class)
        if (ClassChainCheck(nativeObj, off_727EB0))   // sub_4053B0
        {
            *out = nativeObj;
            return;
        }
    }

    // Fallback: call sub_55C7A0(1) and use its result
    *out = sub_55C7A0(1);
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x52D820` | Entry point |
| `dword_E56160` | VM object table base (raw ref --> vmObj lookup) |
| `off_727EB0` | Base object class descriptor (validation target) |

### VM Object Offsets

| Offset | Size | Description |
|--------|------|-------------|
| +0x10 (16) | 4 | Native object pointer |
| +0x1A (26) | 2 | Flags: bit 14 = indirect, bit 0 = has native backing |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_491F30` ([sauSubstitutePlayer](../../objects/subs/sub_491F30_sauSubstitutePlayer.md)) | Pops two character references |
| Various native script handlers | Any handler needing validated object pointers from VM stack |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| (inline) | PopRaw | Pops raw value from VM argument stack |
| `0x4053B0` | [ClassChainCheck](../../objects/subs/sub_4053B0_ClassChainCheck.md) | Validates object class chain against off_727EB0 |
| `0x55C7A0` | Fallback resolver | Called with arg=1 when validation fails; returns a default/fallback object |

---

## Notes / Caveats

1. **This is distinct from the simpler sub_52D820 "PopVariant"** previously documented. With full decompilation, it is now clear this function specifically pops and validates OBJECT references, not generic variants. The existing `sub_52D820_PopVariant.md` should be considered superseded by this document.

2. **The `dword_E56160` table** maps raw VM stack values (integer indices) to VM object descriptors. This is the core of the Kallis VM's object reference system --- scripts manipulate integer handles, and this function resolves them to native pointers.

3. **Flag checking at +26 (0x1A):**
   - **Bit 14 (0x4000):** Indicates an indirect/proxy object that needs additional resolution
   - **Bit 0 (0x0001):** Indicates the VM object has a native C++ backing object at +16

4. **The fallback `sub_55C7A0(1)`** is called when class chain validation fails. This likely returns a sentinel/null object or logs an error. Its exact behavior needs further investigation.

5. **`off_727EB0`** is the BASE object class descriptor --- a very permissive check. This validates that the popped reference is ANY valid game object, not a specific type. Type-specific checks (e.g., ClCharacterObj) are done by the caller after PopObjectValidated returns.

6. **Related functions:**
   - [PopObjectRef](sub_52C860_PopObjectRef.md) (sub_52C860) --- simpler pop without full validation
   - [ClassChainCheck](../../objects/subs/sub_4053B0_ClassChainCheck.md) (sub_4053B0) --- RTTI validation
   - [PopInt](sub_52C610_PopInt.md) / [PopFloat](sub_52C6A0_PopFloat.md) / [PopString](sub_52D7D0_PopString.md) --- other typed pop operations
# sub_52D820 -- VMStackPop (PopObjectValidated)

**Address:** 0x52D820 (Spiderwick.exe+12D820) | **Calling convention:** __cdecl

---

## Purpose

Pops an object reference from the VM argument stack and dereferences it through the VM object table at `dword_E56160`. Performs validation including status flag checks and class chain walking. If the object has a specific class marker (`off_727EB0`), resolves it through `sub_55C7A0(1)` to get the active player character.

This is the "validated pop" -- unlike the simpler `PopObjRef` (sub_52C860), this function performs full validation and special-case resolution.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `result` | int* | Output: pointer to store the resolved object pointer |

**Returns:** `int` -- the resolved object pointer (also written to `*result`)

---

## Decompiled Pseudocode

```c
int __cdecl VMStackPop(int *result)
{
    // Pop raw value from arg stack
    int rawRef = *(DWORD*)(dword_E56168 - 4 * dword_E56200);
    dword_E56200--;  // decrement stack index

    // Null check + resolve through object table
    if (rawRef == 0)
    {
        *result = 0;
        return 0;
    }

    int objEntry = dword_E56160 + 4 * (rawRef / 4);  // index into object table
    if (objEntry == 0)
    {
        *result = 0;
        return 0;
    }

    // Check status flags at objEntry+26 (word)
    short flags = *(WORD*)(objEntry + 26);
    if (flags & 0x4000)
    {
        // Flag 0x4000 set: treat as valid, proceed
    }
    else if ((flags & 2) == 0)
    {
        // Flag 2 not set: treat as valid, proceed
    }
    else
    {
        // Both conditions fail: object is invalid/deleted
        *result = 0;
        return 0;
    }

    // Dereference to actual object
    int obj = *(DWORD*)(objEntry + 16);
    *result = obj;

    if (obj == 0)
        return obj;

    // Walk class chain looking for off_727EB0
    int classChain = *(DWORD*)(obj + 12);
    if (classChain == 0)
        return obj;

    while ((char**)classChain != off_727EB0)
    {
        classChain = *(DWORD*)(classChain + 4);
        if (classChain == 0)
            return obj;  // off_727EB0 not found: return obj as-is
    }

    // off_727EB0 found in chain: resolve to active player
    int playerObj = sub_55C7A0(1);
    *result = playerObj;
    return playerObj;
}
```

---

## VM Object Table

The object table at `dword_E56160` is the VM's entity reference system:

| Offset in Entry | Type | Description |
|-----------------|------|-------------|
| +0 | ... | (unknown) |
| +16 | DWORD | Actual object pointer |
| +26 | WORD | Status flags |

### Status Flags (at entry+26)

| Bit | Hex | Meaning |
|-----|-----|---------|
| 1 | 0x0002 | Object is deleted/invalid (blocks access unless 0x4000 set) |
| 14 | 0x4000 | Override: allow access even if bit 1 is set |

### Object Resolution Formula

```
raw_ref = pop from arg stack
table_index = raw_ref / 4
entry_ptr = dword_E56160 + 4 * table_index
object_ptr = *(entry_ptr + 16)
```

---

## Special Case: off_727EB0 Class

When the resolved object's class chain contains `off_727EB0`, the function does NOT return the object itself. Instead, it calls `sub_55C7A0(1)` which resolves to the currently active player character. This is a "player proxy" pattern -- certain VM objects represent "the current player" rather than a specific character.

---

## Called By

This function has an extremely large number of callers (180+). It is the primary mechanism for popping object references in native VM handler functions. Key callers include:

| Caller | Context |
|--------|---------|
| `sub_491F30` (sauSubstitutePlayer) | Pops 2 object refs |
| `sub_492030` (sauInteractHandler) | Pops character object |
| `sub_4626F0` | VM handler |
| Many `sub_491xxx`-`sub_49Bxxx` | VM handler functions |

---

## Key Global Variables

| Address | Type | Purpose |
|---------|------|---------|
| `dword_E56168` | DWORD | VM arg stack base address |
| `dword_E56200` | DWORD | VM arg stack index (current position) |
| `dword_E56160` | DWORD* | VM object reference table base |
| `off_727EB0` | char** | "Player proxy" class marker |

---

## Notes

1. The arg stack grows downward: `value = *(base - 4 * index); index--;`

2. The division by 4 in the object table lookup (`rawRef / 4`) suggests references are encoded as byte offsets, divided by 4 to get the table index. Alternatively, the table entries are 4 bytes and the reference IS the byte offset.

3. The `off_727EB0` special case is critical for the player system -- it allows scripts to reference "the player" abstractly without knowing which specific character (Jared/Mallory/Simon) is currently active.

---

## Related Documentation

- [../KALLIS_VM.md](../KALLIS_VM.md) -- VM stack system overview
- [../VM_STACK_OPERATIONS.md](../VM_STACK_OPERATIONS.md) -- Stack operations deep dive
- [sub_52C860_PopObjRef.md](sub_52C860_PopObjRef.md) -- Simpler object pop (without full validation)
- [sub_52D5A0_VMStackPush.md](sub_52D5A0_VMStackPush.md) -- Push to return stack
- [sub_52EA70_VMExecute.md](sub_52EA70_VMExecute.md) -- VM execution
