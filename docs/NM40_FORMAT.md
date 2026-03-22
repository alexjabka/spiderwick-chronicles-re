# NM40 Binary Format — Skinned Mesh / Character Model

Engine: "Ogre" by Stormfront Studios (The Spiderwick Chronicles)

## Overview

NM40 is the skinned mesh format used for characters, creatures, and animated props.
Each NM40 file contains:
- Skeleton reference (bone count, remap tables)
- Multiple LOD levels, each with its own embedded PCRD (geometry) chunk
- Submesh/material batch table
- Vertex data (position, normal, UV, bone weights/indices)
- Index data (triangle strips)

File layout: `[NM40 header + metadata + PCRD headers] [idx LOD0] [vtx LOD0] [idx LOD1] [vtx LOD1] ...`

Within an AWAD archive, the NM40 entry includes header + metadata + embedded PCRD + vertex/index buffers as one contiguous block. The AWAD TOC provides the correct total size.

---

## NM40 Header (+0x00 to +0x3F, 64 bytes)

All offset fields (+0x28, +0x30, +0x34, +0x38, +0x3C) are **NM40-relative** and get the base address added by the pointer fixup function at 0x20BC000 (NM40_PointerFixup).

| Offset | Size | Type       | Field               | Description |
|--------|------|------------|----------------------|-------------|
| +0x00  | 4    | char[4]    | magic                | `"NM40"` (0x4E4D3430) |
| +0x04  | 2    | uint16     | version              | 1 or 2. Version 2 adds multi-LOD and skeleton data. |
| +0x06  | 2    | uint16     | fixupFlag            | 0=needs fixup, set to 10 after NM40_PointerFixup runs. |
| +0x08  | 2    | uint16     | numBones             | Total bone/node count in the hierarchy. |
| +0x0A  | 2    | uint16     | numSkelBones         | Number of weighted skeleton bones (ver2 only; 0 for ver1). |
| +0x0C  | 4    | float      | scaleX               | Mesh scale factor X (usually 1.0). |
| +0x10  | 4    | float      | scaleY               | Mesh scale factor Y. |
| +0x14  | 4    | float      | scaleZ               | Mesh scale factor Z. |
| +0x18  | 4    | float      | scaleW               | Fourth scale component (morph/animation scale?). |
| +0x1C  | 4    | float      | lodDistance           | LOD switch distance (e.g., 3.0, 15.0). |
| +0x20  | 1    | uint8      | flags                | Flags (0x01=ver1 basic, 0x41=ver2 standard, 0x43=ver2 extended). |
| +0x21  | 1    | uint8      | reserved21           | Always 0. |
| +0x22  | 2    | uint16     | numLODsAlt           | Alternative LOD count field (usually 1). |
| +0x24  | 2    | uint16     | numMeshTableEntries  | Number of mesh table entries (confirmed by fixup loop at 0x20BC000). |
| +0x26  | 2    | uint16     | numSubmeshes         | Number of submesh/material batch entries. |
| +0x28  | 4    | uint32     | idxDataOffset        | Offset to index buffer data. **Fixup'd** (base address added at runtime). |
| +0x2C  | 4    | uint32     | idxBufSize           | Total index buffer size in bytes. NOT fixup'd. |
| +0x30  | 4    | uint32     | vtxDeclOffset        | Offset to vertex declaration data (always 0x40). **Fixup'd**. |
| +0x34  | 4    | uint32     | meshTableOffset      | Offset to mesh LOD table. **Fixup'd**. |
| +0x38  | 4    | uint32     | bonePaletteOffset    | Offset to bone palette structure. 0 for ver1. **Fixup'd** (if non-zero). |
| +0x3C  | 4    | uint32     | submeshTableOffset   | Offset to submesh/material batch table. **Fixup'd**. |

---

## Vertex Declaration (+0x40, 64 bytes)

| Offset | Size | Type   | Field              | Description |
|--------|------|--------|--------------------|-------------|
| +0x40  | 2    | uint16 | numVtxElements     | Number of vertex elements (always 3: pos, nrm, uv+blend). |
| +0x42  | 2    | uint16 | numVtxElements2    | Duplicate of above. |
| +0x44  | 4    | uint32 | reserved           | 0. |
| +0x48  | 2    | uint16 | numStreams          | D3D stream count (1 for ver1, 2 for ver2). |
| +0x4A  | 2    | uint16 | reserved4A         | 0. |
| +0x4C  | 2    | uint16 | field4C            | Always 0x50 (80). Purpose unclear; NOT the vertex stride. |
| +0x4E  | 2    | uint16 | reserved4E         | 0. |
| +0x50  | 48   | bytes  | vtxDeclEntries     | Packed vertex element descriptors (sparse, mostly zero). |

