# Fade System

**Status:** Investigated (structure mapped, DO NOT HOOK)

---

## Overview

The Spiderwick engine has a fade overlay system that handles screen fades (e.g., fade to black during world transitions). The system uses a slot-based design with up to 10 concurrent fade requests managed by a fade manager object.

**WARNING:** This system is NOT safely hookable with MinHook. Even calling `MH_CreateHook` on `CreateFadeRequest` (without enabling it) corrupts the function prologue and causes crashes during world transitions.

---

## Architecture

```
Fade Manager (object, address resolved via ServiceLookup)
├── CreateFadeRequest(color, duration) — 0x55DB30  __thiscall
├── SubmitFadeRequest                  — 0x55DAA0  (slot allocator)
├── IsFadeSlotAvailable                — 0x59DA70  (slot availability check)
└── 10 fade slots at this+56, 32 bytes each

Service Lookup
├── dword_E9C7C4                       — fade component hash
└── ServiceLookup (0x537D10)           — resolves hash → fade manager instance
```

---

## Fade Slot Layout

The fade manager holds 10 slots starting at `this+56`, each 32 bytes:

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| +0 | ... | (unknown) | First 20 bytes TBD |
| +20 | float | duration | Total fade duration in seconds |
| +24 | float | elapsed | Current elapsed time |
| +28 | int | flag | Active flag |

### Slot Availability
A slot is available when all three conditions are met (checked by `IsFadeSlotAvailable`):
- `duration == 0.0`
- `elapsed == 0.0`
- `flag == 0`

---

## Loading Screen Fades

The game's loading screen (fade + progress bar during world transitions) is driven by the **Kallis VM**, not native code. This means:
- Zeroing `dword_E9C7C4` (the fade component hash) prevents native fade lookups
- But VM-internal fades still work because they bypass the service registry
- Loading screen fades **cannot be suppressed** from native hooks or data writes alone

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0xE9C7C4` | dword | g_FadeComponentHash | Hash for service registry lookup of fade manager |

---

## DO NOT HOOK Warning

MinHook's `MH_CreateHook` modifies the target function's prologue bytes (to place a jump) **even before `MH_EnableHook` is called**. For `sub_55DB30` (CreateFadeRequest), this modification corrupts the function and causes crashes when the game attempts to fade during world transitions.

This is a general hazard for any `__thiscall` function that is called during sensitive engine state transitions (like world loading). The prologue modification changes the instruction stream, and even if the hook redirects correctly, the displaced instructions may not survive relocation.

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x55DB30` | CreateFadeRequest | __thiscall | Create fade overlay (DO NOT HOOK) |
| `0x55DAA0` | SubmitFadeRequest | — | Allocate and populate a fade slot |
| `0x59DA70` | IsFadeSlotAvailable | — | Check if a fade slot is free |
| `0x537D10` | ServiceLookup | — | Resolve component hash to object instance |

---

## Related Documentation

- [subs/sub_55DB30_CreateFadeRequest.md](subs/sub_55DB30_CreateFadeRequest.md) — CreateFadeRequest (DO NOT HOOK)
- [subs/sub_55DAA0_SubmitFadeRequest.md](subs/sub_55DAA0_SubmitFadeRequest.md) — Slot allocator
- [subs/sub_59DA70_IsFadeSlotAvailable.md](subs/sub_59DA70_IsFadeSlotAvailable.md) — Slot check
- [../services/SERVICE_REGISTRY.md](../services/SERVICE_REGISTRY.md) — Service registry
- [../world/WORLD_LOADING.md](../world/WORLD_LOADING.md) — World loading (triggers fades)
