# sub_491F30 --- sauSubstitutePlayer

**Address:** 0x491F30 (Spiderwick.exe+91F30) | **Calling convention:** __cdecl (VM native handler)

**Registered at:** 0x1CB091A (`.kallis` native registration table)

---

## Purpose

Native handler for the `sauSubstitutePlayer` VM script function. Substitutes one playable character for another in the player slot system. Pops two object references from the VM stack --- the new character and the old character --- validates both are `ClCharacterObj` instances, retrieves the state/facet object, and delegates to a `.kallis` thunk for the actual substitution logic.

This is the VM-callable entry point for character substitution, distinct from `SetPlayerType` (sub_463880) which handles type-based switching.

---

## Parameters

Receives arguments from the VM stack (not native C parameters):

| Stack Position | Name | Type | Description |
|----------------|------|------|-------------|
| Top | `v5` (newChar) | object ref | The replacement character (ClCharacterObj) |
| Top-1 | `v4` (oldChar) | object ref | The character being replaced (ClCharacterObj) |

**Returns:** void (result pushed to VM if needed)

---

## Decompiled Pseudocode

```c
char sauSubstitutePlayer(void)
{
    unsigned int newCharRef;
    int oldCharRef;

    // Pop two object references from VM stack
    sub_52D820(&newCharRef);     // Pop new character (top of stack)
    sub_52D820(&oldCharRef);     // Pop old character (next on stack)

    if (!newCharRef)
        return 0;

    // Walk class chain of new character looking for ClCharacterObj
    int classChain = *(DWORD*)(newCharRef + 12);
    if (!classChain)
        return 0;

    while ((char**)classChain != off_6E2830)  // ClCharacterObj descriptor
    {
        classChain = *(DWORD*)(classChain + 4);
        if (!classChain)
            return 0;
    }

    if (!oldCharRef)
        return 0;

    // Validate old character is also ClCharacterObj
    if (!sub_4053B0((DWORD*)(oldCharRef + 4), (int)off_6E2830))
        return 0;

    // Create facet from new character
    void *facet = std::locale::facet::facet(stackBuf, newCharRef);

    // Resolve state component via facet
    sub_53A2B0((int)facet);

    // Delegate to .kallis: sub_539AE0 + sub_4D3730
    return off_1C86858();  // .kallis thunk --> ROP chain
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x491F30` | Entry point |
| `0x1CB091A` | Registration address in `.kallis` native table |
| `off_6E2830` | `ClCharacterObj` class descriptor (validation target) |
| `off_1C86858` | `.kallis` thunk pointer --> `0x1CD5AA0` (ROP chain) |

---

## Called By

| Caller | Context |
|--------|---------|
| `.kallis` VM | Called from script via `sauSubstitutePlayer(oldChar, newChar)` |
| `sub_52D9C0` (VMInterpreter) | Opcode 0x02 (CALL_NATIVE) dispatch |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x52D820` | [PopObjectValidated](../../vm/subs/sub_52D820_PopObjectValidated.md) | Pop + validate object reference from VM stack |
| `0x4053B0` | [ClassChainCheck](sub_4053B0_ClassChainCheck.md) | Validate object is ClCharacterObj |
| `0x53A2B0` | GetStateFacet | Retrieve state object via facet pattern |
| `off_1C86858` | `.kallis` thunk | Actual substitution logic (ROP chain at 0x1CD5AA0) |

---

## Notes / Caveats

1. **NOT callable from native code.** The final delegation via `off_1C86858` targets a `.kallis` ROP chain at `0x1CD5AA0`. Like other `.kallis` thunks, this uses push/pushf/sub/popf/retn dispatch and requires the `.kallis` execution context. Calling this from a native hook (e.g., EndScene) will crash or produce undefined behavior.

2. **Pop order matters.** The VM stack is LIFO, so the first pop (`sub_52D820`) gets the topmost value which is the *last* argument pushed by the script. In sauSubstitutePlayer's case: `v5` = new character (popped first), `v4` = old character (popped second).

3. **Class validation uses `off_6E2830` (ClCharacterObj)**, not `off_6E2C58` (ClPlayerObj). This means the function accepts any character object, not just player-controlled ones. This allows substituting NPCs or special characters like ThimbleTack.

4. **Related to but distinct from SetPlayerType** ([sub_463880](sub_463880_SetPlayerType.md)). SetPlayerType switches by type index (1/2/3), while sauSubstitutePlayer swaps specific object instances. SetPlayerType is the normal gameplay path; sauSubstitutePlayer is used for scripted substitutions.
