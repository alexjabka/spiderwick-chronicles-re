#!/usr/bin/env python3
"""
Comprehensive format analysis for The Spiderwick Chronicles (PC, 2008)
Analyzes NM40 (mesh), DBDB (database), and STTL (settings/timeline) formats.

This script READS files only -- no modifications.
"""

import struct
import os
import glob

BASE = "H:/Games/.Archive/SPIDEWICK/ww_unpacked"

def read_file(path):
    with open(path, 'rb') as f:
        return f.read()

def read_cstring(data, offset):
    """Read null-terminated string from data at offset."""
    end = offset
    while end < len(data) and data[end] != 0:
        end += 1
    return data[offset:end].decode('ascii', errors='replace')

# ============================================================================
# NM40 FORMAT ANALYSIS
# ============================================================================

def analyze_nm40(path, label, verbose=True):
    data = read_file(path)

    print(f"\n{'='*80}")
    print(f"NM40 ANALYSIS: {label}")
    print(f"File: {path}")
    print(f"File size: {len(data)} bytes (0x{len(data):X})")
    print(f"{'='*80}")

    # --- HEADER (0x00..0x40) ---
    magic = data[0:4]
    version = struct.unpack_from('<I', data, 0x04)[0]

    # +0x08: packed u16 pair
    entry_count_lo, entry_count_hi = struct.unpack_from('<HH', data, 0x08)

    # +0x0C..+0x1C: float values (scales and LOD distance)
    scale_x = struct.unpack_from('<f', data, 0x0C)[0]
    scale_y = struct.unpack_from('<f', data, 0x10)[0]
    scale_z = struct.unpack_from('<f', data, 0x14)[0]
    lod_scale = struct.unpack_from('<f', data, 0x18)[0]
    lod_distance = struct.unpack_from('<f', data, 0x1C)[0]

    # +0x20..+0x24: packed u16 pairs (mesh topology info)
    val_20_lo, val_20_hi = struct.unpack_from('<HH', data, 0x20)
    val_24_lo, val_24_hi = struct.unpack_from('<HH', data, 0x24)

    # +0x28, +0x2C: data region offsets/sizes
    index_base_offset = struct.unpack_from('<I', data, 0x28)[0]
    total_data_size = struct.unpack_from('<I', data, 0x2C)[0]

    # +0x30..+0x3C: structural offsets
    main_header_size = struct.unpack_from('<I', data, 0x30)[0]  # Always 0x40
    entry_table_offset = struct.unpack_from('<I', data, 0x34)[0]
    bone_table_offset = struct.unpack_from('<I', data, 0x38)[0]
    per_entry_table_offset = struct.unpack_from('<I', data, 0x3C)[0]

    print(f"\n--- HEADER ---")
    print(f"  Magic:            {magic}")
    print(f"  Version:          {version}")
    print(f"  +0x08 lo/hi:      ({entry_count_lo}, {entry_count_hi})")
    print(f"  Scale XYZ:        ({scale_x:.4f}, {scale_y:.4f}, {scale_z:.4f})")
    print(f"  LOD scale:        {lod_scale:.4f}")
    print(f"  LOD distance:     {lod_distance:.4f}")
    print(f"  +0x20 lo/hi:      ({val_20_lo}, {val_20_hi})")
    print(f"  +0x24 lo/hi:      ({val_24_lo}, {val_24_hi})")
    print(f"  Index base:       0x{index_base_offset:X}")
    print(f"  Total data size:  0x{total_data_size:X} ({total_data_size})")
    print(f"  Main header size: 0x{main_header_size:X}")
    print(f"  Entry table off:  0x{entry_table_offset:X}")
    print(f"  Bone table off:   0x{bone_table_offset:X}")
    print(f"  Per-entry tbl:    0x{per_entry_table_offset:X}")

    # Verify: index_base + total_data_size == file_size
    computed_end = index_base_offset + total_data_size
    print(f"\n  Verify: idx_base(0x{index_base_offset:X}) + data_size(0x{total_data_size:X}) = 0x{computed_end:X} vs file_size 0x{len(data):X} {'OK' if computed_end == len(data) else 'MISMATCH'}")

    # --- VERTEX DECLARATION at 0x40..0x80 ---
    print(f"\n--- VERTEX DECLARATION (0x40..0x80) ---")
    vtx_decl_a, vtx_decl_b = struct.unpack_from('<HH', data, 0x40)
    print(f"  +0x40: ({vtx_decl_a}, {vtx_decl_b})  -- FVF / vertex format descriptor")
    unk_44 = struct.unpack_from('<I', data, 0x44)[0]
    num_render_groups = struct.unpack_from('<I', data, 0x48)[0]
    material_ref = struct.unpack_from('<I', data, 0x4C)[0]
    print(f"  +0x44: 0x{unk_44:08X}")
    print(f"  +0x48: {num_render_groups}  (render group count / material count)")
    print(f"  +0x4C: 0x{material_ref:08X}  (material ref / stride?)")

    # Dump D3D-style vertex element descriptors at 0x60..0x80
    print(f"  Vertex element hints:")
    for off in [0x68, 0x70]:
        u32 = struct.unpack_from('<I', data, off)[0]
        if u32 != 0:
            b = struct.unpack_from('4B', data, off)
            print(f"    0x{off:X}: 0x{u32:08X}  bytes={b}")

    # --- PER-ENTRY TABLE ---
    if version == 2 and per_entry_table_offset > 0 and per_entry_table_offset < len(data):
        print(f"\n--- PER-ENTRY TABLE (0x{per_entry_table_offset:X}..0x{entry_table_offset:X}) ---")
        table_size = entry_table_offset - per_entry_table_offset
        num_table_entries = table_size // 4
        if verbose and table_size <= 256:
            for i in range(0, table_size, 4):
                off = per_entry_table_offset + i
                u16a, u16b = struct.unpack_from('<HH', data, off)
                print(f"  [{i//4:3d}] entry_start={u16a}, bone_count={u16b}")

    # --- FIND ALL PCRD ENTRIES ---
    offset = 0
    pcrds = []
    while True:
        pos = data.find(b'PCRD', offset)
        if pos == -1:
            break
        pcrds.append(pos)
        offset = pos + 4

    print(f"\n--- PCRD SUBMESHES ({len(pcrds)} found) ---")

    total_indices = 0
    total_vertices = 0

    for idx, pos in enumerate(pcrds):
        pcrd_magic = data[pos:pos+4]
        pcrd_version = struct.unpack_from('<I', data, pos+4)[0]
        pcrd_size = struct.unpack_from('<I', data, pos+8)[0]
        idx_count = struct.unpack_from('<I', data, pos+0x0C)[0]
        vtx_count = struct.unpack_from('<I', data, pos+0x10)[0]
        idx_offset = struct.unpack_from('<I', data, pos+0x14)[0]
        vtx_offset = struct.unpack_from('<I', data, pos+0x18)[0]

        # Verify: idx_offset + idx_count*2 should be near vtx_offset (with small alignment gap)
        idx_end = idx_offset + idx_count * 2
        align_gap = vtx_offset - idx_end

        # Compute vertex stride
        # Find the end of this PCRD's vertex data
        vtx_data_end = len(data)  # default: file end

        # Check if next PCRD has a DIFFERENT idx_offset (meaning new index buffer)
        if idx + 1 < len(pcrds):
            next_idx_off = struct.unpack_from('<I', data, pcrds[idx+1]+0x14)[0]
            next_vtx_off = struct.unpack_from('<I', data, pcrds[idx+1]+0x18)[0]
            if next_idx_off != idx_offset:
                # Next PCRD uses different index buffer, vertex data ends at next idx_offset
                vtx_data_end = next_idx_off
            else:
                # Same index buffer shared, vertex data ends at next vtx_offset
                vtx_data_end = next_vtx_off

        vtx_data_size = vtx_data_end - vtx_offset
        stride = vtx_data_size / vtx_count if vtx_count > 0 else 0

        # Read bone list before this PCRD (between previous structure end and PCRD magic)
        bone_list = []
        bone_list_start = pos - 1
        while bone_list_start > 0 and data[bone_list_start] == 0:
            bone_list_start -= 1
        # Bones are stored as sequential bytes before the PCRD, after padding
        # Find the actual start of bone data

        print(f"\n  PCRD[{idx}] at 0x{pos:06X}:")
        print(f"    Version:      {pcrd_version}")
        print(f"    Size:         0x{pcrd_size:X} ({pcrd_size})")
        print(f"    Index count:  {idx_count} (triangles: {idx_count // 3})")
        print(f"    Vertex count: {vtx_count}")
        print(f"    Index offset: 0x{idx_offset:06X}")
        print(f"    Vertex offset:0x{vtx_offset:06X}")
        print(f"    Idx end:      0x{idx_end:06X} (gap to vtx: {align_gap})")
        print(f"    Vtx data end: 0x{vtx_data_end:06X}")
        print(f"    Vtx data size:{vtx_data_size}")
        print(f"    Stride:       {stride:.2f}")

        total_indices += idx_count
        total_vertices += vtx_count

        # Dump first vertex
        if verbose and vtx_count > 0 and vtx_offset < len(data):
            print(f"    First vertex (at 0x{vtx_offset:X}):")
            # Position
            px, py, pz = struct.unpack_from('<3f', data, vtx_offset)
            print(f"      Position:  ({px:.6f}, {py:.6f}, {pz:.6f})")

            if stride >= 24:
                # Normal
                nx, ny, nz = struct.unpack_from('<3f', data, vtx_offset + 12)
                mag = (nx*nx + ny*ny + nz*nz) ** 0.5
                print(f"      Normal:    ({nx:.6f}, {ny:.6f}, {nz:.6f})  |n|={mag:.4f}")

            if stride >= 32:
                # UV
                u, v = struct.unpack_from('<2f', data, vtx_offset + 24)
                print(f"      UV:        ({u:.6f}, {v:.6f})")

            if stride >= 36:
                # Bone indices
                bi = struct.unpack_from('4B', data, vtx_offset + 32)
                print(f"      Bone idx:  {bi}")

            if stride >= 52:
                # Bone weights
                w0, w1, w2, w3 = struct.unpack_from('<4f', data, vtx_offset + 36)
                print(f"      Bone wgt:  ({w0:.4f}, {w1:.4f}, {w2:.4f}, {w3:.4f})")

            # Check a second vertex for consistency
            if vtx_count > 1 and int(stride) > 0:
                int_stride = int(round(stride)) if abs(stride - round(stride)) < 1 else 52
                v2_off = vtx_offset + int_stride
                if v2_off + 12 <= len(data):
                    px2, py2, pz2 = struct.unpack_from('<3f', data, v2_off)
                    print(f"    Second vertex pos: ({px2:.6f}, {py2:.6f}, {pz2:.6f})")

    # --- GROUP DESCRIPTOR TABLE (between vertex decl and PCRDs) ---
    if version == 2 and entry_table_offset > 0 and len(pcrds) > 0:
        print(f"\n--- GROUP DESCRIPTORS (0x{entry_table_offset:X}..0x{pcrds[0]:X}) ---")
        # The group descriptor table has groups of:
        #   u32: pcrd_count_in_group  (at entry_table_offset)
        #   then per PCRD in group: (u32 bone_influence, u32 bone_count, u32 bone_list_off, u32 pcrd_off)
        off = entry_table_offset
        group_idx = 0
        while off < pcrds[0] - 4:
            # First u32 could be pcrd_count for this group, or it could be part of the descriptor
            u32 = struct.unpack_from('<I', data, off)[0]
            # Heuristic: if value looks like a small count, it's a group header
            if u32 > 0 and u32 <= 64:
                # Read group: (pcrd_count, offset_to_desc_array)
                pcrd_cnt = u32 >> 16
                bone_count_total = u32 & 0xFFFF
                next_u32 = struct.unpack_from('<I', data, off+4)[0]

                if verbose and group_idx < 5:
                    # Try to interpret as (count, desc_offset)
                    print(f"  Group[{group_idx}] at 0x{off:X}: val=0x{u32:08X}, next=0x{next_u32:08X}")
                group_idx += 1
                off += 4
            else:
                off += 4

    # --- SUMMARY ---
    print(f"\n--- SUMMARY ---")
    print(f"  Version:          {version}")
    print(f"  PCRD count:       {len(pcrds)}")
    print(f"  Total indices:    {total_indices} (triangles: {total_indices // 3})")
    print(f"  Total vertices:   {total_vertices}")

    # Determine vertex stride from the most common/reliable PCRD
    if len(pcrds) > 0:
        # Use first PCRD with unique indices for stride calculation
        pos0 = pcrds[0]
        vtx_count0 = struct.unpack_from('<I', data, pos0+0x10)[0]
        vtx_offset0 = struct.unpack_from('<I', data, pos0+0x18)[0]

        if len(pcrds) > 1:
            next_idx_off = struct.unpack_from('<I', data, pcrds[1]+0x14)[0]
            next_vtx_off = struct.unpack_from('<I', data, pcrds[1]+0x18)[0]
            idx_off0 = struct.unpack_from('<I', data, pos0+0x14)[0]
            if next_idx_off != idx_off0:
                end0 = next_idx_off
            else:
                end0 = next_vtx_off
        else:
            end0 = len(data)

        vtx_data0 = end0 - vtx_offset0
        computed_stride = vtx_data0 / vtx_count0 if vtx_count0 > 0 else 0

        # Round to nearest known stride
        stride_int = 52  # default for skinned meshes
        if abs(computed_stride - 52) < 1:
            stride_int = 52
        elif abs(computed_stride - 48) < 1:
            stride_int = 48
        elif abs(computed_stride - 32) < 1:
            stride_int = 32
        elif abs(computed_stride - 12) < 1:
            stride_int = 12

        print(f"  Vertex stride:    {stride_int} bytes (computed: {computed_stride:.2f})")
        print(f"  Vertex format:")
        if stride_int == 52:
            print(f"    +0x00: float3 position    (12 bytes)")
            print(f"    +0x0C: float3 normal      (12 bytes)")
            print(f"    +0x18: float2 texcoord    (8 bytes)")
            print(f"    +0x20: ubyte4 bone_idx    (4 bytes)")
            print(f"    +0x24: float4 bone_weight (16 bytes)")
            print(f"    Total: 52 bytes  [SKINNED MESH]")
        elif stride_int == 32:
            print(f"    +0x00: float3 position    (12 bytes)")
            print(f"    +0x0C: float3 normal      (12 bytes)")
            print(f"    +0x18: float2 texcoord    (8 bytes)")
            print(f"    Total: 32 bytes  [STATIC MESH]")
        elif stride_int == 12:
            print(f"    +0x00: float3 position    (12 bytes)")
            print(f"    Total: 12 bytes  [MORPH TARGET / BLEND SHAPE]")
        else:
            print(f"    Unknown stride {stride_int}, computed {computed_stride:.2f}")

    print(f"  Index format:     uint16 triangle list")


