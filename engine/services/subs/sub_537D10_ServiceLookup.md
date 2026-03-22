# sub_537D10 — ServiceLookup

**Address:** `Spiderwick.exe+137D10` (absolute: `00537D10`)
**Convention:** __cdecl
**Returns:** void* (service object pointer)

## Signature
```c
void* __cdecl ServiceLookup(void *hashStruct)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| hashStruct | void* | Pointer to structure containing the component hash to look up |

## Description
Searches the global service registry array at `dword_E57BB8` for an entry matching the provided component hash. When a match is found, calls vtable offset 128 (slot 32) of the matched entry to retrieve the actual component object instance.

## Key Details
- Registry array at `dword_E57BB8`
- Match condition: compares hash from input struct against hash in registry entries
- On match: dereferences vtable, calls function at offset 128 (vtable[32])
- Used by the fade system to resolve the FadeManager from `dword_E9C7C4`
- Zeroing the component hash (`dword_E9C7C4 = 0`) prevents native lookups but does not affect VM-internal references

## Called By
- Fade system code (resolves FadeManager)
- Other service consumers (TBD)

## Related
- [../SERVICE_REGISTRY.md](../SERVICE_REGISTRY.md) — service registry overview
- [../../fade/FADE_SYSTEM.md](../../fade/FADE_SYSTEM.md) — fade system
