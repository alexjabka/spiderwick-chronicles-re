# Spiderwick Freecam — Implementation Plan
**Last updated:** 2026-03-17 (v20.1 — COMPLETED)

## Core Principle

**Work WITH the engine, not against it.** See [../../docs/APPROACH.md](../../docs/APPROACH.md).

## Current State (v20.1) — COMPLETE

### What Works
- [x] **WASD movement** — pitch-aware, all directions correct
- [x] **Mouse rotation** — reads game's input buffer directly (72FC10/14), sensitivity 0.04 (was 0.015)
- [x] **Position persistence** — sub_4356F0 hook prevents overwrite (range check blocks entire camera struct)
- [x] **Engine-native matrix** — sub_5299A0 hook feeds our eye/target to LookAt
- [x] **Decoupling** — MonocleUpdate returns 1 without flags → orbit camera skipped (no monocle side-effects)
- [x] **Speed control** — NUM+/-, PgUp/PgDn (fast), HOME reset, Shift=3x speed
- [x] **Arrow key rotation** — faster in v16 (0.04 vs 0.01)
- [x] **Monocle HUD overlay disabled** — NOP at +C27FD
- [x] **No crash** — fixed NOP byte + monocle init
- [x] **pCamStruct** — clean hook at BuildViewMatrix entry (+1AA1B0)
- [x] **pPlayer** — current player via GetPlayerCharacter hook (+4F890), follows character switching
- [x] **Middle mouse teleport** — teleports player to camera position
- [x] **HUD auto-hide** — cheat flag bit 2 toggled automatically with freecam
- [x] **Character input block** — DebugCam InputValidator (sub_440630, state=0 at +0x70) freezes character
- [x] **Flicker fix** — CopyPositionBlock blocking expanded to entire camera struct (range check instead of single offset)
- [x] **Room visibility (sector tracking)** — Lua AABB sector lookup writes directly to camera_obj+0x788
- [x] **Sector loading** — bitmask freeze 0xFFFFFFFF (all sectors loaded)
- [x] **Sector culling disabled** — jump patches in UpdateVisibility

### Room Clipping Status — SOLVED (v20.1)
- [x] Sector loading forced (bitmask freeze 0xFFFFFFFF)
- [x] Sector culling disabled (jump patches in UpdateVisibility)
- [x] **Sector tracking** — Lua AABB lookup reads sector bounding boxes (`sector_data+0x10`/`+0x20`), writes sector to `camera_obj+0x788`
- [x] **Portal rendering** — portal frustum bypass (`jnz->jmp` at `0x51A3B5`) renders all connected rooms from correct sector
- **Key insight:** The solution is NOT to hack the matrix pipeline. It is to tell the engine the correct sector via direct +0x788 write, and let portal traversal do the rest.

### Teleport Observation
- Teleporting the player to camera position does NOT trigger SectorTransition
- Rooms only load via physical traversal through transition zones (sector boundary triggers)
- This means teleport alone cannot force new rooms to appear

### New Discoveries (Session 2)
- [x] **Character system reversed** — ClPlayerObj/ClCharacterObj layout mapped
- [x] **pPlayer** replaces pPlayerAddy — hooks GetPlayerCharacter (sub_44F890), follows char switching
- [x] **Character switching** — /Player/Character game state (1=Jared, 2=Simon, 3=Mallory), works on level transition
- [x] **Cheat flag system** — invulnerability, HUD hide, heal, ammo via direct memory write
- [x] **Debug camera found** — complete in code (ClDebugCameraManager) but dead in release
- [x] **IDA 9 decompile works** — Hex-Rays pseudocode via MCP

### New Discoveries (Session 3)
- [x] **Debug camera pipeline investigation** — camera pipeline (sub_436190) fully reversed, CamComponent array found
- [x] **Cannot activate debug CameraUpdate** — requires kallis runtime tracing, heap-allocated objects with unique layouts
- [x] **InputValidator reuse** — DebugCam InputValidator (state=0) successfully blocks character input, used for v17 freeze
- [x] **CopyPositionBlock range check** — expanded from single offset comparison to full struct range check, fixes flicker

### Architecture (v20.1)

```
Lua Timer (16ms):
  ├── Read mouse deltas from 72FC14 (yaw) and 72FC10 (pitch)
  ├── Apply to camYaw/camPitch with sensitivity scaling
  ├── Arrow keys → additional yaw/pitch modification
  ├── Apply WASD input → update position
  ├── Write fc_eye + fc_target from camYaw/camPitch
  └── AABB sector lookup → write sector to camera_obj+0x788

ASM Hooks:
  ├── Hook 1 (sub_5299A0): replace eye/target args with ours
  │   → engine builds correct view matrix via LookAt
  ├── Hook 2 (sub_4356F0): skip position copy for camera
  │   → prevents game overwriting our position
  ├── Hook 3 (sub_43DA50): return 1 WITHOUT monocle flags
  │   → orbit camera skipped, no monocle side-effects
  ├── Patch 4 (+C27FD): NOP overlay flag write
  └── Patch 5 (+88B69): NOP PerformRoomCulling call
```

