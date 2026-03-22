#!/usr/bin/env python3
"""
Spiderwick Chronicles Asset Unpacker v2
========================================
Extracts and converts .zwd (WAD) and .pcw (world) archives from
"The Spiderwick Chronicles" (PC, 2008, Stormfront Studios / Ogre engine).

Conversions:
  PCIM  → .dds   (DDS texture, auto-extracted from 193-byte wrapper)
  NM40  → .obj   (Wavefront OBJ mesh with positions, normals, UVs)
  STTL  → .json  (settings/lookup table as key-value JSON)
  DBDB  → .json  (binary database → JSON with resolved field names)
  Devi  → .txt   (plain text device/controller config)
  enum  → .txt   (plain text C-style enum definitions)
  // *  → .txt   (plain text script source code)
  adat  → .adat  (binary animation data)
  aniz  → .aniz  (binary compressed animation)
  AMAP  → .amap  (binary animation map)
  WF    → .json  (widget font glyph atlas → JSON with names, UVs, + DDS textures)

Assets are named using recovered developer names where possible (via hash matching),
falling back to hash-based names for unresolved assets.

Usage:
  python spiderwick_unpack.py <file_or_dir> [output_dir]
  python spiderwick_unpack.py ../../ww/Wads/Common.zwd
  python spiderwick_unpack.py ../../ww/                   # batch all .zwd + .pcw
"""

import sys
import os
import struct
import zlib
import json
import argparse
from pathlib import Path
from collections import Counter


# ---------------------------------------------------------------------------
# Hash function — engine's HashString (0x405380)
# ---------------------------------------------------------------------------
def hash_string(s):
    """Reimplementation of the engine's HashString function."""
    result = 0
    for c in (s.encode("ascii") if isinstance(s, str) else s):
        shift = c & 7
        result = (result + c + ((result << shift) & 0xFFFFFFFF)) & 0xFFFFFFFF
    return result


