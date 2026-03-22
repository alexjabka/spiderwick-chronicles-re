# Sector Loading & Streaming System

**Status:** Fully reversed
**Key addresses:** `0x0133FEC0` (WorldSectorMgr), `0x01340080` (LoadBitmask), `0x0133FEF8` (SectorStateArray)

## Overview

The Spiderwick engine uses a sector-based streaming system to manage indoor geometry. The house has 14 sectors (rooms). At any time, only a subset are loaded into memory based on the player's position. A Kallis VM script computes which sectors to load, and the native streaming pipeline handles the actual I/O and GPU resource management.

## Architecture

```
Kallis VM Script
  |
  v  writes g_SectorLoadBitmask (0x01340080)
  |
SectorUpdateTick (0x57F140)           <-- called each frame from WorldUpdateTick
  |
  +-- SectorTimerUpdate (0x57E2E0)    <-- advances per-sector load timers
  |
  +-- if state == 0 (ready):
  |     LoadUnloadSectors (0x57ED60)  <-- compares bitmask, initiates load/unload
  |
  +-- if state == 1 (streaming):
        StreamingPump (0x57E9E0)      <-- per-frame streaming state machine
```

## Key Functions

| Address | Name | Purpose |
|---------|------|---------|
| `0x57F140` | SectorUpdateTick | Per-frame orchestrator. Reads `g_SectorLoadBitmask`, dispatches to load/unload or streaming. |
| `0x57E2E0` | SectorTimerUpdate | Advances per-sector load timing. Prints load info when sector completes. |
| `0x57ED60` | LoadUnloadSectors | Compares requested bitmask vs loaded. Creates stream slots, submits I/O, handles sync/async. |
| `0x57E9E0` | StreamingPump | Processes stream slots through 6-state pipeline (queued -> IO -> textures -> done -> unload -> free). |
| `0x57ECB0` | CancelUnneededStreams | Cancels in-flight streams that are no longer needed (bitmask changed). |
| `0x57E360` | FindOrCreateStreamSlot | Finds a free stream slot or returns existing one for a chunk. Slots are 2084 bytes each. |
| `0x57DBB0` | StreamingIO_Submit | Submits a streaming I/O request (address + size) to the async I/O system. |
| `0x57DCC0` | StreamingIO_CalcAddress | Converts a file offset to a memory-mapped address using block table. |
| `0x5179A0` | RegisterSectorDrawData | After I/O completes: registers textures, creates D3D resources, hooks into render pipeline. |
| `0x57E540` | UnregisterSectorDrawData | Reverse of above: unregisters draw data, geometry instances, world instances from renderer. |
| `0x57E4B0` | UnregisterSectorTextures | Unregisters sector texture resources. |
| `0x57E960` | UnloadAllSectors | Force-unloads everything (used during world cleanup). |
| `0x57E0D0` | IsSectorLoaded | Returns true if sector state == 3 (loaded). Used by portal traversal. |
| `0x57E0B0` | IsSectorThrottling | Returns true if sector state == 2 (IO in flight). Debug display only. |
| `0x57E0F0` | IsSectorUnloading | Returns true if sector state == 4 (unloading). Debug display only. |
| `0x57E050` | AnySectorLoaded | Scans all sectors, returns true if any are loaded. Readiness check. |
| `0x519350` | WorldUpdateTick | Parent tick function calling SectorUpdateTick + other world systems. |

## Sector State Machine

### Per-Sector State (g_SectorStateArray at 0x0133FEF8)

Array of 14 entries, 12 bytes each (3 DWORDs): `[state, ?, load_timer_float]`

| State | Name | Meaning |
|-------|------|---------|
| 0 | Unloaded | Sector geometry not in memory |
| 1 | Queued/Loading | Stream slot created, waiting for I/O slot |
| 2 | Throttling | I/O submitted, waiting for completion |
| 3 | **Loaded** | Fully loaded, textures registered, renderable |
| 4 | Unloading | Being unloaded (textures/draw data being freed) |

**State Query Functions:**
- `IsSectorLoaded` (`0x57E0D0`): `g_SectorStateArray[sectorIndex * 3] == 3` -- see [subs/sub_57E0D0_IsSectorLoaded.md](subs/sub_57E0D0_IsSectorLoaded.md)
- `IsSectorThrottling` (`0x57E0B0`): `g_SectorStateArray[sectorIndex * 3] == 2` -- see [subs/sub_57E0B0_IsSectorThrottling.md](subs/sub_57E0B0_IsSectorThrottling.md)
- `IsSectorUnloading` (`0x57E0F0`): `g_SectorStateArray[sectorIndex * 3] == 4` -- see [subs/sub_57E0F0_IsSectorUnloading.md](subs/sub_57E0F0_IsSectorUnloading.md)
- `AnySectorLoaded` (`0x57E050`): scans all sectors, returns true if any state == 3 -- see [subs/sub_57E050_AnySectorLoaded.md](subs/sub_57E050_AnySectorLoaded.md)

