# sub_44C730 — sauSpawnObj Handler

## Identity

| Field | Value |
|---|---|
| Address | `0x44C730` |
| Calling Convention | `cdecl` |
| Parameters | none (reads from VM stack) |
| Return Value | `void` (pushes result to VM return stack) |
| Module | engine/objects |

## Purpose

Native handler for the `sauSpawnObj` Kallis script function. This is NOT a VM thunk — it is a real native function registered as a script callback. Reads 3 arguments from the VM stack (object reference, position vec3, rotation vec3), calls the spawn pipeline, and pushes a success result.

## Decompiled (IDA)

```c
int sub_44C730()
{
  int v1;              // [esp+4h] [ebp-24h] BYREF
  _BYTE v2[16];       // [esp+8h] [ebp-20h] BYREF   -- rotation vec3
  _BYTE v3[16];       // [esp+18h] [ebp-10h] BYREF  -- position vec3

  VMPopObjRef(&v1);           // pop asset/object reference
  VMPopVec3(v3);              // pop position (x, y, z)
  VMPopVec3(v2);              // pop rotation (x, y, z)
  sub_44C600(v1, v3, v2);    // SpawnCharacter
  return VMReturnBool(1);    // always returns success
}
```

## Pseudocode

```c
void sauSpawnObj_Handler(void)
{
    // Pop arguments from VM stack (reverse push order)
    void *objRef = PopObjectRef();      // sub_52C860
    float pos[3]; PopVec3(pos);         // sub_52C770
    float rot[3]; PopVec3(rot);         // sub_52C770

    // Spawn the character
    SpawnCharacter(objRef, pos, rot);   // sub_44C600

    // Push success result to VM return stack
    PushResult(1);                      // sub_52CC70
}
```

## VM Stack Reads

| Order | Function | Address | Value |
|-------|----------|---------|-------|
| 1 | PopObjectRef | `0x52C860` | Asset/object reference |
| 2 | PopVec3 | `0x52C770` | Position (x, y, z) |
| 3 | PopVec3 | `0x52C770` | Rotation (x, y, z) |

## Key Details

- This is a **native function**, not a VM thunk — it can be fully decompiled and hooked
- The object reference is resolved through `dword_E56160` (VM object reference base)
- After spawning, always pushes `1` (true/success) to the return stack
- Does not handle error cases explicitly in the visible code

## Called By

- Kallis scripts via the VM function dispatch table (registered as "sauSpawnObj")

## Calls

| Address | Name | Purpose |
|---------|------|---------|
| `0x52C860` | PopObjectRef | Pop object reference from VM stack |
| `0x52C770` | PopVec3 | Pop vec3 from VM stack (called twice) |
| `0x44C600` | SpawnCharacter | Execute the character spawn |
| `0x52CC70` | PushResult | Push success (1) to return stack |

## Related

- [sub_44C600_SpawnCharacter.md](sub_44C600_SpawnCharacter.md) — SpawnCharacter (called by this)
- [sub_44FA20_CreateCharacter_Internal.md](sub_44FA20_CreateCharacter_Internal.md) — CreateCharacter_Internal
- [../../vm/subs/sub_52C860_PopObjectRef.md](../../vm/subs/sub_52C860_PopObjectRef.md) — PopObjectRef
- [../../vm/subs/sub_52C770_PopVec3.md](../../vm/subs/sub_52C770_PopVec3.md) — PopVec3
- [../../vm/subs/sub_52CC70_PushResult.md](../../vm/subs/sub_52CC70_PushResult.md) — PushResult
- [sub_44C6C0_sauCreateCharacter.md](sub_44C6C0_sauCreateCharacter.md) -- sauCreateCharacter (spawner-relative variant)
- [../CHARACTER_CREATION.md](../CHARACTER_CREATION.md) -- Character creation overview
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Full spawn pipeline overview
