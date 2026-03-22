#!/usr/bin/env python3
"""Show AWAD entries near NM40s to understand asset grouping."""
import sys, struct, zlib

def read_u32(d, o): return struct.unpack_from("<I", d, o)[0]

MAGICS = {
    b"NM40": "NM40", b"PCIM": "PCIM", b"PCRD": "PCRD", b"SCT\x00": "SCT",
    b"DBDB": "DBDB", b"STTL": "STTL", b"PCWB": "PCWB", b"AWAD": "AWAD",
    b"adat": "adat", b"aniz": "aniz", b"AMAP": "AMAP", b"Char": "Char",
    b"PCPB": "PCPB", b"NAVM": "NAVM", b"brxb": "brxb", b"arpc": "arpc",
    b"play": "play", b"Worl": "Worl", b"hier": "hier", b"skel": "skel",
    b"EPC\x00": "EPC",
}

def detect(d, off, sz):
    if off + 4 > sz: return "???"
    m = d[off:off+4]
    return MAGICS.get(m, f"?{m.hex()}")

def main():
    zwd = sys.argv[1] if len(sys.argv) > 1 else r"H:\Games\.Archive\SPIDEWICK\ww\Wads\GroundsD.zwd"
    with open(zwd, "rb") as f:
        f.read(4); csz, dsz = struct.unpack("<II", f.read(8))
        blob = zlib.decompress(f.read())

    count = read_u32(blob, 8)
    entries = []
    for i in range(count):
        nh = read_u32(blob, 12 + i*8)
        ep = read_u32(blob, 12 + i*8 + 4)
        th = read_u32(blob, ep)
        do = read_u32(blob, ep + 4)
        entries.append((nh, th, do, i))
    entries.sort(key=lambda e: e[2])

    # Add sizes
    full = []
    for idx, (nh, th, off, orig) in enumerate(entries):
        nxt = entries[idx+1][2] if idx+1 < len(entries) else len(blob)
        full.append((nh, th, off, nxt - off, orig))

    # Find NM40 indices
    nm40_indices = [i for i, (nh,th,off,sz,orig) in enumerate(full) if detect(blob, off, len(blob)) == "NM40"]

    for ni in nm40_indices[:5]:
        start = max(0, ni - 3)
        end = min(len(full), ni + 8)
        print(f"\n--- Around NM40 at sorted index {ni} ---")
        for j in range(start, end):
            nh, th, off, sz, orig = full[j]
            typ = detect(blob, off, len(blob))
            marker = " <<<" if j == ni else ""
            extra = ""
            if typ == "PCIM" and off + 0xA4 < len(blob):
                w = read_u32(blob, off + 0x9C)
                h = read_u32(blob, off + 0xA0)
                extra = f" ({w}x{h})"
            elif typ == "NM40" and off + 0x0A < len(blob):
                bones = struct.unpack_from("<H", blob, off + 0x08)[0]
                extra = f" ({bones} bones)"
            print(f"  [{j:3d}] hash=0x{nh:08X} type=0x{th:08X} {typ:5s} off=0x{off:07X} sz={sz:>8}{extra}{marker}")

if __name__ == "__main__":
    main()
