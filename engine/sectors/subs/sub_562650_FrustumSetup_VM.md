# sub_562650 — FrustumSetup_VM

**Address:** Spiderwick.exe+0x162650 (absolute: 0x00562650)
**Size:** 6 bytes (Kallis VM thunk)
**Status:** IDENTIFIED (VM bytecode — cannot decompile)

## Purpose

Computes frustum planes from camera FOV, direction, and position. Called BEFORE portal traversal to set up the viewing frustum that determines which portals and sectors are potentially visible.

v1 of the freecam attempted to hook this function to override FOV — partially worked but not clean enough.

## Assembly (complete — 6 bytes)

```asm
sub_562650:
  push    offset off_1C8DF4C     ; VM bytecode descriptor
  call    sub_13FA690            ; Kallis VM interpreter
  dd      offset 0x1CEF146       ; inline bytecode reference
```

**Thunk chain:** off_1C8DF4C -> VM entry at 0x1CF7AE0 -> bytecode at 0x1CEF146

## Prototype

```c
int __cdecl FrustumSetup_VM(int camera, int mode);
```

| Arg   | Type | Meaning                          |
|-------|------|----------------------------------|
| camera | int  | Camera object pointer            |
| mode   | int  | Frustum computation mode/flags   |

## Call Sites

| Caller                    | Address    | Context                                      |
|---------------------------|------------|----------------------------------------------|
| SectorVisibilityUpdate2   | 0x488989   | Called before portal traversal in main path   |
| PortalTraversalPath2      | 0x48CBF6   | Called before portal traversal in path 2      |
| PortalTraversalPath3      | 0x49059E   | Called before portal traversal in path 3      |
| PortalTraversal_Native    | 0x51A1D5   | Internal call during recursive portal walking |

## Related

- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) — calls this internally at 0x51A1D5
- [sub_51ABE0_PortalTraversal_VM.md](sub_51ABE0_PortalTraversal_VM.md) — portal traversal entry, runs after frustum is set up
- [sub_516900_SetSectorState_VM.md](sub_516900_SetSectorState_VM.md) — sector state prep, called alongside this in all pipelines
- [sub_5564D0_PostTraversal_VM.md](sub_5564D0_PostTraversal_VM.md) — finalizes traversal results after portal walk
