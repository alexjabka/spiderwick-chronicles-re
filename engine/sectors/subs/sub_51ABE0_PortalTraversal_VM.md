# sub_51ABE0 — PortalTraversal_VM

**Address:** Spiderwick.exe+0x11ABE0 (absolute: 0x0051ABE0)
**Size:** 6 bytes (Kallis VM thunk)
**Status:** IDENTIFIED (VM bytecode — cannot decompile)

## Purpose

Entry point for recursive portal traversal. The VM bytecode calls native sub_51A130 (PortalTraversal_Native) which performs the actual portal walking and sector visibility determination.

The actual room culling fix is NOT here — it is inside sub_51A130 (native code called FROM this VM function).

## Assembly (complete — 6 bytes)

```asm
sub_51ABE0:
  push    offset off_1C8A4E4     ; VM bytecode descriptor
  call    sub_13FA690            ; Kallis VM interpreter
  dd      offset 0x1CEBC5E       ; inline bytecode reference
```

**Thunk chain:** off_1C8A4E4 -> VM entry at 0x1CFDAE0 -> bytecode at 0x1CEBC5E

## Prototype

```c
int __cdecl PortalTraversal_VM(int sector, int camera, int zero);
```

| Arg    | Type | Meaning                                 |
|--------|------|-----------------------------------------|
| sector | int  | Starting sector for traversal           |
| camera | int  | Camera object pointer                   |
| zero   | int  | Always 0 at observed call sites         |

## Call Sites

| Caller                    | Address    | Context                                        |
|---------------------------|------------|------------------------------------------------|
| SectorVisibilityUpdate2   | 0x4889CC   | Main visibility pipeline                       |
| PortalTraversalPath2      | 0x48CC2D   | Alternate traversal path 2                     |
| PortalTraversalPath3      | 0x4905DB   | Alternate traversal path 3                     |

All three callers follow the same pattern: SetSectorState_VM -> FrustumSetup_VM -> PortalTraversal_VM -> PostTraversal_VM.

## Related

- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) — native code called FROM this VM function; where the actual portal walk happens
- [sub_562650_FrustumSetup_VM.md](sub_562650_FrustumSetup_VM.md) — frustum setup, called before this
- [sub_516900_SetSectorState_VM.md](sub_516900_SetSectorState_VM.md) — sector state prep, called before this
- [sub_5564D0_PostTraversal_VM.md](sub_5564D0_PostTraversal_VM.md) — finalizes results, called after this
