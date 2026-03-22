# WF — Widget Font (Glyph Atlas) Format

**Status:** Fully reversed. JSON + DDS converter integrated into `spiderwick_unpack.py`.
**NOT audio** — WF = Widget Font, a UI glyph atlas format.

---

## Structure

```
Header
  +0x00: "WF" magic (2 bytes)
  +0x02: version/flags
  ...
Offset Table
  Array of offsets to glyph entries
Glyph Entries (28 bytes each)
  UV coordinates, dimensions, character mapping
Name Records (stride 0x5C = 92 bytes each)
  Glyph names as fixed-length strings
PCIM Texture Atlases (2 per file)
  Embedded PCIM textures containing the actual glyph graphics
```

## Known Files

2 WF files in Common.zwd:
- **38-glyph atlas**: controller + mouse + keyboard icons (Button_X, Button_SQ, PCMS_ButtonLft, up/down/left/right, etc.)
- **26-glyph atlas**: controller-only icons

## Glyph Names (examples)

Button_X, Button_SQ, Button_TR, Button_CI, PCMS_ButtonLft, PCMS_ButtonRt,
up, down, left, right, select, start, L1, L2, R1, R2, LS, RS

## Tool

`spiderwick_unpack.py` auto-converts WF → JSON (glyph names + UV coordinates) + DDS (texture atlases).
