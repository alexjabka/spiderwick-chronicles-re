SpiderView v1.1 — Spiderwick Chronicles Game Research Tool

SETUP:
  1. Copy SpiderView.exe and nm40_texmap.json to the game directory
     (same folder as ww/, us/, na/ directories)
  2. Run SpiderView.exe

FEATURES:
  - Browse game archives (WADs), view 3D models, textures, scripts, databases
  - Load world levels (.pcw) with full geometry instancing
  - Multi-material NM40 character viewing with armature overlay
  - Export FBX (characters with skeleton + skin weights)
  - Export OBJ (worlds with textures)
  - Blender-style 3D navigation (MMB orbit, Shift+MMB pan, scroll zoom)
  - Press H for hotkey overlay

CONTROLS:
  MMB Drag          Orbit
  Shift+MMB Drag    Pan
  Scroll Wheel      Zoom
  Shift+`           Fly Mode (WASD+Mouse)
  Numpad 1/3/7      Front/Right/Top view
  Numpad 5          Toggle Ortho/Perspective
  H                 Toggle Help Overlay
  Click             Select object
  F                 Focus selected object

BUILD FROM SOURCE:
  cd tools/spiderview
  cmake -B build
  cmake --build build --config Release
  Requires: CMake 3.20+, MSVC 2019+, Windows SDK
  Dependencies auto-downloaded via FetchContent (raylib, imgui, zlib)
