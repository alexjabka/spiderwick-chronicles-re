# SpiderMod -- 3D Debug Overlay System
**Module:** `menu.cpp` (rendering) + `dllmain.cpp` (data capture)
**Status:** Implemented and working

---

## Overview

The 3D debug overlay renders world-space labels and wireframes on top of the game's 3D scene. It projects character names, sector bounding boxes, coordinates, and memory addresses from world space to screen space using the engine's own ViewProjection matrix, then draws them with ImGui's background draw list.

The system has two halves:
1. **Data capture** -- runs in the CameraSectorUpdate hook (game update context)
2. **Rendering** -- runs in the EndScene hook (D3D render context)

---

## Data Capture (dllmain.cpp :: HookedCameraSectorUpdate)

Each frame, the CameraSectorUpdate hook reads projection data from the CameraRender object:

```cpp
uintptr_t camObj = mem::Deref(addr::pCameraObj);  // [0x72F670]
if (camObj) {
    // VP matrix: 16 floats at CameraRender+0x448
    for (int i = 0; i < 16; i++)
        g_vpMatrix[i] = mem::Read<float>(camObj + 0x448 + i * 4);

    // Viewport: short width/height at CameraRender+0x6C8/0x6CA
    g_vpWidth  = mem::Read<short>(camObj + 0x6C8);
    g_vpHeight = mem::Read<short>(camObj + 0x6CA);

    g_matricesValid = (g_vpWidth > 0 && g_vpHeight > 0);
}
```

### Why CameraSectorUpdate?

The VP matrix and viewport must be read from the game update thread, not the EndScene render thread. CameraSectorUpdate (0x488DD0) runs once per game tick after the camera has been positioned and matrices rebuilt. By reading here, the data is guaranteed fresh and consistent.

### Global Variables

| Variable | Type | Source file | Description |
|----------|------|-------------|-------------|
| `g_vpMatrix[16]` | float[16] | dllmain.cpp | ViewProjection matrix (View * GlobalScale * Proj) |
| `g_vpWidth` | short | dllmain.cpp | Viewport width in pixels |
| `g_vpHeight` | short | dllmain.cpp | Viewport height in pixels |
| `g_matricesValid` | bool | dllmain.cpp | True if viewport dimensions are positive |

---

## WorldToScreen Reimplementation (menu.cpp)

SpiderMod reimplements the engine's `CameraRender__WorldToScreen` (sub_52A150) to avoid calling into the Kallis VM from the render thread.

```cpp
static bool WorldToScreen(float wx, float wy, float wz, float& sx, float& sy) {
    if (!g_matricesValid || g_vpWidth <= 0 || g_vpHeight <= 0) return false;

    const float* M = g_vpMatrix;

    // vec4 = worldPos * VP matrix (row-major multiply)
    float cx = wx*M[0] + wy*M[4] + wz*M[8]  + M[12];
    float cy = wx*M[1] + wy*M[5] + wz*M[9]  + M[13];
    float cz = wx*M[2] + wy*M[6] + wz*M[10] + M[14];
    float cw = wx*M[3] + wy*M[7] + wz*M[11] + M[15];

    if (cw <= 0.001f) return false;  // behind camera

    // Perspective divide -> NDC
    float ndx = cx / cw;
    float ndy = cy / cw;
    float ndz = cz / cw;

    // Visibility bounds
    if (ndz < 0.0f || ndz > 1.0f) return false;
    if (ndx < -1.5f || ndx > 1.5f || ndy < -1.5f || ndy > 1.5f) return false;

    // NDC to screen pixels (Y flipped: NDC +Y up, screen +Y down)
    sx = (ndx + 1.0f) * 0.5f * (float)g_vpWidth;
    sy = (1.0f - ndy) * 0.5f * (float)g_vpHeight;
    return true;
}
```

The NDC bounds are relaxed to +/-1.5 (beyond the +/-1.0 screen edges) to allow labels near screen borders to remain partially visible.

---

## Character Labels

### Data Source

Characters are read from the engine's linked list:
- **Head pointer:** `g_CharacterListHead` at 0x7307D8
- **Next pointer:** character_obj + 0x5A0
- **Position:** character_obj + 0x68 (X), + 0x6C (Y), + 0x70 (Z)
- **Safety cap:** 64 iterations maximum

### Identification

Characters are identified using a three-tier lookup:

1. **Widget hash lookup** -- Read widget pointer at char+0x1C0, read hash at widget+0x24, compare against a pre-computed name table (Jared, Mallory, Simon, ThimbleTack, Goblin, etc. -- 30+ entries hashed at startup via `HashString` at 0x405380)

2. **RTTI fallback** -- If no widget, read the vtable pointer, follow MSVC RTTI chain: `vtable[-1]` -> Complete Object Locator -> type_descriptor at +12 -> name at +8, strip `.?AV` prefix and `@@` suffix

3. **Hash stub** -- If both fail, display truncated hash as `NPC#XXXX`

### Label Rendering

Labels are drawn at `origin + Z_OFFSET` (2.0 units above character origin) using `DrawText3D()`, which calls `WorldToScreen()` then `ImDrawList::AddText()`. A filled circle (3px radius) is drawn at the exact origin position.

### Label Content

The label content changes based on toggle options:

| Coords | Addrs | Label Format |
|--------|-------|-------------|
| off | off | `Name` |
| on | off | `Name\n(X,Y,Z)` |
| off | on | `Name [0xAddr]` |
| on | on | `Name\n(X,Y,Z)\n[0xAddr]` |

---

## Sector AABB Wireframes

### Data Source

Sector data is read from the WorldState structure:
- **WorldState pointer:** `g_WorldState` at 0xE416C4
- **Sector array:** WorldState+0x64 (pointer to array of sector_data pointers)
- **Sector count:** 0xE416C8
- **Sector name:** sector_data+0x00 (null-terminated string, e.g. "Kitchen")

### AABB Layout

Each sector has an axis-aligned bounding box stored as two vec4 values:

| Offset | Type | Description |
|--------|------|-------------|
| sector_data+0x10 | float[4] | AABB min (X, Y, Z, 1.0) |
| sector_data+0x20 | float[4] | AABB max (X, Y, Z, 1.0) |

### Wireframe Rendering

1. Compute 8 corners from min/max:
   ```
   {minX,minY,minZ}, {maxX,minY,minZ}, {maxX,maxY,minZ}, {minX,maxY,minZ},
   {minX,minY,maxZ}, {maxX,minY,maxZ}, {maxX,maxY,maxZ}, {minX,maxY,maxZ}
   ```

2. Project all 8 corners through `WorldToScreen()`

3. Draw 12 edges connecting the corners (4 bottom, 4 top, 4 vertical):
   ```
   Bottom: 0-1, 1-2, 2-3, 3-0
   Top:    4-5, 5-6, 6-7, 7-4
   Pillars: 0-4, 1-5, 2-6, 3-7
   ```

4. Only draw edges where both endpoints projected successfully

5. A text label is drawn at the AABB center with sector index and name

### Sector Load State

Load state is read from `g_SectorStateArray` at 0x133FEF8:
- `state[sector * 12] == 3` means loaded

---

## Color Coding

### Character Colors

| Condition | Color | RGBA |
|-----------|-------|------|
| Current player (active) | Green | (80, 255, 80, 230) |
| Other characters | Yellow | (255, 255, 100, 200) |

In the ImGui table (separate from overlay), additional colors are used:
- **Green** -- current player (`*` suffix)
- **Cyan** -- player character (has IsPlayerCharacter flag)
- **Yellow** -- playable character (vtable[20] returns true)
- **White/gray** -- NPC

### Sector Colors

| Condition | Fill Color | Line Color |
|-----------|-----------|------------|
| Camera sector | Blue (100, 200, 255, 220) | Blue (100, 200, 255, 100) |
| Loaded sector | Green (100, 255, 100, 180) | Green (100, 255, 100, 60) |
| Unloaded sector | Red (255, 100, 100, 140) | Red (255, 100, 100, 40) |

Line alpha is intentionally lower than fill/text alpha to keep wireframes subtle.

---

## Toggle Options

Controlled from the "3D Debug" collapsing header in the ImGui menu:

| Option | Variable | Default | Description |
|--------|----------|---------|-------------|
| Enable 3D Overlay | `dbg3d_enabled` | false | Master toggle for the entire overlay |
| Characters | `dbg3d_characters` | true | Show character name labels and origin dots |
| Sectors | `dbg3d_sectors` | true | Show sector AABB wireframes and center labels |
| Coords | `dbg3d_showCoords` | false | Append world coordinates to labels |
| Addrs | `dbg3d_showAddrs` | false | Append memory addresses to character labels |

All toggles are `static bool` in `menu.cpp` and persist for the session (not saved to config).

---

## Rendering Pipeline

```
EndScene hook (d3d render context)
  -> menu::Render()
    -> Draw3DOverlay()                    [menu.cpp:156]
      -> ImGui::GetBackgroundDrawList()   // draws behind all ImGui windows
      -> for each character in linked list:
           WorldToScreen(pos + Z offset)
           AddText() + AddCircleFilled()
      -> for each sector:
           WorldToScreen(8 corners)
           AddLine() for 12 edges
           DrawText3D() at center
```

The background draw list is used so overlay elements appear behind any open ImGui windows (menu, debug panel, etc.) but on top of the game's 3D scene.

---

## Engine Function Reference

| Address | Name | Used For |
|---------|------|----------|
| 0x52A150 | CameraRender__WorldToScreen | Engine's own W2S (VM-protected, reimplemented) |
| 0x44F890 | GetPlayerCharacter | Current player pointer (for green highlight) |
| 0x405380 | HashString | Pre-compute name hashes for character identification |
| 0x488DD0 | CameraSectorUpdate | Hooked for VP matrix + viewport capture |

---

## Related Documentation

- [../../../engine/camera/subs/sub_52A150_CameraRender_WorldToScreen.md](../../../engine/camera/subs/sub_52A150_CameraRender_WorldToScreen.md) -- Engine W2S function
- [../../../engine/camera/WORLD_TO_SCREEN.md](../../../engine/camera/WORLD_TO_SCREEN.md) -- Full projection pipeline
- [CAMERA_OVERRIDE.md](CAMERA_OVERRIDE.md) -- Camera parameter override system
- [HOT_SWITCH.md](HOT_SWITCH.md) -- Character switching system