# ---------------------------------------------------------------------------
# Name dictionary — maps hash → developer-intended asset name
# Built by hashing candidate strings from exe, scripts, and patterns
# ---------------------------------------------------------------------------
def build_name_dictionary():
    """Build hash→name dictionary from all available sources."""
    names = {}
    candidates = set()

    # Known names from RE (confirmed matches)
    known = {
        0x00303D95: "taskdb", 0x00E1FAD0: "propdb", 0x01F10836: "animid",
        0x0330BCD2: "spritemotiondb", 0x0A8180A1: "itemdb",
        0x0C47ABAE: "pickupdb", 0x0FBC1A19: "weapondb",
        0x0FDF0410: "spritedb", 0x130D53D3: "conversationdb",
        0x19E605BA: "spritecapturedb", 0x1E4181A7: "spritepowerdb",
        0x1F2CD0DB: "charactermovedb", 0x20E3FBAD: "cameradb",
        0x22AC2F28: "voicelogicdb", 0x2520BD00: "attackdb",
        0x2946B619: "voice_logic_characters", 0x2BF718F4: "fieldguidedb",
        0x33B614EA: "behavior_types", 0x3EF86830: "linkedanimdb",
        0x48ED9C0D: "projectiledb", 0x529312A6: "projectile_types",
        0x5325B157: "audioduckingdb", 0x5A0764F1: "cursor_wii_pc",
        0x5CA9C4AA: "nav_types", 0x5E7B59CA: "moodid",
        0x62786A68: "questdb", 0x6E89773E: "enemydb",
        0x7AFDB414: "ai_inputs", 0x814B23BF: "difficultydb",
        0x8C1A4667: "audiohookdb", 0x8DB14879: "thumbstickfront",
        0x8DFF41CE: "ai_analog_inputs", 0x913F7FCD: "autotraversedb",
        0x9F96F1EC: "note_track_types", 0xA570672E: "leveldb",
        0xA7CD2C98: "action_types", 0xCCE105B4: "guidelayoutdb",
        0xDDA128FD: "adaptivemusicdb", 0xE7F1F140: "configdb",
        0xEA29F4B9: "soundbankdb", 0xF81D2CFD: "voice_logic_lines",
        0xFF163215: "thumbstickback",
        # Icons and UI
        0x34488E45: "tomatobombicon", 0x3F554B33: "fieldguideicon",
        0x45918920: "ballbearingicon", 0x471F0ABD: "mtspritepage",
        0x473D5776: "fairyfruiticon", 0x5168FFDE: "rockicon",
        0x73854F04: "gobstoneicon", 0xA4715772: "checkbox_icon",
        0xBC453C61: "notepage", 0xC693FDB2: "checkmark_icon",
        0xF2B4363F: "goblintoothicon", 0xF3174EC1: "mansionhealthbar",
        0xA2300CE1: "tomatotimer",
        # Creatures/characters
        0x579A9040: "willowisp", 0x85ED93BA: "salamander",
        0x88944C22: "sproutsprite", 0x6F57CF67: "straysod",
        0x07F84978: "level",
    }
    names.update(known)

    # Try to load extended dictionary if available
    try:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        names_file = os.path.join(script_dir, "asset_names.py")
        if os.path.exists(names_file):
            ns = {}
            exec(open(names_file).read(), ns)
            if "ASSET_NAMES" in ns:
                names.update(ns["ASSET_NAMES"])
    except Exception:
        pass

    # Generate additional candidates from common patterns
    prefixes = [
        "", "mt", "hud", "ui_", "fx_", "sfx_", "env_", "prop_", "char_",
        "npc_", "item_", "wep_", "anim_", "cam_", "icon_", "page_",
        "bar_", "timer_", "sprite_", "menu_", "cursor_", "button_",
        "bg_", "thumb_", "check_", "goblin", "jared", "simon", "mallory",
        "thimbletack",
    ]
    bases = [
        "health", "mana", "ammo", "score", "map", "compass", "quest",
        "inventory", "dialog", "target", "crosshair", "reticle",
        "front", "back", "left", "right", "top", "bottom", "center",
        "normal", "diffuse", "specular", "shadow", "light", "dark",
        "attack", "defend", "dodge", "jump", "run", "walk", "idle",
        "goblin", "sprite", "fairy", "bird", "tree", "rock", "grass",
        "door", "gate", "wall", "floor", "roof", "window", "stairs",
    ]
    for p in prefixes:
        for b in bases:
            candidates.add(f"{p}{b}")

    for c in candidates:
        h = hash_string(c)
        if h not in names:
            names[h] = c

    return names


# Global name dictionary
NAMES = {}


def get_asset_name(name_hash):
    """Look up developer name for a hash, or return hex string."""
    return NAMES.get(name_hash)


# ---------------------------------------------------------------------------
# Format helpers
# ---------------------------------------------------------------------------
MAGIC_EXT = {
    # Reversed formats
    b"PCIM": ".pcim", b"DBDB": ".dbdb", b"STTL": ".sttl",
    b"Devi": ".device.txt", b"enum": ".enum.txt",
    b"AMAP": ".amap", b"NM40": ".nm40",
    b"adat": ".adat", b"aniz": ".aniz", b"PCRD": ".pcrd",
    # Unreversed/partially reversed formats — extract with descriptive extensions
    b"SCT\x00": ".sct",       # Kallis VM compiled script (bytecode)
    b"PCPB": ".pcpb",         # prop batch / physics body
    b"NAVM": ".navm",         # navigation mesh
    b"brxb": ".brxb",         # binary resource (unknown)
    b"arpc": ".arpc",         # animation RPC / action
    b"play": ".play",         # playback / playlist data
    b"Char": ".char",         # character definition
    b"Worl": ".world",        # world configuration
    b"EPC\x00": ".epc",       # unknown EPC format
    b"hier": ".hier",         # hierarchy / skeleton
    b"skel": ".skel",         # skeleton data
    b"PCWB": ".pcwb",         # world geometry (sub-archive)
    b"AWAD": ".awad",         # asset WAD (sub-archive)
}
TEXT_MAGICS = {b"Devi", b"enum"}


