# sub_5814A0 — SectorSubmitLoop
**Address:** Spiderwick.exe+1814A0 (absolute: 0x5814A0)
**Size:** 34 bytes (0x22)
**Status:** IDENTIFIED

## Purpose
Per-sector render submission loop. Takes a render object pointer in ECX (thiscall),
loops 6 times reading sub-object pointers from the object, and calls sub_562D30
(AddToRenderQueue) for each one. This adds 6 sub-objects per visible sector to the
render queue.

Called from sub_1C93F90 (SectorRenderSubmit) once for each sector that passes the
visibility check. The 6 sub-objects likely correspond to the 6 faces or geometry
groups of a sector's renderable data.

## Assembly
```asm
5814A0: xor edi, edi              ; i = 0
5814A2: loop_start:
5814A2:   mov eax, [ecx+edi*4]    ; eax = render_obj->sub_objects[i]
5814A5:   push 0                  ; arg4 = 0
5814A7:   push 0                  ; arg3 = 0
5814A9:   push 0                  ; arg2 = 0
5814AB:   push eax                ; arg1 = sub_object ptr
5814AC:   call sub_562D30          ; AddToRenderQueue(sub_obj, 0, 0, 0)
5814B1:   inc edi                 ; i++
5814B2:   cmp edi, 6              ; i < 6?
5814B5:   jl  loop_start          ; loop
5814B7:   retn                    ; return
```

## Key Logic
```c
// thiscall: ecx = render object
void SectorSubmitLoop(RenderObject *this) {
    for (int i = 0; i < 6; i++) {
        AddToRenderQueue(this->sub_objects[i], 0, 0, 0);
    }
}
```

## Call Chain
```
SectorRenderSubmit (0x1C93F90)
  → SectorSubmitLoop (0x5814A0) ← this function, 6 iterations per sector
    → AddToRenderQueue (0x562D30) ← inserts into dword_ECC2B0
```

## Global Data
| Address | Name | Purpose |
|---------|------|---------|
| `dword_ECC2B0` | Render Queue | Written by sub_562D30 (7752 bytes/slot, up to 62 slots) |

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full portal/render system overview
- [sub_1C93F90_SectorRenderSubmit.md](sub_1C93F90_SectorRenderSubmit.md) — caller (iterates visible sectors)
- [sub_562D30_AddToRenderQueue.md](sub_562D30_AddToRenderQueue.md) — callee (queue insertion)
- [sub_562760_RenderDispatch.md](sub_562760_RenderDispatch.md) — processes the queue after submission
