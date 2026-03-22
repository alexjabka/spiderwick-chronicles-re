#!/usr/bin/env python3
"""
Match D3D9 captured WVP matrices to ALL PCRD chunks from PCWB file.

Reads:
  - PCWB world file → ALL PCRDs with first vertex positions
  - world_matrices_<level>.txt → captured WVP + first vertex per draw call

Matches by first vertex position (exact match from VB lock).
Computes World = WVP * VP⁻¹ for each matched PCRD.
Outputs prop_transforms_<level>.json for use by the world exporter.

Usage:
  python match_prop_transforms.py <world.pcwb> <world_matrices.txt>
"""

import struct, sys, os, json, math
from collections import Counter, defaultdict


def read_u32(data, off):
    return struct.unpack_from("<I", data, off)[0]

def read_f32(data, off):
    return struct.unpack_from("<f", data, off)[0]


# ---------------------------------------------------------------------------
# Pure-Python 4x4 matrix math
# ---------------------------------------------------------------------------
def mat4_mul(a, b):
    r = [0.0] * 16
    for i in range(4):
        for j in range(4):
            s = 0.0
            for k in range(4):
                s += a[i*4+k] * b[k*4+j]
            r[i*4+j] = s
    return r


def mat4_inv(m):
    a = list(m)
    inv = [0.0] * 16
    for i in range(4):
        inv[i*4+i] = 1.0
    for col in range(4):
        best = abs(a[col*4+col])
        best_row = col
        for row in range(col+1, 4):
            v = abs(a[row*4+col])
            if v > best:
                best = v
                best_row = row
        if best < 1e-12:
            return None
        if best_row != col:
            for j in range(4):
                a[col*4+j], a[best_row*4+j] = a[best_row*4+j], a[col*4+j]
                inv[col*4+j], inv[best_row*4+j] = inv[best_row*4+j], inv[col*4+j]
        pivot = a[col*4+col]
        for j in range(4):
            a[col*4+j] /= pivot
            inv[col*4+j] /= pivot
        for row in range(4):
            if row == col:
                continue
            factor = a[row*4+col]
            for j in range(4):
                a[row*4+j] -= factor * a[col*4+j]
                inv[row*4+j] -= factor * inv[col*4+j]
    return inv


def mat4_equal(a, b, tol=0.001):
    return all(abs(a[i] - b[i]) < tol for i in range(16))


def vec3_dist(a, b):
    return math.sqrt(sum((a[i]-b[i])**2 for i in range(3)))


def vtx_key(x, y, z):
    """Round first vertex to 2 decimal places for matching."""
    return (round(x, 2), round(y, 2), round(z, 2))


# ---------------------------------------------------------------------------
# Parse ALL PCRDs from PCWB file
# ---------------------------------------------------------------------------
def parse_all_pcrds(data):
    """Extract first vertex XYZ for every PCRD in the file."""
    result = {}
    pos = 0
    while pos < len(data) - 0x1C:
        if data[pos:pos+4] == b"PCRD" and read_u32(data, pos + 4) == 2:
            vc = read_u32(data, pos + 0x10)
            ic = read_u32(data, pos + 0x0C)
            vo = read_u32(data, pos + 0x18)
            if 0 < vc < 1000000 and 0 < ic < 1000000:
                stride = 32 if read_u32(data, pos + 0x08) <= 0x10 else 24
                first_vtx = (0.0, 0.0, 0.0)
                if vo + 12 <= len(data):
                    first_vtx = struct.unpack_from("<fff", data, vo)
                avg_y = read_f32(data, vo + 4) if vo + stride <= len(data) else 0
                result[pos] = {
                    'vc': vc,
                    'ic': ic,
                    'first_vtx': first_vtx,
                    'avg_y': avg_y,
                }
        pos += 4
    return result


