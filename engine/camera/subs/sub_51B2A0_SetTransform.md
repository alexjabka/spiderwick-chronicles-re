# sub_51B2A0 — SetTransform
**Address:** Spiderwick.exe+11B2A0 (absolute: 0x51B2A0)
**Status:** FULLY REVERSED

## Purpose
Generic transform copy: copies rotation matrix + position from source to destination object.
Called for ALL game entities (~6000/frame).

## Signature
void __thiscall SetTransform(void *this, Transform *src)

## Pseudocode
void SetTransform(void *this, Transform *src) {
    Memcpy16Floats(this+0x28, src);     // copy rotation matrix
    (this+0x28)->pos_x = src->pos_x;    // [this+0x68] = [src+0x30]
    (this+0x28)->pos_y = src->pos_y;    // [this+0x6C] = [src+0x34]
    (this+0x28)->pos_z = src->pos_z;    // [this+0x70] = [src+0x38]
    // ... also copies additional data via sub_5A81D0
}

## Layout
Destination (this+0x28):
- +0x00 to +0x3F: rotation matrix (16 floats via Memcpy16Floats)
- +0x40 (this+0x68): position X
- +0x44 (this+0x6C): position Y
- +0x48 (this+0x70): position Z
- +0x50 (this+0x78): additional data from sub_5A81D0

Source (arg_0):
- +0x00 to +0x2F: rotation matrix
- +0x30: position X
- +0x34: position Y
- +0x38: position Z

## Key Instruction
0051B2CC: fstp [esi+44h] — the position Y writer (only instruction that writes player position, ~6000 hits/frame)

## Finding
Player object has DIFFERENT addresses indoor vs outdoor.
The game creates separate player objects for each scene.
