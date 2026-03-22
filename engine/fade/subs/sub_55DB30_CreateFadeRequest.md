# sub_55DB30 — CreateFadeRequest

**Address:** `Spiderwick.exe+15DB30` (absolute: `0055DB30`)
**Convention:** __thiscall (ecx = fade manager)
**Returns:** void

## Signature
```c
void __thiscall CreateFadeRequest(FadeManager *this, int color, float duration)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| this (ecx) | FadeManager* | Fade manager object (resolved via ServiceLookup) |
| color | int | Fade color (e.g., 0x000000 for black) |
| duration | float | Fade duration in seconds |

## Description
Creates a fade overlay request (e.g., fade to black before a world transition). Internally calls `SubmitFadeRequest` (0x55DAA0) to find an available slot and populate it.

## DO NOT HOOK

**This function MUST NOT be hooked with MinHook.** Even calling `MH_CreateHook` (without `MH_EnableHook`) modifies the prologue bytes, corrupting the function. This causes crashes during world transitions when the engine calls this function for loading screen fades.

The crash occurs because MinHook's trampoline creation overwrites the first bytes of the function to place a `jmp`, and for this particular function, the displaced instructions do not survive relocation.

## Key Details
- Called during world transitions to create the fade-to-black overlay
- Fade manager instance is obtained via `ServiceLookup` (0x537D10) using the hash at `dword_E9C7C4`
- Allocates one of 10 fade slots (32 bytes each, starting at this+56)
- Loading screen fades are VM-internal and bypass this function

## Called By
- World transition code (before LoadWorld)
- Cutscene system (fade effects)

## Calls
- `SubmitFadeRequest` (0x55DAA0) — slot allocation

## Related
- [sub_55DAA0_SubmitFadeRequest.md](sub_55DAA0_SubmitFadeRequest.md) — slot allocator
- [sub_59DA70_IsFadeSlotAvailable.md](sub_59DA70_IsFadeSlotAvailable.md) — slot check
- [../FADE_SYSTEM.md](../FADE_SYSTEM.md) — fade system overview
