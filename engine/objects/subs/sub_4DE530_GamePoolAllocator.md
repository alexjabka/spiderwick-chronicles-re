# sub_4DE530 -- GamePoolAllocator

## Identity

| Field | Value |
|---|---|
| Address | `0x4DE530` |
| Calling Convention | `__cdecl` |
| Parameters | `int size, unsigned int alignment` |
| Return Value | `int` -- pointer to allocated block, or 0 on failure |
| Size | 148 bytes |
| Module | engine/memory |

## Purpose

The game's pool-based memory allocator. Used for all game object allocation during level loading. Supports a stack of allocation contexts (for nested loading operations) and a disabled flag that blocks allocation during normal gameplay.

## Decompiled (IDA)

```c
int __cdecl sub_4DE530(int a1, unsigned int a2)
{
  unsigned int v3;    // esi -- rounded size
  int *v4;            // edi -- pointer to remaining bytes
  int *v5;            // ecx -- pointer to current offset
  int v6;             // edx -- alignment padding
  unsigned int v7;    // eax -- total allocation (padding + size)

  // Path 1: Custom allocator override
  if ( dword_D577F0 )
    return (**(int (__thiscall ***)(int, int, unsigned int))dword_D577F0)
             (dword_D577F0, a1, a2);

  // Round size up to 16-byte multiple
  v3 = 16 * ((unsigned int)(a1 + 15) >> 4);

  // Path 2: Stack-based context (nested loading)
  if ( dword_D577EC )    // g_AllocStackDepth > 0
  {
    v4 = (int *)dword_D57744[5 * dword_D577EC];       // stack[depth].remaining
    v5 = (int *)(20 * dword_D577EC + 13989692);       // stack[depth].current
  }
  else
  {
    // Path 3: Default pool (no stack context)
    if ( byte_D577F4 )   // g_AllocDisabled
      return 0;          // *** ALLOCATION BLOCKED ***
    v5 = &dword_D57730;  // g_DefaultPoolCurrent
    v4 = &dword_D57738;  // g_DefaultPoolRemaining
  }

  // Calculate alignment padding
  v6 = *v5 % a2;
  if ( v6 )
    v6 = a2 - v6;

  // Check if enough space
  v7 = v6 + v3;
  if ( *v4 < v6 + v3 )
    return 0;            // not enough space

  // Bump allocator
  *v5 += v7;
  *v4 -= v7;
  return *v5 - v3;       // return aligned pointer
}
```

## Key Globals

| Address | Type | Name | Purpose |
|---------|------|------|---------|
| `0xD577F0` | ptr | g_CustomAllocator | Custom allocator vtable ptr (if non-null, delegates all allocations) |
| `0xD577EC` | int | g_AllocStackDepth | Stack depth for nested allocation contexts |
| `0xD577F4` | byte | **g_AllocDisabled** | **1 = disabled (gameplay), 0 = enabled (loading)** |
| `0xD57730` | int | g_DefaultPoolCurrent | Default pool bump pointer |
| `0xD57738` | int | g_DefaultPoolRemaining | Default pool bytes remaining |
| `0xD57744` | array | g_AllocStack | Stack of allocation contexts (20 bytes each) |

## Allocation Paths

1. **Custom allocator** (`dword_D577F0 != 0`): Delegates to a virtual function. Used during special allocation phases.
2. **Stack context** (`dword_D577EC > 0`): Uses the nested stack entry. This is used during resource loading when multiple contexts are pushed.
3. **Default pool** (`dword_D577EC == 0`): Uses `g_DefaultPoolCurrent/Remaining`. **Blocked if `g_AllocDisabled` is set.**

## KEY DISCOVERY: Disabled During Gameplay

During normal gameplay, `byte_D577F4 = 1`. This causes the allocator to return NULL for any allocation request when no stack context is active. The engine only enables allocation during level loading phases.

To spawn entities at runtime via a mod:
```c
// 1. Save and clear disabled flag
uint8_t wasDisabled = ReadByte(0xD577F4);
WriteByte(0xD577F4, 0);

// 2. Perform allocation (e.g., via factory)
void* obj = ClPlayerObj_Factory();

// 3. Restore disabled flag
WriteByte(0xD577F4, wasDisabled);
```

## Callers

This function is called by every factory function in the engine (sub_4D60B0, sub_4D60E0, sub_4D6110, sub_4D6140, etc.) as well as numerous resource loading functions. It has 200+ xrefs.

## Related

- [sub_4D6110_ClPlayerObj_Factory.md](sub_4D6110_ClPlayerObj_Factory.md) -- ClPlayerObj factory (caller)
- [sub_4D6030_FactoryDispatch.md](sub_4D6030_FactoryDispatch.md) -- factory dispatcher
- [sub_44FA20_CreateCharacter_Internal.md](sub_44FA20_CreateCharacter_Internal.md) -- creation pipeline
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Full spawn pipeline overview
