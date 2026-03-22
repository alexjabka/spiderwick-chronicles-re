SpiderMod — Spiderwick Chronicles ASI Mod

SETUP:
  1. Copy spidermod.asi and dinput8.dll to the game directory
     (same folder as Spiderwick.exe)
  2. Launch the game normally
  3. Press INSERT for the debug menu

FEATURES:
  - Freecam (WASD + mouse, toggle with HOME)
  - 3D debug overlay (characters, sectors, props with coordinates)
  - Entity spawning and character hot-switching
  - Force All Visible (shows chapter-gated enemies)
  - Chapter setter (quick Ch1-Ch8 buttons)
  - Scene dump (geometry instances + VM placements)
  - Prop positions JSON dump (for SpiderView prop placement)
  - Portal bypass (see all rooms simultaneously)
  - Launch.txt config redirect

BUILD FROM SOURCE:
  cd mods/spidermod
  cmake -B build
  cmake --build build --config Release
  Requires: CMake 3.15+, MSVC 2019+, Windows SDK
  Dependencies: MinHook (auto-downloaded), Dear ImGui (auto-downloaded)
