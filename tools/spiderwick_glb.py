#!/usr/bin/env python3
"""
Spiderwick Chronicles — Skinned Mesh glTF Exporter
====================================================
Exports NM40 (mesh) + aniz (skeleton) pairs as .glb files with:
  - Skeleton hierarchy from 'hier' section in aniz
  - Skinned mesh from NM40 (positions, normals, UVs, bone weights)
  - Inverse bind matrices from hier per-bone data

Usage:
  python spiderwick_glb.py <aniz_file> <nm40_file> [output.glb]
  python spiderwick_glb.py --batch <wad_dir>   # auto-pair and export all
"""

import struct
import sys
import os
import math
import json
import base64
import argparse


def read_u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def read_i8(data, off):
    return struct.unpack_from("<b", data, off)[0]


def read_f32(data, off):
    return struct.unpack_from("<f", data, off)[0]


# ---------------------------------------------------------------------------
# Parse aniz → skeleton (hier section)
# ---------------------------------------------------------------------------
def parse_skeleton(aniz_data):
    """Parse skeleton from aniz file's hier section.
    Returns dict with 'bone_count', 'parents', 'local_matrices', 'world_matrices', 'inv_bind_matrices'."""
    if aniz_data[:4] != b"aniz":
        raise ValueError("Not an aniz file")

    hier_off = 0x40
    if aniz_data[hier_off : hier_off + 4] != b"hier":
        raise ValueError("No hier section at expected offset")

    bone_count = read_u32(aniz_data, hier_off + 8)
    hdr_size = read_u32(aniz_data, hier_off + 0xC)  # 64
    sec1_rel = read_u32(aniz_data, hier_off + 0x10)

    # Bone transform data: 192 bytes per bone (3x float4x4)
    data_start = hier_off + hdr_size
    stride = 192  # 3 * 16 floats * 4 bytes

    local_mats = []
    world_mats = []
    inv_bind_mats = []

    for i in range(bone_count):
        off = data_start + i * stride
        local = [read_f32(aniz_data, off + j * 4) for j in range(16)]
        world = [read_f32(aniz_data, off + 64 + j * 4) for j in range(16)]
        inv_bind = [read_f32(aniz_data, off + 128 + j * 4) for j in range(16)]
        local_mats.append(local)
        world_mats.append(world)
        inv_bind_mats.append(inv_bind)

    # Parent indices: uint8 array at sec1
    sec1_abs = hier_off + sec1_rel
    parents = []
    for i in range(bone_count):
        p = read_i8(aniz_data, sec1_abs + i)
        parents.append(p)

    return {
        "bone_count": bone_count,
        "parents": parents,
        "local_matrices": local_mats,
        "world_matrices": world_mats,
        "inv_bind_matrices": inv_bind_mats,
    }


# ---------------------------------------------------------------------------
# Parse NM40 → mesh
# ---------------------------------------------------------------------------
NM40_STRIDE = 52


def parse_mesh(nm40_data):
    """Parse mesh from NM40 file. Returns dict with vertices, normals, uvs, bone data, indices."""
    if nm40_data[:4] != b"NM40":
        raise ValueError("Not an NM40 file")

    # Find PCRD
    pcrd_off = None
    for pos in range(0x40, min(len(nm40_data), 0x400), 4):
        if nm40_data[pos : pos + 4] == b"PCRD":
            pcrd_off = pos
            break
    if pcrd_off is None:
        raise ValueError("No PCRD section found")

    idx_count = read_u32(nm40_data, pcrd_off + 12)
    vtx_count = read_u32(nm40_data, pcrd_off + 16)
    idx_off = read_u32(nm40_data, pcrd_off + 20)
    vtx_off = read_u32(nm40_data, pcrd_off + 24)

    positions = []
    normals = []
    uvs = []
    joints = []  # ubyte4 bone indices
    weights = []  # float4 bone weights

    for i in range(vtx_count):
        off = vtx_off + i * NM40_STRIDE
        px, py, pz = struct.unpack_from("<fff", nm40_data, off)
        nx, ny, nz = struct.unpack_from("<fff", nm40_data, off + 12)
        u, v = struct.unpack_from("<ff", nm40_data, off + 24)
        # Bone indices at +32 (4 bytes, D3DCOLOR encoded = BGRA order)
        bi = nm40_data[off + 32 : off + 36]
        # Bone weights at +36 (4 floats)
        bw = struct.unpack_from("<ffff", nm40_data, off + 36)

        positions.append((px, py, pz))
        normals.append((nx, ny, nz))
        uvs.append((u, 1.0 - v))  # flip V for glTF
        # D3DCOLORtoUBYTE4 swizzle: BGRA → ARGB → indices x,y,z,w
        joints.append((bi[2], bi[1], bi[0], bi[3]))
        weights.append(bw)

    indices = []
    for i in range(idx_count):
        indices.append(struct.unpack_from("<H", nm40_data, idx_off + i * 2)[0])

    return {
        "vertex_count": vtx_count,
        "index_count": idx_count,
        "positions": positions,
        "normals": normals,
        "uvs": uvs,
        "joints": joints,
        "weights": weights,
        "indices": indices,
    }


