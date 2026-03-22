# sub_4E02D0 -- EngineConsole (Debug Printf)

## Address
`0x004E02D0`

## Signature
```c
int sub_4E02D0(const char* fmt, ...);
```

## Purpose
Engine's debug console printf function. Formats a message string and writes it into a ring buffer for on-screen display. Supports inline color codes for colored text output. Used extensively throughout the engine by the world loader, script system, asset pipeline, and other subsystems for debug logging.

## Ring Buffer Layout

| Address | Name | Type | Description |
|---------|------|------|-------------|
| `byte_D5CB98` | ConsoleRingBuffer | `char[256][256]` | 256 lines, each 256 characters max. Total 64KB. |
| `dword_D6CDA4` | WriteHead | `DWORD` | Current write position (line index) in the ring buffer. Wraps at 256. |
| `dword_D6CD9C` | DisplayStart | `DWORD` | First line to display on screen. Used for scrolling. |

## Color Codes

Inline color codes are embedded in the format string using `{x}` syntax:

| Code | Color |
|------|-------|
| `{g}` | Green |
| `{r}` | Red |
| `{y}` | Yellow |
| `{v}` | Violet |
| `{o}` | Orange |

Example usage:
```c
sub_4E02D0("{g}Loaded sector: {y}%s {g}in %.2fs", sectorName, loadTime);
```

## Callers
Called extensively throughout the engine:
- World/sector loader -- logs sector load/unload events
- Script system (Kallis VM) -- script debug output
- Asset pipeline -- reports asset load status
- Error handling -- logs warnings and errors
- Various initialization subsystems

## Callees
- `vsprintf` or equivalent -- formats the variadic arguments into the ring buffer slot.

## Notes
- The ring buffer holds 256 lines. Once full, the write head wraps around and overwrites the oldest entries.
- Each line is capped at 256 characters (including null terminator). Longer messages will be truncated.
- The display start index (`dword_D6CD9C`) can be adjusted to scroll through console history.
- Color codes are parsed at render time, not at write time -- the raw `{x}` tags are stored in the buffer.
- This is a passive logging system -- it does not halt execution or require acknowledgment. Messages are simply written and rendered on the next frame if the console overlay is visible.
