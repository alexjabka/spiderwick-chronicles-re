# sub_463880 --- SetPlayerType

**Address:** 0x463880 (Spiderwick.exe+63880) | **Calling convention:** __thiscall (ECX = ClPlayerObj*)

**vtable[116]** for ClPlayerObj (offset 464 from vtable base)

---

## Purpose

The core character switching function. Given a target player type (1=Jared, 2=Mallory, 3=Simon), looks up the corresponding player slot, resolves the character, and either delegates to the `.kallis` VM (for ClPlayerObj targets) or executes the native switch path (for non-ClPlayerObj targets like ThimbleTack).

This is called by `sauSetPlayerType` (sub_4626B0) from the VM.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | ClPlayerObj* | The currently active player character |
| `type` | int | Target character type: 1=Jared, 2=Mallory, 3=Simon |

**Returns:** int (ignored by callers)

---

## Decompiled Pseudocode

```c
int __thiscall SetPlayerType(ClPlayerObj *this, int type)
{
    // Guard: bail if character is already mid-switch
    if (IsCharSwitching(this))                          // sub_457850
        return;

    // Reset movement on all playable characters
    ResetAllCharMovement();                             // sub_462170

    // Look up target slot (56-byte entry from dword_E58D70)
    PlayerSlot *slot = GetPlayerSlot(type);             // sub_53A020
    if (!slot)
        goto postSwitch;

    // Resolve character entity from slot
    int entityRef = sub_539AE0(slot);
    ClCharacterObj *targetChar = ResolveCharFromEntity(entityRef);  // sub_450DA0

    if (targetChar)
    {
        // If target char is already switching, bail
        if (IsCharSwitching(targetChar))
            return;

        // === CRITICAL BRANCH at 0x4638DE ===
        // Check: is targetChar a ClPlayerObj?
        if (IsClassType(targetChar+4, off_6E2C58))     // sub_4053B0, checks "ClPlayerObj"
        {
            // YES: delegate to .kallis VM function
            // jmp ds:off_1C867CC --> 0x1CD7430 (ROP dispatcher)
            return off_1C867CC(type);  // VM handles full spawn/despawn
        }
        // NO: fall through to native path (non-ClPlayerObj targets only)
    }

    // === NATIVE PATH (non-ClPlayerObj, e.g., ThimbleTack) ===
    SetSlotState(slot, 1);                              // sub_539CB0 - enable slot
    int charRef = GetCharRef(this);                     // sub_4537B0 - this+300
    SwapSlotChar(slot, charRef);                        // sub_539CF0 - deactivate old, activate new
    Camera *nullCam = GetCameraObject(NULL);            // detach camera
    CameraClearAndInit(nullCam);                        // sub_439770

postSwitch:
    // Clear combat/interaction object field
    if (this->field_338)
        this->field_338->field_2C = 0;

    // Attach camera to this character
    Camera *cam = GetCameraObject(this);                // sub_4368B0
    ActivateCamera(cam);                                // sub_438E70

    // Activate character
    ActivateCharacter(this, 1);                         // sub_55C7B0

    // Update /Player/Character in data store
    if (this->widgetDesc)  // this+0x1C0
    {
        int nameHash = this->widgetDesc->hash;          // +0x24
        int charId = GetCharIdFromHash(nameHash, 1);    // sub_44FEC0
        int dsIndex = HashLookup("/Player/Character");  // sub_41E830
        if (dsIndex > -1)
        {
            *DataStoreGet(byte_E57F68, dsIndex) = charId;  // sub_5392C0
            DataStoreNotify(dsIndex);                       // sub_539750
        }
    }

    // Final activation via vtable[14]
    this->vtable[14](this);  // offset 56 = FinalActivation
}
```

---

## Key Addresses and Data

| Address | Instruction | Significance |
|---------|------------|--------------|
| `0x463885` | `call sub_457850` | Guard: IsCharSwitching check |
| `0x463892` | `call sub_462170` | ResetAllCharMovement |
| `0x46389C` | `call sub_53A020` | GetPlayerSlot(type) |
| `0x4638D7` | `call sub_4053B0` | IsClassType --- checks if target is ClPlayerObj |
| **`0x4638DE`** | **`jz short loc_4638EC`** | **THE critical branch.** `74 0C`. If target IS ClPlayerObj, jz is NOT taken, and we jump to .kallis. If target is NOT ClPlayerObj, jz IS taken, native path executes. |
| `0x4638E2` | `jmp ds:off_1C867CC` | Indirect jump to .kallis ROP code at 0x1CD7430 |
| `0x4638F0` | `call sub_539CB0` | SetSlotState(slot, 1) --- native path |
| `0x4638FF` | `call sub_539CF0` | SwapSlotChar --- native path |
| `0x463924` | `call GetCameraObject` | Camera attachment |
| `0x463933` | `call sub_55C7B0` | ActivateCharacter(this, 1) |
| `0x46395F` | `call sub_41E830` | Hash "/Player/Character" for data store |