| # | Target | What it does |
|---|--------|-------------|
| 1 | `sub_5299A0` (+1299A0) | Replaces eye/target for LookAt |
| 2 | `sub_4356F0` (+356F0) | Skips camera struct copy (ECX==camBase+0x388) |
| 3 | `sub_43DA50` (+3DA50) | Returns 1 without monocle flags (graceful decoupling) |
| 4 | `+C27FD` byte patch | Disables monocle HUD overlay |
| 5 | `+88B69` NOP 5 bytes | Disables PerformRoomCulling |

### Mouse Rotation (v15)

Mouse deltas are at static addresses, written every frame by
sub_43DC10 (vtable[7]) via sub_5522F0:
```
0x72FC14 — mouse delta X (yaw)
0x72FC10 — mouse delta Y (pitch)
```

Lua reads these directly and applies to camYaw/camPitch.
No monocle involvement for rotation — monocle is only for decoupling.

## Next Steps (polish only — core freecam COMPLETE)

1. **Mouse wheel speed** — find wheel delta address, add to Lua
2. **Full world pause** — NPCs/particles still animate (character frozen via InputValidator)
3. **FOV control** — find FOV value in camera struct, hook ready
4. **Debug camera kallis tracing** — trace sub_440660 at runtime to understand pipeline registration

## Evolution History

| Version | Approach | Result |
|---------|----------|--------|
| v1-v5 | Write rotation to wrong offsets | Didn't work |
| v6 | Skip sub_5299A0 + write matrix from Lua | Position overwritten |
| v7 | Hook generic memcpy (sub_5A7DC0) | Broke rendering |
| v8 | Hook sub_4356F0 + manual matrix | Inverted controls |
| v9 | Replace sub_5299A0 args | Forward/back fixed, left/right inverted |
| v10 | Shifted target + read vectors from matrix | WASD perfect, mouse via monocle+Shift |
| v13 | Force monocle + NOP overlay | Crash, left/right swapped, no mouse |
| v14 | Fix crash + game direction delta | Works with RMB only, spinning without |
| v15 | **Direct mouse delta from 72FC10/14** | **WORKING — full mouse + WASD** |
| v16 | Add teleport, faster arrows, room clipping experiments | Working + partial room fix |
| v17 | HUD auto-hide, input block (InputValidator), Shift=3x speed, mouse sens 0.04, flicker fix (range check CopyPositionBlock) | Working + character freeze |
| v20 | Graceful freecam (no monocle flags), Lua AABB sector lookup | Sector tracking works, but SectorTransition can't handle jumps |
| v20.1 | **Remove ALL matrix hacks, AABB sector → direct +0x788 write** | **COMPLETE — rooms render correctly everywhere** |

## Key Files

| File | Purpose |
|------|---------|
| `mods/freecam/freecam_CT_entry.cea` | Main CE table entry (v17) |
| `mods/freecam/pCamStruct_CT_entry.cea` | Camera struct pointer (BuildViewMatrix hook) |
| `mods/freecam/pPlayer_CT_entry.cea` | Player pointer (GetPlayerCharacter hook) |
| `mods/freecam/pPlayerAddy_CT_entry.cea` | OLD player pointer (sector distance hook, replaced by pPlayer) |
| `mods/freecam/nocull_farplane_CT_entry.cea` | Room culling patches v5 |
| `engine/camera/ARCHITECTURE.md` | Camera system architecture |
| `engine/camera/ROOM_CLIPPING.md` | Room clipping investigation (3-layer system) |
| `engine/camera/subs/sub_43E2B0_MainCameraComponent.md` | Main camera component (mouse deltas) |
| `mods/freecam/charswitch_CT_entry.cea` | Character switch (F7, works on level transition) |
| `mods/debug/debugcam_create.cea` | Debug camera instantiation (archaeological) |
| `engine/debug/CHEAT_SYSTEM.md` | Cheat flags documentation |
| `engine/debug/DEBUG_CAMERA.md` | Debug camera archaeological findings |
| `engine/objects/CHARACTER_SWITCHING.md` | Character switching system |
| `engine/objects/ClCharacterObj_layout.md` | Player/character object layout |
