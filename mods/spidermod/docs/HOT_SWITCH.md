# SpiderMod -- Hot-Switch System
**Module:** `dllmain.cpp` (execution) + `menu.cpp` (UI / character table)
**Status:** Implemented and working -- solved via native path trick

---

## Overview

Hot-switch allows the player to take control of any playable character in the current level by clicking a button in the SpiderMod menu. The system uses deferred execution: the UI sets a pending switch request, and the CameraSectorUpdate hook executes it on the next game tick from the correct thread context.

---

## Architecture

### Why Deferred Execution?

Character switching involves calling engine functions that manipulate game state (VM objects, player slots, input ownership). These must run from the game's update thread, not from the D3D EndScene render thread where ImGui runs.

**Solution:** The UI writes to volatile globals, and the CameraSectorUpdate hook (0x488DD0) checks and executes them each tick.

### Globals

Declared in `dllmain.cpp`, externed in `menu.cpp`:

| Variable | Type | Description |
|----------|------|-------------|
| `g_pendingSwitchType` | volatile int | 0=none, 1-3=character type (legacy path) |
| `g_pendingSwitchTarget` | volatile uintptr_t | Direct character address (table button path) |

When `g_pendingSwitchType >= 1`, the hook processes the switch and resets both to 0.

---

## Switch Sequence

Executed in `HookedCameraSectorUpdate` (dllmain.cpp):

### Step 1: OnDispossessed VMCall

Call `OnDispossessed` on the current player's VM object to properly deactivate it:

```cpp
uintptr_t oldPlayer = GetPlayerCharacter();  // 0x44F890
uintptr_t oldVm = mem::Read<uintptr_t>(oldPlayer + 0xA8);  // VM object
uint32_t scriptsLoaded = mem::Read<uint32_t>(0xE561F8);

if (oldVm && scriptsLoaded) {
    VMCall(oldVm, "OnDispossessed");  // 0x52EB40
}
```

This triggers the character's script cleanup (animation stop, state reset, etc.).

### Step 2: Patch jz -> jmp at 0x4638DE

```cpp
constexpr uintptr_t PATCH_ADDR = 0x4638DE;
VirtualProtect((LPVOID)PATCH_ADDR, 1, PAGE_EXECUTE_READWRITE, &oldProt);
uint8_t origByte = *(uint8_t*)PATCH_ADDR;
*(uint8_t*)PATCH_ADDR = 0xEB;  // jz(0x74) -> jmp(0xEB)
```

**Why:** At 0x4638DE in `ClPlayerObj::SetPlayerType` (vtable[116]), there is a conditional branch:
- `jz` (0x74) -- if a `.kallis` coop script exists, take the VM path (cooperative character switching)
- Native path -- direct C++ character activation

The `.kallis` coop path calls into VM scripts that may not be set up correctly for forced switching. By patching `jz` to `jmp`, we force the native code path which performs direct player slot assignment without VM involvement.

### Step 3: Call vtable[116] with type=1

```cpp
uintptr_t vtable = mem::Read<uintptr_t>(targetChar);
typedef void (__thiscall *SetPlayerTypeFn)(void*, int);
auto setType = (SetPlayerTypeFn)(mem::Read<uintptr_t>(vtable + 116 * 4));
setType((void*)targetChar, 1);  // always type=1
```

**vtable[116]** is `ClPlayerObj::SetPlayerType` at 0x463880. It:
1. Calls `sub_53A020(type)` to resolve the player slot for the given type
2. Transfers P1 input ownership to `this` (the target character)
3. Updates the active player pointer

### Why type=1 Always?

`sub_53A020(type)` looks up the player slot by type index. On most levels (MansionD, GroundsD, etc.), only **slot 1** exists in the player slot array. Passing type=1 ensures the slot lookup succeeds and returns the single available slot, regardless of which character (Jared/Simon/Mallory) is being activated.

The function activates `this` (the target character object) regardless of the type parameter -- the type only affects which slot's input mapping is used.

### Step 4: Restore Original Byte

```cpp
*(uint8_t*)PATCH_ADDR = origByte;
VirtualProtect((LPVOID)PATCH_ADDR, 1, oldProt, &oldProt);
```

The patch is applied for a single call and immediately restored. This minimizes the window where game code could take the wrong branch.

---

## Character Identification

The character table in the ImGui menu identifies characters using the same three-tier system as the 3D overlay:

### Tier 1: Widget Hash Lookup

```
character_obj+0x1C0 -> widget pointer
widget+0x24 -> name hash (DWORD)
```

Compare against a pre-computed table of 30+ known entity names, hashed at startup via `HashString` (0x405380):
- Jared, Mallory, Simon, ThimbleTack
- Goblin, BullGoblin, Redcap, Hogsqueal
- Cockroach, LeatherWing, Griffin, MoleTroll
- RiverTroll, FireSalamander, Cockatrice, Mulgarath
- And more (see `EnsureNameTable()` in menu.cpp)

### Tier 2: RTTI Fallback

