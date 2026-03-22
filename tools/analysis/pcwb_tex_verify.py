#!/usr/bin/env python3
"""
Verify texture mapping: check if texture_index from batch entries
maps directly to Nth PCIM, or through a reference table in the header.
"""

import struct
import sys
import os


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


def analyze(path):
    with open(path, "rb") as f:
        data = f.read()

    if data[:4] != b"PCWB":
        return

    pcims = find_all_aligned(data, b"PCIM")
    pcrds = find_all_aligned(data, b"PCRD")

    print(f"PCIMs: {len(pcims)}, PCRDs: {len(pcrds)}")

    # Header fields
    print("\n--- PCWB Header ---")
    for off in range(0x04, 0xA0, 4):
        val = read_u32(data, off)
        # Check if this value looks like a file offset to a PCIM
        is_pcim = "PCIM!" if val in set(pcims) else ""
        print(f"  +0x{off:02X} = 0x{val:08X} ({val:>10,}) {is_pcim}")

    # Look for a texture reference table in the header
    # This table maps texture_index -> PCIM offset
    # Try scanning for consecutive PCIM offsets
    pcim_set = set(pcims)
    first_pcrd = pcrds[0] if pcrds else len(data)
    first_pcim = pcims[0] if pcims else len(data)
    header_end = min(first_pcrd, first_pcim)

    print(f"\n--- Scanning header (0 - 0x{header_end:X}) for PCIM offset arrays ---")

    # Look for ANY values in header that match PCIM offsets
    pcim_refs_in_header = []
    for off in range(0x30, header_end, 4):
        val = read_u32(data, off)
        if val in pcim_set:
            pcim_refs_in_header.append((off, val))

    print(f"  Found {len(pcim_refs_in_header)} PCIM refs in header")
    for off, val in pcim_refs_in_header[:30]:
        pidx = pcims.index(val)
        w = read_u32(data, val + 0x9C) if val + 0xA4 <= len(data) else 0
        h = read_u32(data, val + 0xA0) if val + 0xA4 <= len(data) else 0
        print(f"    @0x{off:06X} -> 0x{val:08X} = PCIM[{pidx}] ({w}x{h})")

    # Also check: the batch tex indices - what's the max?
    pcrd_set = set(pcrds)
    batch_tex_indices = set()
    pos = 0
    while pos < len(data) - 12:
        if (read_u32(data, pos) == 0xFFFFFFFF and
            read_u32(data, pos + 4) == 0xFFFFFFFF and
            read_u32(data, pos + 8) == 0xFFFFFFFF):
            if pos >= 4 and pos + 40 <= len(data):
                tex_idx = read_u32(data, pos - 4)
                pcrd_ref = read_u32(data, pos + 36)
                if pcrd_ref in pcrd_set:
                    batch_tex_indices.add(tex_idx)
            pos += 12
        else:
            pos += 4

    print(f"\n--- Batch texture indices ---")
    print(f"  Unique indices: {len(batch_tex_indices)}")
    print(f"  Range: {min(batch_tex_indices)} - {max(batch_tex_indices)}")
    print(f"  Total PCIMs: {len(pcims)}")
    sorted_idx = sorted(batch_tex_indices)
    print(f"  All: {sorted_idx}")

    # Check: do batch indices exceed PCIM count?
    oob = [i for i in sorted_idx if i >= len(pcims)]
    print(f"  Out of bounds (>= {len(pcims)}): {oob}")

    # Look for a lookup table: array of offsets or indices
    # at various header positions
    print(f"\n--- Checking header for texture table ---")

    # What's at offset right after known header fields?
    # The header region 0x60-0x100 often contains table offsets
    for off in range(0x60, min(0x200, header_end), 4):
        val = read_u32(data, off)
        # Is val a valid offset within the header?
        if 0x100 < val < header_end:
            # Check if what's AT that offset looks like PCIM offsets
            if val + 16 <= len(data):
                v0 = read_u32(data, val)
                v1 = read_u32(data, val + 4)
                v2 = read_u32(data, val + 8)
                is_table = ""
                if v0 in pcim_set:
                    is_table = f" -> starts with PCIM@0x{v0:08X}!"
                elif v1 in pcim_set:
                    is_table = f" -> +4 has PCIM@0x{v1:08X}!"
                if is_table:
                    print(f"  +0x{off:02X} = 0x{val:08X} (offset) {is_table}")

    # Dump what's at common table offsets
    print(f"\n--- Checking known offsets for PCIM ref table ---")
    for table_start in [0x94, 0x98, 0xA0, 0x64, 0x6C, 0x74, 0x78, 0x7C]:
        val = read_u32(data, table_start)
        if 0x100 < val < header_end and val + 64 <= len(data):
            print(f"\n  Offset 0x{table_start:02X} -> 0x{val:08X}:")
            for k in range(min(10, (header_end - val) // 4)):
                entry = read_u32(data, val + k * 4)
                mark = " PCIM!" if entry in pcim_set else ""
                print(f"    [{k:2d}] 0x{entry:08X}{mark}")

    # Check if header offset 0x64 and 0x6C point to table of texture data
    off_64 = read_u32(data, 0x64)
    off_6c = read_u32(data, 0x6C)
    print(f"\n--- Region at +0x64 offset (0x{off_64:08X}) ---")
    if off_64 + 256 <= len(data):
        for k in range(32):
            val = read_u32(data, off_64 + k * 4)
            mark = " PCIM!" if val in pcim_set else ""
            print(f"  [{k:2d}] @0x{off_64 + k*4:08X} = 0x{val:08X}{mark}")

    print(f"\n--- Region at +0x6C offset (0x{off_6c:08X}) ---")
    if off_6c + 256 <= len(data):
        for k in range(32):
            val = read_u32(data, off_6c + k * 4)
            mark = " PCIM!" if val in pcim_set else ""
            print(f"  [{k:2d}] @0x{off_6c + k*4:08X} = 0x{val:08X}{mark}")


if __name__ == "__main__":
    analyze(sys.argv[1])
