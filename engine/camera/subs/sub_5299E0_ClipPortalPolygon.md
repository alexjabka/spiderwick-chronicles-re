# sub_5299E0 — ClipPortalPolygon
**Address:** `Spiderwick.exe+1299E0` (absolute: `0x005299E0`)
**Size:** 8 bytes
**Status:** FULLY REVERSED

## Purpose
Wrapper for the Sutherland-Hodgman polygon clipper (sub_5AC250). Clips a portal polygon against the camera frustum planes. Returns the number of vertices remaining after clipping — 0 means the portal is completely outside the frustum (invisible).

## Signature
```c
int __stdcall ClipPortalPolygon(int polygon_data, int frustum_planes, int output_buffer)
// ECX = this + 0x20 (adjusted before calling SutherlandHodgmanClip)
// Returns: vertex count after clipping (0 = invisible, 3+ = visible)
// Callee cleans 12 bytes (3 args × 4 bytes)
```

## Assembly (complete)
```asm
sub_5299E0:
  add ecx, 20h                ; adjust this pointer
  jmp sub_5AC250              ; tail-call SutherlandHodgmanClip
; total: 8 bytes (83 C1 20 E9 68 28 08 00)
```

## Call Sites
Only ONE caller: `sub_51A130` (PortalTraversal_Native) at `0x51A47A`.

## Bypass (NOT used in v3 solution)
Could be patched to always return 3 (visible):
```
B8 03 00 00 00    mov eax, 3
C2 0C 00          ret 0Ch
```
Fits perfectly in the 8-byte function. However, the v3 solution uses a cleaner approach: patching the branch at 0x51A3B5 to skip the clip test entirely.

## Related
- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) — only caller
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — system overview
