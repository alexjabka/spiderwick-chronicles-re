# sub_5198D0 — IsSectorVisible (DEBUG ONLY)
**Address:** Spiderwick.exe+1198D0 (absolute: 0x5198D0)
**Size:** 18 bytes (0x12)
**Status:** FULLY REVERSED

## Purpose
DEBUG ONLY visibility check. Reads the `dword_E416CC` visibility array and returns
true if the sector's byte[0] is non-zero. Called ONLY from sub_599760
(DebugSectorDisplay) — the debug overlay that shows sector states as text
("Drawing", "Loaded", "Unloaded", etc.).

## WARNING
**Patching this function does NOT affect geometry rendering.** This was confirmed
in experiment v9 (patched to `mov al,1; ret` — objects appeared but rooms stayed
dark). The real render gate is the visibility check in sub_1C93F90
(SectorRenderSubmit) at address 0x1C93FB4.

This function is purely for the debug display overlay. Do not waste time patching it
for render visibility purposes.

## Assembly
```asm
5198D0: mov  eax, [esp+4]        ; eax = sector_index (arg1)
5198D4: mov  ecx, dword_E416CC   ; ecx = visibility array base
5198DA: cmp  byte ptr [ecx+eax*8], 0  ; check visibility flag (byte[0] of 8-byte entry)
5198DE: setnz al                 ; al = 1 if visible, 0 if not
5198E1: retn                     ; return al
```

## Pseudocode
```c
bool __cdecl IsSectorVisible_DEBUG(int sector_index) {
    byte *vis_array = (byte *)dword_E416CC;
    return vis_array[sector_index * 8] != 0;
}
```

## Visibility Array Layout (dword_E416CC)
Each sector entry is 8 bytes:
| Offset | Type | Field |
|--------|------|-------|
| +0x00 | byte | Visibility flag (0 = not visible, non-zero = visible) |
| +0x01 | byte | Side/direction value |
| +0x04 | dword | Frustum ID |

This function reads ONLY byte[0]. The same array is read by sub_1C93F90
(SectorRenderSubmit) which is the ACTUAL rendering gate.

## Call Chain
```
DebugSectorDisplay (0x599760) — debug overlay
  → IsSectorVisible_DEBUG (0x5198D0) ← this function
    → reads dword_E416CC[sector_index * 8]
```

NOT called from:
- sub_1C93F90 (SectorRenderSubmit) — has its own inline visibility check
- sub_562760 (RenderDispatch) — processes queue, no visibility checks
- Any render pipeline function

## Experiment History
- **v9:** Patched to `mov al, 1; ret` (3 bytes). Objects became visible (lamps,
  curtains) but rooms stayed DARK. This proved the function is debug-only and does
  not control actual sector geometry rendering.

## Global Data
| Address | Name | Purpose |
|---------|------|---------|
| `dword_E416CC` | Visibility Array | 8 bytes/sector: byte[0]=visible, byte[1]=side, dword[4]=frustum_id |
| `dword_E416C8` | Sector Count | Total sectors (14 indoors, used as loop bound elsewhere) |

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full portal/render system overview (see v9 in experiment history)
- [sub_1C93F90_SectorRenderSubmit.md](sub_1C93F90_SectorRenderSubmit.md) — the REAL render gate that reads the same array
- [sub_488DD0_CameraSectorUpdate.md](sub_488DD0_CameraSectorUpdate.md) — pipeline 1 entry
