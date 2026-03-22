# NM40 Character Render Pipeline

Reversed from IDA Pro static analysis of the Spiderwick Chronicles engine ("Ogre" by Stormfront Studios).

## Overview

NM40 (skinned mesh) character rendering uses a **two-pass alpha-test compositing** system. ALL sub-batches are rendered TWICE — once with the main diffuse texture, once with the detail texture. Alpha channels in each texture control which body regions are visible per pass.

This is NOT per-submesh texture selection. The submesh descriptor system controls bone visibility bitmasks for material grouping, not texture assignment.

## Render Chain

```
RENDERFUNC_NOAM_DIFFUSE (0x571000)
  ├─ Visibility check (RENDERFUNC_NOAM_VISIBILITY)
  ├─ If desc[13] texA exists (enemies):
  │     └─ RENDERFUNC_NOAM_SKIN (0x5709C0) — uses desc[13]/desc[14]
  └─ If desc[13] is NULL (player characters):
        └─ Kallis_RenderNoamSkin (thunk 0x56FB50 → [0x1C86CF4])
              └─ Uses desc[2]/desc[4] via Kallis runtime
```

## RENDERFUNC_NOAM_SKIN (0x5709C0) — Two-Pass Renderer

### Pass 1: Main Diffuse (desc[2] for Kallis path, desc[13] for standard path)

```c
texA = *(*(actor + 108) + 4);  // actor[27] = desc[13] at offset 108
D3D_BindTexture(texA);
// Set alpha ref = float_value * 128.0
// Iterate ALL mesh table entries → ALL sub-batches:
//   Upload bone matrices to shader constants (c25+)
//   SetVertexIndexBuffers(PCRD render data)
//   DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, vtxCount, 0, idxCount/3)
```

### Pass 2: Detail Texture (desc[4] for Kallis path, desc[14] for standard path)

```c
texB = *(*(actor + 112) + 4);  // actor[28] = desc[14] at offset 112
D3D_BindTexture(texB);
// Lower alpha ref by 15 (clamped to 0)
// Iterate ALL mesh table entries → ALL sub-batches AGAIN:
//   Same bone matrix upload + DrawIndexedPrimitive
```

### D3D9 Render States

```
D3DRS_ALPHATESTENABLE = TRUE
D3DRS_ALPHAREF        = float_value * 128 (pass 1), reduced by 15 (pass 2)
D3DRS_ALPHABLENDENABLE = FALSE  (no alpha blend, pure alpha test)
D3DRS_SRCBLEND        = D3DBLEND_SRCALPHA
D3DRS_DESTBLEND       = D3DBLEND_INVSRCALPHA
D3DRS_ZENABLE         = D3DZB_TRUE
D3DRS_ZWRITEENABLE    = TRUE
D3DRS_ZFUNC           = D3DCMP_LESSEQUAL  (allows pass 2 to overwrite pass 1)
D3DRS_CULLMODE        = D3DCULL_CW
D3DRS_STENCILENABLE   = TRUE (for shadow detection)
```

## Character Descriptor Slots (ClNoamActor_Init 0x527EC0)

22-DWORD descriptor array copied from character template:

```
desc[0]  = ANIZ animation archive          → actor[299]
desc[1]  = NM40 mesh (bones + geometry)    → actor[300]
desc[2]  = MAIN DIFFUSE PCIM (1024x1024)   → actor[301]
desc[3]  = SECONDARY PCIM (256x256 goblins, NULL players) → actor[302]
desc[4]  = DETAIL/NORMAL PCIM (512x512 players, NULL goblins) → actor[303]
desc[5-7]= additional assets (usually NULL) → actor[304-306]
desc[8]  = unknown asset                   → actor[26]
desc[9-12] = unknown                       → actor[427-430]
desc[13] = texA (64x64 secondary for enemies, NULL players) → actor[27]
desc[14] = texB (tiny secondary)           → actor[28]
desc[15-21] = LOD/animation assets         → actor[438-444]
```

### Player Characters vs Enemies

