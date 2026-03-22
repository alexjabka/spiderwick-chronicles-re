# sub_438E70 --- ActivateCamera

**Address:** 0x438E70 (Spiderwick.exe+38E70) | **Calling convention:** __thiscall (ECX = camera object)

---

## Purpose

Activates and initializes a camera on a character. This is a `.kallis` thunk --- the actual implementation lives in the `.kallis` segment and is dispatched via an indirect call through `off_1C899E8`.

Called after character switching to bind the camera to the newly active player character.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | void* | Camera object (obtained via [GetCameraObject](sub_4368B0_GetCameraObject.md)) |

**Returns:** unknown (likely void)

---

## Decompiled Pseudocode

```c
void __thiscall ActivateCamera(void *this)
{
    // .kallis thunk — indirect call to ROP dispatcher
    // off_1C899E8 --> .kallis segment implementation
    return off_1C899E8(this);
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x438E70` | Entry point (thunk) |
| `off_1C899E8` | `.kallis` indirect call target |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_463880` ([SetPlayerType](sub_463880_SetPlayerType.md)) | Activates camera on new player character at 0x463924 |
| Camera initialization paths | Startup, cutscene transitions, etc. |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `off_1C899E8` | `.kallis` camera activation | Actual camera init/bind logic (ROP chain) |

---

## Notes / Caveats

1. **This is a `.kallis` thunk.** The actual camera activation logic runs in the `.kallis` segment via ROP dispatch. It cannot be called from arbitrary native contexts (e.g., EndScene hooks) without ensuring the `.kallis` execution environment is properly set up.

2. **Usage pattern** in SetPlayerType:
   ```c
   Camera *cam = GetCameraObject(this);    // sub_4368B0
   ActivateCamera(cam);                    // sub_438E70
   ```

3. **Related functions:**
   - [GetCameraObject](sub_4368B0_GetCameraObject.md) (sub_4368B0) --- retrieves camera for a character
   - `sub_439770` (CameraClearAndInit) --- clears camera chain, called when detaching
