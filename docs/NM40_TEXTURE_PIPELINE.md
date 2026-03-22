# NM40 Texture Pipeline & Character Rendering System

Engine: "Ogre" by Stormfront Studios (The Spiderwick Chronicles, 2008)
Base address: 0x00400000 (no ASLR, 32-bit x86)

This document is the definitive engineering reference for the NM40 (skinned mesh) texture
binding pipeline, covering the full chain from asset loading through character creation
to render-time texture application.

---

## Table of Contents

1. [Asset Type Registry (Complete)](#1-asset-type-registry-complete)
2. [NM40 File Format (Complete)](#2-nm40-file-format-complete)
3. [Character Descriptor Array](#3-character-descriptor-array)
4. [Character Creation Pipeline](#4-character-creation-pipeline)
5. [Texture Binding at Render Time](#5-texture-binding-at-render-time)
6. [Prop (PCPB) Texture System](#6-prop-pcpb-texture-system)
7. [AWAD Loading Pipeline](#7-awad-loading-pipeline)
8. [Accessor Functions (NM40)](#8-accessor-functions-nm40)
9. [Render Function Registry](#9-render-function-registry)
10. [Implementation Notes for SpiderView](#10-implementation-notes-for-spiderview)

---

## 1. Asset Type Registry (Complete)

All 22 asset types registered via `ClAssetTypeMap_Register` (0x52A340).
The global type map lives at `unk_E56010`. Type hashes are computed by `HashString(type_name)`
(0x405380) at registration time.

The registration function pattern is identical for all types:
```c
void Register(this) {
    int hash = HashString("TypeName");
    ClAssetTypeMap_Register(hash, factory_function);
}
```

### Complete Type Table

| # | Type Name | typeHash | Register Function | Factory Function | vtable Address | Magic Bytes |
|---|-----------|----------|-------------------|------------------|----------------|-------------|
| 1 | `Mesh` | 0x0000BB12 | 0x56D970 | 0x56D940 | 0x63EA80 (`ClModelAsset`) | `NM40` |
| 2 | `Prop` | 0x00020752 | 0x55B9E0 | 0x55B9B0 | 0x641D34 | `PCPB` (version 4) |
| 3 | `Script` | 0x0006D8A6 | 0x431C50 | 0x431C20 | -- | `SCT\0` |
| 4 | `Font` | 0x009A2807 | 0x542750 | 0x542720 | -- | `WF` |
| 5 | `World` | 0x00FA6F60 | -- (separate path) | -- | -- | `PCWB` (v10) |
| 6 | `Image` | 0x01F1096F | 0x56DD20 | 0x56DCF0 | `ClTextureAssetPc` | `PCIM` |
| 7 | `Database` | 0x04339C43 | 0x4318D0 | `ClDatabaseAsset_factory` | -- | `DBDB` |
| 8 | `Playlist` | 0x06572A64 | 0x56B190 | 0x56B160 | -- | `play` (text) |
| 9 | `NavMesh` | 0x194738A4 | 0x53A3E0 | 0x53A3B0 | -- | `NAVM` |
| 10 | `AssetMap` | 0x2A7E6F30 | 0x542540 | 0x542510 | -- | `AMAP` |
| 11 | `LoadedCinematic` | 0x3072B942 | 0x556070 | 0x556040 | -- | `brxb` |
| 12 | `Animation` | 0x44FE8920 | 0x56DA50 | 0x56DA20 | -- | `adat` |
| 13 | `BehaviorStateMachine` | 0x5550C44A | 0x4317E0 (`ClBehaviorStateAsset_Register`) | 0x4317A0 | -- | `a10000a1` |
| 14 | `Strings` | 0x690BE5E8 | 0x542640 | 0x542610 | -- | `STRI` |
| 15 | `Table` | 0x000FD514 | 0x55BF80 | 0x55BF50 | -- | -- |
| 16 | `Subtitle` | -- | 0x542900 (`ClSubtitlesAsset_Register`) | `ClSubtitlesAsset_Factory` | -- | `STTL` |
| 17 | `ControlMap` | 0xB8D1C6C6 | 0x542270 | 0x542240 | -- | `Devi`/`Char` (text) |
| 18 | `ActionStateMachine` | 0xBCBFB478 | 0x4316A0 (`ClActionStateAsset_Register`) | 0x431660 | -- | `AC0000AC` |
| 19 | `AudioEffects` | 0xE2F8E66E | 0x5A7040 | 0x5A7010 | -- | `EPC` |
| 20 | `Particles` | 0xF9C2A901 | 0x4319D0 | 0x4319A0 | -- | `arpc` |
| 21 | `AnimationList` | 0xFA5E717C | 0x55C890 | 0x55C860 | -- | `aniz` |
| 22 | `SaveGameIcon` | 0x451A228E | 0x431AC0 | 0x431A90 | -- | -- |
| 23 | `StreamedCinematic` | 0xEAB838D4 | 0x555B00 | 0x555AD0 | -- | -- |

### Registration Mechanism

`ClAssetTypeMap_Register` (0x52A340) is a thin wrapper:
```c
void ClAssetTypeMap_Register(int typeHash, int factoryFunc) {
    if (!(dword_E56114 & 1)) {
        dword_E56114 |= 1;
        sub_58B720(&unk_E56010);   // init the type map
    }
    sub_58B740(typeHash, factoryFunc);  // insert into map
}
```

### Lookup Mechanism

`ClAssetTypeMap_Lookup` (0x58B770) performs a linear scan of the type map:
```c
int ClAssetTypeMap_Lookup(this, int typeHash) {
    for (int i = 0; i < this->count; i++) {
        if (this->entries[i].hash == typeHash)
            return this->entries[i].factoryFunc;
    }
    return 0;
}
```

The map at `unk_E56010` is structured as: `{ count, [hash0, factory0, hash1, factory1, ...] }`.

### Factory Function Pattern

All factory functions follow the same pattern:
```c
void* Factory(int allocator, int assetData) {
    void* obj = allocator->vtable[1](allocator, 8, 16);  // allocate 8 bytes, 16-aligned
    if (obj) {
        obj[0] = &TypeVtable;   // set vtable
        obj[1] = assetData;     // store raw asset data pointer
    }
    return obj;
}
```

The asset object is just 8 bytes: `{vtable_ptr, data_ptr}`. The vtable's validation
method (e.g., NM40_Validate at vtable[2]) checks magic bytes and triggers parsing.

---

## 2. NM40 File Format (Complete)

### Header Layout (0x00 - 0x3F, 64 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| +0x00 | 4 | char[4] | `magic` | `"NM40"` (0x4E4D3430) |
| +0x04 | 4 | uint32 | `version` | 1 or 2. Version 2 adds multi-LOD + skeleton data. |
| +0x06 | 2 | uint16 | `fixupFlag` | 0 = needs fixup, 10 = already fixed up. Written by fixup function. |
| +0x08 | 2 | uint16 | `numBones` | Total bone/node count in the hierarchy. |
| +0x0A | 2 | uint16 | `numSkelBones` | Number of weighted skeleton bones (ver2 only; 0 for ver1). |
| +0x0C | 4 | float | `scaleX` | Mesh scale factor X (usually 1.0). |
| +0x10 | 4 | float | `scaleY` | Mesh scale factor Y. |
| +0x14 | 4 | float | `scaleZ` | Mesh scale factor Z. |
| +0x18 | 4 | float | `scaleW` | Fourth scale component (morph/animation scale). |
| +0x1C | 4 | float | `lodDistance` | LOD switch distance (e.g., 3.0, 15.0). |
| +0x20 | 1 | uint8 | `flags` | 0x01=ver1 basic, 0x41=ver2 standard, 0x43=ver2 extended. |
| +0x21 | 1 | uint8 | `reserved21` | Always 0. |
| +0x22 | 2 | uint16 | `numLODsAlt` | Alternative LOD count field (usually 1). |
| +0x24 | 2 | uint16 | `numMeshTableEntries` | Number of mesh table entries (used by fixup loop). |
| +0x26 | 2 | uint16 | `numSubmeshes` | Number of submesh/material batch entries. |
| +0x28 | 4 | uint32 | `vtxBufTotalSize` | Total vertex buffer block size (page-aligned). **Fixup: += base** |
| +0x2C | 4 | uint32 | `idxBufTotalSize` | Total index buffer block size (page-aligned). |
| +0x30 | 4 | uint32 | `vtxDeclOffset` | Offset to vertex declaration (always 0x40). **Fixup: += base** |
| +0x34 | 4 | uint32 | `meshTableOffset` | Offset to mesh LOD table. **Fixup: += base** |
| +0x38 | 4 | uint32 | `lastLODBonePalette` | Offset to last LOD's bone palette in PCRD header (+0x30). 0 for ver1. |
| +0x3C | 4 | uint32 | `submeshTableOffset` | Offset to submesh/material batch table. **Fixup: += base** |

### Fixup Function (0x20BC000)

`sub_20BC000(eax=unused, ecx=nm40_base)` -- `.kallis` native code (ROP-style).

This function converts relative offsets in the NM40 header to absolute pointers by adding
the NM40 base address. It runs once when the asset is first accessed.

**Guard:** `*(nm40 + 6) != 0` -- if fixupFlag is already non-zero, returns immediately.

**Phase 1 -- Header offsets:**
```
*(nm40 + 48) += nm40      // +0x30: vtxDeclOffset
```
If `*(nm40 + 34)` (numLODsAlt at +0x22) is non-zero, takes a shortened early-return path.

**Phase 2 -- Remaining header offsets:**
```
*(nm40 + 60) += nm40      // +0x3C: submeshTableOffset
*(nm40 + 40) += nm40      // +0x28: vtxBufTotalSize (becomes absolute ptr)
*(nm40 + 52) += nm40      // +0x34: meshTableOffset
```

**Phase 3 -- Mesh table chain:**

For each of `*(nm40 + 36)` (numMeshTableEntries at +0x24) entries:
```
meshTable = *(nm40 + 52)
For i in 0..numMeshTableEntries:
    entry = meshTable + 8*i
    *(entry + 4) += nm40                    // sub-entry pointer
    For j in 0..*(entry + 2):               // sub-entry count at entry+2 (uint16)
        subEntry = *(entry + 4) + 16*j
        *(subEntry + 8) += nm40             // render data pointer A
        *(subEntry + 12) += nm40            // render data pointer B
        boneCount = *(subEntry + 6)         // uint16
        if (boneCount + 1 > 0):
            renderData = *(subEntry + 12)
            *(renderData + 24) += nm40      // vertex data offset
            *(renderData + 20) += nm40      // index data offset
            return  // early return after first valid sub-entry!
```

**Phase 4 -- Optional extra data at +0x38:**
```
if *(nm40 + 56):                            // +0x38: lastLODBonePalette
    *(nm40 + 56) = nm40 + *(nm40 + 56)     // convert to absolute
    extraData = *(nm40 + 56)
    *(extraData + 8) += nm40
    *(extraData + 12) += nm40
    *(extraData + 16) += nm40
```

**Finalize:** `*(nm40 + 6) = 10` -- mark as fixed up.

### Mesh Table Structure (8-byte entries)

Located at `meshTableOffset` (after fixup). `numMeshTableEntries` entries, each 8 bytes:

| Offset | Size | Type | Field |
|--------|------|------|-------|
| +0x00 | 2 | uint16 | entry count (for this mesh group) |
| +0x02 | 2 | uint16 | sub-batch count |
| +0x04 | 4 | ptr | pointer to sub-batch array (absolute after fixup) |

### Sub-Batch Structure (16-byte entries)

Each mesh table entry points to an array of 16-byte sub-batches:

| Offset | Size | Type | Field |
|--------|------|------|-------|
| +0x00 | 2 | uint16 | mesh group index |
| +0x02 | 2 | uint16 | sub-batch index within group |
| +0x04 | 2 | uint16 | bone palette count |
| +0x06 | 2 | uint16 | flags / bone count |
| +0x08 | 4 | ptr | render data pointer A (absolute after fixup) |
| +0x0C | 4 | ptr | render data pointer B / PCRD pointer (absolute after fixup) |

The render data structure pointed to by +0x0C contains:
- +0x14 (20): index data offset (absolute after fixup)
- +0x18 (24): vertex data offset (absolute after fixup)

### Bone Palette Structure

Located at PCRD header +0x30. A byte array mapping LOD-local bone indices to global
skeleton bone indices. The palette is referenced by the bone remap entries in the LOD table.

Example: `[1, 2, 3, 4, 6, 8, 9, ...]` -- LOD-local bone 0 = global bone 1.

### Submesh Table (at submeshTableOffset)

Array of `numSubmeshes` entries, each 4 bytes:

| Offset | Size | Type | Field |
|--------|------|------|-------|
| +0x00 | 2 | uint16 | firstIndex |
| +0x02 | 2 | uint16 | indexCount |

Divides the index buffer into material/texture batches within the triangle strip.

### Embedded PCRD Location and Header

Each LOD has one PCRD chunk embedded within the NM40 file. The PCRD header is 0x34 (52) bytes:

| Offset | Size | Type | Field |
|--------|------|------|-------|
| +0x00 | 4 | char[4] | `magic` = `"PCRD"` |
| +0x04 | 4 | uint32 | `version` = 2 |
| +0x08 | 4 | uint32 | `hdrSize` = 0x34 |
| +0x0C | 4 | uint32 | `indexCount` |
| +0x10 | 4 | uint32 | `vertexCount` |
| +0x14 | 4 | uint32 | `indexDataOff` (absolute file offset) |
| +0x18 | 4 | uint32 | `vertexDataOff` (absolute file offset) |
| +0x1C | 20 | bytes | reserved (zeros) |
| +0x30 | var | bytes | bone palette |

**Key difference from standalone PCRD (in PCWB):**
- Standalone PCRD vertex stride = 24 bytes (pos + color + UV).
- NM40 embedded PCRD vertex stride = **52 bytes** (pos + normal + UV + bone indices + bone weights).

### Vertex Format (52 bytes per vertex)

| Offset | Size | Type | Semantic | Description |
|--------|------|------|----------|-------------|
| +0x00 | 12 | float3 | POSITION | XYZ position in bind pose. |
| +0x0C | 12 | float3 | NORMAL | Unit normal vector. |
| +0x18 | 8 | float2 | TEXCOORD | UV texture coordinates. |
| +0x20 | 4 | D3DCOLOR | BLENDINDICES | 4 bone indices as bytes (BGRA order). Use `D3DCOLORtoUBYTE4()`. |
| +0x24 | 16 | float4 | BLENDWEIGHT | 4 bone weights (sum = 1.0). |

Total stride: **52 bytes**.

### Vertex Shader Confirmation

8 skinned mesh shader variants are embedded in `.rdata` starting at 0x6492E0:
```hlsl
float4x3 boneMatrix[22] : register(c25);
struct VS_INPUT {
    float3 mPosition    : POSITION;
    float3 mNormal      : NORMAL;
    float2 mTexCoord    : TEXCOORD;
    float4 mBlendIndices: BLENDINDICES;   // D3DCOLOR -> 4 byte indices
    float4 mBlendWeight : BLENDWEIGHT;    // 4 float weights
    // Optional: float3 mPosition1..mPosition11 : POSITION1..POSITION11
};
```

Max 22 bones per draw call, max 11 morph targets. Variants include with/without morph
targets, specular, and fog.

### Index Format

- uint16 triangle strips
- Convert to triangle list: for each i in [0, count-2], emit triangle (i, i+1, i+2)
  with winding flip on odd indices
- Skip degenerate triangles (any two indices equal)

---

## 3. Character Descriptor Array

The character descriptor is a variable-length array of DWORD slots passed to
`ClNoamActor_Init` (0x527EC0) as the `a2` parameter. It is populated by the
`.kallis` VM during `sauCharacterInit` execution. Each slot is a 4-byte value
(pointer, hash, or integer) that the init function copies into the ClNoamActor
object at specific offsets.

### Descriptor Slot Table

The `a2` parameter is an array of DWORDs. `sub_527EC0` reads slots a2[0] through
a2[21] and stores them into the ClNoamActor object (`this`).

| Slot | a2 Index | Stored At | this Offset | Purpose | Asset Type |
|------|----------|-----------|-------------|---------|------------|
| [0] | a2[0] | this[299] | +0x4AC | Primary data ref (skeleton/mesh base) | NM40/aniz |
| [1] | a2[1] | this[300] | +0x4B0 | NM40 mesh data pointer (LOD 0) | NM40 |
| [2] | a2[2] | this[301] | +0x4B4 | NM40 mesh data pointer (validated) | NM40 |
| [3] | a2[3] | this[302] | +0x4B8 | NM40 mesh data pointer | NM40 |
| [4] | a2[4] | this[303] | +0x4BC | NM40 mesh data pointer | NM40 |
| [5] | a2[5] | this[304] | +0x4C0 | NM40 mesh data pointer | NM40 |
| [6] | a2[6] | this[305] | +0x4C4 | NM40 mesh data pointer | NM40 |
| [7] | a2[7] | this[306] | +0x4C8 | NM40 mesh data pointer | NM40 |
| [8] | a2[8] | this[26] | +0x68 | Animation reference | aniz |
| [9] | a2[9] | this[427] | +0x6AC | Scale/param 0 | float |
| [10] | a2[10] | this[428] | +0x6B0 | Scale/param 1 | float |
| [11] | a2[11] | this[429] | +0x6B4 | Scale/param 2 | float |
| [12] | a2[12] | this[430] | +0x6B8 | Scale/param 3 | float |
| [13] | a2[13] | this[27] | +0x6C | **TEX_A: Texture hash A (PCIM)** | PCIM hash |
| [14] | a2[14] | this[28] | +0x70 | **TEX_B: Texture hash B (PCIM)** | PCIM hash |
| [15] | a2[15] | this[438] | +0x6D8 | Per-LOD skin asset (LOD 0) | NM40 |
| [16] | a2[16] | this[439] | +0x6DC | Per-LOD skin asset (LOD 1) | NM40 |
| [17] | a2[17] | this[440] | +0x6E0 | Per-LOD skin asset (LOD 2) | NM40 |
| [18] | a2[18] | this[441] | +0x6E4 | Per-LOD skin asset (LOD 3) | NM40 |
| [19] | a2[19] | this[442] | +0x6E8 | Per-LOD skin asset (LOD 4) | NM40 |
| [20] | a2[20] | this[443] | +0x6EC | Per-LOD skin asset (LOD 5) | NM40 |
| [21] | a2[21] | this[444] | +0x6F0 | Per-LOD skin asset (LOD 6) | NM40 |

### Texture Slots: a2[13] and a2[14]

These are the **critical** slots for texture resolution. They contain PCIM asset
name hashes, NOT direct pointers to texture data. The engine uses these hashes to
look up texture data from loaded WAD archives.

`sub_527EC0` stores them at `this+0x6C` (this[27]) and `this+0x70` (this[28]).

Later, `NoamFigure_Init` (0x56E970) reads them back via accessor functions:
```c
this[23] = sub_5278B0(a2)   // returns a2[27] = TEX_A
this[24] = sub_5278C0(a2)   // returns a2[28] = TEX_B
```

This places TEX_A at `NoamFigure + 92` (offset +0x5C) and TEX_B at `NoamFigure + 96`
(offset +0x60).

### LOD Skin Count

After copying slots [15]-[21], the code determines the active LOD count:
```c
this[445] = 0;              // clear last slot
this[437] = 8;              // max LOD count
for (int i = 0; i < this[437]; i++) {
    if (!this[438 + i])     // if LOD slot is NULL
        this[437] = i;      // truncate LOD count
}
```

This means NULL-terminated: the first NULL in slots [15]-[21] ends the LOD list.

---

## 4. Character Creation Pipeline

### Overview

```
sauCharacterInit (VM native, 0x458170)
    |
    +-- Pops 3 strings from VM stack
    |   - arg3 = character template name
    |   - arg2 = TEX_A name (diffuse texture)
    |   - arg1 = TEX_B name (alternate/specular texture)
    |
    +-- HashString(arg2) -> TEX_A hash
    +-- HashString(arg1) -> TEX_B hash
    |
    +-- vtable[18](this, 0, TEX_B_hash, TEX_A_hash, arg3)
        |
        +-- ClNoamActor_Init (0x527EC0)
            |
            +-- Copy descriptor array to object fields
            +-- ModelInstance_Init (0x523A00)
            +-- NoamFigure_Init (0x56E970)
            +-- MaterialBuilder_Init (0x5861A0)
            +-- SkinRenderer_Init (0x589760)
```

### sauCharacterInit (0x458170)

VM native handler. Pops 3 string arguments from the Kallis VM stack and hashes two
of them (the texture names) to produce PCIM lookup keys.

```c
int __thiscall sauCharacterInit(void *this) {
    char* texB_name;  VMPopString(&texB_name);   // TEX_B name
    char* texA_name;  VMPopString(&texA_name);   // TEX_A name
    int   template;   VMPopString(&template);    // character template

    int texA_hash = HashString(texA_name);       // inline HashString
    int texB_hash = HashString(texB_name);       // inline HashString

    // Call vtable[18] on the character object
    return this->vtable[18](this, 0, texB_hash, texA_hash, template);
}
```

The vtable[18] call dispatches through `.kallis` ROP code which eventually populates
the descriptor array and calls `ClNoamActor_Init`.

### ClNoamActor_Init (0x527EC0)

`int __thiscall ClNoamActor_Init(this, descriptorArray, a3, a4)`

The main initialization function for character models. Steps:

1. **Copy descriptor slots** [0]-[21] into object fields (see Section 3).
2. **Initialize skeleton helper** via `sub_526C70`.
3. **Determine active LOD count** from slots [15]-[21] (NULL-terminated).
4. **Create ModelInstance** (sub_523A00) -- skeleton, bone matrices, material builder.
5. **Create NoamFigure** (sub_56E970) -- rendering instance with texture refs.
6. **Optionally create SkinRenderer** (sub_589760) -- if model has renderable submeshes.
7. **Create additional LOD figures** for each LOD level beyond the first.
8. **Initialize render state** and call `sub_526F70`.

Key logic:
```c
if (this[299] && this[300] && this[301]) {
    // Valid mesh data in first 3 slots
    numBones_a3 = sub_56D8E0(0);    // get bone count param A
    numBones_a4 = sub_56D8E0(1);    // get bone count param B

    // Allocate 304 bytes, 16-aligned -> ModelInstance
    this[446] = sub_523A00(this[299], numBones_a3, numBones_a4);

    // Allocate 1192 bytes, 16-aligned -> NoamFigure (primary)
    this[448] = sub_56E970(this, this[300], ...);
    this[460] = this[448];  // duplicate reference

    // Check if model has renderable submeshes
    if (sub_56D910(this[300])) {
        // Allocate 172 bytes, 16-aligned -> SkinRenderer
        this[447] = sub_589760(this[299], this[300]);
    }

    // Link SkinRenderer to NoamFigure
    *(this[460] + 12) = this[447];
}

// Create additional LOD figures (slots [15]-[21])
for (int lod = 1; lod < this[457]; lod++) {
    this[449 + lod-1] = sub_56E970(this, this[438 + lod-1], ...);
}
```

### ModelInstance_Init (0x523A00)

`int __thiscall ModelInstance_Init(this, skeletonRef, boneCountA, boneCountB)`

Initializes the skeleton and animation system for a character:

1. **Resolve skeleton** via `sub_55C830(skeletonRef)`.
2. **Get bone hierarchy** via `sub_53A640`.
3. **Set bone counts** at `this+60` and `this+62` (uint16).
4. **Read bone reference transforms** from the skeleton (3 floats each for two params).
5. **Allocate bone matrices** -- `numBones * 128` bytes (two 4x4 matrices per bone).
6. **Initialize morph targets** if present.
7. **Create MaterialBuilder** via `sub_5861A0(skeletonRef, this)` -> stored at `this+124`.
8. **Allocate world-space bone matrices** -- `(boneCount+1) * 64` bytes.
9. **Allocate skinning bone matrices** -- `(boneCountA + boneCountB) * 64` bytes.
10. **Initialize animation state** (blend weights, current animation, flags).

### NoamFigure_Init (0x56E970)

`void* __thiscall NoamFigure_Init(this, nm40Data, a3, a4)`

Creates the rendering instance for a single LOD level:

```c
void* NoamFigure_Init(this, nm40Data, a3, flag) {
    sub_55C460();                           // init base class
    this->vtable = &off_643100;             // NoamFigure vtable
    this->flags = flag;                     // at +64
    this->nm40Data = nm40Data;              // at +4
    this->pcrdHeader = sub_56D8C0(a3);      // at +60: embedded PCRD header ptr

    // Read accessor values from the ClNoamActor
    this[17] = sub_527840(nm40Data);        // nm40Data[301] = descriptor[2]
    this[18] = sub_527850(nm40Data);        // nm40Data[302] = descriptor[3]
    this[19] = sub_527860(nm40Data);        // nm40Data[303] = descriptor[4]
    this[20] = sub_527870(nm40Data);        // nm40Data[304] = descriptor[5]
    this[21] = sub_527880(nm40Data);        // nm40Data[305] = descriptor[6]
    this[22] = sub_527890(nm40Data);        // nm40Data[306] = descriptor[7]
    this[23] = sub_5278B0(nm40Data);        // nm40Data[27]  = TEX_A hash
    this[24] = sub_5278C0(nm40Data);        // nm40Data[28]  = TEX_B hash

    // Read PCRD header metrics
    pcrd = this->pcrdHeader;
    this->scaleX = pcrd[3];                 // float at PCRD+12
    this->scaleY = pcrd[4];                 // float at PCRD+16
    this->scaleZ = pcrd[5];                 // float at PCRD+20

    // Read LOD distance from PCRD header
    this->lodNear = *(pcrd + 24);           // float at PCRD+24
    this->lodFar  = *(pcrd + 28);           // float at PCRD+28

    // Initialize rendering state
    Matrix_Identity(flt_133C970);           // global temp matrix
    this->boundingSphere = -1.0 (x3);       // at +1148, +1152, +1156
    // ... additional state init ...
}
```

**TEX_A at NoamFigure+92 (this[23])** and **TEX_B at NoamFigure+96 (this[24])** are
the texture asset hashes that will be resolved at render time to actual D3D texture handles.

### MaterialBuilder_Init (0x5861A0)

`int* __thiscall MaterialBuilder_Init(this, skeletonRef, modelInstance)`

Builds the per-bone material lookup structures:

1. **Get bone hierarchy count** via `sub_53A640(skeletonRef)`.
2. **Count visible materials** via `sub_53A8D0(skeletonRef)` -> total material count.
3. **Allocate material info** array: `32 * materialCount` bytes.
4. **Copy material data** from skeleton for each material via `sub_53A8F0`.
5. **Allocate render batch array**: `68 * boneGroupCount` bytes.
6. **Allocate material object array**: `32 * materialCount` bytes.
7. **For each material**, create two `Material_Init` objects (front/back face or diffuse/specular):
   ```c
   for (int i = 0; i < materialCount; i++) {
       materialData = sub_53A8F0(i);
       this->materials[i*8 + 4] = Material_Init(skeletonRef, modelInstance, materialData);  // material A
       this->materials[i*8 + 5] = Material_Init(skeletonRef, modelInstance, materialData);  // material B
       // Optionally create material C (shadow/rim) if sub_5882E0 returns true
       // Optionally create material D (env map) if sub_5852B0 returns true
   }
   ```
8. **Setup render batches** via `RenderBatch_Setup` (sub_585F20).

### Material_Init (0x586FA0)

`float* __thiscall Material_Init(this, skeletonRef, modelInstance, materialData)`

Creates a single material object (132 bytes):

```c
float* Material_Init(this, skeletonRef, modelInstance, materialData) {
    this->vtable = &off_645B34;         // Material vtable
    this->skeleton = skeletonRef;       // at +56
    this->skeletonRef2 = skeletonRef;   // at +60
    this->modelInstance = modelInstance; // at +64
    this->materialData = materialData;  // at +24

    // Count visible bones for this material
    int visibleBones = 0;
    for (int bone = 0; bone < GetBoneCount(skeletonRef); bone++) {
        if ((1 << (bone & 0x1F)) & materialData[bone >> 5])
            visibleBones++;
    }
    this->visibleBoneCount = visibleBones;  // at +28

    // Allocate bone visibility map
    this->boneVisMap = alloc(GetBoneCount(skeletonRef));
    // Allocate bone render data: 16 bytes per visible bone
    this->boneRenderData = alloc(16 * visibleBones);

    // Build bone index map (visible bone index -> global bone index)
    for (int bone = 0; bone < GetBoneCount(skeletonRef); bone++) {
        if (bone is visible)
            boneVisMap[bone] = localIndex++;
        else
            boneVisMap[bone] = 0xFF;  // invisible
    }

    // Initialize blend/color state to defaults
    this->alpha = 1.0;     // at +20
    // ... more state init ...
}
```

### SkinRenderer_Init (0x589760)

`DWORD* __thiscall SkinRenderer_Init(this, skeletonRef, nm40Data)`

Creates the skin rendering dispatch structure (172 bytes):

1. **Get PCRD data** via `sub_56D8D0(nm40Data)`.
2. **Get skeleton reference** via `sub_55C830(skeletonRef)`.
3. **Initialize two texture slots** (loop count = 2):
   ```c
   for (int slot = 0; slot < 2; slot++) {
       // Initialize sampler states, blend factors, default textures
       this->slots[slot].scaleU = 1.0;
       this->slots[slot].scaleV = 1.0;
       this->slots[slot].offsetU = NAN;  // sentinel
       // ... 15 fields per slot, 60 bytes each ...
   }
   ```
4. **Count total sub-batches** across all mesh table entries:
   ```c
   int totalSubBatches = 0;
   for (int m = 0; m < numMeshTableEntries; m++) {
       boneCount = meshTable[m].subEntryArray->boneCount;
       if (boneCount) totalSubBatches += boneCount;
   }
   ```
5. **Allocate render batch array**: `16 * totalSubBatches` bytes.
6. **Initialize each render batch** with mesh group index, sub-batch index, and zero-initialized
   render state.

### RenderBatch_Setup (0x585F20)

`unsigned int __thiscall RenderBatch_Setup(this)`

Called after MaterialBuilder and all Materials are created. Iterates over all bone groups
and populates the render batch entries with actual rendering data (shader parameters,
texture bindings, vertex/index buffer references).

For each bone group:
1. Find which material owns this bone group.
2. Call the material's vtable[0] to get render parameters.
3. Store render state into the batch entry (16 bytes per bone group).

---

## 5. Texture Binding at Render Time

### RENDERFUNC_NOAM_DIFFUSE (sub_571000)

The main render callback for NM40 character meshes. Registered as the render function
for NoamFigure render batches.

```c
int sub_571000(DWORD* a1) {
    int renderBatch = a1[1];
    dword_133C944 = a1[5];              // NoamFigure pointer
    dword_133C940 = a1[4];              // skin data
    dword_133C948 = a1;                 // batch context

    int skinData = *(dword_133C944 + 4);
    Matrix_Copy(byte_133C9B8, *(skinData + 88) + 40);  // world matrix

    if (!sub_56E7D0(dword_133C944))     // visibility check
        return 1;

    // Check render path based on material properties
    float* matParams = sub_51B030(*(skinData + 88), 2);

    if (matParams[12] > 0.0) {
        // Path A: simple single-pass render
        sub_5709C0(0, *(skinData + 100));  // RENDERFUNC_NOAM_SKIN
        return 1;
    }

    if (matParams[14] * matParams[13] != 1.0) {
        // Path B: alpha-blended render (if flag set)
        if (byte_133C94F)
            sub_56FB50(dword_133C944, 0, *(skinData + 100));
        return 1;
    }

    // Path C: standard 2-pass render
    if (*(*(dword_133C944 + 60) + 32) & 0x20) == 0) {
        // No double-sided flag -> single pass
        sub_56FB50(0, *(skinData + 100));
        return 1;
    }

    // Double-sided: two passes
    if (byte_133C94F)
        sub_56FB50(0, *(skinData + 100));  // pass 1: front face
    else
        sub_56EC20(a1[4], renderBatch, 0, a1[5], *(skinData + 100));  // pass 2: back face
    return 1;
}
```

### RENDERFUNC_NOAM_SKIN (sub_5709C0)

The actual skinned mesh drawing function. Called from `sub_571000` and handles:

1. **Get bone matrices** via `sub_523230(a1[2])`.
2. **Set up render state** (fog, shading, sampler states).
3. **Bind texture** for the first pass:
   ```c
   // Get texture hash from skin data
   int texHash = *(a1[1] + 108) + 4;    // TEX_A from NoamFigure+92 indirectly
   sub_4E9970(texHash, ...);             // bind via D3D SetTexture
   ```
4. **Iterate mesh table entries** (numMeshTableEntries from NM40 header +36):
   ```c
   for (int meshGroup = 0; meshGroup < numMeshTableEntries; meshGroup++) {
       for (int subBatch = 0; subBatch < subBatchCount; subBatch++) {
           // Upload bone matrices to shader constants
           for (int bone = 0; bone < boneCount; bone++) {
               // Read 4x4 matrix from bone palette
               // Transpose to float4x3 for shader register c25+
               boneMatrixArray[bone] = transpose(boneMatrices[boneIndex]);
           }
           SetVertexShaderConstantF(25, boneMatrixArray, 3 * boneCount);

           // Set vertex format and stream source
           sub_4EA2E0(renderData);

           // DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, ...)
           device->DrawIndexedPrimitive(4, 0, 0, vertexCount, 0, indexCount/3);
       }
   }
   ```
5. **Second pass** with TEX_B:
   ```c
   // Rebind with second texture
   sub_4E9970(a1[1], ...);               // TEX_B from NoamFigure+96
   // Repeat mesh iteration...
   ```

### sub_4E9970 -- D3D SetTexture Call

The low-level texture binding function:

```c
void sub_4E9970(int texSlotIdx, ..., int texStage, int texObj, int texSlot) {
    if (byte_E32614) return;  // rendering disabled

    if (texObj) {
        int d3dTexture = *(texObj + 4 * texSlot + 28);  // D3D9 texture handle

        // IDirect3DDevice9::SetTexture(stage, texture)
        device->vtable[69](device, texStage, d3dTexture);

        // Set sampler states from texture object
        device->vtable[69](device, texStage, D3DSAMP_MIPMAPLODBIAS, *(byte*)(texObj + 188));
        device->vtable[69](device, texStage, D3DSAMP_MAGFILTER, *(byte*)(texObj + 189));
        device->vtable[69](device, texStage, D3DSAMP_MINFILTER, *(byte*)(texObj + 191));
        device->vtable[69](device, texStage, D3DSAMP_MIPFILTER, *(byte*)(texObj + 192));

        // Anisotropic filtering setup
        if (*(texObj + 164) == 1)
            maxAniso = 0;
        else {
            float lodBias = -*(float*)(texObj + 184);
            if (fabs(lodBias) >= 16.0) lodBias = 0.0;
            SetSamplerState(texStage, D3DSAMP_MIPMAPLODBIAS, lodBias);
            maxAniso = (byte_6EE790 != 0) + 1;
        }
        SetSamplerState(texStage, D3DSAMP_MAXANISOTROPY, maxAniso);
    } else {
        d3dTexture = 0;  // unbind
    }

    // Final SetTexture call
    device->vtable[65](device, texStage, d3dTexture);
}
```

### Texture Object Structure

| Offset | Size | Type | Field |
|--------|------|------|-------|
| +0x00 | 4 | ptr | vtable |
| +0x04 | -- | -- | ... |
| +0x1C | 4*N | ptr[] | D3D texture handles array (indexed by texSlot) |
| +0xA4 | 4 | int | texture slot index (texSlot) |
| +0xA8 | 4 | int | ... |
| +0xAC | 4 | int | active texture slot index (used by sub_59D000) |
| +0xBC | 1 | byte | mipmap LOD bias |
| +0xBD | 1 | byte | mag filter |
| +0xBF | 1 | byte | min filter |
| +0xC0 | 1 | byte | mip filter |
| +0xB8 | 4 | float | LOD bias (float) |

---

## 6. Prop (PCPB) Texture System

Props use a fundamentally different texture system from NM40 characters. Props have
their textures **embedded inline** in the PCPB file, whereas NM40 characters reference
textures by hash from external AWAD archives.

### PCPB Has Embedded PCIM Data

The PCPB file format (version 4) contains embedded PCIM texture data within the file
itself. Each prop mesh group has direct pointer references to its texture data.

### Prop_Init (0x55A850)

The prop initialization function performs fixup and texture creation:

```c
void Prop_Init(int propBase, int pcwbBase) {
    // Version check
    if (*(propBase + 4) != 4)
        printf("*** Invalid prop file version ***");

    if (*(propBase + 6) != 0)   // already initialized
        return;

    // Phase 1: Fix up texture pointer table at +48
    if (*(propBase + 48)) {
        *(propBase + 48) += propBase;   // texture pointer table
        for (int i = 0; i < numTexRefs; i++)
            texPtrTable[i] += propBase; // each texture ptr += base
    }

    // Phase 2: Fix up texture objects at +52
    if (*(propBase + 52)) {
        *(propBase + 52) += propBase;   // texture object array
        for (int i = 0; i < numTextures; i++) {
            texObjArray[i] += propBase;
            sub_4ED380(texObjArray[i], propBase, 5, 1);  // create D3D texture
        }
    }

    // Phase 3: Fix up mesh groups
    for (int g = 0; g < numMeshGroups; g++) {
        meshGroup = meshGroupTable[g];

        // Fix up mesh-level pointers
        if (meshGroup[26]) meshGroup[26] += propBase;   // optional data
        meshGroup[27] += propBase;                       // sub-mesh array

        for (int s = 0; s < meshGroup[16]; s++) {
            subMesh = meshGroup[27] + 4 * s;
            *subMesh += propBase;

            entry = *subMesh;
            *(entry + 12) += propBase;                   // vertex/index data
            *(entry + 16) = entry;                       // self-reference

            if (*(entry + 20))
                *(entry + 24) = propBase + *(entry + 20); // extra data
            else
                *(entry + 24) = 0;

            *(entry + 60) += propBase;                   // draw data A
            *(entry + 64) += propBase;                   // draw data B

            drawData = *(entry + 60);

            if (*(entry + 42) == 0x2080) {
                // SPECIAL RENDER PATH (multi-texture)
                drawData[3] += propBase;
                drawData[4] += propBase;
                drawData[5] += propBase;
                drawData[6] += propBase;
                *(entry + 44) = 0;     // no pre-render callback
                *(entry + 52) = 0;     // no post-render callback
            } else {
                // STANDARD RENDER PATH
                sub_4EA320(propBase);               // create vertex buffer
                sub_4EA160(drawData, 2, 1);         // create index buffer
                *(entry + 44) = sub_59D000;         // pre-render = texture binder
                *(entry + 52) = sub_59D030;         // post-render = texture unbinder
            }

            sub_562B30(entry, 0);                   // register with render dispatch
        }
    }

    *(propBase + 6) = 1;   // mark as initialized
}
```

### Sub-Batch +4 = Texture Object Pointer (for props)

In the prop sub-batch entry structure, offset +4 holds a pointer to the pre-render
callback context, and the texture object pointer is at context+4 (i.e., the second DWORD
after the context start at sub-batch+8 is the actual texture object pointer).

When `sub_59D000` is called as the pre-render callback, it reads `context[1]` to get
the texture object pointer, then uses `texObj + 0xAC` to get the active texture slot index,
and finally reads `texObj + 28 + 4*slotIndex` to get the D3D texture handle.

### Key Difference: Props vs Characters

| Aspect | Props (PCPB) | Characters (NM40) |
|--------|-------------|-------------------|
| Texture location | Embedded inline in PCPB | External, in AWAD archive |
| Texture reference | Direct pointer (after fixup) | Hash-based lookup |
| Binding callback | sub_59D000 (standard) | Custom render function (sub_5709C0) |
| Render type | Standard or 0x2080 special | Always skinned shader |
| Texture creation | sub_4ED380 during Prop_Init | During character construction |

---

## 7. AWAD Loading Pipeline

### SfAssetRepository_Load (0x52B0E8)

The main asset loading entry point. Searches for WAD archives across locale chains:

```
Search order for WAD "Common" with locale "us;ww":
  1. us/Wads/Common.zwd  (compressed SFZC)
  2. us/Wads/Common.wad  (raw AWAD)
  3. us/Wads/Common.lst  (list, fallback)
  4. ww/Wads/Common.zwd
  5. ww/Wads/Common.wad
  6. ww/Wads/Common.lst
```

### Search Table (g_assetSearchTable at 0x63EC3C)

3 entries, 12 bytes each:

| # | Path Prefix | Extension | Decompress | Continue on Fail |
|---|-------------|-----------|------------|------------------|
| 0 | `Wads/` | `.zwd` | Yes (SFZC) | Yes |
| 1 | `Wads/` | `.wad` | No (raw) | Yes |
| 2 | `Wads/` | `.lst` | No | No |

### ClAssetTypeMap_Lookup (0x58B770)

After loading the AWAD into memory, individual assets are resolved by type hash:

```c
int ClAssetTypeMap_Lookup(TypeMap* map, int typeHash) {
    for (int i = 0; i < map->count; i++) {
        if (map->entries[2*i + 1] == typeHash)
            return map->entries[2*i + 2];   // factory function
    }
    return 0;  // not found
}
```

### Asset Factory Dispatch

When an asset is loaded from an AWAD:

1. The AWAD TOC entry contains `{name_hash, type_hash, data_offset}`.
2. `ClAssetTypeMap_Lookup(type_hash)` returns the factory function.
3. The factory function allocates an 8-byte object: `{vtable_ptr, data_ptr}`.
4. On first access, the vtable's validate method checks magic bytes and runs parsing/fixup.

### WAD File Search Paths

The engine searches for WADs relative to the game root directory:

```
<game_root>/<locale>/Wads/<name>.<ext>
```

Where `<locale>` is iterated from the locale chain (e.g., `"us"` then `"ww"`).

Up to 32 WAD slots are maintained at `dword_E55E90` (3 DWORDs per slot).

---

## 8. Accessor Functions (NM40)

The ClNoamActor object provides accessor functions that return values from the descriptor
array. These are called by NoamFigure_Init to populate the rendering instance.

All accessors are simple single-instruction `__thiscall` functions:

| Address | Function | Returns | Formula | Purpose |
|---------|----------|---------|---------|---------|
| 0x527840 | `GetDescSlot2` | int | `this[301]` | Descriptor slot [2]: NM40 data ref |
| 0x527850 | `GetDescSlot3` | int | `this[302]` | Descriptor slot [3]: NM40 data ref |
| 0x527860 | `GetDescSlot4` | int | `this[303]` | Descriptor slot [4]: NM40 data ref |
| 0x527870 | `GetDescSlot5` | int | `this[304]` | Descriptor slot [5]: NM40 data ref |
| 0x527880 | `GetDescSlot6` | int | `this[305]` | Descriptor slot [6]: NM40 data ref |
| 0x527890 | `GetDescSlot7` | int | `this[306]` | Descriptor slot [7]: NM40 data ref |
| 0x5278B0 | `GetTexA` | int | `this[27]` | TEX_A hash (PCIM texture A) |
| 0x5278C0 | `GetTexB` | int | `this[28]` | TEX_B hash (PCIM texture B) |

**Example decompilation** (all follow this pattern):
```c
int __thiscall GetTexA(DWORD* this) {
    return this[27];    // offset +0x6C
}
```

NoamFigure_Init calls these in sequence:
```c
noamFigure[17] = GetDescSlot2(actor);   // NM40 mesh ref
noamFigure[18] = GetDescSlot3(actor);   // NM40 mesh ref
noamFigure[19] = GetDescSlot4(actor);   // NM40 mesh ref
noamFigure[20] = GetDescSlot5(actor);   // NM40 mesh ref
noamFigure[21] = GetDescSlot6(actor);   // NM40 mesh ref
noamFigure[22] = GetDescSlot7(actor);   // NM40 mesh ref
noamFigure[23] = GetTexA(actor);        // PCIM texture hash A
noamFigure[24] = GetTexB(actor);        // PCIM texture hash B
```

### Additional Accessors

| Address | Function | Returns | Purpose |
|---------|----------|---------|---------|
| 0x56D8C0 | `GetDataPtr` | `this[1]` | Raw NM40 data pointer (from factory) |
| 0x56D8D0 | `GetPCRDHeader` | -- | PCRD header accessor (thunk) |
| 0x56D8E0 | `GetBoneCount` | -- | Bone count accessor (`.kallis` thunk) |
| 0x56D910 | `HasRenderableSubmeshes` | bool | Checks if NM40 has any renderable sub-batches |

---

## 9. Render Function Registry

The engine registers render functions as callbacks in the render dispatch system.
Character meshes use dedicated render functions distinct from the standard world/prop
path.

### NOAM Render Functions

| Address | Name | Size | Description |
|---------|------|------|-------------|
| 0x571000 | `RENDERFUNC_NOAM_DIFFUSE` | -- | Main entry: visibility check, render path selection, 2-pass dispatch |
| 0x5709C0 | `RENDERFUNC_NOAM_SKIN` | 0x632 | Full skinned mesh renderer: bone matrix upload, texture bind, DrawIndexedPrimitive |
| 0x56EC20 | `RENDERFUNC_NOAM_BACKFACE` | 0xE9 | Back-face pass for double-sided materials: distance-sorted insertion into render queue |
| 0x56E7D0 | `RENDERFUNC_NOAM_VISIBILITY` | 0x13D | Visibility/LOD check: determines if NoamFigure should render |

### Render Path Selection (sub_571000)

The NOAM diffuse function selects between three rendering paths:

```
Path A: matParams[12] > 0.0
  -> Simple single-pass (sub_5709C0 with one texture)
  -> Used for self-illuminated or emissive materials

Path B: matParams[14] * matParams[13] != 1.0
  -> Alpha-blended pass (sub_56FB50 for transparency)
  -> Only renders if byte_133C94F is set

Path C: Standard double-pass (DEFAULT)
  -> If double-sided flag (bit 0x20 at PCRD+32):
       Pass 1: front faces with TEX_A (via sub_56FB50)
       Pass 2: back faces with TEX_B (via sub_56EC20)
  -> If single-sided:
       Single pass with TEX_A (via sub_56FB50)
```

### RENDERFUNC_NOAM_SKIN Detail (sub_5709C0)

This is the workhorse function. It:

1. **Gets bone matrices** from `sub_523230(a1[2])` -- the animated pose.
2. **Sets shader constants** c13 = `{0, 1, 2, 4}` (bone matrix scale factors).
3. **Computes world-view-projection matrix** from local matrix and camera VP.
4. **Sets D3D render states:**
   - AlphaBlendEnable = FALSE (D3DRS 23 = 5... likely specific to this engine's state encoding)
   - CullMode, ZEnable, ZWriteEnable, AlphaTestEnable, AlphaRef, etc.
5. **Binds shader program** via `sub_4FFD10`.
6. **Sets vertex declaration** (position, normal, UV, blend indices, blend weights).
7. **First texture pass** -- binds TEX_A via `sub_4E9970`:
   ```c
   int texHandle = *(a1[1] + 108) + 4;  // from NoamFigure skin data
   sub_4E9970(texHandle, ..., 0, texHandle, 0);
   ```
8. **For each mesh group** (numMeshTableEntries iterations):
   - For each sub-batch in the group:
     - Upload bone matrices to vertex shader constants c25+ (3 float4 per bone = float4x3):
       ```c
       for (bone = 0; bone < boneCount; bone++) {
           src = boneMatrices + boneRemap[bone] * 64;  // 4x4 source
           dst = shaderBuffer + bone * 12;              // float4x3 dest
           // Transpose 4x4 -> 3x4 (skip last row)
           dst[0..3]  = src col0;  // row 0
           dst[4..7]  = src col1;  // row 1
           dst[8..11] = src col2;  // row 2
       }
       SetVertexShaderConstantF(25, shaderBuffer, 3 * boneCount);
       ```
     - Set vertex/index buffers via `sub_4EA2E0`.
     - `DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, 0, 0, vertexCount, 0, primitiveCount)`.
9. **Adjust alpha ref** (reduce by 15 for second pass).
10. **Second texture pass** -- binds TEX_B via `sub_4E9970`:
    ```c
    int texHandle2 = *(a1[1] + 112) + 4;  // from NoamFigure skin data
    sub_4E9970(texHandle2, ..., 0, texHandle2, 0);
    ```
11. **Repeat mesh group iteration** with the second texture.

---

## 10. Implementation Notes for SpiderView

### Auto-Texture Resolution Strategy

To display NM40 models with correct textures in SpiderView (offline, no game running),
the following pipeline must be implemented:

### Step 1: Parse SCT for sauCharacterInit Calls

SCT scripts contain the character initialization calls that specify which textures
to use. The relevant pattern in the bytecode is:

```
PUSH_STRING "texture_name_A"     ; TEX_A (diffuse)
PUSH_STRING "texture_name_B"     ; TEX_B (normal/specular)
PUSH_STRING "character_template"  ; template name
CALL sauCharacterInit
```

The SCT disassembler (`tools/sct_disasm.py`) can extract these string constants.
Look for sequences of 3 string pushes followed by a call to the `sauCharacterInit`
native function.

### Step 2: Extract String Constants

From the SCT string table (NTV section), resolve the string indices pushed before
the call. The strings are the human-readable asset names (e.g., `"boggart_warrior"`,
`"jared_body_d"`, `"jared_body_n"`).

### Step 3: Hash Strings to Get PCIM Name Hashes

Apply `HashString` to each texture name to produce the PCIM asset lookup key:

```c
int HashString(char* str) {
    int result = 0;
    for (char c = *str; c; c = *++str)
        result += c + (result << (c & 7));
    return result;
}
```

The resulting hash is the `name_hash` field in the AWAD TOC entry.

### Step 4: Look Up in AWAD

Search the AWAD TOC (sorted by `name_hash`) using binary search:
1. Find the entry where `name_hash` matches.
2. The entry's `type_hash` should be `0x01F1096F` (Image/PCIM).
3. Read `data_offset` to locate the PCIM data within the AWAD blob.

### Step 5: Extract DDS from PCIM

The PCIM format has a 193-byte header. The DDS data starts at the offset stored at
PCIM+0x10 (relative to the PCIM start in the AWAD).

```
pcim_data = awad_blob + toc_entry.data_offset
dds_offset = *(uint32*)(pcim_data + 0x10)
dds_data = pcim_data + dds_offset
```

### Step 6: Apply to NM40 Model

Create two texture objects (TEX_A and TEX_B) from the extracted DDS data and bind
them during rendering of the NM40 mesh. TEX_A is the primary diffuse texture used
in the first render pass, TEX_B is used in the second pass (often a detail/specular
map or alternate skin).

### Alternative: Direct WAD Scanning

If SCT parsing is not available, a simpler heuristic approach:

1. For a given NM40 asset in a WAD (e.g., `boggart_warrior` mesh), look for PCIM
   assets in the same WAD whose name hashes are numerically close or whose names
   (if known) contain the character name prefix.
2. The asset name dictionary (`tools/asset_names.py`) has 143 resolved names that
   can help with this matching.

### Data Flow Diagram

```
SCT Script (bytecode)
  |
  +-- sauCharacterInit("tex_diffuse", "tex_normal", "template")
      |
      +-- HashString("tex_diffuse")  ->  0xABCD1234 (TEX_A hash)
      +-- HashString("tex_normal")   ->  0xEFGH5678 (TEX_B hash)
      |
      +-- AWAD TOC binary search by name_hash
          |
          +-- PCIM asset at data_offset
              |
              +-- DDS at PCIM + *(PCIM+0x10)
                  |
                  +-- D3D texture handle (runtime)
                  |   OR
                  +-- OpenGL texture (SpiderView)
```

### Known Character-Texture Associations

From runtime hash capture and SCT analysis:

| Character | WAD | Likely TEX_A | Likely TEX_B |
|-----------|-----|-------------|-------------|
| Jared | Chapter*.zwd | jared_body_d | jared_body_n |
| Simon | Chapter*.zwd | simon_body_d | simon_body_n |
| Mallory | Chapter*.zwd | mallory_body_d | mallory_body_n |
| Boggart Warrior | DeepWood.zwd | boggart_warrior_d | boggart_warrior_n |
| ThimbleTack | Common.zwd | thimbletack_d | thimbletack_n |

(Names are hypothetical based on naming conventions observed in recovered asset names.
Exact names require SCT disassembly or runtime hash capture.)

### Integration with SpiderView Architecture

In SpiderView's `FormatRegistry` pattern:

1. **NM40 handler** already parses geometry (vertices, indices, bone data).
2. Add a `resolveTextures(nm40Node, archiveNode)` step that:
   - Finds the parent WAD of the NM40 asset.
   - Scans SCT assets in the same WAD for `sauCharacterInit` calls referencing this NM40.
   - Extracts PCIM data for matched texture hashes.
   - Creates OpenGL textures from the DDS data.
3. The 3D viewer binds these textures before drawing the NM40 mesh.

---

## Appendix A: Key Address Reference

### Functions

| Address | Name | Convention | Purpose |
|---------|------|-----------|---------|
| 0x405380 | HashString | __cdecl | Compute name hash |
| 0x458170 | sauCharacterInit | __thiscall | VM native: pop 3 strings, hash textures, call vtable[18] |
| 0x4E9970 | SetTextureAndSamplers | __usercall | Bind D3D texture + set sampler states |
| 0x523A00 | ModelInstance_Init | __thiscall | Create skeleton, bone matrices, material builder |
| 0x527840-5278C0 | Accessor functions | __thiscall | Return descriptor array values |
| 0x527EC0 | ClNoamActor_Init | __thiscall | Copy descriptor, create sub-objects |
| 0x52A340 | ClAssetTypeMap_Register | __cdecl | Register asset type (hash + factory) |
| 0x52B0E8 | SfAssetRepository_Load | -- | Load WAD archive |
| 0x55A850 | Prop_Init | __thiscall | Fix up PCPB, create textures, register render |
| 0x5709C0 | RENDERFUNC_NOAM_SKIN | __userpurge | Full skinned mesh draw |
| 0x571000 | RENDERFUNC_NOAM_DIFFUSE | __cdecl | Render path selector (2-pass) |
| 0x56D860 | NM40_Validate | __thiscall | Check magic "NM40", trigger parse |
| 0x56D910 | HasRenderableSubmeshes | __thiscall | Check if NM40 has drawable batches |
| 0x56D940 | ClModelAsset_Factory | __cdecl | Create 8-byte asset object for NM40 |
| 0x56DCF0 | ClTextureAssetPc_Factory | __cdecl | Create 8-byte asset object for PCIM |
| 0x56E630 | NM40_Parse | thunk | .kallis thunk to NM40 data parser |
| 0x56E970 | NoamFigure_Init | __thiscall | Create render instance with tex refs |
| 0x56EC20 | RENDERFUNC_NOAM_BACKFACE | __cdecl | Back-face render pass |
| 0x56E7D0 | RENDERFUNC_NOAM_VISIBILITY | -- | LOD/visibility check |
| 0x5861A0 | MaterialBuilder_Init | __thiscall | Build material lookup structures |
| 0x585F20 | RenderBatch_Setup | __thiscall | Populate render batches from materials |
| 0x586FA0 | Material_Init | __thiscall | Create single material (132 bytes) |
| 0x589760 | SkinRenderer_Init | __thiscall | Create skin dispatch structure (172 bytes) |
| 0x58B770 | ClAssetTypeMap_Lookup | __thiscall | Linear search type map |
| 0x59D000 | PreRenderCallback_BindTexture | __usercall | Prop texture binding |
| 0x59D030 | PostRenderCallback_UnbindTexture | __usercall | Prop texture unbinding |
| 0x20BC000 | NM40_PointerFixup | __usercall | .kallis native: fix relative offsets to absolute |

### Vtable Addresses

| Address | Class | Notes |
|---------|-------|-------|
| 0x63EA80 | ClModelAsset | NM40 asset vtable |
| 0x643100 | NoamFigure | Render instance vtable |
| 0x645B34 | Material | Per-bone material vtable |
| 0x641D34 | ClPropAsset | PCPB prop vtable |
| ClTextureAssetPc | ClTextureAssetPc | PCIM texture vtable |

### Global Variables

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| 0xE56010 | struct | g_AssetTypeMap | Global type map (count + hash/factory pairs) |
| 0xE56114 | dword | g_AssetTypeMapInitFlag | bit 0 = map initialized |
| 0x63EC3C | struct[3] | g_assetSearchTable | WAD search paths |
| 0xE55E90 | DWORD[96] | g_WADSlots | 32 WAD slots (3 DWORDs each) |
| 0x133C940 | dword | g_CurrentSkinData | Active skin data during NOAM render |
| 0x133C944 | dword | g_CurrentNoamFigure | Active NoamFigure during render |
| 0x133C948 | dword | g_CurrentBatchContext | Active batch context during render |
| 0x133C94F | byte | g_RenderPassFlag | Second-pass flag for 2-pass rendering |
| 0x133C954 | dword | g_BackfaceQueueCount | Back-face render queue depth (max 32) |
| 0x133C970 | float[16] | g_TempMatrix | Temporary 4x4 matrix |
| 0x133C9B8 | byte[64] | g_WorldMatrix | Current world matrix during NOAM render |
| 0x133BFE0 | byte[1056] | g_BoneShaderBuffer | Bone matrix upload buffer (22 * float4x3) |

---

## Appendix B: HashString Implementation

Used universally for asset name hashing, widget identification, and type registration:

```c
// Address: 0x405380
int HashString(const char* str) {
    int result = 0;
    char c = *str;
    while (c) {
        result += c + (result << (c & 7));
        c = *++str;
    }
    return result;
}
```

### Known Type Name Hashes

| Type Name | Hash | Hex |
|-----------|------|-----|
| Mesh | 48914 | 0x0000BB12 |
| Prop | 132946 | 0x00020752 |
| Script | 448678 | 0x0006D8A6 |
| Font | 10101767 | 0x009A2807 |
| Image | 32698208 | 0x01F1096F |
| Database | 70499395 | 0x04339C43 |
| Table | 1037588 | 0x000FD514 |
| Animation | 1157073184 | 0x44FE8920 |
| AnimationList | -93097092 | 0xFA5E717C |
| NavMesh | 426722468 | 0x194738A4 |
| Playlist | 106170980 | 0x06572A64 |
| Strings | 1762919912 | 0x690BE5E8 |
| ControlMap | -1194542394 | 0xB8D1C6C6 |
| ActionStateMachine | -1128733576 | 0xBCBFB478 |
| BehaviorStateMachine | 1431388234 | 0x5550C44A |
| Particles | -101082879 | 0xF9C2A901 |
| AudioEffects | -487126418 | 0xE2F8E66E |
| LoadedCinematic | 812448066 | 0x3072B942 |
| StreamedCinematic | -357836588 | 0xEAB838D4 |
| SaveGameIcon | 1159373454 | 0x451A228E |
| AssetMap | 714407728 | 0x2A7E6F30 |
| World | 16412512 | 0x00FA6F60 |