If no widget exists, follow the MSVC RTTI chain from the vtable:
```
character_obj -> vtable
vtable[-1] -> Complete Object Locator
COL+12 -> type_descriptor pointer
type_desc+8 -> decorated name string (".?AVClPlayerObj@@")
```

Strip `.?AV` prefix and `@@` suffix to get the class name.

### Tier 3: Hash Stub

If both fail, display `NPC#XXXX` (truncated hash).

---

## Character Table UI

The "Characters" section shows a table with columns:

| Column | Width | Content |
|--------|-------|---------|
| # | 20px | Row index |
| Name | 90px | Identified name (current player has `*` suffix) |
| Position | stretch | World coordinates (X, Y, Z) |
| Player | 40px | "Yes" if `vtable[19]` (IsPlayerCharacter) returns true |
| (buttons) | 50px | [TP] and [Play] action buttons |
| Addr | 70px | Memory address (0xXXXXXXXX) |

### Row Colors

| Condition | Color |
|-----------|-------|
| Current player | Green (0.3, 1.0, 0.3) |
| Player character (not current) | Cyan (0.3, 0.8, 1.0) |
| Playable character | Yellow (0.9, 0.9, 0.5) |
| NPC | Gray (0.8, 0.8, 0.8) |

### Action Buttons

#### [TP] -- Teleport

- If freecam is active: teleports freecam position to character's location (+2.0 Z offset)
- If freecam is off: teleports the current player to the character's position
- Hidden for the current player (teleporting to yourself is meaningless)

#### [Play] -- Switch Character

- Sets `g_pendingSwitchTarget = charAddress` and `g_pendingSwitchType = 1`
- The CameraSectorUpdate hook processes the switch on the next tick
- Hidden for the current player

### [Play] Safety Check: vtable[116] Validation

The [Play] button is only shown if:
```cpp
mem::Read<uintptr_t>(vtable + 116 * 4) == 0x463880  // ClPlayerObj::SetPlayerType
```

This ensures the target character's vtable[116] points to the `ClPlayerObj` implementation. Non-player objects (goblins, sprites, etc.) have different functions at vtable[116] that would crash if called with our parameters.

---

## Target Resolution

The switch has two paths for finding the target character:

### Path 1: Direct Address (Table Button)

When the user clicks [Play] in the table, `g_pendingSwitchTarget` is set to the character's memory address. This is the primary path -- no lookup needed.

### Path 2: Widget Hash Lookup (Legacy)

If `g_pendingSwitchTarget` is 0 (legacy button path), the system walks the character linked list and matches by widget hash:

```cpp
const char* widgetName = (switchType == 1) ? "Jared" :
                         (switchType == 2) ? "Mallory" :
                                             "Simon";
int targetHash = HashString(widgetName);

uintptr_t cur = g_CharacterListHead;
while (cur) {
    uintptr_t widget = *(uintptr_t*)(cur + 0x1C0);
    int hash = *(int*)(widget + 0x24);
    if (hash == targetHash) { targetChar = cur; break; }
    cur = *(uintptr_t*)(cur + 0x5A0);
}
```

---

## Key Addresses

| Address | Name | Role |
|---------|------|------|
| 0x463880 | ClPlayerObj::SetPlayerType | vtable[116] -- the character switch function |
| 0x4638DE | jz/jmp patch site | Coop branch in SetPlayerType |
| 0x53A020 | sub_53A020 | Player slot resolver (type -> slot pointer) |
| 0x52EB40 | VMCall | Calls named function on VM object |
| 0x44F890 | GetPlayerCharacter | Returns current player pointer |
| 0x488DD0 | CameraSectorUpdate | Hook site for deferred execution |
| 0x007307D8 | g_CharacterListHead | Head of character linked list |
| 0x00E561F8 | Scripts loaded flag | Must be nonzero for VMCall |

---

## Design Notes

### Why Not Use the VM's SwitchPlayerCharacter?

The engine has `SwitchPlayerCharacter` (0x493A80), a VM thunk that pops a type from the VM arg stack. This was the first approach tried but it:
1. Requires the VM arg stack to be set up correctly
2. Goes through the `.kallis` coop script path which may hang or fail
3. Cannot switch to arbitrary characters (only type 1/2/3)

The vtable[116] approach with the `jz->jmp` patch bypasses all VM involvement and works with any `ClPlayerObj`.

### Graceful Behavior

- The `jz->jmp` patch is applied for exactly one function call and immediately restored
- `OnDispossessed` VMCall properly deactivates the old player via script
- Input ownership transfers cleanly through the engine's slot system
- No persistent patches, NOPs, or hacks remain after the switch

---

## Related Documentation

- [3D_DEBUG_OVERLAY.md](3D_DEBUG_OVERLAY.md) -- Character identification (shared code)
- [CAMERA_OVERRIDE.md](CAMERA_OVERRIDE.md) -- Camera override (same hook)
- [../../../engine/camera/subs/sub_488DD0_CameraSectorUpdate.md](../../../engine/camera/subs/sub_488DD0_CameraSectorUpdate.md) -- Hook target function
