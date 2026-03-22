# Camera Settings Structure
**Loader function:** `sub_426450` (Spiderwick.exe+26450)
**Settings object obtained via:** `sub_4263B0(a1)` → returns settings pointer (`v5`)

## Offset Map

### Relative Position
```
+0x54  RELATIVEPOSITION_LOCALOFFSET_X      (float)
+0x58  RELATIVEPOSITION_LOCALOFFSET_Y      (float)
+0x5C  RELATIVEPOSITION_LOCALOFFSET_Z      (float)
+0x60  (byte) auto-set to 1 if ATTACH_TO_BONE != 0
+0x61  RELATIVEPOSITION_IS_OFFSET_IN_WORLD_SPACE  (bool)
+0x62  RELATIVEPOSITION_IS_AXES_WORLD_ALIGNED     (bool)
+0x64  RELATIVEPOSITION_ATTACH_TO_BONE            (int/ptr)
```

### Character Offset (3rd person camera positioning)
```
+0x68  CHAROFFSET_DISTFROMSUBJECT              (float) — distance from character
+0x6C  CHAROFFSET_HEIGHTABOVESUBJECT           (float) — height above character
+0x70  CHAROFFSET_ROTATIONSPEED                (float)
+0x74  CHAROFFSET_LOOKATOFFSETX                (float) — look-at offset
+0x78  CHAROFFSET_LOOKATOFFSETY                (float)
+0x7C  CHAROFFSET_LOOKATOFFSETZ                (float)
+0x80  CHAROFFSET_CAMERABEHINDSUBJECTBIAS      (float)
+0x84  CHAROFFSET_INVERSE_ROTATE               (bool, bit)
+0x84  CHAROFFSET_NO_DRIFT_SAFETY_ZONE         (bool, bit) — same offset, bit flags
```

### Lerp / Interpolation
```
+0x8C  LERP_VELOCITY                  (float) — camera movement speed
+0x90  LERP_MAXVELOCITY               (float) — max camera speed
+0x94  LERP_ROTATIONVELOCITY          (float) — rotation interpolation speed
+0x98  LERP_ACCELERATION              (float) — camera acceleration
+0x9C  LERP_EASEIN_DISTANCE           (float) — ease-in start distance
+0xA0  LERP_EASEIN_SLOWDOWNEXPONENT   (int)   — ease-in curve exponent
```

### Standard 3rd Person Camera
```
+0xBC  STANDARD3RDPERSON_HEIGHT                   (float) — default height
+0xC0  STANDARD3RDPERSON_HEIGHT_MIN               (float) — min height clamp
+0xC4  STANDARD3RDPERSON_HEIGHT_MAX               (float) — max height clamp
+0xC8  STANDARD3RDPERSON_OUTER_RADIUS              (float) — outer orbit radius
+0xCC  STANDARD3RDPERSON_INNER_RADIUS              (float) — inner orbit radius
+0xD0  STANDARD3RDPERSON_MAX_ROTATION_SPEED        (float) — max orbit rotation speed
+0xD4  STANDARD3RDPERSON_MAX_FOLLOW_SPEED          (float) — max follow speed
+0xD8  STANDARD3RDPERSON_ROTATION_ACCEL            (float) — rotation acceleration
+0xDC  STANDARD3RDPERSON_ROTATION_DECEL            (float) — rotation deceleration
+0xE0  STANDARD3RDPERSON_COLLISION_RADIUS           (float) — collision detection radius
+0xE4  STANDARD3RDPERSON_MAX_ALLOWABLE_DISTANCE    (float) — max camera distance from player
+0xE8  STANDARD3RDPERSON_DAMPEN                    (float) — damping factor
+0xEC  STANDARD3RDPERSON_LERP_FIRST_FRAME          (bool)  — lerp on first frame
```

### Camera Settings (FOV / Clipping)
```
+0x44  CAMERASETTINGS_FOV                  (float) — field of view
+0x48  CAMERASETTINGS_FOV_WS               (float) — FOV widescreen
+0x50  CAMERASETTINGS_FARPLANE             (float) — far clipping plane distance
```

### Camera Subject Alpha
```
+0x??  CAMSUBJECTALPHA_ALPHA_IN            (float)
+0x??  CAMSUBJECTALPHA_ALPHA_OUT           (float)
+0x??  CAMSUBJECTALPHA_SMALLEST_ALLOWED_ALPHA (float)
```

### World Collision
```
+0x??  WORLDCOLLISION_SWEPTSPEHERERADIUS    (float) — collision sphere radius
+0x??  WORLDCOLLISION_RESOLVE_FIRST_PERSON  (?)
```

### Additional Lerp
```
+0x??  LERP_FORCE_LERP_LOOKAT              (?)
+0x??  LERP_LERPFIRSTFRAME                 (?)
+0x??  LERP_LERPORIENT...                  (?)
```

## Class: ClCameraDB
```
RTTI:   .?AVClCameraDB@@  at 006238FE
vtable: 00623904
  [0] = sub_426450  (settings loader, this function)
  [1] = byte_427D70

String "CameraDB" at 0062390C
```

## Settings Block Structure
- Each settings block is **0x50C bytes**
- Multiple blocks processed in a loop (one per camera mode?)
- `add ebx, 50Ch` advances to next block

## Loader Functions
```
sub_428130(name, data, count, dest)  — load float setting
sub_428160(name, data, count, dest)  — load bool setting
sub_4280C0(name, data, count, dest)  — load int/string setting
sub_4281A0(a1, name)                 — load named component
```

## Notes
- Settings loaded from data file/asset at initialization by ClCameraDB
- `v5` = camera settings object, obtained from `sub_4263B0(camera_component)`
- Settings block size = 0x50C bytes, may have multiple blocks
- FARPLANE at +0x50 controls far clipping plane — key for room clipping fix
- FOV at +0x44 — can be modified for FOV control feature

## Related
- [camera_struct.md](camera_struct.md) — runtime camera state
- [../subs/sub_43E2B0_MainCameraComponent.md](../subs/sub_43E2B0_MainCameraComponent.md) — uses these settings at runtime
- [../ROOM_CLIPPING.md](../ROOM_CLIPPING.md) — room clipping investigation
