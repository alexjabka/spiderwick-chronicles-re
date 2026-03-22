# sub_492030 -- sauInteractHandler

**Address:** 0x492030 (Spiderwick.exe+92030) | **Calling convention:** __cdecl (VM handler)

---

## Purpose

VM script handler for character interaction events. Checks whether the `INP_ACTIVATE` input (E key) was pressed for a given character object, consumes the input if so, and also registers `INP_IO` input on the character. Returns a boolean result to the VM indicating whether the activate input was detected.

---

## Parameters

Takes arguments from the VM stack:

| Pop Order | Type | Description |
|-----------|------|-------------|
| 1 | object ref | Character object to check input on |

**Returns (VM):** `bool` -- 1 if INP_ACTIVATE was pressed, 0 otherwise

---

## Decompiled Pseudocode

```c
BOOL sauInteractHandler()
{
    char result = 0;
    int charObj;
    sub_52D820(&charObj);  // PopObjectRef -- pop character from VM stack

    if (charObj)
    {
        int classChain = *(DWORD*)(charObj + 12);
        if (classChain)
        {
            // Walk class chain to find ClCharacterObj
            while ((char**)classChain != off_6E2830)  // "ClCharacterObj" descriptor
            {
                classChain = *(DWORD*)(classChain + 4);
                if (!classChain)
                    goto done;
            }

            // Lazy-initialize INP_ACTIVATE input binding
            if ((dword_D42CD8 & 1) == 0)
            {
                dword_D42CD8 |= 1;
                dword_D42CD4 = sub_408240(2, "INP_ACTIVATE");  // register input
            }

            // vtable[57]: check if input is active
            result = charObj->vtable[57](charObj, dword_D42CD4);

            if (result)
            {
                // vtable[56]: consume the input
                charObj->vtable[56](charObj, dword_D42CD4);
            }

            // Lazy-initialize INP_IO input binding
            if ((dword_D42CD8 & 2) == 0)
            {
                dword_D42CD8 |= 2;
                dword_D42CD0 = sub_408240(2, "INP_IO");
            }

            // vtable[55]: register INP_IO on the character
            charObj->vtable[55](charObj, dword_D42CD0);
        }
    }
done:
    return sub_52CC70(result);  // PushResult -- push bool to VM return stack
}
```

---

## Key Addresses

| Address | Description |
|---------|-------------|
| `0x492030` | Entry point |
| `dword_D42CD4` | Cached INP_ACTIVATE input binding handle |
| `dword_D42CD8` | Initialization bitmask: bit 0 = INP_ACTIVATE initialized, bit 1 = INP_IO initialized |
| `dword_D42CD0` | Cached INP_IO input binding handle |
| `off_6E2830` | "ClCharacterObj" class descriptor for class chain check |

### Vtable Calls

| Vtable Index | Offset | Purpose |
|--------------|--------|---------|
| [55] | +220 | Register input binding on character |
| [56] | +224 | Consume input (mark as handled) |
| [57] | +228 | Check if input is active/pressed |

---

## Input System Integration

The handler uses `sub_408240(2, "INP_ACTIVATE")` to register an input binding with type 2 (button). The string `"INP_ACTIVATE"` maps to the E key in the default control scheme.

The INP_IO binding is also registered but not checked -- it is set up for subsequent script logic to use.

Both bindings are lazily initialized (cached in globals with a bitmask guard at `dword_D42CD8`) to avoid redundant registration calls on every frame.

---

## Called By

This function is registered as a VM handler (likely for a script method like `sauCheckInteract` on `ClCharacterObj`). It is not called directly from native code.

---

## Notes

1. The class chain walk at `off_6E2830` ensures the popped object is a `ClCharacterObj` or subclass thereof. If the class chain does not contain `off_6E2830`, the function exits early.

2. The check-then-consume pattern (vtable[57] then vtable[56]) ensures the E key press is only used once per frame -- subsequent checks in the same frame will see the input as consumed.

3. The lazy initialization pattern (bitmask + global cache) is common across many input handlers in this engine.

---

## Related Documentation

- [../HOT_SWITCH_SYSTEM.md](../HOT_SWITCH_SYSTEM.md) -- Hot-switch system overview
- [../../vm/KALLIS_VM.md](../../vm/KALLIS_VM.md) -- VM stack operations
- [../../vm/subs/sub_52D820_PopVariant.md](../../vm/subs/sub_52D820_PopVariant.md) -- PopObjectRef
- [../../vm/subs/sub_52CC70_PushResult.md](../../vm/subs/sub_52CC70_PushResult.md) -- PushResult