def read_u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def read_u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def read_f32(data, offset):
    return struct.unpack_from("<f", data, offset)[0]


# ---------------------------------------------------------------------------
# Outer wrapper decompression
# ---------------------------------------------------------------------------
def decompress_outer(filepath):
    with open(filepath, "rb") as f:
        magic = f.read(4)
        if magic not in (b"SFZC", b"ZLIB"):
            raise ValueError(f"Unknown outer magic: {magic!r}")
        compressed_size, decompressed_size = struct.unpack("<II", f.read(8))
        compressed_data = f.read()
    decompressed = zlib.decompress(compressed_data)
    return decompressed[:4], decompressed


# ---------------------------------------------------------------------------
# PCIM → DDS extraction
# ---------------------------------------------------------------------------
def extract_pcim_dds(data, pcim_offset, pcim_size):
    if pcim_offset + 0x14 > len(data):
        return None
    dds_ptr = read_u32(data, pcim_offset + 0x10)
    abs_off = pcim_offset + dds_ptr
    if 0 < dds_ptr < pcim_size and abs_off + 4 <= len(data):
        if data[abs_off : abs_off + 4] == b"DDS ":
            return abs_off
    if dds_ptr + 4 <= len(data) and data[dds_ptr : dds_ptr + 4] == b"DDS ":
        return dds_ptr
    return None


def pcim_dimensions(data, offset):
    """Read width x height from PCIM header."""
    if offset + 0xA4 <= len(data):
        w = read_u32(data, offset + 0x9C)
        h = read_u32(data, offset + 0xA0)
        if 0 < w <= 4096 and 0 < h <= 4096:
            return w, h
    return None, None


# ---------------------------------------------------------------------------
# NM40 → OBJ conversion
# ---------------------------------------------------------------------------
NM40_VERTEX_STRIDE = 52  # 13 floats: pos(3) + normal(3) + uv(2) + pad(5)


def convert_nm40_to_obj(data, obj_path):
    """Convert NM40 mesh to Wavefront OBJ."""
    if data[:4] != b"NM40" or len(data) < 0x40:
        return False

    # Find PCRD section
    pcrd_off = None
    for pos in range(0x40, min(len(data), 0x400), 4):
        if data[pos : pos + 4] == b"PCRD":
            pcrd_off = pos
            break

    if pcrd_off is None:
        return False

    idx_count = read_u32(data, pcrd_off + 12)
    vtx_count = read_u32(data, pcrd_off + 16)
    idx_off = read_u32(data, pcrd_off + 20)
    vtx_off = read_u32(data, pcrd_off + 24)

    if vtx_count == 0 or idx_count == 0:
        return False
    if vtx_off + vtx_count * NM40_VERTEX_STRIDE > len(data):
        return False
    if idx_off + idx_count * 2 > len(data):
        return False

    lines = [f"# Spiderwick Chronicles NM40 mesh", f"# {vtx_count} vertices, {idx_count // 3} triangles", ""]

    # Vertices
    for i in range(vtx_count):
        off = vtx_off + i * NM40_VERTEX_STRIDE
        x, y, z = struct.unpack_from("<fff", data, off)
        lines.append(f"v {x:.6f} {y:.6f} {z:.6f}")

    lines.append("")

    # Normals
    for i in range(vtx_count):
        off = vtx_off + i * NM40_VERTEX_STRIDE + 12
        nx, ny, nz = struct.unpack_from("<fff", data, off)
        lines.append(f"vn {nx:.6f} {ny:.6f} {nz:.6f}")

    lines.append("")

    # UVs
    for i in range(vtx_count):
        off = vtx_off + i * NM40_VERTEX_STRIDE + 24
        u, v = struct.unpack_from("<ff", data, off)
        lines.append(f"vt {u:.6f} {1.0 - v:.6f}")  # flip V for OBJ convention

    lines.append("")
    lines.append("g mesh")

    # Faces (1-indexed in OBJ)
    for i in range(0, idx_count, 3):
        i0 = read_u16(data, idx_off + i * 2) + 1
        i1 = read_u16(data, idx_off + (i + 1) * 2) + 1
        i2 = read_u16(data, idx_off + (i + 2) * 2) + 1
        lines.append(f"f {i0}/{i0}/{i0} {i1}/{i1}/{i1} {i2}/{i2}/{i2}")

    with open(obj_path, "w") as f:
        f.write("\n".join(lines))
    return True


