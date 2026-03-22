#!/usr/bin/env python3
"""
Final comprehensive asset name dictionary builder.
Combines all sources: exe strings, text assets, brute-force word combos.
"""
import struct, zlib, os, re, sys
from pathlib import Path
from collections import defaultdict, Counter

if sys.platform == 'win32':
    sys.stdout.reconfigure(errors='replace')
    sys.stderr.reconfigure(errors='replace')


def hash_string(s):
    result = 0
    for c in s.encode('ascii'):
        shift = c & 7
        result = (result + c + ((result << shift) & 0xFFFFFFFF)) & 0xFFFFFFFF
    return result


def safe_magic_str(m):
    if all(32 <= b < 127 for b in m):
        return m.decode('ascii')
    return '0x' + m.hex()


def read_u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


# ================================================================
# STEP 1: Load all WAD hashes
# ================================================================
print("=" * 70)
print("STEP 1: Collecting hashes from all .zwd files")
print("=" * 70)

all_hashes = set()
hash_to_wads = defaultdict(set)
hash_to_type = {}
type_hash_to_magics = defaultdict(set)
all_type_hashes = set()
wad_count = 0

for wad_dir in ['h:/Games/.Archive/SPIDEWICK/ww/Wads/',
                'h:/Games/.Archive/SPIDEWICK/na/Wads/',
                'h:/Games/.Archive/SPIDEWICK/us/Wads/']:
    if not os.path.isdir(wad_dir):
        continue
    region = Path(wad_dir).parent.name
    for f in sorted(Path(wad_dir).glob('*.zwd')):
        wname = f.stem
        with open(f, 'rb') as fh:
            magic = fh.read(4)
            csz, dsz = struct.unpack('<II', fh.read(8))
            data = zlib.decompress(fh.read())
        if data[:4] != b'AWAD':
            continue
        count = read_u32(data, 8)
        wad_count += 1
        for i in range(count):
            nh = read_u32(data, 12 + i * 8)
            ep = read_u32(data, 12 + i * 8 + 4)
            th = read_u32(data, ep)
            do = read_u32(data, ep + 4)
            magic4 = bytes(data[do:do + 4]) if do + 4 <= len(data) else b""
            all_hashes.add(nh)
            all_type_hashes.add(th)
            hash_to_wads[nh].add(f"{region}/{wname}")
            hash_to_type[nh] = th
            type_hash_to_magics[th].add(magic4[:4])

print(f"  Processed {wad_count} WAD files")
print(f"  Unique name_hashes: {len(all_hashes)}")
print(f"  Unique type_hashes: {len(all_type_hashes)}")

# ================================================================
# STEP 2: Collect ALL candidate strings
# ================================================================
print("\n" + "=" * 70)
print("STEP 2: Collecting candidate strings")
print("=" * 70)

candidates = set()

# 2a: All strings from exe
strings_file = "h:/Games/.Archive/SPIDEWICK/_Spiderwick_RE/strings_full.txt"
if os.path.isfile(strings_file):
    with open(strings_file, 'r', errors='replace') as f:
        for line in f:
            parts = line.strip().split('\t')
            if len(parts) >= 4:
                s = parts[3]
                if len(s) >= 2 and all(32 <= ord(c) < 127 for c in s):
                    candidates.add(s)
                    for m in re.findall(r'[A-Za-z_][A-Za-z0-9_]+', s):
                        candidates.add(m)
    print(f"  Exe strings: {len(candidates)} candidates")

# 2b: Text assets
for unpacked in ['h:/Games/.Archive/SPIDEWICK/ww_unpacked/',
                 'h:/Games/.Archive/SPIDEWICK/na_unpacked/',
                 'h:/Games/.Archive/SPIDEWICK/us_unpacked/']:
    if not os.path.isdir(unpacked):
        continue
    for root, dirs, files in os.walk(unpacked):
        for fn in files:
            if fn.endswith(('.enum.txt', '.device.txt', '.script.txt')):
                try:
                    with open(os.path.join(root, fn), 'r', errors='replace') as f:
                        text = f.read()
                    for m in re.findall(r'[A-Za-z_][A-Za-z0-9_]+', text):
                        candidates.add(m)
                except:
                    pass
        for d in dirs:
            candidates.add(d)

