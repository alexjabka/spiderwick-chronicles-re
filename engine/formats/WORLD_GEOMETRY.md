# World Geometry Format (PCRD in PCWB)

**Status:** Fully reversed + textured exporter (Session 11)

---

## Overview

World geometry in The Spiderwick Chronicles is stored as PCRD (PC Render Data)
sections inside PCWB world files. Unlike character meshes (NM40 in ZWD archives),
world geometry uses simpler vertex formats with baked lighting and is rendered
as triangle strips.

Each PCWB world contains hundreds to thousands of PCRD sections: a small set
in the header region (~340) and a larger set of geometry-region PCRDs (~2067).

---

## PCRD Section

```
Offset   Size   Description
------   ----   -----------
+0x00    4      Magic: "PCRD"
+0x04    4      Version (2)
+0x08    ...    Render data (vertices, indices, material refs)
```

### Distribution in PCWB

| Category              | Typical Count | Description                        |
|----------------------|---------------|------------------------------------|
| Header PCRDs          | ~340          | Referenced from PCWB header region |
| Geometry region PCRDs | ~2067         | Bulk world mesh data               |
| **Total**             | ~2400+        | Per world file                     |

---

## Vertex Formats

World geometry uses two known vertex strides, determined by the PCRD header size field.

### Stride 24 (hdr_size = 0x18)

```
Offset   Size   Type      Description
------   ----   ----      -----------
+0x00    12     float3    Position (x, y, z)
+0x0C    4      ubyte4    Vertex color RGBA (baked lighting)
+0x10    8      float2    UV texture coordinates (u, v)
```

**Total stride: 24 bytes (0x18)**

### Stride 32 (hdr_size = 0x10)

```
Offset   Size   Type      Description
------   ----   ----      -----------
+0x00    12     float3    Position (x, y, z)
+0x0C    4      ubyte4    Vertex color RGBA (baked lighting)
+0x10    8      float2    UV texture coordinates (u, v)
+0x18    8      ...       Additional data (TBD -- possibly lightmap UV or tangent)
```

**Total stride: 32 bytes (0x20)**

### Vertex Colors = Baked Lighting

The vertex color RGBA at +0x0C stores **baked lighting data**, specifically
ambient occlusion and shadow information. This is NOT artist-painted color --
it encodes pre-computed illumination from the level's light bake pass.

When extracting for Blender, these vertex colors should be interpreted as
light/shadow multipliers, not albedo color.

---

## Triangle Strip Topology

World geometry uses **D3DPT_TRIANGLESTRIP** (triangle strips), NOT triangle lists.

### Strip Winding Order

Direct3D triangle strips alternate winding to maintain consistent face normals:

```
Even triangle (i=0,2,4,...): vertices (v[i], v[i+1], v[i+2])
Odd  triangle (i=1,3,5,...): vertices (v[i+1], v[i], v[i+2])
```

The alternating winding ensures all triangles face the same direction without
requiring explicit winding flags.

### Degenerate Restart

Strips are concatenated using **degenerate triangles** as separators. A degenerate
triangle has zero area (two or more identical vertex indices) and is automatically
culled by the GPU. This allows multiple disjoint mesh strips to be submitted in
a single draw call.

Pattern: `...last_of_strip_A, last_of_strip_A, first_of_strip_B, first_of_strip_B...`

When converting to triangle lists for tools like Blender, degenerate triangles
(any triangle where two or more indices are equal) must be filtered out.

---

## Render Batch Table (Triple-FFF Pattern)

Each PCRD section has an associated **batch entry** embedded in the data stream
between consecutive PCRD headers. The batch entry is identified by a triple
`FFFFFFFF` signature and contains the PCRD-to-texture mapping.

### Batch Entry Layout

```
Offset from FFF   Size   Description
---------------   ----   -----------
FFF - 4            4      texture_index (u32) -- index into PCIM array
FFF + 0            4      0xFFFFFFFF (sentinel 1)
FFF + 4            4      0xFFFFFFFF (sentinel 2)
FFF + 8            4      0xFFFFFFFF (sentinel 3)
FFF + 12           4      0x00000000
FFF + 16           4      Hash/flags (e.g. 0x1240002A)
FFF + 20           4      0x00000000
FFF + 24           4      Size/flags (e.g. 0x000011A0)
FFF + 28           4      0x00000000
FFF + 32           4      Flags (e.g. 0x00500000)
FFF + 36           4      PCRD offset (u32) -- offset to PCRD section in PCWB
```

### Discovery Method

Scan the entire PCWB for triple `FFFFFFFF` sequences. For each hit:
- `texture_index` = u32 at `FFF_offset - 4`
- `pcrd_offset` = u32 at `FFF_offset + 36`

This maps 1:1 to PCRD sections. Tested on all 20 world files:
**100% mapping rate** (every PCRD gets a valid texture assignment).

### Texture Index Resolution

