# sub_52A0F0 -- CameraRender__SetFarClip
**Address:** 0x52A0F0 (Spiderwick.exe+12A0F0)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
int __userpurge CameraRender__SetFarClip@<eax>(
    float *this@<ecx>,  // camera render object
    int a2@<esi>,       // context (passed to nullsub)
    float farClip       // new far clip distance
);
```

## Purpose

Sets the far clip plane distance and rebuilds all derived data. Mirror of `SetNearClip`.

## Behavior

1. `this[1] = farClip` -- write far clip to +0x04
2. Call `CameraRender__SetFovAndClip(this, this[3], this[4], this[5], *this, farClip)`
   - Passes existing FOV, aspect, and nearClip from object
3. Call `nullsub_28(this + 434, a2)` -- no-op
4. Call `CameraRender__RebuildMatrices(this)` -- sub_529790
5. Call `sub_5AC230(this + 8, this + 418)` -- additional matrix operations

## Size

85 bytes. Single basic block.

## Callers

No callers found in static analysis. May be called from VM or dead code.

## Related

- [sub_5293D0_CameraRender__SetFovAndClip.md](sub_5293D0_CameraRender__SetFovAndClip.md) -- Called to rebuild
- [sub_52A090_CameraRender__SetNearClip.md](sub_52A090_CameraRender__SetNearClip.md) -- Counterpart for near clip
