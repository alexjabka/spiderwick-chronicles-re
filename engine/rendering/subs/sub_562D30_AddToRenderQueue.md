# sub_562D30 — AddToRenderQueue
**Address:** Spiderwick.exe+162D30 (absolute: 0x562D30)
**Status:** FULLY REVERSED

## Purpose
Adds render items to the global render queue. Each queue slot is 7752 bytes.
Called from sub_5814A0 (SectorSubmitLoop) 6 times per visible sector — once
per sub-object. Switches on render type to handle different item categories.

## Signature
```c
void __cdecl AddToRenderQueue(void *render_data, int render_type, int param3, int param4);
// render_data = pointer to sub-object from sector render object
// render_type = type selector (cases 1, 3, 4)
// param3, param4 = additional parameters (often 0 from SectorSubmitLoop)
```

## Queue Structure
| Field | Value |
|-------|-------|
| Queue base | `dword_ECC2B0` |
| Slot size | 7752 bytes (0x1E48) |
| Max slots | 62 |
| Total size | ~469 KB |

## Render Type Switch
The function contains a switch on `render_type`:
- **Case 1:** Standard sector geometry
- **Case 3:** Transparent/alpha geometry
- **Case 4:** Special render objects (effects, overlays)

Each case writes different data into the 7752-byte queue slot at `dword_ECC2B0`.

## Call Chain
```
SectorRenderSubmit (0x1C93F90)
  → SectorSubmitLoop (0x5814A0) — loops 6 times
    → AddToRenderQueue (0x562D30) ← this function
      ↓ (queue populated)
RenderDispatch (0x562760) — iterates queue, issues D3D draws
  → MeshGroupIterator (0x559B30)
    → MeshRenderer (0x55B160)
      → D3DSetBuffers (0x4EA2E0) — SetStreamSource + SetIndices
        → IDirect3DDevice9::DrawIndexedPrimitive
```

## Global Data
| Address | Name | Purpose |
|---------|------|---------|
| `dword_ECC2B0` | Render Queue | 7752 bytes/slot, up to 62 slots |
| `dword_E36E8C` | D3D Device | IDirect3DDevice9 pointer (used downstream) |

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full portal/render system overview
- [sub_5814A0_SectorSubmitLoop.md](sub_5814A0_SectorSubmitLoop.md) — caller (6 iterations per sector)
- [sub_1C93F90_SectorRenderSubmit.md](sub_1C93F90_SectorRenderSubmit.md) — top-level caller (visibility loop)
- [sub_562760_RenderDispatch.md](sub_562760_RenderDispatch.md) — reads the queue and dispatches draws