# ---------------------------------------------------------------------------
# Build glTF 2.0 binary (.glb)
# ---------------------------------------------------------------------------
def pack_floats(float_list):
    return b"".join(struct.pack("<f", f) for f in float_list)


def pack_vec3_list(vecs):
    buf = bytearray()
    for v in vecs:
        buf += struct.pack("<fff", *v)
    return bytes(buf)


def pack_vec2_list(vecs):
    buf = bytearray()
    for v in vecs:
        buf += struct.pack("<ff", *v)
    return bytes(buf)


def pack_vec4_list(vecs):
    buf = bytearray()
    for v in vecs:
        buf += struct.pack("<ffff", *v)
    return bytes(buf)


def pack_ubyte4_list(vecs):
    buf = bytearray()
    for v in vecs:
        buf += bytes([v[0], v[1], v[2], v[3]])
    return bytes(buf)


def pack_u16_list(vals):
    buf = bytearray()
    for v in vals:
        buf += struct.pack("<H", v)
    return bytes(buf)


def pack_mat4_list(mats):
    """Pack list of 16-float matrices as column-major (glTF convention)."""
    buf = bytearray()
    for m in mats:
        # Engine stores row-major, glTF needs column-major
        # m[row*4+col] → col-major: m[col*4+row]
        cm = [
            m[0], m[4], m[8], m[12],
            m[1], m[5], m[9], m[13],
            m[2], m[6], m[10], m[14],
            m[3], m[7], m[11], m[15],
        ]
        buf += struct.pack("<16f", *cm)
    return bytes(buf)


def compute_bounds(positions):
    if not positions:
        return [0, 0, 0], [0, 0, 0]
    mins = [min(p[i] for p in positions) for i in range(3)]
    maxs = [max(p[i] for p in positions) for i in range(3)]
    return mins, maxs


def build_glb(skeleton, mesh, output_path):
    """Build and write a .glb file with skinned mesh."""
    bone_count = skeleton["bone_count"]
    vtx_count = mesh["vertex_count"]
    idx_count = mesh["index_count"]

    # --- Pack all binary buffers ---
    pos_buf = pack_vec3_list(mesh["positions"])
    norm_buf = pack_vec3_list(mesh["normals"])
    uv_buf = pack_vec2_list(mesh["uvs"])
    joint_buf = pack_ubyte4_list(mesh["joints"])
    weight_buf = pack_vec4_list(mesh["weights"])
    idx_buf = pack_u16_list(mesh["indices"])
    ibm_buf = pack_mat4_list(skeleton["inv_bind_matrices"])

    # Pad each to 4-byte alignment
    def pad4(b):
        r = len(b) % 4
        return b + b"\x00" * (4 - r) if r else b

    buffers = [pos_buf, norm_buf, uv_buf, joint_buf, weight_buf, idx_buf, ibm_buf]
    offsets = []
    total = 0
    for b in buffers:
        offsets.append(total)
        total += len(pad4(b))

    # Concatenate all into one binary buffer
    all_bin = b""
    for b in buffers:
        all_bin += pad4(b)

    pos_min, pos_max = compute_bounds(mesh["positions"])

    # --- Build JSON ---
    gltf = {
        "asset": {"version": "2.0", "generator": "SpiderwickUnpack"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [],
        "meshes": [
            {
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": 0,
                            "NORMAL": 1,
                            "TEXCOORD_0": 2,
                            "JOINTS_0": 3,
                            "WEIGHTS_0": 4,
                        },
                        "indices": 5,
                    }
                ]
            }
        ],
        "skins": [
            {
                "inverseBindMatrices": 6,
                "joints": list(range(1, 1 + bone_count)),
                "skeleton": 1,
            }
        ],
        "accessors": [
            # 0: positions
            {
                "bufferView": 0, "componentType": 5126, "count": vtx_count,
                "type": "VEC3", "min": pos_min, "max": pos_max,
            },
            # 1: normals
            {"bufferView": 1, "componentType": 5126, "count": vtx_count, "type": "VEC3"},
            # 2: uvs
            {"bufferView": 2, "componentType": 5126, "count": vtx_count, "type": "VEC2"},
            # 3: joints
            {"bufferView": 3, "componentType": 5121, "count": vtx_count, "type": "VEC4"},
            # 4: weights
            {"bufferView": 4, "componentType": 5126, "count": vtx_count, "type": "VEC4"},
            # 5: indices
            {"bufferView": 5, "componentType": 5123, "count": idx_count, "type": "SCALAR"},
            # 6: inverse bind matrices
            {"bufferView": 6, "componentType": 5126, "count": bone_count, "type": "MAT4"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": offsets[0], "byteLength": len(pos_buf)},
            {"buffer": 0, "byteOffset": offsets[1], "byteLength": len(norm_buf)},
            {"buffer": 0, "byteOffset": offsets[2], "byteLength": len(uv_buf)},
            {"buffer": 0, "byteOffset": offsets[3], "byteLength": len(joint_buf)},
            {"buffer": 0, "byteOffset": offsets[4], "byteLength": len(weight_buf)},
            {"buffer": 0, "byteOffset": offsets[5], "byteLength": len(idx_buf)},
            {"buffer": 0, "byteOffset": offsets[6], "byteLength": len(ibm_buf)},
        ],
        "buffers": [{"byteLength": len(all_bin)}],
    }

    # Build skeleton node tree
    # Node 0: mesh node (references skin)
    # Nodes 1..N: bone joints
    parents = skeleton["parents"]
    children_map = {i: [] for i in range(bone_count)}
    root_bones = []
    for i, p in enumerate(parents):
        if p == i or p < 0:  # root bone
            root_bones.append(i)
        else:
            children_map[p].append(i)

    # Node 0: skinned mesh
    mesh_node = {"mesh": 0, "skin": 0, "children": [1 + r for r in root_bones]}
    gltf["nodes"].append(mesh_node)

    # Bone nodes (1-indexed in glTF nodes)
    for i in range(bone_count):
        node = {"name": f"bone_{i:02d}"}
        # Local transform as 4x4 matrix (column-major for glTF)
        m = skeleton["local_matrices"][i]
        cm = [
            m[0], m[4], m[8], m[12],
            m[1], m[5], m[9], m[13],
            m[2], m[6], m[10], m[14],
            m[3], m[7], m[11], m[15],
        ]
        node["matrix"] = cm
        kids = children_map.get(i, [])
        if kids:
            node["children"] = [1 + c for c in kids]
        gltf["nodes"].append(node)

    # --- Write GLB ---
    json_str = json.dumps(gltf, separators=(",", ":"))
    json_bytes = json_str.encode("utf-8")
    # Pad JSON to 4-byte alignment
    json_pad = (4 - len(json_bytes) % 4) % 4
    json_bytes += b" " * json_pad

    # GLB header: magic(4) + version(4) + length(4)
    # JSON chunk: length(4) + type(4) + data
    # BIN chunk: length(4) + type(4) + data
    json_chunk = struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes
    bin_chunk = struct.pack("<II", len(all_bin), 0x004E4942) + all_bin
    total_len = 12 + len(json_chunk) + len(bin_chunk)
    header = struct.pack("<III", 0x46546C67, 2, total_len)

    with open(output_path, "wb") as f:
        f.write(header + json_chunk + bin_chunk)

    return vtx_count, idx_count // 3, bone_count


