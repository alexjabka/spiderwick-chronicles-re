# sub_462B80 --- CommitPlayerState

**Address:** 0x462B80 (Spiderwick.exe+62B80) | **Calling convention:** __thiscall (ECX = ClPlayerObj*)

**vtable[113]** for ClPlayerObj (offset 452 from vtable base)

---

## Purpose

Called after `SetPlayerType` (vtable[116]) completes. Restores the switched-to character's health from the data store, sets the input mode (checking for minigame context), and loads weapon instances (melee, ranged, butterfly net) from per-character data store paths. Finally clears the player type flag at `this+0x5D0`.

---

## Parameters

| Name | Type | Description |
|------|------|-------------|
| `this` (ECX) | ClPlayerObj* | The active player character after switching |

**Returns:** int (last weapon creation result, or 0)

---

## Decompiled Pseudocode

```c
int __thiscall CommitPlayerState(ClPlayerObj *this)
{
    const char *charName = (const char *)(this->widgetDesc->nameStr);  // *(this+112) + 4

    // === HEALTH RESTORATION ===
    char buf[256];
    snprintf(buf, 256, "/Player/%s/Health", charName);      // sub_5AA780

    int checkResult = sub_42B940(byte_E57F68);
    if (sub_42BC20(checkResult))                             // minigame/special mode check
    {
        this->curHealth = 125;                               // *(this+126) = hardcoded
        this->maxHealth = 125;                               // *(this+127) = hardcoded
    }
    else
    {
        int dsIndex = HashLookup(buf);                       // sub_41E830
        int health;
        if (dsIndex > -1)
            health = *DataStoreGet(byte_E57F68, dsIndex);    // sub_5392C0
        else
            health = 0;
        this->curHealth = health;                            // *(this+126) = +0x1F8

        int maxIdx = HashLookup("/Player/MaxHealth");
        int maxHealth;
        if (maxIdx > -1)
            maxHealth = *DataStoreGet(byte_E57F68, maxIdx);
        else
            maxHealth = 0;
        this->maxHealth = maxHealth;                         // *(this+127) = +0x1FC
    }

    // === INPUT MODE ===
    int checkResult2 = sub_42B940(byte_E57F68);
    if (sub_42BC20(checkResult2))                            // in minigame context
    {
        int inputMode = sub_408240(2, "INP_MINIGAME");
        sub_452620(5, inputMode);                            // set input mode

        // Check session type for sprites-only minigame
        if ((dword_730EE0 & 1) == 0)
        {
            dword_730EE0 |= 1;
            dword_730EDC = HashString("/Level/MGArenas/SessionType");
        }
        int sessionIdx = sub_539630(dword_730EDC);
        if (sessionIdx > -1 && *DataStoreGet(byte_E57F68, sessionIdx) == 1)
        {
            int inputMode2 = sub_408240(2, "INP_MINIGAME_SPRITES_ONLY");
            sub_452620(5, inputMode2);
        }
    }

    // === WEAPON LOADING ===
    const char *name = (const char *)(this->widgetDesc->nameStr);

    // Melee weapon
    snprintf(buf, 256, "/Game/Characters/%s/MeleeWeapon", name);
    int meleeIdx = HashLookup(buf);
    int meleeRef = (meleeIdx > -1) ? *sub_5392E0(meleeIdx) : 0;

    // Ranged weapon
    snprintf(buf, 256, "/Game/Characters/%s/RangedWeapon", name);
    int rangedIdx = HashLookup(buf);
    int rangedRef = (rangedIdx > -1) ? *sub_5392E0(rangedIdx) : 0;

    // Butterfly net
    snprintf(buf, 256, "/Game/Characters/%s/ButterflyNet", name);
    int netIdx = HashLookup(buf);
    int netRef = (netIdx > -1) ? *sub_5392E0(netIdx) : 0;

    // Create weapon instances via vtable[68] (offset 272)
    if (meleeRef)
    {
        int asset = sub_42AC10(meleeRef);
        this->vtable[68](this, asset, 0, 0);   // CreateWeaponInstance
    }
    if (rangedRef)
    {
        int asset = sub_42AC10(rangedRef);
        this->vtable[68](this, asset, 0, 0);
    }
    if (netRef)
    {
        int asset = sub_42AC10(netRef);
        this->vtable[68](this, asset, 0, 0);
    }

    // Clear player type flag
    *(BYTE *)(this + 0x5D0) = 0;
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x462BB3` | snprintf for "/Player/%s/Health" |
| `0x462C4E` | sub_408240(2, "INP_MINIGAME") --- input mode lookup |
| `0x462C80` | HashString("/Level/MGArenas/SessionType") |
| `0x462CAE` | sub_408240(2, "INP_MINIGAME_SPRITES_ONLY") |
| `0x462CD9` | snprintf for "/Game/Characters/%s/MeleeWeapon" |
| `0x462D1B` | snprintf for "/Game/Characters/%s/RangedWeapon" |
| `0x462D53` | snprintf for "/Game/Characters/%s/ButterflyNet" |
| `0x462D9B` | vtable[68] call for melee weapon |
| `0x462DB9` | vtable[68] call for ranged weapon |
| `0x462DD7` | vtable[68] call for butterfly net |
| `0x462DE1` | `*(BYTE*)(this + 0x5D0) = 0` --- clear player type flag |