### Stream Slot States (internal to StreamingPump)

Each stream slot is 2084 bytes (521 DWORDs). Field layout: `[state, sectorBitmask, chunkID, fileAddr, fileSize, ioHandle, ..., seqNumber@520]`

| State | Meaning |
|-------|---------|
| 0 | Free slot |
| 1 | Queued - waiting for I/O bandwidth |
| 2 | I/O in flight |
| 3 | I/O complete - ready for texture registration |
| 4 | Textures registered - ready for draw data |
| 5 | Fully loaded (renderable) |
| 6 | Pending free (unload requested) |

### WorldSectorMgr State (g_WorldSectorMgr at 0x0133FEC0)

| Offset | Field | Purpose |
|--------|-------|---------|
| +0 | state | 0=ready for load/unload, 1=streaming active |
| +5 (DWORD) | forceSync | If false, async loading allowed |
| +6..+9 (DWORDs) | phase bitmasks | Sectors in different loading phases |
| +8 (DWORD) | activeIOCount | Number of concurrent I/O operations |
| +11 (DWORD) | streamSlotCount | Total stream slot capacity |
| +12 (DWORD) | sectorCount | Number of sectors (14 for indoor) |
| +20 (DWORD) | loadedSectorCount | Count of fully loaded sectors |
| +40 (DWORD) | pendingUnload | Pending unload bitmask |
| +44 (DWORD) | maxStreamSlots | Max stream slots |
| +48 (DWORD) | timerEntryCount | Per-sector timer array size |
| +52 | timerArray | 12-byte entries: [sectorID, timerState, elapsedTime] |
| +109 (DWORD) | unloadDelay | Threshold time before unloading sector (at +436 of this) |
| +110 (DWORD) | streamSlotsPtr | Pointer to stream slot array |
| +112 (DWORD) | deferredBitmask | Bitmask saved when async not ready |
| +178 (DWORD) | priorityTableOffset | Offset into sector priority table |
| +452 (DWORD) | currentStreamSlot | Currently active stream slot ptr |
| +716 (BYTE) | ioCancelFlag | Set when an I/O needs cancellation |
| +717 (BYTE) | canBlock | Whether LoadUnloadSectors can block |

## Loading Bitmask (g_SectorLoadBitmask at 0x01340080)

**Written by:** Kallis VM script (no native writer found - only 1 data xref, a read at `0x57F16B`)
**Read by:** SectorUpdateTick -> LoadUnloadSectors

The VM script computes this bitmask based on the player's current sector position. Bit N set = sector N should be loaded. Typically loads the current room + adjacent rooms reachable through portals.

### How Loading Decides What to Load

1. VM script writes `g_SectorLoadBitmask` based on player position
2. `SectorUpdateTick` reads it each frame
3. `LoadUnloadSectors` XORs requested vs currently loaded to find deltas
4. New sectors: creates stream slots, submits async I/O, registers draw data
5. Removed sectors: cancels streams, unregisters textures/draw data, frees slots
6. Sorting: sectors sorted by priority (from a priority table) to load most important first

## Streaming I/O System

| Address | Global | Purpose |
|---------|--------|---------|
| `0x0133FE8C` | g_StreamingPoolSize | Total streaming memory pool in bytes |
| `0x0133FE7C` | g_StaticMemUsage | Static (always-loaded) memory usage |
| `0x0133FE80` | g_StreamBlockCount | Number of streaming blocks available |
| `0x0133FE84` | g_StreamBlockSize | Size of each streaming block |
| `0x0133FE94` | g_StreamBlockCapacity | Maximum streaming blocks |
| `0x0133FE98` | (IO queue ptr) | Pointer to I/O request queue |
| `0x0133FEA0` | (block map base) | Base of streaming block map |
| `0x0133FEA8` | (block offset table) | Block address offset table |
| `0x0133FE78` | (block alignment) | Block alignment size for I/O |
| `0x01340188` | g_PendingStreamCount | Number of pending stream operations |
| `0x01340190` | g_StreamingSeqCounter | Monotonically increasing sequence number for stream priority |

The streaming system manages one I/O at a time (`activeIOCount` at mgr+8). When a slot is in state 1 (queued), StreamingPump picks the one with the lowest sequence number and submits it via `StreamingIO_Submit`. After completion, textures are registered (`RegisterSectorDrawData`) and geometry is hooked into the renderer.

## Kallis VM Script Functions (Sector-related)

