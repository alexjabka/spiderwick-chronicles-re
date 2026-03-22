# sub_59DA70 — IsFadeSlotAvailable

**Address:** `Spiderwick.exe+19DA70` (absolute: `0059DA70`)
**Convention:** __thiscall
**Returns:** bool (int)

## Signature
```c
int __thiscall IsFadeSlotAvailable(FadeManager *this, int slotIndex)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| this (ecx) | FadeManager* | Fade manager object |
| slotIndex | int | Slot index (0-9) |

## Description
Returns true when a fade slot is available for use. Checks three conditions on the slot at `this+56 + slotIndex*32`:

```c
return (duration == 0.0f) && (elapsed == 0.0f) && (flag == 0);
```

## Key Details
- Slot base: `this + 56 + slotIndex * 32`
- Duration at slot+20, elapsed at slot+24, flag at slot+28
- All three must be zero/0.0 for the slot to be considered free

## Called By
- `SubmitFadeRequest` (0x55DAA0) — iterates slots looking for a free one

## Related
- [sub_55DAA0_SubmitFadeRequest.md](sub_55DAA0_SubmitFadeRequest.md) — caller
- [../FADE_SYSTEM.md](../FADE_SYSTEM.md) — fade system overview
