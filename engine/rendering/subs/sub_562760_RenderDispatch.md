# sub_562760 — RenderDispatch
**Address:** Spiderwick.exe+162760 (absolute: 0x562760)
**Status:** FULLY REVERSED

## Purpose
The render dispatch function. Called during the cleanup phase of CameraSectorUpdate
(sub_488DD0) after all visible sectors have been submitted to the render queue.
Processes the render queue and dispatches D3D draw calls for each entry.

## Full Decompiled Behavior
Processes the render queue located at `dword_ECC2B4` (note: ECC2B4, not ECC2B0).
Each queue entry is **7752 bytes**. The total number of entries is stored in
`dword_F80090`. Runs **2 passes** controlled by `dword_F80094` looping from 0 to 1.

```c
void RenderDispatch() {
    SetupViewport();

    for (int pass = 0; pass < 2; pass++) {
        dword_F80094 = pass;               // current pass index (0 or 1)

        int count = dword_F80090;           // number of render queue entries
        char *queue = (char *)dword_ECC2B4; // render queue base

        for (int i = 0; i < count; i++) {
            char *entry = queue + i * 7752; // 7752 bytes per entry
            ProcessRenderEntry(entry);       // mesh iteration → D3D draws
        }
    }
}
```

## Render Queue Layout
| Field | Value |
|-------|-------|
| Queue base | `dword_ECC2B4` |
| Entry size | 7752 bytes (0x1E48) |
| Entry count | `dword_F80090` |
| Pass count | 2 (dword_F80094 loops 0-1) |

## D3D Draw Chain
```
RenderDispatch (0x562760) ← this function
  → SetupViewport
  → [2 passes, dword_F80094 = 0..1]
    → MeshGroupIterator (0x559B30) — switch on mesh type
      → MeshRenderer (0x55B160) — per-mesh draw setup
        → D3DSetBuffers (0x4EA2E0) — SetStreamSource (vtable+400) + SetIndices (vtable+416)
          → IDirect3DDevice9::DrawIndexedPrimitive (vtable+328 on dword_E36E8C)
```

## Full Render Pipeline Context
```
CameraSectorUpdate (0x488DD0)
  ├─ Pass 1: UpdateVisibility → PerformRoomCulling
  ├─ Pass 2: SectorVisibilityUpdate2 → PortalTraversal → PostTraversal
  │   └─ SectorRenderSubmit (0x1C93F90) — populates render queue
  │       └─ SectorSubmitLoop (0x5814A0) → AddToRenderQueue (0x562D30)
  └─ Pass 3: RenderDispatch (0x562760) ← this function — flushes render queue
```

## Global Data
| Address | Name | Purpose |
|---------|------|---------|
| `dword_ECC2B4` | Render Queue Base | 7752 bytes/entry |
| `dword_F80090` | Render Queue Count | Number of entries to process |
| `dword_F80094` | Pass Index | Current pass (0 or 1) |
| `dword_E36E8C` | D3D Device | IDirect3DDevice9 pointer for draw calls |

## Sub-Functions
| Address | Name | Purpose |
|---------|------|---------|
| `0x559B30` | MeshGroupIterator | Iterates mesh groups within a queue entry, switch on type |
| `0x55B160` | MeshRenderer | Sets up and issues draw call for a single mesh |
| `0x4EA2E0` | D3DSetBuffers | Calls SetStreamSource (vtable+400) + SetIndices (vtable+416) |

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full portal/render system overview
- [sub_562D30_AddToRenderQueue.md](sub_562D30_AddToRenderQueue.md) — populates the queue this function reads
- [sub_5814A0_SectorSubmitLoop.md](sub_5814A0_SectorSubmitLoop.md) — per-sector submission (6 sub-objects)
- [sub_1C93F90_SectorRenderSubmit.md](sub_1C93F90_SectorRenderSubmit.md) — visibility loop that feeds the queue
- [sub_488DD0_CameraSectorUpdate.md](sub_488DD0_CameraSectorUpdate.md) — pipeline 1 entry (calls this in cleanup)