Vertex elements observed in the vtxDecl area:
- `+0x6A: 0x2020` — packed descriptor entry
- `+0x70: 0x2010` — packed descriptor entry

---

## Submesh Table (at submeshTableOffset)

Array of `numSubmeshes` entries, each 4 bytes:

| Offset | Size | Type   | Field      | Description |
|--------|------|--------|------------|-------------|
| +0x00  | 2    | uint16 | firstIndex | First index in the triangle strip for this batch. |
| +0x02  | 2    | uint16 | indexCount | Number of indices in this batch. |

The submesh table divides the index buffer into material/texture batches.
Total size = `numSubmeshes * 4` bytes.

---

## Mesh LOD Table (at meshTableOffset)

### LOD Table Header (8 bytes)

| Offset | Size | Type   | Field            | Description |
|--------|------|--------|------------------|-------------|
| +0x00  | 2    | uint16 | numMorphTargets  | Number of morph/blend shape targets (0 in all observed files). |
| +0x02  | 2    | uint16 | numLODs          | Number of LOD levels (1-3 typically). |
| +0x04  | 4    | uint32 | firstLODOffset   | Absolute file offset to the first LOD entry. |

### Per-LOD Entry (16 bytes each, at firstLODOffset + i*16)

| Offset | Size | Type   | Field          | Description |
|--------|------|--------|----------------|-------------|
| +0x00  | 4    | uint32 | boneIdxSize    | Bone index element size (always 4 = sizeof DWORD). |
| +0x04  | 4    | uint32 | numRemapBones  | Number of bones in this LOD's bone palette/remap. |
| +0x08  | 4    | uint32 | boneRemapOff   | Absolute file offset to bone remap byte array. |
| +0x0C  | 4    | uint32 | pcrdOffset     | Absolute file offset to this LOD's embedded PCRD header. |

### Bone Remap Array (at boneRemapOff, numRemapBones bytes)

A byte array mapping LOD-local bone indices to the global skeleton bone index.
Example: `[1, 2, 3, 4, 6, 8, 9, ...]` means LOD-local bone 0 = global bone 1, etc.
Vertex blend indices reference LOD-local bones, which must be remapped via this table.

Padded to 2-byte alignment after the array.

---

## Embedded PCRD (Per-LOD Render Data)

Each LOD has one PCRD chunk embedded in the NM40 file. The PCRD header is 0x34 (52) bytes:

| Offset | Size | Type   | Field          | Description |
|--------|------|--------|----------------|-------------|
| +0x00  | 4    | char[4]| magic          | `"PCRD"` |
| +0x04  | 4    | uint32 | version        | Always 2. |
| +0x08  | 4    | uint32 | hdrSize        | Header size (0x34 for NM40 embedded PCRDs). |
| +0x0C  | 4    | uint32 | indexCount     | Number of triangle strip indices. |
| +0x10  | 4    | uint32 | vertexCount    | Number of vertices. |
| +0x14  | 4    | uint32 | indexDataOff   | Absolute file offset to index data (uint16 triangle strip). |
| +0x18  | 4    | uint32 | vertexDataOff  | Absolute file offset to vertex data. |
| +0x1C  | 20   | bytes  | reserved       | Zeros. |
| +0x30  | var  | bytes  | bonePalette    | Bone palette indices (same data as boneRemap array). |

**Key difference from standalone PCRD (in PCWB):**
- Standalone PCRD vertex stride = 24 bytes (pos + color + UV).
- NM40 embedded PCRD vertex stride = **52 bytes** (pos + normal + UV + bone indices + bone weights).

---

## Vertex Format (52 bytes per vertex)

| Offset | Size | Type        | Semantic        | Description |
|--------|------|-------------|-----------------|-------------|
| +0x00  | 12   | float3      | POSITION        | XYZ position in bind pose. |
| +0x0C  | 12   | float3      | NORMAL          | Unit normal vector (length = 1.0). |
| +0x18  | 8    | float2      | TEXCOORD        | UV texture coordinates. |
| +0x20  | 4    | D3DCOLOR    | BLENDINDICES    | 4 bone indices as bytes (BGRA order). Use `D3DCOLORtoUBYTE4()` to decode. Indices are LOD-local; remap via bone palette. |
| +0x24  | 16   | float4      | BLENDWEIGHT     | 4 bone weights (sum = 1.0). Typically only 1-2 are non-zero. |

Total stride: **52 bytes**.

### Vertex Shader Confirmation

The engine embeds HLSL vertex shader source code in `.rdata`. The skinned mesh shader declares:

