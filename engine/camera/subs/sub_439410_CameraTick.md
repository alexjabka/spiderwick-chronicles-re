# sub_439410 — CameraTick
**Address:** `Spiderwick.exe+39410` (absolute: `0x00439410`)
**Status:** PARTIALLY REVERSED

## Purpose
Camera tick function called every frame. Contains two critical write sites for
camera_obj's position matrices, and calls SectorTransition to determine which
sector the camera is in.

## Entry
Starts with `push esi; mov esi, ecx; jmp ds:off_1C82C08` — enters Kallis VM
which eventually executes the body at orphaned code block 0x439419-0x439519.

## Write Site 1: Player Position Stamp (0x439449)
Conditionally writes the PLAYER's world position into camera_obj+0x864/868/86C
(the translation row of the 4x4 matrix at +0x834):
```asm
439449  fld  [edi+68h]       ; player.X
43944C  fstp [esi+864h]      ; → camera_obj+0x864
439452  fld  [edi+6Ch]       ; player.Y
439455  fstp [esi+868h]      ; → camera_obj+0x868
43945B  fld  [edi+70h]       ; player.Z
43945E  fstp [esi+86Ch]      ; → camera_obj+0x86C
```
**Condition:** `(camera_obj+0x78C flags & 0x20) && !(flags & 0x2) && GetPlayerCharacter() != NULL`

**Freecam v15 hook:** Replaces player position with fc_eye (freecam position)
so SectorTransition computes the correct sector from the freecam's position.

## SectorTransition Call (~0x439500)
After Write Site 1, calls SectorTransition (sub_4391D0) which raycasts from
the position written to +0x864/868/86C to determine the camera's sector.
Writes result to camera_obj+0x788.

## Write Site 2: CopyPositionBlock (0x439512)
```asm
439505  lea edx, [esi+790h]   ; source = working camera buffer
43950B  push edx
43950C  lea ecx, [esi+834h]   ; dest = final camera matrix
439512  call CopyPositionBlock ; copies ~162 bytes +0x790 → +0x834
```
This commits the computed camera state into the final output matrix.
Runs AFTER SectorTransition, so it doesn't affect sector computation.

## Callers
| Address | Function |
|---------|----------|
| `0x43972F` | sub_4396B0 (via sub_4397B0) |
| `0x4368CE` | sub_4368C0 |
| `0x490DEC` | RenderPipeline3 |
| `0x4D4840` | Anonymous |

## Key Finding: Freecam Position Sync
Runtime debug dump confirmed:
- `camera_obj+0x6B8` (GetCameraPosition) DOES update to freecam position
- `camera_obj+0x864` (matrix at +0x834, row+0x30) stays FROZEN at activation position
- `camera_obj+0x788` (sector) stays frozen

Write Site 1 writes player position to +0x864 every frame. The freecam v15 hook
replaces this with fc_eye, making SectorTransition compute the correct sector.

## Related
- [sub_4391D0_SectorTransition.md](sub_4391D0_SectorTransition.md) — reads from +0x864
- [sub_488DD0_CameraSectorUpdate.md](sub_488DD0_CameraSectorUpdate.md) — caller chain
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full system overview
