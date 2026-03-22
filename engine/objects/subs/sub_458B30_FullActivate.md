# sub_458B30 -- FullActivate

## Identity

| Field | Value |
|---|---|
| Address | `0x458B30` |
| Calling Convention | `__thiscall` |
| this | Character object (ClCharacterObj / ClPlayerObj) |
| Parameters | none |
| Return Value | `int` |
| Size | 789 bytes |
| Basic Blocks | 23 |
| Cyclomatic Complexity | 11 |
| Module | engine/objects |

## Purpose

The "full" activation function for character objects. This is the critical path that makes a character **visible** in the game world. Unlike the "light" activate (vtable[2], sub_4546A0), FullActivate runs the complete render pipeline: VM event handler registration, scene node creation, render object setup, sector registration, and child component activation.

Called by the `sauActivateObj` VM handler via vtable[14] (0x4621D0 for ClPlayerObj). This is the activation path used when the VM system brings an object into the active world.

## Calling Context

```
VM Script: ActivateObj(objRef)
  └── sauActivateObj handler
      └── vtable[14]: ClPlayerObj_sauActivateObj (0x4621D0)
          └── sub_458B30 (this function)
```

In the VM Steal spawn pipeline, this function is called directly via vtable[14] after VMInitObject has linked the VM object and run the Init script.

## Execution Steps

### Step 1: vtable[16] Validation Check

Calls `this->vtable[16]()` to check whether the object is in a valid state for activation. If this returns a specific failure condition, the function may bail early.

### Step 2: .kallis Trampoline

Enters the Kallis VM context for event handler setup. This bridges between native code and the VM's event dispatch system.

### Step 3: VM Event Handler Registration

Registers multiple VM event handlers on the object:

| Call | Function | Purpose |
|------|----------|---------|
| 1 | sub_52EDB0 | Primary event handler setup |
| 2 | sub_52F300 | Secondary event handler setup |
| 3 | sub_52F9A0 | Event handler setup (called 4 times with different parameters) |
| 4 | sub_52F9A0 | (second call) |
| 5 | sub_52F9A0 | (third call) |
| 6 | sub_52F9A0 | (fourth call) |
| 7 | sub_52F570 | Final event handler setup |

These handlers allow the VM system to dispatch script events (damage, interact, proximity, etc.) to the native object.

### Step 4: Scene Node Creation

Calls `sub_556170` to create a scene graph node for the character. This node is the container in the scene hierarchy that holds the character's transform and render data.

The created node is stored at character offsets +0x138 (activation scene node) and linked to the scene graph.

### Step 5: Render Object Setup (CRITICAL)

Calls `sub_4584C0` (RenderObjSetup) to create the visual representation. This is the step that actually makes the character visible. It:
- Reads mesh/skeleton/material data created by the Init script
- Creates render objects (draw calls, vertex buffers, etc.)
- Attaches render data to the scene node
- Sets up render bounds at +0x13C

**Precondition:** The VM Init script must have already run. Without Init, there is no mesh/skeleton data and this call crashes on the next frame.

### Step 6: Scene Registration

Calls `sub_556200` to register the scene node with the scene management system. This makes the node visible to the scene traversal and culling algorithms.

### Step 7: Sector Registration (x2)

Calls `sub_53A470` twice with different parameters to register the character with the sector/portal system. The Spiderwick engine uses indoor portal-based visibility, so objects must be registered in sectors to be rendered.

Uses the transform matrix at character+0x28 to determine which sector(s) the character belongs to.

### Step 8: Child Activation

Iterates over the 6 child component slots at offsets +0x368 through +0x37C:

```c
for (int i = 0; i < 6; i++) {
    void* child = *(void**)(this + 0x368 + i * 4);
    if (child) {
        // Activate child component
        child->vtable[activate](child);
    }
}
```

Child components include attached weapons, accessories, effects, and other visual elements that need their own activation.

### Step 9: Flag Updates

Updates status flags on the character to mark it as activated:
- Sets activation bits in the flags DWORD
- May update the sector flags at +0x1CC
- Marks the character as needing render updates

## Why This Function Is Required for Visibility

The "light" activate (vtable[2], sub_4546A0) only does sector registration and scene node positioning. It assumes render objects already exist. FullActivate (this function) is the only path that calls `RenderObjSetup` (sub_4584C0), which is the function that actually creates the drawable representation.

For the VM Steal spawn pipeline:
- `VMInitObject` creates the raw visual data (mesh, skeleton, materials) via the Init script
- `FullActivate` (this function) takes that data and creates the render objects, scene nodes, and sector registrations needed to actually draw the character

Without FullActivate, a character that has been through VMInitObject has visual data in memory but no render system knows about it.

## Called By

| Address | Name | Via |
|---------|------|-----|
| `0x4621D0` | ClPlayerObj_sauActivateObj | vtable[14] |
| SpiderMod | VM Steal spawn pipeline | Direct vtable[14] call |

## Calls

| Address | Name | Purpose |
|---------|------|---------|
| `0x52EDB0` | VM event setup 1 | Event handler registration |
| `0x52F300` | VM event setup 2 | Event handler registration |
| `0x52F9A0` | VM event setup 3 | Event handler registration (called x4) |
| `0x52F570` | VM event setup 4 | Event handler registration |
| `0x556170` | SceneNode_Create | Creates scene graph node |
| `0x4584C0` | RenderObjSetup | Creates visual render objects |
| `0x556200` | SceneNode_Register | Registers node with scene system |
| `0x53A470` | Sector_Register | Registers with portal/sector system (called x2) |

## Related

- [sub_4546A0_ActivateBase.md](sub_4546A0_ActivateBase.md) -- "Light" activate (vtable[2]), contrast with this function
- [sub_44C600_SpawnCharacter.md](sub_44C600_SpawnCharacter.md) -- SpawnCharacter (calls vtable[2], not this)
- [../ENTITY_SPAWN_PIPELINE.md](../ENTITY_SPAWN_PIPELINE.md) -- Full spawn pipeline documentation
- [../ENTITY_SPAWNING.md](../ENTITY_SPAWNING.md) -- Original spawn system overview
