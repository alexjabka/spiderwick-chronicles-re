# Hot-Switch System

**Status:** Fully reversed (Session 4) -- complete native switching pipeline, VM delegation boundary, input transfer, deferred hook mechanism

---

## Overview

The "hot-switch" system is the engine's mechanism for switching the active player character at runtime. It encompasses the full pipeline from the VM `sauSetPlayerType` call down through slot manipulation, input controller transfer, camera reattachment, character activation, and data store updates.

The system is centered on **SetPlayerType** (`sub_463880`, vtable[116] on ClPlayerObj), which orchestrates the entire switch. For ClPlayerObj targets (Jared/Mallory/Simon), the function delegates to the `.kallis` VM for full spawn/despawn orchestration. For non-ClPlayerObj targets (ThimbleTack), it executes a native path that swaps slot references, transfers input, and reattaches the camera.

---

## Architecture

```
sauSetPlayerType (sub_4626B0) -- VM handler
  |
  +-- SetPlayerType (sub_463880, vtable[116])
  |     |
  |     +-- IsCharacterDead guard (sub_457850) -- bail if dead/switching
  |     +-- DeactivateAllPlayers (sub_462170) -- reset all chars' movement
  |     +-- PlayerSlotLookup (sub_53A020) -- resolve type to 56-byte slot
  |     +-- GetCharacterFromSlot (sub_539AE0) -- slot -> entity ref
  |     +-- GetEntityFromCharacter (sub_450DA0) -- entity ref -> ClCharacterObj*
  |     +-- ClassChainCheck (sub_4053B0) -- is target ClPlayerObj?
  |     |     |
  |     |     +-- YES: jmp to .kallis at off_1C867CC (VM orchestration)
  |     |     +-- NO:  native path below
  |     |
  |     +-- [NATIVE PATH]
  |     |   +-- SetActivePlayerCount (sub_539CB0) -- enable slot
  |     |   +-- GetInputController (sub_4537B0) -- this+300
  |     |   +-- TransferInputController (sub_539CF0) -- swap input bindings
  |     |   +-- Camera detach + reattach (GetCameraObject, sub_438E70)
  |     |
  |     +-- ActivateAsPlayer (sub_55C7B0, flag=1)
  |     +-- WidgetHashToType (sub_44FEC0) -- hash to type ID
  |     +-- Write /Player/Character to data store
  |     +-- vtable[14](this) -- FinalActivation
  |
  +-- CommitPlayerState (sub_462B80, vtable[113])
        +-- Load health, weapons, input mode from data store
```

---

## The Critical Branch (0x4638DE)

The `jz` instruction at address `0x4638DE` is the decision point that determines whether the switch is handled by the VM or by native code.

### Assembly at 0x4638CF--0x4638E8

```asm
4638cf  push    offset off_6E2C58    ; "ClPlayerObj" class descriptor
4638d4  lea     ecx, [ebx+4]        ; targetChar + 4 (class chain start)
4638d7  call    sub_4053B0           ; IsClassType(targetChar+4, "ClPlayerObj")
4638dc  test    al, al
4638de  jz      short loc_4638EC    ; if NOT ClPlayerObj -> native path
4638e0  mov     edx, [ebx]          ; edx = targetChar->vtable (artifact)
4638e2  jmp     ds:off_1C867CC      ; --> .kallis ROP at 0x1CD7430
```

| Condition | jz behavior | Result |
|-----------|-------------|--------|
| Target IS ClPlayerObj | jz NOT taken | Delegates to .kallis VM (full spawn/despawn) |
| Target NOT ClPlayerObj | jz IS taken | Falls through to native path |

### Why patching jz to jmp fixes coop-to-substitute switching

When the target character is a ClPlayerObj, the VM path is taken. The VM orchestrates a full spawn/despawn cycle that is not callable from arbitrary native contexts (e.g., an EndScene hook). Patching `jz` (opcode `74 0C`) to `jmp` (opcode `EB 0C`) forces the native path for ALL targets, including ClPlayerObj. This means:

- **Works for coop-to-substitute:** When a second player character already exists in memory (coop mode), the native path correctly swaps the slot reference, transfers input, and reattaches the camera without needing to spawn/despawn.
- **Breaks for standard switching:** When only one character exists in memory, the native path cannot spawn the target character -- it assumes the target already exists. This leaves the game in a broken state.

---

## Sub-Function Details

### 1. SetPlayerType (sub_463880, vtable[116])

The core switching function. Full documentation: [subs/sub_463880_SetPlayerType.md](subs/sub_463880_SetPlayerType.md)

