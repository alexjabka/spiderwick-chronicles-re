# sub_4546A0 -- ActivateBase (vtable[2])

## Identity

| Field | Value |
|---|---|
| Address | `0x4546A0` |
| Calling Convention | `__thiscall` |
| this | Character object (ClCharacterObj / ClPlayerObj) |
| Parameters | none (`int this` only) |
| Return Value | `int` |
| Module | engine/objects |

## Purpose

The "light" activation function for character objects. Called by `SpawnCharacter` (sub_44C600) via vtable[2]. Handles sector registration, render component positioning, and scene node registration. Does **NOT** create render objects or run VM event setup -- those are handled by FullActivate (sub_458B30) via vtable[14].

For ClPlayerObj, vtable[2] at address 0x462250 is a thin wrapper that calls this function (ActivateBase) and then `CameraDirtyFlag` (sub_436910) to force a camera update.

## Calling Context

```
SpawnCharacter (sub_44C600)
  └── character->vtable[2](character)
      └── ClPlayerObj vtable[2] wrapper (0x462250)
          ├── sub_4546A0 (this function)  -- ActivateBase
          └── sub_436910                  -- CameraDirtyFlag
```

## Execution Steps

### Step 1: Sector Registration via +0x1B8

Reads the sector/scene data pointer at `this+0x1B8`. If non-null, calls sector registration functions to place the character in the appropriate visibility sector based on its transform matrix at `this+0x28`.

```c
void* sectorData = *(void**)(this + 0x1B8);
if (sectorData) {
    // Register character in sector using transform at +0x28
    RegisterWithSector(sectorData, this + 0x28);
}
```

### Step 2: Render Component Positioning Loop

Iterates over the render component array at +0x4D8/+0x4DC:

```c
int count = *(int*)(this + 0x4D8);     // render component count
void** comps = (void**)(this + 0x4DC); // render component array

for (int i = 0; i < count; i++) {
    if (comps[i]) {
        SetRenderComponentPosition(comps[i], this + 0x28);
    }
}
```

**Important:** For player characters, +0x4D8 is **always 0**. This loop executes zero times. Player rendering goes through the child component path at +0x368, which is activated by FullActivate (sub_458B30), not this function.

### Step 3: EntityRegister_SceneNodes (sub_51B220)

Calls `EntityRegister_SceneNodes` to register the character's scene nodes (+0x9C, +0xA0) with the scene management system.

```c
EntityRegister_SceneNodes(this);  // sub_51B220
```

### Step 4: Attachment Visitor

Iterates over attached objects/components and updates their positions relative to the character's current transform.

### Step 5: Position Copy

Copies the character's position data to ensure consistency between the position fields (+0x68/+0x6C/+0x70) and the transform matrix (+0x28).

## Why This Is Insufficient for VM Steal Spawns

ActivateBase assumes that render objects already exist (created during a previous FullActivate or level load). It positions and registers existing render data but does not create it. When called on a character that has only been through:
- Factory allocation + construction: No render data exists. Empty scene nodes are registered. Character is invisible.
- Factory + VMInitObject: Visual data has been created by the Init script, but render objects (draw calls, vertex buffers) have not been built from that data. Character is invisible.

Only `FullActivate` (sub_458B30) calls `RenderObjSetup` (sub_4584C0), which converts the Init script's visual data into actual render objects.

## vtable[2] Wrapper: ClPlayerObj (0x462250)

The ClPlayerObj vtable entry for slot 2 is at address 0x462250. This is a thin wrapper:

```c
int __thiscall ClPlayerObj_Activate_vtable2(void* this)
{
    int result = ActivateBase(this);     // sub_4546A0
    CameraDirtyFlag();                   // sub_436910 -- force camera recalc
    return result;
}
```

The camera dirty flag forces the camera system to recalculate visibility, which is necessary when a new character enters the scene.

## Called By

| Address | Name | Via |
|---------|------|-----|
| `0x44C600` | SpawnCharacter | vtable[2] call |
| `0x462250` | ClPlayerObj vtable[2] wrapper | Direct call from vtable |

## Calls

| Address | Name | Purpose |
|---------|------|---------|
| `0x51B220` | EntityRegister_SceneNodes | Scene node registration |
| Sector registration funcs | (via +0x1B8) | Portal/sector visibility |

## Contrast with FullActivate (sub_458B30)

| Feature | ActivateBase (this) | FullActivate (sub_458B30) |
|---------|---------------------|--------------------------|
| VM event handlers | No | Yes (7 registrations) |
| Scene node creation | No (uses existing) | Yes (sub_556170) |
| Render object setup | No | **Yes** (sub_4584C0) |
| Scene registration | Yes (sub_51B220) | Yes (sub_556200) |
| Sector registration | Yes (via +0x1B8) | Yes (sub_53A470 x2) |
| Child activation | No | Yes (6 slots at +0x368) |
| Render comp positioning | Yes (+0x4D8/+0x4DC loop) | No (not needed, just created) |
| Camera dirty flag | Yes (via wrapper) | No |
| Makes character visible | Only if render objects exist | **Yes** (creates render objects) |

## Related

- [sub_458B30_FullActivate.md](sub_458B30_FullActivate.md) -- "Full" activate (vtable[14]), creates render objects
- [sub_44C600_SpawnCharacter.md](sub_44C600_SpawnCharacter.md) -- SpawnCharacter (calls this via vtable[2])
- [../ENTITY_SPAWN_PIPELINE.md](../ENTITY_SPAWN_PIPELINE.md) -- Full spawn pipeline documentation
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Original spawn system overview
