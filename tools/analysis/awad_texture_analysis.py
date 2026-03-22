"""
AWAD TOC analysis — find NM40↔PCIM relationships
Dumps full TOC with nameHash, typeHash, dataOffset, and data magic.
Checks if character name hashes match any entries.
"""
import struct, zlib, sys, os

def hash_string(s):
    """Engine's HashString at 0x405380 — verified by decompile"""
    h = 0
    for c in s.encode('ascii'):
        h = (h + c + ((h << (c & 7)) & 0xFFFFFFFF)) & 0xFFFFFFFF
    return h

def decompress_zwd(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:4] in (b'ZLIB', b'SFZC'):
        comp_size = struct.unpack_from('<I', data, 4)[0]
        decomp_size = struct.unpack_from('<I', data, 8)[0]
        return zlib.decompress(data[12:12+comp_size])
    return data

def analyze_awad(path):
    print(f"\n{'='*60}")
    print(f"Analyzing: {os.path.basename(path)}")
    print(f"{'='*60}")

    data = decompress_zwd(path)
    if data[:4] != b'AWAD':
        print("Not AWAD format!")
        return

    version = struct.unpack_from('<I', data, 4)[0]
    count = struct.unpack_from('<I', data, 8)[0]
    print(f"AWAD version={version}, entries={count}, size={len(data)} bytes")

    # Parse TOC
    entries = []
    for i in range(count):
        toc_base = 12 + i * 8
        if toc_base + 8 > len(data):
            break
        name_hash = struct.unpack_from('<I', data, toc_base)[0]
        ent_ptr = struct.unpack_from('<I', data, toc_base + 4)[0]

        if ent_ptr + 8 > len(data):
            continue
        type_hash = struct.unpack_from('<I', data, ent_ptr)[0]
        data_off = struct.unpack_from('<I', data, ent_ptr + 4)[0]

        # Read magic at data offset
        magic = ''
        if data_off + 4 <= len(data):
            raw = data[data_off:data_off+4]
            if all(32 <= b < 127 for b in raw):
                magic = raw.decode('ascii')
            else:
                magic = raw.hex()

        entries.append({
            'nameHash': name_hash,
            'typeHash': type_hash,
            'dataOff': data_off,
            'magic': magic,
            'entPtr': ent_ptr
        })

    # Categorize by type
    TYPE_NAMES = {
        0x0000BB12: 'NM40/Mesh',
        0x01F1096F: 'PCIM/Image',
        0x0006D8A6: 'SCT/Script',
        0x04339C43: 'DBDB/Database',
        0x00020752: 'PCPB/Prop',
        0x44FE8920: 'Animation',
        0xFA5E717C: 'AnimList',
        0x000FD514: 'Table',
        0x06572A64: 'Playlist',
        0xA117D668: 'STTL',
        0x690BE5E8: 'Strings',
        0x2A7E6F30: 'AssetMap',
        0xBCBFB478: 'ActionSM',
        0x5550C44A: 'BehaviorSM',
        0xB8D1C6C6: 'ControlMap',
        0xE2F8E66E: 'AudioFX',
        0x194738A4: 'NavMesh',
        0x009A2807: 'Font',
        0xF9C2A901: 'Particles',
        0x3072B942: 'Cinematic',
    }

    nm40_entries = [e for e in entries if e['typeHash'] == 0x0000BB12]
    pcim_entries = [e for e in entries if e['typeHash'] == 0x01F1096F]

    print(f"\nNM40 entries: {len(nm40_entries)}")
    for e in nm40_entries:
        # Read bone count from NM40 header at data+8
        bones = 0
        if e['dataOff'] + 0x0A <= len(data):
            bones = struct.unpack_from('<H', data, e['dataOff'] + 8)[0]
        print(f"  nameHash=0x{e['nameHash']:08X} off=0x{e['dataOff']:X} bones={bones}")

    print(f"\nPCIM entries: {len(pcim_entries)}")
    for e in pcim_entries[:30]:  # first 30
        w, h = 0, 0
        if e['dataOff'] + 0xA4 <= len(data):
            w = struct.unpack_from('<I', data, e['dataOff'] + 0x9C)[0]
            h = struct.unpack_from('<I', data, e['dataOff'] + 0xA0)[0]
        print(f"  nameHash=0x{e['nameHash']:08X} off=0x{e['dataOff']:X} {w}x{h}")

    # Check known character names
    print(f"\n--- Character name hash check ---")
    char_names = [
        "Mallory", "Jared", "Simon", "GoblinB", "Goblin", "BullGoblin",
        "Helen", "MrTibbs", "StraySod", "Thimbletack", "HogSqueal",
        "RedCap", "FireSalamander", "Actor", "SpriteAIObject",
        "DarkJared", "DarkSimon", "DarkMallory"
    ]

    all_hashes = {e['nameHash']: e for e in entries}
    pcim_hashes = {e['nameHash']: e for e in pcim_entries}
    nm40_hashes = {e['nameHash']: e for e in nm40_entries}

    for name in char_names:
        h = hash_string(name)
        in_any = "YES" if h in all_hashes else "no"
        in_pcim = "PCIM!" if h in pcim_hashes else ""
        in_nm40 = "NM40!" if h in nm40_hashes else ""
        if in_any == "YES" or True:
            print(f"  '{name}' hash=0x{h:08X} → {in_any} {in_pcim} {in_nm40}")

    # Check shared nameHashes between NM40 and PCIM
    shared = set(nm40_hashes.keys()) & set(pcim_hashes.keys())
    print(f"\nShared NM40↔PCIM nameHashes: {len(shared)}")
    for h in shared:
        print(f"  0x{h:08X}: NM40@0x{nm40_hashes[h]['dataOff']:X} PCIM@0x{pcim_hashes[h]['dataOff']:X}")

    # Check entry pointer structure — look at bytes around entPtr
    print(f"\n--- Entry pointer analysis (first 5 entries) ---")
    for e in entries[:5]:
        ep = e['entPtr']
        if ep + 16 <= len(data):
            raw = data[ep:ep+16]
            print(f"  entPtr=0x{ep:X}: {raw.hex()} nameHash=0x{e['nameHash']:08X}")

# Analyze game WADs
game_dir = r"H:\Games\.Archive\SPIDEWICK"
for wad in ["ww/Wads/Common.zwd", "ww/Wads/GroundsD.zwd", "ww/Wads/MansionD.zwd"]:
    path = os.path.join(game_dir, wad)
    if os.path.exists(path):
        try:
            analyze_awad(path)
        except Exception as ex:
            print(f"Error: {ex}")
