# sub_55DAA0 — SubmitFadeRequest

**Address:** `Spiderwick.exe+15DAA0` (absolute: `0055DAA0`)
**Convention:** __thiscall
**Returns:** void

## Signature
```c
void __thiscall SubmitFadeRequest(FadeManager *this, /* params TBD */)
```

## Description
Allocates a fade slot and populates it with the requested fade parameters. The fade manager has 10 slots (32 bytes each) starting at `this+56`. Iterates through slots looking for a free one via `IsFadeSlotAvailable`.

## Key Details
- 10 fade slots, 32 bytes each, starting at `this+56`
- Slot layout: offset +20 = duration (float), +24 = elapsed (float), +28 = flag (int)
- A slot is free when duration==0.0, elapsed==0.0, and flag==0
- On allocation: sets duration, zeros elapsed, sets flag to active

## Called By
- `CreateFadeRequest` (0x55DB30)

## Calls
- `IsFadeSlotAvailable` (0x59DA70) — checks slot availability

## Related
- [sub_55DB30_CreateFadeRequest.md](sub_55DB30_CreateFadeRequest.md) — caller
- [sub_59DA70_IsFadeSlotAvailable.md](sub_59DA70_IsFadeSlotAvailable.md) — slot check
- [../FADE_SYSTEM.md](../FADE_SYSTEM.md) — fade system overview
