# sub_450DA0 --- GetEntityFromCharacter

**Address:** 0x450DA0 (Spiderwick.exe+50DA0) | **Size:** 30 bytes | **Calling convention:** __cdecl

---

## Purpose

Resolves an entity/character reference to an actual game object pointer. First validates the reference by calling `vtable[1]` on it (a boolean validity check), then if valid, extracts the game object via `sub_4D3740` (which returns `*(ref + 4)`).

This is the standard pattern for going from a slot's character reference (obtained via [GetCharacterFromSlot](sub_539AE0_GetCharacterFromSlot.md)) to a usable `ClCharacterObj*`.

---

## Prototype

```c
int __cdecl GetEntityFromCharacter(int entityRef)
```

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `entityRef` | int | Entity/character reference (from slot+0x04 via GetCharacterFromSlot) |

**Returns:** `int` (ClCharacterObj*) --- resolved game object pointer, or 0 if the reference is invalid

---

## Decompiled Pseudocode

```c
int __cdecl GetEntityFromCharacter(int entityRef)
{
    // Validate reference via vtable[1]
    if ((*(unsigned __int8 (__thiscall **)(int))(*(DWORD *)entityRef + 4))(entityRef))
    {
        // Valid: extract game object via sub_4D3740
        return sub_4D3740(entityRef);   // returns *(entityRef + 4)
    }
    else
    {
        return 0;   // invalid reference
    }
}
```

Where `sub_4D3740` is:
```c
int __thiscall sub_4D3740(_DWORD *this)
{
    return *(this + 1);   // return this[1], i.e., *(ref + 4)
}
```

---

## Key Operations

1. **Validity check via vtable[1]:** Calls the virtual function at `*(*(DWORD*)entityRef) + 4)` (vtable offset +4, i.e., vtable[1]) on the entity reference. This returns a boolean --- 0 = invalid, non-zero = valid.
2. **Extract game object:** If valid, calls `sub_4D3740(entityRef)` which returns `*(entityRef + 4)` --- the actual game object pointer stored at offset +4 of the reference.
3. **Null on failure:** Returns 0 if the validity check fails (dead entity, unloaded, etc.).

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x450DA0` | Entry point |
| `0x450DAC` | vtable[1] validity check call |
| `0x450DB5` | `call sub_4D3740` --- extract game object |
| `0x450DBA` | Return 0 (invalid path) |
| `0x4D3740` | Sub-accessor: returns `*(this + 4)` |

### Entity Reference Layout

| Offset | Type | Description |
|--------|------|-------------|
| +0x00 | DWORD* | vtable pointer |
| +0x04 | DWORD | Game object pointer (ClCharacterObj*) |

| vtable[1] | Purpose |
|-----------|---------|
| offset +4 | Validity check --- returns bool |

---

## Called By (selected)

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | At 0x4638B2: resolve target character from slot entity ref |
| `sub_440F60` | At 0x440FA7 |
| `sub_455620` | At 0x4557A2 |
| `SectorDistanceCheck` | At 0x487DCC |
| `sub_491E10` | At 0x491E3B |
| `sub_49A0B0` | Coop events at 0x49A108, 0x49A139, 0x49A16A |
| `sub_4B6CA0` | At 0x4B6CE5 |
| `sub_4B9110` | At 0x4B9129 |
| `sub_4BECD0` | At 0x4BED6A, 0x4BED98 |
| `sub_4C6720` / `sub_4C69A0` | At 0x4C6754, 0x4C6790, 0x4C6A87 |
| `.kallis` segment | At 0x1C9F93B, 0x1CA55C9 |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x4D3740` | (accessor) | Returns `*(entityRef + 4)` --- the actual game object |

---

## Notes / Caveats

1. **Always paired with GetCharacterFromSlot.** The standard slot-to-character resolution is:
   ```c
   int entityRef = GetCharacterFromSlot(slot);      // sub_539AE0 - returns slot+4
   ClCharacterObj* obj = GetEntityFromCharacter(entityRef);  // sub_450DA0 - validates + extracts
   ```

2. **The vtable[1] validity check** acts as the engine's "is this entity still alive" test. If the entity has been destroyed, unloaded, or is in an invalid state, vtable[1] returns 0 and this function returns NULL.

3. **18+ cross-references** including calls from the `.kallis` segment itself, confirming this is part of the engine's core entity resolution infrastructure.

4. **The inner accessor sub_4D3740** is a separate 4-byte function (`mov eax, [ecx+4]; retn`) that could be inlined but is kept as a separate call, likely for polymorphism or debugging.

5. **No null check on entityRef itself** --- callers must ensure the entity reference is not NULL before calling. In SetPlayerType, the slot lookup + GetCharacterFromSlot is guarded against NULL before this is called.

6. **Related functions:**
   - [GetCharacterFromSlot](sub_539AE0_GetCharacterFromSlot.md) (sub_539AE0) --- provides the entityRef argument
   - [PlayerSlotLookup](sub_53A020_PlayerSlotLookup.md) (sub_53A020) --- provides the slot
   - [SetPlayerType](sub_463880_SetPlayerType.md) (sub_463880) --- primary consumer chain
   - [ClassChainCheck](sub_4053B0_ClassChainCheck.md) (sub_4053B0) --- called on the result to check type
