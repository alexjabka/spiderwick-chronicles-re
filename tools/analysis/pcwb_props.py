#!/usr/bin/env python3
"""Analyze prop instance data in PCWB header to find transforms and PCRD links."""
import struct, sys, math

def r32(d, o): return struct.unpack_from("<I", d, o)[0]
def rf32(d, o): return struct.unpack_from("<f", d, o)[0]

def read_string(data, off):
    """Read null-terminated ASCII string."""
    end = off
    while end < len(data) and data[end] != 0:
        end += 1
    return data[off:end].decode('ascii', errors='replace')

def is_matrix(data, off):
    """Check if 64 bytes at off look like a 4x4 transform matrix."""
    if off + 64 > len(data): return False
    vals = [rf32(data, off + i*4) for i in range(16)]
    # A valid transform matrix should have reasonable values
    # and the last row should be close to (0, 0, 0, 1) or (x, y, z, 1)
    if abs(vals[15] - 1.0) > 0.01: return False
    # Check that at least some non-zero rotation/scale values exist
    has_nonzero = any(abs(v) > 0.0001 for v in vals[:12])
    return has_nonzero

def main(path):
    with open(path, "rb") as f:
        data = f.read()

    # Find first PCRD to know header boundary
    first_pcrd = len(data)
    pos = 0
    while pos < len(data) - 4:
        if data[pos:pos+4] == b"PCRD" and r32(data, pos+4) == 2:
            first_pcrd = pos; break
        pos += 4

    # Header offset +0x98 points to prop instance region?
    # Let's scan the header for prop-like structures
    # Props have: name string, transform matrix, possibly PCRD refs

    # First find all readable prop names
    print(f"Header region: 0x0 - 0x{first_pcrd:X}")

    # Scan for "Prop" strings
    props = []
    i = 0
    while i < first_pcrd - 4:
        # Look for prop-related string patterns
        s = read_string(data, i)
        if len(s) >= 4 and (s.startswith("Prop") or s.startswith("prop") or
                           s.startswith("PROP") or s.startswith("_SFS")):
            props.append((i, s))
            i += len(s) + 1
        else:
            i += 1

    print(f"Found {len(props)} prop-like strings")

    # For each prop name, look for transform matrix nearby
    for name_off, name in props[:20]:
        print(f"\n=== {name} @0x{name_off:06X} ===")

        # Scan backwards for a pointer to this name
        # Prop structures likely have a pointer to name, then matrix data
        # Or the name is at the START of the structure

        # Look at data around the name
        # Before the name: check if there's a structure header
        pre_start = max(0, name_off - 64)
        post_end = min(first_pcrd, name_off + len(name) + 256)

        # Dump what's after the name (likely: padding, then matrix)
        after_name = name_off + len(name) + 1
        # Align to 4
        after_aligned = (after_name + 3) & ~3

        # Look for a 4x4 matrix after the name
        found_matrix = False
        for scan in range(after_aligned, min(after_aligned + 64, post_end), 4):
            if is_matrix(data, scan):
                # Found it - dump the matrix
                m = [rf32(data, scan + j*4) for j in range(16)]
                # Extract position from last row/column
                # Row-major 4x4: position is at m[12], m[13], m[14]
                # Or m[3], m[7], m[11] depending on convention
                pos_a = (m[12], m[13], m[14])  # row-major position
                pos_b = (m[3], m[7], m[11])    # column-major position

                print(f"  Matrix @0x{scan:06X}:")
                for row in range(4):
                    vals = [f"{m[row*4+c]:10.3f}" for c in range(4)]
                    print(f"    [{' '.join(vals)}]")
                print(f"  Position (row-major m[12-14]): ({pos_a[0]:.1f}, {pos_a[1]:.1f}, {pos_a[2]:.1f})")
                print(f"  Position (col-major m[3,7,11]): ({pos_b[0]:.1f}, {pos_b[1]:.1f}, {pos_b[2]:.1f})")

                # Check what's between name and matrix
                gap = scan - after_name
                if gap > 0:
                    gap_vals = [r32(data, after_aligned + j) for j in range(0, min(gap, 32), 4)]
                    print(f"  Gap ({gap} bytes): {' '.join(f'{v:08X}' for v in gap_vals)}")

                # Check what's AFTER the matrix (might have PCRD refs)
                after_mat = scan + 64
                print(f"  After matrix:")
                for k in range(8):
                    if after_mat + k*4 + 4 <= len(data):
                        v = r32(data, after_mat + k*4)
                        print(f"    +{k*4:2d}: 0x{v:08X} ({v})")

                found_matrix = True
                break

        if not found_matrix:
            # Dump raw data after name
            print(f"  No matrix found. Raw after name:")
            for k in range(16):
                off = after_aligned + k * 4
                if off + 4 <= len(data):
                    v = r32(data, off)
                    fv = rf32(data, off)
                    print(f"    +0x{k*4:02X}: 0x{v:08X} (f={fv:.3f})")

    # Also check the table at header +0x9C which had structured data
    print(f"\n\n=== Table at header +0x9C ===")
    table_off = r32(data, 0x9C)
    if 0 < table_off < first_pcrd:
        # Earlier we saw entries like: {0, 1, 2, offset, big_val, ...}
        # Stride seems to be ~24 bytes?
        # Let me dump entries and look for pattern
        for i in range(20):
            base = table_off + i * 24
            if base + 24 > first_pcrd: break
            vals = [r32(data, base + j) for j in range(0, 24, 4)]
            print(f"  [{i:3d}] {' '.join(f'{v:08X}' for v in vals)}")

main(sys.argv[1])
