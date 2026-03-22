# sub_599760 -- DebugDrawSectorInfo

## Address
`0x00599760`

## Signature
```c
char __cdecl DebugDrawSectorInfo(float deltaTime);
```

## Purpose
Native debug HUD that displays streaming system statistics. Shows memory pool usage, sector states, sector names, load times, and memory consumption. This is a developer diagnostic overlay for monitoring the sector streaming pipeline in real time.

## Gate Conditions
The function only executes when BOTH conditions are met:
1. `sub_516940() == 1` -- streaming system is active/initialized.
2. `dword_1345C48 != 0` -- debug sector info display is enabled.

If either condition fails, the function returns immediately.

## Display Elements

### Streaming Pool Memory Bar
- Rendered via `sub_5996C0` (progress bar drawing function).
- Shows current memory usage vs. total streaming pool capacity as a visual bar.
- This sub-function works correctly in retail builds.

### Sector State Table
Displays each sector with its current state:

| State | Meaning |
|-------|---------|
| Drawing | Sector is fully loaded and being rendered |
| Loaded | Sector data is in memory but not currently drawn |
| Loading | Sector is being streamed in from disk |
| Unloading | Sector is being evicted from memory |
| Throttling | Sector load is deferred due to bandwidth/memory limits |

### Per-Sector Details
- Sector name string
- Load time (how long the sector took to stream in)
- Memory usage (how much streaming pool memory the sector occupies)

## Key Globals

| Address | Name | Type | Description |
|---------|------|------|-------------|
| `dword_1345C48` | DebugSectorInfoEnable | `DWORD` | Non-zero enables the sector debug HUD. |

## Text Rendering
- Uses `sub_599360` for text drawing, which is a Kallis script thunk.
- **Requires font system initialization** to function. If the font system is not initialized, text will not render (the progress bar from `sub_5996C0` still works since it uses primitive drawing).

## Callers
- Called from thunk at `0x555F00`.
- **Never registered in retail builds** -- the thunk exists but is not hooked into the main render loop. Must be manually called or patched in to activate.

## Callees
- `sub_516940` -- checks if streaming system is active.
- `sub_5996C0` -- draws the streaming pool memory progress bar.
- `sub_599360` -- Kallis thunk for text rendering (requires font init).
- Various streaming system query functions for sector state/names/times/memory.

## Notes
- This is a fully functional debug overlay that was left in the retail binary but disconnected from the render loop.
- The progress bar (`sub_5996C0`) renders correctly even without font init, making it useful as a standalone diagnostic.
- To activate in a mod, the thunk at `0x555F00` must be called from the render loop, or `DebugDrawSectorInfo` can be called directly with a valid delta time.
- The text rendering dependency on font system init via Kallis means this feature was designed to work within the full engine startup sequence, not as an isolated debug tool.
