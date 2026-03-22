#!/usr/bin/env python3
"""
Spiderwick Chronicles — World Level OBJ Exporter v3
=====================================================
Smart export: per-texture materials, spatial clustering into objects,
surface/underground categorization, external DDS textures.

Each spatial cluster of chunks sharing a texture = one named OBJ object.
Surface geometry and underground props are separated.

Usage:
  python spiderwick_world_obj.py <world.pcwb> [output_dir]
  python spiderwick_world_obj.py --batch <wad_dir>
"""

import struct, sys, os, math, argparse
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
# Parse PCRD chunk
# ---------------------------------------------------------------------------
def parse_pcrd(data, pcrd_off):
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

    # Validate first vertex
    px, py, pz = struct.unpack_from("<fff", data, vo)
    if any(math.isnan(v) or math.isinf(v) or abs(v) > 50000 for v in (px, py, pz)):
        return None

    # Read vertices
    positions = []
    uvs = []
    for i in range(vc):
        off = vo + i * stride
        x, y, z = struct.unpack_from("<fff", data, off)
        if any(math.isnan(v) or math.isinf(v) or abs(v) > 50000 for v in (x, y, z)):
            return None
        u, v = struct.unpack_from("<ff", data, off + 16)
        positions.append((x, z, -y))  # Y-up -> Z-up
        uvs.append((u, 1.0 - v))

    # Strip -> triangle list
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

    # Center Y (in engine coords, before axis swap) for categorization
    ys = [struct.unpack_from("<f", data, vo + i * stride + 4)[0] for i in range(vc)]
    center_y = sum(ys) / len(ys)

    return {
        'positions': positions, 'uvs': uvs, 'indices': tris,
        'center_y': center_y,
    }


# ---------------------------------------------------------------------------
# Spatial clustering (union-find)
# ---------------------------------------------------------------------------
CLUSTER_DIST = 25.0  # merge chunks within this distance

def cluster_chunks(chunks_with_centers):
    """Group chunks by spatial proximity. Returns list of lists."""
    n = len(chunks_with_centers)
    if n <= 1:
        return [list(range(n))]

    parent = list(range(n))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        parent[find(a)] = find(b)

    for i in range(n):
        ci = chunks_with_centers[i]
        for j in range(i + 1, n):
            cj = chunks_with_centers[j]
            dx = ci[0] - cj[0]
            dy = ci[1] - cj[1]
            dz = ci[2] - cj[2]
            if dx*dx + dy*dy + dz*dz < CLUSTER_DIST * CLUSTER_DIST:
                union(i, j)

    groups = defaultdict(list)
    for i in range(n):
        groups[find(i)].append(i)
    return list(groups.values())


# ---------------------------------------------------------------------------
# Export
# ---------------------------------------------------------------------------
UNDERGROUND_Y = -10.0  # engine Y threshold for underground props