# ---------------------------------------------------------------------------
# STTL → JSON conversion
# ---------------------------------------------------------------------------
def convert_sttl_to_json(data, offset, size, json_path):
    """Convert STTL settings table to JSON."""
    if data[offset : offset + 4] != b"STTL" or size < 16:
        return False

    ver = read_u32(data, offset + 4)
    count = read_u32(data, offset + 8)
    total = read_u32(data, offset + 12)

    entries = {}
    entry_off = offset + 16
    # Each entry: hash(4) + 3 floats(12) = 16 bytes
    for i in range(count):
        if entry_off + 16 > offset + size:
            break
        h = read_u32(data, entry_off)
        v1 = read_f32(data, entry_off + 4)
        v2 = read_f32(data, entry_off + 8)
        v3 = read_f32(data, entry_off + 12)
        name = NAMES.get(h, f"0x{h:08X}")
        entries[name] = [round(v1, 4), round(v2, 4), round(v3, 4)]
        entry_off += 16

    obj = {"format": "STTL", "version": ver, "entries": entries}
    with open(json_path, "w") as f:
        json.dump(obj, f, indent=2)
    return True


# ---------------------------------------------------------------------------
# DBDB → JSON conversion
# ---------------------------------------------------------------------------
def _build_dbdb_field_dict():
    """Build hash→name dictionary for DBDB field names."""
    def _h(s):
        r = 0
        for c in s.encode("ascii"):
            r = (r + c + ((r << (c & 7)) & 0xFFFFFFFF)) & 0xFFFFFFFF
        return r

    # Known field names from IDA (AttackDB, EnemyDB, ItemDB, WeaponDB loaders etc.)
    names = [
        "NAME","ID","TYPE","DESCRIPTION","VALUE","COUNT","LEVEL","HEALTH","SPEED",
        "WEIGHT","COST","DURATION","RANGE","RADIUS","ANGLE","HEIGHT","WIDTH","SCALE",
        "DAMAGE","IMPACT_PFX","AOE_RADIUS","AOE_FOV","AOE_DAMAGE","AOE_DIRECTION",
        "ATTACK_MASK","META_ATTACK_MASK","AOE_ATTACK_MASK","BLOCK_LEVEL_INCREMENT",
        "STUN_TIME","INPUTS","ON_HIT_SFX","DAMAGES_FRIENDS","DAMAGES_ENEMIES",
        "DOT_COUNT","DOT_INTERVAL","DOT_PFX","ATTACK","DEFENSE","ARMOR",
        "PRIORITY","FLAGS","STATE","MODE","INDEX","GROUP","CATEGORY","ENABLED",
        "ICON","MESH","ANIM","SOUND","EFFECT","MATERIAL","TEXTURE","COLOR","ALPHA",
        "POSITION","ROTATION","DIRECTION","TARGET","SOURCE","SIZE","OFFSET",
        "SPAWN","PICKUP","QUEST","OBJECTIVE","REWARD","ITEM","SLOT","PROJECTILE",
        "VELOCITY","GRAVITY","MASS","FORCE","FRICTION","BOUNCE",
        "ANIMATION","FRAME","BLEND","LOOP","TRANSITION","COMBO","CRITICAL",
        "KNOCKBACK","STAGGER","BLOCK","DODGE","PARRY","VOLUME","PITCH",
        "BANK_ID","CUE_ID","FADE_IN","FADE_OUT","DISTANCE","FALLOFF",
        "HIT_POINTS","MAX_HIT_POINTS","MANA","STAMINA","ENERGY","POWER",
        "ATTACK_SPEED","SWING_SPEED","SWING_RANGE","COOLDOWN","DELAY","RATE",
        "MIN","MAX","LABEL","TITLE","TEXT","TAG","LINK","REF","DATA","INFO",
    ]

    # Also read field names from strings_full.txt if available
    import re, os
    strings_path = os.path.join(os.path.dirname(__file__), "strings_full.txt")
    if not os.path.exists(strings_path):
        strings_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     "..", "strings_full.txt")
    if os.path.exists(strings_path):
        with open(strings_path, "r", errors="ignore") as f:
            for line in f:
                for m in re.findall(r"\b[A-Z][A-Z0-9_]{2,40}\b", line):
                    names.append(m)

    d = {}
    for n in set(names):
        d.setdefault(_h(n), n)
    return d

