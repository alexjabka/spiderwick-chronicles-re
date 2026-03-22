#!/usr/bin/env python3
"""
Spiderwick Chronicles — World Level Exporter v5
=================================================
Dual export: OBJ+MTL (geometry+textures) + glTF (.glb) with vertex colors.

OBJ: external DDS textures, smart grouping, no vertex colors (OBJ limitation)
GLB: embedded vertex colors as COLOR_0, per-object nodes, materials

Props: named objects parsed from PCWB header, with transform matrices applied
to move them from storage positions to correct world positions.

Both share: smart categorization, correct texture mapping via header table
+ inline PCIM refs.

Usage:
  python spiderwick_world_export.py <world.pcwb> [output_dir]
  python spiderwick_world_export.py --batch <wad_dir>
  python spiderwick_world_export.py --format obj  (OBJ only)
  python spiderwick_world_export.py --format glb  (GLB only)
  python spiderwick_world_export.py --format both (default)
"""

import struct, sys, os, math, argparse, json, re
from collections import defaultdict


def read_u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


# ---------------------------------------------------------------------------
# Section finders
# ---------------------------------------------------------------------------
def find_pcims_validated(data):
    pcims = []
    pos = 0
    while pos < len(data) - 0xC1:
        if data[pos:pos+4] == b"PCIM":
            ver = read_u32(data, pos + 4)
            tsz = read_u32(data, pos + 8)
            dsz = read_u32(data, pos + 0x0C)
            doff = read_u32(data, pos + 0x10)
            if ver == 2 and 0 < tsz < len(data) and 0 < dsz <= tsz:
                ok = False
                if doff + 4 <= len(data) and data[doff:doff+4] == b"DDS ":
                    ok = True
                elif pos + 0xC1 + 4 <= len(data) and data[pos+0xC1:pos+0xC1+4] == b"DDS ":
                    ok = True
                if ok:
                    pcims.append(pos)
                    pos += 0xC4 if doff != pos + 0xC1 else ((0xC1 + dsz + 3) & ~3)
                    continue
        pos += 4
    return pcims


def find_pcrds_validated(data):
    pcrds = []
    pos = 0
    while pos < len(data) - 0x1C:
        if data[pos:pos+4] == b"PCRD" and read_u32(data, pos + 4) == 2:
            ic = read_u32(data, pos + 0x0C)
            vc = read_u32(data, pos + 0x10)
            if 0 < ic < 1000000 and 0 < vc < 1000000:
                pcrds.append(pos)
        pos += 4
    return pcrds


# ---------------------------------------------------------------------------
# Texture mapping
# ---------------------------------------------------------------------------
def build_texture_ref_table(data, pcims):
    tex_ref = {}
    tp = read_u32(data, 0x94)
    if 0 < tp < len(data):
        pos = tp
        while pos + 16 <= len(data):
            ti = read_u32(data, pos)
            po = read_u32(data, pos + 4)
            if po + 4 <= len(data) and data[po:po+4] == b"PCIM":
                tex_ref[ti] = po
            else:
                break
            pos += 16
    for po in pcims:
        if po in tex_ref.values():
            continue
        if po >= 16:
            pti = read_u32(data, po - 16)
            ppo = read_u32(data, po - 12)
            if ppo == po and read_u32(data, po - 8) == 0 and read_u32(data, po - 4) == 0:
                tex_ref[pti] = po
    return tex_ref


def build_pcrd_texture_map(data, pcrds):
    ps = set(pcrds)
    m = {}
    pos = 0
    while pos < len(data) - 12:
        if (read_u32(data, pos) == 0xFFFFFFFF and
            read_u32(data, pos+4) == 0xFFFFFFFF and
            read_u32(data, pos+8) == 0xFFFFFFFF):
            if pos >= 4 and pos + 40 <= len(data):
                ti = read_u32(data, pos - 4)
                pr = read_u32(data, pos + 36)
                if pr in ps:
                    m[pr] = ti
            pos += 12
        else:
            pos += 4
    return m


def extract_dds(data, pcim_off):
    if data[pcim_off:pcim_off+4] != b"PCIM":
        return None, 0, 0
    dsz = read_u32(data, pcim_off + 0x0C)
    doff = read_u32(data, pcim_off + 0x10)
    w = read_u32(data, pcim_off + 0x9C)
    h = read_u32(data, pcim_off + 0xA0)
    if doff + dsz <= len(data) and data[doff:doff+4] == b"DDS ":
        return data[doff:doff+dsz], w, h
    rel = pcim_off + 0xC1
    if rel + dsz <= len(data) and data[rel:rel+4] == b"DDS ":
        return data[rel:rel+dsz], w, h
    return None, w, h


# ---------------------------------------------------------------------------
# Parse prop entries from PCWB header
# ---------------------------------------------------------------------------
def read_f32(data, off):
    return struct.unpack_from("<f", data, off)[0]


