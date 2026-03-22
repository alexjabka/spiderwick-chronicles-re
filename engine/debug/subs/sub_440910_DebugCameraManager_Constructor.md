# sub_440910 — DebugCameraManager Constructor

## Identity

| Field | Value |
|---|---|
| Address | `0x440910` |
| Calling Convention | `thiscall` (ECX = `this`, ~0x80 bytes allocated by caller) |
| Module | engine/debug |

## Purpose

Constructs a `DebugCameraManager` object. Responsibilities:

1. Registers the object with the `"Debug"` input group via `sub_43C420`.
2. Sets up a `MethodValidator` using callback `sub_440630` and registers it via `sub_54F7C0`.
3. Sets up a `MethodListener` using callback `sub_440790` ([DebugCameraManager_InputHandler](sub_440790_DebugCameraManager_InputHandler.md)) and registers it via `sub_54F880`.
4. Initialises the camera state field to `-1` (inactive / no controller assigned).

For the full object layout see [engine/debug/DEBUG_CAMERA.md](../DEBUG_CAMERA.md).

## Key References

| Symbol | Role |
|---|---|
| `sub_43C420(hash, 0)` | `kallis` — registers object with named input group (`"Debug"`) |
| `sub_54F880` | Registers the `MethodListener` callback |
| `sub_54F7C0` | Registers the `MethodValidator` callback |
| `sub_440630` | Validator callback (method validity predicate) |
| `sub_440790` | Listener callback — [DebugCameraManager_InputHandler](sub_440790_DebugCameraManager_InputHandler.md) |

## Object Layout (relevant fields)

| Offset | Type | Description |
|---|---|---|
| `+0x00` | `vtable*` | vftable pointer |
| `+0x04` | ... | (base class / listener chain) |
| `+0x14` | `float` | Pitch accumulator |
| `+0x18` | `float` | Yaw accumulator |
| `+0x1C` | `float` | Forward movement |
| `+0x20` | `float` | Right movement |
| `+0x24` | `float` | Up movement |
| `+0x28` | `int` | Teleport flag |
| `+0x2C` | `int` | Camera state (`-1` = inactive, else = active controller ID) |

## Decompiled Pseudocode

```c
DebugCameraManager *__thiscall DebugCameraManager_Constructor(DebugCameraManager *this)
{
    // Register with "Debug" input group
    sub_43C420(hash("Debug"), 0);   // kallis

    // Set up validator (sub_440630) and register it
    InitMethodValidator(&this->validator, sub_440630);
    sub_54F7C0(&this->validator);

    // Set up listener (sub_440790) and register it
    InitMethodListener(&this->listener, sub_440790);
    sub_54F880(&this->listener);

    // Initial state: inactive
    this->cameraState = -1;

    return this;
}
```

## Notes

- `hash("Debug")` denotes the runtime string hash of the literal `"Debug"` — the actual numeric value should be confirmed in the IDA database.
- The ~0x80 byte allocation is performed by the caller before ECX is set; the constructor itself does not call `operator new`.
- See [DEBUG_CAMERA.md](../DEBUG_CAMERA.md) for the complete documented object layout and camera system overview.