_DBDB_FIELDS = None

def convert_dbdb_to_json(data, offset, size, json_path):
    """Convert DBDB binary database to JSON with resolved field names."""
    global _DBDB_FIELDS
    if _DBDB_FIELDS is None:
        _DBDB_FIELDS = _build_dbdb_field_dict()

    if data[offset:offset+4] != b"DBDB" or size < 0x20:
        return False

    rec_count = read_u32(data, offset + 0x0C)
    data_start = read_u32(data, offset + 0x18)

    records = []
    pos = offset + 0x20

    for _ in range(rec_count):
        if pos + 4 > offset + size:
            break
        fc = read_u32(data, pos)
        pos += 4
        if fc > 200 or pos + fc * 8 > offset + size:
            break

        record = {}
        for _ in range(fc):
            fhash = read_u32(data, pos)
            fval_raw = read_u32(data, pos + 4)
            pos += 8

            fname = _DBDB_FIELDS.get(fhash, f"0x{fhash:08X}")

            # Determine type: string offset or float
            if (fval_raw >= data_start and fval_raw < offset + size and
                    fval_raw + offset < len(data)):
                # Check if valid ASCII string at absolute offset
                abs_off = fval_raw  # data_start is already absolute within the asset
                # In AWAD, offsets are relative to asset start
                # Try both interpretations
                str_val = None
                for try_off in [offset + fval_raw, fval_raw]:
                    if 0 <= try_off < len(data):
                        end = try_off
                        while end < len(data) and data[end] != 0:
                            end += 1
                        if end > try_off and all(32 <= data[j] < 127 for j in range(try_off, min(end, try_off + 200))):
                            str_val = data[try_off:end].decode("ascii")
                            break

                if str_val is not None:
                    record[fname] = str_val
                else:
                    record[fname] = struct.unpack("<f", struct.pack("<I", fval_raw))[0]
            else:
                fval = struct.unpack("<f", struct.pack("<I", fval_raw))[0]
                # Round clean integers
                if fval == int(fval) and abs(fval) < 1e7:
                    record[fname] = int(fval)
                else:
                    record[fname] = round(fval, 6)

        records.append(record)

    obj = {"format": "DBDB", "version": read_u32(data, offset + 4),
           "record_count": rec_count, "records": records}

    with open(json_path, "w") as f:
        json.dump(obj, f, indent=2, ensure_ascii=False)
    return True