def parse_props(data):
    """Parse prop instance table from PCWB header.

    Each prop entry is 0xA0 (160) bytes:
      +0x00: 4x4 transform matrix (3x3 rotation rows + translation, row-major)
      +0x40: AABB min (vec3 + pad)
      +0x50: AABB max (vec3 + pad)
      +0x60: name (null-terminated ASCII, 64 bytes max)
      +0x8C: def_ptr → prop definition {index, pcrd_count, mesh_list_off, ...}

    Returns list of {name, matrix, position, pcrd_offsets} for each prop.
    """
    prop_count = read_u32(data, 0x50)
    prop_table = read_u32(data, 0x98)
    STRIDE = 0xA0

    if prop_count == 0 or prop_count > 1000 or prop_table == 0:
        return []

    props = []
    for pi in range(prop_count):
        entry = prop_table + pi * STRIDE

        # 4x4 matrix: rows are (rot0,0), (rot1,0), (rot2,0), (tx,ty,tz,1)
        mat = [read_f32(data, entry + j * 4) for j in range(16)]
        pos = (mat[12], mat[13], mat[14])

        # Name at +0x60
        name_off = entry + 0x60
        end = name_off
        while end < len(data) and data[end] != 0:
            end += 1
        name = data[name_off:end].decode('ascii', errors='replace')

        # Prop definition pointer at +0x8C
        def_ptr = read_u32(data, entry + 0x8C)
        if def_ptr == 0 or def_ptr + 12 > len(data):
            continue

        pcrd_count = read_u32(data, def_ptr + 4)
        mesh_list_off = read_u32(data, def_ptr + 8)

        if pcrd_count == 0 or pcrd_count > 10000 or mesh_list_off == 0:
            continue

        # Trace mesh_block[] → submesh[] → batch_entry → PCRD offset
        pcrd_offsets = []
        for mi in range(pcrd_count):
            block_ptr_off = mesh_list_off + mi * 4
            if block_ptr_off + 4 > len(data):
                break
            block_off = read_u32(data, block_ptr_off)
            if block_off + 20 > len(data):
                continue

            sub_count = read_u32(data, block_off)
            submesh_list_ptr = read_u32(data, block_off + 16)

            if sub_count == 0 or sub_count > 100 or submesh_list_ptr == 0:
                continue

            for si in range(sub_count):
                sm_entry = submesh_list_ptr + si * 4
                if sm_entry + 4 > len(data):
                    break
                submesh_ptr = read_u32(data, sm_entry)
                if submesh_ptr + 16 > len(data):
                    continue
                batch_ptr = read_u32(data, submesh_ptr + 12)
                if batch_ptr + 48 > len(data):
                    continue

                pcrd_off = read_u32(data, batch_ptr + 44)
                if (pcrd_off + 4 <= len(data) and
                        data[pcrd_off:pcrd_off + 4] == b"PCRD"):
                    pcrd_offsets.append(pcrd_off)

        # D3D9 row-vector convention: world = vertex * matrix
        # Transpose 3x3 rotation for column-vector multiply: world = M^T * v + T
        rot = [
            [mat[0], mat[4], mat[8]],
            [mat[1], mat[5], mat[9]],
            [mat[2], mat[6], mat[10]],
        ]

        # Prop type: def_ptr+12 == 1 → STATIC, == 2 → ANIMATED
        prop_type = read_u32(data, def_ptr + 12) if def_ptr + 16 <= len(data) else 0

        props.append({
            'name': name,
            'matrix': mat,
            'rotation': rot,
            'position': pos,
            'pcrd_offsets': set(pcrd_offsets),
            'type': prop_type,  # 1=STATIC, 2=ANIMATED
        })

    return props


def transform_vertex(rot, pos, vx, vy, vz):
    """Apply 3x3 rotation + translation to a vertex in engine space (column-vector)."""
    wx = rot[0][0] * vx + rot[0][1] * vy + rot[0][2] * vz + pos[0]
    wy = rot[1][0] * vx + rot[1][1] * vy + rot[1][2] * vz + pos[1]
    wz = rot[2][0] * vx + rot[2][1] * vy + rot[2][2] * vz + pos[2]
    return wx, wy, wz


def transform_vertex_4x4(mat, vx, vy, vz):
    """Apply 4x4 row-major world matrix using D3D9 row-vector convention.
    world = [vx vy vz 1] * M  (row-vector * matrix)."""
    wx = vx * mat[0] + vy * mat[4] + vz * mat[8]  + mat[12]
    wy = vx * mat[1] + vy * mat[5] + vz * mat[9]  + mat[13]
    wz = vx * mat[2] + vy * mat[6] + vz * mat[10] + mat[14]
    return wx, wy, wz