| Script Name | Native Handler | Purpose |
|-------------|---------------|---------|
| `sauIsSectorLoaded` | `0x541DE0` | Calls IsSectorLoaded, returns bool to script |
| `sauHideSector` | `0x541DB0` | Sets sector hidden flag (field +140 of sector object) |
| `sauSetCameraSector` | `0x490F40` | Sets camera's sector index (field +1928 of camera object) |
| `sauSetPlayerSector` | (via `0x49BA20`) | Sets player's current sector |
| `sauGetSector` | (via `0x52F0F0`) | Gets object's current sector |
| `sauSetSector` | (via multiple) | Sets object's sector assignment |
| `sauGetObjSectorIndex` | (via `0x49BA20`) | Gets object's sector index |

## How to Force-Load All Sectors (Freecam Fix)

### The Problem
The VM script computes `g_SectorLoadBitmask` based on **player position**, not camera position. When freecam moves the camera to a different room, that room's sector isn't loaded because the player is still in the original room.

### Solution Options

#### Option 1: Force Bitmask to 0x3FFF (Simplest)
Write `0x3FFF` (bits 0-13 = all 14 sectors) to `g_SectorLoadBitmask` at `0x01340080` every frame.

**CE Auto Assembler:**
```
[ENABLE]
alloc(forceSectors, 64)
createthread(forceSectors)

forceSectors:
  mov dword ptr [0x01340080], 3FFF
  ret

[DISABLE]
dealloc(forceSectors)
```

**Pros:** Dead simple, one write.
**Cons:** One-shot -- VM script will overwrite it next frame. Need to hook or loop.

#### Option 2: Hook SectorUpdateTick (Recommended)
Patch at `0x57F16B` where the bitmask is read: replace `mov eax, [g_SectorLoadBitmask]` with `mov eax, 0x3FFF`.

**Original bytes at 0x57F16B:** `A1 80 00 34 01` (mov eax, [0x01340080])
**Patched bytes:** `B8 FF 3F 00 00` (mov eax, 0x00003FFF)

This is a 5-byte instruction replaced with a 5-byte instruction -- perfect fit.

**CE Auto Assembler:**
```
[ENABLE]
// Force all 14 sectors to load
// Original: mov eax, [g_SectorLoadBitmask]  (A1 80 00 34 01)
// Patched:  mov eax, 3FFF                   (B8 FF 3F 00 00)
0x57F16B:
  db B8 FF 3F 00 00

[DISABLE]
// Restore original
0x57F16B:
  db A1 80 00 34 01
```

**Pros:** Surgical, does not fight the VM. The bitmask is read exactly once per frame right here. All downstream loading logic works normally. Sectors load asynchronously as designed.
**Cons:** Uses more memory (all 14 sectors in VRAM). May take a few seconds to stream all sectors initially.

#### Option 3: Force All Sector States to Loaded
Write `3` to every `g_SectorStateArray[i*3]` for i=0..13 at `0x0133FEF8`. This would trick the portal system into thinking all sectors are loaded without actually loading geometry.

**WARNING:** This would cause rendering artifacts or crashes because the GPU resources (textures, vertex buffers) wouldn't actually exist. NOT recommended.

#### Option 4: NOP the Bitmask Write + Pre-set (Persistent)
Write `0x3FFF` to `g_SectorLoadBitmask` once, then NOP any VM instructions that write to it. This requires finding and patching the Kallis VM bytecode, which is more fragile.

### Recommended Approach: Option 2

The 5-byte patch at `0x57F16B` is the cleanest solution:
- Works with the engine's streaming system (not against it)
- All sectors load properly with textures and geometry
- Streaming is async, so the game doesn't freeze
- Only 5 bytes patched, easily reversible
- Compatible with existing freecam + portal bypass mods

### Memory Considerations

With all 14 sectors loaded:
- Each sector has N chunks of `g_StreamBlockSize` bytes
- Total = sum of all sector chunk counts * block size
- The streaming pool (`g_StreamingPoolSize`) may not be large enough
- If pool is exhausted, the engine will throttle (state 2) and wait
- May need to increase `g_StreamBlockCount` or `g_StreamBlockCapacity` if sectors fail to load

### Integration with Existing Mods

The freecam mod already handles:
1. **Portal Bypass** (Layer 3) -- `jnz->jmp` at `0x51A3B5`
2. **Room Culling** (Layer 2) -- NOP `PerformRoomCulling` calls
3. **Sector Transition** (Layer 4) -- NOP write at `0x439356`

Adding **Sector Loading** (Layer 1) completes the picture:
4. **Sector Loading** -- `mov eax, 0x3FFF` at `0x57F16B`

With all four layers patched, the freecam can fly anywhere in the house and see all rooms rendered with full geometry.