# ---------------------------------------------------------------------------
# WF (Widget Font) → JSON + DDS conversion
# ---------------------------------------------------------------------------
def convert_wf_to_json(data, offset, size, json_path, out_dir):
    """Convert WF widget font to JSON glyph table + extract DDS atlas textures."""
    if data[offset:offset+2] != b"WF" or size < 0x44:
        return False

    ver = struct.unpack_from("<H", data, offset + 2)[0]
    named_count = struct.unpack_from("<H", data, offset + 0x10)[0]
    table_count = struct.unpack_from("<H", data, offset + 0x12)[0]
    glyph_off = struct.unpack_from("<I", data, offset + 0x14)[0]
    name_off = struct.unpack_from("<I", data, offset + 0x18)[0]
    tex1_off = struct.unpack_from("<I", data, offset + 0x1C)[0]
    tex0_off = struct.unpack_from("<I", data, offset + 0x20)[0]

    # Parse named glyph records (stride 0x5C = 92 bytes, name at +0x20 UTF-16LE)
    glyphs = []
    for i in range(named_count):
        rec = offset + name_off + i * 0x5C
        if rec + 0x5C > offset + size:
            break

        name_raw = data[rec + 0x20:rec + 0x40]
        name = name_raw.decode("utf-16-le", errors="replace").rstrip("\x00")
        name = "".join(c for c in name if 32 <= ord(c) < 127)

        u1 = struct.unpack_from("<f", data, rec + 0x08)[0]
        v1 = struct.unpack_from("<f", data, rec + 0x0C)[0]
        flags = struct.unpack_from("<I", data, rec + 0x50)[0]

        if name:
            glyphs.append({"name": name, "u": round(u1, 6), "v": round(v1, 6),
                           "flags": flags, "index": i})

    # Extract PCIM texture atlases
    textures = []
    for label, toff in [("atlas_0", tex0_off), ("atlas_1", tex1_off)]:
        abs_off = offset + toff
        if abs_off + 0xC1 > len(data) or data[abs_off:abs_off + 4] != b"PCIM":
            continue
        dsz = read_u32(data, abs_off + 0x0C)
        w = read_u32(data, abs_off + 0x9C)
        h = read_u32(data, abs_off + 0xA0)
        dds_off = abs_off + 0xC1
        if dds_off + dsz <= len(data) and data[dds_off:dds_off + 4] == b"DDS ":
            dds_name = os.path.basename(json_path).replace(".json", f"_{label}_{w}x{h}.dds")
            with open(os.path.join(out_dir, dds_name), "wb") as f:
                f.write(data[dds_off:dds_off + dsz])
            textures.append({"name": label, "width": w, "height": h, "file": dds_name})

    obj = {"format": "WF (Widget Font)", "version": ver,
           "named_glyph_count": named_count, "table_entries": table_count,
           "textures": textures, "glyphs": glyphs}

    with open(json_path, "w") as f:
        json.dump(obj, f, indent=2, ensure_ascii=False)
    return True


# ---------------------------------------------------------------------------
# Text name extraction from content
# ---------------------------------------------------------------------------
def extract_name_from_text(data, offset, size):
    text = data[offset : offset + min(size, 512)]
    try:
        text_str = text.decode("ascii", errors="replace")
    except Exception:
        return None

    if text_str.startswith("enum "):
        name = text_str[5:].split("\r")[0].split("\n")[0].split("{")[0].strip()
        if name and len(name) < 80:
            return "".join(c for c in name if c.isalnum() or c == "_")

    if text_str.startswith("Device["):
        idx = text_str.find('Name = "')
        if idx >= 0:
            end = text_str.find('"', idx + 8)
            if end > idx + 8:
                name = text_str[idx + 8 : end]
                return "".join(c for c in name if c.isalnum() or c in "_ -")

    if text_str.startswith("// "):
        line = text_str[3:].split("\r")[0].split("\n")[0].strip()
        words = line.split()
        if len(words) >= 2:
            name = "_".join(words[:4])[:60]
            return "".join(c for c in name if c.isalnum() or c == "_")

    return None


# ---------------------------------------------------------------------------
# Asset format detection
# ---------------------------------------------------------------------------
def detect_asset_ext(data, offset, size):
    magic4 = data[offset : offset + 4]
    if magic4 in MAGIC_EXT:
        return MAGIC_EXT[magic4], magic4 in TEXT_MAGICS
    if magic4[:2] == b"//":
        return ".script.txt", True
    if magic4[:2] == b"WF":
        return ".wf", False
    # Detect by first byte patterns
    b0 = magic4[0] if len(magic4) > 0 else 0
    if b0 == 0xA1 and size > 16:
        return ".anim_a1", False      # animation data (0xA1 prefix)
    if b0 == 0xAC and size > 16:
        return ".anim_ac", False      # animation data (0xAC prefix)
    # Check for SCT with non-null-terminated magic
    if magic4[:3] == b"SCT":
        return ".sct", False
    # Fallback: save as .bin with magic bytes in name for future identification
    return ".bin", False