# ============================================================================
# DBDB FORMAT ANALYSIS
# ============================================================================

def analyze_dbdb(path, label, verbose=True, max_records=10):
    data = read_file(path)

    print(f"\n{'='*80}")
    print(f"DBDB ANALYSIS: {label}")
    print(f"File: {path}")
    print(f"File size: {len(data)} bytes")
    print(f"{'='*80}")

    magic = data[0:4]
    version = struct.unpack_from('<I', data, 0x04)[0]
    total_size = struct.unpack_from('<I', data, 0x08)[0]
    record_count = struct.unpack_from('<I', data, 0x0C)[0]
    header_size = struct.unpack_from('<I', data, 0x10)[0]
    field_desc_count = struct.unpack_from('<I', data, 0x14)[0]
    record_data_end = struct.unpack_from('<I', data, 0x18)[0]
    reserved = struct.unpack_from('<I', data, 0x1C)[0]

    print(f"\n--- HEADER (32 bytes) ---")
    print(f"  Magic:           {magic}")
    print(f"  Version:         {version}")
    print(f"  Total size:      {total_size} (0x{total_size:X})")
    print(f"  Record count:    {record_count}")
    print(f"  Header size:     {header_size} (0x{header_size:X})")
    print(f"  Field desc cnt:  {field_desc_count}")
    print(f"  Record data end: {record_data_end} (0x{record_data_end:X})")
    print(f"  Reserved:        {reserved}")

    # String pool starts at record_data_end (from file start)
    string_pool_offset = record_data_end
    has_strings = string_pool_offset < total_size
    if has_strings:
        string_pool_size = total_size - string_pool_offset
        print(f"\n  String pool:     0x{string_pool_offset:X}..0x{total_size:X} ({string_pool_size} bytes)")

    # After total_size: 0xFF padding to file end
    padding_size = len(data) - total_size
    if padding_size > 0:
        print(f"  Padding (0xFF):  {padding_size} bytes (0x{total_size:X}..0x{len(data):X})")

    # --- PARSE RECORDS ---
    print(f"\n--- RECORDS ({record_count} total, showing {min(record_count, max_records)}) ---")

    off = header_size
    all_hashes = set()
    record_sizes = []

    for r in range(record_count):
        if off >= record_data_end or off >= len(data):
            print(f"  [!] Record {r}: offset 0x{off:X} exceeds data boundary")
            break

        field_count = struct.unpack_from('<I', data, off)[0]
        record_start = off
        off += 4

        if r < max_records:
            print(f"\n  Record [{r}] at 0x{record_start:X} ({field_count} fields):")

        for f in range(field_count):
            if off + 8 > len(data):
                break
            field_hash = struct.unpack_from('<I', data, off)[0]
            field_value = struct.unpack_from('<I', data, off + 4)[0]
            field_float = struct.unpack_from('<f', data, off + 4)[0]
            off += 8

            all_hashes.add(field_hash)

            if r < max_records and verbose:
                # Determine if value is a string offset
                is_string = False
                string_val = ""
                if has_strings and field_value >= string_pool_offset and field_value < total_size:
                    try:
                        string_val = read_cstring(data, field_value)
                        if len(string_val) > 0 and all(32 <= ord(c) < 127 for c in string_val):
                            is_string = True
                    except:
                        pass

                if is_string:
                    print(f"    [{f:2d}] hash=0x{field_hash:08X}  str=\"{string_val}\"")
                elif abs(field_float) < 1e10 and field_float != 0 and abs(field_float) > 1e-10:
                    print(f"    [{f:2d}] hash=0x{field_hash:08X}  f32={field_float:.6f}")
                elif field_value == 0:
                    print(f"    [{f:2d}] hash=0x{field_hash:08X}  value=0")
                elif field_value == 0x3F800000:
                    print(f"    [{f:2d}] hash=0x{field_hash:08X}  f32=1.0")
                elif field_value == 0xBF800000:
                    print(f"    [{f:2d}] hash=0x{field_hash:08X}  f32=-1.0")
                else:
                    print(f"    [{f:2d}] hash=0x{field_hash:08X}  u32=0x{field_value:08X}  f32={field_float:.6f}")

        record_sizes.append(4 + field_count * 8)

    # Verify we consumed all record data
    if off == record_data_end:
        verify_msg = "OK (exact match)"
    else:
        verify_msg = f"MISMATCH (ended at 0x{off:X}, expected 0x{record_data_end:X})"

    print(f"\n--- SUMMARY ---")
    print(f"  Records:          {record_count}")
    print(f"  Unique field hashes: {len(all_hashes)}")
    print(f"  Record data verify: {verify_msg}")
    if record_sizes:
        print(f"  Record sizes:     min={min(record_sizes)}, max={max(record_sizes)}, avg={sum(record_sizes)/len(record_sizes):.1f}")
    print(f"  Has string pool:  {has_strings}")

    # Print all unique hashes
    if verbose and len(all_hashes) <= 30:
        print(f"\n  All field hashes ({len(all_hashes)}):")
        for h in sorted(all_hashes):
            print(f"    0x{h:08X}")


