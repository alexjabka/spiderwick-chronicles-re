# sub_488660 — PostWorldLoadInit

**Address:** `Spiderwick.exe+88660` (absolute: `00488660`)
**Convention:** __cdecl
**Returns:** void

## Signature
```c
void __cdecl PostWorldLoadInit(void)
```

## Description
Called after world loading completes to perform post-load initialization. This is the bridge between the world loading system and the script/event system. Its key responsibility is dispatching the `"MissionStart"` event, which triggers all level initialization scripts.

## Key Details
- Called after the world loading pipeline finishes (geometry, sectors, objects all loaded)
- Sequence of calls:
  1. `sub_4DC6A0(flag)` — pre-initialization step
  2. `DispatchEvent("MissionStart")` (0x52EBE0) — triggers level scripts
  3. `sub_4DC330(2)` — post-initialization step
- The `"MissionStart"` string is at address `0x62F4C0` in .rdata
- Modifying the first byte of this string (`'M'` -> `'X'`) suppresses all mission scripting (Free Explore technique)

## Called By
- World loading pipeline (after LoadWorld completes)

## Calls
- `sub_4DC6A0` — pre-init (details TBD)
- `DispatchEvent` (0x52EBE0) — dispatches "MissionStart"
- `sub_4DC330` — post-init with parameter 2

## Related
- [sub_52EBE0_DispatchEvent.md](sub_52EBE0_DispatchEvent.md) — event dispatch
- [../../world/WORLD_LOADING.md](../../world/WORLD_LOADING.md) — world loading pipeline
- [../EVENT_SYSTEM.md](../EVENT_SYSTEM.md) — event system overview