def parse_prop_pcrds(data):
    """Get prop info for PCRDs that belong to props."""
    prop_count = read_u32(data, 0x50)
    prop_table = read_u32(data, 0x98)
    if prop_count == 0 or prop_count > 1000 or prop_table == 0:
        return {}

    result = {}
    for pi in range(prop_count):
        entry = prop_table + pi * 0xA0
        name_off = entry + 0x60
        end = name_off
        while end < len(data) and data[end] != 0:
            end += 1
        name = data[name_off:end].decode('ascii', errors='replace')

        def_ptr = read_u32(data, entry + 0x8C)
        if def_ptr == 0 or def_ptr + 16 > len(data):
            continue
        prop_type = read_u32(data, def_ptr + 12)
        pcrd_count = read_u32(data, def_ptr + 4)
        mesh_list_off = read_u32(data, def_ptr + 8)
        if pcrd_count == 0 or mesh_list_off == 0:
            continue

        pcrd_idx = 0
        for mi in range(pcrd_count):
            bpo = mesh_list_off + mi * 4
            if bpo + 4 > len(data): break
            block_off = read_u32(data, bpo)
            if block_off + 20 > len(data): continue
            sub_count = read_u32(data, block_off)
            submesh_list_ptr = read_u32(data, block_off + 16)
            if sub_count == 0 or submesh_list_ptr == 0: continue
            for si in range(sub_count):
                sm = submesh_list_ptr + si * 4
                if sm + 4 > len(data): break
                sp = read_u32(data, sm)
                if sp + 16 > len(data): continue
                bp = read_u32(data, sp + 12)
                if bp + 48 > len(data): continue
                po = read_u32(data, bp + 44)
                if po + 4 <= len(data) and data[po:po+4] == b"PCRD":
                    result[po] = {
                        'prop_name': name,
                        'prop_idx': pi,
                        'pcrd_sub_idx': pcrd_idx,
                        'prop_type': prop_type,
                    }
                    pcrd_idx += 1
    return result