**Key flow:**
1. Guard: bail if `IsCharSwitching(this)` returns true
2. `DeactivateAllPlayers()` -- reset movement on all playable characters
3. `GetPlayerSlot(type)` -- look up 56-byte slot from `dword_E58D70`
4. Resolve target character from slot entity reference
5. If target exists and is switching, bail
6. **Class check at 0x4638DE** -- ClPlayerObj? delegate to VM. Otherwise, native path.
7. Native path: enable slot, transfer input, swap camera
8. Write `/Player/Character` to data store via `sub_44FEC0` + `sub_41E830`

### 2. PlayerSlotLookup (sub_53A020)

Resolves a character type to a player slot entry. Full documentation: [subs/sub_53A020_PlayerSlotLookup.md](subs/sub_53A020_PlayerSlotLookup.md)

**Slot table structure:**
```
dword_E58D70 --> table pointer:
  [+0x00] DWORD  max_count    -- number of valid slots
  [+0x04] DWORD* base         -- pointer to start of slot array

Formula: slot_ptr = base + 56 * (type - 1)
Slots are 56 (0x38) bytes each, 1-indexed (type 1 = offset 0)
Type 0 is silently remapped to 1
Returns NULL if type > max_count
```

**Critical behavior:** On levels with only 1 slot defined (max_count=1), requesting type 2 or 3 returns NULL. This causes `SetPlayerType` to skip slot manipulation entirely and fall through to the camera/data-store code, which results in a coop-mode camera attachment without an actual character switch.

### 3. DeactivateAllPlayers (sub_462170)

Iterates all playable characters via `GetPlayerCharacter2()` and `NextPlayableCharacter()`, resetting their movement parameters. Full documentation: [subs/sub_462170_DeactivateAllPlayers.md](subs/sub_462170_DeactivateAllPlayers.md)

```c
void DeactivateAllPlayers()
{
    ClCharacterObj *cur = GetPlayerCharacter2();
    while (cur)
    {
        if (cur[78] != 0)  // has active controller at offset +0x138
        {
            sub_551F50(1.0f);  // reset X axis
            sub_551F80(1.0f);  // reset Y axis
        }
        cur = NextPlayableCharacter(cur);
    }
}
```

### 4. SetActivePlayerCount (sub_539CB0)

A `.kallis` thunk that sets the active player count/slot state. Dispatches to ROP code at `off_1C8D810`. Full documentation: [subs/sub_539CB0_SetSlotState.md](subs/sub_539CB0_SetSlotState.md)

```c
int __stdcall SetActivePlayerCount(int flag)
{
    return off_1C8D810(flag);  // .kallis ROP dispatcher
}
```

Called as `sub_539CB0(1)` in `SetPlayerType` to enable the target slot before swapping.

### 5. GetInputController (sub_4537B0)

Returns the character's input controller block at `this + 300` (0x12C). Full documentation: [subs/sub_4537B0_GetInputController.md](subs/sub_4537B0_GetInputController.md)

```c
void* __thiscall GetInputController(char *this)
{
    return this + 300;  // offset 0x12C
}
```

This 7-byte function is the accessor for the input controller embedded within each character object. The returned pointer is passed to `TransferInputController` during character switches.

### 6. TransferInputController (sub_539CF0)

Transfers P1 input bindings from the slot's current controller to a new controller. Full documentation: [subs/sub_539CF0_TransferInputController.md](subs/sub_539CF0_TransferInputController.md)

```c
int __thiscall TransferInputBindings(PlayerSlot *this, void *newController)
{
    if (!newController)
        newController = &unk_E58D6C;  // null sentinel

    int oldController = this->charRef;  // slot+4
    int newEntity = newController->vtable[2](newController);  // compare via vtable[2]
    int oldEntity = oldController->vtable[2](oldController);

    if (newEntity != oldEntity)
    {
        sub_539BC0(this->controlByte, newController, this->descriptor);  // unbind old
        sub_539C20(this->controlByte, newController, this->descriptor);  // bind new
        this->charRef = newController;  // update slot+4
    }
}
```

**vtable[2]** is used to extract a comparable identity from each controller, ensuring the transfer only happens when the controllers actually differ.

### 7. ActivateAsPlayer (sub_55C7B0)

A `.kallis` thunk that activates a character as a player. Called with `flag=1` for player activation. Full documentation: [subs/sub_55C7B0_ActivateAsPlayer.md](subs/sub_55C7B0_ActivateAsPlayer.md)