### Data Store Paths

| Path | Content |
|------|---------|
| `/Player/%s/Health` | Per-character health (e.g., `/Player/Jared/Health`) |
| `/Player/MaxHealth` | Global max health |
| `/Game/Characters/%s/MeleeWeapon` | Melee weapon asset reference |
| `/Game/Characters/%s/RangedWeapon` | Ranged weapon asset reference |
| `/Game/Characters/%s/ButterflyNet` | Butterfly net asset reference |
| `/Level/MGArenas/SessionType` | Minigame session type (1 = sprites only) |

### Globals

| Address | Name | Purpose |
|---------|------|---------|
| `0x730EE0` | dword_730EE0 | Lazy-init flag (bit 0 = session type hash computed) |
| `0x730EDC` | dword_730EDC | Cached hash of "/Level/MGArenas/SessionType" |
| `0xE57F68` | byte_E57F68 | Data store object (thiscall target) |

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_4626B0` (sauSetPlayerType) | `vtable[113](this)` after vtable[116] |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x5AA780` | snprintf | Format data store paths |
| `0x42B940` | sub_42B940 | Get mode check context |
| `0x42BC20` | sub_42BC20 | Is minigame/special mode |
| `0x41E830` | HashLookup | Data store key --> index |
| `0x5392C0` | DataStoreGet | Index --> DWORD value pointer |
| `0x5392E0` | sub_5392E0 | Index --> value pointer (variant) |
| `0x408240` | sub_408240 | Lookup input mode by name |
| `0x452620` | sub_452620 | Set input mode |
| `0x405380` | HashString | Hash a string |
| `0x539630` | sub_539630 | Data store hash lookup (cached) |
| `0x42AC10` | sub_42AC10 | Resolve weapon reference to asset |
| vtable[68] | CreateWeaponInstance | Create weapon object on character |

---

## Notes / Caveats

1. **Health is hardcoded to 125 in minigame mode.** The `sub_42BC20` check gates this; in normal gameplay, health is read from the data store.

2. **Weapons are loaded from per-character paths** in the data store, meaning each character has independent weapon state. Mallory has her sword (MeleeWeapon), Simon has the butterfly net (ButterflyNet), Jared has a slingshot (RangedWeapon), etc.

3. **The `sub_5392E0` call** (used for weapon refs) is a variant of DataStoreGet that may return a different pointer type or handle reference resolution differently from `sub_5392C0` (used for health/integers).

4. **`this+0x5D0` is cleared at the end** (set to 0). This byte is set during the ClPlayerObj constructor and is checked by `CharacterInitWeaponsHealth` (sub_4628C0) to determine whether to clamp health to max. Clearing it here indicates the switch is complete.

5. **Input mode "INP_MINIGAME_SPRITES_ONLY"** is only set when the session type equals 1, suggesting a specific minigame variant with simplified controls.
