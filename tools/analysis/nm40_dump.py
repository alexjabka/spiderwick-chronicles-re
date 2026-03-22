#!/usr/bin/env python3
"""Dump NM40 header + embedded PCRD from a ZWD archive via AWAD TOC."""
import sys, struct, zlib

def read_u32(d, o): return struct.unpack_from("<I", d, o)[0]
def read_u16(d, o): return struct.unpack_from("<H", d, o)[0]

def main():
    zwd = sys.argv[1] if len(sys.argv) > 1 else r"H:\Games\.Archive\SPIDEWICK\ww\Wads\GroundsD.zwd"

    with open(zwd, "rb") as f:
        magic = f.read(4)
        csz, dsz = struct.unpack("<II", f.read(8))
        blob = zlib.decompress(f.read())

    print(f"Decompressed: {len(blob)} bytes")
    assert blob[:4] == b"AWAD"

    ver = read_u32(blob, 4)
    count = read_u32(blob, 8)
    print(f"AWAD v{ver}, {count} entries")

    # Parse TOC
    entries = []
    for i in range(count):
        name_hash = read_u32(blob, 12 + i*8)
        entry_ptr = read_u32(blob, 12 + i*8 + 4)
        type_hash = read_u32(blob, entry_ptr)
        data_off  = read_u32(blob, entry_ptr + 4)
        entries.append((name_hash, type_hash, data_off, i))

    entries.sort(key=lambda e: e[2])

    # Find NM40 entries
    nm40s = []
    for idx, (nhash, thash, off, orig) in enumerate(entries):
        next_off = entries[idx+1][2] if idx+1 < len(entries) else len(blob)
        size = next_off - off
        magic4 = blob[off:off+4]
        if magic4 == b"NM40":
            nm40s.append((nhash, off, size))

    print(f"\nFound {len(nm40s)} NM40 entries in AWAD TOC")

    # Dump first 3 NM40s
    for i, (nhash, off, size) in enumerate(nm40s[:3]):
        d = blob[off:]
        print(f"\n{'='*60}")
        print(f"NM40 #{i}: hash=0x{nhash:08X}, offset=0x{off:X}, AWAD_size={size}")

        # Header dump (first 64 bytes)
        print(f"\nHeader (64 bytes):")
        for row in range(4):
            hex_str = " ".join(f"{d[row*16+c]:02X}" for c in range(16))
            vals = []
            for c in range(4):
                v = read_u32(blob, off + row*16 + c*4)
                f = struct.unpack_from("<f", blob, off + row*16 + c*4)[0]
                if v < 0x10000:
                    vals.append(f"{v:>8}")
                elif 0.001 < abs(f) < 100000:
                    vals.append(f"{f:>8.3f}")
                else:
                    vals.append(f"0x{v:08X}")

            print(f"  +0x{row*16:02X}: {hex_str}  | {' '.join(vals)}")

        # Key fields
        bones = read_u16(blob, off + 0x08)
        skel  = read_u16(blob, off + 0x0A)
        f28   = read_u32(blob, off + 0x28)
        f2c   = read_u32(blob, off + 0x2C)
        f30   = read_u32(blob, off + 0x30)
        f34   = read_u32(blob, off + 0x34)
        f38   = read_u32(blob, off + 0x38)
        f3c   = read_u32(blob, off + 0x3C)

        print(f"\n  bones={bones}, skelBones={skel}")
        print(f"  +0x28=0x{f28:08X} ({f28})  +0x2C=0x{f2c:08X} ({f2c})")
        print(f"  +0x30=0x{f30:08X} ({f30})  +0x34=0x{f34:08X} ({f34})")
        print(f"  +0x38=0x{f38:08X} ({f38})  +0x3C=0x{f3c:08X} ({f3c})")

        # Scan for PCRD within the AWAD-sized NM40 entry
        pcrd_off = None
        for pos in range(0x40, min(size, 0x10000), 4):
            if blob[off+pos:off+pos+4] == b"PCRD":
                pcrd_off = pos
                break

        if pcrd_off:
            print(f"\n  PCRD found at NM40+0x{pcrd_off:X} (abs 0x{off+pcrd_off:X})")
            pd = blob[off+pcrd_off:]
            print(f"  PCRD header:")
            for row in range(2):
                hex_str = " ".join(f"{pd[row*16+c]:02X}" for c in range(16))
                vals = []
                for c in range(4):
                    v = read_u32(blob, off+pcrd_off + row*16 + c*4)
                    vals.append(f"0x{v:08X}" if v > 0xFFFF else f"{v:>8}")
                print(f"    +0x{row*16:02X}: {hex_str}  | {' '.join(vals)}")

            idx_count = read_u32(blob, off+pcrd_off + 12)
            vtx_count = read_u32(blob, off+pcrd_off + 16)
            idx_off   = read_u32(blob, off+pcrd_off + 20)
            vtx_off   = read_u32(blob, off+pcrd_off + 24)
            print(f"  idxCount={idx_count}, vtxCount={vtx_count}")
            print(f"  idxOff=0x{idx_off:X} (NM40-relative), vtxOff=0x{vtx_off:X} (NM40-relative)")

            # Check if offsets are NM40-relative or blob-relative
            if vtx_off < size:
                vx, vy, vz = struct.unpack_from("<fff", blob, off + vtx_off)
                print(f"  First vtx (NM40-relative): {vx:.3f} {vy:.3f} {vz:.3f}")
            if vtx_off + off < len(blob):
                vx, vy, vz = struct.unpack_from("<fff", blob, vtx_off)
                print(f"  First vtx (blob-absolute): {vx:.3f} {vy:.3f} {vz:.3f}")
        else:
            print(f"\n  No PCRD found in first 0x10000 bytes")

    # --- Cross-reference: find PCIM hashes referenced within NM40 data ---
    print(f"\n{'='*60}")
    print(f"=== Cross-reference: NM40 -> PCIM texture matches ===")

    # Collect all PCIM name hashes
    pcim_entries = {}
    for idx, (nhash, thash, doff, orig) in enumerate(entries):
        next_off = entries[idx+1][2] if idx+1 < len(entries) else len(blob)
        sz = next_off - doff
        if doff + 4 <= len(blob) and blob[doff:doff+4] == b"PCIM":
            pcim_entries[nhash] = (doff, sz)

    print(f"Archive has {len(pcim_entries)} PCIM entries")

    # For first 3 NM40s, scan their data for PCIM hash references
    for i, (nhash, off, size) in enumerate(nm40s[:3]):
        print(f"\nNM40 #{i} (hash=0x{nhash:08X}, size={size}):")
        matches = []
        for doff in range(0, min(size, 0x1000), 4):
            v = read_u32(blob, off + doff)
            if v in pcim_entries:
                matches.append((doff, v))
        if matches:
            for moff, mhash in matches:
                pcim_off, pcim_sz = pcim_entries[mhash]
                w = read_u32(blob, pcim_off + 0x9C) if pcim_off + 0xA0 < len(blob) else 0
                h = read_u32(blob, pcim_off + 0xA0) if pcim_off + 0xA4 < len(blob) else 0
                print(f"  +0x{moff:03X}: refs PCIM 0x{mhash:08X} at blob+0x{pcim_off:X} ({w}x{h})")
        else:
            print(f"  No PCIM hash matches in first 0x1000 bytes")
            # Try scanning the submesh table area specifically
            sub_off = read_u32(blob, off + 0x3C)
            mesh_off = read_u32(blob, off + 0x34)
            print(f"  subTblOff=0x{sub_off:X}, meshTblOff=0x{mesh_off:X}")
            if sub_off < size:
                print(f"  SubTbl bytes: {blob[off+sub_off:off+sub_off+64].hex(' ')}")

if __name__ == "__main__":
    main()
