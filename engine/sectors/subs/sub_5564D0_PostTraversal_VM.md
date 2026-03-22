# sub_5564D0 — PostTraversal_VM

**Address:** Spiderwick.exe+0x1564D0 (absolute: 0x005564D0)
**Size:** 6 bytes (Kallis VM thunk)
**Status:** IDENTIFIED (VM bytecode — cannot decompile)

## Purpose

Finalizes portal traversal results. Called right after PortalTraversal_VM in all three traversal pipelines to commit or post-process the visibility data computed during the portal walk.

## Assembly (complete — 6 bytes)

```asm
sub_5564D0:
  push    offset off_1C88DBC     ; VM bytecode descriptor
  call    sub_13FA690            ; Kallis VM interpreter
  dd      offset 0x1CFB630       ; inline bytecode reference
```

**Thunk chain:** off_1C88DBC -> VM entry at 0x1CFB630

## Prototype

```c
int __thiscall PostTraversal_VM(void *this);
```

| Arg  | Type   | Meaning                                           |
|------|--------|---------------------------------------------------|
| this | void * | Pointer to &off_7147C0 (traversal state object)   |

**Note:** Unlike the other three VM thunks in this pipeline which are __cdecl, this one uses __thiscall with `this = &off_7147C0`.

## Call Sites

Called after portal traversal in all 3 pipelines:

| Caller                    | Context                                        |
|---------------------------|------------------------------------------------|
| SectorVisibilityUpdate2   | Main visibility pipeline                       |
| PortalTraversalPath2      | Alternate traversal path 2                     |
| PortalTraversalPath3      | Alternate traversal path 3                     |

## Related

- [sub_516900_SetSectorState_VM.md](sub_516900_SetSectorState_VM.md) — sector state prep, first in the pipeline
- [sub_562650_FrustumSetup_VM.md](sub_562650_FrustumSetup_VM.md) — frustum setup, second in the pipeline
- [sub_51ABE0_PortalTraversal_VM.md](sub_51ABE0_PortalTraversal_VM.md) — portal traversal entry, third in the pipeline
- [sub_51A130_PortalTraversal_Native.md](sub_51A130_PortalTraversal_Native.md) — native portal walker called from the VM traversal
