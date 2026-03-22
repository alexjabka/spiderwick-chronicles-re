# sub_50B760 — SetProjection
**Address:** Spiderwick.exe+0B760 (absolute: 0x50B760)
**Status:** FULLY REVERSED

## Purpose
Sets the projection matrix via D3DXMatrixPerspectiveFovLH.
Thiscall — stores parameters in the object, then calls D3D.

## Signature
void __thiscall SetProjection(void *this, float fov, float aspect, float near, float far)

## Object Offsets
- this+0x124: FOV (stored before ecx adjustment)
- this+0x128: aspect ratio (= this+0x44+0xE4)
- this+0x12C: near plane (= this+0x44+0xE8)
- this+0x130: far plane (= this+0x44+0xEC)
- this+0x44: projection matrix output (passed to D3DXMatrixPerspectiveFovLH)

## Assembly
sub_50B760:
  fld   [esp+arg_0]           ; fov
  sub   esp, 10h
  fst   [ecx+124h]            ; store FOV
  add   ecx, 44h              ; ecx -> projection matrix
  fld   [esp+10h+arg_4]       ; aspect
  fst   [ecx+0E4h]            ; store aspect
  fld   [esp+10h+arg_8]       ; near
  fst   [ecx+0E8h]            ; store near
  fld   [esp+10h+arg_C]       ; far
  fst   [ecx+0ECh]            ; store far
  fstp  [esp+var_4]            ; push args for D3DX (reverse FPU order)
  fstp  [esp+var_8]
  fstp  [esp+var_C]
  fstp  [esp+var_10]
  push  ecx                    ; matrix pointer
  call  D3DXMatrixPerspectiveFovLH
  retn  10h                    ; stdcall cleanup (4 float args)

## Hook Point (for far plane override)
First 7 bytes: D9 44 24 04 83 EC 10 (fld [esp+4] / sub esp,10h)
Hook replaces these, overrides far arg at [esp+20h] after sub esp.

## Callers
Called via vtable entries at 0x63e1bc, 0x63e1dc, 0x63e1fc.
