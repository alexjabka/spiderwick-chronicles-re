# sub_44C3C0 -- ClearCoopEventsArray

**Address:** 0x44C3C0 (Spiderwick.exe+4C3C0) | **Calling convention:** __cdecl

---

## Purpose

Clears the coop events registration array at `dword_730270` and resets the count at `dword_730268` to zero. Called during level transitions and initialization to ensure no stale coop event registrations carry over.

---

## Parameters

None.

**Returns:** 0 (always)

---

## Decompiled Pseudocode

```c
int ClearCoopEventsArray()
{
    memset(&dword_730270, 0, 0x4B0);  // clear 1200 bytes (300 * 4 bytes)
    dword_730268 = 0;                  // reset count to 0
    return 0;
}
```

---

## Coop Events Array Layout

| Global | Type | Description |
|--------|------|-------------|
| `dword_730268` | DWORD | Current count of registered objects |
| `dword_730270` | DWORD[300] | Array of VM object references (up to 300 entries) |

The array is a flat list of VM object references. Each entry is 4 bytes (a DWORD). The total capacity is **300 objects** (0x4B0 / 4 = 300).

The 8-byte gap between `dword_730268` (count) and `dword_730270` (array start) at offset `0x730269`-`0x73026F` is padding/alignment.

---

## Called By

| Caller | Context |
|--------|---------|
| `sub_4886E0` | Level initialization (at 0x4886ED) |
| `0x431B0D` | World loading/reset |

---

## Notes

1. The `memset` with size `0x4B0` (1200 bytes) confirms the array capacity of exactly 300 DWORD entries.

2. This function is called early in level initialization to ensure the coop events array starts clean. Objects then register themselves via `sauRegObjForCoopEvents` (sub_49A430) during level script execution.

3. The array is iterated by `CallEnterCoopAll` (sub_44C3E0) which dispatches "EnterCoop" events to all registered objects.

---

## Related Documentation

- [sub_49A430_sauRegObjForCoopEvents.md](sub_49A430_sauRegObjForCoopEvents.md) -- VM handler that registers objects
- [sub_44C3E0_CallEnterCoopAll.md](sub_44C3E0_CallEnterCoopAll.md) -- Iterates and dispatches to array
- [../HOT_SWITCH_SYSTEM.md](../HOT_SWITCH_SYSTEM.md) -- Hot-switch system overview