```c
int __cdecl ActivateAsPlayer(int charObj, int flag)
{
    return off_1C82C34(charObj);  // .kallis ROP dispatcher
}
```

Note: The `flag` parameter (always 1 in `SetPlayerType`) is passed on the stack but the thunk only forwards `charObj` to the .kallis handler.

### 8. IsCharacterDead (sub_457850)

Guard check at the start of SetPlayerType. Returns true if the character is dead, invalid, or mid-switch. Full documentation: [subs/sub_457850_IsCharacterDead.md](subs/sub_457850_IsCharacterDead.md)

```c
int __thiscall IsCharacterDead(void *this)
{
    return off_1C890B0(this);   // .kallis thunk
}
```

Called TWICE in SetPlayerType: once on `this` (current player) and once on the resolved target character. Both must return 0 for the switch to proceed.

### 9. GetCharacterFromSlot (sub_539AE0)

Retrieves the character/entity reference from a player slot. Full documentation: [subs/sub_539AE0_GetCharacterFromSlot.md](subs/sub_539AE0_GetCharacterFromSlot.md)

```c
int __thiscall GetCharacterFromSlot(_DWORD *this)
{
    return *(this + 1);   // slot+0x04: character reference
}
```

Returns the raw entity reference at slot offset +0x04. Must be resolved via `GetEntityFromCharacter` (sub_450DA0) to get the actual `ClCharacterObj*`.

### 10. GetEntityFromCharacter (sub_450DA0)

Resolves an entity reference to an actual game object pointer. Full documentation: [subs/sub_450DA0_GetEntityFromCharacter.md](subs/sub_450DA0_GetEntityFromCharacter.md)

```c
int __cdecl GetEntityFromCharacter(int entityRef)
{
    if (entityRef->vtable[1](entityRef))        // validity check
        return sub_4D3740(entityRef);           // returns *(entityRef + 4)
    else
        return 0;                               // invalid
}
```

Standard pattern: `GetCharacterFromSlot(slot)` -> `GetEntityFromCharacter(ref)` -> usable `ClCharacterObj*`.

### 11. ClassChainCheck (sub_4053B0)

RTTI class hierarchy check. Full documentation: [subs/sub_4053B0_ClassChainCheck.md](subs/sub_4053B0_ClassChainCheck.md)

```c
char __thiscall ClassChainCheck(_DWORD *this, int targetClass)
{
    int node = *(this + 2);
    while (node) {
        if (node == targetClass) return 1;
        node = *(DWORD *)(node + 4);
    }
    return 0;
}
```

Used at 0x4638DE to determine if the switch target is a `ClPlayerObj` (VM path) or not (native path).

### 12. CameraSectorUpdate (sub_488DD0)

The per-frame render/update function. Full documentation: [../camera/subs/sub_488DD0_CameraSectorUpdate.md](../camera/subs/sub_488DD0_CameraSectorUpdate.md)

