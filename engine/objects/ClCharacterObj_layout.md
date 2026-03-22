# ClCharacterObj / ClPlayerObj — Object Layout

**Vtables:**
- ClCharacterObj: `0x62A0E4` (base), `0x62A0D8` (secondary at +0xAC)
- ClPlayerObj: `0x62B9EC` (base), `0x62B9E0` (secondary at +0xAC)

**Inheritance:** ClPlayerObj → ClCharacterObj → (base entity via sub_52ED80)

**Object size:** 0x630+ bytes (ClPlayerObj)

## Layout

### Base Entity (+0x00 — +0x27)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 0 | 0x00 | ptr | vtable | constructor |
| 4 | 0x04 | word | flags/type | constructor (0x1D for ProximityService) |
| 8 | 0x08 | int | 0 | constructor |
| 12 | 0x0C | ptr | class name string ptr ("ClPlayerObj") | constructor +0xCA0 |
| 36 | 0x24 | | entity ID / ref? | sub_460050 uses this+9 (DWORD*) |

### Transform (+0x28 — +0x80)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 40 | 0x28 | float[12] | 3x4 rotation matrix | SetTransform (memcpy) |
| 104 | 0x68 | float | **position X** | SetTransform, SectorDistanceCheck |
| 108 | 0x6C | float | **position Y** | SetTransform, SectorDistanceCheck |
| 112 | 0x70 | float | **position Z** | SetTransform, SectorDistanceCheck |
| 120 | 0x78 | float[3] | secondary vector (up/normal?) | SetTransform |

### Animation / Physics (+0xAC — +0xD4)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 172 | 0xAC | ptr | secondary vtable | constructor |
| 176 | 0xB0 | float | velocity/anim value (zeroed on death) | sub_459070 |
| 180 | 0xB4 | float | velocity/anim value (zeroed on death) | sub_459070 |
| 212 | 0xD4 | float | zeroed on death | sub_459070 |

### Sub-objects (+0x134 — +0x1EC)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 308 | 0x134 | ptr | render/mesh object (sub_52F330) | sub_451050 |
| 312 | 0x138 | ptr | animation object (sub_5561A0) | sub_451050 |
| 316 | 0x13C | ptr | physics/collision object (sub_432060) | sub_451050 |
| 360 | 0x168 | | sub-object (passed to sub_559830) | sub_451010 |
| 384 | 0x180 | | sub-object (used with sub_433190) | sub_459070 |
| 440 | 0x1B8 | ptr | AI/controller object (+0x918 = state) | sub_460050, sub_459070 |
| 448 | 0x1C0 | ptr | **name struct** (+4 = char* name) | sub_4628C0, sub_462F00 |

### Flags (+0x1C8 — +0x1CC)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 456 | 0x1C8 | int | **state flags** (bitfield) | sub_451010, sub_460050 |
| | | | bit 1: active/enabled | |
| | | | bit 3: init state | sub_451010 |
| | | | bit 6: some mode | sub_460050 |
| | | | bit 13: animation flag | sub_460050 |
| | | | bit 21: death-related | sub_460050 |
| 460 | 0x1CC | int | **state flags 2** (bitfield) | sub_451010, sub_454750 |
| | | | bits 0-1: held projectile count (0-3) | sub_454750 |
| | | | bit 9: death variant | sub_459070 |
| | | | bit 16-17: some state | sub_460050 |
| | | | bit 22: sector-related | sub_459070 |
| | | | bit 26: init flag | sub_451010 |

### Health (+0x1F8 — +0x1FC)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 504 | 0x1F8 | int | **current health** | sub_4628C0, sub_462F00 |
| 508 | 0x1FC | int | **max health** | sub_4628C0, sub_462F00 |

### Combat (+0x338 — +0x3C8)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 824 | 0x338 | ptr | combat/interaction object | sub_460050 |
| 872 | 0x368 | ptr[6] | **weapon/item slots** | sub_4628C0 |
| 928 | 0x3A0 | ptr[3] | held projectile array | sub_454750 |
| 968 | 0x3C8 | | sub-object (sub_44D160) | sub_4512D0 |

### Movement (+0x44C — +0x454)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 1100 | 0x44C | float | direction/movement X (zeroed on death) | sub_459070 |
| 1104 | 0x450 | float | direction/movement Y | sub_459070 |
| 1108 | 0x454 | float | direction/movement Z | sub_459070 |
| 1148 | 0x47C | int | zeroed on init | sub_451010 |
| 1232 | 0x4D0 | int | zeroed (combat state?) | sub_460050 |

### Character List (+0x5A0)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 1440 | 0x5A0 | ptr | **next character in linked list** | GetPlayerCharacter |

### ClPlayerObj-specific (+0x5B4 — +0x630)

| Offset | Hex | Type | Field | Source |
|--------|-----|------|-------|--------|
| 1460 | 0x5B4 | float | zeroed on death | sub_459070 |
| 1488 | 0x5D0 | byte | **player type flag** | ClPlayerObj_Constructor, sub_462F00 |
| 1496 | 0x5D8 | int | 0 | ClPlayerObj_Constructor |
| 1500-1568 | 0x5DC-0x620 | int[10] | zeroed array | ClPlayerObj_Constructor |
| 1584 | 0x630 | float | 0.0 | ClPlayerObj_Constructor |
| 1576 | 0x628 | byte | 0 | ClPlayerObj_Constructor |
| 1580 | 0x62C | int | -1 (0xFFFFFFFF) | ClPlayerObj_Constructor |

## Pointer Access

**Static chain:** `GetPlayerCharacter()` (sub_44F890) → iterates `[0x7307D8]` linked list

**CE hook:** `pPlayer_CT_entry.cea` — hooks GetPlayerCharacter, saves result to `pPlayer` symbol

**Example CE read:**
```
[pPlayer]       → player object base
[pPlayer]+68    → position X (float)
[pPlayer]+6C    → position Y (float)
[pPlayer]+70    → position Z (float)
[pPlayer]+1F8   → current health (int)
[pPlayer]+1FC   → max health (int)
```

## Characters

| Name | String in engine |
|------|-----------------|
| Jared Grace | "Jared" |
| Simon Grace | "Simon" |
| Mallory Grace | "Mallory" |
| ThimbleTack | "ThimbleTack" |

Character name: `[[pPlayer]+0x1C0]+4` → null-terminated string

## Character Name Strings in Binary

| Address | String | Context |
|---------|--------|---------|
| `0x6230D0` | "Simon" | Character name reference |
| `0x6230F8` | "Jared" | Character name reference |
| `0x629CAC` | "Mallory" | Character name reference |
| `0x629CA0` | "ThimbleTack" | Character name reference |
| `0x62B7E4` | "simon" (lowercase) | Hash/lookup key |
| `0x62B7F4` | "jared" (lowercase) | Hash/lookup key |
| `0x62B7EC` | "mallory" (lowercase) | Hash/lookup key |
| `0x62B414` | "ClCharacterObj" | Class name string |
| `0x62BC40` | "ClPlayerObj" | Class name string |

## Related Documentation

- [CHARACTER_CREATION.md](CHARACTER_CREATION.md) — Character creation pipeline
- [CHARACTER_SWITCHING.md](CHARACTER_SWITCHING.md) — Character switching system
- [subs/sub_493A80_sauResolvePlayer.md](subs/sub_493A80_sauResolvePlayer.md) --- sauResolvePlayer (widget creation + character resolution)
- [../vm/KALLIS_VM.md](../vm/KALLIS_VM.md) — VM stack system (creation args)