# ---------------------------------------------------------------------------
# Parse PCRD chunk (now with vertex colors)
# ---------------------------------------------------------------------------
def parse_pcrd(data, pcrd_off, prop_transform=None, world_matrix=None):
    """Parse PCRD chunk. Transform options (mutually exclusive, world_matrix preferred):
    - world_matrix: flat 16-float row-major 4x4 from D3D9 capture (row-vector multiply)
    - prop_transform: (rotation, position) tuple from PCWB header (column-vector)"""
    hs = read_u32(data, pcrd_off + 0x08)
    ic = read_u32(data, pcrd_off + 0x0C)
    vc = read_u32(data, pcrd_off + 0x10)
    io = read_u32(data, pcrd_off + 0x14)
    vo = read_u32(data, pcrd_off + 0x18)
    stride = 32 if hs <= 0x10 else 24

    if vc == 0 or ic == 0:
        return None
    if vo + vc * stride > len(data) or io + ic * 2 > len(data):
        return None

    px, py, pz = struct.unpack_from("<fff", data, vo)
    if any(math.isnan(v) or math.isinf(v) or abs(v) > 50000 for v in (px, py, pz)):
        return None

    positions = []
    uvs = []
    colors = []
    for i in range(vc):
        off = vo + i * stride
        x, y, z = struct.unpack_from("<fff", data, off)
        if any(math.isnan(v) or math.isinf(v) or abs(v) > 50000 for v in (x, y, z)):
            return None
        r, g, b, a = data[off+12], data[off+13], data[off+14], data[off+15]
        u, v = struct.unpack_from("<ff", data, off + 16)

        # Apply world transform (captured D3D9 matrix preferred over PCWB header)
        if world_matrix:
            x, y, z = transform_vertex_4x4(world_matrix, x, y, z)
        elif prop_transform:
            rot, pos = prop_transform
            x, y, z = transform_vertex(rot, pos, x, y, z)

        positions.append((x, z, -y))  # Y-up -> Z-up
        uvs.append((u, 1.0 - v))
        colors.append((r / 255.0, g / 255.0, b / 255.0, a / 255.0))

    raw = [struct.unpack_from("<H", data, io + i * 2)[0] for i in range(ic)]
    tris = []
    for i in range(len(raw) - 2):
        i0, i1, i2 = raw[i], raw[i+1], raw[i+2]
        if i0 == i1 or i1 == i2 or i0 == i2:
            continue
        if i % 2 == 0:
            tris.extend([i0, i1, i2])
        else:
            tris.extend([i1, i0, i2])

    if not tris:
        return None

    ys = [struct.unpack_from("<f", data, vo + i * stride + 4)[0] for i in range(vc)]
    center_y = sum(ys) / len(ys)

    return {
        'positions': positions, 'uvs': uvs, 'colors': colors,
        'indices': tris, 'center_y': center_y,
    }


# ---------------------------------------------------------------------------
# Spatial clustering
# ---------------------------------------------------------------------------
CLUSTER_DIST = 25.0
UNDERGROUND_Y = -10.0

def cluster_chunks(chunks_with_centers):
    n = len(chunks_with_centers)
    if n <= 1:
        return [list(range(n))]
    parent = list(range(n))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]; x = parent[x]
        return x

    def union(a, b):
        parent[find(a)] = find(b)

    for i in range(n):
        ci = chunks_with_centers[i]
        for j in range(i + 1, n):
            cj = chunks_with_centers[j]
            dx, dy, dz = ci[0]-cj[0], ci[1]-cj[1], ci[2]-cj[2]
            if dx*dx + dy*dy + dz*dz < CLUSTER_DIST * CLUSTER_DIST:
                union(i, j)

    groups = defaultdict(list)
    for i in range(n):
        groups[find(i)].append(i)
    return list(groups.values())


# ---------------------------------------------------------------------------
# Build categorized objects
# ---------------------------------------------------------------------------
def build_objects(parsed, tex_is_shader, prop_pcrd_map=None, geom_transforms=None, pcrd_index_map=None):
    """Group chunks into named objects with traceable names.
    Props: named by prop name + PCRD sub-index.
    World geometry with transform: named by PCRD index + position.
    World geometry without transform: grouped by texture (tex###).
    """
    if prop_pcrd_map is None:
        prop_pcrd_map = {}
    if geom_transforms is None:
        geom_transforms = {}
    if pcrd_index_map is None:
        pcrd_index_map = {}

    objects = []
    world_by_tex = defaultdict(list)

    for i, (off, ti, chunk) in enumerate(parsed):
        pname = prop_pcrd_map.get(off)
        pcrd_idx = pcrd_index_map.get(off, -1)

        if pname:
            # Prop: individual named object per PCRD
            obj_name = f"{pname}_pcrd{pcrd_idx}" if pcrd_idx >= 0 else pname
            objects.append((obj_name, ti, [i]))
        elif off in geom_transforms:
            # World geometry with transform: individual named object
            cx = chunk['center_y']  # actually center in engine Y
            pos = chunk['positions']
            avg_x = sum(p[0] for p in pos) / len(pos) if pos else 0
            avg_z = sum(p[1] for p in pos) / len(pos) if pos else 0
            obj_name = f"world_{pcrd_idx:04d}_tex{ti:03d}" if pcrd_idx >= 0 else f"world_0x{off:X}_tex{ti:03d}"
            objects.append((obj_name, ti, [i]))
        else:
            # Untransformed world geometry: group by texture
            world_by_tex[ti].append(i)

    # Add grouped world geometry
    for ti in sorted(world_by_tex.keys()):
        cis = world_by_tex[ti]
        name = f"tex{ti:03d}" if ti >= 0 else "notex"
        objects.append((name, ti, cis))

    return objects


