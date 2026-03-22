#!/usr/bin/env python3
"""
Spiderwick Chronicles — World Level glTF Exporter v2
======================================================
Exports decompressed PCWB world files as .glb with:
  - Per-PCRD-chunk materials (separate primitives)
  - Correct texture assignment from batch table (triple-FFF pattern)
  - Embedded DDS->PNG textures in glTF materials
  - Vertex colors (baked lighting) and UVs

Each PCRD chunk becomes its own primitive with its own material.
In Blender: one mesh, many materials, correct UVs+textures.

Usage:
  python spiderwick_world_glb.py <world.pcwb> [output.glb]
  python spiderwick_world_glb.py --batch <wad_dir>   # all worlds in dir
"""

import struct
import sys
import os
import json
import math
import argparse
import io

# DDS -> PNG conversion (try Pillow, fallback to raw embedding)
try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


def read_u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def find_all_aligned(data, magic):
    results = []
    pos = 0
    while pos < len(data) - 3:
        if data[pos:pos+4] == magic:
            results.append(pos)
        pos += 4
    return results


# ---------------------------------------------------------------------------
# Build PCRD -> texture index mapping from batch entries
# ---------------------------------------------------------------------------
def build_pcrd_texture_map(data, pcrds):
    """Scan for triple-FFFFFFFF batch entries linking PCRDs to texture indices.

    Each batch entry has:
      - texture_index (u32) at FFF_offset - 4
      - PCRD offset (u32) at FFF_offset + 36
    """
    pcrd_set = set(pcrds)
    pcrd_to_tex = {}

    pos = 0
    while pos < len(data) - 12:
        if (read_u32(data, pos) == 0xFFFFFFFF and
            read_u32(data, pos + 4) == 0xFFFFFFFF and
            read_u32(data, pos + 8) == 0xFFFFFFFF):
            if pos >= 4 and pos + 40 <= len(data):
                tex_idx = read_u32(data, pos - 4)
                pcrd_ref = read_u32(data, pos + 36)
                if pcrd_ref in pcrd_set:
                    pcrd_to_tex[pcrd_ref] = tex_idx
            pos += 12
        else:
            pos += 4

    return pcrd_to_tex


# ---------------------------------------------------------------------------
# Extract DDS texture data from PCIM section
# ---------------------------------------------------------------------------
def extract_dds_from_pcim(data, pcim_off):
    """Extract raw DDS bytes from a PCIM section in PCWB."""
    if data[pcim_off:pcim_off+4] != b"PCIM":
        return None
    dds_size = read_u32(data, pcim_off + 0x0C)
    dds_off = read_u32(data, pcim_off + 0x10)

    if dds_off + dds_size > len(data):
        return None
    if data[dds_off:dds_off+4] != b"DDS ":
        # Try relative offset (0xC1 from PCIM start)
        dds_off = pcim_off + 0xC1
        if dds_off + dds_size > len(data) or data[dds_off:dds_off+4] != b"DDS ":
            return None

    return data[dds_off:dds_off + dds_size]


def dds_to_png_bytes(dds_data):
    """Convert DDS bytes to PNG bytes using Pillow. Returns None on failure."""
    if not HAS_PIL:
        return None
    try:
        img = Image.open(io.BytesIO(dds_data))
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        return buf.getvalue()
    except Exception:
        return None


def dds_to_raw_rgba(dds_data):
    """Extract raw uncompressed RGBA from DDS. Returns (width, height, rgba_bytes) or None."""
    if len(dds_data) < 128:
        return None
    # DDS header
    height = struct.unpack_from("<I", dds_data, 12)[0]
    width = struct.unpack_from("<I", dds_data, 16)[0]
    # Check if uncompressed (pitchOrLinearSize, pfFlags, fourCC)
    pf_flags = struct.unpack_from("<I", dds_data, 80)[0]
    # 0x41 = DDPF_RGB | DDPF_ALPHAPIXELS for RGBA
    rgb_bit_count = struct.unpack_from("<I", dds_data, 88)[0]

    if rgb_bit_count == 32 and (pf_flags & 0x40):  # DDPF_RGB
        pixel_data = dds_data[128:128 + width * height * 4]
        if len(pixel_data) == width * height * 4:
            # DDS stores BGRA, convert to RGBA
            rgba = bytearray(pixel_data)
            for i in range(0, len(rgba), 4):
                rgba[i], rgba[i+2] = rgba[i+2], rgba[i]
            return width, height, bytes(rgba)
    return None