| Character | Bones | desc[2] | desc[4] | desc[13] | Render Path |
|-----------|-------|---------|---------|----------|-------------|
| Helen | 47 | 1024x1024 | 512x512 | NULL | Kallis thunk |
| Jared | 53 | 1024x1024 | 512x512 | NULL | Kallis thunk |
| Simon | 51 | 1024x1024 | 512x512 | NULL | Kallis thunk |
| Mallory | 56 | 1024x1024 | 512x512 | NULL | Kallis thunk |
| GoblinB | 48 | 1024x1024 | NULL | 64x64 | NOAM_SKIN standard |
| GoblinMedium | 48 | 1024x1024 | NULL | 128x128 | NOAM_SKIN standard |
| MrTibbs | 31 | ? | ? | NULL | Kallis thunk |

## Submesh Descriptor System

### NM40_GetSubmeshCount (0x53A8D0)

```c
// Returns *(meshHeaderExt + 64)
int NM40_GetSubmeshCount(NM40* this) {
    void* ext = this + this[3] + *(this + this[3] + 20);
    return ext ? *(int*)(ext + 64) : 0;
}
```

### NM40_GetSubmeshDesc (0x53A8F0)

```c
// Returns pointer to 16-byte descriptor at index
// Located at: meshHeaderExt + *(meshHeaderExt + 68) + 16 * index
int NM40_GetSubmeshDesc(NM40* this, int index) {
    void* ext = this + this[3] + *(this + this[3] + 20);
    if (!ext || !*(int*)(ext + 68)) return 0;
    return ext + *(int*)(ext + 68) + 16 * index;
}
```

### Submesh Descriptor Format (16 bytes)

The descriptor is a **128-bit bitmask** (4 DWORDs). Each bit represents one mesh table entry. If bit N is set, mesh table entry N belongs to this submesh (material group).

Used by `Material_Init` (0x586FA0):
```c
// Check if LOD index 'i' belongs to this submesh:
bool belongs = (1 << (i & 0x1F)) & desc[i >> 5];
```

### MaterialBuilder_Init (0x5861A0)

Creates **2 materials per submesh** (front face + back face):
1. Calls `NM40_GetSubmeshCount` → allocate arrays
2. For each submesh: copy 16-byte descriptor, create front `Material_Init`, create back `Material_Init`
3. Optionally creates shadow and env-map materials
4. Calls `RenderBatch_Setup` to connect materials to render groups

### Material_Init (0x586FA0)

132-byte material object. Reads bone visibility bitmask from submesh descriptor. Creates a **bone index map** (visible bone index → global bone index). Does NOT reference textures directly — texture binding happens in the render pipeline.

## nameHash Mapping (Captured from MnAttack.zwd via IDA live debugger + D3D hooks)

```
Helen:   NM40=0x6625FB28 → diffuse=0x4C8CF6BF detail=0x38F4BBE7
Jared:   NM40=0x23AE159C → diffuse=0x9589F0E7 detail=0xE735E5D7
Mallory: NM40=0x2B5B0254 → diffuse=0x687335CB detail=0x0E55B3AF
Simon:   NM40=0xFB33692C → diffuse=0x3D829FDB detail=0x7702420F
```

## Key Engine Functions

| Address | Name | Purpose |
|---------|------|---------|
| 0x571000 | RENDERFUNC_NOAM_DIFFUSE | Main NM40 render entry, dispatches to Kallis or NOAM_SKIN |
| 0x5709C0 | RENDERFUNC_NOAM_SKIN | Two-pass skinned mesh renderer (TEX_A + TEX_B) |
| 0x56FB50 | Kallis_RenderNoamSkin | JMP thunk to Kallis runtime for player characters |
| 0x527EC0 | ClNoamActor_Init | Copies 22-DWORD descriptor to actor fields |
| 0x5861A0 | MaterialBuilder_Init | Creates per-submesh materials with bone bitmasks |
| 0x586FA0 | Material_Init | Material constructor, bone index mapping |
| 0x587DB0 | Material_GetRenderParams | Returns render function pointers per submesh |
| 0x53A8D0 | NM40_GetSubmeshCount | Submesh count from mesh header extension |
| 0x53A8F0 | NM40_GetSubmeshDesc | 16-byte submesh descriptor by index |
| 0x589760 | SkinRenderer_Init | Creates 2 texture slots + render batch array |
