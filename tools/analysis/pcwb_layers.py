#!/usr/bin/env python3
"""Detect terrain layers: chunks with overlapping XZ footprints but different textures."""
import struct, sys, math
from collections import defaultdict

def r32(d, o): return struct.unpack_from("<I", d, o)[0]

def find_pcrds(data):
    pcrds = []
    pos = 0
    while pos < len(data) - 0x1C:
        if data[pos:pos+4] == b"PCRD" and r32(data, pos+4) == 2:
            ic, vc = r32(data, pos+0x0C), r32(data, pos+0x10)
            if 0 < ic < 1000000 and 0 < vc < 1000000:
                pcrds.append(pos)
        pos += 4
    return pcrds

def find_pcims(data):
    pcims = []
    pos = 0
    while pos < len(data) - 0xC1:
        if data[pos:pos+4] == b"PCIM":
            ver, tsz, dsz = r32(data, pos+4), r32(data, pos+8), r32(data, pos+0x0C)
            doff = r32(data, pos+0x10)
            if ver == 2 and 0 < tsz < len(data) and 0 < dsz <= tsz:
                ok = (doff+4 <= len(data) and data[doff:doff+4] == b"DDS ") or \
                     (pos+0xC1+4 <= len(data) and data[pos+0xC1:pos+0xC1+4] == b"DDS ")
                if ok:
                    pcims.append(pos)
                    pos += 0xC4 if doff != pos+0xC1 else ((0xC1+dsz+3) & ~3)
                    continue
        pos += 4
    return pcims

def build_tex_ref(data, pcims):
    tr = {}
    tp = r32(data, 0x94)
    if 0 < tp < len(data):
        pos = tp
        while pos+16 <= len(data):
            ti, po = r32(data, pos), r32(data, pos+4)
            if po+4 <= len(data) and data[po:po+4] == b"PCIM": tr[ti] = po
            else: break
            pos += 16
    for po in pcims:
        if po in tr.values(): continue
        if po >= 16:
            pti, ppo = r32(data, po-16), r32(data, po-12)
            if ppo == po and r32(data, po-8) == 0 and r32(data, po-4) == 0:
                tr[pti] = po
    return tr

def build_batch(data, pcrds):
    ps = set(pcrds); m = {}
    pos = 0
    while pos < len(data)-12:
        if r32(data,pos)==0xFFFFFFFF and r32(data,pos+4)==0xFFFFFFFF and r32(data,pos+8)==0xFFFFFFFF:
            if pos >= 4 and pos+40 <= len(data):
                ti, pr = r32(data, pos-4), r32(data, pos+36)
                if pr in ps: m[pr] = ti
            pos += 12
        else: pos += 4
    return m

