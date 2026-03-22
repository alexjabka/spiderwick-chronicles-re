# ============================================================================
# SpiderView — NM40 Texture Mapping Capture Script for IDA Pro
# ============================================================================
# Captures character descriptor arrays from ClNoamActor_Init breakpoints.
# Matches NM40 and PCIM data pointers to AWAD nameHashes by fingerprinting.
#
# Usage:
#   1. Open Spiderwick.exe in IDA with the debugger
#   2. Start debugging (F9), pass Kallis exceptions
#   3. Once the game is at the main menu, run this script (Alt+F7)
#   4. The script sets a breakpoint and auto-captures
#   5. In the game: start a new game, play through levels
#   6. When done, type capture_save() in IDA's Python console
#
# Output: nm40_texmap_captured.json in the game directory
# ============================================================================

import idc
import struct
import json
import os

# Global capture storage
_captures = []
_seen_nm40 = set()

def _read_desc(a2_ptr):
    """Read descriptor array and extract NM40 + PCIM info."""
    result = {}
    for slot in [0, 1, 2, 3, 4, 13, 14]:
        handle = idc.get_wide_dword(a2_ptr + slot * 4)
        if not handle or handle < 0x10000:
            continue
        data = idc.get_wide_dword(handle + 4)
        if not data or data < 0x10000:
            continue
        magic = idc.get_wide_dword(data)
        if magic == 0x30344D4E:  # NM40
            bones = idc.get_wide_word(data + 8)
            skel = idc.get_wide_word(data + 0x0A)
            bpal = idc.get_wide_dword(data + 0x38)
            bpal_rel = bpal - data if bpal > data else bpal
            # Fingerprint: first 8 non-fixup bytes after header
            fp = []
            for fi in [0x08, 0x0A, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x26]:
                fp.append(idc.get_wide_word(data + fi))
            result["desc%d" % slot] = {
                "type": "NM40", "bones": bones, "skel": skel,
                "bonePalOff": bpal_rel, "addr": data,
                "fingerprint": fp
            }
        elif magic == 0x4D494350:  # PCIM
            w = idc.get_wide_dword(data + 0x9C)
            h = idc.get_wide_dword(data + 0xA0)
            # Fingerprint: dimensions + first 4 bytes of DDS header
            dds_off = data + 0xC1
            dds_b = [idc.get_wide_byte(dds_off + i) for i in range(8)]
            result["desc%d" % slot] = {
                "type": "PCIM", "w": w, "h": h, "addr": data,
                "fingerprint": [w, h] + dds_b
            }
    return result

def capture_one():
    """Capture current breakpoint's character descriptor."""
    eip = idc.get_reg_value("EIP")
    if eip != 0x527EC0:
        print("[Capture] Not at ClNoamActor_Init (EIP=0x%X)" % eip)
        return False

    esp = idc.get_reg_value("ESP")
    a2 = idc.get_wide_dword(esp + 4)
    desc = _read_desc(a2)

    if "desc1" not in desc:
        return False

    nm40 = desc["desc1"]
    addr = nm40["addr"]
    if addr in _seen_nm40:
        return False  # already captured
    _seen_nm40.add(addr)

    bones = nm40["bones"]
    diff = desc.get("desc2", {})
    det = desc.get("desc4", {})

    entry = {
        "bones": bones,
        "nm40_fingerprint": nm40["fingerprint"],
        "nm40_bonePalOff": nm40["bonePalOff"],
    }
    if diff:
        entry["diffuse_w"] = diff.get("w", 0)
        entry["diffuse_h"] = diff.get("h", 0)
        entry["diffuse_fingerprint"] = diff.get("fingerprint", [])
    if det:
        entry["detail_w"] = det.get("w", 0)
        entry["detail_h"] = det.get("h", 0)
        entry["detail_fingerprint"] = det.get("fingerprint", [])

    _captures.append(entry)
    tag = "PLAYER" if bones >= 40 else "prop"
    print("[Capture] #%d: %s %d bones, diffuse=%s detail=%s" % (
        len(_captures), tag, bones,
        "%dx%d" % (diff["w"], diff["h"]) if diff else "none",
        "%dx%d" % (det["w"], det["h"]) if det else "none"))
    return True

def capture_save():
    """Save all captures to JSON file."""
    path = os.path.join(idc.get_input_file_path().rsplit("\\",1)[0], "nm40_captures.json")
    with open(path, "w") as f:
        json.dump({"captures": _captures}, f, indent=2)
    print("[Capture] Saved %d captures to %s" % (len(_captures), path))
    # Also print summary
    players = [c for c in _captures if c["bones"] >= 40]
    print("[Capture] Player characters: %d" % len(players))
    for p in players:
        print("  %d bones, diffuse=%dx%d" % (
            p["bones"], p.get("diffuse_w", 0), p.get("diffuse_h", 0)))

# Set up breakpoint
idc.add_bpt(0x527EC0)
idc.enable_bpt(0x527EC0, True)
print("=" * 60)
print("SpiderView Texture Mapping Capture")
print("=" * 60)
print("Breakpoint set on ClNoamActor_Init (0x527EC0)")
print("")
print("Now:")
print("  1. Press F9 to continue the game")
print("  2. Start a new game or load different chapters")
print("  3. Each character spawn is auto-captured")
print("  4. When done, type: capture_save()")
print("")
print("Captures so far: %d" % len(_captures))
