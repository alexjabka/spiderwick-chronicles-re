# sub_459070 — CharacterDeath

**Address:** `0x459070` (Spiderwick.exe+59070)
**Convention:** thiscall (ECX = this = ClCharacterObj)
**Args:** `(char a3)` — death type/variant

## Purpose

Character death/reset handler. Zeroes velocity, resets animation state,
clears combat, and sets death flags.

## Decompiled (Hex-Rays)

```c
int __thiscall CharacterDeath(float *this, char deathType)
{
  if ( !deathType )
  {
    if ( sub_457850() )
      sub_4578B0(this);
    if ( !sub_454CE0(this) )
    {
      int combatObj = *((_DWORD *)this + 206);  // +0x338
      if ( combatObj )
      {
        // Reset combat controller
        int *v5 = (int *)(combatObj + 48);
        (*(void (**)(int*, int, int))(*v5 + 24))(v5, sub_49CAA0(0), ebp);
        (*(void (**)(int*, int))(*v5 + 8))(v5, 0);
      }
    }
  }
  (*(void (**)(float *))(*(_DWORD *)this + 204))(this);   // vtable[51] cleanup
  (*(void (**)(float *))(*(_DWORD *)this + 208))(this);   // vtable[52] reset
  sub_452900(this);
  sub_4521A0(this);
  (*(void (**)(float *, int, int))(*(_DWORD *)this + 100))(this, 0, -1);  // vtable[25]

  int *aiCtrl = (int *)*((_DWORD *)this + 123);  // +0x1EC
  if ( aiCtrl )
    // Reset AI controller ...

  int controller = *((_DWORD *)this + 110);  // +0x1B8
  if ( controller )
    (*(void (**)(int))(*(_DWORD *)controller + 72))(controller);

  this[45] = 0.0;   // +0xB4
  this[44] = 0.0;   // +0xB0
  this[53] = 0.0;   // +0xD4
  *((_DWORD *)this + 114) |= 2u;              // +0x1C8 death flag
  *((_DWORD *)this + 115) ^= ... & 0x200;     // +0x1CC death variant bit 9

  if ( sub_51B030(12) )
    *((_DWORD *)this + 115) |= 0x400000u;     // +0x1CC sector flag
  else
    *((_DWORD *)this + 115) &= ~0x400000u;

  this[365] = 0.0;  // +0x5B4
  this[275] = 0.0;  // +0x44C direction X
  this[276] = 0.0;  // +0x450 direction Y
  this[277] = 0.0;  // +0x454 direction Z
}
```

## Key Offsets Zeroed

| Offset | Type | Description |
|--------|------|-------------|
| +0xB0 | float | velocity/anim |
| +0xB4 | float | velocity/anim |
| +0xD4 | float | anim value |
| +0x44C | float | direction X |
| +0x450 | float | direction Y |
| +0x454 | float | direction Z |
| +0x5B4 | float | ClPlayerObj-specific |
