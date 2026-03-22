# sub_4368B0 — GetCameraObject
**Address:** `Spiderwick.exe+368B0` (absolute: `004368B0`)

## Purpose
Returns the SectorCameraObject singleton — the camera object used by the
sector/visibility system. **NOT the same as CameraManager/pCamStruct.**

## Signature
```c
CameraObject* __cdecl GetCameraObject()
// Returns: pointer to camera object (sector/visibility)
// No arguments
```

## Runtime Code (after .kallis patching)
In IDA this is a dead thunk chain ending in `nullsub_56` (`retn`).
At runtime, .kallis patches it to load from a global:

```asm
004368B0:  JMP 00447AD6           ; E9 21 12 01 00
004368B5:  RET                    ; C3

00447AD6:  MOV EAX, [0072F670]    ; A1 70 F6 72 00 — load camera_obj pointer
00447ADB:  JMP 004368B5           ; E9 D5 ED FE FF — back to RET
```

**Effective implementation:** `return *(DWORD*)0x0072F670;`

## Key Offset
- **`camera_obj + 0x788`** — camera sector index (int)

## Static Address
| Address | Type | Value |
|---------|------|-------|
| `0x0072F670` | ptr | camera_obj pointer (always valid in-game) |

## IDA vs Runtime
- **IDA:** `sub_4368B0 → sub_447AD6 → nullsub_56` (nullsub_56 = just `retn`)
- **Runtime:** nullsub_56 is never reached; 00447AD6 is overwritten by .kallis
  with `MOV EAX, [0072F670]; JMP back`

## Callers
- `sub_488DD0` (CameraSectorUpdate) — every frame, to read camera sector
- `sub_490F40` (sauSetCameraSector) — scripting API
- Any code needing the camera's sector membership

## Not To Confuse With
| Object | How to get | Purpose |
|--------|-----------|---------|
| CameraManager | `pCamStruct - 0x480` | View matrix pipeline, component system |
| pCamStruct | `[pCamStruct]` | View matrix destination (+0x00 = 4x4 matrix) |
| **SectorCameraObject** | `[0x0072F670]` / `sub_4368B0()` | Sector/visibility system |

## Status: FULLY REVERSED
