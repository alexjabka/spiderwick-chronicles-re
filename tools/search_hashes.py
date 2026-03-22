import struct, zlib

def hs(s):
    r = 0
    for c in s.encode('ascii'):
        r = (r + c + ((r << (c & 7)) & 0xFFFFFFFF)) & 0xFFFFFFFF
    return r

with open('h:/Games/.Archive/SPIDEWICK/ww/Wads/MnAttack.zwd', 'rb') as f:
    data = f.read()
cs = struct.unpack_from('<I', data, 4)[0]
d = zlib.decompress(data[12:12+cs])
v, cnt = struct.unpack_from('<II', d, 4)
ah = {}
for i in range(cnt):
    n, e = struct.unpack_from('<II', d, 12+i*8)
    if e+8<=len(d):
        t, o = struct.unpack_from('<II', d, e)
        ah[n] = (t, o)

tn = {0x0000BB12:'NM40',0x01F1096F:'PCIM',0x00020752:'PCPB',0x44FE8920:'ANIM',0x0006D8A6:'SCT'}
chars = ['Mallory','Jared','Simon','Helen','GoblinB','GoblinEasy','GoblinMedium','BullGoblin','ThimbleTack','MrTibbs','StraySod','LeatherLeaf']
pfx = ['', 'actors/', 'actors\\', 'char/', 'models/', 'ww/actors/', 'ww/', 'na/', 'characters/']
sfx = ['', '.pcg', '.nm40', '.pci', '_body', '_skin', '_mesh', '_diffuse', '_idle', '_default']

for c in chars:
    for nm in [c, c.lower()]:
        for p in pfx:
            for s in sfx:
                path = p + nm + s
                h = hs(path)
                if h in ah:
                    t, o = ah[h]
                    print(f'MATCH: "{path}" -> 0x{h:08X} {tn.get(t,hex(t))} @0x{o:X}')
print('Done.')