# ---------------------------------------------------------------------------
# Write OBJ + MTL
# ---------------------------------------------------------------------------
def write_obj(obj_path, mtl_path, tex_dir, objects, parsed, tex_files, tex_has_alpha):
    mtl_name = os.path.basename(mtl_path)
    with open(mtl_path, "w") as f:
        for ti in sorted(tex_files.keys()):
            fname = tex_files[ti]
            f.write(f"newmtl tex_{ti:03d}\n")
            f.write(f"Ka 1.0 1.0 1.0\nKd 1.0 1.0 1.0\nKs 0.0 0.0 0.0\n")
            f.write(f"illum 2\nd 1.0\nmap_Kd textures/{fname}\n")
            if tex_has_alpha.get(ti, False):
                f.write(f"map_d textures/{fname}\n")
            f.write(f"\n")
        f.write(f"newmtl unmapped\nKa 0.5 0.5 0.5\nKd 0.5 0.5 0.5\n\n")

    with open(obj_path, "w") as f:
        f.write(f"# Spiderwick Chronicles World\n")
        f.write(f"# {len(objects)} objects\n")
        f.write(f"mtllib {mtl_name}\n\n")
        gv = 0
        for obj_name, ti, chunk_indices in objects:
            f.write(f"o {obj_name}\n")

            # Write all vertices + UVs first
            for ci in chunk_indices:
                chunk = parsed[ci][2]
                for j, p in enumerate(chunk['positions']):
                    c = chunk['colors'][j]
                    f.write(f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f} {c[0]:.4f} {c[1]:.4f} {c[2]:.4f}\n")
                for uv in chunk['uvs']:
                    f.write(f"vt {uv[0]:.6f} {uv[1]:.6f}\n")

            # Build per-chunk vertex offsets
            ci_offsets = {}
            off_acc = gv
            for ci in chunk_indices:
                ci_offsets[ci] = off_acc
                off_acc += len(parsed[ci][2]['positions'])

            if ti == -2:
                # Multi-material object: group faces by texture
                by_mat = defaultdict(list)
                for ci in chunk_indices:
                    by_mat[parsed[ci][1]].append(ci)
                for mat_ti in sorted(by_mat.keys()):
                    mat = f"tex_{mat_ti:03d}" if mat_ti >= 0 and mat_ti in tex_files else "unmapped"
                    f.write(f"usemtl {mat}\n")
                    for ci in by_mat[mat_ti]:
                        cb = ci_offsets[ci]
                        idx = parsed[ci][2]['indices']
                        for i in range(0, len(idx), 3):
                            f.write(f"f {cb+idx[i]+1}/{cb+idx[i]+1} {cb+idx[i+1]+1}/{cb+idx[i+1]+1} {cb+idx[i+2]+1}/{cb+idx[i+2]+1}\n")
            else:
                # Single material
                mat = f"tex_{ti:03d}" if ti >= 0 and ti in tex_files else "unmapped"
                f.write(f"usemtl {mat}\n")
                for ci in chunk_indices:
                    cb = ci_offsets[ci]
                    idx = parsed[ci][2]['indices']
                    for i in range(0, len(idx), 3):
                        f.write(f"f {cb+idx[i]+1}/{cb+idx[i]+1} {cb+idx[i+1]+1}/{cb+idx[i+1]+1} {cb+idx[i+2]+1}/{cb+idx[i+2]+1}\n")

            gv = off_acc
            f.write(f"\n")


# ---------------------------------------------------------------------------
# Write GLB with vertex colors
# ---------------------------------------------------------------------------
def pad4(b):
    r = len(b) % 4
    return bytes(b) + b"\x00" * (4 - r) if r else bytes(b)


