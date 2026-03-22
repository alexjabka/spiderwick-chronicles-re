# sub_516900 — SetSectorState_VM

**Address:** Spiderwick.exe+0x116900 (absolute: 0x00516900)
**Size:** 6 bytes (Kallis VM thunk)
**Status:** IDENTIFIED (VM bytecode — cannot decompile)

## Purpose

Prepares a sector for portal traversal. Called right before PortalTraversal_VM in all three traversal pipelines to initialize sector state prior to the recursive portal walk.

## Assembly (complete — 6 bytes)

```asm
sub_516900:
  push    offset off_1C8CEE0     ; VM bytecode descriptor
  call    sub_13FA690            ; Kallis VM interpreter
  dd      offset 0x20A5000       ; inline bytecode reference
```

**Thunk chain:** off_1C8CEE0 -> VM entry at 0x20A5000

## Prototype

```c
int __cdecl SetSectorState_VM(int sector_index);
```

| Arg          | Type | Meaning                                    |
|--------------|------|--------------------------------------------|
| sector_index | int  | Index of the sector to prepare for traversal |

## Call Sites

Called before portal traversal in all 3 pipelines:

| Caller                    | Context                                        |
|---------------------------|------------------------------------------------|
| SectorVisibilityUpdate2   | Main visibility pipeline                       |
| PortalTraversalPath2      | Alternate traversal path 2                     |
| PortalTraversalPath3      | Alternate traversal path 3                     |

## Related

- [sub_562650_FrustumSetup_VM.md](sub_562650_FrustumSetup_VM.md) — frustum setup, called after sector state is prepared
- [sub_51ABE0_PortalTraversal_VM.md](sub_51ABE0_PortalTraversal_VM.md) — portal traversal entry, runs after this
- [sub_5564D0_PostTraversal_VM.md](sub_5564D0_PostTraversal_VM.md) — finalizes traversal results
- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) — native portal walker called from the VM traversal
