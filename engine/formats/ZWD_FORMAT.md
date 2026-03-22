# ZWD Archive Format (SFZC → AWAD)

**Status:** Fully reversed + unpacker tool (Session 10)

---

## Overview

ZWD files are the primary asset archive format for The Spiderwick Chronicles (PC, 2008).
Each `.zwd` file is a zlib-compressed archive containing multiple assets (textures, animations,
database tables, scripts, device configs). The format uses a two-layer structure:

1. **Outer: SFZC** — zlib compression wrapper with 12-byte header
2. **Inner: AWAD** — hash-indexed asset archive with two-level TOC

### Unpacker Tool

```
python tools/spiderwick_unpack.py <file_or_dir> [output_dir]
```

---

## Outer Format: SFZC

```
Offset  Size  Type     Description
------  ----  ----     -----------
0x00    4     char[4]  Magic: "SFZC"
0x04    4     u32 LE   Compressed size (= file_size - 12)
0x08    4     u32 LE   Uncompressed size (exact match after inflate)
0x0C    ...   bytes    Standard zlib deflate stream
```

Decompress: skip 12-byte header, `zlib.decompress()` → AWAD data.

---

## Inner Format: AWAD

```
Offset                  Size             Description
------                  ----             -----------
0x00                    4                Magic: "AWAD"
0x04                    4                Version (always 1)
0x08                    4                Entry count (N)
0x0C                    N × 8            TOC Array 1: { name_hash: u32, entry_ptr: u32 }
0x0C + N×8              N × 8            TOC Array 2: { type_hash: u32, data_offset: u32 }
(page-aligned)          ...              Asset data blobs
```

### Two-Level TOC

The TOC consists of two parallel arrays of N entries each:

**Array 1** (at offset 0x0C): `name_hash` (u32) + `entry_ptr` (u32)
- `name_hash`: hash of asset name (engine's HashString function)
- `entry_ptr`: byte offset within AWAD to the corresponding Array 2 entry

**Array 2** (at offset 0x0C + N×8): `type_hash` (u32) + `data_offset` (u32)
- `type_hash`: hash identifying the asset type (e.g., 0x01F1096F = PCIM)
- `data_offset`: byte offset within AWAD to actual asset data

The `entry_ptr` in Array 1 simply points to the corresponding entry in Array 2
(trivially computable as `0x0C + N*8 + i*8`), allowing direct pointer-based access.

### Asset Sizes

Assets do not store explicit sizes in the TOC. Compute sizes by sorting entries
by `data_offset` and using gaps between consecutive offsets. The last asset extends
to EOF of the decompressed data.

Some formats declare their own size internally:
- **DBDB**: total_size at header +0x08
- **PCIM**: total_size at header +0x08, DDS_size at header +0x0C
- **STTL**: total_size at header +0x0C

---

## Sub-Asset Types

### PCIM — PC Image (Texture)

Most common asset type. Contains a 193-byte proprietary header followed by standard DDS texture data.

```
Offset  Size  Description
------  ----  -----------
+0x00   4     Magic: "PCIM"
+0x04   4     Version (2)
+0x08   4     Total asset size (header + DDS data)
+0x0C   4     DDS data size
+0x10   4     DDS offset (in AWAD: 0xC1 = relative; in PCWB: absolute)
+0x94   4     Format flags (0x15 = uncompressed RGBA)
+0x9C   4     Width (pixels)
+0xA0   4     Height (pixels)
+0xA4   4     Mip levels
+0xA8   4     Array count
+0xB0   4     LOD bias (float)
+0xBC   4     Pixel format descriptor
+0xC1   ...   DDS file data (starts with "DDS " magic)
```

**Type hash:** 0x01F1096F

DDS textures are standard DirectDraw Surface format — viewable with Paint.NET, GIMP, DirectXTex.
Observed formats: uncompressed RGBA (32bpp), various sizes from 32×32 to 512×512.

### DBDB — Database Table

Binary structured data tables used for game configuration, AI parameters, item definitions, etc.

```
+0x00   4     Magic: "DBDB"
+0x04   4     Version (4)
+0x08   4     Total size (includes header)
+0x0C   4     Record count
+0x10   4     Header size (0x20 = 32)
+0x14   4     Field descriptors size
+0x18   4     Data section size
+0x1C   4     Reserved (0)
+0x20   ...   Field descriptors + row data
```

**Type hash:** 0x04339C43

### STTL — Settings/Lookup Table

Compact binary tables for numeric parameters (camera settings, physics values, etc.).

```
+0x00   4     Magic: "STTL"
+0x04   4     Version (1)
+0x08   4     Entry count
+0x0C   4     Total size
+0x10   ...   Entries (hash + float values)
```

**Type hash:** 0xA117D668

### Devi — Device Configuration (Plain Text)

Input device mapping files (controller bindings). Plain text format:

```
Device["uniqueId"] = {
  Name = "Xbox 360 Controller"
  Signals["uniqueId"] = {
    CAMERA_YAW = MULTIPLY(NORMALIZE(MAP("axis", "fmt"), 0.3, 0.9), SWITCH("RB:Invert", 1, -1))
    PLAYER_MOVE_FORWARD = ...
    ...
  }
}
```

**Type hash:** 0xB8D1C6C6

### enum — Enumeration Definitions (Plain Text)

C-style enum definitions used by the Kallis VM and engine systems:

```
enum ClAnimType
{
    ANIM_IDLEA,
    ANIM_RUN,
    ...
}
```

**Type hash:** 0x7B0095BF

### Script Source ("// ...")

Plain text script source code. Identified by starting with `//` comment.
Contains game logic definitions (voice logic, behavior trees, etc.).

### AMAP — Animation Map

Binary animation mapping tables.

```
+0x00   4     Magic: "AMAP"
+0x04   4     Version (1)
+0x08   4     Entry count
+0x0C   4     Total size
```

### NM40 — Normal Map / Mesh Data

Binary mesh or normal map data.

```
+0x00   4     Magic: "NM40"
+0x04   4     Version (1)
+0x08   4     Channel count
```

### adat — Animation Data

Binary animation keyframe data. No text header — identified by position in TOC
relative to corresponding AMAP/NM40/aniz entries.

### aniz — Compressed Animation

Compressed/indexed animation data. Paired with adat entries.

### WF — Widget Font (Glyph Atlas)

UI font glyph atlas format. Contains named glyphs (Button_X, PCMS_ButtonLft, etc.) with UV coordinates and embedded PCIM texture atlases. NOT audio. See `engine/formats/WF_FORMAT.md`.

### .bin Sub-Types (Unrecognized Binary Blobs)

Many assets extracted as unknown binary blobs (the 1,263 "unrecognized" entries in
statistics) can be further classified by their leading magic bytes or content pattern:

| Magic / Pattern    | Description                                      |
|-------------------|--------------------------------------------------|
| `ac0000ac`        | Behavior state machines (AI logic)                |
| `brxb`            | Bounding box / spatial data                       |
| `PCPB`            | PC Push Buffer (embedded PCRD + PCIM data)        |
| `play` / `playpc_`| Music playlists                                   |
| `NAVM`            | Navigation mesh                                   |
| `Char`            | Character input binding config (plain text)       |
| `arpc`            | Indexed data with string references               |
| `a10000a1`        | Unknown binary (purpose TBD)                      |

These sub-types were discovered during bulk extraction analysis. They account for
a significant portion of the previously unclassified binary assets. Further
reverse engineering is needed to document their internal structures.

---

## Known Type Hashes

| Type Hash    | Magic | Description           |
|-------------|-------|-----------------------|
| 0x01F1096F  | PCIM  | PC Image (texture)    |
| 0x04339C43  | DBDB  | Database table        |
| 0xA117D668  | STTL  | Settings table        |
| 0xB8D1C6C6  | Devi  | Device config         |
| 0x7B0095BF  | enum  | Enum definition       |
| (varies)    | AMAP  | Animation map         |
| (varies)    | NM40  | Normal/mesh data      |
| (varies)    | adat  | Animation data        |
| (varies)    | aniz  | Compressed animation  |

---

## File Layout

```
.zwd file:
┌──────────────────┐
│ SFZC Header (12) │  magic + compressed_size + uncompressed_size
├──────────────────┤
│ zlib data        │  standard deflate stream
└──────────────────┘
         │ decompress
         ▼
┌──────────────────┐
│ AWAD Header (12) │  magic + version(1) + entry_count(N)
├──────────────────┤
│ TOC Array 1      │  N × { name_hash, entry_ptr }
│ (N × 8 bytes)    │
├──────────────────┤
│ TOC Array 2      │  N × { type_hash, data_offset }
│ (N × 8 bytes)    │
├──────────────────┤
│ Asset 0: PCIM    │  193-byte header + DDS texture
│   ┌─ DDS @+0xC1 │
├──────────────────┤
│ Asset 1: DBDB    │  binary database table
├──────────────────┤
│ Asset 2: Devi    │  plain text device config
├──────────────────┤
│ ...              │
└──────────────────┘
```

---

## Statistics (Full Game)

Tested on all 69 .zwd archives across ww/, na/, us/ locales — **100% extraction success**.

| Asset Type | Count | Description |
|-----------|-------|-------------|
| adat      | 3,509 | Animation data |
| PCIM/DDS  | 2,420 | Textures (2,560 DDS extracted) |
| (unknown) | 1,263 | Unrecognized binary blobs |
| NM40      |   247 | Mesh/normal data |
| aniz      |   247 | Compressed animations |
| Devi      |    20 | Controller configs |
| AMAP      |    25 | Animation maps |
| DBDB      |    28 | Database tables |
| STTL      |    12 | Settings tables |
| enum      |    10 | Enum definitions |
| WF        |     7 | Audio waveforms |
| script    |     1 | Source code |

---

## Engine Loading: Search Priority

`SfAssetRepository_Load` (0x52B0E8) searches for WAD assets in this order,
using a dispatch table at `g_assetSearchTable` (0x63EC3C):

| Priority | Pattern | Ext | Compressed | Flags |
|----------|---------|-----|------------|-------|
| 1 | `Wads/<locale>/<name>.zwd` | `.zwd` | Yes (SFZC→AWAD) | flag1=1, flag2=1 |
| 2 | `Wads/<locale>/<name>.wad` | `.wad` | **No** (raw AWAD) | flag1=0, flag2=1 |
| 3 | `Wads/<locale>/<name>.lst` | `.lst` | No (dev format?) | flag1=0, flag2=0 |

- **flag1** (byte at +8 in each table entry): whether to decompress SFZC before parsing AWAD
- **flag2** (byte at +9 in each table entry): whether to continue searching after a match

### Locale Search

The engine uses a locale chain string like `"us;ww"` (set via `SfAssetRepo_SetSearchPath` at 0x52A330).
It splits by `;` and tries each locale directory in order. So for a request to load "Common":

```
Wads/us/Common.zwd  →  Wads/us/Common.wad  →  Wads/us/Common.lst
Wads/ww/Common.zwd  →  Wads/ww/Common.wad  →  Wads/ww/Common.lst
```

### Uncompressed .wad Support

The engine **natively supports** uncompressed `.wad` files containing raw AWAD data
(no SFZC wrapper). When flag1=0, the loader calls `SfAssetRepo_LoadRawFile` (0x52A520)
which reads the file directly into memory and parses it as AWAD.

This means modders can place `.wad` files alongside `.zwd` files. However, `.zwd` is
checked **first**, so to override you must either:
- **Replace** the `.zwd` file
- **Remove** the `.zwd` and provide a `.wad` instead
- **Hook** the search order in the ASI mod to check `.wad` first

### .lst Files

The `.lst` extension (flag1=0, flag2=0) may be a development-era file list format.
No `.lst` files ship with the retail game. The flag2=0 means the engine stops after
finding a .lst (doesn't continue to next locale).

### Modding Implications

The engine does **NOT** support loading individual loose files (no .dds/.txt override).
All assets must be packaged in AWAD container format with hash-indexed TOC.
A **repacker** tool is needed for asset modification.

---

## IDA Names (Session 10)

| Address | Name | Description |
|---------|------|-------------|
| 0x52B0E8 | SfAssetRepository_Load | Main WAD loading dispatcher |
| 0x52A520 | SfAssetRepo_LoadRawFile | Load uncompressed file into buffer |
| 0x52A610 | SfAssetRepo_LoadCompressed | Load + decompress SFZC file |
| 0x52A330 | SfAssetRepo_SetSearchPath | Set locale search chain |
| 0x52ADB0 | (thunk) | Process loaded WAD data |
| 0x63EC3C | g_assetSearchTable | 3-entry search table {path, ext, flags} |

---

## Related

- [PCW_FORMAT.md](PCW_FORMAT.md) — World files (.pcw / PCWB)
- [ASSET_LOADING.md](ASSET_LOADING.md) — Full engine file I/O pipeline
- `tools/spiderwick_unpack.py` — Extraction tool