def dds_to_png_manual(dds_path):
    """Convert uncompressed BGRA DDS to PNG without Pillow."""
    import zlib
    try:
        with open(dds_path, "rb") as f:
            dds = f.read()
    except Exception:
        return None

    if len(dds) < 128 or dds[:4] != b"DDS ":
        return None

    h = struct.unpack_from("<I", dds, 12)[0]
    w = struct.unpack_from("<I", dds, 16)[0]
    pf_flags = struct.unpack_from("<I", dds, 80)[0]
    bpp = struct.unpack_from("<I", dds, 88)[0]

    if bpp != 32 or not (pf_flags & 0x40):
        return None  # not uncompressed RGBA

    pixel_data = dds[128:128 + w * h * 4]
    if len(pixel_data) != w * h * 4:
        return None

    # BGRA -> RGBA
    rgba = bytearray(pixel_data)
    for i in range(0, len(rgba), 4):
        rgba[i], rgba[i+2] = rgba[i+2], rgba[i]

    # Build minimal PNG
    def write_chunk(ctype, cdata):
        c = ctype + cdata
        return struct.pack(">I", len(cdata)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    ihdr = write_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    raw_rows = b""
    stride = w * 4
    for y in range(h):
        raw_rows += b"\x00" + rgba[y * stride:(y + 1) * stride]
    idat = write_chunk(b"IDAT", zlib.compress(raw_rows))
    iend = write_chunk(b"IEND", b"")

    return b"\x89PNG\r\n\x1a\n" + ihdr + idat + iend


def write_glb(glb_path, objects, parsed, tex_files, tex_has_alpha, tex_dir, level_name):
    accessors = []
    buffer_views = []
    all_bin = bytearray()
    nodes = []
    meshes = []
    materials = []
    images = []
    textures_gltf = []
    samplers = [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}]

    # Build materials from tex_files
    mat_index = {}  # ti -> material index
    for ti in sorted(tex_files.keys()):
        mi = len(materials)
        mat_index[ti] = mi

        # Read PNG from DDS (try Pillow, fallback manual BGRA->PNG)
        dds_path = os.path.join(tex_dir, tex_files[ti])
        png_bytes = None
        try:
            from PIL import Image
            import io as _io
            with open(dds_path, "rb") as f:
                img = Image.open(f)
                buf = _io.BytesIO()
                img.save(buf, format="PNG")
                png_bytes = buf.getvalue()
        except Exception:
            pass

        if png_bytes is None:
            # Manual DDS BGRA -> PNG conversion (no Pillow needed)
            png_bytes = dds_to_png_manual(dds_path)

        mat = {
            "name": f"tex_{ti:03d}",
            "pbrMetallicRoughness": {
                "metallicFactor": 0.0,
                "roughnessFactor": 1.0,
            },
        }

        if png_bytes:
            # Add image
            img_bv = len(buffer_views)
            offset = len(all_bin)
            padded = pad4(png_bytes)
            all_bin.extend(padded)
            buffer_views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(png_bytes)})
            img_idx = len(images)
            images.append({"mimeType": "image/png", "bufferView": img_bv})
            tex_idx_gltf = len(textures_gltf)
            textures_gltf.append({"sampler": 0, "source": img_idx})
            mat["pbrMetallicRoughness"]["baseColorTexture"] = {"index": tex_idx_gltf}

            if tex_has_alpha.get(ti, False):
                mat["alphaMode"] = "BLEND"

        materials.append(mat)

    # Fallback material
    fallback_mi = len(materials)
    materials.append({
        "name": "unmapped",
        "pbrMetallicRoughness": {"baseColorFactor": [0.5, 0.5, 0.5, 1.0], "metallicFactor": 0.0, "roughnessFactor": 1.0},
    })

    # Build mesh + node per object
    for obj_name, ti, chunk_indices in objects:
        # Merge all chunks in this object
        all_pos = []
        all_uv = []
        all_col = []
        all_idx = []
        vbase = 0
        for ci in chunk_indices:
            c = parsed[ci][2]
            all_pos.extend(c['positions'])
            all_uv.extend(c['uvs'])
            all_col.extend(c['colors'])
            all_idx.extend([i + vbase for i in c['indices']])
            vbase += len(c['positions'])

        nv = len(all_pos)
        ni = len(all_idx)
        if nv == 0 or ni == 0:
            continue

        # Pack buffers
        pos_buf = bytearray()
        for p in all_pos:
            pos_buf += struct.pack("<fff", *p)
        pos_bv = len(buffer_views)
        off = len(all_bin); all_bin.extend(pad4(pos_buf))
        buffer_views.append({"buffer": 0, "byteOffset": off, "byteLength": len(pos_buf)})
        mins = [min(p[i] for p in all_pos) for i in range(3)]
        maxs = [max(p[i] for p in all_pos) for i in range(3)]
        pos_acc = len(accessors)
        accessors.append({"bufferView": pos_bv, "componentType": 5126, "count": nv, "type": "VEC3", "min": mins, "max": maxs})

        uv_buf = bytearray()
        for u in all_uv:
            uv_buf += struct.pack("<ff", *u)
        uv_bv = len(buffer_views)
        off = len(all_bin); all_bin.extend(pad4(uv_buf))
        buffer_views.append({"buffer": 0, "byteOffset": off, "byteLength": len(uv_buf)})
        uv_acc = len(accessors)
        accessors.append({"bufferView": uv_bv, "componentType": 5126, "count": nv, "type": "VEC2"})

        col_buf = bytearray()
        for c in all_col:
            col_buf += struct.pack("<ffff", *c)
        col_bv = len(buffer_views)
        off = len(all_bin); all_bin.extend(pad4(col_buf))
        buffer_views.append({"buffer": 0, "byteOffset": off, "byteLength": len(col_buf)})
        col_acc = len(accessors)
        accessors.append({"bufferView": col_bv, "componentType": 5126, "count": nv, "type": "VEC4"})

        use_u32 = nv > 65535
        idx_buf = bytearray()
        for i in all_idx:
            idx_buf += struct.pack("<I" if use_u32 else "<H", i)
        idx_bv = len(buffer_views)
        off = len(all_bin); all_bin.extend(pad4(idx_buf))
        buffer_views.append({"buffer": 0, "byteOffset": off, "byteLength": len(idx_buf)})
        idx_acc = len(accessors)
        accessors.append({"bufferView": idx_bv, "componentType": 5125 if use_u32 else 5123, "count": ni, "type": "SCALAR"})

        mi = mat_index.get(ti, fallback_mi)
        mesh_idx = len(meshes)
        meshes.append({
            "name": obj_name,
            "primitives": [{
                "attributes": {"POSITION": pos_acc, "TEXCOORD_0": uv_acc, "COLOR_0": col_acc},
                "indices": idx_acc,
                "material": mi,
            }]
        })
        nodes.append({"mesh": mesh_idx, "name": obj_name})

    # Root node with all children
    # Root is node 0, children are nodes 1..N (offset by 1)
    root = {"name": level_name, "children": list(range(1, len(nodes) + 1))}
    all_nodes = [root] + nodes
    scene_nodes = [0]

    gltf = {
        "asset": {"version": "2.0", "generator": "SpiderwickWorldExport v5"},
        "scene": 0,
        "scenes": [{"nodes": scene_nodes}],
        "nodes": all_nodes,
        "meshes": meshes,
        "accessors": accessors,
        "bufferViews": buffer_views,
        "buffers": [{"byteLength": len(all_bin)}],
        "materials": materials,
    }
    if images:
        gltf["images"] = images
        gltf["textures"] = textures_gltf
        gltf["samplers"] = samplers

    json_str = json.dumps(gltf, separators=(",", ":"))
    json_bytes = json_str.encode("utf-8")
    json_pad = (4 - len(json_bytes) % 4) % 4
    json_bytes += b" " * json_pad
    json_chunk = struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes
    bin_data = bytes(all_bin)
    bin_chunk = struct.pack("<II", len(bin_data), 0x004E4942) + bin_data
    total_len = 12 + len(json_chunk) + len(bin_chunk)
    header = struct.pack("<III", 0x46546C67, 2, total_len)

    with open(glb_path, "wb") as f:
        f.write(header + json_chunk + bin_chunk)


