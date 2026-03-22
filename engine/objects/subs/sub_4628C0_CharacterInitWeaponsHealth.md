# sub_4628C0 — CharacterInitWeaponsHealth

**Address:** `0x4628C0` (Spiderwick.exe+628C0)
**Convention:** thiscall (ECX = this = ClCharacterObj/ClPlayerObj)

## Purpose

Initializes weapon slots and health for a character. Iterates 6 weapon/item
slots at `this+0x368`, categorizes each (melee/ranged/butterfly net), and
registers them in the game state system via `/Game/Characters/%s/...` paths.
Then sets health via `/Player/%s/Health` and `/Player/MaxHealth`.

## Decompiled (Hex-Rays)

```c
int __thiscall CharacterInitWeaponsHealth(int this)
{
  const char *charName = (const char *)(*(_DWORD *)(this + 448) + 4); // +0x1C0 -> name
  int *weaponSlots = (int *)(this + 872);  // +0x368, 6 slots
  int count = 6;
  do
  {
    int weapon = *weaponSlots;
    if ( weapon )
    {
      _BYTE *weaponData = *(_BYTE **)(weapon + 172);  // weapon+0xAC
      if ( weaponData[44] )       // isMelee
        snprintf(buf, 256, "/Game/Characters/%s/MeleeWeapon", charName);
      else if ( weaponData[45] )  // isRanged
        snprintf(buf, 256, "/Game/Characters/%s/RangedWeapon", charName);
      else if ( weaponData[46] )  // isNet
        snprintf(buf, 256, "/Game/Characters/%s/ButterflyNet", charName);
      // ... registers weapon hash in game state system ...
    }
    ++weaponSlots;
    --count;
  } while ( count );

  // Health
  snprintf(buf, 256, "/Player/%s/Health", charName);
  int health;
  if ( *(_BYTE *)(this + 1488) )  // +0x5D0 flag
    health = min(*(_DWORD *)(this + 504), *(_DWORD *)(this + 508));  // min(cur, max)
  else
    health = *(_DWORD *)(this + 504);  // +0x1F8 current

  // Write health to game state, update MaxHealth if needed
  SetGameState(hash(buf), health);
  int globalMax = GetGameState(hash("/Player/MaxHealth"));
  if ( globalMax < *(_DWORD *)(this + 508) )  // +0x1FC max
    SetGameState(hash("/Player/MaxHealth"), *(_DWORD *)(this + 508));
}
```

## Key Offsets Accessed

| Offset | Type | Field |
|--------|------|-------|
| +0x1C0 | ptr | name struct (+4 = character name "Jared"/"Mallory"/...) |
| +0x1F8 | int | current health |
| +0x1FC | int | max health |
| +0x368 | ptr[6] | weapon/item slot array |
| +0x5D0 | byte | player type flag (ClPlayerObj specific) |