# ---------------------------------------------------------------------------
# Parse captured WVP file (with first vertex)
# ---------------------------------------------------------------------------
def parse_captures(path):
    """Read world_matrices_<level>.txt → list of (numVerts, primCount, wvp, first_vtx)"""
    draws = []
    has_vtx = False
    with open(path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                if 'with vtx' in line:
                    has_vtx = True
                continue
            parts = line.split('\t')
            if len(parts) < 18:
                continue
            verts = int(parts[0])
            prims = int(parts[1])
            vals = [float(x) for x in parts[2:18]]
            mat = [
                vals[0], vals[4], vals[8],  vals[12],
                vals[1], vals[5], vals[9],  vals[13],
                vals[2], vals[6], vals[10], vals[14],
                vals[3], vals[7], vals[11], vals[15],
            ]
            first_vtx = None
            if len(parts) >= 21:
                vx, vy, vz = float(parts[18]), float(parts[19]), float(parts[20])
                if vx != 0.0 or vy != 0.0 or vz != 0.0:
                    first_vtx = (vx, vy, vz)
            draws.append((verts, prims, mat, first_vtx))

    n_with_vtx = sum(1 for d in draws if d[3] is not None)
    print(f"  {len(draws)} draws, {n_with_vtx} with first vertex data")
    return draws


# ---------------------------------------------------------------------------
# Find VP
# ---------------------------------------------------------------------------
def find_vp(draws):
    mat_counts = Counter()
    mat_map = {}
    for v, p, m, fv in draws:
        key = tuple(round(x, 4) for x in m)
        mat_counts[key] += 1
        mat_map[key] = m
    most_common = mat_counts.most_common(1)[0]
    vp = mat_map[most_common[0]]
    print(f"VP found: {most_common[1]} draws use it (of {len(draws)} total)")
    return vp


# ---------------------------------------------------------------------------
# Match by first vertex
# ---------------------------------------------------------------------------
def match_by_first_vertex(all_pcrds, draws, vp):
    """Match captured draws to PCRDs using first vertex position."""
    vp_inv = mat4_inv(vp)
    if vp_inv is None:
        print("ERROR: VP matrix is singular!")
        return {}

    # Build lookup: first_vtx_key → pcrd_offset
    vtx_to_pcrd = defaultdict(list)
    for off, info in all_pcrds.items():
        key = vtx_key(*info['first_vtx'])
        vtx_to_pcrd[key].append(off)

    # For non-VP draws with first vertex data, try matching
    matched = {}
    n_vp = 0
    n_no_vtx = 0
    n_matched = 0
    n_multi = 0
    n_miss = 0

    for verts, prims, wvp, first_vtx in draws:
        if mat4_equal(wvp, vp):
            n_vp += 1
            continue
        if first_vtx is None:
            n_no_vtx += 1
            continue

        key = vtx_key(*first_vtx)
        candidates = vtx_to_pcrd.get(key, [])

        if len(candidates) == 0:
            n_miss += 1
            continue

        world = mat4_mul(wvp, vp_inv)

        if len(candidates) == 1:
            off = candidates[0]
            if off not in matched:
                matched[off] = world
                n_matched += 1
            # else: duplicate draw for same PCRD (multi-pass), keep first
        else:
            # Multiple PCRDs with same first vertex — match by (vc, primCount)
            for off in candidates:
                info = all_pcrds[off]
                if info['vc'] == verts and info['ic'] - 2 == prims:
                    if off not in matched:
                        matched[off] = world
                        n_matched += 1
                    break
            else:
                n_multi += 1

    print(f"  VP draws: {n_vp}")
    print(f"  No vertex data: {n_no_vtx}")
    print(f"  Matched: {n_matched}")
    print(f"  Multi-candidate misses: {n_multi}")
    print(f"  No match: {n_miss}")

    return matched


def match_by_key_proximity(all_pcrds, prop_info, draws, vp):
    """Match by (vc, primCount) with PCWB-position proximity disambiguation."""
    vp_inv = mat4_inv(vp)
    if vp_inv is None:
        return {}

    non_vp = [(v, p, m) for v, p, m, fv in draws if not mat4_equal(m, vp)]

    # Only match prop PCRDs (they have PCWB positions for disambiguation)
    prop_pcrds = {off: info for off, info in all_pcrds.items() if off in prop_info}

    pcrd_by_key = defaultdict(list)
    for off, info in prop_pcrds.items():
        key = (info['vc'], info['ic'] - 2)
        pcrd_by_key[key].append(off)

    draws_by_key = defaultdict(list)
    for v, p, m in non_vp:
        draws_by_key[(v, p)].append(m)

    # Read PCWB matrices for proximity
    pcwb_pos = {}
    with open(sys.argv[1], 'rb') as f:
        data = f.read()
    prop_count = read_u32(data, 0x50)
    prop_table = read_u32(data, 0x98)
    for pi_idx in range(prop_count):
        entry = prop_table + pi_idx * 0xA0
        mat = [read_f32(data, entry + j * 4) for j in range(16)]
        pos = (mat[12], mat[13], mat[14])
        def_ptr = read_u32(data, entry + 0x8C)
        if def_ptr == 0 or def_ptr + 16 > len(data):
            continue
        pcrd_cnt = read_u32(data, def_ptr + 4)
        mesh_list = read_u32(data, def_ptr + 8)
        if pcrd_cnt == 0 or mesh_list == 0:
            continue
        for mi in range(pcrd_cnt):
            bpo = mesh_list + mi * 4
            if bpo + 4 > len(data): break
            bo = read_u32(data, bpo)
            if bo + 20 > len(data): continue
            sc = read_u32(data, bo)
            slp = read_u32(data, bo + 16)
            if sc == 0 or slp == 0: continue
            for si in range(sc):
                sm = slp + si * 4
                if sm + 4 > len(data): break
                sp = read_u32(data, sm)
                if sp + 16 > len(data): continue
                bp = read_u32(data, sp + 12)
                if bp + 48 > len(data): continue
                po = read_u32(data, bp + 44)
                if po + 4 <= len(data) and data[po:po+4] == b"PCRD":
                    pcwb_pos[po] = pos

    matched = {}
    for key, pcrd_list in pcrd_by_key.items():
        draw_list = draws_by_key.get(key, [])
        if not draw_list:
            continue

        if len(pcrd_list) == 1 and len(draw_list) == 1:
            world = mat4_mul(draw_list[0], vp_inv)
            matched[pcrd_list[0]] = world
        elif len(pcrd_list) <= len(draw_list):
            # Proximity: match each PCRD to closest draw by World translation vs PCWB pos
            used = set()
            for off in pcrd_list:
                ppos = pcwb_pos.get(off, (0, 0, 0))
                best_dist = float('inf')
                best_di = -1
                best_world = None
                for di, wvp in enumerate(draw_list):
                    if di in used:
                        continue
                    world = mat4_mul(wvp, vp_inv)
                    wpos = (world[12], world[13], world[14])
                    dist = vec3_dist(wpos, ppos)
                    if dist < best_dist:
                        best_dist = dist
                        best_di = di
                        best_world = world
                if best_world is not None and best_dist < 200:
                    used.add(best_di)
                    matched[off] = best_world
        else:
            # More PCRDs than draws — best-effort
            used = set()
            for off in pcrd_list:
                ppos = pcwb_pos.get(off, (0, 0, 0))
                best_dist = float('inf')
                best_di = -1
                best_world = None
                for di, wvp in enumerate(draw_list):
                    if di in used:
                        continue
                    world = mat4_mul(wvp, vp_inv)
                    wpos = (world[12], world[13], world[14])
                    dist = vec3_dist(wpos, ppos)
                    if dist < best_dist:
                        best_dist = dist
                        best_di = di
                        best_world = world
                if best_world is not None and best_dist < 200:
                    used.add(best_di)
                    matched[off] = best_world

    return matched


# ---------------------------------------------------------------------------
# Save
# ---------------------------------------------------------------------------
def save_transforms(path, matched, all_pcrds, prop_info):
    output = {}
    for off, world in matched.items():
        pinfo = prop_info.get(off, {})
        output[str(off)] = {
            'prop_name': pinfo.get('prop_name', ''),
            'prop_idx': pinfo.get('prop_idx', -1),
            'pcrd_sub_idx': pinfo.get('pcrd_sub_idx', -1),
            'prop_type': pinfo.get('prop_type', 0),
            'match_type': 'first_vertex' if all_pcrds[off].get('first_vtx') else 'key',
            'world_matrix': world,
        }
    with open(path, 'w') as f:
        json.dump(output, f, indent=2)
    return output


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    if len(sys.argv) < 3:
        print("Usage: python match_prop_transforms.py <world.pcwb> <world_matrices.txt>")
        sys.exit(1)

    pcwb_path = sys.argv[1]
    matrices_path = sys.argv[2]

    with open(pcwb_path, 'rb') as f:
        data = f.read()
    if data[:4] != b"PCWB":
        print("Not a PCWB file"); sys.exit(1)

    level_name = os.path.basename(os.path.dirname(pcwb_path))

    print(f"Parsing {pcwb_path}...")
    all_pcrds = parse_all_pcrds(data)
    prop_info = parse_prop_pcrds(data)
    n_underground = sum(1 for info in all_pcrds.values() if info['avg_y'] < -10)
    n_prop = len(prop_info)
    print(f"  {len(all_pcrds)} total PCRDs ({n_prop} prop, {len(all_pcrds)-n_prop} world)")
    print(f"  {n_underground} underground (Y < -10)")

    print(f"\nParsing {matrices_path}...")
    draws = parse_captures(matrices_path)

    vp = find_vp(draws)

    print("\nMatching...")
    # Phase 1: first-vertex matching (for draws that have VB data)
    has_vtx_data = any(d[3] is not None for d in draws)
    matched = {}
    if has_vtx_data:
        print("  Phase 1: first-vertex matching")
        matched = match_by_first_vertex(all_pcrds, draws, vp)
        print(f"  -> {len(matched)} PCRDs matched by first vertex")

    # Phase 2: (vc, primCount) + proximity for remaining prop PCRDs
    print("  Phase 2: key + proximity matching for props")
    matched2 = match_by_key_proximity(all_pcrds, prop_info, draws, vp)
    n_new = 0
    for off, world in matched2.items():
        if off not in matched:
            matched[off] = world
            n_new += 1
    print(f"  -> {n_new} additional PCRDs matched by key+proximity")

    # Identify VP-drawn PCRDs (surface world geometry at correct file positions)
    # For each (vc, primCount) key, count VP draws → include that many PCRDs
    # sorted by Y (surface first), skipping underground duplicates
    vp_draw_keys = Counter()
    for v, p, m, fv in draws:
        if mat4_equal(m, vp):
            vp_draw_keys[(v, p)] += 1

    pcrd_by_key = defaultdict(list)
    for off, info in all_pcrds.items():
        key = (info['vc'], info['ic'] - 2)
        pcrd_by_key[key].append((off, info['avg_y']))

    vp_drawn_pcrds = set()
    for key, n_draws in vp_draw_keys.items():
        candidates = pcrd_by_key.get(key, [])
        # Exclude already-matched non-VP PCRDs
        candidates = [(off, y) for off, y in candidates if off not in matched]
        # Sort by Y descending (surface first)
        candidates.sort(key=lambda x: -x[1])
        for off, y in candidates[:n_draws]:
            vp_drawn_pcrds.add(off)

    # Final transform map: non-VP matched + VP-drawn (identity, no entry needed)
    final = {}
    for off, world in matched.items():
        identity = [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]
        if not mat4_equal(world, identity, tol=0.01):
            final[off] = world

    print(f"\nResults:")
    print(f"  Non-VP matched (need transform): {len(final)} PCRDs")
    print(f"  VP-drawn (surface, no transform): {len(vp_drawn_pcrds)} PCRDs")
    print(f"  Total rendered: {len(final) + len(vp_drawn_pcrds)} PCRDs")
    print(f"  Undrawn (skip): {len(all_pcrds) - len(final) - len(vp_drawn_pcrds)} PCRDs")

    n_prop_matched = sum(1 for off in final if off in prop_info)
    n_world_matched = sum(1 for off in final if off not in prop_info)
    print(f"  Props with transform: {n_prop_matched}")
    print(f"  World geometry with transform: {n_world_matched}")

    out_path = os.path.join(os.path.dirname(pcwb_path), f"prop_transforms_{level_name}.json")
    output = save_transforms(out_path, final, all_pcrds, prop_info)

    # Save VP-drawn set (PCRDs that need no transform, just inclusion)
    vp_path = os.path.join(os.path.dirname(pcwb_path), f"vp_drawn_{level_name}.json")
    with open(vp_path, 'w') as f:
        json.dump(sorted(vp_drawn_pcrds), f)

    print(f"\nSaved {len(output)} transforms to {out_path}")
    print(f"Saved {len(vp_drawn_pcrds)} VP-drawn PCRDs to {vp_path}")


if __name__ == "__main__":
    main()
