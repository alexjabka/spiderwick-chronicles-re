"""
Noesis plugin — Spiderwick Chronicles PCWB/PCW World Importer
=============================================================
Loads world geometry with textures, props, and optional scene dump transforms.

Install: copy to Noesis/plugins/python/
Supports: .pcwb (raw), .pcw (zlib-compressed)
"""

from inc_noesis import *
import struct
import os
import math
import zlib

def registerNoesisTypes():
    handle = noesis.register("Spiderwick World [PCWB]", ".pcwb")
    noesis.setHandlerTypeCheck(handle, pcwbCheckType)
    noesis.setHandlerLoadModel(handle, pcwbLoadModel)

    handle2 = noesis.register("Spiderwick World [PCW]", ".pcw")
    noesis.setHandlerTypeCheck(handle2, pcwCheckType)
    noesis.setHandlerLoadModel(handle2, pcwLoadModel)
    return 1


# ============================================================================
# Type checks
# ============================================================================

def pcwbCheckType(data):
    return 1 if data[:4] == b"PCWB" else 0

def pcwCheckType(data):
    # PCW files are zlib-compressed PCWB with a 12-byte header
    if len(data) < 16:
        return 0
    # Try decompressing first few bytes
    try:
        d = zlib.decompress(data[12:12+256])
        if d[:4] == b"PCWB":
            return 1
    except:
        pass
    return 0


# ============================================================================
# Helpers
# ============================================================================

def ru32(data, off):
    return struct.unpack_from("<I", data, off)[0]

def rf32(data, off):
    return struct.unpack_from("<f", data, off)[0]

def ru16(data, off):
    return struct.unpack_from("<H", data, off)[0]


# ============================================================================
# PCWB parsers (ported from spiderwick_world_export.py)
# ============================================================================

def find_pcims(data):
    pcims = []
    pos = 0
    while pos < len(data) - 0xC1:
        if data[pos:pos+4] == b"PCIM":
            ver = ru32(data, pos + 4)
            tsz = ru32(data, pos + 8)
            dsz = ru32(data, pos + 0x0C)
            doff = ru32(data, pos + 0x10)
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


def find_pcrds(data):
    pcrds = []
    pos = 0
    while pos < len(data) - 0x1C:
        if data[pos:pos+4] == b"PCRD" and ru32(data, pos + 4) == 2:
            ic = ru32(data, pos + 0x0C)
            vc = ru32(data, pos + 0x10)
            if 0 < ic < 1000000 and 0 < vc < 1000000:
                pcrds.append(pos)
        pos += 4
    return pcrds


def build_tex_ref_table(data, pcims):
    tex_ref = {}
    tp = ru32(data, 0x94)
    if 0 < tp < len(data):
        pos = tp
        while pos + 16 <= len(data):
            ti = ru32(data, pos)
            po = ru32(data, pos + 4)
            if po + 4 <= len(data) and data[po:po+4] == b"PCIM":
                tex_ref[ti] = po
            else:
                break
            pos += 16
    for po in pcims:
        if po in tex_ref.values():
            continue
        if po >= 16:
            pti = ru32(data, po - 16)
            ppo = ru32(data, po - 12)
            if ppo == po and ru32(data, po - 8) == 0 and ru32(data, po - 4) == 0:
                tex_ref[pti] = po
    return tex_ref


def build_pcrd_tex_map(data, pcrds):
    ps = set(pcrds)
    m = {}
    pos = 0
    while pos < len(data) - 12:
        if (ru32(data, pos) == 0xFFFFFFFF and
            ru32(data, pos+4) == 0xFFFFFFFF and
            ru32(data, pos+8) == 0xFFFFFFFF):
            if pos >= 4 and pos + 40 <= len(data):
                ti = ru32(data, pos - 4)
                pr = ru32(data, pos + 36)
                if pr in ps:
                    m[pr] = ti
            pos += 12
        else:
            pos += 4
    return m


def extract_dds(data, pcim_off):
    if data[pcim_off:pcim_off+4] != b"PCIM":
        return None, 0, 0
    dsz = ru32(data, pcim_off + 0x0C)
    doff = ru32(data, pcim_off + 0x10)
    w = ru32(data, pcim_off + 0x9C)
    h = ru32(data, pcim_off + 0xA0)
    if doff + dsz <= len(data) and data[doff:doff+4] == b"DDS ":
        return data[doff:doff+dsz], w, h
    rel = pcim_off + 0xC1
    if rel + dsz <= len(data) and data[rel:rel+4] == b"DDS ":
        return data[rel:rel+dsz], w, h
    return None, w, h


