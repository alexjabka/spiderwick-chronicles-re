# sub_463BF0 — ClPlayerObj_Constructor

**Address:** `0x463BF0` (Spiderwick.exe+63BF0)
**Convention:** thiscall (ECX = this)
**Returns:** this

## Purpose

Constructor for ClPlayerObj. Calls parent ClCharacterObj constructor (sub_454060),
then initializes ClPlayerObj-specific fields (+0x5D0 to +0x630).

## Initialized Fields

| Offset | Hex | Value | Description |
|--------|-----|-------|-------------|
| +0x00 | 0x00 | 0x62B9EC | ClPlayerObj vtable |
| +0xAC | 0xAC | 0x62B9E0 | Secondary vtable |
| +0x0C | 0x0C | off_6E2C58 | → "ClPlayerObj" string |
| +0x5D0 | 0x5D0 | 0 (byte) | Player type flag |
| +0x5D8 | 0x5D8 | 0 | |
| +0x5DC-0x620 | | 0 | Zeroed array (10 dwords) |
| +0x628 | 0x628 | 0 (byte) | |
| +0x62C | 0x62C | -1 | Sentinel/invalid ID |
| +0x630 | 0x630 | 0.0 | |

## Decompiled (Hex-Rays)

```c
_BYTE *__thiscall ClPlayerObj_Constructor(_BYTE *this)
{
  sub_454060();  // parent ClCharacterObj constructor
  *(this + 1488) = 0;                          // +0x5D0 byte
  *((_DWORD *)this + 374) = 0;                 // +0x5D8
  *(_DWORD *)this = &ClPlayerObj::`vftable';   // +0x00
  *((_DWORD *)this + 43) = &ClPlayerObj::`vftable'; // +0xAC secondary
  *((_DWORD *)this + 375) = 0;                 // +0x5DC
  // ... zeroes +0x5E0 through +0x620 ...
  *((float *)this + 396) = 0.0;               // +0x630
  *(this + 1576) = 0;                          // +0x628 byte
  *((_DWORD *)this + 395) = -1;               // +0x62C
  *((_DWORD *)this + 3) = off_6E2C58;         // +0x0C = "ClPlayerObj"
  return this;
}
```

## Inheritance Chain

```
sub_52ED80 (base entity) -- via kallis
  +-- sub_454060 (ClCharacterObj)
        +-- sub_463BF0 (ClPlayerObj)
```

## Key Details

- **Size 1592 bytes** -- extends ClCharacterObj (1488 bytes) with 104 bytes of player-specific data
- Parent constructor `sub_454060` is called first, which chains through `sub_52ED80` (base entity)
- Sets **two vtables**: primary at +0x00, secondary at +0xAC (both point to `ClPlayerObj::vftable`)
- RTTI string pointer at +0x0C is set to `off_6E2C58` which points to `"ClPlayerObj"`
- Player-specific fields (+0x5D0 to +0x630) are all zeroed, with sentinel value -1 at +0x62C

## Called By

- `ClPlayerObj_Factory` (sub_4D6110) -- factory function in the spawn pipeline
- `ClThimbletackObj_Constructor` (sub_4659F3) -- derived class

## Related

- [sub_4D6110_ClPlayerObj_Factory.md](sub_4D6110_ClPlayerObj_Factory.md) -- factory (caller)
- [sub_4DE530_GamePoolAllocator.md](sub_4DE530_GamePoolAllocator.md) -- allocator
- [sub_44FA20_CreateCharacter_Internal.md](sub_44FA20_CreateCharacter_Internal.md) -- creation pipeline
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Full spawn pipeline overview
