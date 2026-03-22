# Character Switching System

**Status:** Reversed (full switching pipeline, slot system, VM delegation identified)

---

## Overview

The Spiderwick Chronicles features three playable Grace siblings: **Jared** (type 1), **Mallory** (type 2), and **Simon** (type 3). Character switching is orchestrated by the `.kallis` VM through the `sauSetPlayerType` script function, which calls `SetPlayerType` (vtable[116]) followed by `CommitPlayerState` (vtable[113]) on the active `ClPlayerObj`.

The engine maintains a **player slot table** (`dword_E58D70`) with 56-byte entries per character type. Each slot tracks which character object is assigned to that type and whether that type is currently player-controlled (byte at slot+0x31). The `IsPlayerControlled` check (vtable[19]) resolves through this slot system, not through a flag on the character object itself.

At runtime, typically only **one** player character (ClPlayerObj) is spawned. The others do not exist in memory. Switching between ClPlayerObj characters therefore requires the VM to destroy the old character and spawn the new one --- the native `SetPlayerType` path was designed for non-ClPlayerObj targets (like ThimbleTack) and does not fully handle ClPlayerObj-to-ClPlayerObj transitions.

---

## Characters

| Type | Name | Widget Hash | Data Store Path |
|------|------|-------------|-----------------|
| 1 | **Jared Grace** | 0xEA836 | `/Game/Characters/Jared/...` |
| 2 | **Mallory Grace** | 0xB283C70 | `/Game/Characters/Mallory/...` |
| 3 | **Simon Grace** | 0x5E4B5EC | `/Game/Characters/Simon/...` |
| -- | **ThimbleTack** | 0x5923C9C6 | Separate class (ClThimbletackObj) |

Widget hashes computed by `HashString` (0x405380) on lowercase names ("jared", "mallory", "simon").

---

## Player Slot System

The engine maintains a global slot table for player characters:

```
dword_E58D70 → [count (DWORD), entries...]
Each entry: 56 bytes
```

- **sub_53A020(type)** --- looks up a slot by type (1-based; type 0 maps to 1)
- **Slot+0x00** --- ptr to slot descriptor
- **Slot+0x04** --- current character reference (passed to sub_539CF0 for swaps)
- **Slot+0x31** --- **IsPlayerControlled flag** (byte). This is THE control byte that vtable[19] reads via sub_539AC0
- **sub_539CB0(flag)** --- enable/disable slot (`.kallis` thunk to `off_1C8D810`)
- **sub_539CF0(slot, charRef)** --- swap the character assigned to a slot; calls deactivate callback (sub_539BC0) on old, activate callback (sub_539C20) on new

See: [PLAYER_SLOT_SYSTEM.md](PLAYER_SLOT_SYSTEM.md) for full details.

---

## The Switching Flow

### Script Layer: sauSetPlayerType (sub_4626B0)

The VM handler registered on `ClPlayerObj`. Called from `.kallis` scripts when the game wants to change characters:

1. Pops `type` (int) from VM stack via `sub_52C640`
2. Calls `vtable[116](this, type)` --- SetPlayerType
3. Calls `vtable[113](this)` --- CommitPlayerState (loads health, weapons, input mode)

See: [subs/sub_4626B0_sauSetPlayerType.md](subs/sub_4626B0_sauSetPlayerType.md)

### Native Layer: SetPlayerType (sub_463880, vtable[116])

The core switch function. Full flow:

1. **Guard:** `sub_457850(this)` --- if character is already switching, bail out
2. **Reset movement:** `sub_462170()` --- iterates all playable characters, resets movement params
3. **Slot lookup:** `sub_53A020(type)` --- get the 56-byte slot for the target type
4. **Resolve character:** `sub_539AE0(slot)` then `sub_450DA0()` --- resolve entity from slot to character object
5. **Type check:** `sub_4053B0(charObj+4, off_6E2C58)` --- is the target a `ClPlayerObj`?
6. **Critical branch at 0x4638DE:**
   - If **yes** (ClPlayerObj): `jz` is NOT taken --> jumps to `.kallis` ROP code at `off_1C867CC` (-->0x1CD7430). The VM handles the full spawn/despawn orchestration.
   - If **no** (e.g., ThimbleTack): falls through to the native path below.