# 2c: WAD stem names
for wad_dir in ['h:/Games/.Archive/SPIDEWICK/ww/Wads/',
                'h:/Games/.Archive/SPIDEWICK/na/Wads/',
                'h:/Games/.Archive/SPIDEWICK/us/Wads/']:
    if os.path.isdir(wad_dir):
        for f in os.listdir(wad_dir):
            if f.endswith('.zwd'):
                candidates.add(f[:-4])

# 2d: Lowercase everything
lowered = {c.lower() for c in candidates}
candidates.update(lowered)

# 2e: CamelCase splitting
extra = set()
for s in list(candidates):
    parts = re.findall(r'[A-Z][a-z]+|[A-Z]+(?=[A-Z][a-z])|[A-Z]+$|[a-z]+', s)
    if len(parts) >= 2:
        extra.add(''.join(p.lower() for p in parts))
        extra.add('_'.join(p.lower() for p in parts))
candidates.update(extra)

print(f"  After CamelCase split + lowercase: {len(candidates)} candidates")

# 2f: Game-specific word combos
# Core vocabulary for brute-force
game_words = set()

# Characters/creatures
for w in ["jared", "simon", "mallory", "thimbletack", "mulgarath", "hogsqueal",
          "byron", "lucinda", "arthur", "spiderwick", "grace",
          "goblin", "sprite", "troll", "spriggan", "boggart", "brownie", "knocker",
          "griffin", "salamander", "phooka", "nixie", "sylph", "cockroach", "roach",
          "sprout", "mushroom", "stray", "sod", "wisp", "will", "willow", "fairy",
          "hobgoblin", "redcap", "mole", "rat", "bat", "bull", "boar",
          "firefly", "dragonfly", "beetle", "worm", "serpent"]:
    game_words.add(w)

# Items and objects
for w in ["sword", "sling", "slingshot", "net", "butterfly", "bag", "bomb", "tomato",
          "ball", "bearing", "gobstone", "tongs", "monocle", "guide", "book", "field",
          "stone", "rock", "tooth", "teeth", "fruit", "key", "potion",
          "oven", "dumbwaiter", "painting", "seeing", "sight", "jack", "swing",
          "torch", "lantern", "needle", "thread", "rope", "chain", "shield",
          "message", "fuse", "wick", "door", "gate", "bridge", "ladder", "wire",
          "mushroom", "ring", "compass", "map", "chest", "crate", "barrel",
          "flower", "herb", "berry", "seed", "leaf", "vine"]:
    game_words.add(w)

# UI/HUD elements
for w in ["health", "meter", "bar", "timer", "icon", "widget", "page", "menu",
          "button", "cursor", "arrow", "comp", "border", "frame", "panel",
          "bg", "background", "portrait", "thumbnail", "preview",
          "screen", "dialog", "popup", "card", "check", "mark", "radio",
          "slider", "scroll", "list", "text", "label", "header", "footer",
          "title", "logo", "splash", "loading", "load", "save", "fade",
          "hud", "overlay", "notification", "alert", "hint", "tip",
          "reticle", "crosshair", "target", "aim", "indicator", "counter",
          "progress", "score", "point", "scurry", "capture", "power", "motion",
          "letterbox", "notepage", "guidebook", "bookmark", "note",
          "thumbstick", "stick", "pad", "trigger", "bumper", "tab",
          "checkbox", "checkmark", "dropdown", "input", "output",
          "more", "less", "max", "min", "full", "empty", "half",
          "left", "right", "up", "down", "front", "back", "top", "bottom", "center",
          "start", "end", "begin", "finish", "open", "close", "on", "off",
          "main", "sub", "alt", "default", "custom", "base", "special", "extra",
          "glow", "flash", "shine", "sparkle", "trail",
          "idle", "active", "hover", "pressed", "disabled",
          "normal", "highlight", "selected", "focused"]:
    game_words.add(w)