def parse_props(data):
    prop_count = ru32(data, 0x50)
    prop_table = ru32(data, 0x98)
    STRIDE = 0xA0
    if prop_count == 0 or prop_count > 1000 or prop_table == 0:
        return []
    props = []
    for pi in range(prop_count):
        entry = prop_table + pi * STRIDE
        if entry + STRIDE > len(data):
            break
        mat = [rf32(data, entry + j * 4) for j in range(16)]
        pos = (mat[12], mat[13], mat[14])

        name_off = entry + 0x60
        end = name_off
        while end < min(len(data), name_off + 44) and data[end] != 0:
            end += 1
        name = data[name_off:end].decode('ascii', errors='replace')

        def_ptr = ru32(data, entry + 0x8C)
        if def_ptr == 0 or def_ptr + 12 > len(data):
            continue
        pcrd_count = ru32(data, def_ptr + 4)
        mesh_list_off = ru32(data, def_ptr + 8)
        if pcrd_count == 0 or pcrd_count > 10000 or mesh_list_off == 0:
            continue

        pcrd_offsets = []
        for mi in range(pcrd_count):
            bpo = mesh_list_off + mi * 4
            if bpo + 4 > len(data):
                break
            block_off = ru32(data, bpo)
            if block_off + 20 > len(data):
                continue
            sub_count = ru32(data, block_off)
            sml = ru32(data, block_off + 16)
            if sub_count == 0 or sub_count > 100 or sml == 0:
                continue
            for si in range(sub_count):
                sme = sml + si * 4
                if sme + 4 > len(data):
                    break
                smp = ru32(data, sme)
                if smp + 16 > len(data):
                    continue
                bp = ru32(data, smp + 12)
                if bp + 48 > len(data):
                    continue
                po = ru32(data, bp + 44)
                if po + 4 <= len(data) and data[po:po+4] == b"PCRD":
                    pcrd_offsets.append(po)

        prop_type = ru32(data, def_ptr + 12) if def_ptr + 16 <= len(data) else 0
        props.append({
            'name': name,
            'matrix': mat,
            'position': pos,
            'pcrd_offsets': set(pcrd_offsets),
            'type': prop_type,
        })
    return props


# ============================================================================
# Scene dump loader (optional — from ASI mod's DUMP SCENE)
# ============================================================================