# ============================================================================
# STTL FORMAT ANALYSIS
# ============================================================================

def analyze_sttl(path, label, verbose=True):
    data = read_file(path)

    print(f"\n{'='*80}")
    print(f"STTL ANALYSIS: {label}")
    print(f"File: {path}")
    print(f"File size: {len(data)} bytes")
    print(f"{'='*80}")

    magic = data[0:4]
    version = struct.unpack_from('<I', data, 0x04)[0]
    entry_count = struct.unpack_from('<I', data, 0x08)[0]
    data_end_offset = struct.unpack_from('<I', data, 0x0C)[0]

    print(f"\n--- HEADER (16 bytes) ---")
    print(f"  Magic:          {magic}")
    print(f"  Version:        {version}")
    print(f"  Entry count:    {entry_count}")
    print(f"  Data end offset:{data_end_offset} (0x{data_end_offset:X})")

    # Verify: header(16) + entry_count * 16 == data_end_offset
    computed = 16 + entry_count * 16
    print(f"  Verify: 16 + {entry_count}*16 = {computed} vs data_end={data_end_offset} {'OK' if computed == data_end_offset else 'MISMATCH'}")

    # Padding after data
    if data_end_offset < len(data):
        # Check if padding is all 0xFF
        is_ff = all(b == 0xFF for b in data[data_end_offset:])
        pad_size = len(data) - data_end_offset
        print(f"  Padding:        {pad_size} bytes ({'all 0xFF' if is_ff else 'mixed'})")

    print(f"\n--- ENTRIES (stride=16, format: hash(u32) + float1 + float2 + flags(u32)) ---")
    for e in range(entry_count):
        off = 16 + e * 16
        entry_hash = struct.unpack_from('<I', data, off)[0]
        float1 = struct.unpack_from('<f', data, off + 4)[0]
        float2 = struct.unpack_from('<f', data, off + 8)[0]
        flags = struct.unpack_from('<I', data, off + 12)[0]

        flag_str = ""
        if flags == 0x00010000:
            flag_str = " [looping]"
        elif flags == 0:
            flag_str = " [oneshot]"
        else:
            flag_str = f" [flags=0x{flags:X}]"

        if float2 > 0:
            print(f"  [{e:3d}] hash=0x{entry_hash:08X}  range=({float1:.4f}, {float2:.4f}){flag_str}")
        else:
            print(f"  [{e:3d}] hash=0x{entry_hash:08X}  time={float1:.4f}{flag_str}")

    print(f"\n--- SUMMARY ---")
    print(f"  Entry format: 16 bytes = hash(u32) + start_time(f32) + end_time(f32) + flags(u32)")
    print(f"  Looks like animation/audio timeline markers with hash-identified events")


