# SpiderMod — ASI Mod for The Spiderwick Chronicles (2008)

FreeCam and debug tooling injected via dinput8.dll proxy + MinHook EndScene hook.

## Project Structure

```
spidermod/
├── src/
│   ├── addresses.h         - All reversed addresses from RE
│   ├── memory.h            - Nop/Restore/Read/Write helpers (inline)
│   ├── d3d_hook.h/.cpp     - EndScene hook via game's real D3D device
│   ├── menu.h/.cpp         - On-screen menu overlay
│   ├── freecam.h/.cpp      - FreeCam with ASM hooks + AABB sector
│   ├── dinput8_proxy.cpp   - DLL proxy for injection
│   ├── dinput8_proxy.def   - Export definition
│   └── dllmain.cpp         - Entry point + EndScene hook loop
├── lib/MinHook/            - (fetched via CMake)
├── build/                  - Build output
└── CMakeLists.txt          - MSVC x86, C++17, MinHook, d3d9/d3dx9
```

## How It Works

1. `dinput8.dll` proxy loaded by game (game imports `DirectInput8Create`)
2. Proxy forwards `DirectInput8Create` to real system `dinput8.dll`
3. Proxy loads `spidermod.asi` via `LoadLibrary`
4. spidermod hooks EndScene via MinHook (uses game's real D3D device at `0xE36E8C`)
5. EndScene hook runs every frame: menu + freecam update + overlay drawing

## Build Requirements

- VS2019 BuildTools (MSVC 14.29)
- DirectX SDK June 2010 (for d3dx9)
- CMake 3.15+
- MinHook (auto-fetched from GitHub)

## Build Commands

```
cd build
cmake -G "Visual Studio 16 2019" -A Win32 ..
cmake --build . --config Release
```

Output: `build/Release/spidermod.asi` + `build/Release/dinput8.dll`

## Installation

Copy `dinput8.dll` + `spidermod.asi` to the game directory (next to `Spiderwick.exe`).

## FreeCam Architecture (from deep RE)

### Hooks

- **Hook 1** (`0x5299A0` SetViewMatrix): replaces eye/target with freecam position
- **Hook 2** (`0x4356F0` CopyPositionBlock): blocks engine from overwriting camera struct
- **Hook 3** (`0x43DA50` MonocleUpdate): returns 1 WITHOUT setting monocle flags -- graceful skip of normal camera while keeping engine in normal rendering mode

### Sector Tracking

- AABB lookup reads sector bounding boxes from world state (`sector_data+0x10` min, `+0x20` max)
- Writes correct sector to `camera_obj+0x788` every frame
- Falls back to nearest sector when outside all AABBs

### Key Discovery: Why Monocle Mode Kills Rendering

MonocleUpdate returning 1 with monocle flags set causes `camera_obj+0x78C` flags to be zeroed. CameraTick VM then takes the monocle path and `sub_1C93F90` (SectorRenderSubmit) is NEVER called, so rooms don't render.

**Solution:** return 1 WITHOUT flags. The VM takes the normal path and the full render pipeline stays active.

## Current Status

- [x] dinput8.dll proxy -- WORKING
- [x] EndScene hook -- WORKING (uses game's real D3D device)
- [x] On-screen status message -- WORKING
- [x] Font rendering -- WORKING
- [x] FreeCam hooks (Hook 1/2/3) -- IMPLEMENTED, needs testing
- [x] AABB sector tracking -- IMPLEMENTED, needs testing
- [x] Menu system -- IMPLEMENTED, INPUT NOT WORKING (GetAsyncKeyState may not see arrow/Enter keys)
- [ ] Render ALL patches (frustum zero, cap raise, IsSectorLoaded) -- NOT YET PORTED
- [ ] HUD hide / DebugCam input blocking -- NOT YET PORTED
- [ ] Menu input fix -- needs debug (red debug line shows key states)

## Known Issues

1. **Menu input:** INSERT works for toggle but arrow keys / Enter / Numpad don't respond. Debug line added to show GetAsyncKeyState results -- needs testing to determine if keys are invisible or if edge detection logic is broken.
2. **Alt-tab crash:** Game crashes on alt-tab (existing game bug, not mod-related).
3. **Render ALL:** All rooms visible at all angles not yet ported from CE scripts.

## CE Script Equivalents (for reference)

The CE scripts in `../freecam/` have working implementations:

- `freecam_core.cea` -- base freecam (Hook 1/2/3 + Lua movement)
- `freecam_sector_aabb.cea` -- AABB sector lookup + camera position write
- `freecam_render_all.cea` -- 3 patches: frustum zero, cap 5->16, IsSectorLoaded=true