This function is called every game update frame and is the hook point for deferred character switching. By hooking `CameraSectorUpdate`, a mod can execute a switch at a safe point in the game loop (after the previous frame's rendering is complete but before the next frame begins).

**Pipeline:**
1. Get camera object
2. Pass 1: UpdateVisibility (sector culling, world tick)
3. Camera object updates (sector may change)
4. Pass 2: SectorVisibilityUpdate2 (frustum, portal traversal)
5. Pass 3: RenderDispatch (D3D draw calls)

---

## Data Store Variables

The hot-switch system reads and writes several data store paths:

| Path | Type | Values | Purpose |
|------|------|--------|---------|
| `/Player/Character` | int | 1=Jared, 2=Mallory(widget)/Simon(visual), 3=Simon(widget)/Mallory(visual) | Active player type |
| `/Player/Jared/Health` | int | -- | Jared's current health |
| `/Player/Simon/Health` | int | -- | Simon's current health |
| `/Player/Mallory/Health` | int | -- | Mallory's current health |
| `/Story/Chapter` | int | 1-8 | Current chapter number |
| `/Story/Index` | int | threshold 1030 for endgame camera | Story progression index |

**Data store index for `/Player/Character`:** 655 (hash `0xA488A96A`)

---

## Key Addresses

| Address | Name | Type | Purpose |
|---------|------|------|---------|
| `0x463880` | SetPlayerType | vtable[116] | Core switching function |
| `0x4638DE` | -- | instruction | Critical jz: ClPlayerObj check |
| `0x53A020` | PlayerSlotLookup | __cdecl | Slot lookup by type (56-byte entries) |
| `0x462170` | DeactivateAllPlayers | __cdecl | Reset all playable chars' movement |
| `0x539AE0` | GetCharacterFromSlot | __thiscall | Returns slot+4 entity reference |
| `0x450DA0` | GetEntityFromCharacter | __cdecl | Entity ref -> ClCharacterObj* |
| `0x457850` | IsCharacterDead | __thiscall (.kallis) | Dead/switching guard check |
| `0x4053B0` | ClassChainCheck | __thiscall | Walk class chain for RTTI match |
| `0x539CB0` | SetActivePlayerCount | .kallis thunk | Enable/disable slot |
| `0x4537B0` | GetInputController | __thiscall | Returns this+300 (input controller) |
| `0x539CF0` | TransferInputController | __thiscall | Transfer input bindings in slot |
| `0x55C7B0` | ActivateAsPlayer | .kallis thunk | Activate character as player |
| `0x488DD0` | CameraSectorUpdate | __cdecl | Per-frame update (hook point) |
| `0x44FEC0` | WidgetHashToType | .kallis thunk | Widget hash to player type number |
| `0xE58D70` | dword_E58D70 | DWORD | Player slot table base pointer |
| `0x1C867CC` | off_1C867CC | .kallis | VM entry for ClPlayerObj switching |

---

## Related Documentation

### System-level docs
- [CHARACTER_SWITCHING.md](CHARACTER_SWITCHING.md) -- Full switching system overview and failed approaches
- [PLAYER_SLOT_SYSTEM.md](PLAYER_SLOT_SYSTEM.md) -- Slot table layout and manipulation
- [CHARACTER_SYSTEM.md](CHARACTER_SYSTEM.md) -- Character linked list and identification
- [../vm/KALLIS_VM.md](../vm/KALLIS_VM.md) -- VM architecture
- [../data/DATA_STORE.md](../data/DATA_STORE.md) -- Data store system

### Core switching pipeline
- [subs/sub_463880_SetPlayerType.md](subs/sub_463880_SetPlayerType.md) -- SetPlayerType (vtable[116], core orchestrator)
- [subs/sub_457850_IsCharacterDead.md](subs/sub_457850_IsCharacterDead.md) -- IsCharacterDead (guard check, .kallis thunk)
- [subs/sub_462170_DeactivateAllPlayers.md](subs/sub_462170_DeactivateAllPlayers.md) -- DeactivateAllPlayers (reset all chars' movement)

### Slot resolution chain
- [subs/sub_53A020_PlayerSlotLookup.md](subs/sub_53A020_PlayerSlotLookup.md) -- PlayerSlotLookup (type -> 56-byte slot)
- [subs/sub_539AE0_GetCharacterFromSlot.md](subs/sub_539AE0_GetCharacterFromSlot.md) -- GetCharacterFromSlot (slot -> entity ref)
- [subs/sub_450DA0_GetEntityFromCharacter.md](subs/sub_450DA0_GetEntityFromCharacter.md) -- GetEntityFromCharacter (entity ref -> ClCharacterObj*)

### Type checking and native path
- [subs/sub_4053B0_ClassChainCheck.md](subs/sub_4053B0_ClassChainCheck.md) -- ClassChainCheck (RTTI class hierarchy walk)
- [subs/sub_539CB0_SetSlotState.md](subs/sub_539CB0_SetSlotState.md) -- SetActivePlayerCount (.kallis thunk)
- [subs/sub_4537B0_GetInputController.md](subs/sub_4537B0_GetInputController.md) -- GetInputController (this+300 accessor)
- [subs/sub_539CF0_TransferInputController.md](subs/sub_539CF0_TransferInputController.md) -- TransferInputController (swap input bindings)

### Activation and finalization
- [subs/sub_55C7B0_ActivateAsPlayer.md](subs/sub_55C7B0_ActivateAsPlayer.md) -- ActivateAsPlayer (.kallis thunk, flag=1)
- [subs/sub_44FEC0_WidgetHashToType.md](subs/sub_44FEC0_WidgetHashToType.md) -- WidgetHashToType (hash to type ID)

### Related handlers
- [subs/sub_491F30_sauSubstitutePlayer.md](subs/sub_491F30_sauSubstitutePlayer.md) -- sauSubstitutePlayer handler
- [subs/sub_492030_sauInteractHandler.md](subs/sub_492030_sauInteractHandler.md) -- sauInteractHandler
- [../camera/subs/sub_488DD0_CameraSectorUpdate.md](../camera/subs/sub_488DD0_CameraSectorUpdate.md) -- CameraSectorUpdate (hook point)
