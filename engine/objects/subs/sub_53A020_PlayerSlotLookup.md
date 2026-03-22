# sub_53A020 --- PlayerSlotLookup

**Address:** 0x53A020 (Spiderwick.exe+13A020) | **Size:** 47 bytes | **Calling convention:** __cdecl

---

## Purpose

Resolves a player type index (1=Jared, 2=Simon, 3=Mallory) to a 56-byte slot entry in the global player slot table at `dword_E58D70`. Uses 1-based indexing: type 1 maps to offset 0, type 2 to offset 56, etc. Returns NULL if the type exceeds the table's max slot count.

This is the central slot lookup used throughout the hot-switch system. On levels like MansionD where only slot 1 (Jared) exists, requesting type 2 or 3 returns NULL, which causes `SetPlayerType` to skip slot manipulation entirely.

---

## Prototype

```c
int __cdecl PlayerSlotLookup(unsigned int type)
```

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `type` | unsigned int | Character type: 1=Jared, 2=Simon(?), 3=Mallory(?). 0 is silently remapped to 1. |

**Returns:** `PlayerSlot*` (int) --- pointer to 56-byte slot entry, or 0 (NULL) if table is NULL or type exceeds max slots

---

## Decompiled Pseudocode

```c
int __cdecl PlayerSlotLookup(unsigned int type)
{
    if (!dword_E58D70)                          // global slot table pointer
        return 0;

    unsigned int v1 = type;
    if (!type)
        v1 = 1;                                 // default to Jared

    if (v1 <= *(DWORD *)dword_E58D70)           // table[+0x00] = max_count
        return *(DWORD *)(dword_E58D70 + 4) + 56 * v1 - 56;  // base + 56*(type-1)
    else
        return 0;                               // out of range
}
```

---

## Key Operations

1. **Null table guard:** Returns 0 immediately if the global `dword_E58D70` is NULL (table not initialized).
2. **Type 0 remap:** Silently remaps type 0 to type 1 (Jared), acting as a safe default.
3. **Bounds check:** Compares type against `*(dword_E58D70)` (max slot count). Returns 0 if out of range.
4. **Slot address calculation:** `base + 56 * (type - 1)`, where base is `*(dword_E58D70 + 4)`.

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x53A020` | Entry point |
| `dword_E58D70` | Global pointer to player slot table |
| `*(dword_E58D70 + 0x00)` | Max slot count |
| `*(dword_E58D70 + 0x04)` | Pointer to slot array base |

### Slot Table Layout

```
dword_E58D70 --> table pointer:
  [+0x00] DWORD  max_count    -- number of valid slots
  [+0x04] DWORD* base         -- pointer to start of slot array

Formula: slot_ptr = base + 56 * (type - 1)
Slots are 56 (0x38) bytes each, 1-indexed
```

### PlayerSlot Structure (56 bytes / 0x38)

| Offset | Size | Type | Name | Description |
|--------|------|------|------|-------------|
| +0x00 | 4 | DWORD | type | Slot type index |
| +0x04 | 4 | void* | charRef | Character object reference |
| +0x31 | 1 | byte | isPlayerControlled | Control byte: 1 = player-controlled |

---

## Called By (selected)

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | Slot lookup at 0x46389C |
| `sub_465AD0` | Alternate switching path |
| `sub_44F320` | Player iteration |
| `sub_455620` | Player validation |
| `DebugCameraManager_InputHandler` | Debug camera input |
| `SectorDistanceCheck` | Distance-based checks |
| `sub_491E10` | Player management |
| 120+ xrefs total | Heavily used throughout engine |

## Calls

None --- this is a pure arithmetic lookup function (leaf function, no sub-calls).

---

## Notes / Caveats

1. **Type 0 is silently remapped to 1 (Jared).** Passing 0 always returns Jared's slot.

2. **The 56-byte slot size** comes from the formula `base + 56 * type - 56`. The `-56` accounts for 1-based indexing.

3. **On MansionD, only slot 1 exists** (max_count=1). Requesting type 2 or 3 returns NULL, which causes `SetPlayerType` to skip slot manipulation and fall through to camera/data-store code only.

4. **Over 120 cross-references** make this one of the most heavily called functions in the player management subsystem. It is the canonical way to go from a type ID to a slot pointer.

5. **Related functions:**
   - [GetCharacterFromSlot](sub_539AE0_GetCharacterFromSlot.md) (sub_539AE0) --- dereferences slot to get character reference
   - [TransferInputController](sub_539CF0_TransferInputController.md) (sub_539CF0) --- uses slot for input transfer
   - [SetPlayerType](sub_463880_SetPlayerType.md) (sub_463880) --- primary consumer
