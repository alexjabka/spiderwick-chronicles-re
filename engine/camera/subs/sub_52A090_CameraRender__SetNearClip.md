# sub_52A090 -- CameraRender__SetNearClip
**Address:** 0x52A090 (Spiderwick.exe+12A090)
**Status:** FULLY REVERSED (Session 5)

## Signature

```c
int __userpurge CameraRender__SetNearClip@<eax>(
    float *this@<ecx>,  // camera render object
    int a2@<esi>,       // context (passed to nullsub)
    float nearClip      // new near clip distance
);
```

## Purpose

Sets the near clip plane distance and rebuilds all derived data (frustum corners, projection matrix, additional matrices).

## Behavior

1. `this[0] = nearClip` -- write near clip to +0x00
2. Call `CameraRender__SetFovAndClip(this, this[3], this[4], this[5], nearClip, this[1])`
   - Passes existing FOV (+0x0C), aspectH (+0x10), aspectV (+0x14), and farClip (+0x04)
3. Call `nullsub_28(this + 434, a2)` -- no-op
4. Call `CameraRender__RebuildMatrices(this)` -- sub_529790
5. Call `sub_5AC230(this + 8, this + 418)` -- additional matrix operations

## Size

85 bytes. Single basic block.

## Callers

| Address | Function |
|---------|----------|
| 0x51A093 | sub_519F40 (called from PortalTraversal_Native) |

## Related

- [sub_5293D0_CameraRender__SetFovAndClip.md](sub_5293D0_CameraRender__SetFovAndClip.md) -- Called to rebuild
- [sub_52A0F0_CameraRender__SetFarClip.md](sub_52A0F0_CameraRender__SetFarClip.md) -- Counterpart for far clip
