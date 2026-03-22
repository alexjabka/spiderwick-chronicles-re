# sub_44C6C0 -- sauCreateCharacter

## Identity

| Field | Value |
|---|---|
| Address | `0x44C6C0` |
| Calling Convention | `__thiscall` |
| this | ClSpawnerObj (world-placed spawner entity) |
| Parameters | none (reads 1 arg from VM stack) |
| Return Value | `BOOL` (pushes result to VM return stack) |
| Module | engine/objects |

## Purpose

VM native method on `ClSpawnerObj`. Called from Kallis scripts when a spawner entity creates a character. Pops only 1 argument (the asset reference) from the VM stack and uses the spawner's own position and rotation to place the spawned character.

## Decompiled (IDA)

```c
BOOL __thiscall sub_44C6C0(int this)
{
  int v3;            // [esp+4h] [ebp-24h] BYREF
  float v4[4];       // [esp+8h] [ebp-20h] BYREF    -- rotation
  float v5[4];       // [esp+18h] [ebp-10h] BYREF   -- position

  VMPopObjRef(&v3);                          // pop asset reference from VM stack
  v5[0] = *(float *)(this + 104);            // spawner pos X (+0x68)
  v5[1] = *(float *)(this + 108);            // spawner pos Y (+0x6C)
  v5[2] = *(float *)(this + 112);            // spawner pos Z (+0x70)
  v4[0] = 0.0;                               // no X rotation
  v4[1] = 0.0;                               // no Y rotation
  v4[2] = *(float *)(this + 128);            // spawner Z/heading rotation (+0x80)
  sub_44C600((int *)this, v3, (int)v5, (int)v4);  // SpawnCharacter
  return VMReturnBool(1);                    // always succeeds
}
```

## Key Details

- `__thiscall` -- `this` is the spawner entity (ClSpawnerObj), a world-placed object with position/rotation
- Only pops 1 argument (asset reference); position comes from spawner's own fields
- Position read from `this+0x68/0x6C/0x70` (standard ClCharacterObj position offsets)
- Rotation: only heading (yaw) from `this+0x80`, pitch and roll zeroed
- Delegates to SpawnCharacter (sub_44C600) with spawner as `this`

## Spawner Field Reads

| Offset | Hex | Type | Value |
|--------|-----|------|-------|
| +104 | +0x68 | float | Position X |
| +108 | +0x6C | float | Position Y |
| +112 | +0x70 | float | Position Z |
| +128 | +0x80 | float | Heading rotation (yaw) |

## Xrefs

- **To this function:** data ref at `0x1C95ABC` (VM dispatch table entry)
- **Calls:** VMPopObjRef, sub_44C600 (SpawnCharacter), VMReturnBool

## Related

- [sub_44C730_sauSpawnObj.md](sub_44C730_sauSpawnObj.md) -- sauSpawnObj (explicit pos/rot variant)
- [sub_44C600_SpawnCharacter.md](sub_44C600_SpawnCharacter.md) -- SpawnCharacter (called by this)
- [sub_44FA20_CreateCharacter_Internal.md](sub_44FA20_CreateCharacter_Internal.md) -- CreateCharacter_Internal
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Full spawn pipeline overview
