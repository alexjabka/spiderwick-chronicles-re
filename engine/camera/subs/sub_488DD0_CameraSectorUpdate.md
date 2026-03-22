# sub_488DD0 — CameraSectorUpdate
**Address:** `Spiderwick.exe+88DD0` (absolute: `00488DD0`)
**Status:** FULLY REVERSED

## Purpose
The main per-frame render function. Gets the camera object, reads its sector
index, and orchestrates the full visibility + render pipeline. Calls
UpdateVisibility, SectorVisibilityUpdate2, and RenderDispatch in sequence.

## Signature
```c
void __cdecl CameraSectorUpdate(float deltaTime)
// deltaTime passed via stack (from caller's [esp+08])
```

## Full Decompiled Pseudocode
```c
void CameraSectorUpdate(float deltaTime) {
    CameraObject *cam = GetCameraObject();           // sub_4368B0 → [0x0072F670]
    int sector = cam->sectorIndex;                   // cam+0x788

    // Pass 1: UpdateVisibility
    UpdateVisibility(sector, cam, deltaTime);        // sub_488B30 (stdcall, ECX=0x006E4780)
    //   → PerformRoomCulling if sector system active
    //   → WorldUpdateTick
    //   → FinalizeVisibility_VM

    // Camera object method calls (sector may change during these)
    cam->Update1([0x6ED394], [0x6ED414], 0, 0, 2);  // sub_438E50
    cam->Update2();                                   // sub_438EE0
    cam->Update3(deltaTime);                          // sub_438EA0

    // Pass 2: SectorVisibilityUpdate2 (re-read sector, may have changed)
    sector = cam->sectorIndex;                        // cam+0x788 again
    SectorVisibilityUpdate2(sector, cam, deltaTime);  // sub_488970 (cdecl)
    //   → FrustumSetup_VM → PortalTraversal_VM → PostTraversal_VM
    //   → ambient → CheckVisMode_VM → FinalizeRender_VM

    // Pass 3: RenderDispatch
    RenderDispatch();                                  // sub_562760
    //   → processes render queue, issues D3D draw calls

    sub_55FFB0();
    jmp sub_552420();   // tail call
}
```

## Assembly
```asm
+88DD0: 56                push esi
+88DD1: E8 DADAFAFF       call sub_4368B0           ; GetCameraObject()
+88DD6: D9 44 24 08       fld  dword ptr [esp+08]   ; load deltaTime
+88DDA: 51                push ecx
+88DDB: 8B F0             mov  esi, eax             ; esi = camera_obj
+88DDD: D9 1C 24          fstp dword ptr [esp]      ; deltaTime on stack
+88DE0: 8B 86 88070000    mov  eax,[esi+00000788]   ; eax = camera sector
+88DE6: 56                push esi                  ; arg: camera_obj
+88DE7: 50                push eax                  ; arg: sector
+88DE8: B9 80476E00       mov  ecx, 006E4780        ; this = SectorVisibilityManager
+88DED: E8 3EFDFFFF       call sub_488B30           ; UpdateVisibility()
+88DF2: 8B 0D 14D46E00    mov  ecx,[006ED414]
+88DF8: 8B 15 94D36E00    mov  edx,[006ED394]
+88DFE: 6A 02             push 02
+88E00: 6A 00             push 00
+88E02: 6A 00             push 00
+88E04: 51                push ecx
+88E05: 52                push edx
+88E06: 8B CE             mov  ecx, esi             ; this = camera_obj
+88E08: E8 4300FBFF       call sub_438E50
+88E0D: 8B CE             mov  ecx, esi
+88E0F: E8 CC00FBFF       call sub_438EE0
+88E14: D9 44 24 08       fld  dword ptr [esp+08]
+88E18: 51                push ecx
+88E19: 8B CE             mov  ecx, esi
+88E1B: D9 1C 24          fstp dword ptr [esp]
+88E1E: E8 7D00FBFF       call sub_438EA0
+88E23: D9 44 24 08       fld  dword ptr [esp+08]
+88E27: 8B 86 88070000    mov  eax,[esi+00000788]   ; re-read sector
+88E2D: 51                push ecx
+88E2E: D9 1C 24          fstp dword ptr [esp]
+88E31: 56                push esi                  ; arg: camera_obj
+88E32: 50                push eax                  ; arg: sector
+88E33: E8 38FBFFFF       call sub_488970           ; SectorVisibilityUpdate2
+88E38: 83 C4 0C          add  esp, 0C              ; cdecl cleanup (3 args)
+88E3B: E8 20990D00       call sub_562760           ; RenderDispatch
+88E40: E8 6B710D00       call sub_55FFB0
+88E45: 5E                pop  esi
+88E46: E9 D5950C00       jmp  sub_552420           ; tail call
```

## Pipeline Sequence
```
CameraSectorUpdate (this function)
├── Pass 1: UpdateVisibility (sub_488B30)
│   ├── PerformRoomCulling (sub_564950) — if sector system active
│   ├── WorldUpdateTick
│   └── FinalizeVisibility_VM
├── Camera object updates
│   └── sub_438E50, sub_438EE0, sub_438EA0
├── Pass 2: SectorVisibilityUpdate2 (sub_488970)
│   ├── FrustumSetup_VM → PortalTraversal_VM → PostTraversal_VM
│   ├── Ambient color
│   ├── CheckVisMode_VM
│   └── FinalizeRender_VM
└── Pass 3: RenderDispatch (sub_562760)
    └── Processes render queue → D3D draw calls
```

## Key Observations
- Reads camera sector **twice**: at +88DE0 and +88E27
- First read -> `sub_488B30` (UpdateVisibility, stdcall, callee cleans)
- Second read -> `sub_488970` (SectorVisibilityUpdate2, cdecl, caller cleans 0xC)
- Camera object methods called between the two reads (sector may change)

## Globals Referenced
| Address | Purpose |
|---------|---------|
| `0x006E4780` | SectorVisibilityManager singleton (ECX for sub_488B30) |
| `0x006ED414` | Parameter for cam->Update1 |
| `0x006ED394` | Parameter for cam->Update1 |

## Mod Hook Point

This function is called every game update frame and is the primary hook point for **deferred character switching** in SpiderMod. By hooking `CameraSectorUpdate`, the mod can:

1. Detect a pending character switch request
2. Execute the switch at a safe point in the game loop (after rendering, before next frame)
3. Avoid the instability of switching during EndScene or other D3D callbacks

The function runs in the main game thread, after the previous frame's rendering is complete, making it a safe context for calling engine functions that manipulate player state, slots, and camera.

See [../../objects/HOT_SWITCH_SYSTEM.md](../../objects/HOT_SWITCH_SYSTEM.md) for full hot-switch documentation.

## Related
- [sub_488B30](sub_488B30_UpdateVisibility.md) -- Pass 1: UpdateVisibility
- [sub_488970](sub_488970_SectorVisibilityUpdate2.md) -- Pass 2: portal traversal + render submit
- [sub_562760](sub_562760_RenderDispatch.md) -- Pass 3: flush render queue to D3D
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) -- complete portal system reverse engineering
- [../../objects/HOT_SWITCH_SYSTEM.md](../../objects/HOT_SWITCH_SYSTEM.md) -- Hot-switch system (uses this as deferred hook)
