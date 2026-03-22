# sub_51A130 — PortalTraversal_Native
**Address:** `Spiderwick.exe+1A130` (absolute: `0x0051A130`)
**Size:** 2232 bytes (0x8B8)
**Status:** FULLY REVERSED — full decompilation obtained, multiple patch approaches tested

## Critical Correction
Initial analysis assumed LABEL_65 (0x51A8CC) exits the portal loop.
**Full decompilation proved this WRONG**: LABEL_65 ends with `goto LABEL_76` (0x51A9C5)
which increments the portal counter and loops to the next portal. The outer while(1) loop
at the top processes ALL portals in the sector. Both LABEL_65 and the clip path
iterate through all portals correctly.

## Full Decompilation Available
The complete decompiled C code was obtained via `mcp__ida-pro-mcp__decompile(0x51A130)`.
Key structure: outer while(1) iterating portals, inner do-while for vertex clipping,
LABEL_65 for fast-path (pre-test pass), LABEL_76 for next-portal iteration.

## Purpose
The core recursive portal traversal function. Despite being called through a Kallis VM thunk (sub_51ABE0 / PortalTraversal_VM), this function is **native x86 code**. It iterates every portal in the current sector, performs visibility checks, and recursively traverses into connected sectors.

## Signature
```c
int __cdecl PortalTraversal_Native(
    int unknown1,       // arg_0
    int unknown2,       // arg_4
    int *sector_data,   // arg_8 — sector/portal data pointer
    int unknown4,       // arg_C
    int *camera_data,   // arg_10 — camera/frustum data
    int unknown6,       // arg_14
    int unknown7        // arg_18
)
// Stack frame: 0x19A8 bytes (huge — includes clip buffers)
// Called from: Kallis VM (via PortalTraversal_VM at 0x51ABE0)
// Recursively calls itself for each visible connected sector
```

## Internal Flow (per portal)

```
[0x51A2D9] Load portal position data
[0x51A33B] sub_529B30 — compute portal bounding data
[0x51A3A8] call sub_517E50 (PortalDistancePreTest)
           Sets arg_120 = 1 if portal is within camera range

[0x51A3AD] cmp [arg_120], 0
[0x51A3B5] jnz loc_51A8CC          ★ PATCH POINT
           If pre-test passed → jump to LABEL_65 (process portal)

[0x51A3BB] test byte ptr [esi+4], 2
[0x51A3BF] jz loc_51A8CC
           If portal flag bit 1 clear → jump to LABEL_65

--- Frustum Clip Path (only reached if pre-test failed AND flag set) ---

[0x51A3C5+] Set up portal polygon vertices from portal data at [esi+0x60]
[0x51A47A] call sub_5299E0 (ClipPortalPolygon)
           Clips portal polygon against frustum planes
           Returns: vertex count after clipping (0 = invisible)

[0x51A592] jle loc_51A9C5
           If 0 vertices → skip portal (portal not visible)

[0x51A782] Front/back face test (dot product)
           Determines if looking at front or back of portal

[0x51A800+] Recursive call to sub_51A130 for connected sector

--- LABEL_65: Direct Portal Processing ---

[0x51A8CC] mov ebp, [esi+8]       — load connected sector
[0x51A8CF] mov edx, [ebx]         — load current sector data
[0x51A8D1] cmp ebp, [edx+38h]     — compare sector IDs
           Process portal → recursive traversal into connected sector
```

## Key Sub-Function Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x51A1D5` | FrustumSetup_VM (sub_562650) | Compute frustum planes for this recursion level |
| `0x51A3A8` | PortalDistancePreTest (sub_517E50) | Check if portal within camera range |
| `0x51A47A` | ClipPortalPolygon (sub_5299E0) | Sutherland-Hodgman clip vs frustum |
| `0x51A800+` | sub_51A130 (self) | Recursive call for connected sector |

## Portal Data Structure (at ESI)

| Offset | Type | Field |
|--------|------|-------|
| +0x00 | ptr | Sector data pointer |
| +0x04 | byte | Portal flags (bit 1 = requires frustum test) |
| +0x08 | int | Connected sector index |
| +0x60+ | floats | Portal polygon vertices (triangles at stride 0x18) |

## Patch Point

**Address:** `0x51A3B5` = `Spiderwick.exe+1A3B5`

| | Bytes | Instruction |
|---|---|---|
| Original | `0F 85 11 05 00 00` | `jnz loc_51A8CC` (conditional) |
| Patched | `E9 12 05 00 00 90` | `jmp loc_51A8CC` + NOP (unconditional) |

Effect: ALL portals skip the frustum clip test and go directly to LABEL_65
for processing. The recursive traversal visits every connected sector
regardless of camera angle.

## Why This Works
- LABEL_65 handles portals correctly without clipped polygon data
- Traversal recursion is bounded by "visited sectors" tracking (prevents infinite loops)
- Performance: ~14 sectors in the Spiderwick house — negligible overhead on modern hardware
- No camera parameters are modified — character rendering unaffected

## Related
- [../PORTAL_SYSTEM.md](../PORTAL_SYSTEM.md) — full system overview
- [sub_5299E0_ClipPortalPolygon.md](sub_5299E0_ClipPortalPolygon.md) — the bypassed frustum clipper
- [sub_517E50_PortalDistancePreTest.md](sub_517E50_PortalDistancePreTest.md) — distance pre-test
- [sub_51ABE0_PortalTraversal_VM.md](sub_51ABE0_PortalTraversal_VM.md) — VM entry point
