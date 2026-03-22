#!/usr/bin/env python3
"""Dump PCWB header tables in detail to find texture lookup table."""
import struct, sys, os

def r32(d, o): return struct.unpack_from("<I", d, o)[0]
def rf32(d, o): return struct.unpack_from("<f", d, o)[0]

def main(path):
    with open(path, "rb") as f:
        data = f.read()

    # Validated PCIM finder
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

    pcim_set = set(pcims)
    print(f"Validated PCIMs: {len(pcims)}")

    # Header pointer fields
    header_ptrs = {}
    for off in range(0x60, 0xA0, 4):
        val = r32(data, off)
        if 0x100 < val < 0x100000:  # looks like a file offset
            header_ptrs[off] = val
            print(f"  +0x{off:02X} -> 0x{val:08X}")

    # For each header pointer, dump the region it points to
    for hdr_off, ptr in sorted(header_ptrs.items()):
        print(f"\n=== Table at +0x{hdr_off:02X} -> 0x{ptr:08X} ===")
        # Dump first 256 bytes as u32
        for k in range(64):
            addr = ptr + k * 4
            if addr + 4 > len(data):
                break
            val = r32(data, addr)
            marks = []
            if val in pcim_set:
                idx = pcims.index(val)
                marks.append(f"PCIM[{idx}]")
            # Check if it's a float
            fval = rf32(data, addr)
            if 0.001 < abs(fval) < 10000 and not (val > 0x10000):
                marks.append(f"f={fval:.3f}")
            # Check ASCII
            try:
                s = data[addr:addr+4]
                if all(32 <= b < 127 for b in s):
                    marks.append(f"'{s.decode()}'")
            except:
                pass
            mark_str = f"  ({', '.join(marks)})" if marks else ""
            print(f"  [{k:3d}] @0x{addr:06X} = 0x{val:08X}{mark_str}")

    # Also check: what's at the field +0x90 and +0x94 which were documented
    # as "texture ref table"
    print(f"\n=== Examining header +0x90 and +0x94 ===")
    val_90 = r32(data, 0x90)
    val_94 = r32(data, 0x94)
    print(f"  +0x90 = 0x{val_90:08X}")
    print(f"  +0x94 = 0x{val_94:08X}")

    if 0x100 < val_94 < len(data):
        print(f"\n  Data at 0x{val_94:08X} (stride 4):")
        for k in range(min(40, (len(data) - val_94) // 4)):
            addr = val_94 + k * 4
            val = r32(data, addr)
            mark = f" -> PCIM[{pcims.index(val)}]" if val in pcim_set else ""
            print(f"    [{k:3d}] 0x{val:08X}{mark}")

    if 0x100 < val_90 < len(data):
        print(f"\n  Data at 0x{val_90:08X} (stride 4):")
        for k in range(min(40, (len(data) - val_90) // 4)):
            addr = val_90 + k * 4
            val = r32(data, addr)
            mark = f" -> PCIM[{pcims.index(val)}]" if val in pcim_set else ""
            print(f"    [{k:3d}] 0x{val:08X}{mark}")

    # Scan ENTIRE header region for any array of PCIM offsets
    first_section = pcims[0] if pcims else len(data)
    pcrds_start = 0
    p = 0
    while p < len(data) - 4:
        if data[p:p+4] == b"PCRD" and r32(data, p+4) == 2:
            pcrds_start = p
            break
        p += 4

    header_end = min(first_section, pcrds_start) if pcrds_start else first_section
    print(f"\n=== Scanning 0-0x{header_end:X} for PCIM offset sequences ===")

    best_run = 0
    best_start = 0
    best_stride = 0
    for stride in [4, 8, 12, 16, 20, 24]:
        for start in range(0x30, min(header_end, 0x50000), 4):
            run = 0
            for k in range(500):
                addr = start + k * stride
                if addr + 4 > header_end:
                    break
                val = r32(data, addr)
                if val in pcim_set:
                    run += 1
                else:
                    break
            if run > best_run:
                best_run = run
                best_start = start
                best_stride = stride

    if best_run >= 3:
        print(f"  Best: {best_run} consecutive PCIM refs, start=0x{best_start:X}, stride={best_stride}")
        for k in range(min(best_run, 30)):
            addr = best_start + k * best_stride
            val = r32(data, addr)
            idx = pcims.index(val) if val in pcim_set else -1
            # Dump the full entry
            entry = [r32(data, addr + j) for j in range(0, best_stride, 4)]
            print(f"    [{k:3d}] {' '.join(f'{v:08X}' for v in entry)}  (PCIM[{idx}])")
    else:
        print(f"  No PCIM offset table found (best run: {best_run})")

    # Maybe the table doesn't contain PCIM offsets but something else.
    # Let me check: each PCIM has internal fields that could serve as ID.
    # Dump some PCIM header fields that might be an identifier
    print(f"\n=== PCIM header analysis (first 20) ===")
    for i, off in enumerate(pcims[:20]):
        # Dump fields we don't know yet (between +0x14 and +0x94)
        fields = []
        for foff in [0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30]:
            if off + foff + 4 <= len(data):
                fields.append(r32(data, off + foff))
        w = r32(data, off + 0x9C)
        h = r32(data, off + 0xA0)
        print(f"  [{i:3d}] @0x{off:08X} {w}x{h}  "
              f"fields: {' '.join(f'{v:08X}' for v in fields)}")

main(sys.argv[1])
