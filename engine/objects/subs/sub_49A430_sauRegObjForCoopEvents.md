# sub_49A430 -- sauRegObjForCoopEvents Handler

**Address:** 0x49A430 (Spiderwick.exe+9A430) | **Calling convention:** __thiscall (VM handler)

---

## Purpose

VM script handler that registers a game object in the coop events array. Pops an object reference from the VM argument stack, passes it to `sub_44C370` (a `.kallis` thunk) which registers the object at `dword_730270`, and pushes the registration result back to the VM return stack.

---

## Parameters

Takes arguments from the VM stack:

| Pop Order | Type | Description |
|-----------|------|-------------|
| 1 | object ref | Object to register for coop events |

**Returns (VM):** `byte` -- registration result (via `sub_52CC30`)

---

## Decompiled Pseudocode

```c
int __thiscall sauRegObjForCoopEvents(void *this)
{
    void *obj = this;  // initial ECX
    sub_52C860(&obj);  // PopObjRef -- pop object reference from VM stack

    if (!obj)
        return sub_52CC30(0);  // push 0 (failure) to return stack

    unsigned char result = sub_44C370(obj);  // .kallis thunk: register in coop array
    return sub_52CC30(result);               // push result to return stack
}
```

---

## Key Addresses

| Address | Description |
|---------|-------------|
| `0x49A430` | Entry point |
| `0x49BF0C` | Registration data reference (xref from .kallis registration) |
| `0x44C370` | RegisterForCoopEvents (.kallis thunk to `off_1C88C60`, resolves to `0x1D02D50`) |
| `0x52C860` | PopObjRef -- pop object reference from VM arg stack |
| `0x52CC30` | PushByte -- push byte result to VM return stack |

---

## Coop Events System

The coop events array at `dword_730270` tracks objects that participate in cooperative gameplay events. Objects registered here receive notifications when coop mode transitions occur (e.g., `CallEnterCoopAll` at sub_44C3E0 iterates this array).

See [sub_44C3C0_ClearCoopEventsArray.md](sub_44C3C0_ClearCoopEventsArray.md) for the array clear function.

---

## Called By

Registered as a VM handler (script function). The registration site is at `0x49BF0C` in the `.kallis` section.

---

## Related Documentation

- [sub_44C3C0_ClearCoopEventsArray.md](sub_44C3C0_ClearCoopEventsArray.md) -- Array clear function
- [sub_44C3E0_CallEnterCoopAll.md](sub_44C3E0_CallEnterCoopAll.md) -- Iterates coop array
- [../HOT_SWITCH_SYSTEM.md](../HOT_SWITCH_SYSTEM.md) -- Hot-switch system overview
- [../../vm/subs/sub_52C860_PopObjRef.md](../../vm/subs/sub_52C860_PopObjRef.md) -- PopObjRef