def load_scene_dump(scene_path):
    """Load instance transforms and rendered set from scene_*.txt"""
    rendered_keys = set()  # (vc, ic, (v0x, v0y, v0z))
    transforms = []         # (vc, ic, (v0x, v0y, v0z), mat[16])
    section = None
    try:
        with open(scene_path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if line.startswith('[rendered_pcrds]'):
                    section = 'rendered'; continue
                elif line.startswith('[instance_transforms]'):
                    section = 'transforms'; continue
                elif line.startswith('['):
                    section = None; continue

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
    except:
        pass
    return rendered_keys, transforms


# ============================================================================
# PCRD geometry parsing
# ============================================================================

def parse_pcrd_mesh(data, pcrd_off):
    """Parse a single PCRD chunk into vertex/index arrays.
    Returns (positions, uvs, colors, tri_indices) or None."""
    hs = ru32(data, pcrd_off + 0x08)
    ic = ru32(data, pcrd_off + 0x0C)
    vc = ru32(data, pcrd_off + 0x10)
    io = ru32(data, pcrd_off + 0x14)
    vo = ru32(data, pcrd_off + 0x18)
    stride = 32 if hs <= 0x10 else 24

    if vc == 0 or ic == 0:
        return None
    if vo + vc * stride > len(data) or io + ic * 2 > len(data):
        return None

    # Quick NaN check on first vertex
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
        u, vt = struct.unpack_from("<ff", data, off + 16)
        positions.append((x, y, z))
        uvs.append((u, vt))
        colors.append((r, g, b, a))

    # Triangle strip → triangle list
    raw = [ru16(data, io + i * 2) for i in range(ic)]
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

    return {
        'positions': positions,
        'uvs': uvs,
        'colors': colors,
        'indices': tris,
        'vc': vc, 'ic': ic,
        'first_vtx': (round(px, 3), round(py, 3), round(pz, 3)),
    }


# ============================================================================
# Noesis model builders
# ============================================================================

def submit_pcrd_mesh(rapi, mesh_data, name, mat_name, transform=None):
    """Submit a parsed PCRD as a Noesis mesh via rapi."""
    positions = mesh_data['positions']
    uvs = mesh_data['uvs']
    colors = mesh_data['colors']
    indices = mesh_data['indices']

    # Apply transform if given (4x4 row-major, row-vector convention)
    if transform:
        m = transform
        transformed = []
        for (x, y, z) in positions:
            wx = x * m[0] + y * m[4] + z * m[8]  + m[12]
            wy = x * m[1] + y * m[5] + z * m[9]  + m[13]
            wz = x * m[2] + y * m[6] + z * m[10] + m[14]
            transformed.append((wx, wy, wz))
        positions = transformed

    # Pack position buffer
    posBuf = bytes()
    for (x, y, z) in positions:
        posBuf += struct.pack("<fff", x, y, z)

    # Pack UV buffer
    uvBuf = bytes()
    for (u, v) in uvs:
        uvBuf += struct.pack("<ff", u, v)

    # Pack color buffer (RGBA ubyte)
    colBuf = bytes()
    for (r, g, b, a) in colors:
        colBuf += struct.pack("BBBB", r, g, b, a)

    # Pack index buffer
    idxBuf = bytes()
    for idx in indices:
        idxBuf += struct.pack("<H", idx)

    rapi.rpgSetName(name)
    rapi.rpgSetMaterial(mat_name)

    rapi.rpgBindPositionBuffer(posBuf, noesis.RPGEODATA_FLOAT, 12)
    rapi.rpgBindUV1Buffer(uvBuf, noesis.RPGEODATA_FLOAT, 8)
    rapi.rpgBindColorBuffer(colBuf, noesis.RPGEODATA_UBYTE, 4, 4)
    rapi.rpgCommitTriangles(idxBuf, noesis.RPGEODATA_USHORT, len(indices), noesis.RPGEO_TRIANGLE)


# ============================================================================
# Main load handler
# ============================================================================

def pcwLoadModel(data, mdlList):
    """Load a zlib-compressed .pcw file."""
    try:
        pcwb_data = zlib.decompress(data[12:])
    except:
        noesis.logError("Failed to decompress PCW file")
        return 0
    return _loadPCWB(pcwb_data, mdlList)


def pcwbLoadModel(data, mdlList):
    """Load a raw .pcwb file."""
    return _loadPCWB(data, mdlList)


def _loadPCWB(data, mdlList):
    if data[:4] != b"PCWB":
        noesis.logError("Not a PCWB file")
        return 0

    rapi.rpgCreateContext()

    # ---- Parse all sections ----
    noesis.logPopup()
    print("Parsing PCWB (%d bytes)..." % len(data))

    pcims = find_pcims(data)
    pcrds = find_pcrds(data)
    tex_ref = build_tex_ref_table(data, pcims)
    batch_map = build_pcrd_tex_map(data, pcrds)
    props = parse_props(data)

    print("  %d PCRDs, %d PCIMs, %d tex mappings, %d props" %
          (len(pcrds), len(pcims), len(tex_ref), len(props)))

    # ---- Build prop lookup ----
    prop_pcrd_map = {}     # pcrd_off → prop name
    prop_transform = {}    # pcrd_off → 4x4 matrix (row-major)
    for prop in props:
        for po in prop['pcrd_offsets']:
            prop_pcrd_map[po] = prop['name']
            prop_transform[po] = prop['matrix']

    # ---- Check for scene dump ----
    scene_path = None
    base_dir = rapi.getDirForFilePath(rapi.getLastCheckedName())
    for name_try in ["scene_GroundsD.txt", "scene_MansionD.txt", "scene_Shell.txt"]:
        p = os.path.join(base_dir, name_try)
        if os.path.exists(p):
            scene_path = p
            break
    # Also check game root
    if not scene_path:
        for d in [base_dir, os.path.dirname(base_dir), os.path.dirname(os.path.dirname(base_dir))]:
            for f in os.listdir(d) if os.path.isdir(d) else []:
                if f.startswith("scene_") and f.endswith(".txt"):
                    scene_path = os.path.join(d, f)
                    break
            if scene_path:
                break

    instance_transforms = {}  # (vc, ic, v0) → list of mat[16]
    rendered_set = None
    if scene_path:
        print("  Loading scene dump: %s" % scene_path)
        rendered_keys, inst_xforms = load_scene_dump(scene_path)
        if rendered_keys:
            # Build rendered set by matching to file PCRDs
            rendered_set = set()
            pcrd_v0_map = {}
            for po in pcrds:
                vc = ru32(data, po + 0x10)
                ic = ru32(data, po + 0x0C)
                vo = ru32(data, po + 0x18)
                if vo + 12 <= len(data):
                    v0 = (round(rf32(data, vo), 3), round(rf32(data, vo+4), 3), round(rf32(data, vo+8), 3))
                    key = (vc, ic, v0)
                    pcrd_v0_map[key] = po
            for key in rendered_keys:
                if key in pcrd_v0_map:
                    rendered_set.add(pcrd_v0_map[key])
            # Add all props to rendered set
            for po in prop_pcrd_map:
                rendered_set.add(po)
            print("  Rendered set: %d PCRDs" % len(rendered_set))

        # Build instance transform map
        for vc, ic, v0, mat in inst_xforms:
            key = (vc, ic, v0)
            po = pcrd_v0_map.get(key) if 'pcrd_v0_map' in dir() else None
            if po is not None:
                instance_transforms.setdefault(po, []).append(mat)
        if instance_transforms:
            total_inst = sum(len(v) for v in instance_transforms.values())
            print("  Instance transforms: %d instances of %d unique PCRDs" %
                  (total_inst, len(instance_transforms)))

    # ---- Load textures ----
    texList = []
    matList = []
    tex_loaded = {}  # tex_index → material name

    for ti in sorted(tex_ref.keys()):
        pcim_off = tex_ref[ti]
        dds_data, w, h = extract_dds(data, pcim_off)
        if dds_data:
            mat_name = "tex_%03d" % ti
            try:
                tex = NoeTexture(mat_name, w, h, rapi.loadTexByHandler(dds_data, ".dds"))
                texList.append(tex)
            except:
                tex = NoeTexture(mat_name, 1, 1, bytes([128, 128, 128, 255]))
                texList.append(tex)

            mat = NoeMaterial(mat_name, mat_name)
            mat.setFlags(noesis.NMATFLAG_TWOSIDED, 1)
            matList.append(mat)
            tex_loaded[ti] = mat_name

    # Default material for unmapped PCRDs
    mat_def = NoeMaterial("unmapped", "")
    mat_def.setDiffuseColor(NoeVec4([0.5, 0.5, 0.5, 1.0]))
    matList.append(mat_def)

    print("  Loaded %d textures, %d materials" % (len(texList), len(matList)))

    # ---- Submit geometry ----
    mesh_count = 0
    skipped = 0

    for pcrd_idx, pcrd_off in enumerate(pcrds):
        # Filter to rendered set if available
        if rendered_set is not None and pcrd_off not in rendered_set:
            skipped += 1
            continue

        mesh_data = parse_pcrd_mesh(data, pcrd_off)
        if mesh_data is None:
            continue

        # Determine material
        ti = batch_map.get(pcrd_off, -1)
        mat_name = tex_loaded.get(ti, "unmapped")

        # Determine name and transform
        pname = prop_pcrd_map.get(pcrd_off)

        if pcrd_off in instance_transforms:
            # Instanced geometry — submit multiple copies
            for inst_i, mat in enumerate(instance_transforms[pcrd_off]):
                name = "inst_%s_%d" % (pname or ("pcrd%d" % pcrd_idx), inst_i)
                submit_pcrd_mesh(rapi, mesh_data, name, mat_name, transform=mat)
                mesh_count += 1
        elif pname:
            # Prop with PCWB header transform
            name = "%s_pcrd%d" % (pname, pcrd_idx)
            transform = prop_transform.get(pcrd_off)
            submit_pcrd_mesh(rapi, mesh_data, name, mat_name, transform=transform)
            mesh_count += 1
        else:
            # World geometry at file position (identity transform)
            name = "world_%04d" % pcrd_idx
            submit_pcrd_mesh(rapi, mesh_data, name, mat_name)
            mesh_count += 1

    print("  Submitted %d meshes, skipped %d" % (mesh_count, skipped))

    mdl = rapi.rpgConstructModel()
    mdl.setModelMaterials(NoeModelMaterials(texList, matList))
    mdlList.append(mdl)

    print("Done! %d meshes loaded." % mesh_count)
    return 1
