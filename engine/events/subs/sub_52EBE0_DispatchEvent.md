# sub_52EBE0 — DispatchEvent

**Address:** `Spiderwick.exe+12EBE0` (absolute: `0052EBE0`)
**Convention:** __cdecl (VM thunk)
**Returns:** void

## Signature
```c
void __cdecl DispatchEvent(const char *eventName)
```

## Parameters
| Name | Type | Description |
|------|------|-------------|
| eventName | const char* | Name of the event to dispatch (e.g., "MissionStart") |

## Description
Dispatches a named event to all registered Kallis script handlers. This is a VM thunk that pushes the event name into the VM interpreter, which then looks up and invokes all handlers registered for that event name.

## Key Details
- VM thunk pattern: pushes parameters and calls into the Kallis VM interpreter
- The event name is matched against registered handler names in the script system
- If no handlers match the event name, nothing happens (no error, no crash)
- Accepts DLL-allocated string pointers (not limited to .rdata strings)
- Key use: `DispatchEvent("MissionStart")` after every world load

## Called By
- `PostWorldLoadInit` (0x488660) — dispatches "MissionStart" after world load completes

## Related
- [sub_488660_PostWorldLoadInit.md](sub_488660_PostWorldLoadInit.md) — caller
- [sub_52EA10_RegisterScriptFunction.md](sub_52EA10_RegisterScriptFunction.md) — related registration API
- [../EVENT_SYSTEM.md](../EVENT_SYSTEM.md) — event system overview