# ---------------------------------------------------------------------------
# Parse scene dump file (scene_<level>.txt from DUMP SCENE button)
# ---------------------------------------------------------------------------
def parse_scene_dump(scene_path):
    """Parse scene_<level>.txt from ASI mod's DUMP SCENE button.
    Returns:
      geom_instances: dict pcrd_index → 16-float world matrix (old format)
      vm_placements: list of {name, pos, rot, obj_addr}
      total_geom: total rendered geometry count from sector info
      fingerprints: list of (vc, ic, v0x, v0y, v0z, mat[16]) — vertex fingerprint entries
    """
    geom_instances = {}  # pcrd_index → [16 floats]
    vm_placements = []
    total_geom = 0
    fingerprints = []  # (vc, ic, (v0x,v0y,v0z), mat[16])
    section = None

    with open(scene_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            if line.startswith('[geometry_instances]'):
                section = 'geom'
                continue
            elif line.startswith('[vm_placements]'):
                section = 'vm'
                continue
            elif line.startswith('[all_geometry]'):
                section = 'allgeom'
                continue
            elif line.startswith('[sector_info]'):
                section = 'sector'
                continue

            if section == 'geom':
                parts = line.split('\t')
                if len(parts) >= 20:
                    pcrd_idx = int(parts[0])
                    mat = [float(x) for x in parts[4:20]]
                    geom_instances[pcrd_idx] = mat

            elif section == 'allgeom':
                parts = line.split('\t')
                if len(parts) >= 24:  # vc + ic + v0(3) + pos(3) + mat(16)
                    vc = int(parts[0])
                    ic = int(parts[1])
                    v0 = (float(parts[2]), float(parts[3]), float(parts[4]))
                    mat = [float(x) for x in parts[8:24]]
                    fingerprints.append((vc, ic, v0, mat))

            elif section == 'vm':
                parts = line.split('\t')
                if len(parts) >= 10:
                    vm_placements.append({
                        'name': parts[0],
                        'obj_addr': parts[1],
                        'pos': (float(parts[2]), float(parts[3]), float(parts[4])),
                        'rot': (float(parts[5]), float(parts[6]), float(parts[7])),
                        'has_pos': int(parts[8]),
                        'has_rot': int(parts[9]),
                    })

            elif section == 'sector':
                if line.startswith('total_geometry_instances='):
                    total_geom = int(line.split('=')[1])

    return geom_instances, vm_placements, total_geom, fingerprints


def parse_scene_rendered(scene_path):
    """Parse [rendered_pcrds] and [instance_transforms] from scene dump.
    Returns:
      rendered_keys: set of (vc, ic, (v0x,v0y,v0z)) — all rendered PCRDs
      transforms: list of (vc, ic, (v0x,v0y,v0z), mat[16]) — non-identity instances
    """
    rendered_keys = set()
    transforms = []
    section = None

    with open(scene_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('[rendered_pcrds]'):
                section = 'rendered'
                continue
            elif line.startswith('[instance_transforms]'):
                section = 'transforms'
                continue
            elif line.startswith('['):
                section = None
                continue

            parts = line.split('\t')
            if section == 'rendered' and len(parts) >= 5:
                vc, ic = int(parts[0]), int(parts[1])
                v0 = (round(float(parts[2]), 3), round(float(parts[3]), 3), round(float(parts[4]), 3))
                rendered_keys.add((vc, ic, v0))

            elif section == 'transforms' and len(parts) >= 21:
                vc, ic = int(parts[0]), int(parts[1])
                v0 = (round(float(parts[2]), 3), round(float(parts[3]), 3), round(float(parts[4]), 3))
                mat = [float(x) for x in parts[5:21]]
                transforms.append((vc, ic, v0, mat))

    return rendered_keys, transforms


# ---------------------------------------------------------------------------
# Main export
# ---------------------------------------------------------------------------
def export_world(pcwb_path, output_dir=None, fmt="both", transforms_path=None, vp_drawn_path=None, geom_path=None, scene_path=None):
    with open(pcwb_path, "rb") as f:
        data = f.read()
    if data[:4] != b"PCWB":
        print(f"  Not a PCWB file"); return False

    level_name = os.path.basename(os.path.dirname(pcwb_path))
    if not level_name or level_name == ".":
        level_name = os.path.basename(pcwb_path).replace(".pcwb", "")
    if output_dir is None:
        output_dir = os.path.dirname(pcwb_path)
    os.makedirs(output_dir, exist_ok=True)
    tex_dir = os.path.join(output_dir, "textures")
    os.makedirs(tex_dir, exist_ok=True)

    pcims = find_pcims_validated(data)
    pcrds = find_pcrds_validated(data)
    tex_ref = build_texture_ref_table(data, pcims)
    batch_map = build_pcrd_texture_map(data, pcrds)

    # Load captured per-PCRD world transforms (from match_prop_transforms.py)
    captured_transforms = {}  # pcrd_offset → 16-float row-major world matrix
    if transforms_path and os.path.exists(transforms_path):
        with open(transforms_path, 'r') as f:
            tdata = json.load(f)
        for off_str, entry in tdata.items():
            captured_transforms[int(off_str)] = entry['world_matrix']
        print(f"  Loaded {len(captured_transforms)} captured transforms from {transforms_path}")

    # Load geometry instance world matrices (from GeomInstance_Init hook)
    geom_transforms = {}  # pcrd_offset → 16-float row-major world matrix
    if geom_path and os.path.exists(geom_path):
        with open(geom_path, 'r') as f:
            gdata = json.load(f)
        for off_str, entry in gdata.items():
            geom_transforms[int(off_str)] = entry['world_matrix']
        print(f"  Loaded {len(geom_transforms)} geometry instance transforms from {geom_path}")

    # Load scene dump (from DUMP SCENE button)
    rendered_set = None     # set of file offsets confirmed as rendered
    if scene_path and os.path.exists(scene_path):
        # Build file PCRD lookup: (vc, ic, rounded_v0) → [file_offsets]
        pcrd_v0_lookup = {}  # key → list of file offsets (handles duplicates)
        for pcrd_off in pcrds:
            vc_f = read_u32(data, pcrd_off + 0x10)
            ic_f = read_u32(data, pcrd_off + 0x0C)
            vo_f = read_u32(data, pcrd_off + 0x18)
            if vo_f + 12 <= len(data):
                v0 = (round(read_f32(data, vo_f), 3),
                      round(read_f32(data, vo_f + 4), 3),
                      round(read_f32(data, vo_f + 8), 3))
                key = (vc_f, ic_f, v0)
                pcrd_v0_lookup.setdefault(key, []).append(pcrd_off)

        # Parse rendered set + instance transforms
        rendered_keys, inst_transforms = parse_scene_rendered(scene_path)
        print(f"  Scene dump: {len(rendered_keys)} unique rendered PCRDs, {len(inst_transforms)} instance transforms")

        # Build rendered_set from fingerprint matching
        rendered_set = set()
        for key in rendered_keys:
            offsets = pcrd_v0_lookup.get(key, [])
            for off in offsets:
                rendered_set.add(off)

        # Match instance transforms to file PCRDs
        n_matched = 0
        for vc, ic, v0, mat in inst_transforms:
            key = (vc, ic, v0)
            offsets = pcrd_v0_lookup.get(key, [])
            if offsets:
                foff = offsets[0]  # use first match
                if foff not in geom_transforms:
                    geom_transforms[foff] = mat
                else:
                    existing = geom_transforms[foff]
                    if isinstance(existing[0], list):
                        existing.append(mat)
                    else:
                        geom_transforms[foff] = [existing, mat]
                n_matched += 1
        print(f"  Rendered: {len(rendered_set)} file PCRDs, transforms: {n_matched} matched")

    # Load VP-drawn PCRD set (surface geometry that needs no transform)
    vp_drawn_set = set()
    if vp_drawn_path and os.path.exists(vp_drawn_path):
        with open(vp_drawn_path, 'r') as f:
            vp_drawn_set = set(json.load(f))
        print(f"  Loaded {len(vp_drawn_set)} VP-drawn PCRDs from {vp_drawn_path}")

    # Parse props from PCWB header
    props = parse_props(data)
    prop_pcrd_map = {}       # pcrd_offset → prop_name
    prop_transform_map = {}  # pcrd_offset → (rotation, position) — PCWB fallback
    n_static = sum(1 for p in props if p['type'] == 1)
    n_animated = sum(1 for p in props if p['type'] == 2)
    for prop in props:
        for po in prop['pcrd_offsets']:
            prop_pcrd_map[po] = prop['name']
            # PCWB matrix as fallback for STATIC props without captured transform
            if prop['type'] == 1 and po not in captured_transforms:
                prop_transform_map[po] = (prop['rotation'], prop['position'])

    if props:
        n_captured = sum(1 for p in props for po in p['pcrd_offsets'] if po in captured_transforms)
        print(f"  {len(props)} props ({n_static} static + {n_animated} animated, {sum(len(p['pcrd_offsets']) for p in props)} PCRDs)")
        if captured_transforms:
            print(f"  {n_captured} PCRDs use captured transforms, {sum(len(p['pcrd_offsets']) for p in props) - n_captured} use PCWB fallback/none")
    print(f"  {len(pcrds)} PCRD, {len(pcims)} PCIM, {len(tex_ref)} tex mappings")

    # Build set of confirmed-rendered PCRDs
    has_scene = rendered_set is not None and len(rendered_set) > 0
    if has_scene:
        # Add props + transforms to rendered set (in case fingerprint missed them)
        for po in prop_pcrd_map:
            rendered_set.add(po)
        for off in geom_transforms:
            rendered_set.add(off)
        for off in captured_transforms:
            rendered_set.add(off)

    parsed = []
    n_geom = 0
    n_instanced = 0
    n_prop_fb = 0
    n_identity = 0
    n_skipped = 0
    for pcrd_idx, pcrd_off in enumerate(pcrds):
        is_prop = pcrd_off in prop_pcrd_map
        fallback = prop_transform_map.get(pcrd_off)
        cap_mat = captured_transforms.get(pcrd_off)
        geom_mat = geom_transforms.get(pcrd_off)
        ti = batch_map.get(pcrd_off, -1)
        if ti not in tex_ref: ti = -1

        # When scene dump available: skip PCRDs not confirmed as rendered
        if has_scene and pcrd_off not in rendered_set:
            n_skipped += 1
            continue

        # Handle instanced geometry (multiple world matrices per PCRD)
        if geom_mat and isinstance(geom_mat[0], list):
            for inst_idx, mat in enumerate(geom_mat):
                chunk = parse_pcrd(data, pcrd_off, world_matrix=mat)
                if chunk is None: continue
                parsed.append((pcrd_off, ti, chunk))
                n_instanced += 1
            continue

        # Single transform priority: fingerprint > D3D capture > PCWB prop > identity
        if geom_mat and not isinstance(geom_mat[0], list):
            world_mat = geom_mat
            n_geom += 1
        elif cap_mat:
            world_mat = cap_mat
            n_geom += 1
        else:
            world_mat = None
            if fallback:
                n_prop_fb += 1
            else:
                n_identity += 1

        chunk = parse_pcrd(data, pcrd_off,
                          prop_transform=fallback if not world_mat else None,
                          world_matrix=world_mat)
        if chunk is None: continue
        parsed.append((pcrd_off, ti, chunk))

    if has_scene:
        print(f"  Rendered: {len(rendered_set)} confirmed, skipped {n_skipped} non-rendered")
    print(f"  Transforms: {n_geom} engine, {n_instanced} instanced, {n_prop_fb} prop fallback, {n_identity} identity")

    # All rendered meshes included

    by_tex = defaultdict(list)
    for i, (off, ti, chunk) in enumerate(parsed):
        by_tex[ti].append(i)

    # Extract textures + detect properties
    tex_files = {}
    tex_has_alpha = {}
    tex_is_shader = {}
    for ti in sorted(by_tex.keys()):
        if ti < 0: continue
        pcim_off = tex_ref.get(ti)
        if pcim_off is None: continue
        dds, w, h = extract_dds(data, pcim_off)
        if dds:
            fname = f"tex_{ti:03d}_{w}x{h}.dds"
            with open(os.path.join(tex_dir, fname), "wb") as f:
                f.write(dds)
            tex_files[ti] = fname
            max_dim, min_dim = max(w, h), min(w, h)
            tex_is_shader[ti] = (max_dim <= 32) or (min_dim <= 8 and max_dim <= 64)
            has_alpha = False
            if len(dds) > 128:
                pf = struct.unpack_from("<I", dds, 80)[0]
                bpp = struct.unpack_from("<I", dds, 88)[0]
                if bpp == 32 and (pf & 0x41):
                    for p in range(131, min(len(dds), 128 + w*h*4), 4):
                        if dds[p] < 250: has_alpha = True; break
            tex_has_alpha[ti] = has_alpha

    # Build PCRD index map (pcrd_offset → index in file order)
    pcrd_index_map = {off: idx for idx, off in enumerate(pcrds)}
    objects = build_objects(parsed, tex_is_shader, prop_pcrd_map, geom_transforms, pcrd_index_map)

    tv = sum(len(parsed[ci][2]['positions']) for _, _, cis in objects for ci in cis)
    tt = sum(len(parsed[ci][2]['indices'])//3 for _, _, cis in objects for ci in cis)
    n_props = sum(1 for o in objects if not o[0].startswith("tex") and not o[0].startswith("notex"))
    n_world = len(objects) - n_props
    print(f"  {len(objects)} objects ({n_world} world + {n_props} props)")
    print(f"  {tv:,} verts, {tt:,} tris, {len(tex_files)} textures")

    if fmt in ("obj", "both"):
        obj_path = os.path.join(output_dir, f"{level_name}_world.obj")
        mtl_path = os.path.join(output_dir, f"{level_name}_world.mtl")
        write_obj(obj_path, mtl_path, tex_dir, objects, parsed, tex_files, tex_has_alpha)
        print(f"  -> {obj_path} ({os.path.getsize(obj_path)/1024/1024:.1f} MB)")

    if fmt in ("glb", "both"):
        glb_path = os.path.join(output_dir, f"{level_name}_world.glb")
        write_glb(glb_path, objects, parsed, tex_files, tex_has_alpha, tex_dir, level_name)
        print(f"  -> {glb_path} ({os.path.getsize(glb_path)/1024/1024:.1f} MB)")

    return True


def main():
    parser = argparse.ArgumentParser(description="Spiderwick world exporter v4")
    parser.add_argument("input", help="world.pcwb file or directory")
    parser.add_argument("output", nargs="?", help="output dir")
    parser.add_argument("--batch", action="store_true")
    parser.add_argument("--format", choices=["obj", "glb", "both"], default="both")
    parser.add_argument("--transforms", help="prop_transforms_<level>.json from match_prop_transforms.py")
    parser.add_argument("--vp-drawn", help="vp_drawn_<level>.json — VP-drawn PCRD set")
    parser.add_argument("--geom", help="geom_transforms_<level>.json — geometry instance world matrices")
    parser.add_argument("--scene", help="scene_<level>.txt from DUMP SCENE (replaces --geom and --transforms)")
    args = parser.parse_args()

    if args.batch or os.path.isdir(args.input):
        base = args.input
        for d in sorted(os.listdir(base)):
            pcwb = os.path.join(base, d, "world.pcwb")
            if os.path.exists(pcwb):
                print(f"\n{d}:")
                tf = os.path.join(base, d, f"prop_transforms_{d}.json")
                vp = os.path.join(base, d, f"vp_drawn_{d}.json")
                export_world(pcwb, os.path.join(base, d), args.format,
                           transforms_path=tf if os.path.exists(tf) else args.transforms,
                           vp_drawn_path=vp if os.path.exists(vp) else args.vp_drawn)
    else:
        out = args.output or os.path.dirname(args.input)
        export_world(args.input, out, args.format,
                    transforms_path=args.transforms,
                    vp_drawn_path=getattr(args, 'vp_drawn', None),
                    geom_path=getattr(args, 'geom', None),
                    scene_path=getattr(args, 'scene', None))


if __name__ == "__main__":
    main()
