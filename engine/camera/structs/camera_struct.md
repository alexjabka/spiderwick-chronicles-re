# Camera Struct (pCamStruct)
**Pointer symbol:** `pCamStruct`
**Capture point:** `Spiderwick.exe+1AA333` (captures ESI register)
**Example base:** `0x21490258`

## How to obtain
Script "Camera Struct" injects at Spiderwick.exe+1AA333:
```
Original: fstp dword ptr [esi+38] / fld1
```
Inside a dot product computation (-dot(axis, position)) for view matrix
translation row. Stores ESI into `pCamStruct`.

## Offset Map

### Rotation/View Matrix (+0x00 to +0x2C)
4x3 row-major matrix, 12 floats:
```
+0x00  R00 (cos yaw)       +0x04  R01              +0x08  R02              +0x0C  pad (0)
+0x10  R10 (sin yaw)       +0x14  R11              +0x18  R12              +0x1C  pad (0)
+0x20  R20 (0)             +0x24  R21 (sin pitch)  +0x28  R22 (cos pitch)  +0x2C  pad (0)
```

Rotation matrix is constructed from yaw/pitch as:
```
Row 0: [ cos(yaw),  -sin(yaw)*cos(pitch),  sin(yaw)*sin(pitch),  0 ]
Row 1: [ sin(yaw),   cos(yaw)*cos(pitch), -cos(yaw)*sin(pitch),  0 ]
Row 2: [ 0,          sin(pitch),            cos(pitch),           0 ]
```

Angle extraction:
```lua
yaw   = atan2(R10, R00) = atan2([+0x10], [+0x00])
pitch = atan2(R21, R22) = atan2([+0x24], [+0x28])
```

### View Matrix Translation (+0x30 to +0x38)
```
+0x30  tx (Float, ~63.25)     // -dot(right, position)
+0x34  ty (Float, ~128.02)    // -dot(forward, position)
+0x38  tz (Float, ~-16.93)    // -dot(up, position)
```
These are NOT simple euler angles. They are computed as dot products
of rotation axes with camera position (standard view matrix construction).

### Other Parameters
```
+0x2C  Camera Clipping (Float, value: 0)
+0x394 Camera identifier? (DWORD, value: 1 when this is camera)
```

### Position (+0x3B8 to +0x3C0)
```
+0x3B8  Camera X (Float) — CONFIRMED writable
+0x3BC  Camera Y (Float) — CONFIRMED writable
+0x3C0  Camera Z (Float) — Z is vertical axis
```

## Matrix Layout (from sub_5AA1B0 LookAt analysis)
The view matrix is stored column-major:
```
+0x00 right.x    +0x04 forward.x    +0x08 up.x    +0x0C  0
+0x10 right.y    +0x14 forward.y    +0x18 up.y    +0x1C  0
+0x20 right.z    +0x24 forward.z    +0x28 up.z    +0x2C  0
+0x30 tx         +0x34 ty           +0x38 tz      +0x3C  1.0
```
Built by sub_5AA1B0(eye, target, up={0,0,1}) via sub_5299A0 wrapper.
ECX at sub_5299A0 + 0x480 = destination for this matrix.

## What Writes to This Struct

| Offset Range | Writer | Status (v10) |
|-------------|--------|--------|
| +0x00..+0x3C | sub_5299A0 → sub_5AA1B0 (LookAt) | HOOKED — args replaced with our eye/target |
| +0x00..+0x3C | sub_5A7DC0 (generic memcpy) | Allowed through (source is what LookAt wrote) |
| +0x388..+0x3C4 | sub_4356F0 → sub_5A7DC0 (position copy) | HOOKED — skipped for camera |
| Various | Camera pipeline (+3E547 block) | PATCHED — always skipped |

## Related Structs

### pCamCollide (collision camera)
Captured at `Spiderwick.exe+3D921` (EDI register).
```
+0x30  Camera X Collide
+0x34  Camera Y Collide
+0x38  Camera Z Collide
```

### pAnotherCamera (monocle camera)
Captured at `Spiderwick.exe+3DB0E`.
Read by monocle instruction. Separate struct used during monocle gameplay.
Monocle camera rotation drives player's forward direction.

## Notes
- Room loading/unloading is tied to camera position. Moving freecam far
  from player causes player's area to unload.
- struct is at least 0x3C4 bytes (position Z at +0x3C0 + 4 bytes)
