# Using strings_full.txt for Reverse Engineering

**File:** `strings_full.txt` — all strings extracted from Spiderwick.exe via IDA (Shift+F12)
**Count:** ~19,000 strings

## How to Search

The file format is:
```
line:.section:address    length    type    string_content
```

Example:
```
2085:.rdata:00630430    00000013    C    sauSetCameraSector
```
- Address `00630430` in `.rdata` section
- String length 0x13 (19 bytes)
- C string type
- Content: `sauSetCameraSector`

## Naming Conventions in This Engine

The engine uses prefixes for its scripting/API functions:

| Prefix | Meaning | Examples |
|--------|---------|---------|
| `sau` | Script/API utility function | `sauSetSector`, `sauIsVisible` |
| `Cl` | Class (game object type) | `ClWorld`, `ClInvisibleWeaponObj` |
| `INP_` | Input binding name | `INP_CAMERA_BEHIND_PLAYER` |
| `.?AV` | MSVC RTTI class name | `.?AVClSetSectorIndexVisitor@@` |
| `flt_` / `dword_` | IDA auto-named globals | (not in strings, but in IDA) |

## Workflow: Finding a System

### Step 1: Search by keywords
```bash
grep -i "keyword" strings_full.txt
```
Good keywords by engine system:

| System | Keywords to search |
|--------|--------------------|
| Room/Zone visibility | `sector`, `portal`, `room`, `visible`, `cull` |
| Camera | `camera`, `cam`, `look`, `orbit`, `zoom` |
| Rendering | `draw`, `render`, `shader`, `texture`, `mesh` |
| Input | `INP_`, `input`, `mouse`, `key`, `button` |
| Audio | `sound`, `audio`, `music`, `sfx` |
| Physics | `collision`, `physics`, `raycast`, `hit` |
| Animation | `anim`, `skeleton`, `bone`, `blend` |
| AI/NPC | `brain`, `patrol`, `wander`, `attack`, `idle` |
| UI/HUD | `hud`, `menu`, `widget`, `health`, `timer` |
| World/Level | `world`, `level`, `load`, `spawn`, `trigger` |

### Step 2: Find address in IDA
From the grep result, take the address (e.g., `00630430`) and go to it in IDA:
- Jump → Jump to address (G) → paste address

### Step 3: Find who uses this string (xrefs)
- Click on the string in IDA
- Press **X** to see cross-references
- This shows which functions reference this string
- Usually leads to: registration function, debug print, or error handler

### Step 4: Decompile the using function
- Go to the xref (double-click)
- Press **F5** to decompile
- The string is often used as a function name registration:
```c
// Common pattern: scripting API registration
registerFunction("sauSetCameraSector", sub_XXXXXX);
```
- `sub_XXXXXX` is the actual implementation

## Key Findings So Far

### Camera System
```
sauSetCameraSector     00630430  — binds camera to a sector
sauSetPlayerSector     00631E74  — binds player to a sector
sauMarkAsVisibleByPlayer 00629560 — marks sector visible
sauHideSector          006410F4  — hides a sector
sauIsVisible           006321A0  — checks visibility
sauIsSectorLoaded      006410E0  — checks if sector loaded
sauSetPortalActive     006411C0  — activates portal between sectors
draw_distance          00639F58  — draw distance parameter
```

### Sector System (from debug strings)
```
"Sector Changed: was %d now %d\n"           — sector transition logging
"ClWorld: Set initial sector to \"%s\"\n"    — world init
"ClWorld: Sector:\t{g%s} (loaded in %.1fs)\n" — sector load timing
"ClWorld: Sectoring:\t%d sectors, %d portals\n" — sector/portal counts
"%d total number of ticking objects in inactive sectors.\n" — inactive sector objects
"ClWorld: Sector \"%s\" already loaded\n"    — duplicate load guard
"ClWorld: Loading sector \"%s\"\n"           — sector loading
```

### RTTI Classes
```
.?AVClSetSectorIndexVisitor@@       — sector index visitor
.?AUDrawDistance_controller@...@@   — draw distance controller class
.?AVClRemoveWhenNotVisibleService@@ — remove-when-not-visible service
```

## Tips

- **RTTI names** (`.?AV...@@`) reveal class hierarchy. Search for these to find vtables.
- **Debug format strings** (`%d`, `%s`, `\n`) indicate logging/debug functions — often contain useful parameter names.
- **`sau` functions** are the game's scripting API. Each one wraps an engine function. Following xrefs from the string to the registration call reveals the actual implementation.
- **Error strings** (`ERROR_SECTOR_NOT_FOUND`) show what can go wrong — useful for understanding expected state.
