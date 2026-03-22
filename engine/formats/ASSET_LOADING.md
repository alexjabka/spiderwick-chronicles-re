# Engine Asset Loading Pipeline

**Status:** Fully documented (Session 10)

---

## Overview

The Spiderwick Chronicles engine ("Ogre" by Stormfront Studios) uses a two-tier
asset loading system:

1. **WAD Archives** (`.zwd`/`.wad`) — streamed asset packs loaded via `SfAssetRepository`
2. **World Files** (`.pcw`) — monolithic level data loaded via `ClWorld`

Both support zlib compression as an outer wrapper, with graceful fallback to
uncompressed data.

---

## Directory Structure

```
<game_root>/
├── ww/                          # Worldwide (shared) assets
│   ├── Wads/
│   │   ├── Common.zwd           # Global assets (DB tables, enums, device configs)
│   │   ├── Chapter1.zwd         # Chapter-specific textures/anims
│   │   ├── Chapter2.zwd
│   │   ├── ...
│   │   ├── DeepWood.zwd         # Level-specific assets
│   │   ├── Shell.zwd            # Main menu assets
│   │   └── ...
│   └── Worlds/
│       ├── Shell.pcw            # Main menu world
│       ├── GroundsD.pcw         # Estate grounds
│       └── ...
├── us/                          # US English locale
│   └── Wads/
│       ├── Common.zwd           # US-specific overrides (voice, text)
│       ├── Shell.zwd
│       └── ...
└── na/                          # North America locale
    └── Wads/
        ├── Common.zwd           # NA-specific overrides
        └── ...
```

---

## Locale System

The engine uses a locale chain to search for assets with regional overrides.

### Search Chain

Set via `SfAssetRepo_SetSearchPath` (0x52A330). Observed values:
- `"us;ww"` — US English build: search US first, then worldwide
- `"na;ww"` — North America: search NA first, then worldwide

The chain is split by `;`. For each locale, the full search path is tried
before moving to the next locale.

### Resolution Order

For a request to load WAD "Common" with locale chain "us;ww":

```
1. us/Wads/Common.zwd    (compressed)
2. us/Wads/Common.wad    (raw)
3. us/Wads/Common.lst    (list?)
4. ww/Wads/Common.zwd    (compressed)
5. ww/Wads/Common.wad    (raw)
6. ww/Wads/Common.lst    (list?)
```

The first successful match wins. This allows locale-specific WADs to override
worldwide assets (e.g., US voice lines replacing worldwide placeholder audio).

---

## WAD Loading Pipeline

### Entry Point

`SfAssetRepository_Load` (0x52B0E8) — main dispatcher.

### Search Table

`g_assetSearchTable` (0x63EC3C) — 3 entries, each 12 bytes:

```c
struct SearchEntry {
    const char* path_prefix;  // e.g., "Wads/"
    const char* extension;    // e.g., ".zwd"
    uint8_t     decompress;   // 1 = SFZC decompress, 0 = raw
    uint8_t     continue_on_fail; // 1 = try next entry, 0 = stop
    uint16_t    pad;
};
```

| # | Path | Ext | Decompress | Continue |
|---|------|-----|------------|----------|
| 0 | `Wads/` | `.zwd` | Yes | Yes |
| 1 | `Wads/` | `.wad` | No | Yes |
| 2 | `Wads/` | `.lst` | No | No |

### Loading Flow

```
SfAssetRepository_Load(name)
│
├── For each locale in search chain:
│   ├── For each search table entry (.zwd, .wad, .lst):
│   │   │
│   │   ├── Build path: "<path_prefix><locale>/<name><extension>"
│   │   │   Example: "Wads/us/Common.zwd"
│   │   │
│   │   ├── Try to open file via streaming I/O
│   │   │   └── File not found? → try next entry
│   │   │
│   │   ├── If decompress flag set:
│   │   │   └── SfAssetRepo_LoadCompressed (0x52A610)
│   │   │       └── Read SFZC header, zlib inflate → raw AWAD
│   │   │
│   │   ├── If decompress flag not set:
│   │   │   └── SfAssetRepo_LoadRawFile (0x52A520)
│   │   │       └── Read file directly into memory buffer
│   │   │
│   │   ├── Register loaded data into asset slots
│   │   │   └── Up to 32 WAD slots (dword_E55E90, 3 DWORDs each)
│   │   │
│   │   └── Log: "SfAssetRepository: Loading {g"%s"}..."
│   │
│   └── Continue to next locale
│
└── Post-load: register textures + streaming chunks
```

### Asset Slot System

The engine maintains up to 32 WAD "slots" in a fixed array at 0xE55E90
(3 DWORDs per slot = 96 entries total). Each loaded WAD occupies one slot.
The slot stores:
- Pointer to loaded AWAD data
- Streaming handle
- Asset count/metadata

Assets are resolved at runtime by hashing the name and binary-searching
across all loaded WADs.

---

## World Loading Pipeline

### Entry Point

`ClWorld_LoadFromFile` (0x518C20) — called from `WorldViewer_Init` (0x490770).

### Path Construction

