# Player Struct (pPlayerAddy)
**Pointer symbol:** `pPlayerAddy`
**Capture point:** `Spiderwick.exe+87DF6` (captures EDI register)

## Offset Map
```
+0x58   X - Player Model
+0x5C   Y - Player Model
+0x60   Z - Player Model

+0x68   Player X (Outside) — used in distance calculations (fsub operations)
+0x6C   Player Y (Outside)
+0x70   Player Z (Outside)

+0x678  Another X
+0x67C  Another Y
+0x680  Another Z
```

## Notes
- Offsets +68/+6C/+70 used in fsub operations for distance/collision checks
- The "Outside" positions may represent world-space coordinates used for
  room loading distance calculations
- Multiple position sets suggest different coordinate spaces or update phases

## Model Fading
**Pointer symbol:** `pModelFade`
**Capture point:** `Spiderwick.exe+170510` (captures EAX register)

Fade disable patch at `Spiderwick.exe+151D73`:
```
Original: 74 06  (je +151D7B)
Patched:  EB 06  (jmp +151D7B)
```
Prevents model fade value write → character stays visible at any distance.
Useful for freecam when camera moves far from player.

## Related
- Camera reads player position in the update pipeline (see [camera_pipeline_3E547](../subs/camera_pipeline_3E547.md))
- sub_452AD0 = dying function
