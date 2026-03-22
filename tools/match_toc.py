import struct, zlib, sys
sys.stdout.reconfigure(encoding='utf-8')

with open('h:/Games/.Archive/SPIDEWICK/ww/Wads/MnAttack.zwd', 'rb') as f:
    data = f.read()
cs = struct.unpack_from('<I', data, 4)[0]
d = zlib.decompress(data[12:12+cs])
ver, count = struct.unpack_from('<II', d, 4)

off_map = {}
for i in range(count):
    nh, ep = struct.unpack_from('<II', d, 12+i*8)
    if ep+8 <= len(d):
        th, doff = struct.unpack_from('<II', d, ep)
        off_map[doff] = (nh, th, i)

targets = {
    'Mallory_NM40': 0x00F21000, 'Mallory_1k': 0x03375000, 'Mallory_512': 0x00A3A000,
    'Jared_NM40':   0x00EA2000, 'Jared_1k':   0x03C57000, 'Jared_512':   0x04B87000,
    'Simon_NM40':   0x04DA4000, 'Simon_1k':   0x015D9000, 'Simon_512':   0x03939000,
    'Helen_NM40':   0x03308000, 'Helen_1k':   0x02BF4000, 'Helen_512':   0x0142E000,
    'Goblin_NM40':  0x030AA000, 'Goblin_1k':  0x01E76000,
}

tn = {0x0000BB12:'NM40', 0x01F1096F:'PCIM', 0xFA5E717C:'ANIZ'}

print('Matching runtime offsets to AWAD TOC entries:')
for name, runtime_off in sorted(targets.items()):
    best = None
    # Runtime ptrs are at TOC_offset + 0x4000 (AWAD header/TOC size)
    # So TOC_offset = runtime_off - 0x4000
    HEADER_SIZE = 0x4000
    check = runtime_off - HEADER_SIZE
    if check in off_map:
        best = (check, off_map[check], -HEADER_SIZE)
    else:
        # Try nearby
        for delta in range(-0x5000, 0x5001, 0x1000):
            check = runtime_off + delta
            if check in off_map:
                best = (check, off_map[check], delta)
                break
    if best:
        toc_off, (nh, th, idx), delta = best
        t = tn.get(th, hex(th))
        print(f'  {name:15s} -> TOC[{idx:3d}] nH=0x{nh:08X} {t:4s} (delta={delta:+d})')
    else:
        print(f'  {name:15s} -> NO MATCH')

# Print final mapping table for SpiderView
print('\n=== SPIDERVIEW MAPPING TABLE ===')
print('// NM40 nameHash -> {main_diffuse_nameHash, secondary_nameHash}')
chars = ['Mallory', 'Jared', 'Simon', 'Helen', 'Goblin']
for c in chars:
    nm = targets.get(f'{c}_NM40')
    p1k = targets.get(f'{c}_1k')
    p512 = targets.get(f'{c}_512')

    nm_nh = 0
    p1k_nh = 0
    p512_nh = 0

    H = 0x4000
    if nm and (nm-H) in off_map: nm_nh = off_map[nm-H][0]
    if p1k and (p1k-H) in off_map: p1k_nh = off_map[p1k-H][0]
    if p512 and (p512-H) in off_map: p512_nh = off_map[p512-H][0]

    print(f'  {{0x{nm_nh:08X}, 0x{p1k_nh:08X}, 0x{p512_nh:08X}}}, // {c}')
