# PCW World Format (ZLIB → PCWB)

**Status:** Fully reversed + unpacker tool (Session 10)

---

## Overview

PCW files are world/level data for The Spiderwick Chronicles. Each `.pcw` file contains
a complete level's geometry, textures, sectors, props, and render data. The format uses
the same two-layer pattern as ZWD:

1. **Outer: ZLIB** — zlib compression wrapper with 12-byte header
2. **Inner: PCWB** — PC World Binary (monolithic level data)

### Unpacker Tool

```
python tools/spiderwick_unpack.py <file.pcw> [output_dir]
```

Extracts the decompressed world blob + all embedded DDS textures.

---

## Outer Format: ZLIB

```
Offset  Size  Type     Description
------  ----  ----     -----------
0x00    4     char[4]  Magic: "ZLIB"
0x04    4     u32 LE   Compressed size (= file_size - 12)
0x08    4     u32 LE   Uncompressed size (exact match after inflate)
0x0C    ...   bytes    Standard zlib deflate stream
```

Identical structure to SFZC, just different magic.

---

## Inner Format: PCWB

The decompressed data is a monolithic world binary — NOT an archive of separate files
like AWAD. It contains all level data in a structured binary format.

### Header

```
Offset  Size  Description
------  ----  -----------
+0x00   4     Magic: "PCWB"
+0x04   4     Version (10) — engine checks for exactly v10
+0x08   4     Page size (0x1000 = 4096)
+0x0C   4     Total decompressed size
+0x10   4     Page size (repeated)
+0x14   4     Geometry data offset
+0x18   4     Texture data offset
+0x1C   4     (always 1)
+0x20   4     End-of-data offset
...
```

The engine loader `ClWorld_LoadFromFile` (0x518C20) validates:
1. Inner magic matches "PCWB" — via `PCWB_ValidateMagic` (0x518260)
2. Version == 10 (rejects with "Version mismatch (got v%d, expected v%d)")
3. Alignment — via `PCWB_ValidateAlignment` (0x518280): page_size must be power-of-2,
   and offsets at +0x0C, +0x10, +0x14, +0x18 must be divisible by page_size

### Embedded Sub-Sections

PCWB data is page-aligned (0x1000 boundaries). Sub-sections found at page boundaries:

| Magic | Description | Typical Count |
|-------|-------------|---------------|
| PCWB  | World header | 1 |
| PCRD  | PC Render Data (mesh/geometry) | 2–2400+ |
| PCIM  | PC Image (embedded texture) | 0–18 |

### PCRD — Render Data

The most numerous sub-section. Contains mesh geometry, vertex/index buffers,
material references. Each PCRD section is typically 300–1000 bytes.

```
+0x00   4     Magic: "PCRD"
+0x04   4     Version (2)
+0x08   ...   Render data (vertices, indices, materials)
```

### PCIM in PCWB — Embedded Textures

PCIM textures in PCWB use a **split layout** — the PCIM metadata header is at one
page-aligned offset, and the DDS pixel data is at a separate offset:

```
+0x00   4     Magic: "PCIM"
+0x04   4     Version (2)
+0x08   4     Total data size (header + DDS)
+0x0C   4     DDS data size
+0x10   4     DDS data offset (ABSOLUTE within PCWB — NOT relative!)
+0x94   4     Format flags
+0x9C   4     Width
+0xA0   4     Height
+0xA4   4     Mip levels
```

**Key difference from AWAD:** In AWAD, +0x10 = 0xC1 (relative offset to embedded DDS).
In PCWB, +0x10 is an absolute offset within the decompressed file where the DDS data lives
(always at the next page boundary after the PCIM header).

### World Content Strings

The PCWB header region contains embedded strings for world objects:
- Prop names: "Prop_Birdhouse01", "prop_FrontDoor", "PROP_BackGate"
- Sector names: "Default", "PPEopen", "PPEclose"
- Portal/transition labels

---

## World Files

All world files are in `ww/Worlds/`. Each level has 1–2 variants:

| File | Level |
|------|-------|
| Shell.pcw | Main menu / shell |
| GroundsD.pcw | Spiderwick Estate grounds (day) |
| Grounds1.pcw | Grounds variant 1 |
| Grounds2.pcw | Grounds variant 2 |
| MansionD.pcw | Mansion interior (day) |
| Mansion2.pcw | Mansion variant 2 |
| GrGarage.pcw | Garage area |
| DeepWood.pcw | Deep woods |
| DeepWoo1.pcw | Deep woods variant |
| FrstRoad.pcw | Forest road |
| FrstRoa1.pcw | Forest road variant |
| GoblCamp.pcw | Goblin camp |
| GoblCam1.pcw | Goblin camp variant |
| MnAttack.pcw | Mansion attack |
| ThimbleT.pcw | Thimbletack's area |
| Tnl2Town.pcw | Tunnel to town |
| MGArena1–4.pcw | Minigame arenas |

---

## Statistics

Tested on all 20 .pcw world files — **100% extraction success**.

| World | Compressed | Decompressed | Textures | PCRD Sections |
|-------|-----------|-------------|----------|---------------|
| Shell | 17 MB | 37 MB | 18 | 10 |
| GroundsD | 21 MB | 40 MB | 16 | 6 |
| GoblCamp | 30 MB | 48 MB | 10 | 3 |
| FrstRoad | 30 MB | 50 MB | 5 | 10 |
| MnAttack | 21 MB | 43 MB | 6 | 7 |

---

## Engine Loading: Raw PCWB Support

`ClWorld_LoadFromFile` (0x518C20) checks whether the loaded file starts with `"ZLIB"`:

```
if (memcmp(buffer, "ZLIB", 4) == 0) {
    // decompress zlib → use decompressed buffer
    allocate temp buffer
    zlib_decompress(buffer, decompressed, ...)
    buffer = decompressed
} else {
    // use raw buffer directly → expects PCWB
}
// validate PCWB magic, version, alignment
```

This means **uncompressed PCWB files saved as `.pcw` work natively**. The engine
will skip decompression and parse the raw PCWB data directly.

### World Path Construction

World files are loaded from a hardcoded path pattern in `WorldViewer_Init` (0x490770):

```c
sprintf(path, "Worlds\\%s.%s", world_name, g_worldFileExt);  // g_worldFileExt = "pcw"
```

The extension is hardcoded at `g_worldFileExt` (0x6301A4) as `"pcw"`.
There is no alternative extension search like WADs have (.zwd/.wad/.lst).

### Streaming vs Immediate Loading

The world loader has two paths:
- **Streaming** (when ZLIB wrapper present): uses `StreamingIO_CalcAddress` and async
  block-based loading with the engine's streaming pool
- **Immediate** (raw PCWB): synchronous file read into memory, then parse

The streaming path is more memory-efficient for large worlds but requires the ZLIB wrapper.

### Modding Implications

- Replace `.pcw` file directly (compressed or uncompressed — both work)
- Uncompressed PCWB is 2–3× larger on disk but avoids recompression
- World extension is hardcoded; no override mechanism without ASI hook

---

## IDA Names (Session 10)

| Address | Name | Description |
|---------|------|-------------|
| 0x518C20 | ClWorld_LoadFromFile | Load .pcw file (ZLIB+PCWB or raw PCWB) |
| 0x518260 | PCWB_ValidateMagic | Check "PCWB" magic bytes |
| 0x518280 | PCWB_ValidateAlignment | Validate page alignment of offsets |
| 0x490770 | WorldViewer_Init | World initialization (builds path, loads) |
| 0x6301A4 | g_worldFileExt | World file extension = "pcw" |

---

## Related

- [ZWD_FORMAT.md](ZWD_FORMAT.md) — Asset archives (.zwd / AWAD)
- [ASSET_LOADING.md](ASSET_LOADING.md) — Full engine file I/O pipeline
- `tools/spiderwick_unpack.py` — Extraction tool