The `texture_index` is a simple 0-based index into the ordered list of
PCIM sections found in the PCWB file. To resolve:

1. Find all PCIM sections by scanning for `"PCIM"` magic (4-byte aligned)
2. Sort by file offset
3. `texture_index` N maps to the Nth PCIM section
4. Extract DDS data from that PCIM (see PCW_FORMAT.md)

### Statistics

| World      | PCRDs | Mapped | Textures |
|-----------|-------|--------|----------|
| Grounds1  | 2322  | 2322   | 93       |
| FrstRoad  | 2808  | 2808   | 57       |
| MansionD  | 2657  | 2651   | 373      |

### Additional Batch Metadata

Between consecutive PCRD headers, the data stream also contains:
- **Per-PCRD descriptor** (~92 bytes): includes a `3F800000` (float 1.0) constant
  and a material hash value. Present for every PCRD.
- **Batch sequential index**: `FFFF00XX` pattern where XX is a sequential counter,
  useful for debugging but not needed for texture mapping.
- **PCRD idx/vtx counts**: the batch entry also stores the target PCRD's index
  and vertex counts (redundant with the PCRD header itself).

---

## Coordinate System

The engine uses a **Y-up** coordinate system (standard Direct3D convention).

When exporting to Blender (which uses Z-up), a coordinate swap is required:

```
Blender.X =  Engine.X
Blender.Y = -Engine.Z    (or Engine.Z depending on handedness)
Blender.Z =  Engine.Y
```

---

## Extraction Notes

When converting PCRD geometry for external tools:

1. **Parse vertices** using the correct stride (24 or 32) based on header
2. **Convert strips to triangles** -- iterate strip indices, emit triangles with
   correct winding (even/odd), discard degenerates
3. **Swap Y/Z axes** for Blender compatibility
4. **Preserve vertex colors** as a separate color layer (baked lighting data)
5. **Apply UV mapping** with texture resolved from the triple-FFF batch entry
6. **Embed textures** as PNG in glTF materials for auto-textured Blender import

### Exporter Tool (v3 — OBJ+MTL)

```
python tools/spiderwick_world_obj.py <world.pcwb> [output_dir]
python tools/spiderwick_world_obj.py --batch <wad_dir>
```

Produces:
- `.obj` file with named objects (one per unique texture)
- `.mtl` file referencing external DDS textures
- `textures/` folder with extracted DDS files
- Vertex colors as extended OBJ format: `v x y z r g b`

**Object grouping:** All PCRD chunks sharing the same texture are merged into
a single named object with one material slot. Category prefixes:
- `srf_texNNN` — surface geometry
- `ugr_texNNN` — underground props (below Y=-10 in engine coords)
- `shd_texNNN` — shader/technical meshes (small gradient textures, quads)

**Why OBJ over GLB:** Blender's glTF importer auto-multiplies vertex colors ×
base texture (glTF spec behavior). OBJ keeps vertex colors as a separate
Color Attribute without affecting the shader — users control blending manually.

### Terrain Multi-Layer Blending

World terrain uses overlapping geometry layers at the same XZ position:
- Base layer (e.g., grass 512x512) at ground level
- Detail layers (e.g., dirt, gravel) slightly above
- Engine blends via vertex color alpha as blend weight at runtime
- Detail texturing: base texture × small tiled texture (e.g., 64x32)

In export, layers become separate objects. Users can recreate blending in
Blender using Mix Shader with vertex color alpha as Factor.

### Underground Props

Props (doors, furniture, trees) are stored below ground (center_y < -10).
The engine positions them at runtime via transform matrices stored in the
PCWB header region (prop entries with names like `Prop_FrontDoor`).
Currently exported at storage positions — transform application is WIP.

---

## Data Flow

```
PCWB World File
│
├── PCIM sections (embedded textures)
│   └── 193-byte header + DDS data (see PCW_FORMAT.md)
│   └── Indexed 0..N by file offset order
│
├── Batch entries (embedded between PCRD headers)
│   └── Triple-FFF pattern: tex_index at FFF-4, PCRD offset at FFF+36
│   └── 1:1 mapping to PCRD sections
│
├── PCRD sections (2000-3000 per world)
│   └── { magic, version, hdr_size, idx_count, vtx_count, idx_off, vtx_off }
│   └── Vertices: pos + vertex_color + UV [+ extra]
│   └── Indices: triangle strip with degenerate restart
│
└── Resolution chain:
    PCRD offset -> batch entry -> texture_index -> PCIM[N] -> DDS texture
```

---

## Related

- [PCW_FORMAT.md](PCW_FORMAT.md) -- PCWB container format and PCIM embedded textures
- [SKELETON_FORMAT.md](SKELETON_FORMAT.md) -- Character mesh (NM40) vertex format for comparison
- [ZWD_FORMAT.md](ZWD_FORMAT.md) -- Asset archives (textures referenced by world geometry)
