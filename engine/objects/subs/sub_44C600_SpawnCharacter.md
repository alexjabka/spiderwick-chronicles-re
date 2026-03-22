# sub_44C600 -- SpawnCharacter

## Identity

| Field | Value |
|---|---|
| Address | `0x44C600` |
| Calling Convention | `__thiscall` |
| this | Spawner object (ClSpawnerObj or equivalent) |
| Parameters | `int assetRef, float* pos, float* rot` |
| Return Value | `int` -- character object pointer |
| Size | 181 bytes |
| Cyclomatic Complexity | 3 |
| Module | engine/objects |

## Purpose

The actual spawn function. Called by both `sauSpawnObj` (sub_44C730) and `sauCreateCharacter` (sub_44C6C0). Creates a character from an asset reference, positions it, and registers it with the render/game systems via vtable calls.

## Decompiled (IDA)

```c
int __thiscall sub_44C600(int *this, int a2, int a3, int a4)
{
  int *Character_Internal;   // esi -- the newly created character
  void (__thiscall *v6)(int *, int);  // edx -- vtable[1] (SetPosition)
  int v7;   // eax
  int v8;   // ebx
  int v9;   // eax
  int v10;  // esi
  int v11;  // eax
  int v13;  // [esp+8h] [ebp-14h]
  int v14;  // [esp+Ch] [ebp-10h]

  Character_Internal = (int *)CreateCharacter_Internal(a2);       // sub_44FA20
  v6 = *(void (__thiscall **)(int *, int))(*Character_Internal + 4);  // vtable[1]
  Character_Internal[139] = (int)this;                             // store spawner backref at +0x22C
  v6(Character_Internal, a3);                                      // vtable[1](pos) = SetPosition
  sub_51AE30(a4);                                                  // SetRotation(rot)
  (*(void (__thiscall **)(int *))(*Character_Internal + 8))(Character_Internal);  // vtable[2]() = Activate
  (*(void (__thiscall **)(int *, _DWORD))(*Character_Internal + 40))             // vtable[10](spawner[9])
    (Character_Internal, *(this + 9));

  // Optional animation setup (if spawner has animation data)
  v7 = *(this + 44);                                              // spawner[44] = anim data?
  if ( v7 )
  {
    v8 = Character_Internal[110];                                  // character[110] = anim controller?
    v9 = sub_5360F0(v7, (int)(Character_Internal + 26),
                    (int)(Character_Internal + 30), Character_Internal[9], -1.0, 0);
    v10 = sub_535D60(v9);
    if ( v10 )
    {
      v11 = sub_5274D0(v8, *(this + 45), 0);
      (*(void (__thiscall **)(int, _DWORD, int, int, int))(*(_DWORD *)v10 + 24))(
        v10, *(_DWORD *)(v8 + 88), v11, v13, v14);
    }
  }
  return VMReturnBool(1);
}
```

## Spawn Sequence

| Step | Code | Purpose |
|------|------|---------|
| 1 | `CreateCharacter_Internal(assetRef)` | Allocate from pool, construct via factory, link VM |
| 2 | `character[139] = this` | Store spawner backref at offset +0x22C |
| 3 | `vtable[1](character, pos)` | SetPosition -- place in world |
| 4 | `sub_51AE30(rot)` | SetRotation -- orient in world |
| 5 | `vtable[2](character)` | **Activate** -- render system registration |
| 6 | `vtable[10](character, spawner[9])` | Setup with spawner data |
| 7 | (optional) animation setup | If spawner[44] non-null, configure animation |

## Key Details

- **vtable[2] (Activate)** is the critical call for making the character visible. Without it, the character exists in the linked list but has no visual representation. This explains why factory-only spawns and memcpy clones are invisible.
- `character[139]` (offset +0x22C) stores a back-reference to the spawner object
- `spawner[9]` (offset +0x24) is passed to the character's vtable[10] setup function
- The optional animation branch at the end handles spawners that have associated animation data

## Called By

| Address | Name |
|---------|------|
| `0x44C70D` | sauCreateCharacter (sub_44C6C0) |
| `0x44C768` | sauSpawnObj (sub_44C730) |

## Calls

| Address | Name | Purpose |
|---------|------|---------|
| `0x44FA20` | CreateCharacter_Internal | Allocate + construct + VMInit |
| `0x51AE30` | SetRotation | Set rotation from vec3 |
| `0x5360F0` | (animation) | Animation resource lookup |
| `0x535D60` | (animation) | Animation controller |
| `0x5274D0` | (animation) | Animation binding |

## Related

- [sub_44C730_sauSpawnObj.md](sub_44C730_sauSpawnObj.md) -- sauSpawnObj handler (caller)
- [sub_44C6C0_sauCreateCharacter.md](sub_44C6C0_sauCreateCharacter.md) -- sauCreateCharacter (caller)
- [sub_44FA20_CreateCharacter_Internal.md](sub_44FA20_CreateCharacter_Internal.md) -- CreateCharacter_Internal
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Full spawn pipeline overview
- [../CHARACTER_CREATION.md](../CHARACTER_CREATION.md) -- Character creation overview
