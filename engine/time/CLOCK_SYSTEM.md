# Clock / Time System

**Status:** Reversed (core API mapped, script interface identified)

---

## Overview

The Spiderwick engine uses a clock system to control the global game speed. Clock entries are managed through a central clock object (`dword_D57810`), and each active speed override is tracked by an integer entry ID. The system is exposed to Kallis scripts via `sauSetClockSpeed` and `sauPauseGame`.

Setting the clock speed to `0.0` freezes time (pause), while `1.0` runs at normal speed. Intermediate values produce slow-motion effects.

---

## Architecture

```
Script Layer (Kallis VM)
├── sauSetClockSpeed(speed)     — 0x497DD0
├── sauPauseGame()              — 0x497FB0
├── sauUnPauseGame()            — (registered, address TBD)
├── sauDelay()                  — (registered, address TBD)
└── sauSetTimer()               — (registered, address TBD)

Native Clock API
├── SetClockSpeed(speed)        — 0x4DF360  __cdecl, returns entry ID
├── RemoveClockEntry(entryId)   — 0x4DF070  __cdecl, removes an entry
└── RegisterTimeScriptFunctions — 0x498180  registers all time script funcs

Clock State
├── dword_D57810                — clock system "this" object (ecx)
└── dword_D42D6C                — current clock entry ID
```

---

## Usage Pattern

The canonical pattern used by the script system is:

```c
// Remove old clock entry first (cleanup)
RemoveClockEntry(dword_D42D6C);

// Set new speed and store the returned entry ID
dword_D42D6C = SetClockSpeed(speed);
```

This ensures only one speed override is active at a time. Failing to call `RemoveClockEntry` before `SetClockSpeed` would leak clock entries.

---

## Speed Values

| Speed | Effect |
|-------|--------|
| `0.0` | Freeze time (full pause) |
| `0.5` | Half speed (slow motion) |
| `1.0` | Normal speed |

The speed parameter is clamped to `[0.0, 1.0]` internally by `SetClockSpeed`.

---

## Key Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0xD57810` | dword | g_ClockSystemObj | Clock system "this" pointer (set as ecx internally) |
| `0xD42D6C` | dword | g_CurrentClockEntryId | Current active clock entry ID |

---

## Script Registration

All time-related script functions are registered by `RegisterTimeScriptFunctions` (0x498180) via `RegisterScriptFunction` (0x52EA10):

| Script Name | Handler | Address |
|-------------|---------|---------|
| `sauSetClockSpeed` | sauSetClockSpeed handler | `0x497DD0` |
| `sauPauseGame` | sauPauseGame handler | `0x497FB0` |
| `sauUnPauseGame` | (handler TBD) | — |
| `sauDelay` | (handler TBD) | — |
| `sauSetTimer` | (handler TBD) | — |

---

## Function Reference

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| `0x4DF360` | SetClockSpeed | __cdecl | Set game speed (0.0-1.0), returns entry ID |
| `0x4DF070` | RemoveClockEntry | __cdecl | Remove a clock speed entry by ID |
| `0x497DD0` | sauSetClockSpeed handler | __cdecl (VM) | Script handler for sauSetClockSpeed |
| `0x497FB0` | sauPauseGame handler | __cdecl (VM) | Script handler for sauPauseGame |
| `0x498180` | RegisterTimeScriptFunctions | — | Registers all time script functions |

---

## Related Documentation

- [subs/sub_4DF360_SetClockSpeed.md](subs/sub_4DF360_SetClockSpeed.md) — SetClockSpeed native API
- [subs/sub_4DF070_RemoveClockEntry.md](subs/sub_4DF070_RemoveClockEntry.md) — RemoveClockEntry
- [subs/sub_497DD0_sauSetClockSpeed.md](subs/sub_497DD0_sauSetClockSpeed.md) — Script handler
- [subs/sub_497FB0_sauPauseGame.md](subs/sub_497FB0_sauPauseGame.md) — Pause game handler
- [subs/sub_498180_RegisterTimeScriptFunctions.md](subs/sub_498180_RegisterTimeScriptFunctions.md) — Registration function
- [../events/EVENT_SYSTEM.md](../events/EVENT_SYSTEM.md) — Event dispatch system