# ---------------------------------------------------------------------------
# Build filename with proper name
# ---------------------------------------------------------------------------
def build_filename(name_hash, ext, data=None, offset=0, size=0, is_text=False):
    """Build filename: prefer developer name, fall back to hash."""
    dev_name = get_asset_name(name_hash)

    # For text assets, try extracting name from content
    friendly = None
    if (is_text or ext == ".script.txt") and data:
        friendly = extract_name_from_text(data, offset, size)

    # For PCIM, add dimensions
    dim_str = ""
    if ext == ".pcim" and data and offset + 0xA4 <= len(data):
        w, h = pcim_dimensions(data, offset)
        if w and h:
            dim_str = f"_{w}x{h}"

    if dev_name:
        return f"{dev_name}{dim_str}{ext}"
    elif friendly:
        return f"{name_hash:08x}_{friendly}{dim_str}{ext}"
    else:
        return f"{name_hash:08x}{dim_str}{ext}"


# ---------------------------------------------------------------------------
# AWAD extraction
# ---------------------------------------------------------------------------
def unpack_awad(data, out_dir, convert=True):
    if data[:4] != b"AWAD":
        raise ValueError(f"Expected AWAD, got {data[:4]!r}")

    version = read_u32(data, 4)
    count = read_u32(data, 8)
    print(f"  AWAD v{version}, {count} entries")

    toc1_base = 12
    entries = []
    for i in range(count):
        name_hash = read_u32(data, toc1_base + i * 8)
        entry_ptr = read_u32(data, toc1_base + i * 8 + 4)
        type_hash = read_u32(data, entry_ptr)
        data_offset = read_u32(data, entry_ptr + 4)
        entries.append((name_hash, type_hash, data_offset, i))

    entries_sorted = sorted(entries, key=lambda e: e[2])
    type_counts = Counter()
    conv_counts = Counter()

    os.makedirs(out_dir, exist_ok=True)

    for idx, (name_hash, type_hash, offset, orig_idx) in enumerate(entries_sorted):
        next_off = entries_sorted[idx + 1][2] if idx + 1 < len(entries_sorted) else len(data)
        size = next_off - offset

        ext, is_text = detect_asset_ext(data, offset, size)
        magic4 = data[offset : offset + 4]
        type_counts[ext] += 1

        filename = build_filename(name_hash, ext, data, offset, size, is_text)

        # Write raw asset
        asset_path = os.path.join(out_dir, filename)
        with open(asset_path, "wb") as f:
            f.write(data[offset : offset + size])

        if not convert:
            continue

        # --- Conversions ---

        # PCIM → DDS
        if magic4 == b"PCIM" and size > 0x14:
            dds_abs = extract_pcim_dds(data, offset, size)
            if dds_abs is not None:
                dds_size = read_u32(data, offset + 0x0C)
                dds_name = filename.replace(".pcim", ".dds")
                with open(os.path.join(out_dir, dds_name), "wb") as f:
                    f.write(data[dds_abs : dds_abs + dds_size])
                conv_counts["dds"] += 1

        # NM40 → OBJ
        elif magic4 == b"NM40":
            obj_name = filename.replace(".nm40", ".obj")
            if convert_nm40_to_obj(data[offset : offset + size], os.path.join(out_dir, obj_name)):
                conv_counts["obj"] += 1

        # STTL → JSON
        elif magic4 == b"STTL":
            json_name = filename.replace(".sttl", ".json")
            if convert_sttl_to_json(data, offset, size, os.path.join(out_dir, json_name)):
                conv_counts["json"] += 1

        # DBDB → JSON
        elif magic4 == b"DBDB":
            json_name = filename.replace(".dbdb", ".json")
            if convert_dbdb_to_json(data, offset, size, os.path.join(out_dir, json_name)):
                conv_counts["json"] += 1

        # WF → JSON + DDS
        elif magic4[:2] == b"WF":
            json_name = filename.replace(".wf", "_font.json")
            if convert_wf_to_json(data, offset, size, os.path.join(out_dir, json_name), out_dir):
                conv_counts["json"] += 1

    named = sum(1 for nh, _, _, _ in entries if get_asset_name(nh))
    print(f"  Extracted {count} assets: {dict(type_counts.most_common())}")
    if conv_counts:
        print(f"  Converted: {dict(conv_counts)}")
    if named:
        print(f"  Named: {named}/{count} assets have developer names")