7. **Native path** (non-ClPlayerObj targets only):
   - `sub_539CB0(1)` --- enable slot
   - `sub_539CF0(slot, GetCharRef(this))` --- swap character in slot
   - `GetCameraObject(0)` + `sub_439770()` --- detach/clear camera
   - `GetCameraObject(this)` + `sub_438E70()` --- attach camera to this character
   - `sub_55C7B0(this, 1)` --- activate character
   - Write `/Player/Character` to data store (index from widget hash via sub_44FEC0)
   - `vtable[14](this)` --- final activation

See: [subs/sub_463880_SetPlayerType.md](subs/sub_463880_SetPlayerType.md)

### Post-Switch: CommitPlayerState (sub_462B80, vtable[113])

Called after SetPlayerType completes:

1. Reads `/Player/%s/Health` from data store, sets character health
2. Checks INP_MINIGAME mode, switches input if needed
3. Loads weapons from data store:
   - `/Game/Characters/%s/MeleeWeapon`
   - `/Game/Characters/%s/RangedWeapon`
   - `/Game/Characters/%s/ButterflyNet`
4. Creates weapon instances via `vtable[68]` (offset 272)
5. Clears `this+0x5D0` byte (player type flag)

See: [subs/sub_462B80_CommitPlayerState.md](subs/sub_462B80_CommitPlayerState.md)

---

## Why Native Hot-Switch Fails

The **jz at 0x4638DE** is the crux of the problem. When switching between ClPlayerObj characters (Jared/Mallory/Simon), the engine ALWAYS delegates to the `.kallis` VM at `off_1C867CC` (resolves to `0x1CD7430`). This VM function uses ROP-style dispatchers to orchestrate:

- Despawning the old character (model, physics, AI, inventory)
- Spawning the new character from template
- Transferring camera, input bindings, and game state
- Resuming gameplay scripts

The native path (below the branch) was designed for switching to non-ClPlayerObj characters like ThimbleTack, where the target object already exists in memory. It only swaps the slot reference and moves the camera --- it does NOT handle character spawning/despawning.

### What We Tried

| Attempt | Method | Result |
|---------|--------|--------|
| 1 | Patch `jz` to `jmp` at 0x4638DE (force native path) | Camera partially works, input routing broken. Native path lacks ClPlayerObj spawn/despawn logic. |
| 2 | Slot byte flip (+0x31) + sub_539CF0 + camera | Slot correctly updates IsPlayerControlled, but camera/input binding incomplete. |
| 3 | Manual steps: enable slot, swap char ref, get camera, activate | Same incomplete result --- missing weapon/inventory/physics/AI transfer. |
| 4 | Data store write to `/Player/Character` (index 655) | **WORKS on reload.** Wrote type=2, loaded world, spawned as Mallory. Not instant though. |

### Root Cause

ClPlayerObj-to-ClPlayerObj switching fundamentally requires VM orchestration because:
- Only one player character exists in memory at a time
- The new character must be spawned with full model/animation/physics/collision/AI/inventory
- The old character must be properly destroyed with state saved
- Script bindings and game state must be transferred
- The `.kallis` VM owns entity allocation and script binding lifecycle

---

## Data Store Approach (Working)

Writing `/Player/Character` directly in the data store changes the character on next world load:

```
Data store index: 655 (hash 0xA488A96A for "/Player/Character")
Values: 1 = Jared, 2 = Mallory, 3 = Simon

Access:
  sub_41E830("/Player/Character") --> index
  sub_5392C0(byte_E57F68, index) --> pointer to value
  *ptr = newType;
  sub_539750(index);  // notify data store of change
```

This is the **graceful** approach: the engine's own level loading pipeline reads `/Player/Character` and spawns the correct character through the normal creation chain. No patches, no hacks --- the engine does all the work.

---

## Supporting Functions

### sauResolvePlayer (sub_493A80)

VM function handler that ensures character widgets are created for Mallory/Simon/Jared. Uses a creation bitmask (`dword_D42D14`) to track which widgets have been created. Resolves a character from the linked list by matching widget hash at `char+0x1C0` --> `+0x24`.

See: [subs/sub_493A80_sauResolvePlayer.md](subs/sub_493A80_sauResolvePlayer.md)

