# sub_488B30 — UpdateVisibility
**Address:** `Spiderwick.exe+88B30` (absolute: `00488B30`)
**Status:** FULLY REVERSED

## Purpose
Sector visibility manager method. Pipeline 1 Pass 1. Conditionally calls
PerformRoomCulling based on whether the sector system is active, then runs
WorldUpdateTick and FinalizeVisibility_VM.

Called every frame by CameraSectorUpdate (sub_488DD0).

## Signature
```c
int __stdcall UpdateVisibility(int cameraSector, CameraObject *camera_obj, float deltaTime)
// ECX = this (SectorVisibilityManager singleton at 0x006E4780)
// cameraSector = camera's current sector index (arg1) — NOT USED DIRECTLY
// camera_obj   = SectorCameraObject pointer (arg2)
// deltaTime    = frame delta (arg3, float)
// Returns: int (from FinalizeVisibility_VM / sub_5625E0)
// Callee cleans: retn 0Ch (3 args x 4 bytes)
```

## Full Decompiled Pseudocode
```c
int UpdateVisibility(int cameraSector, CameraObject *camera_obj, float deltaTime) {
    nullsub_182();                    // no-op (debug stub?)
    sub_53E9C0();                     // unknown setup

    // Step 1: Time-based update
    WorldUpdateTick(deltaTime);       // sub_519350

    // Step 2: Room culling (conditional)
    if (IsSectorSystemActive() || IsSectorSystemActive2()) {
    //  sub_516B10()                     sub_516E30()
        PerformRoomCulling(deltaTime, camera_obj);   // sub_564950
    }

    // Step 3: Post update
    PostUpdate(deltaTime);            // sub_55FD60

    // Step 4: Finalize
    return FinalizeVisibility_VM();   // sub_5625E0
}
```

**Critical:** `cameraSector` (arg1) is passed in but **never read** by this function.
`sub_564950` reads the sector directly from `camera_obj+0x788`.

## Assembly
```asm
sub_488B30:
+88B30: E8 ...           call  nullsub_182
+88B35: E8 ...           call  sub_53E9C0
+88B3A: D9 44 24 0C      fld   [esp+0Ch]          ; deltaTime (arg3)
+88B3E: 51               push  ecx
+88B3F: D9 1C 24         fstp  [esp]
+88B42: E8 ...           call  sub_519350          ; WorldUpdateTick(dt)
+88B47: 83 C4 04         add   esp, 4
+88B4A: E8 ...           call  sub_516B10          ; IsSectorSystemActive()
+88B4F: 84 C0            test  al, al
+88B51: 75 09            jnz   short loc_488B5C    ; if true → do culling
+88B53: E8 ...           call  sub_516E30          ; IsSectorSystemActive2()
+88B58: 84 C0            test  al, al
+88B5A: 74 15            jz    short loc_488B71    ; if false → skip culling

loc_488B5C:                                        ; — culling block —
+88B5C: 8B 44 24 08      mov   eax, [esp+08h]     ; eax = camera_obj (arg2)
+88B60: D9 44 24 0C      fld   [esp+0Ch]          ; deltaTime (arg3)
+88B64: 50               push  eax                ; arg: camera_obj
+88B65: 51               push  ecx
+88B66: D9 1C 24         fstp  [esp]              ; arg: deltaTime
+88B69: E8 ...           call  sub_564950          ; PerformRoomCulling
+88B6E: 83 C4 08         add   esp, 8

loc_488B71:                                        ; — after culling —
+88B71: D9 44 24 0C      fld   [esp+0Ch]
+88B75: 51               push  ecx
+88B76: D9 1C 24         fstp  [esp]
+88B79: E8 ...           call  sub_55FD60          ; PostUpdate(dt)
+88B7E: 83 C4 04         add   esp, 4
+88B81: E8 ...           call  sub_5625E0          ; FinalizeVisibility_VM()
+88B86: C2 0C 00         retn  0Ch                 ; stdcall cleanup
```

## Pipeline Sequence
```
UpdateVisibility (this function)
├── PerformRoomCulling (sub_564950) — if sector system active
├── WorldUpdateTick (sub_519350)
└── FinalizeVisibility_VM (sub_5625E0)
```

## Patch Target
**NOP the call at +88B69** to disable room culling:
```
+88B69: E8 xx xx xx xx  →  90 90 90 90 90
```
Stack stays balanced: `push eax` + `push ecx` before, `add esp, 8` after.

## Gate Functions
| Function | Returns true when... |
|----------|---------------------|
| `sub_516B10` | Sector culling system is active (likely "indoors") |
| `sub_516E30` | Secondary check (likely "sector system initialized") |

Both must return false to skip culling entirely.

## Sub-functions
| Address | Name | Args | Purpose |
|---------|------|------|---------|
| `sub_519350` | WorldUpdateTick | (float dt) | Pre-culling time update |
| `sub_516B10` | IsSectorSystemActive | () → bool | Gate check 1 |
| `sub_516E30` | IsSectorSystemActive2 | () → bool | Gate check 2 |
| `sub_564950` | PerformRoomCulling | (float dt, CameraObject*) | The culling |
| `sub_55FD60` | PostUpdate | (float dt) | Post-culling update |
| `sub_5625E0` | FinalizeVisibility_VM | () → int | Return value |

## Relationship to Portal System
This function controls **Layer 2** (PerformRoomCulling) only. The portal rendering
system (Layer 3) is handled separately by `SectorVisibilityUpdate2` (sub_488970),
which is called AFTER this function in the CameraSectorUpdate pipeline.

PerformRoomCulling (sub_564950) is **object-level distance culling** — NOT the portal
frustum system. The portal traversal happens in native code at `sub_51A130`.

See [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) for the full portal system.
