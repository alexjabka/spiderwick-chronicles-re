# sub_44C3E0 --- CallEnterCoopAll

**Address:** 0x44C3E0 (Spiderwick.exe+4C3E0) | **Calling convention:** __cdecl (presumed, no parameters)

---

## Purpose

Iterates all registered co-op objects and calls the `"EnterCoop"` VM script function on each of them. Uses [VMCall](../../vm/subs/sub_52EB40_VMCall.md) (sub_52EB40) to invoke the script callback.

Has a sister function at `0x44C420` that performs the identical operation but calls `"ExitCoop"` instead.

---

## Parameters

None.

**Returns:** void

---

## Decompiled Pseudocode

```c
void __cdecl CallEnterCoopAll(void)
{
    int count = dword_730268;           // number of registered coop objects
    void **coopArray = dword_730270;    // array of VM object pointers

    for (int i = 0; i < count; i++)
    {
        VMCall(coopArray[i], "EnterCoop");    // sub_52EB40
    }
}
```

---

## Key Addresses and Data

| Address | Description |
|---------|-------------|
| `0x44C3E0` | Entry point (CallEnterCoopAll) |
| `0x44C420` | Sister function (CallExitCoopAll --- calls "ExitCoop") |
| `dword_730268` | Count of registered coop objects |
| `dword_730270` | Array of coop VM object pointers |

---

## Called By

| Caller | Context |
|--------|---------|
| Co-op mode activation logic | When co-op mode is entered (second player joins) |

## Calls

| Address | Function | Purpose |
|---------|----------|---------|
| `0x52EB40` | [VMCall](../../vm/subs/sub_52EB40_VMCall.md) | Calls `"EnterCoop"` script function on each coop object |

---

## Notes / Caveats

1. **The coop object array at `dword_730270`** is populated during level loading when objects with `EnterCoop`/`ExitCoop` script functions are registered. The count at `dword_730268` tracks how many are currently registered.

2. **Sister function at 0x44C420** is structurally identical but calls `"ExitCoop"` instead of `"EnterCoop"`. Both use the same object array and count.

3. **VMCall only finds SCRIPT functions.** If a coop object's `EnterCoop` is implemented as a native handler rather than a script function, VMCall will return 0 (not found) and silently skip it. In practice, `EnterCoop`/`ExitCoop` appear to always be script-defined.

4. **Related functions:**
   - `0x44C420` (CallExitCoopAll) --- sister function for "ExitCoop"
   - [VMCall](../../vm/subs/sub_52EB40_VMCall.md) (sub_52EB40) --- the VM call mechanism used
