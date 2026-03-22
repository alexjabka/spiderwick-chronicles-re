# Skeleton & Skinning Format (aniz / hier / skel / NM40)

**Status:** Partially reversed (hierarchy, skinning bones, vertex format)

---

## Overview

The Spiderwick Chronicles uses a multi-section skeleton system embedded within
aniz (compressed animation) assets. The skeleton data is split across several
sections that together define the bone hierarchy, GPU skinning matrices, and
vertex-to-bone binding.

**Pipeline:** aniz header -> hier (hierarchy) -> skel (GPU skinning) -> animation index

Vertex data in NM40 meshes references skel bones via packed indices. The hier
section provides the full hierarchy tree with parent relationships and rest-pose
transforms.

---

## aniz File Structure

The aniz asset begins with a 0x40-byte header, followed by sequentially laid-out
sections identified by 4-byte magic strings:

```
Offset   Size   Description
------   ----   -----------
+0x00    0x40   aniz header (animation metadata)
+0x40    ...    hier section (bone hierarchy)
  ...    ...    skel section (GPU skinning bones)
  ...    ...    animation index / keyframe data
```

---

## hier Section (Bone Hierarchy)

Located at aniz + 0x40. Defines the full skeleton hierarchy with rest-pose transforms.

### Header

```
Offset       Size   Description
------       ----   -----------
+0x00        4      Magic: "hier"
+0x04        4      bone_count (u32) -- varies per character: 2-82
```

### Per-Bone Data (192 bytes each)

Each bone stores three 4x4 float matrices (3 x 64 bytes = 192 bytes):

```
Offset within bone   Size   Description
-------------------   ----   -----------
+0x00                 64     Local transform (float4x4) -- bone-local space
+0x40                 64     World transform (float4x4) -- rest-pose world space
+0x80                 64     Inverse bind matrix (float4x4) -- world -> bone space
```

All matrices are 4x4 floats (16 floats x 4 bytes = 64 bytes each) in row-major order.

### Parent Indices

Parent indices are stored as a uint8 array in sec1 (a secondary data section),
one byte per bone. A value of 0xFF (255) typically indicates a root bone with
no parent.

### Known Bone Counts

| Character / Asset       | hier Bones |
|------------------------|-----------|
| Simple props            | 2         |
| Basic characters        | ~30-40    |
| Complex characters      | up to 82  |

The hier bone count varies by character complexity. Every asset regardless
of hier bone count uses a fixed 39-bone skel section for GPU skinning.

---

## skel Section (GPU Skinning Bones)

Follows the hier section. Defines the subset of bones used for GPU vertex
skinning, always exactly 39 bones.

### Header

```
Offset   Size   Description
------   ----   -----------
+0x00    4      Magic: "skel"
+0x04    ...    (metadata)
+0x20    48     Root transform (float4x3)
```

### Bone Matrices

The skel section stores **39 bones** as float4x3 matrices (3 rows x 4 columns =
48 bytes per matrix). These are the skinning matrices uploaded to the GPU as
shader constants.

```
Per bone: 48 bytes = float4x3 (3 rows of float4)
Total: 39 x 48 = 1872 bytes of matrix data
```

The root transform at offset +0x20 defines the skeleton's base world placement.

### Connection to hier

The 39 skel bones are a GPU-friendly subset/remapping of the full hier skeleton.
The hier section provides the complete hierarchy (parent chains, rest poses),
while skel provides the compact matrix palette for vertex shader skinning.

---

## Vertex Shader Skinning

The game's vertex shader confirms the skinning setup:

### Shader Constants

```hlsl
float4x3 boneMatrix[22] : register(c25);   // 22 bone matrices starting at c25
```

Note: The shader declares 22 matrices in registers, though skel stores 39 bones.
The engine likely batches draw calls to stay within the register limit, remapping
bone indices per batch.

### Bone Index Decoding

```hlsl
int4 indices = D3DCOLORtoUBYTE4(v.boneIndices);
// D3DCOLORtoUBYTE4 swizzles BGRA -> RGBA order for correct index extraction
```

Bone indices are packed as a D3DCOLOR (4 unsigned bytes in BGRA order). The
`D3DCOLORtoUBYTE4` intrinsic swizzles them to the correct order for array indexing.

### Morph Targets

The vertex shader supports up to **11 morph targets** (blend shapes), used for
facial animation and deformation.

---

## NM40 Skinned Vertex Format

NM40 mesh assets use a vertex stride of **52 bytes** for skinned geometry:

```
Offset   Size   Type          Description
------   ----   ----          -----------
+0x00    12     float3        Position (x, y, z)
+0x0C    12     float3        Normal (nx, ny, nz)
+0x18    8      float2        UV texture coordinates (u, v)
+0x20    4      ubyte4        Bone indices (D3DCOLOR packed, 4 bone refs)
+0x24    16     float4        Bone weights (w0, w1, w2, w3)
```

**Total stride: 52 bytes (0x34)**

### Bone Index Reference

The ubyte4 bone indices at +0x20 reference **skel** bones (the 39-bone GPU palette),
NOT hier bones directly. The engine maps hier hierarchy bones to skel GPU bones
during asset loading.

Weights at +0x24 are normalized (sum to 1.0). Up to 4 bones can influence each
vertex (standard GPU skinning limit).

---

## Data Flow Summary

```
┌─────────────────────────────────┐
│          aniz asset             │
│  ┌───────────┐  ┌───────────┐  │
│  │   hier    │  │   skel    │  │
│  │ N bones   │  │ 39 bones  │  │
│  │ float4x4  │  │ float4x3  │  │
│  │ ×3 each   │  │           │  │
│  │ + parents │  │           │  │
│  └─────┬─────┘  └─────┬─────┘  │
│        │              │         │
│        │   mapping     │         │
│        └──────┬───────┘         │
└───────────────┼─────────────────┘
                │
                ▼
┌─────────────────────────────────┐
│         NM40 mesh               │
│  vertex.boneIndices → skel[i]   │
│  vertex.boneWeights → blend     │
│  stride = 52 bytes              │
└─────────────────────────────────┘
                │
                ▼
┌─────────────────────────────────┐
│       Vertex Shader             │
│  boneMatrix[22] : c25           │
│  D3DCOLORtoUBYTE4 unpack       │
│  up to 11 morph targets         │
└─────────────────────────────────┘
```

---

## Related

- [ZWD_FORMAT.md](ZWD_FORMAT.md) -- aniz/NM40 assets stored in ZWD archives
- [WORLD_GEOMETRY.md](WORLD_GEOMETRY.md) -- World (static) geometry uses different vertex format
- [PCW_FORMAT.md](PCW_FORMAT.md) -- World files containing PCRD render data
