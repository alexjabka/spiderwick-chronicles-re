# Debug Camera — Archaeological Findings

## Status: EXISTS IN CODE, NOT INSTANTIATED IN RELEASE

The game contains a complete `ClDebugCameraManager` implementation with
free-fly camera, rotation, movement, and player teleport. However, the
object is never created in the release build (constructor has 0 xrefs).

## Architecture

```
ClDebugCameraManager object (0x80 bytes)
  ├── +0x00  Base class vtable (0x627F1C) — set by kallis constructor
  ├── +0x08  Registration pointer
  ├── +0x10  Flags byte (bit 2 = active)
  ├── +0x14  float pitch (from DEBUG_CAMERA_PITCH input)
  ├── +0x18  float yaw (from DEBUG_CAMERA_YAW input)
  ├── +0x1C  float forward (from DEBUG_CAMERA_FORWARD input)
  ├── +0x20  float right (from DEBUG_CAMERA_RIGHT input)
  ├── +0x24  float up (from DEBUG_CAMERA_UP input)
  ├── +0x28  byte teleport flag (from DEBUG_CAMERA_TELEPORT_SUBJECT)
  ├── +0x30  MethodValidator vtable (0x6282A0)
  ├── +0x38  Self pointer
  ├── +0x40  Validator callback → sub_440630
  ├── +0x50  MethodListener vtable (0x6282A8)
  ├── +0x58  Self pointer
  ├── +0x60  Input handler → sub_440790
  └── +0x70  State: -1=inactive, >=0=active (controller ID)
```

## Base Class Vtable (0x627F1C)

| Index | Address | Name | Description |
|-------|---------|------|-------------|
| [0] | 0x435490 | nullsub | Empty/destructor |
| [1] | 0x43C4C0 | **Activate** | Sets flag bit 2, zeros movement |
| [2] | 0x43C450 | **Deactivate** | Clears flag bit 2 |
| [3] | 0x43C4E0 | **CameraUpdate** | Full free-camera: rotation + translation + teleport |

## Camera Update (sub_43C4E0) — The Core

Decompiled pseudocode:
```c
void CameraUpdate(DebugCamObj* this, float* inputMatrix, CamOutput* output, float yawSpeed) {
    float pitch = abs(this->pitch);     // +0x14
    float yaw = abs(this->yaw);         // +0x18
    float fwd = abs(this->forward);     // +0x1C
    float right = abs(this->right);     // +0x20
    float up = abs(this->up);           // +0x24

    if (pitch + yaw + fwd + right + up > 0.001) {
        // Get current rotation from inputMatrix
        vec3 rot = { inputMatrix[4], inputMatrix[5], inputMatrix[6] };
        normalize(&rot);

        // Apply pitch/yaw rotation with delta time
        float dt = GetDeltaTime();
        newYaw = rot + this->yaw * dt;
        newPitch = this->pitch * dt + yawSpeed;

        // Build new rotation matrix
        Identity(output);
        SetRotation(output, newPitch, newYaw);

        // Apply translation
        vec3 movement = { this->right * dt, this->forward * dt, this->up * dt };
        vec3 worldMovement = TransformByRotation(rotation, movement);
        vec3 newPos = { inputMatrix[12] + worldMovement.x,
                        inputMatrix[13] + worldMovement.y,
                        inputMatrix[14] + worldMovement.z };
        SetPosition(output, newPos);
    } else {
        CopyMatrix(output, inputMatrix);  // no movement, keep current
    }

    // Teleport player to camera
    if (this->teleportFlag) {
        this->teleportFlag = 0;
        vec3 camPos = GetCameraForward() * 20.0 + GetCameraPosition();
        for (player = GetPlayerCharacter(); player; player = NextPlayer(player)) {
            player->SetTransform(camPos);
        }
    }
}
```

## Input System

| Input Action | Object Offset | Type |
|-------------|--------------|------|
| `DEBUG_CAMERA_ACTIVATE` | +0x70 (state toggle) | Digital |
| `DEBUG_CAMERA_PITCH` | +0x14 | Analog |
| `DEBUG_CAMERA_YAW` | +0x18 | Analog |
| `DEBUG_CAMERA_FORWARD` | +0x1C | Analog |
| `DEBUG_CAMERA_RIGHT` | +0x20 | Analog |
| `DEBUG_CAMERA_LEFT` | (negated right) | Analog |
| `DEBUG_CAMERA_UP` | +0x24 | Analog |
| `DEBUG_CAMERA_TELEPORT_SUBJECT` | +0x28 | Digital |

Input handler: sub_440790 checks `DEBUG_CAMERA_ACTIVATE`, toggles between
sub_440660 (activate, kallis) and sub_440690 (deactivate, kallis).