---

## Assembly (Critical Section: 0x4638CF--0x4638EC)

```asm
4638cf  push    offset off_6E2C58    ; "ClPlayerObj" class descriptor
4638d4  lea     ecx, [ebx+4]        ; targetChar + 4 (class chain start)
4638d7  call    sub_4053B0           ; IsClassType(targetChar+4, "ClPlayerObj")
4638dc  test    al, al
4638de  jz      short loc_4638EC    ; if NOT ClPlayerObj -> native path
4638e0  mov     edx, [ebx]          ; edx = targetChar->vtable (unused, artifact)
4638e2  jmp     ds:off_1C867CC      ; --> .kallis at 0x1CD7430 (ROP dispatcher)
                                     ; type is still in [esp+arg_0]
; --- .kallis ROP code at 1CD7430 ---
; 1cd7430  push    offset off_1CD744A
; 1cd7435  push    401FF6h
; 1cd743a  push    offset dword_1419690
; 1cd743f  pushf
; 1cd7440  sub     [esp+1Ch+var_18], 1F000h
; 1cd7448  popf
; 1cd7449  retn    ; ROP dispatch continues
```

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_4626B0` (sauSetPlayerType) | VM handler: `vtable[116](this, type)` |
| `.kallis` scripts | Via VM method dispatch on ClPlayerObj |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x457850` | IsCharSwitching | Guard check |
| `0x462170` | ResetAllCharMovement | Reset all playable chars' movement |
| `0x53A020` | GetPlayerSlot | Slot lookup by type |
| `0x539AE0` | GetSlotEntityRef | Dereference slot to entity |
| `0x450DA0` | ResolveCharFromEntity | Entity --> ClCharacterObj* |
| `0x4053B0` | IsClassType | Walk class chain for type match |
| `0x539CB0` | SetSlotState | Enable/disable slot (.kallis thunk) |
| `0x4537B0` | GetCharRef | Returns this+300 (character reference) |
| `0x539CF0` | SwapSlotChar | Swap char in slot + deactivate/activate |
| `0x4368B0` | GetCameraObject | Get camera for character (0 = detach) |
| `0x439770` | CameraClearAndInit | Clear camera chain + init |
| `0x438E70` | ActivateCamera | Activate camera (.kallis thunk) |
| `0x55C7B0` | ActivateCharacter | Activate character (.kallis thunk) |
| `0x44FEC0` | GetCharIdFromHash | Widget hash --> character ID |
| `0x41E830` | HashLookup | Data store key --> index |
| `0x5392C0` | DataStoreGet | Index --> value pointer |
| `0x539750` | DataStoreNotify | Notify data store of change |

---

## Notes / Caveats

1. **The jz at 0x4638DE is why native hot-switch fails for player characters.** For ClPlayerObj targets, the function unconditionally delegates to the `.kallis` VM. The VM orchestrates character spawning/despawning, which cannot execute from an EndScene hook context.

2. **Patching jz (74 0C) to jmp (EB 0C) forces the native path**, but the native path was designed for non-ClPlayerObj (ThimbleTack). It assumes the target character already exists in memory and only swaps slot references + camera. It does NOT spawn a new character or despawn the old one, resulting in broken input routing.

3. **The native path writes `/Player/Character` to the data store** (at 0x46395F), which means a subsequent world load WILL correctly pick up the new type. This is the basis of the "data store approach" --- write the type, trigger a load.

4. **`off_1C867CC`** is an indirect jump target in the `.kallis` segment. It resolves to `0x1CD7430`, which is ROP-style code (push/push/push/pushf/sub/popf/retn). This is how the Kallis VM implements computed gotos.

5. **`sub_4053B0` (IsClassType)** walks a linked list at `obj+8` checking each node against the provided class descriptor. For ClPlayerObj, the chain includes "ClPlayerObj" --> "ClCharacterObj" --> base. This is how the engine implements runtime type identification.
