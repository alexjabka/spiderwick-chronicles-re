# sub_440790 — DebugCameraManager::InputHandler

## Identity

| Field | Value |
|---|---|
| Address | `0x440790` |
| Calling Convention | `thiscall` |
| Role | `MethodListener` callback |
| Module | engine/debug |

## Purpose

Input handler for the debug camera system. Registered during construction ([sub_440910](sub_440910_DebugCameraManager_Constructor.md)) as the `MethodListener` callback. Fires on every input event that matches the `"Debug"` input group.

Responsibilities:

1. On `DEBUG_CAMERA_ACTIVATE` with camera inactive (`state == -1`): calls `sub_440660` to activate the camera and assign a controller.
2. On `DEBUG_CAMERA_ACTIVATE` with the same controller already active (`state == controllerID`): calls `sub_440690` to deactivate.
3. While the camera is active: calls `sub_43C740` every frame to read analog movement inputs and write them into the object's movement fields.

`sub_43C740` reads each analog axis via `sub_5522F0` and stores the results at the movement offsets (`+0x14` through `+0x24`).

## Key References

| Symbol | Role |
|---|---|
| `this->cameraState` (`+0x2C`) | `-1` = inactive, otherwise = active controller ID |
| `sub_440660(this, controllerID)` | `kallis` — activates debug camera, stores controller ID |
| `sub_440690(this)` | `kallis` — deactivates debug camera, resets state to `-1` |
| `sub_43C740(this)` | Reads all movement axes from input hardware |
| `sub_5522F0` | Low-level analog input reader (called by `sub_43C740`) |

## Movement Fields Written by sub_43C740

| Offset | Field | Analog Input |
|---|---|---|
| `+0x14` | `pitch` | Vertical look axis |
| `+0x18` | `yaw` | Horizontal look axis |
| `+0x1C` | `forward` | Forward/back movement |
| `+0x20` | `right` | Left/right strafe |
| `+0x24` | `up` | Vertical elevation |

## Decompiled Pseudocode

```c
void __thiscall DebugCameraManager_InputHandler(
    DebugCameraManager *this,
    InputEvent *event)
{
    int inputHash = GetInputHash(event);
    int controllerID = GetControllerID(event);

    if (inputHash == hash("DEBUG_CAMERA_ACTIVATE"))
    {
        if (this->cameraState == -1)
        {
            // Camera inactive — activate for this controller
            sub_440660(this, controllerID);   // kallis: activate
        }
        else if (this->cameraState == controllerID)
        {
            // Same controller pressed again — toggle off
            sub_440690(this);                 // kallis: deactivate
        }
    }

    // If active, poll analog movement inputs every frame
    if (this->cameraState != -1)
    {
        sub_43C740(this);   // reads pitch/yaw/forward/right/up via sub_5522F0
    }
}
```

### sub_43C740 — Movement Input Reader (inline summary)

```c
void __thiscall sub_43C740(DebugCameraManager *this)
{
    // sub_5522F0(inputName) returns current analog float value [-1, 1]
    this->pitch   = sub_5522F0("DEBUG_CAMERA_PITCH");    // +0x14
    this->yaw     = sub_5522F0("DEBUG_CAMERA_YAW");      // +0x18
    this->forward = sub_5522F0("DEBUG_CAMERA_FORWARD");  // +0x1C
    this->right   = sub_5522F0("DEBUG_CAMERA_RIGHT");    // +0x20
    this->up      = sub_5522F0("DEBUG_CAMERA_UP");       // +0x24
}
```

## Notes

- The activate/deactivate pattern implements a toggle: pressing `DEBUG_CAMERA_ACTIVATE` once from the same controller that activated it switches the camera back off.
- A second controller pressing `DEBUG_CAMERA_ACTIVATE` while the camera is already active for a different controller is a no-op (neither branch matches).
- `sub_440660` and `sub_440690` are labelled `kallis` in the IDA database.
- Movement values written here are consumed each frame by [sub_43C4E0 — DebugCameraManager_CameraUpdate](sub_43C4E0_DebugCameraManager_CameraUpdate.md).
