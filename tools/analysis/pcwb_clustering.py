#!/usr/bin/env python3
"""Analyze how PCRD chunks cluster: by texture, by spatial proximity, by sector."""
import struct, sys, math, os
from collections import defaultdict

def r32(d, o): return struct.unpack_from("<I", d, o)[0]

def find_pcims(data):
    pcims = []
    pos = 0
    while pos < len(data) - 0xC1:
        if data[pos:pos+4] == b"PCIM":
            ver = r32(data, pos + 4)
            tsz = r32(data, pos + 8)
            dsz = r32(data, pos + 0x0C)
            doff = r32(data, pos + 0x10)
            if ver == 2 and 0 < tsz < len(data) and 0 < dsz <= tsz:
                ok = False
                if doff + 4 <= len(data) and data[doff:doff+4] == b"DDS ": ok = True
                elif pos + 0xC1 + 4 <= len(data) and data[pos+0xC1:pos+0xC1+4] == b"DDS ": ok = True
                if ok:
                    pcims.append(pos)
                    if doff == pos + 0xC1: pos += (0xC1 + dsz + 3) & ~3
                    else: pos += 0xC4
                    continue
        pos += 4
    return pcims

def build_tex_ref(data, pcims):
    tex_ref = {}
    tp = r32(data, 0x94)
    if 0 < tp < len(data):
        pos = tp
        while pos + 16 <= len(data):
            ti, po = r32(data, pos), r32(data, pos+4)
            if po + 4 <= len(data) and data[po:po+4] == b"PCIM":
                tex_ref[ti] = po
            else: break
            pos += 16
    for po in pcims:
        if po in tex_ref.values(): continue
        if po >= 16:
            pti, ppo = r32(data, po-16), r32(data, po-12)
            if ppo == po and r32(data, po-8) == 0 and r32(data, po-4) == 0:
                tex_ref[pti] = po
    return tex_ref

def build_batch_map(data, pcrds):
    ps = set(pcrds)
    m = {}
    pos = 0
    while pos < len(data) - 12:
        if r32(data, pos) == 0xFFFFFFFF and r32(data, pos+4) == 0xFFFFFFFF and r32(data, pos+8) == 0xFFFFFFFF:
            if pos >= 4 and pos + 40 <= len(data):
                ti, pr = r32(data, pos-4), r32(data, pos+36)
                if pr in ps: m[pr] = ti
            pos += 12
        else: pos += 4
    return m

def main(path):
    with open(path, "rb") as f: data = f.read()
    if data[:4] != b"PCWB": return

    # Find sections
    pcrds = []
    pos = 0
    while pos < len(data) - 0x1C:
        if data[pos:pos+4] == b"PCRD" and r32(data, pos+4) == 2:
            ic = r32(data, pos+0x0C)
            vc = r32(data, pos+0x10)
            if 0 < ic < 1000000 and 0 < vc < 1000000:
                pcrds.append(pos)
        pos += 4

    pcims = find_pcims(data)
    tex_ref = build_tex_ref(data, pcims)
    batch_map = build_batch_map(data, pcrds)

    # For each PCRD, compute center position and texture
    chunks = []
    for pcrd_off in pcrds:
        hs = r32(data, pcrd_off + 0x08)
        ic = r32(data, pcrd_off + 0x0C)
        vc = r32(data, pcrd_off + 0x10)
        vo = r32(data, pcrd_off + 0x18)
        stride = 32 if hs <= 0x10 else 24
        if vc == 0 or vo + vc * stride > len(data): continue

        # Compute AABB center
        px, py, pz = struct.unpack_from("<fff", data, vo)
        if any(math.isnan(v) or math.isinf(v) or abs(v) > 50000 for v in (px, py, pz)):
            continue
        min_x = max_x = px
        min_y = max_y = py
        min_z = max_z = pz
        bad = False
        for i in range(1, vc):
            off = vo + i * stride
            x, y, z = struct.unpack_from("<fff", data, off)
            if any(math.isnan(v) or math.isinf(v) or abs(v) > 50000 for v in (x, y, z)):
                bad = True; break
            min_x, max_x = min(min_x, x), max(max_x, x)
            min_y, max_y = min(min_y, y), max(max_y, y)
            min_z, max_z = min(min_z, z), max(max_z, z)
        if bad: continue

        cx = (min_x + max_x) / 2
        cy = (min_y + max_y) / 2
        cz = (min_z + max_z) / 2
        extent = max(max_x - min_x, max_y - min_y, max_z - min_z)

        ti = batch_map.get(pcrd_off, -1)
        chunks.append({
            'off': pcrd_off, 'tex': ti, 'vc': vc, 'ic': ic,
            'cx': cx, 'cy': cy, 'cz': cz,
            'min_y': min_y, 'max_y': max_y, 'extent': extent,
        })

    print(f"Chunks: {len(chunks)}")

    # Group by texture
    by_tex = defaultdict(list)
    for c in chunks:
        by_tex[c['tex']].append(c)

    print(f"Unique textures: {len(by_tex)}")

    # Analyze spatial clustering within each texture group
    print(f"\n--- Texture groups with spatial analysis ---")
    for ti in sorted(by_tex.keys()):
        group = by_tex[ti]
        # Compute overall Y range (engine Y = vertical)
        ys = [c['cy'] for c in group]
        min_gy = min(ys)
        max_gy = max(ys)
        underground = sum(1 for c in group if c['cy'] < -10)  # below Y=-10 is likely underground

        pcim_off = tex_ref.get(ti)
        w = r32(data, pcim_off + 0x9C) if pcim_off and pcim_off + 0xA4 <= len(data) else 0
        h = r32(data, pcim_off + 0xA0) if pcim_off and pcim_off + 0xA4 <= len(data) else 0

        if len(group) >= 3 or underground > 0:
            print(f"  tex={ti:3d} ({w:4d}x{h:<4d}): {len(group):3d} chunks  "
                  f"Y=[{min_gy:.0f}..{max_gy:.0f}]  underground={underground}")

    # Overall statistics
    total_underground = sum(1 for c in chunks if c['cy'] < -10)
    total_surface = len(chunks) - total_underground
    print(f"\n--- Summary ---")
    print(f"  Surface chunks (Y >= -10): {total_surface}")
    print(f"  Underground chunks (Y < -10): {total_underground}")

    # Spatial clustering: how many distinct "islands" per texture?
    # Simple: group by proximity (chunks within 5 units of each other)
    print(f"\n--- Spatial islands per texture (distance threshold = 20) ---")
    for ti in sorted(by_tex.keys()):
        group = by_tex[ti]
        if len(group) < 2: continue

        # Simple union-find clustering by distance
        parent = list(range(len(group)))
        def find(x):
            while parent[x] != x: parent[x] = parent[parent[x]]; x = parent[x]
            return x
        def union(a, b): parent[find(a)] = find(b)

        for i in range(len(group)):
            for j in range(i+1, len(group)):
                dx = group[i]['cx'] - group[j]['cx']
                dy = group[i]['cy'] - group[j]['cy']
                dz = group[i]['cz'] - group[j]['cz']
                dist = math.sqrt(dx*dx + dy*dy + dz*dz)
                if dist < 20:
                    union(i, j)

        clusters = defaultdict(list)
        for i in range(len(group)):
            clusters[find(i)].append(i)

        if len(clusters) > 1 and len(clusters) <= 10:
            sizes = sorted([len(v) for v in clusters.values()], reverse=True)
            print(f"  tex={ti:3d}: {len(group)} chunks -> {len(clusters)} islands: {sizes}")

main(sys.argv[1])