```hlsl
float4x3 boneMatrix[22] : register(c25);
struct VS_INPUT {
    float3 mPosition    : POSITION;
    float3 mNormal      : NORMAL;
    float2 mTexCoord    : TEXCOORD;
    float4 mBlendIndices: BLENDINDICES;   // D3DCOLOR → 4 byte indices
    float4 mBlendWeight : BLENDWEIGHT;    // 4 float weights
    // Optional morph targets: float3 mPosition1..mPosition11 : POSITION1..POSITION11
};
```

Morph target variants add extra `float3` position streams per vertex, increasing the stride.
No morph targets were observed in the analyzed files (numMorphTargets = 0), so stride stays at 52.

---

## Index Format

- **Type:** uint16 triangle strips
- **Count:** `indexCount` from PCRD header
- Convert to triangle list: for each i in [0, count-2], emit triangle (i, i+1, i+2) with winding flip on odd indices.
- Degenerate triangles (where any two indices are equal) must be skipped.

---

## Data Layout Within File

The NM40 file is divided into two logical blocks:

1. **Vertex buffer block** (first `vtxBufTotalSize` bytes): Contains the NM40 header, vertex declarations, submesh table, mesh LOD table, bone remap arrays, embedded PCRD headers, and (at the end) actual vertex data for the first LOD.

2. **Index buffer block** (next `idxBufTotalSize` bytes): Contains index data.

For multi-LOD files, the data interleaves per-LOD:
```
[NM40 header area with all PCRD headers]
[LOD0 index data] [LOD0 vertex data] [padding]
[LOD1 index data] [LOD1 vertex data] [padding]
[LOD2 index data] [LOD2 vertex data] [padding]
```

The PCRD `indexDataOff` and `vertexDataOff` fields are **absolute file offsets** into this combined data.

---

## Bone Hierarchy

The bone hierarchy (parent indices, bone names, bind-pose transforms) is **NOT** stored in the NM40 file.
It resides in a separate skeleton resource referenced by the engine's asset system.

The NM40 file only stores:
- `numBones`: total nodes in the skeleton
- `numSkelBones`: number of weighted bones
- Per-LOD bone remap tables (LOD-local index -> global skeleton bone index)
- Bone palette copies in PCRD headers at +0x30

The engine code at `sub_55D44A` classifies bone nodes as: **ROOT** (no parent), **BONE** (has bone index), **SKELETON** (root of skeleton tree), or **XFORM** (transform-only node).

---

## Engine Code References

| Address    | Function | Purpose |
|------------|----------|---------|
| 0x20BC000  | NM40_PointerFixup | Converts NM40-relative offsets to absolute pointers (+0x28,+0x30,+0x34,+0x3C, mesh table chain). |
| 0x56D970   | NM40_Register | Registers "Mesh" asset type (typeHash=0x0000BB12) with factory. |
| 0x56D940   | NM40_Factory | Creates 8-byte NM40 asset object {vtable, data_ptr}. |
| 0x56D860   | NM40_Load | Validates magic bytes 'N','M','4','0'. Calls NM40_PointerFixup. |
| 0x527EC0   | ClNoamActor_Init | Character init: stores 22-slot descriptor, creates ModelInstance + NoamFigure + SkinRenderer. |
| 0x523A00   | ModelInstance_Init | Creates 304-byte model instance from NM40: mesh data, bone matrices, MaterialBuilder. |
| 0x56E970   | NoamFigure_Init | Creates 1192-byte render figure with texture refs (TEX_A at +92, TEX_B at +96). |
| 0x5861A0   | MaterialBuilder_Init | Creates per-submesh Material objects (132 bytes each), builds render batches. |
| 0x589760   | SkinRenderer_Init | Creates 172-byte skin material renderer with material group/batch tables. |
| 0x571000   | RENDERFUNC_NOAM_DIFFUSE | Character diffuse render: 2-pass with TEX_A and TEX_B. |
| 0x458170   | sauCharacterInit | VM native: pops model/skin names, hashes them, calls character init. |
| 0x55D44A   | BoneNode_GetType | Returns "ROOT"/"BONE"/"SKELETON"/"XFORM" for a bone hierarchy node. |
| 0x53A640   | NM40_GetLODCount | Returns LOD count from mesh header. |
| 0x53A8D0   | NM40_GetSubmeshCount | Returns submesh count from mesh header +64. |
| 0x53A8F0   | NM40_GetSubmeshDesc | Returns 16-byte submesh descriptor by index. |

### Embedded Vertex Shaders

8 skinned mesh shader variants are embedded in .rdata starting at 0x6492E0:
- With/without morph targets (up to 11 morph positions)
- With/without specular lighting
- With/without fog

All variants use `boneMatrix[22]` (max 22 bones per draw call) at register c25.