def export_world(pcwb_path, output_dir=None):
    with open(pcwb_path, "rb") as f:
        data = f.read()

    if data[:4] != b"PCWB":
        print(f"  Not a PCWB file")
        return False

    level_name = os.path.basename(os.path.dirname(pcwb_path))
    if not level_name or level_name == ".":
        level_name = os.path.basename(pcwb_path).replace(".pcwb", "")

    if output_dir is None:
        output_dir = os.path.dirname(pcwb_path)
    os.makedirs(output_dir, exist_ok=True)
    tex_dir = os.path.join(output_dir, "textures")
    os.makedirs(tex_dir, exist_ok=True)

    obj_path = os.path.join(output_dir, f"{level_name}_world.obj")
    mtl_path = os.path.join(output_dir, f"{level_name}_world.mtl")

    # Parse
    pcims = find_pcims_validated(data)
    pcrds = find_pcrds_validated(data)
    tex_ref = build_texture_ref_table(data, pcims)
    batch_map = build_pcrd_texture_map(data, pcrds)

    print(f"  {len(pcrds)} PCRD, {len(pcims)} PCIM, {len(tex_ref)} tex mappings")

    # Parse all chunks
    parsed = []  # (pcrd_off, tex_idx, chunk_data)
    for pcrd_off in pcrds:
        chunk = parse_pcrd(data, pcrd_off)
        if chunk is None:
            continue
        ti = batch_map.get(pcrd_off, -1)
        if ti not in tex_ref:
            ti = -1
        parsed.append((pcrd_off, ti, chunk))

    # Group by texture
    by_tex = defaultdict(list)
    for i, (off, ti, chunk) in enumerate(parsed):
        by_tex[ti].append(i)

    # Extract DDS textures and detect properties
    tex_files = {}
    tex_has_alpha = {}  # ti -> bool
    tex_dims = {}       # ti -> (w, h)
    tex_is_shader = {}  # ti -> bool (small gradient/LUT textures)
    for ti in sorted(by_tex.keys()):
        if ti < 0:
            continue
        pcim_off = tex_ref.get(ti)
        if pcim_off is None:
            continue
        dds, w, h = extract_dds(data, pcim_off)
        if dds:
            fname = f"tex_{ti:03d}_{w}x{h}.dds"
            with open(os.path.join(tex_dir, fname), "wb") as f:
                f.write(dds)
            tex_files[ti] = fname
            tex_dims[ti] = (w, h)

            # Detect shader/technical texture:
            # - Tiny gradient ramps/LUTs: max dimension <= 32
            # - 64x32 or 32x64 could be real tiled textures, don't flag those
            max_dim = max(w, h)
            min_dim = min(w, h)
            is_shader = (max_dim <= 32) or (min_dim <= 8 and max_dim <= 64)
            tex_is_shader[ti] = is_shader

            # Check if alpha is actually used (not all 255)
            has_alpha = False
            if len(dds) > 128:
                pf_flags = struct.unpack_from("<I", dds, 80)[0]
                bpp = struct.unpack_from("<I", dds, 88)[0]
                if bpp == 32 and (pf_flags & 0x41):  # RGBA
                    pixel_start = 128
                    pixel_end = min(len(dds), 128 + w * h * 4)
                    for p in range(pixel_start + 3, pixel_end, 4):
                        if dds[p] < 250:
                            has_alpha = True
                            break
            tex_has_alpha[ti] = has_alpha

    # Build objects: per texture, per spatial island, categorized
    # Categories: srf (surface geometry), ugr (underground props), shd (shader/fx meshes)
    objects = []  # list of (name, tex_idx, [chunk_indices])

    for ti in sorted(by_tex.keys()):
        chunk_indices = by_tex[ti]
        if not chunk_indices:
            continue

        is_shader_tex = tex_is_shader.get(ti, False)

        # Categorize each chunk
        cat_buckets = {"srf": [], "ugr": [], "shd": []}
        for ci in chunk_indices:
            _, _, chunk = parsed[ci]
            n_verts = len(chunk['positions'])
            n_tris = len(chunk['indices']) // 3

            if is_shader_tex:
                # Small texture = shader object regardless of position
                cat_buckets["shd"].append(ci)
            elif chunk['center_y'] < UNDERGROUND_Y:
                cat_buckets["ugr"].append(ci)
            elif n_verts <= 4 and n_tris <= 2:
                # Single quad = likely decal/shadow plane/fx
                cat_buckets["shd"].append(ci)
            else:
                cat_buckets["srf"].append(ci)

        # Cluster each category spatially
        for category, indices in cat_buckets.items():
            if not indices:
                continue

            centers = []
            for ci in indices:
                _, _, chunk = parsed[ci]
                ps = chunk['positions']
                cx = sum(p[0] for p in ps) / len(ps)
                cy = sum(p[1] for p in ps) / len(ps)
                cz = sum(p[2] for p in ps) / len(ps)
                centers.append((cx, cy, cz))

            clusters = cluster_chunks(centers)

            for gi, cluster in enumerate(clusters):
                cis = [indices[k] for k in cluster]
                tex_label = f"tex{ti:03d}" if ti >= 0 else "notex"
                if len(clusters) == 1:
                    name = f"{category}_{tex_label}"
                else:
                    name = f"{category}_{tex_label}_g{gi}"
                objects.append((name, ti, cis))

    # Sort: surface first, then shader, then underground
    sort_key = {"srf": 0, "shd": 1, "ugr": 2}
    objects.sort(key=lambda o: (sort_key.get(o[0][:3], 3), o[0]))

    total_verts = sum(len(parsed[ci][2]['positions']) for _, _, cis in objects for ci in cis)
    total_tris = sum(len(parsed[ci][2]['indices']) // 3 for _, _, cis in objects for ci in cis)
    srf_count = sum(1 for o in objects if o[0].startswith("srf"))
    shd_count = sum(1 for o in objects if o[0].startswith("shd"))
    ugr_count = sum(1 for o in objects if o[0].startswith("ugr"))

    print(f"  {len(objects)} objects ({srf_count} surface, {shd_count} shader, {ugr_count} underground)")
    print(f"  {total_verts:,} verts, {total_tris:,} tris, {len(tex_files)} textures")

    # Write MTL
    mtl_name = os.path.basename(mtl_path)
    with open(mtl_path, "w") as f:
        for ti in sorted(tex_files.keys()):
            fname = tex_files[ti]
            f.write(f"newmtl tex_{ti:03d}\n")
            f.write(f"Ka 1.0 1.0 1.0\nKd 1.0 1.0 1.0\nKs 0.0 0.0 0.0\n")
            f.write(f"illum 2\nd 1.0\n")
            f.write(f"map_Kd textures/{fname}\n")
            if tex_has_alpha.get(ti, False):
                f.write(f"map_d textures/{fname}\n")
            f.write(f"\n")
        f.write(f"newmtl unmapped\n")
        f.write(f"Ka 0.5 0.5 0.5\nKd 0.5 0.5 0.5\n\n")

    # Write OBJ
    with open(obj_path, "w") as f:
        f.write(f"# Spiderwick Chronicles - {level_name}\n")
        f.write(f"# {len(objects)} objects, {total_verts:,} verts, {total_tris:,} tris\n")
        f.write(f"mtllib {mtl_name}\n\n")

        global_v = 0  # OBJ 1-based vertex counter

        for obj_name, ti, chunk_indices in objects:
            f.write(f"o {obj_name}\n")
            mat_name = f"tex_{ti:03d}" if ti >= 0 and ti in tex_files else "unmapped"
            f.write(f"usemtl {mat_name}\n")

            obj_base = global_v
            # Write all vertices and UVs for this object's chunks
            for ci in chunk_indices:
                _, _, chunk = parsed[ci]
                for p in chunk['positions']:
                    f.write(f"v {p[0]:.6f} {p[1]:.6f} {p[2]:.6f}\n")
                for uv in chunk['uvs']:
                    f.write(f"vt {uv[0]:.6f} {uv[1]:.6f}\n")

            # Write faces
            chunk_base = obj_base
            for ci in chunk_indices:
                _, _, chunk = parsed[ci]
                indices = chunk['indices']
                for i in range(0, len(indices), 3):
                    i0 = chunk_base + indices[i] + 1
                    i1 = chunk_base + indices[i+1] + 1
                    i2 = chunk_base + indices[i+2] + 1
                    f.write(f"f {i0}/{i0} {i1}/{i1} {i2}/{i2}\n")
                chunk_base += len(chunk['positions'])

            global_v = chunk_base
            f.write(f"\n")

    sz = os.path.getsize(obj_path) / (1024 * 1024)
    print(f"  -> {obj_path} ({sz:.1f} MB)")
    return True


def main():
    parser = argparse.ArgumentParser(description="Spiderwick world OBJ exporter v3")
    parser.add_argument("input", help="world.pcwb file or directory")
    parser.add_argument("output", nargs="?", help="output dir")
    parser.add_argument("--batch", action="store_true")
    args = parser.parse_args()

    if args.batch or os.path.isdir(args.input):
        base = args.input
        for d in sorted(os.listdir(base)):
            pcwb = os.path.join(base, d, "world.pcwb")
            if os.path.exists(pcwb):
                print(f"\n{d}:")
                export_world(pcwb, os.path.join(base, d))
    else:
        out = args.output or os.path.dirname(args.input)
        export_world(args.input, out)


if __name__ == "__main__":
    main()