```c
// WorldViewer_Init builds the path:
const char* world_name = ReadGameData("VIEWER_WORLD");  // e.g., "Shell"
sprintf(path, "Worlds\\%s.%s", world_name, "pcw");       // "Worlds\Shell.pcw"
```

World extension is hardcoded at `g_worldFileExt` (0x6301A4) = `"pcw"`.
No alternative extensions are searched (unlike WADs).

### Loading Flow

```
ClWorld_LoadFromFile(path)
│
├── Open file via streaming I/O
│   └── Error? → "ClWorld: *** Error: File not found ***"
│
├── Read first 4KB into buffer
│
├── Check for ZLIB wrapper:
│   ├── memcmp(buffer, "ZLIB", 4) == 0?
│   │   ├── YES: Allocate decompression buffer
│   │   │        zlib_inflate() → raw PCWB data
│   │   │        (streaming path: async block-based decompression)
│   │   │
│   │   └── NO:  Use buffer as-is (raw PCWB expected)
│   │
├── Validate PCWB:
│   ├── PCWB_ValidateMagic (0x518260): bytes == "PCWB"
│   │   └── Fail → "ClWorld: *** Error: Invalid magic number ***"
│   │
│   ├── Version check: header[1] == 10
│   │   └── Fail → "ClWorld: *** Error: Version mismatch (got v%d, expected v%d) ***"
│   │
│   └── PCWB_ValidateAlignment (0x518280):
│       │   page_size is power of 2
│       │   offsets at +0C/+10/+14/+18 divisible by page_size
│       └── Fail → "ClWorld: *** Error: Invalid alignment ***"
│
├── Parse world structure:
│   ├── Register textures: "ClWorld: Registering textures..."
│   ├── Parse sectors: "ClWorld: Sectoring: %d sectors, %d portals"
│   ├── Parse props: "ClWorld: Static: %d props, %d prop definitions"
│   └── Load geometry: "ClWorld: Counts: %d geometry instances"
│
└── Activate initial sector
```

### Streaming vs Immediate

| Mode | Trigger | Method |
|------|---------|--------|
| Streaming | ZLIB wrapper present | Async block decompression via streaming pool |
| Immediate | Raw PCWB (no ZLIB) | Synchronous file read |

The streaming path uses `StreamingIO_CalcAddress` and engine tick-based chunk loading.
The immediate path reads the whole file in one blocking call.

---

## Compression Formats

Both WADs and worlds use the same zlib compression, but with different outer wrappers:

| Container | Magic | Used By | Decompresses To |
|-----------|-------|---------|-----------------|
| SFZC | `"SFZC"` | `.zwd` files | AWAD archive |
| ZLIB | `"ZLIB"` | `.pcw` files | PCWB world |

Both share identical structure: `magic(4) + compressed_size(u32) + decompressed_size(u32) + zlib_data`.

The engine uses standard zlib inflate (linked `inflate` at 0x6C57C9 in CRT).

---

## Modding Summary

| Goal | Approach | Notes |
|------|----------|-------|
| Replace WAD assets | Repack into `.zwd` or `.wad` | `.wad` = raw AWAD (no compression) |
| Add WAD override | Place `.wad` alongside `.zwd` | Must REMOVE `.zwd` since it has priority |
| Replace world | Replace `.pcw` file | Compressed or uncompressed both work |
| Loose file override | **Not supported** | Engine requires AWAD/PCWB containers |
| Hook search order | ASI mod | Could intercept `SfAssetRepository_Load` |

### What a Repacker Needs

To modify individual assets (e.g., replace one texture):
1. Unpack `.zwd` → extract all AWAD entries
2. Replace the target asset (e.g., swap a .dds file within PCIM wrapper)
3. Rebuild AWAD with correct hash-indexed TOC
4. Optionally compress with SFZC wrapper → `.zwd`, or save raw → `.wad`

---

## Full IDA Rename Summary

| Address | Name | Type |
|---------|------|------|
| 0x52B0E8 | SfAssetRepository_Load | Function |
| 0x52A520 | SfAssetRepo_LoadRawFile | Function |
| 0x52A610 | SfAssetRepo_LoadCompressed | Function |
| 0x52A330 | SfAssetRepo_SetSearchPath | Function |
| 0x518C20 | ClWorld_LoadFromFile | Function |
| 0x518260 | PCWB_ValidateMagic | Function |
| 0x518280 | PCWB_ValidateAlignment | Function |
| 0x490770 | WorldViewer_Init | Function |
| 0x58BCF0 | StreamCursor_AlignTo | Function |
| 0x58BD20 | StreamCursor_Advance | Function |
| 0x63EC3C | g_assetSearchTable | Global |
| 0x6301A4 | g_worldFileExt | Global |

---

## Related

- [ZWD_FORMAT.md](ZWD_FORMAT.md) — WAD archive binary format
- [PCW_FORMAT.md](PCW_FORMAT.md) — World file binary format
- `tools/spiderwick_unpack.py` — Extraction tool
- Engine console messages: grep for "SfAssetRepository:" and "ClWorld:" in strings
