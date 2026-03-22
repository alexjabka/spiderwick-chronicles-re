#!/usr/bin/env python3
"""Dump full PCIM headers to find where texture_index is stored."""
import struct, sys

def r32(d, o): return struct.unpack_from("<I", d, o)[0]
def rf32(d, o): return struct.unpack_from("<f", d, o)[0]

def find_pcims(data):
    pcims = []
    pos = 0
    while pos < len(data) - 0xC1:
        if data[pos:pos+4] == b"PCIM":
            ver = r32(data, pos + 4)
            total_sz = r32(data, pos + 8)
            dds_sz = r32(data, pos + 0x0C)
            dds_off = r32(data, pos + 0x10)
            if ver == 2 and 0 < total_sz < len(data) and 0 < dds_sz <= total_sz:
                dds_ok = False
                if dds_off + 4 <= len(data) and data[dds_off:dds_off+4] == b"DDS ":
                    dds_ok = True
                elif pos + 0xC1 + 4 <= len(data) and data[pos+0xC1:pos+0xC1+4] == b"DDS ":
                    dds_ok = True
                if dds_ok:
                    pcims.append(pos)
                    if dds_off == pos + 0xC1:
                        pos += (0xC1 + dds_sz + 3) & ~3
                    else:
                        pos += 0xC4
                    continue
        pos += 4
    return pcims

def main(path):
    with open(path, "rb") as f:
        data = f.read()

    pcims = find_pcims(data)
    print(f"PCIMs: {len(pcims)}")

    # Parse header texture ref table
    header_map = {}  # pcim_off -> tex_idx (from header table)
    table_ptr = r32(data, 0x94)
    if 0 < table_ptr < len(data):
        pos = table_ptr
        while pos + 16 <= len(data):
            tex_idx = r32(data, pos)
            pcim_off = r32(data, pos + 4)
            if pcim_off > 0 and pcim_off + 4 <= len(data) and data[pcim_off:pcim_off+4] == b"PCIM":
                header_map[pcim_off] = tex_idx
            else:
                if not (pos == table_ptr and tex_idx == 0):
                    break
            pos += 16

    print(f"Header table entries: {len(header_map)}")

    # Dump FULL 193-byte header of each PCIM, comparing with known tex_idx
    print(f"\n{'idx':>4} {'offset':>10} {'WxH':>10} {'hdr_idx':>8}  header bytes (hex, every 4 bytes)")
    for i, off in enumerate(pcims):
        w = r32(data, off + 0x9C)
        h = r32(data, off + 0xA0)
        known_idx = header_map.get(off, "?")

        # Dump all u32 values in the 193-byte header
        # Focus on finding which field matches known_idx
        fields = []
        for foff in range(0, 0xC1, 4):
            if off + foff + 4 <= len(data):
                fields.append(r32(data, off + foff))

        # Find fields that match known_idx
        matches = []
        if isinstance(known_idx, int):
            for j, v in enumerate(fields):
                if v == known_idx and j > 1:  # skip magic and version
                    matches.append(f"+0x{j*4:02X}")

        match_str = f" MATCH@{','.join(matches)}" if matches else ""

        if i < 30 or matches:
            # Print compact: show fields that differ between PCIMs
            # Skip magic(+0), version(+4), total_sz(+8), dds_sz(+C), dds_off(+10)
            interesting = []
            for foff in [0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30,
                         0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50,
                         0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x70,
                         0x74, 0x78, 0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90,
                         0x94, 0x98, 0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xB0,
                         0xB4, 0xB8, 0xBC]:
                v = r32(data, off + foff) if off + foff + 4 <= len(data) else 0
                interesting.append(f"{v:08X}")

            print(f"[{i:3d}] @0x{off:08X} {w:4d}x{h:<4d} idx={str(known_idx):>4s}  "
                  f"{' '.join(interesting[:16])}{match_str}")

    # Check: look at the 4 bytes BEFORE each PCIM header
    # Maybe the index is stored just before the PCIM magic
    print(f"\n--- 16 bytes BEFORE each PCIM ---")
    for i, off in enumerate(pcims[:30]):
        known_idx = header_map.get(off, "?")
        if off >= 16:
            pre = [r32(data, off - 16 + j) for j in range(0, 16, 4)]
            print(f"  [{i:3d}] idx={str(known_idx):>4s}  "
                  f"pre: {' '.join(f'{v:08X}' for v in pre)}  "
                  f"PCIM@0x{off:08X}")

main(sys.argv[1])