def make_png_from_raw(width, height, rgba_data):
    """Create a minimal PNG from raw RGBA data without Pillow."""
    import zlib

    def write_chunk(chunk_type, chunk_data):
        c = chunk_type + chunk_data
        return struct.pack(">I", len(chunk_data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    # IHDR
    ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)  # 8-bit RGBA
    ihdr = write_chunk(b"IHDR", ihdr_data)

    # IDAT - raw pixel data with filter byte 0 per row
    raw_rows = b""
    stride = width * 4
    for y in range(height):
        raw_rows += b"\x00" + rgba_data[y * stride:(y + 1) * stride]
    compressed = zlib.compress(raw_rows)
    idat = write_chunk(b"IDAT", compressed)

    # IEND
    iend = write_chunk(b"IEND", b"")

    return b"\x89PNG\r\n\x1a\n" + ihdr + idat + iend


# ---------------------------------------------------------------------------
# Main export
# ---------------------------------------------------------------------------
def export_world(pcwb_path, output_path):
    with open(pcwb_path, "rb") as f:
        data = f.read()

    if data[:4] != b"PCWB":
        print(f"  Not a PCWB file: {pcwb_path}")
        return False

    # Find all sections
    pcrds = find_all_aligned(data, b"PCRD")
    pcims = find_all_aligned(data, b"PCIM")

    # Build batch table: PCRD offset -> texture index
    pcrd_tex_map = build_pcrd_texture_map(data, pcrds)

    # Extract unique textures referenced by PCRDs
    used_tex_indices = set(pcrd_tex_map.values())
    tex_index_to_pcim_off = {}
    for idx in used_tex_indices:
        if idx < len(pcims):
            tex_index_to_pcim_off[idx] = pcims[idx]

    # Convert DDS textures to PNG bytes
    tex_png_data = {}  # tex_index -> png bytes
    for tex_idx, pcim_off in sorted(tex_index_to_pcim_off.items()):
        dds = extract_dds_from_pcim(data, pcim_off)
        if dds is None:
            continue
        png = dds_to_png_bytes(dds)
        if png is None:
            # Try manual RGBA extraction
            raw = dds_to_raw_rgba(dds)
            if raw:
                w, h, rgba = raw
                png = make_png_from_raw(w, h, rgba)
        if png:
            tex_png_data[tex_idx] = png

    print(f"  Textures: {len(tex_png_data)}/{len(used_tex_indices)} converted to PNG")

    # Collect per-chunk geometry
    chunks = []  # list of {positions, uvs, colors, indices, tex_idx}
    total_verts = 0
    total_tris = 0

    for pcrd_off in pcrds:
        ver = read_u32(data, pcrd_off + 4)
        if ver != 2:
            continue

        hdr_size = read_u32(data, pcrd_off + 0x08)
        idx_count = read_u32(data, pcrd_off + 0x0C)
        vtx_count = read_u32(data, pcrd_off + 0x10)
        idx_offset = read_u32(data, pcrd_off + 0x14)
        vtx_offset = read_u32(data, pcrd_off + 0x18)

        stride = 32 if hdr_size <= 0x10 else 24

        # Validate first vertex
        def vtx_ok(s):
            off = vtx_offset
            if off + 12 > len(data):
                return False
            px, py, pz = struct.unpack_from("<fff", data, off)
            return all(not (math.isnan(v) or math.isinf(v) or abs(v) > 50000) for v in (px, py, pz))

        if not vtx_ok(stride):
            for alt in (28, 32, 36, 40, 24):
                if alt != stride and vtx_ok(alt):
                    stride = alt
                    break
            else:
                continue

        if vtx_count == 0 or idx_count == 0:
            continue
        if vtx_offset + vtx_count * stride > len(data):
            continue
        if idx_offset + idx_count * 2 > len(data):
            continue

        # Validate all vertices
        chunk_bad = False
        for i in range(vtx_count):
            off = vtx_offset + i * stride
            px, py, pz = struct.unpack_from("<fff", data, off)
            if any(math.isnan(v) or math.isinf(v) or abs(v) > 50000 for v in (px, py, pz)):
                chunk_bad = True
                break
        if chunk_bad:
            continue

        # Read vertices
        positions = []
        uvs = []
        colors = []
        for i in range(vtx_count):
            off = vtx_offset + i * stride
            px, py, pz = struct.unpack_from("<fff", data, off)
            r, g, b, a = data[off+12], data[off+13], data[off+14], data[off+15]
            u, v = struct.unpack_from("<ff", data, off + 16)
            positions.append((px, pz, -py))  # Y-up -> Z-up
            colors.append((r / 255.0, g / 255.0, b / 255.0, a / 255.0))
            uvs.append((u, v))

        # Read indices as triangle strip -> triangle list
        raw_indices = []
        for i in range(idx_count):
            raw_indices.append(struct.unpack_from("<H", data, idx_offset + i * 2)[0])

        tri_indices = []
        for i in range(len(raw_indices) - 2):
            i0, i1, i2 = raw_indices[i], raw_indices[i+1], raw_indices[i+2]
            if i0 == i1 or i1 == i2 or i0 == i2:
                continue
            if i % 2 == 0:
                tri_indices.extend([i0, i1, i2])
            else:
                tri_indices.extend([i1, i0, i2])

        if not tri_indices:
            continue

        tex_idx = pcrd_tex_map.get(pcrd_off, -1)
        chunks.append({
            'positions': positions,
            'uvs': uvs,
            'colors': colors,
            'indices': tri_indices,
            'tex_idx': tex_idx,
        })
        total_verts += vtx_count
        total_tris += len(tri_indices) // 3

    if not chunks:
        print(f"  No valid PCRD chunks found")
        return False

    print(f"  {len(chunks)} chunks, {total_verts:,} verts, {total_tris:,} tris, "
          f"{len(tex_png_data)} textures")

    # --- Build glTF 2.0 with per-chunk primitives ---

    # Group chunks by texture index for shared materials
    tex_to_mat_idx = {}  # tex_idx -> material index
    materials = []       # glTF material objects
    images = []          # glTF image objects
    textures = []        # glTF texture objects
    samplers = [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}]

    # Collect all PNG image data in order
    image_bin_data = []  # (png_bytes, ...)

    for tex_idx in sorted(tex_png_data.keys()):
        img_idx = len(images)
        mat_idx = len(materials)
        tex_to_mat_idx[tex_idx] = mat_idx

        image_bin_data.append(tex_png_data[tex_idx])
        images.append({
            "mimeType": "image/png",
            "bufferView": None,  # will be filled later
        })
        textures.append({
            "sampler": 0,
            "source": img_idx,
        })
        materials.append({
            "name": f"tex_{tex_idx}",
            "pbrMetallicRoughness": {
                "baseColorTexture": {"index": len(textures) - 1},
                "metallicFactor": 0.0,
                "roughnessFactor": 1.0,
            },
        })

    # Fallback material for unmapped chunks
    fallback_mat_idx = len(materials)
    materials.append({
        "name": "unmapped",
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.5, 0.5, 0.5, 1.0],
            "metallicFactor": 0.0,
            "roughnessFactor": 1.0,
        },
    })

    # Pack all chunk data into buffers and build accessors/bufferViews
    accessors = []
    buffer_views = []
    primitives = []
    all_bin = bytearray()

    def pad4(b):
        r = len(b) % 4
        return bytes(b) + b"\x00" * (4 - r) if r else bytes(b)

    def add_buffer_view(buf_bytes):
        """Add padded buffer data, return bufferView index."""
        bv_idx = len(buffer_views)
        offset = len(all_bin)
        padded = pad4(buf_bytes)
        all_bin.extend(padded)
        buffer_views.append({
            "buffer": 0,
            "byteOffset": offset,
            "byteLength": len(buf_bytes),
        })
        return bv_idx

    for chunk in chunks:
        positions = chunk['positions']
        uvs = chunk['uvs']
        colors = chunk['colors']
        indices = chunk['indices']
        tex_idx = chunk['tex_idx']
        vtx_count = len(positions)
        idx_count = len(indices)

        # Pack position buffer
        pos_buf = bytearray()
        for p in positions:
            pos_buf += struct.pack("<fff", *p)
        pos_bv = add_buffer_view(pos_buf)
        pos_mins = [min(p[i] for p in positions) for i in range(3)]
        pos_maxs = [max(p[i] for p in positions) for i in range(3)]
        pos_acc = len(accessors)
        accessors.append({
            "bufferView": pos_bv, "componentType": 5126,
            "count": vtx_count, "type": "VEC3",
            "min": pos_mins, "max": pos_maxs,
        })

        # Pack UV buffer
        uv_buf = bytearray()
        for u in uvs:
            uv_buf += struct.pack("<ff", *u)
        uv_bv = add_buffer_view(uv_buf)
        uv_acc = len(accessors)
        accessors.append({
            "bufferView": uv_bv, "componentType": 5126,
            "count": vtx_count, "type": "VEC2",
        })

        # Pack vertex color buffer
        col_buf = bytearray()
        for c in colors:
            col_buf += struct.pack("<ffff", *c)
        col_bv = add_buffer_view(col_buf)
        col_acc = len(accessors)
        accessors.append({
            "bufferView": col_bv, "componentType": 5126,
            "count": vtx_count, "type": "VEC4",
        })

        # Pack index buffer
        use_u32 = vtx_count > 65535
        if use_u32:
            idx_buf = bytearray()
            for i in indices:
                idx_buf += struct.pack("<I", i)
            idx_comp = 5125
        else:
            idx_buf = bytearray()
            for i in indices:
                idx_buf += struct.pack("<H", i)
            idx_comp = 5123
        idx_bv = add_buffer_view(idx_buf)
        idx_acc = len(accessors)
        accessors.append({
            "bufferView": idx_bv, "componentType": idx_comp,
            "count": idx_count, "type": "SCALAR",
        })

        # Primitive
        mat_idx = tex_to_mat_idx.get(tex_idx, fallback_mat_idx)
        primitives.append({
            "attributes": {
                "POSITION": pos_acc,
                "TEXCOORD_0": uv_acc,
                "COLOR_0": col_acc,
            },
            "indices": idx_acc,
            "material": mat_idx,
        })

    # Append texture image data to the binary buffer
    for i, png_bytes in enumerate(image_bin_data):
        bv_idx = add_buffer_view(png_bytes)
        images[i]["bufferView"] = bv_idx

    # Build glTF JSON
    level_name = os.path.basename(os.path.dirname(pcwb_path))
    if not level_name or level_name == ".":
        level_name = os.path.basename(pcwb_path).replace(".pcwb", "")

    gltf = {
        "asset": {"version": "2.0", "generator": "SpiderwickWorldExport v2"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": level_name}],
        "meshes": [{"primitives": primitives}],
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(all_bin)}],
        "materials": materials,
    }

    if images:
        gltf["images"] = images
        gltf["textures"] = textures
        gltf["samplers"] = samplers

    # Write GLB
    json_str = json.dumps(gltf, separators=(",", ":"))
    json_bytes = json_str.encode("utf-8")
    json_pad = (4 - len(json_bytes) % 4) % 4
    json_bytes += b" " * json_pad

    json_chunk = struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes
    bin_data = bytes(all_bin)
    bin_chunk = struct.pack("<II", len(bin_data), 0x004E4942) + bin_data
    total_len = 12 + len(json_chunk) + len(bin_chunk)
    header = struct.pack("<III", 0x46546C67, 2, total_len)

    with open(output_path, "wb") as f:
        f.write(header + json_chunk + bin_chunk)

    size_mb = os.path.getsize(output_path) / (1024 * 1024)
    mapped = sum(1 for c in chunks if c['tex_idx'] in tex_to_mat_idx)
    print(f"  -> {output_path} ({size_mb:.1f} MB, {mapped}/{len(chunks)} textured)")
    return True


def main():
    parser = argparse.ArgumentParser(description="Spiderwick world glTF exporter v2")
    parser.add_argument("input", help="world.pcwb file or directory")
    parser.add_argument("output", nargs="?", help="output .glb (or dir for batch)")
    parser.add_argument("--batch", action="store_true")
    args = parser.parse_args()

    if args.batch or os.path.isdir(args.input):
        base = args.input
        out_dir = args.output or base
        for d in sorted(os.listdir(base)):
            pcwb = os.path.join(base, d, "world.pcwb")
            if os.path.exists(pcwb):
                out = os.path.join(out_dir, d, f"{d}_world.glb")
                os.makedirs(os.path.dirname(out), exist_ok=True)
                print(f"\n{d}:")
                export_world(pcwb, out)
    else:
        out = args.output or args.input.replace(".pcwb", ".glb")
        export_world(args.input, out)


if __name__ == "__main__":
    main()