# Locations
for w in ["mansion", "grounds", "forest", "road", "camp", "arena", "tunnel", "town",
          "deep", "wood", "woods", "meadow", "yard", "garage",
          "attic", "kitchen", "library", "basement", "garden", "porch", "cellar",
          "shell", "common", "chapter", "level", "world", "sector",
          "hallway", "corridor", "room"]:
    game_words.add(w)

# Systems/data
for w in ["camera", "anim", "animation", "blend", "transition", "loop",
          "audio", "sound", "music", "voice", "effect", "particle",
          "config", "settings", "options", "difficulty", "brightness", "contrast",
          "volume", "master", "sfx", "ambient", "speech",
          "db", "data", "table", "record", "type", "id", "name",
          "collision", "physics", "nav", "path", "waypoint",
          "trigger", "event", "action", "state", "behavior",
          "script", "system", "manager", "controller"]:
    game_words.add(w)

# Prefixes
for w in ["mt", "h", "v", "cl", "ui", "fx", "pfx", "gfx",
          "bg", "fg", "bk", "sf", "p1", "p2",
          "a", "b", "c", "d", "1", "2", "3", "4", "5", "6", "7", "8",
          "01", "02", "03", "04", "05"]:
    game_words.add(w)

# Misc
for w in ["bink", "rad", "video", "movie", "cinema", "cinematic",
          "font", "string", "texture", "material", "shader", "model", "mesh",
          "cooperative", "versus", "multiplayer", "singleplayer",
          "credits", "trailer", "extras", "bonus", "pause", "resume", "quit",
          "confirm", "cancel", "accept", "selection", "chosen",
          "day", "night", "red", "green", "blue", "white", "black", "gray",
          "small", "medium", "large", "big", "little", "mini", "tiny", "huge",
          "net", "test", "debug", "info", "error", "warning",
          "interface", "subtitles", "level", "common"]:
    game_words.add(w)

game_words_list = sorted(game_words)
print(f"  Word list for combos: {len(game_words_list)} words")

# Try 2-word combinations (joined + underscore)
combo_candidates = set()
for w1 in game_words_list:
    for w2 in game_words_list:
        if w1 == w2:
            continue
        combo_candidates.add(w1 + w2)
        combo_candidates.add(w1 + '_' + w2)

print(f"  2-word combos: {len(combo_candidates)}")

# Try 3-word combos with restricted prefix/suffix sets
prefixes_3w = ['mt', 'h', 'v', 'ui', 'hud', 'fx', 'bg', 'cursor', 'scroll',
               'field', 'health', 'arena', 'portrait', 'tomato', 'oven', 'jack',
               'fairy', 'stray', 'ball', 'goblin', 'sprite', 'quest', 'level',
               'main', 'mini', 'game', 'pause', 'chapter', 'collection', 'control',
               'family', 'player', 'dumbwaiter', 'letter', 'check', 'thumb',
               'inventory', 'objective', 'subtitle', 'message', 'alert', 'hint',
               'book', 'guide', 'note', 'pickup', 'attract', 'cinematic',
               'multiplayer', 'cooperative', 'weapon', 'projectile',
               'damage', 'combat', 'seeing', 'butterfly', 'stopwatch',
               'scurry', 'gobstone', 'capture', 'power', 'motion', 'timer',
               'auto', 'adaptive', 'audio', 'voice', 'sound', 'music',
               'fire', 'mole', 'stray', 'bull', 'river']
