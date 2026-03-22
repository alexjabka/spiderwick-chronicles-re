# Script Debugger

**Status:** Confirmed (Session 3)

---

## Overview

The Kallis VM has a built-in **remote script debugger** that can block game startup while waiting for a debugger connection. This feature was likely used during development and is still present in the shipping binary. When enabled, it halts execution at world load time and waits for an external script debugger tool to connect.

---

## Enable Flag

```c
*(dword_E561D8 + 0x2E) & 1    // bit 0 of byte at offset +0x2E
```

| Global | Address | Description |
|--------|---------|-------------|
| `dword_E561D8` | `0xE561D8` | Pointer to the script system state object |

- `dword_E561D8` points to a script system state struct
- Byte at offset `+0x2E` within that struct contains the debugger enable bit
- **Bit 0** (value `1`) = debugger enabled
- When bit 0 is set, the VM enters a wait loop during world loading

---

## Behavior When Enabled

When the debugger flag is set and a world is being loaded, the following sequence occurs:

1. **First wait** -- prints message and blocks:
   ```
   {gSCRIPT}: {yINFO:} Waiting for remote script debugger to connect (Press 's' to skip)
   ```
   The game halts and waits for either a debugger connection or the user to press 's'.

2. **Second wait** -- after connection (or skip), prints:
   ```
   {gSCRIPT}: {yINFO:} Hit Run or Step in the script debugger to begin execution (Press 's' to skip)
   ```
   The game halts again, waiting for the debugger to issue a Run or Step command.

The `{gSCRIPT}` and `{yINFO:}` prefixes are color formatting tags for the engine's debug console (green and yellow respectively).

---

## String References

| Address | String |
|---------|--------|
| `0x63EF28` | `"{gSCRIPT}: {yINFO:} Waiting for remote script debugger to connect (Press 's' to skip)\n"` |
| `0x63EEC0` | `"{gSCRIPT}: {yINFO:} Hit Run or Step in the script debugger to begin execution (Press 's' to skip)\n"` |

### .kallis References

The wait-for-debugger string at `0x63EF28` is referenced from:
- `0x1C8EA54` (.kallis)
- `0x1D06980` (.kallis)

Both are within ROP-style native code blocks in the `.kallis` section.

---

## Check Code

The debugger check occurs within `.kallis` native code. The relevant assembly:

```asm
; At approximately 0x20FC072 - 0x20FC0BF:
test    al, al                      ; prior condition check
jz      skip
mov     eax, dword_E561D8           ; load script system state pointer
test    byte ptr [eax+2Eh], 1       ; check bit 0 of debugger flag
jz      skip
; ... debugger wait loop follows ...
```

---

## Wait Loop Implementation

The wait loop is implemented in **ROP-style .kallis native code**. This means:

- It CANNOT be patched by simply NOPing instructions (the ROP chains would break)
- It CANNOT be hooked at a stable address (the control flow is obfuscated)
- The ONLY reliable way to disable it is to **clear the flag** before it is checked

---

## SpiderMod Fix

SpiderMod handles the debugger flag using a **pendingDebugRestore** pattern:

### Before World Load
1. Read the current debugger flag value: `byte val = *(byte*)(*(DWORD*)0xE561D8 + 0x2E)`
2. If bit 0 is set, save the value and clear bit 0
3. Set `pendingDebugRestore = true`

### After World Load
1. If `pendingDebugRestore` is true, restore the original flag value
2. Set `pendingDebugRestore = false`

This approach is **graceful** -- it temporarily disables the debugger only during the critical window when the wait loop would trigger, then restores the original state so the debugger system remains functional for any other purpose.

```c
// Pseudocode of the SpiderMod approach
static bool pendingDebugRestore = false;
static BYTE savedDebugByte = 0;

void BeforeWorldLoad() {
    DWORD scriptState = *(DWORD*)0xE561D8;
    if (scriptState) {
        BYTE* debugFlag = (BYTE*)(scriptState + 0x2E);
        if (*debugFlag & 1) {
            savedDebugByte = *debugFlag;
            *debugFlag &= ~1;           // clear bit 0
            pendingDebugRestore = true;
        }
    }
}

void AfterWorldLoad() {
    if (pendingDebugRestore) {
        DWORD scriptState = *(DWORD*)0xE561D8;
        if (scriptState) {
            *(BYTE*)(scriptState + 0x2E) = savedDebugByte;
        }
        pendingDebugRestore = false;
    }
}
```

---

## Debugger Protocol

The debugger protocol is **unknown**. No TCP/IP socket or named pipe references were found near the debugger code. Possible communication mechanisms:

- File-based communication (shared files)
- Shared memory / memory-mapped files
- A proprietary IPC mechanism built into the Kallis toolchain

The original debugger tool (likely part of the Kallis SDK) has not been identified.

---

## Key Addresses

| Address | Type | Description |
|---------|------|-------------|
| `0xE561D8` | global ptr | Script system state object pointer |
| `+0x2E` | byte offset | Debugger enable flag (bit 0) |
| `0x63EF28` | .rdata string | "Waiting for remote script debugger..." |
| `0x63EEC0` | .rdata string | "Hit Run or Step in the script debugger..." |
| `0x1C8EA54` | .kallis | Reference to wait string |
| `0x1D06980` | .kallis | Reference to wait string |

---

## Related

- [KALLIS_VM.md](KALLIS_VM.md) -- VM architecture overview
- [VM_ROP_DISPATCHERS.md](VM_ROP_DISPATCHERS.md) -- Why the wait loop cannot be patched
- [../../mods/spidermod/README.md](../../mods/spidermod/README.md) -- SpiderMod (implements the fix)
