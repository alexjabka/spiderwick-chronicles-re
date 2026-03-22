# sub_436190 — Camera Pipeline / Component Manager
**Address:** `Spiderwick.exe+36190` (absolute: `00436190`)

## Purpose
The main camera update function. Processes camera components (orbit, collision,
monocle, etc.), computes final eye position and look direction, then calls
sub_5299A0 to build the view matrix.

## Signature
```c
int __thiscall sub_436190(_DWORD *this, float deltaTime, int cameraState, int a4)
```

## How Eye/Target Are Computed
```c
// After component processing:
direction = (v20[8], v20[9], v20[10])  // from camera component pipeline
eye = (v21, v22, v23)                   // camera position

target.x = eye.x + direction.x
target.y = eye.y + direction.y
target.z = eye.z + direction.z

sub_5299A0(eye, target)                 // → LookAt builds view matrix
```

## Component System
```c
// this[0x22] (offset +0x88) = array of component pointers
// this[0x32] (offset +0xC8) = component count

for each component:
    vtable[0x2C/4](deltaTime)         // update with delta time
    vtable[0x0C/4](cameraState, buf)  // process camera state
    vtable[0x10/4]()                  // finalize
```

Components are processed in order. Each one modifies the camera state
(position, direction). The final state is used for the view matrix.

Mouse input for camera rotation is processed by one of these components.
The yaw/pitch values are stored in the component objects.

## Object Relationship
```
Camera manager object (this in sub_436190)
  = pCamStruct - 0x480
  +0x88: component[0] pointer
  +0x8C: component[1] pointer
  ...
  +0xC8: component count

pCamStruct (view matrix destination)
  = camera manager + 0x480
  +0x00..+0x3C: view matrix (written by sub_5AA1B0)
  +0x3B8..+0x3C0: position
```

## Call Chain
```
sub_436190 (camera pipeline manager)
  ├── component virtual calls (update camera state from input)
  ├── sub_5A7DC0 (copy rotation data)
  ├── sub_5A8530 (process transformation)
  ├── sub_4356F0 (copy state to camera struct) ← HOOKED
  └── sub_5299A0 (build view matrix) ← HOOKED
      └── sub_5AA1B0 (LookAt)
```

## Status: ANALYZED (not hooked)
Yaw/pitch source is in the camera components. Use `find_yaw.lua` to locate.

## Related
- Calls [sub_5299A0](sub_5299A0_ViewMatrixWrapper.md) at offset +1A4
- Calls [sub_4356F0](sub_4356F0_PositionWriter.md) to copy camera state
- Calls [sub_5A7DC0](sub_5A7DC0_Memcpy16Floats.md) for matrix copy
- Components contain yaw/pitch from mouse input (TODO: find exact offsets)