suffixes_3w = ['icon', 'bar', 'timer', 'page', 'widget', 'meter', 'indicator',
               'comp', 'fuse', 'back', 'front', 'button', 'card',
               'menu', 'screen', 'dialog', 'popup', 'list', 'slot', 'counter',
               'portrait', 'name', 'label', 'text', 'desc', 'info',
               'db', 'data', 'table', 'layout', 'config',
               'arrow', 'cursor', 'more', 'left', 'right', 'up', 'down',
               'activate', 'activation', 'billboard', 'glow', 'flash',
               'fg', 'sprite', 'troll']

combo3_count = 0
for w1 in prefixes_3w:
    for w2 in game_words_list:
        if w1 == w2:
            continue
        for w3 in suffixes_3w:
            if w2 == w3 or w1 == w3:
                continue
            combo_candidates.add(w1 + w2 + w3)
            combo_candidates.add(w1 + '_' + w2 + '_' + w3)
            combo_candidates.add(w1 + '_' + w2 + w3)
            combo_candidates.add(w1 + w2 + '_' + w3)
            combo3_count += 4

print(f"  3-word combos: {combo3_count}")
candidates.update(combo_candidates)

# Filter to ASCII-only, 2+ chars
candidates = {c for c in candidates if len(c) >= 2 and all(32 <= ord(ch) < 127 for ch in c)}
print(f"  Final candidate count: {len(candidates)}")

# ================================================================
# STEP 3: Hash and match
# ================================================================
print("\n" + "=" * 70)
print("STEP 3: Hashing candidates and matching")
print("=" * 70)

matched = {}
hash_to_all = defaultdict(set)

for c in candidates:
    try:
        h = hash_string(c)
    except:
        continue
    if h in all_hashes:
        hash_to_all[h].add(c)
        if h not in matched:
            matched[h] = c
        else:
            existing = matched[h]
            # Scoring: prefer shorter, identifier-like, no spaces
            def score(s):
                sc = len(s)
                if ' ' in s:
                    sc += 50
                if any(not (c.isalnum() or c == '_') for c in s):
                    sc += 30
                # Prefer names that look like identifiers
                if s[0].isupper() or s.isupper():
                    sc -= 3
                if '_' in s:
                    sc -= 2
                # Prefer all-lowercase (that's the WAD convention)
                if s.islower() or all(c.islower() or c == '_' for c in s):
                    sc -= 5
                return sc
            if score(c) < score(existing):
                matched[h] = c

# Validate: remove suspicious matches (very short or gibberish)
suspicious = []
for h, name in list(matched.items()):
    # Remove very short (1 char) or names that are just numbers
    if len(name) <= 1 or name.isdigit():
        suspicious.append((h, name))
        del matched[h]

collision_count = sum(1 for h, names in hash_to_all.items() if len(names) > 1)
print(f"  Matched: {len(matched)} / {len(all_hashes)} ({100*len(matched)/len(all_hashes):.1f}%)")
print(f"  Unmatched: {len(all_hashes) - len(matched)}")
print(f"  Collisions: {collision_count}")
if suspicious:
    print(f"  Removed suspicious: {len(suspicious)}")

# Show collisions
if collision_count > 0:
    print("\n  Hash collisions (up to 30):")
    shown = 0
    for h, names in sorted(hash_to_all.items()):
        if len(names) > 1 and shown < 30:
            chosen = matched.get(h, "REMOVED")
            print(f"    0x{h:08X}: chosen={chosen!r}, all={names}")
            shown += 1

# ================================================================
# STEP 4: Build type dictionary
# ================================================================
print("\n" + "=" * 70)
print("STEP 4: Type hash dictionary")
print("=" * 70)

TYPE_NAMES = {}

