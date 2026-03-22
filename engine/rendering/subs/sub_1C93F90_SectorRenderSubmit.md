# sub_1C93F90 — SectorRenderSubmit
**Address:** absolute `0x1C93F90` (in .kallis section, but native x86 code)
**Size:** 0x193 bytes
**Status:** FULLY REVERSED

## Purpose
The actual sector geometry render submission function. Native x86 code residing
in the .kallis memory section. Called repeatedly by the VM — each call processes
one visible sector.

## Calling Convention
```c
__usercall SectorRenderSubmit(ebx, ebp, esi, int stack_arg1, int stack_arg2)
// ebx, ebp, esi = register arguments (set by VM caller)
// stack_arg1, stack_arg2 = 2 stack arguments
// Returns after FrustumSetup_VM call
// NOT called in monocle mode
```

## Key Logic
```c
for (int i = 0; i < dword_E416C8; i++) {
    if (dword_E416CC[i * 8] == 0)  // byte 0 = visibility flag
        continue;                    // NOT visible → skip render

    int side = dword_E416CC[i * 8 + 1];   // byte 1 = direction/side
    int render_id = sector_data[i] + 0x80; // get render object ID

    // Set up frustum for this sector
    FrustumSetup_VM(frustum_data);

    // Get render object and submit
    ecx = world_data[0x78][render_id * 4];
    call sub_5814A0;  // SectorSubmitLoop — adds 6 sub-objects to render queue
}
```

## Visibility Check (the rendering gate)
```asm
1C93FB4  cmp byte ptr [eax+edi*8], 0   ; check visibility flag
1C93FB8  jz  loc_1C940BA               ; skip if not visible
```
- `eax` = dword_E416CC (visibility array base)
- `edi` = sector index (0 to dword_E416C8 - 1)
- 8 bytes per sector entry: byte[0]=visible, byte[1]=side, dword[4]=frustum_id

## Render Queue Submission
Calls `sub_5814A0` (SectorSubmitLoop) which loops 6 times calling `sub_562D30`
to insert render data into the queue at `dword_ECC2B0` (7752 bytes per slot).

## D3D Draw Chain
```
sub_1C93F90 (this function)
  → sub_5814A0 (SectorSubmitLoop, 6 iterations)
    → sub_562D30 (add to render queue at dword_ECC2B0)
      → sub_562760 (render dispatch, iterates queue)
        → sub_559B30 (mesh group iterator)
          → sub_55B160 (mesh renderer)
            → sub_4EA2E0 (SetStreamSource + SetIndices)
              → IDirect3DDevice9::DrawIndexedPrimitive (vtable+328 on dword_E36E8C)
```

## Global Data
| Address | Name | Purpose |
|---------|------|---------|
| `dword_E416C4` | World/Sector System | Main sector system object |
| `dword_E416C8` | Sector Count | Loop bound (14 indoors) |
| `dword_E416CC` | Visibility Array | 8 bytes/sector: visible, side, frustum_id |
| `dword_ECC2B0` | Render Queue | 7752 bytes/slot, up to 62 slots |
| `dword_E36E8C` | D3D Device | IDirect3DDevice9 pointer |

## Patch Points
**v17 attempt (failed):** NOP the jz at 0x1C93FB8 (6 bytes -> 90x6).
Did not work because unvisited sectors lack frustum data (dword[4] in visibility entry).

**v18 approach:** Hook call to PostTraversal_VM at 0x4889DF -> force ALL visibility
flags to 1 before this function runs. PostTraversal_VM then calls this function
which sees all sectors as visible.

## Call Chain
```
SectorVisibilityUpdate2 (0x488970)
  → PostTraversal_VM (0x5564D0) [VM thunk]
    → sub_1C93F90 [native x86 in .kallis section]
      → sub_5814A0 (SectorSubmitLoop)
```

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full system overview
- [sub_5564D0_PostTraversal_VM.md](sub_5564D0_PostTraversal_VM.md) — caller (VM thunk)
- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) — sets visibility flags