# ---------------------------------------------------------------------------
# PCWB extraction
# ---------------------------------------------------------------------------
def unpack_pcwb(data, out_dir, convert=True):
    if data[:4] != b"PCWB":
        raise ValueError(f"Expected PCWB, got {data[:4]!r}")

    version = read_u32(data, 4)
    print(f"  PCWB v{version}, {len(data):,} bytes")

    os.makedirs(out_dir, exist_ok=True)

    world_path = os.path.join(out_dir, "world.pcwb")
    with open(world_path, "wb") as f:
        f.write(data)

    dds_count = 0
    pcrd_count = 0
    for pos in range(0, len(data), 0x1000):
        magic = data[pos : pos + 4]

        if magic == b"PCIM" and pos + 0x14 <= len(data):
            dds_abs = extract_pcim_dds(data, pos, len(data) - pos)
            if dds_abs is not None:
                dds_size = read_u32(data, pos + 0x0C)
                w, h = pcim_dimensions(data, pos)
                dim = f"_{w}x{h}" if w else ""
                dds_path = os.path.join(out_dir, f"texture_{dds_count:03d}{dim}.dds")
                with open(dds_path, "wb") as f:
                    f.write(data[dds_abs : dds_abs + dds_size])
                dds_count += 1
        elif magic == b"PCRD":
            pcrd_count += 1

    print(f"  Saved world.pcwb + {dds_count} textures, {pcrd_count} PCRD sections")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def process_file(filepath, output_base, convert=True):
    filepath = Path(filepath)
    print(f"\n{'='*60}")
    print(f"Processing: {filepath.name} ({filepath.stat().st_size:,} bytes)")

    try:
        inner_magic, decompressed = decompress_outer(str(filepath))
    except Exception as e:
        print(f"  ERROR: {e}")
        return False

    out_dir = os.path.join(output_base, filepath.stem)

    if inner_magic == b"AWAD":
        unpack_awad(decompressed, out_dir, convert=convert)
    elif inner_magic == b"PCWB":
        unpack_pcwb(decompressed, out_dir, convert=convert)
    else:
        os.makedirs(out_dir, exist_ok=True)
        with open(os.path.join(out_dir, f"{filepath.stem}.bin"), "wb") as f:
            f.write(decompressed)
        print(f"  Unknown format {inner_magic!r}, saved raw")

    return True


def main():
    global NAMES

    parser = argparse.ArgumentParser(description="Spiderwick Chronicles asset unpacker v2")
    parser.add_argument("input", help="Input .zwd/.pcw file or directory")
    parser.add_argument("output", nargs="?", help="Output directory")
    parser.add_argument("--no-convert", action="store_true", help="Skip format conversions")
    parser.add_argument("--raw", action="store_true", help="Raw extraction only (no DDS/OBJ/JSON)")
    args = parser.parse_args()

    # Build name dictionary
    NAMES = build_name_dictionary()
    print(f"Name dictionary: {len(NAMES)} entries")

    input_path = Path(args.input)
    if args.output:
        output_base = args.output
    elif input_path.is_file():
        output_base = str(input_path.parent / (input_path.stem + "_unpacked"))
    else:
        output_base = str(input_path) + "_unpacked"

    files = []
    if input_path.is_file():
        files = [input_path]
    elif input_path.is_dir():
        files = sorted(input_path.rglob("*.zwd")) + sorted(input_path.rglob("*.pcw"))

    if not files:
        print("No .zwd or .pcw files found.")
        sys.exit(1)

    print(f"Found {len(files)} archive(s)")
    print(f"Output: {output_base}")

    ok = sum(1 for f in files if process_file(f, output_base, convert=not args.raw))
    print(f"\n{'='*60}")
    print(f"Done. {ok}/{len(files)} files processed.")


if __name__ == "__main__":
    main()