### ResetAllCharMovement (sub_462170)

Called at the start of SetPlayerType. Iterates all playable characters and resets their movement parameters to 1.0.

See: [subs/sub_462170_ResetAllCharMovement.md](subs/sub_462170_ResetAllCharMovement.md)

### IsPlayerControlled (sub_462E00)

vtable[19] for ClPlayerObj. Resolves through slot system to check the control byte at slot+0x31. Base class version (sub_454330) always returns 0.

See: [subs/sub_462E00_IsPlayerControlled.md](subs/sub_462E00_IsPlayerControlled.md)

---

## Key Addresses

| Address | Name | Description |
|---------|------|-------------|
| `0x463880` | SetPlayerType | vtable[116], the core switch function |
| `0x462B80` | CommitPlayerState | vtable[113], health/weapons/input setup |
| `0x4626B0` | sauSetPlayerType | VM handler, pops type + calls vtable[116]+[113] |
| `0x493A80` | sauResolvePlayer | VM function, ensures widgets created, resolves char |
| `0x462170` | ResetAllCharMovement | Iterates playable chars, resets movement |
| `0x462E00` | IsPlayerControlled | vtable[19], checks slot+0x31 control byte |
| `0x454330` | IsPlayerControlled_Base | Base class version, always returns 0 |
| `0x53A020` | GetPlayerSlot | Slot lookup from type (1-based) |
| `0x539CF0` | SwapSlotChar | Deactivate old + activate new character in slot |
| `0x539CB0` | SetSlotState | Enable/disable slot (.kallis thunk) |
| `0x539AC0` | GetControlByte | Returns slot+0x31 (IsPlayerControlled flag) |
| `0x4053B0` | IsClassType | Walks class chain checking for type match |
| `0xE58D70` | dword_E58D70 | Player slot table base |
| `0xD42CFC` | dword_D42CFC | Jared widget global |
| `0xD42D04` | dword_D42D04 | Simon widget global |
| `0xD42D0C` | dword_D42D0C | Mallory widget global |
| `0xD42D14` | dword_D42D14 | Widget creation bitmask |
| `0x4638DE` | -- | Critical jz: ClPlayerObj check --> VM delegation |
| `0x1C867CC` | off_1C867CC | .kallis ROP entry for ClPlayerObj switch |
| `0x1CD7430` | -- | .kallis switch function body |

---

## Related Documentation

- [HOT_SWITCH_SYSTEM.md](HOT_SWITCH_SYSTEM.md) --- Hot-switch mechanism (Session 4: full pipeline with input transfer, deferred hook)
- [PLAYER_SLOT_SYSTEM.md](PLAYER_SLOT_SYSTEM.md) --- Slot table layout, lookup, swap mechanics
- [CHARACTER_CREATION.md](CHARACTER_CREATION.md) --- Full character creation pipeline
- [ClCharacterObj_layout.md](ClCharacterObj_layout.md) --- Object field layout
- [subs/sub_463880_SetPlayerType.md](subs/sub_463880_SetPlayerType.md) --- SetPlayerType internals
- [subs/sub_462B80_CommitPlayerState.md](subs/sub_462B80_CommitPlayerState.md) --- CommitPlayerState internals
- [subs/sub_4626B0_sauSetPlayerType.md](subs/sub_4626B0_sauSetPlayerType.md) --- VM handler
- [subs/sub_493A80_sauResolvePlayer.md](subs/sub_493A80_sauResolvePlayer.md) --- Widget creation + character resolution
- [subs/sub_462170_ResetAllCharMovement.md](subs/sub_462170_ResetAllCharMovement.md) --- Movement reset
- [subs/sub_462E00_IsPlayerControlled.md](subs/sub_462E00_IsPlayerControlled.md) --- Control byte check
- [subs/sub_491F30_sauSubstitutePlayer.md](subs/sub_491F30_sauSubstitutePlayer.md) --- sauSubstitutePlayer VM handler
- [subs/sub_492030_sauInteractHandler.md](subs/sub_492030_sauInteractHandler.md) --- sauInteractHandler (INP_ACTIVATE)
- [../vm/KALLIS_VM.md](../vm/KALLIS_VM.md) --- VM stack system