# Try hashing known names
for name in ["PCIM", "DBDB", "STTL", "Device", "enum", "AMAP", "NM40",
             "adat", "aniz", "PCRD", "Script", "WF", "Waveform",
             "Texture", "Database", "Settings", "Animation", "Sound",
             "Image", "PCImage", "NormalMap", "AnimationMap",
             "pcim", "dbdb", "sttl", "device", "amap", "nm40",
             "pcrd", "script", "wf", "waveform",
             "texture", "database", "settings", "animation", "sound",
             "image", "model", "mesh", "skeleton", "material",
             "audio", "music", "voice", "sfx", "effect",
             "world", "level", "scene", "sector", "node",
             "collision", "navmesh", "config", "data", "table",
             "render", "display", "screen", "ui", "hud",
             "Mesh", "Skeleton", "Material", "Shader", "Particle",
             "World", "Level", "Scene", "Sector", "NavMesh",
             "String", "Font", "Binary", "Config",
             "PlayList", "Playlist", "playlist"]:
    h = hash_string(name)
    if h in all_type_hashes and h not in TYPE_NAMES:
        TYPE_NAMES[h] = name
        print(f"  Type match: 0x{h:08X} = {name!r}")

# Fallback: use magic bytes for unnamed types
for th, magics in type_hash_to_magics.items():
    if th in TYPE_NAMES:
        continue
    magic_list = list(magics)
    if len(magic_list) == 1:
        m = magic_list[0]
        if all(32 <= b < 127 for b in m):
            try:
                name = m.decode('ascii').strip()
                if all(c.isalnum() or c in ' _/' for c in name) and len(name) >= 2:
                    TYPE_NAMES[th] = name
            except:
                pass

print(f"\n  Known: {len(TYPE_NAMES)} / {len(all_type_hashes)} type hashes")
for th in sorted(all_type_hashes):
    name = TYPE_NAMES.get(th, "???")
    magics_str = ', '.join(safe_magic_str(m) for m in type_hash_to_magics.get(th, set()))
    print(f"    0x{th:08X}: {name:20s} [{magics_str}]")

# ================================================================
# STEP 5: Write asset_names.py
# ================================================================
print("\n" + "=" * 70)
print("STEP 5: Writing asset_names.py")
print("=" * 70)

output_path = "h:/Games/.Archive/SPIDEWICK/_Spiderwick_RE/tools/asset_names.py"

lines = []
lines.append('#!/usr/bin/env python3')
lines.append('"""')
lines.append('Spiderwick Chronicles - Asset Name Dictionary')
lines.append('=' * 46)
lines.append('Auto-generated hash -> name mapping for AWAD archive assets.')
lines.append('')
lines.append('Hash function:')
lines.append('  def hash_string(s):')
lines.append('      result = 0')
lines.append('      for c in s.encode("ascii"):')
lines.append('          shift = c & 7')
lines.append('          result = (result + c + ((result << shift) & 0xFFFFFFFF)) & 0xFFFFFFFF')
lines.append('      return result')
lines.append('')
lines.append(f'Sources: {wad_count} WAD files across ww/na/us regions')
lines.append(f'Total unique asset hashes: {len(all_hashes)}')
lines.append(f'Matched: {len(matched)} ({100*len(matched)/len(all_hashes):.1f}%)')
lines.append(f'Unmatched: {len(all_hashes) - len(matched)}')
lines.append('"""')
lines.append('')
lines.append('')
lines.append('def hash_string(s: str) -> int:')
lines.append('    """Compute Spiderwick asset name hash."""')
lines.append('    result = 0')
lines.append('    for c in s.encode("ascii"):')
lines.append('        shift = c & 7')
lines.append('        result = (result + c + ((result << shift) & 0xFFFFFFFF)) & 0xFFFFFFFF')
lines.append('    return result')
lines.append('')
lines.append('')

# Type hash dictionary
lines.append('# Type hash -> type name (identifies sub-asset format)')
lines.append('# Known from hash matching + magic byte identification')
lines.append('TYPE_NAMES = {')
for th in sorted(all_type_hashes):
    if th in TYPE_NAMES:
        name = TYPE_NAMES[th]
        magics_str = ', '.join(safe_magic_str(m) for m in type_hash_to_magics.get(th, set()))
        lines.append(f'    0x{th:08X}: {name!r},  # {magics_str}')
    else:
        magics_str = ', '.join(safe_magic_str(m) for m in type_hash_to_magics.get(th, set()))
        lines.append(f'    # 0x{th:08X}: ???,  # {magics_str}')
