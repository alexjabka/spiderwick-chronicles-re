# sub_43C4E0 — DebugCameraManager::CameraUpdate

## Identity

| Field | Value |
|---|---|
| Address | `0x43C4E0` |
| Calling Convention | `thiscall` (ECX = debug camera object) |
| Stack Arguments | `float *inputMatrix`, `int outputTarget`, `float yawSpeed` |
| Module | engine/debug |

## Purpose

Core free-camera update function. Called each frame while the debug camera is active. It:

1. Reads per-frame movement/rotation deltas from the object's movement fields (pitch, yaw, forward, right, up).
2. If any movement magnitude exceeds `0.001`:
   - Scales values by delta time.
   - Builds a new rotation matrix from pitch/yaw.
   - Translates the camera position along the local forward/right/up axes.
   - Writes the result to `outputTarget`.
3. If no movement: copies `inputMatrix` directly to `outputTarget` (camera is stationary).
4. If the teleport flag (`+0x28`) is set: teleports all player characters to the camera position offset `+20` units along the camera's forward axis.

## Input Matrix Layout

| Index | Meaning |
|---|---|
| `[4]`–`[6]` | Right axis (rotation row) |
| `[8]`–`[10]` | Up axis (rotation row) |
| `[12]`–`[14]` | World position (translation column) |

## Key References

| Symbol | Role |
|---|---|
| `this+0x14` | `float pitch` — accumulated pitch delta |
| `this+0x18` | `float yaw` — accumulated yaw delta |
| `this+0x1C` | `float forward` — forward movement delta |
| `this+0x20` | `float right` — right/strafe movement delta |
| `this+0x24` | `float up` — vertical movement delta |
| `this+0x28` | `int teleportFlag` — when non-zero, triggers player teleport |
| `GetPlayerCharacter()` | Returns player character object for teleport |

## Decompiled Pseudocode

```c
void __thiscall DebugCameraManager_CameraUpdate(
    DebugCameraManager *this,
    float *inputMatrix,
    int outputTarget,
    float yawSpeed)
{
    float pitch   = this->pitch;    // +0x14
    float yaw     = this->yaw;      // +0x18
    float forward = this->forward;  // +0x1C
    float right   = this->right;    // +0x20
    float up      = this->up;       // +0x24

    float dt = GetDeltaTime();

    // Check if there is any meaningful movement input
    if (fabsf(pitch)   > 0.001f ||
        fabsf(yaw)     > 0.001f ||
        fabsf(forward) > 0.001f ||
        fabsf(right)   > 0.001f ||
        fabsf(up)      > 0.001f)
    {
        // Scale by delta time
        float dPitch   = pitch   * dt;
        float dYaw     = yaw     * yawSpeed * dt;
        float dForward = forward * dt;
        float dRight   = right   * dt;
        float dUp      = up      * dt;

        // Build rotation from accumulated pitch/yaw, apply to current matrix
        float newMatrix[16];
        BuildRotationMatrix(newMatrix, inputMatrix, dPitch, dYaw);

        // Translate position along local axes
        // inputMatrix[4-6]   = right axis
        // inputMatrix[8-10]  = up axis
        // inputMatrix[12-14] = position
        newMatrix[12] = inputMatrix[12]
                      + inputMatrix[4]  * dRight
                      + inputMatrix[8]  * dUp
                      + inputMatrix[0]  * dForward;  // forward row
        newMatrix[13] = inputMatrix[13]
                      + inputMatrix[5]  * dRight
                      + inputMatrix[9]  * dUp
                      + inputMatrix[1]  * dForward;
        newMatrix[14] = inputMatrix[14]
                      + inputMatrix[6]  * dRight
                      + inputMatrix[10] * dUp
                      + inputMatrix[2]  * dForward;

        WriteMatrix(outputTarget, newMatrix);
    }
    else
    {
        // No movement — pass through
        CopyMatrix(outputTarget, inputMatrix);
    }

    // Teleport all players to camera position + 20 units forward
    if (this->teleportFlag)  // +0x28
    {
        float *camPos = GetCurrentCameraPosition();
        float *camFwd = GetCurrentCameraForward();

        float destX = camPos[0] + camFwd[0] * 20.0f;
        float destY = camPos[1] + camFwd[1] * 20.0f;
        float destZ = camPos[2] + camFwd[2] * 20.0f;

        PlayerCharacter *player = GetPlayerCharacter();
        if (player)
            SetCharacterPosition(player, destX, destY, destZ);

        this->teleportFlag = 0;
    }
}
```

## Notes

- The `0.001f` dead-zone prevents floating-point drift from causing constant matrix rebuilds when no input is being given.
- `inputMatrix[4-6]` is the rotation row used for right-direction translation, and `[12-14]` is the position; this is consistent with a row-major 4x4 transform.
- The teleport offset of `20` units is a fixed world-space distance; it places the player just in front of the camera rather than at the exact camera origin.
- See [sub_440790_DebugCameraManager_InputHandler.md](sub_440790_DebugCameraManager_InputHandler.md) for how movement values are written into the object fields that this function reads.