Movement input reader: sub_43C740 reads analog values via sub_5522F0.

## Key Functions

| Address | Name | Description |
|---------|------|-------------|
| sub_440910 | Constructor | Creates object, registers with "Debug" input group |
| sub_440790 | InputHandler | MethodListener callback, checks ACTIVATE |
| sub_440630 | InputValidator | MethodValidator callback, intercepts normal input |
| sub_43C740 | ReadMovementInput | Reads DEBUG_CAMERA_* analog inputs |
| sub_43C4E0 | **CameraUpdate** | Applies movement to camera matrix |
| sub_440660 | Activate (kallis) | Registers with camera pipeline — CRASHES when called manually |
| sub_440690 | Deactivate (kallis) | Unregisters from camera pipeline |
| sub_43C4C0 | vtable Activate | Sets flag bit 2, zeros movement (native) |
| sub_43C450 | vtable Deactivate | Clears flag bit 2 (native) |

## Manual Instantiation (CE)

The object CAN be created manually:
```
1. Enable debugcam_create.cea (allocates memory + registers symbol)
2. Lua: executeCodeEx(0, 5000, getAddress("dbgCamInit"))
3. Object created at dbgCamObj, vtable verified at +0x30 = 0x6282A0
4. Setting state (+0x70) to 0 intercepts character input
5. But camera doesn't move freely — activate function (kallis) never
   registered the object with the camera rendering pipeline
```

## Why It Doesn't Fully Work

1. Constructor (`sub_440910`) successfully creates the object and registers
   with the INPUT system (via sub_43C420, kallis — works)
2. Setting `state = 0` makes the InputValidator intercept character controls
3. BUT the camera pipeline never calls `vtable[3]` (CameraUpdate) because
   the Activate function (`sub_440660`, kallis) was never called
4. Calling Activate manually crashes — likely expects valid internal state
   that only exists when the object is created through the normal engine flow
5. The "Debug" input group has no key bindings in release

## Camera Pipeline Investigation (Session 3)

### Pipeline Structure

The camera pipeline function `sub_436190` (CameraComponentPipeline) iterates a
CamComponent array to call each component's CameraUpdate (vtable[3]):

```
camera_obj = [0x72F670]
manager = [camera_obj + 0x8D8]
components = manager + 0x88   // CamComponent* array
count = *(manager + 0xC8)     // number of active components
```

The pipeline iterates `count` entries starting at `components`, calling
`vtable[3]` on each CamComponent object. This is how the engine chains
camera behaviors (orbit, follow, debug, etc.).

### Key Findings

1. **Array is NOT fixed-size** -- the CamComponent array at +0x88 holds exactly
   `count` entries. Data beyond `count` slots belongs to other manager fields,
   not empty component slots.

2. **Cannot inject custom objects** -- the game expects heap-allocated objects
   with valid internal state. Writing a pointer to CE-allocated memory causes
   crashes when the pipeline tries to read vtable/internal pointers.

3. **Cannot overwrite existing components** -- each CamComponent type
   (orbit camera, follow camera, etc.) has a unique data layout with internal
   pointers. Overwriting one type with a debug camera pointer corrupts the
   expected layout and crashes.

4. **Constructor works for input** -- `sub_440910` (Constructor) successfully
   creates the DebugCameraManager and registers it with the input system.
   The "Debug" input group responds to InputValidator checks.

5. **Activate crashes** -- `sub_440660` (kallis activate) attempts to register
   the object with the camera pipeline but crashes when called manually.
   The kallis-obfuscated code likely validates internal state or calls into
   systems that expect a fully initialized pipeline context.

### InputValidator Reuse (Used in Freecam v17)

Although the debug camera's CameraUpdate cannot be activated, the
**InputValidator** (sub_440630) works independently of the camera pipeline:

- Creating the DebugCameraManager object and setting `state = 0` at offset
  `+0x70` causes the InputValidator to intercept all character input
- The character becomes frozen (no movement, no actions) while freecam is active
- This is used in freecam v17 to block character input during free-camera mode

### Conclusion (Updated)

The debug camera's CameraUpdate (vtable[3]) cannot be activated without
tracing the kallis-obfuscated activate function (`sub_440660`) at runtime.
The camera pipeline registration requires valid internal state that cannot
be replicated through static analysis or manual memory writes alone.

However, the InputValidator component works independently and has been
successfully reused in freecam v17 to block character input.

Our custom freecam (v17) provides equivalent functionality through a different
mechanism (hooking BuildLookAtMatrix + direct position control), with the
addition of InputValidator-based character freezing borrowed from the debug
camera system.