lines.append('}')
lines.append('')
lines.append('')

# Asset names
lines.append('# Asset name_hash -> name mapping')
lines.append('# Names are all lowercase (game convention for WAD asset lookups)')
lines.append('ASSET_NAMES = {')
for h in sorted(matched.keys()):
    name = matched[h]
    wads = sorted(hash_to_wads.get(h, set()))
    wad_str = ", ".join(wads[:3])
    if len(wads) > 3:
        wad_str += f", +{len(wads)-3} more"
    # Type info
    th = hash_to_type.get(h, 0)
    type_name = TYPE_NAMES.get(th, f"0x{th:08X}")
    # Collision note
    all_names = hash_to_all.get(h, set())
    collision_note = ""
    if len(all_names) > 1:
        others = sorted(all_names - {name})[:3]
        collision_note = f"  (also: {', '.join(others)})"
    lines.append(f'    0x{h:08X}: {name!r},  # {type_name} | {wad_str}{collision_note}')
lines.append('}')
lines.append('')
lines.append('')

# Unmatched hashes
lines.append('# Unmatched hashes (no name found yet):')
unmatched_list = sorted(all_hashes - set(matched.keys()))
for h in unmatched_list:
    wads = sorted(hash_to_wads.get(h, set()))
    wad_str = ", ".join(wads[:3])
    if len(wads) > 3:
        wad_str += f", +{len(wads)-3} more"
    th = hash_to_type.get(h, 0)
    type_name = TYPE_NAMES.get(th, f"0x{th:08X}")
    lines.append(f'# 0x{h:08X}  # {type_name} | {wad_str}')
lines.append('')
lines.append('')

# Reverse lookup + helpers
lines.append('# Reverse lookup: name -> hash')
lines.append('NAME_TO_HASH = {v: k for k, v in ASSET_NAMES.items()}')
lines.append('')
lines.append('')
lines.append('def lookup(name_hash: int) -> str:')
lines.append('    """Look up an asset name by its hash. Returns hex string if unknown."""')
lines.append('    return ASSET_NAMES.get(name_hash, f"0x{name_hash:08X}")')
lines.append('')
lines.append('')
lines.append('def lookup_type(type_hash: int) -> str:')
lines.append('    """Look up a type name by its hash. Returns hex string if unknown."""')
lines.append('    return TYPE_NAMES.get(type_hash, f"0x{type_hash:08X}")')
lines.append('')

with open(output_path, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(lines))

print(f"  Written to: {output_path}")
print(f"  File size: {os.path.getsize(output_path):,} bytes")

# ================================================================
# SUMMARY
# ================================================================
print("\n" + "=" * 70)
print("SUMMARY")
print("=" * 70)
print(f"  WAD files processed:  {wad_count}")
print(f"  Total unique hashes:  {len(all_hashes)}")
print(f"  Candidates tested:    {len(candidates):,}")
print(f"  Matched:              {len(matched)} ({100*len(matched)/len(all_hashes):.1f}%)")
print(f"  Unmatched:            {len(all_hashes) - len(matched)}")
print(f"  Type hashes known:    {len(TYPE_NAMES)} / {len(all_type_hashes)}")

# Unmatched by type
type_counter = Counter()
for h in (all_hashes - set(matched.keys())):
    type_counter[hash_to_type.get(h, 0)] += 1
print("\n  Unmatched by type:")
for th, count in type_counter.most_common():
    name = TYPE_NAMES.get(th, f'0x{th:08X}')
    print(f"    {name:20s}: {count}")

# All matches
print("\n  All matched names:")
for h in sorted(matched.keys()):
    name = matched[h]
    print(f"    0x{h:08X} = {name!r}")
