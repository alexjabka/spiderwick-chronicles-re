#!/usr/bin/env python3
"""
PCWB Batch/Texture Mapping Analyzer v3
========================================
Focus on finding how PCRD chunks map to PCIM textures.
Strategy: Look at data between PCRD headers for embedded batch entries.
"""

import struct
import sys
import os


def read_u32(data, off):
    return struct.unpack_from("<I", data, off)[0]

def read_i32(data, off):
    return struct.unpack_from("<i", data, off)[0]

def read_f32(data, off):
    return struct.unpack_from("<f", data, off)[0]


def find_all_aligned(data, magic):
    results = []
    pos = 0
    while pos < len(data) - 3:
        if data[pos:pos+4] == magic:
            results.append(pos)
        pos += 4
    return results


def analyze(path):
    with open(path, "rb") as f:
        data = f.read()

    if data[:4] != b"PCWB":
        return

    pcrds = find_all_aligned(data, b"PCRD")
    pcims = find_all_aligned(data, b"PCIM")

    print(f"=== {os.path.basename(os.path.dirname(path))} ===")
    print(f"PCRDs: {len(pcrds)}, PCIMs: {len(pcims)}")

    # For each PCRD, compute where its index data ends
    # Then look at what's between end of indices and next PCRD
    pcrd_info = []
    for off in pcrds:
        ver = read_u32(data, off + 4)
        field_08 = read_u32(data, off + 0x08)
        idx_count = read_u32(data, off + 0x0C)
        vtx_count = read_u32(data, off + 0x10)
        idx_off = read_u32(data, off + 0x14)
        vtx_off = read_u32(data, off + 0x18)
        idx_end = idx_off + idx_count * 2
        pcrd_info.append({
            'off': off, 'ver': ver, 'field_08': field_08,
            'idx_count': idx_count, 'vtx_count': vtx_count,
            'idx_off': idx_off, 'vtx_off': vtx_off, 'idx_end': idx_end
        })

    # Build a full picture: sorted by offset
    pcrd_sorted = sorted(pcrd_info, key=lambda x: x['off'])

    # Look at data between each PCRD's idx_end and the next structure
    print("\n--- Inter-PCRD data analysis (first 40 PCRDs) ---")
    for i in range(min(40, len(pcrd_sorted))):
        pi = pcrd_sorted[i]
        pcrd_off = pi['off']
        idx_end = pi['idx_end']
        # Find the next PCRD after this one's header
        next_off = pcrd_sorted[i+1]['off'] if i+1 < len(pcrd_sorted) else len(data)

        inter_start = idx_end
        inter_end = next_off
        inter_size = inter_end - inter_start

        # Pad inter_start to 4-byte alignment
        inter_start_aligned = (inter_start + 3) & ~3

        # Dump the inter-PCRD data as u32s
        u32s = []
        for j in range(inter_start_aligned, min(inter_end, inter_start_aligned + 200), 4):
            if j + 4 <= len(data):
                u32s.append((j, read_u32(data, j)))

        # Look for FFFFFFFF pattern (batch entry marker)
        fff_positions = [j for j, v in u32s if v == 0xFFFFFFFF]

        # Look for small values (0-200) that could be texture indices
        small_vals = [(j, v) for j, v in u32s if 0 < v < 200 and v != 1 and v != 2]

        print(f"\n  PCRD[{i}] @0x{pcrd_off:08X} (idx={pi['idx_count']}, vtx={pi['vtx_count']})")
        print(f"    idx ends @0x{idx_end:08X}, next PCRD @0x{next_off:08X}, inter={inter_size} bytes")

        if inter_size > 0 and inter_size < 500:
            # Dump all u32s
            vals_str = " ".join(f"{v:08X}" for _, v in u32s[:40])
            print(f"    data: {vals_str}")
            if fff_positions:
                print(f"    FFF at: {[f'0x{p:08X}' for p in fff_positions]}")
            if small_vals:
                print(f"    small: {[(f'0x{j:08X}', v) for j, v in small_vals[:10]]}")

    # Now look for the pattern: scan for triple-FFFFFFFF and extract texture index
    print("\n\n--- Triple-FFFFFFFF batch entry scan (full file) ---")
    batch_entries = []
    pos = 0
    while pos < len(data) - 12:
        if (read_u32(data, pos) == 0xFFFFFFFF and
            read_u32(data, pos+4) == 0xFFFFFFFF and
            read_u32(data, pos+8) == 0xFFFFFFFF):
            # Found triple-FFF, now look backwards for texture index
            # Pattern observed: tex_idx at pos-4
            if pos >= 4:
                tex_idx = read_u32(data, pos - 4)
                # And PCRD offset at pos + 36 (0x24 from first FFF)
                pcrd_ref = read_u32(data, pos + 36) if pos + 40 <= len(data) else 0
                # Also look at the field 24 bytes after last FFF
                batch_entries.append({
                    'fff_off': pos,
                    'tex_idx': tex_idx,
                    'pcrd_ref': pcrd_ref,
                    'is_pcrd': pcrd_ref in set(p['off'] for p in pcrd_info),
                })
            pos += 12
        else:
            pos += 4

    print(f"  Found {len(batch_entries)} triple-FFF entries")
    pcrd_offset_set = set(p['off'] for p in pcrd_info)

    # Show first 30
    for i, be in enumerate(batch_entries[:30]):
        pcrd_mark = "PCRD" if be['is_pcrd'] else "????"
        tex_valid = "ok" if be['tex_idx'] < len(pcims) else "OOB"
        print(f"  [{i:4d}] FFF@0x{be['fff_off']:08X}  tex={be['tex_idx']:3d} ({tex_valid})  "
              f"pcrd_ref=0x{be['pcrd_ref']:08X} ({pcrd_mark})")

    # Count how many batch entries have valid PCRD refs
    valid = sum(1 for be in batch_entries if be['is_pcrd'])
    valid_tex = sum(1 for be in batch_entries if be['tex_idx'] < len(pcims))
    print(f"\n  Valid PCRD refs: {valid}/{len(batch_entries)}")
    print(f"  Valid tex indices: {valid_tex}/{len(batch_entries)}")

    # Texture index distribution
    from collections import Counter
    tex_counts = Counter(be['tex_idx'] for be in batch_entries if be['tex_idx'] < len(pcims))
    print(f"\n  Texture index distribution (top 20):")
    for idx, count in tex_counts.most_common(20):
        pi_off = pcims[idx] if idx < len(pcims) else 0
        w = read_u32(data, pi_off + 0x9C) if pi_off + 0xA0 <= len(data) else 0
        h = read_u32(data, pi_off + 0xA0) if pi_off + 0xA4 <= len(data) else 0
        print(f"    tex[{idx:2d}] -> PCIM@0x{pi_off:08X} ({w}x{h}): {count} batches")

    # Now build the complete mapping: batch entry -> PCRD -> texture
    print(f"\n--- Building PCRD -> texture mapping ---")
    # For each batch entry, find which PCRD it refers to
    pcrd_to_tex = {}
    for be in batch_entries:
        if be['is_pcrd'] and be['tex_idx'] < len(pcims):
            pcrd_off = be['pcrd_ref']
            pcrd_to_tex[pcrd_off] = be['tex_idx']

    # For PCRDs without a direct batch entry, try nearest batch before them
    mapped = len(pcrd_to_tex)
    print(f"  Direct mapping: {mapped}/{len(pcrds)} PCRDs have texture assignments")

    # Try alternative: assign batch entries to PCRDs by proximity
    # Sort batch entries by fff_off
    batch_sorted = sorted(batch_entries, key=lambda x: x['fff_off'])
    pcrd_offsets_sorted = sorted(pcrds)

    # For each PCRD, find the nearest preceding batch entry
    pcrd_to_tex_nearest = {}
    bi = 0
    for pcrd_off in pcrd_offsets_sorted:
        while bi < len(batch_sorted) - 1 and batch_sorted[bi+1]['fff_off'] < pcrd_off:
            bi += 1
        if bi < len(batch_sorted) and batch_sorted[bi]['fff_off'] < pcrd_off:
            be = batch_sorted[bi]
            if be['tex_idx'] < len(pcims):
                pcrd_to_tex_nearest[pcrd_off] = be['tex_idx']

    print(f"  Nearest-before mapping: {len(pcrd_to_tex_nearest)}/{len(pcrds)} PCRDs")

    # Show sample of the mapping
    print(f"\n  Sample nearest-before assignments (first 30):")
    for pcrd_off in pcrd_offsets_sorted[:30]:
        tex = pcrd_to_tex_nearest.get(pcrd_off, -1)
        direct_tex = pcrd_to_tex.get(pcrd_off, -1)
        mark = " *DIRECT*" if direct_tex >= 0 else ""
        print(f"    PCRD@0x{pcrd_off:08X} -> tex={tex}{mark}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: pcwb_analyze3.py <world.pcwb>")
        sys.exit(1)
    analyze(sys.argv[1])
