# World Geometry Instances & Multi-Layer Loading

**Status:** Discovered 2026-03-19 from engine console output
**Key Finding:** Levels load MULTIPLE world files with overlapping geometry. Only registered "geometry instances" are rendered.

---

## Discovery: Engine Console Output (GroundsD Level Load)

```
ClWorld: Static load "Worlds/grounds2.pcw"...
ClWorld: File:       Worlds/grounds2.pcw
ClWorld: Version:    PCW v10 (4K alignment, 4K header)
ClWorld: Sizes:      File 42272K (11188K static, 31084K streaming)
ClWorld: Streaming:  1 chunks, 31092K chunk size, 1 concurrent chunks maximum
ClWorld: Streaming:  42280K effective file size
ClWorld: Static:     1 fogs, 1 skyboxes, 5 dummies
ClWorld: Static:     10 props, 10 prop definitions (1 collision materials)
ClWorld: Sectoring:  2 sectors, 0 portals

ClWorld: Set initial sector to "Default"
ClWorld: Loading sector "Default"
ClWorld: Loading sector "SecA_1stRd"

ClWorld: Sector:     SecA_1stRd (loaded in 0.0s)
ClWorld: Sizes:      1 chunks (31092K)
ClWorld: Counts:     0 geometry instances, 0 hanging edges

ClWorld: Sector:     Default (loaded in 0.0s)
ClWorld: Sizes:      1 chunks (31092K)
ClWorld: Counts:     487 geometry instances, 0 hanging edges
```

## Key Facts

### Multi-Layer World Loading
1. The game loads MULTIPLE world files for a single level
2. First world loaded: has "Rooftop" + "Default" sectors (484 geom instances)
3. Then `GroundsD.zwd` loads → triggers `Worlds/grounds2.pcw`
4. Second world: "Default" (487 instances) + "SecA_1stRd" (0 instances)

### Geometry Instances
- **487 geometry instances** in the "Default" sector = the ACTUAL rendered chunk count
- The PCWB file contains **2346 PCRDs total** — far more than rendered
- The excess PCRDs (~1859) are from:
  - Alternate world versions/layers
  - Unloaded sectors (SecA_1stRd has 0 instances)
  - Different game states (open/closed door variants)
- The engine's geometry instance list IS the definitive set of what to render

### PCWB File Structure
- `grounds2.pcw` → 42MB PCW file containing PCWB world data
- Static portion: 11MB (header, prop table, sector defs, texture refs)
- Streaming portion: 31MB (geometry data, textures — loaded on demand)
- 2 sectors: "Default" (all gameplay geometry) + "SecA_1stRd" (empty, purpose unknown)
- 10 props with 10 definitions → 340 prop PCRDs (via mesh chain)
- 2006 non-prop PCRDs (world geometry, many are alternate states)

### Sector Structure
| Sector | Geometry Instances | Purpose |
|--------|-------------------|---------|
| Default | 487 | Main gameplay geometry |
| SecA_1stRd | 0 | Unknown (empty, loaded first) |
| Rooftop | 0 | First world layer (separate load) |

### Implications for Export
1. **Only 487 PCRDs should be exported** (geometry instances), not all 2346
2. The engine has an internal list of which 487 PCRDs to render
3. This list includes: world terrain + building surfaces + visible props
4. Underground duplicate PCRDs are alternate states NOT in the instance list
5. Finding this instance list in engine memory = perfect export with no duplicates

## Next Steps
1. Find where the engine stores the 487 geometry instance entries
2. Each instance likely contains: PCRD reference + World transform matrix
3. Hook the instance registration (`Registering geometry instances... Ok`) to capture all entries
4. Use instance data for perfect world export — engine's own render set with transforms

## Related Engine Functions
- World loading: `ClWorld::StaticLoad`, `ClWorld::LoadSector`
- Instance registration: `Registering geometry instances` (log line from engine)
- Sector management: `ClWorld::SetInitialSector`, `ClLevel::Stabilize`
- Streaming: `SfAssetRepository::Loading`, `StreamingIO_CalcAddress`

## Console Strings to Search in Binary
- `"Registering geometry instances"` → finds the registration function
- `"geometry instances"` → finds the instance count/registration
- `"Counts:"` → finds the sector loading summary
- `"ClWorld"` → finds all world management functions
