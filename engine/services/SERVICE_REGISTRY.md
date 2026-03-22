# Service Registry

**Status:** Partially reversed (lookup mechanism identified)

---

## Overview

The Spiderwick engine uses a service registry pattern to resolve engine component instances by hash. A component hash (stored as a DWORD) is used to look up the corresponding service object, which then provides access to the component via a vtable call.

---

## Architecture

```
ServiceLookup(hashStruct)  — 0x537D10
├── Searches dword_E57BB8 array for matching hash
├── On match: calls vtable[32] (offset 128) to get object instance
└── Returns: pointer to the service object

Known Component Hashes
├── dword_E9C7C4 — Fade component hash (resolves to FadeManager)
└── (others TBD)
```

---

## Lookup Mechanism

`ServiceLookup` (0x537D10) searches a global array at `dword_E57BB8` for an entry matching the provided component hash. When found, it calls offset 128 (vtable slot 32) of the matched entry to retrieve the actual object instance.

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0xE57BB8` | dword[] | g_ServiceRegistry | Array of registered service entries |
| `0xE9C7C4` | dword | g_FadeComponentHash | Hash for fade manager lookup |

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x537D10` | ServiceLookup | __cdecl | Look up service by component hash |

---

## Related Documentation

- [subs/sub_537D10_ServiceLookup.md](subs/sub_537D10_ServiceLookup.md) — ServiceLookup function
- [../fade/FADE_SYSTEM.md](../fade/FADE_SYSTEM.md) — Fade system (uses service registry)