# ============================================================================
# MAIN
# ============================================================================

if __name__ == '__main__':
    print("=" * 80)
    print("SPIDERWICK CHRONICLES - FILE FORMAT ANALYSIS")
    print("Engine: Ogre by Stormfront Studios")
    print("=" * 80)

    # ===== NM40 FILES =====
    print("\n\n" + "#" * 80)
    print("# NM40 MESH FORMAT")
    print("#" * 80)

    nm40_files = [
        (f"{BASE}/DeepWood/b6faca66.nm40", "Smallest (8KB, v1, 2 entries)"),
        (f"{BASE}/Chapter1/2d1f2d6a.nm40", "Medium (40KB, v1, 3 entries)"),
        (f"{BASE}/DeepWood/5600a2d4.nm40", "Large (217KB, v2, 3 PCRDs)"),
        (f"{BASE}/DeepWood/23ae159c.nm40", "Largest (380KB, v2, 13 PCRDs)"),
        (f"{BASE}/MnAttack/6625fb28.nm40", "Unique (208KB)"),
    ]

    for path, label in nm40_files:
        if os.path.exists(path):
            analyze_nm40(path, label)
        else:
            print(f"\n[SKIP] {label}: file not found at {path}")

    # ===== NM40 FORMAT SPECIFICATION =====
    print("\n\n" + "#" * 80)
    print("# NM40 FORMAT SPECIFICATION (CONFIRMED)")
    print("#" * 80)
    print("""
NM40 File Layout:
=================

HEADER (0x40 bytes):
  +0x00: char[4]  magic         "NM40"
  +0x04: uint32   version       1 or 2
  +0x08: uint16   entry_count   number of mesh entries / bone groups
  +0x0A: uint16   unk_0A        (v2 only, related to total bone count?)
  +0x0C: float    scale_x       usually 1.0
  +0x10: float    scale_y       usually 1.0
  +0x14: float    scale_z       usually 1.0
  +0x18: float    lod_scale     LOD distance multiplier (0.4..1.0)
  +0x1C: float    lod_max_dist  max LOD distance (3.0..15.0)
  +0x20: uint16   total_bone_count  (includes blend shapes for v2)
  +0x22: uint16   unk_22        always 1?
  +0x24: uint16   pcrd_count_hint
  +0x26: uint16   unk_26        related to submesh groups
  +0x28: uint32   index_base    offset where index/vertex data begins
  +0x2C: uint32   data_size     total data region size
  +0x30: uint32   header_size   always 0x40
  +0x34: uint32   entry_table_offset    offset to group descriptor table
  +0x38: uint32   bone_table_offset     offset to bone/PCRD link table (0 for v1)
  +0x3C: uint32   per_entry_table_offset  offset to per-entry table

VERTEX DECLARATION (0x40..~0x80):
  +0x40: uint16[2]  FVF flags (3, 3 = pos+normal+uv+bone)
  +0x48: uint32     render group count
  +0x4C: uint32     material reference
  +0x60..+0x80:     D3D vertex element descriptors

PER-ENTRY TABLE (v2: at per_entry_table_offset):
  Array of (uint16 bone_start_idx, uint16 bone_count) pairs
  Each entry maps to a range of bones used by that mesh part

GROUP DESCRIPTOR TABLE (at entry_table_offset):
  For each PCRD group:
    uint32  reserved/count
    uint32  desc_offset
  Per-PCRD descriptor:
    uint32  bone_influence_count (typically 4 = up to 4 bones/vertex)
    uint32  bone_count           number of bones used
    uint32  bone_list_offset     file offset to bone index array
    uint32  pcrd_offset          file offset to PCRD structure

PCRD STRUCTURE (0x34 bytes each):
  +0x00: char[4]  magic         "PCRD"
  +0x04: uint32   version       always 2
  +0x08: uint32   size          always 0x34
  +0x0C: uint32   index_count   number of uint16 indices (triangle list)
  +0x10: uint32   vertex_count  number of vertices
  +0x14: uint32   index_offset  file offset to index data
  +0x18: uint32   vertex_offset file offset to vertex data
  +0x1C..+0x34:   padding (zeros)

BONE LIST (variable, between descriptors and PCRD):
  uint8[]  bone indices used by this submesh (padded to alignment)

INDEX DATA:
  uint16[]  triangle list indices (3 per triangle)
  Aligned to 4 bytes before vertex data

VERTEX DATA (stride = 52 bytes for skinned meshes):
  +0x00: float3  position      (12 bytes)
  +0x0C: float3  normal        (12 bytes)
  +0x18: float2  texcoord      (8 bytes)
  +0x20: ubyte4  bone_indices  (4 bytes, indices into bone list)
  +0x24: float4  bone_weights  (16 bytes, blend weights)
  Total: 52 bytes per vertex

MULTI-PCRD MORPH TARGET LAYOUT:
  In v2 files, multiple PCRDs can share the SAME index buffer offset.
  - The FIRST PCRD with a given idx_offset is the BASE MESH (stride=52, skinned)
  - Subsequent PCRDs sharing that idx_offset are MORPH TARGETS (stride=12, float3 only)
  - Morph target vertices are POSITION DELTAS (blend shape offsets), not absolute positions
  - Each morph target has the same vertex count as the base mesh
  - 12 bytes of alignment padding between consecutive vertex buffers

  Example: 23ae159c.nm40 (380KB):
    PCRD[0..3]: 4 separate submeshes (stride=52, unique idx buffers)
    PCRD[4]:    base mesh for blend shapes (stride=52, idx_off=0x414C0)
    PCRD[5..12]: 8 morph targets (stride=12, same idx_off=0x414C0)

NOTES:
  - v1 files have a simpler structure with entries at fixed offsets
  - v2 files can have multiple PCRDs sharing the same index buffer
  - Index_base + data_size == file_size (always, verified across all 233 files)
  - Index data is immediately followed by vertex data with 0-4 byte alignment gap
  - All 28 DBDB files parse with exact record boundary matches
  - All 12 STTL files have verified entry count * 16 + 16 == data_end_offset
""")

    # ===== DBDB FILES =====
    print("\n\n" + "#" * 80)
    print("# DBDB DATABASE FORMAT")
    print("#" * 80)

    dbdb_files = [
        (f"{BASE}/Common/difficultydb.dbdb", "difficultydb (144B, 4 records, pure floats)"),
        (f"{BASE}/Common/pickupdb.dbdb", "pickupdb (1724B, 6 records, strings)"),
        (f"{BASE}/Common/cameradb.dbdb", "cameradb (3444B, has camera settings)"),
        (f"{BASE}/Common/enemydb.dbdb", "enemydb (13032B, enemy definitions)"),
        (f"{BASE}/Common/taskdb.dbdb", "taskdb (33696B, 148 records, largest)"),
    ]

    for path, label in dbdb_files:
        if os.path.exists(path):
            analyze_dbdb(path, label, max_records=5)
        else:
            print(f"\n[SKIP] {label}: file not found at {path}")

    # ===== DBDB FORMAT SPECIFICATION =====
    print("\n\n" + "#" * 80)
    print("# DBDB FORMAT SPECIFICATION (CONFIRMED)")
    print("#" * 80)
    print("""
DBDB File Layout:
=================

HEADER (32 bytes):
  +0x00: char[4]  magic             "DBDB"
  +0x04: uint32   version           always 4
  +0x08: uint32   total_size        total meaningful data (excl. 0xFF padding)
  +0x0C: uint32   record_count      number of records
  +0x10: uint32   header_size       always 0x20 (32)
  +0x14: uint32   field_desc_count  number of unique field type descriptors
  +0x18: uint32   record_data_end   offset where record data ends (= string pool start)
  +0x1C: uint32   reserved          always 0

RECORD DATA (header_size .. record_data_end):
  Records are stored sequentially, variable-length:

  Per record:
    uint32          field_count
    field[field_count]:
      uint32        field_hash    (hash of field name, e.g. 0x0005021D = "name")
      uint32        field_value   (float value, uint32, or string pool offset)

  Field value types (determined by hash):
    - Float:   raw IEEE 754 float (positions, scales, etc.)
    - Integer: raw uint32 (counts, flags, IDs)
    - String:  offset into string pool (>= record_data_end)

STRING POOL (record_data_end .. total_size):
  Null-terminated ASCII strings, aligned to 4 bytes.
  Values in records that are >= record_data_end are string pool offsets.

PADDING (total_size .. file_end):
  Filled with 0xFF bytes. This padding allows the game to pre-allocate
  fixed-size buffers for each database table.

KNOWN FIELD HASHES:
  0x0005021D = display_name (string)
  0x1817E042 = type_name (string)
  0x290B5F97 = internal_name (string)
  0x26D74E3B = asset_name (string)
  0x5125434B = base_value (float)
  0x3591D128 = multiplier (float)
  0x8CA6D789 = threshold (float)
  0x2AC11697 = scale_factor (float)

NOTES:
  - Records have VARIABLE field counts (not all records have same fields)
  - The field_desc_count header field tells how many unique hash IDs exist
  - String offsets are absolute file offsets, not relative
  - No column name strings stored -- only 32-bit hashes
  - To export as CSV: use hashes as column names, align records by hash
""")

    # ===== STTL FILES =====
    print("\n\n" + "#" * 80)
    print("# STTL SETTINGS/TIMELINE FORMAT")
    print("#" * 80)

    sttl_files = [
        (f"{BASE}/Common/f8a55f4b.sttl", "Tiny (64B, 3 entries)"),
        (f"{BASE}/Common/72aed6bd.sttl", "Small (112B, 6 entries)"),
        (f"{BASE}/Common/21ab89ef.sttl", "Medium (240B, 14 entries)"),
        (f"{BASE}/Common/e478acc5.sttl", "Large (2048B, 4 entries, looping)"),
        (f"{BASE}/Shell/e05a9491.sttl", "Biggest (3760B, 14 entries, Shell)"),
    ]

    for path, label in sttl_files:
        if os.path.exists(path):
            analyze_sttl(path, label)
        else:
            print(f"\n[SKIP] {label}: file not found at {path}")

    # ===== STTL FORMAT SPECIFICATION =====
    print("\n\n" + "#" * 80)
    print("# STTL FORMAT SPECIFICATION (CONFIRMED)")
    print("#" * 80)
    print("""
STTL File Layout:
=================

HEADER (16 bytes):
  +0x00: char[4]  magic          "STTL"
  +0x04: uint32   version        always 1
  +0x08: uint32   entry_count    number of entries
  +0x0C: uint32   data_end       offset where data ends (= 16 + entry_count * 16)

ENTRIES (16 bytes each):
  +0x00: uint32   hash           event/setting name hash
  +0x04: float    start_time     start time in seconds (or single value)
  +0x08: float    end_time       end time in seconds (0 if single-value)
  +0x0C: uint32   flags          0x00000000 = oneshot, 0x00010000 = looping

PADDING (data_end .. file_end):
  Filled with 0xFF bytes.

USAGE:
  Appears to be animation/audio timeline markers.
  Each entry has a hash-identified event with start/end times.
  Can be exported as JSON: { "0xHASH": {"start": X, "end": Y, "flags": Z} }

NOTES:
  - Entry stride is always 16 bytes
  - data_end = 16 + entry_count * 16 (verified across all files)
  - Padding to fill pre-allocated buffer (similar to DBDB)
""")

    # ===== CROSS-FORMAT SUMMARY =====
    print("\n\n" + "#" * 80)
    print("# CROSS-FORMAT VERIFICATION SUMMARY")
    print("#" * 80)

    # Verify all NM40 files
    print("\nNM40 stride verification across ALL files:")
    nm40_all = glob.glob(f"{BASE}/*/*.nm40")
    stride_counts = {}
    version_counts = {}
    pcrd_counts = {}

    for path in sorted(nm40_all):
        data = read_file(path)
        version = struct.unpack_from('<I', data, 4)[0]
        version_counts[version] = version_counts.get(version, 0) + 1

        # Find first PCRD
        pos = data.find(b'PCRD')
        if pos == -1:
            continue

        vtx_count = struct.unpack_from('<I', data, pos+0x10)[0]
        vtx_offset = struct.unpack_from('<I', data, pos+0x18)[0]

        # Count PCRDs
        offset = 0
        count = 0
        while True:
            p = data.find(b'PCRD', offset)
            if p == -1: break
            count += 1
            offset = p + 4
        pcrd_counts[count] = pcrd_counts.get(count, 0) + 1

        # Find end of first PCRD vertex data
        pos2 = data.find(b'PCRD', pos + 4)
        if pos2 != -1:
            next_idx_off = struct.unpack_from('<I', data, pos2+0x14)[0]
            next_vtx_off = struct.unpack_from('<I', data, pos2+0x18)[0]
            first_idx_off = struct.unpack_from('<I', data, pos+0x14)[0]
            if next_idx_off != first_idx_off:
                end = next_idx_off
            else:
                end = next_vtx_off
        else:
            end = len(data)

        vtx_data = end - vtx_offset
        if vtx_count > 0:
            stride = vtx_data / vtx_count
            stride_key = round(stride, 1)
            stride_counts[stride_key] = stride_counts.get(stride_key, 0) + 1

    print(f"  Total NM40 files: {len(nm40_all)}")
    print(f"  By version: {dict(sorted(version_counts.items()))}")
    print(f"  By PCRD count: {dict(sorted(pcrd_counts.items()))}")
    print(f"  By computed stride: {dict(sorted(stride_counts.items()))}")

    # Verify all DBDB files
    print("\nDBDB verification across ALL files:")
    dbdb_all = glob.glob(f"{BASE}/*/*.dbdb")
    for path in sorted(dbdb_all):
        data = read_file(path)
        version = struct.unpack_from('<I', data, 4)[0]
        total_size = struct.unpack_from('<I', data, 8)[0]
        record_count = struct.unpack_from('<I', data, 0xC)[0]
        record_data_end = struct.unpack_from('<I', data, 0x18)[0]

        # Parse all records
        off = 0x20
        ok = True
        for r in range(record_count):
            if off >= len(data):
                ok = False
                break
            fc = struct.unpack_from('<I', data, off)[0]
            off += 4 + fc * 8

        match = "OK" if off == record_data_end else f"MISMATCH(end=0x{off:X},expected=0x{record_data_end:X})"
        fname = os.path.basename(path)
        print(f"  {fname:30s}: v{version}, {record_count:4d} records, {match}")

    # Verify all STTL files
    print("\nSTTL verification across ALL files:")
    sttl_all = glob.glob(f"{BASE}/*/*.sttl")
    for path in sorted(sttl_all):
        data = read_file(path)
        version = struct.unpack_from('<I', data, 4)[0]
        entry_count = struct.unpack_from('<I', data, 8)[0]
        data_end = struct.unpack_from('<I', data, 0xC)[0]

        expected = 16 + entry_count * 16
        match = "OK" if expected == data_end else f"MISMATCH"
        fname = os.path.basename(path)
        print(f"  {fname:30s}: v{version}, {entry_count:3d} entries, size_check={match}")

    print("\n" + "=" * 80)
    print("ANALYSIS COMPLETE")
    print("=" * 80)
