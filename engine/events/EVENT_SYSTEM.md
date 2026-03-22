# Script / Event System

**Status:** Partially reversed (dispatch mechanism and key events identified)

---

## Overview

The Spiderwick engine has a named event dispatch system that allows native code to trigger Kallis script handlers. Events are dispatched by name (string), and all registered handlers for that event name are invoked. The system is also used to register native functions that scripts can call (e.g., `sauSetClockSpeed`).

---

## Architecture

```
Event Dispatch
├── DispatchEvent(name)                — 0x52EBE0  VM thunk, fires event to all handlers
└── "MissionStart" event              — dispatched after every world load

Script Registration
├── RegisterScriptFunction(name, fn)  — 0x52EA10  registers native func for script access
└── RegisterTimeScriptFunctions       — 0x498180  registers all time-related script funcs

Post-World-Load Pipeline
└── PostWorldLoadInit (0x488660)
    ├── sub_4DC6A0(flag)              — pre-init step
    ├── DispatchEvent("MissionStart") — triggers level scripts
    └── sub_4DC330(2)                 — post-init step
```

---

## Event Dispatch

`DispatchEvent` (0x52EBE0) is a VM thunk that dispatches a named event string to all registered Kallis script handlers. When called with `"MissionStart"`, it triggers all level initialization scripts for the current world.

### Known Events

| Event Name | Address of String | Dispatched By | Purpose |
|------------|-------------------|---------------|---------|
| `"MissionStart"` | `0x62F4C0` | PostWorldLoadInit (0x488660) | Triggers level scripts after world load |

---

## Free Explore Technique

The `"MissionStart"` event is the entry point for all level scripts, including mission logic (e.g., the MnAttack final mission). By modifying the first byte of the event string at `0x62F4C0` from `'M'` (0x4D) to `'X'` (0x58), the string becomes `"XissionStart"` — no handlers match this name, so no mission scripts execute.

This is the **graceful Free Explore implementation**: the world loads normally with all geometry, NPCs, and objects, but no mission scripting runs. The player can explore freely without cutscenes, triggers, or mission state changes.

### Implementation
```c
// Patch: change "MissionStart" -> "XissionStart" at 0x62F4C0
*(BYTE*)0x62F4C0 = 'X';   // suppress mission scripts

// Restore: change back to "MissionStart"
*(BYTE*)0x62F4C0 = 'M';   // re-enable mission scripts
```

### Why This Works
- The string at `0x62F4C0` is the literal used by `PostWorldLoadInit` — it passes this address directly to `DispatchEvent`
- Changing the string means the event name no longer matches any registered handler
- All other world loading (geometry, sectors, NPCs, objects) proceeds normally
- This is a single-byte patch with instant toggle capability

### Limitation

With `"XissionStart"`: the player is not spawned and the camera is not initialized, because `MissionStart` also handles core initialization (character spawn, camera setup). A **better approach** is to let `MissionStart` fire normally, then freeze the world clock on the first frame after load — this gets the player and camera initialized while still preventing mission script progression.

---

## Script Function Registration

`RegisterScriptFunction` (0x52EA10) allows native C++ functions to be called from Kallis scripts. It takes a name string and a function pointer, registering them in the VM's function table.

### Known Registered Functions

**Native-registered** (called from .text code — handlers are accessible native functions):

| Script Name | Native Handler | Registered By |
|-------------|---------------|---------------|
| `sauSetClockSpeed` | 0x497DD0 | RegisterTimeScriptFunctions (0x498180) |
| `sauDelay` | TBD | RegisterTimeScriptFunctions (0x498180) |
| `sauSetTimer` | TBD | RegisterTimeScriptFunctions (0x498180) |
| `sauPauseGame` | 0x497FB0 | RegisterTimeScriptFunctions (0x498180) |
| `sauUnPauseGame` | TBD | RegisterTimeScriptFunctions (0x498180) |
| `sauSpawnObj` | 0x44C730 | (registration TBD) |

**VM-registered** (registered from .kallis bytecode — registration call is in VM, but handlers may be native code blocks within .kallis):

| Script Name | Handler | Notes |
|-------------|---------|-------|
| `sauCreateCharacter` | .kallis | Character creation from template |
| `sauDestroyCharacter` | .kallis | Remove character |
| `sauFreeCharacter` | .kallis | Free character memory |
| `sauMakeCurrentForCharacter` | .kallis | Switch active character (full) |
| `sauSetPlayerType` | .kallis | Set player type |

Both native-registered and VM-registered functions use the same `RegisterScriptFunction` mechanism — the difference is only where the registration call originates.

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0x62F4C0` | char[] | "MissionStart" | Event name string (modifiable for Free Explore) |

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x52EBE0` | DispatchEvent | __cdecl (VM thunk) | Dispatch named event to script handlers |
| `0x52EA10` | RegisterScriptFunction | __cdecl | Register native function for script access |
| `0x488660` | PostWorldLoadInit | — | Post-world-load initialization sequence |
| `0x498180` | RegisterTimeScriptFunctions | — | Registers time-related script functions |

---

## Related Documentation

- [subs/sub_52EBE0_DispatchEvent.md](subs/sub_52EBE0_DispatchEvent.md) — DispatchEvent
- [subs/sub_52EA10_RegisterScriptFunction.md](subs/sub_52EA10_RegisterScriptFunction.md) — RegisterScriptFunction
- [subs/sub_488660_PostWorldLoadInit.md](subs/sub_488660_PostWorldLoadInit.md) — Post-world-load init
- [../time/CLOCK_SYSTEM.md](../time/CLOCK_SYSTEM.md) — Clock system (uses script registration)
- [../world/WORLD_LOADING.md](../world/WORLD_LOADING.md) — World loading pipeline