def main(path):
    with open(path, "rb") as f: data = f.read()
    pcrds = find_pcrds(data)
    pcims = find_pcims(data)
    tr = build_tex_ref(data, pcims)
    bm = build_batch(data, pcrds)

    # Parse chunks: compute XZ bounding box + center Y
    chunks = []
    for pcrd_off in pcrds:
        hs = r32(data, pcrd_off+0x08)
        vc = r32(data, pcrd_off+0x10)
        vo = r32(data, pcrd_off+0x18)
        stride = 32 if hs <= 0x10 else 24
        if vc == 0 or vo+vc*stride > len(data): continue

        xs, ys, zs = [], [], []
        bad = False
        for i in range(vc):
            off = vo + i*stride
            x, y, z = struct.unpack_from("<fff", data, off)
            if any(math.isnan(v) or math.isinf(v) or abs(v) > 50000 for v in (x,y,z)):
                bad = True; break
            xs.append(x); ys.append(y); zs.append(z)
        if bad or not xs: continue

        ti = bm.get(pcrd_off, -1)
        if ti not in tr: ti = -1

        chunks.append({
            'off': pcrd_off, 'tex': ti, 'vc': vc,
            'xmin': min(xs), 'xmax': max(xs),
            'ymin': min(ys), 'ymax': max(ys),  # engine Y = vertical
            'zmin': min(zs), 'zmax': max(zs),
            'ycenter': sum(ys)/len(ys),
        })

    print(f"Parsed {len(chunks)} chunks")

    # Find overlapping chunks: same XZ footprint, different textures
    # Grid-based spatial hashing for XZ overlap detection
    CELL = 5.0  # grid cell size
    grid = defaultdict(list)
    for i, c in enumerate(chunks):
        # Hash the center XZ
        gx = int(c['xmin']/CELL + c['xmax']/CELL) // 2
        gz = int(c['zmin']/CELL + c['zmax']/CELL) // 2
        grid[(gx, gz)].append(i)

    # Find cells with multiple chunks at similar XZ but different textures
    overlap_groups = []  # list of (cell, [chunk indices])
    for cell, indices in grid.items():
        if len(indices) < 2: continue
        # Group by Y level (chunks at same Y = same layer, different Y = different layers)
        by_y = defaultdict(list)
        for ci in indices:
            # Quantize Y to 2-unit bins
            ybin = round(chunks[ci]['ycenter'] / 2.0)
            by_y[ybin].append(ci)

        # If multiple Y levels exist at same XZ = terrain layers
        if len(by_y) >= 2:
            overlap_groups.append((cell, indices, by_y))

    print(f"Found {len(overlap_groups)} cells with overlapping layers")

    # Analyze the texture patterns in overlapping groups
    layer_textures = defaultdict(int)  # tex_idx -> count of appearances in overlap groups
    layer_y_stats = defaultdict(list)  # tex_idx -> list of Y centers

    for cell, indices, by_y in overlap_groups:
        sorted_layers = sorted(by_y.keys())
        for layer_num, ybin in enumerate(sorted_layers):
            for ci in by_y[ybin]:
                ti = chunks[ci]['tex']
                layer_textures[ti] += 1
                layer_y_stats[ti].append((chunks[ci]['ycenter'], layer_num))

    # For each texture, determine which layer it typically belongs to
    print(f"\n--- Texture layer assignments ---")
    tex_layer = {}  # tex_idx -> most common layer number
    for ti in sorted(layer_textures.keys()):
        if ti < 0: continue
        layers = [ln for _, ln in layer_y_stats[ti]]
        if not layers: continue
        avg_layer = sum(layers) / len(layers)
        # Also get Y stats
        ys = [y for y, _ in layer_y_stats[ti]]
        avg_y = sum(ys) / len(ys)

        pcim_off = tr.get(ti)
        w = r32(data, pcim_off+0x9C) if pcim_off and pcim_off+0xA4 <= len(data) else 0
        h = r32(data, pcim_off+0xA0) if pcim_off and pcim_off+0xA4 <= len(data) else 0

        assigned = round(avg_layer)
        tex_layer[ti] = assigned
        count = layer_textures[ti]
        if count >= 5:
            print(f"  tex={ti:3d} ({w:4d}x{h:<4d}): layer={assigned} (avg={avg_layer:.1f})  "
                  f"avgY={avg_y:.0f}  count={count}")

    # Show unique layer combinations found
    print(f"\n--- Layer combinations at overlap cells (sample) ---")
    combo_count = defaultdict(int)
    for cell, indices, by_y in overlap_groups[:200]:
        sorted_layers = sorted(by_y.keys())
        combo = []
        for ybin in sorted_layers:
            texs = tuple(sorted(set(chunks[ci]['tex'] for ci in by_y[ybin])))
            combo.append(texs)
        combo_count[tuple(combo)] += 1

    for combo, count in sorted(combo_count.items(), key=lambda x: -x[1])[:20]:
        layers_str = " | ".join(str(list(c)) for c in combo)
        print(f"  x{count:3d}: {layers_str}")

main(sys.argv[1])