# ---------------------------------------------------------------------------
# Batch export
# ---------------------------------------------------------------------------
def batch_export(wad_dir, out_dir=None):
    if out_dir is None:
        out_dir = wad_dir

    nm40_files = sorted([f for f in os.listdir(wad_dir) if f.endswith(".nm40")])
    aniz_files = sorted([f for f in os.listdir(wad_dir) if f.endswith(".aniz")])

    if not nm40_files or not aniz_files:
        print(f"No NM40/aniz pairs in {wad_dir}")
        return

    # Pair by index (same order in WAD)
    pairs = list(zip(nm40_files, aniz_files))
    os.makedirs(out_dir, exist_ok=True)

    for nm40_name, aniz_name in pairs:
        nm40_path = os.path.join(wad_dir, nm40_name)
        aniz_path = os.path.join(wad_dir, aniz_name)
        out_name = nm40_name.replace(".nm40", ".glb")
        out_path = os.path.join(out_dir, out_name)

        try:
            with open(aniz_path, "rb") as f:
                aniz_data = f.read()
            with open(nm40_path, "rb") as f:
                nm40_data = f.read()

            skel = parse_skeleton(aniz_data)
            mesh = parse_mesh(nm40_data)
            verts, tris, bones = build_glb(skel, mesh, out_path)
            print(f"  {out_name}: {verts} verts, {tris} tris, {bones} bones")
        except Exception as e:
            print(f"  SKIP {nm40_name}: {e}")


def main():
    parser = argparse.ArgumentParser(description="Spiderwick skinned mesh glTF exporter")
    parser.add_argument("input1", help="aniz file, or --batch wad_dir")
    parser.add_argument("input2", nargs="?", help="nm40 file (if not --batch)")
    parser.add_argument("output", nargs="?", help="output .glb path")
    parser.add_argument("--batch", action="store_true", help="Batch export all pairs in directory")
    args = parser.parse_args()

    if args.batch:
        batch_export(args.input1, args.input2)
    else:
        if not args.input2:
            print("Usage: spiderwick_glb.py <aniz> <nm40> [output.glb]")
            sys.exit(1)

        with open(args.input1, "rb") as f:
            aniz_data = f.read()
        with open(args.input2, "rb") as f:
            nm40_data = f.read()

        out = args.output or args.input2.replace(".nm40", ".glb")
        skel = parse_skeleton(aniz_data)
        mesh = parse_mesh(nm40_data)
        verts, tris, bones = build_glb(skel, mesh, out)
        print(f"Exported: {verts} vertices, {tris} triangles, {bones} bones → {out}")


if __name__ == "__main__":
    main()
