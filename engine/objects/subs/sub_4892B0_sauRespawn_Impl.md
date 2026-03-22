# sub_4892B0 — sauRespawn_Impl

## Identity

| Field | Value |
|---|---|
| Address | `0x4892B0` |
| Calling Convention | `cdecl` |
| Module | engine/objects |

## Purpose

C-side implementation of the `sauRespawn` script function. When called from script, it triggers the respawn / checkpoint-reload flow: sets a flag, initialises required subsystems, and invokes the respawn chain that ultimately shows the game-over / reload screen.

Steps:

1. Ensures the `"book"` subsystem is initialised — calls `sub_418290("book")` if it has not been set up already.
2. Sets `byte_6E47C5 = 1` to mark that a respawn has been requested.
3. Calls `sub_537D10` (unknown; likely flushes or resets some state).
4. Calls `sub_4AE4B0` then `sub_4AE4D0` to drive the respawn / game-over screen sequence.

## Key References

| Symbol | Role |
|---|---|
| `sub_418290("book")` | Initialises the `"book"` subsystem if not already active |
| `byte_6E47C5` | Respawn-requested flag; set to `1` here |
| `sub_537D10` | State reset / flush (purpose partially resolved) |
| `sub_4AE4B0` | First stage of respawn flow |
| `sub_4AE4D0` | Second stage — triggers checkpoint reload / game-over screen |

## Decompiled Pseudocode

```c
void sauRespawn_Impl(void)
{
    // Ensure "book" subsystem is ready
    sub_418290("book");

    // Signal that respawn was requested
    byte_6E47C5 = 1;

    // Flush / reset state
    sub_537D10();

    // Trigger respawn sequence (game-over / checkpoint reload screen)
    sub_4AE4B0();
    sub_4AE4D0();
}
```

## Notes

- This function is the native backing of the `sauRespawn()` script call; script-side callers do not need to manage subsystem state directly.
- `byte_6E47C5` is checked elsewhere to determine whether to run the respawn path vs. a clean level transition.
- The `"book"` subsystem relates to the Field Guide / Arthur's Book mechanic; its exact role in the respawn path is not fully resolved but the init guard suggests it must be live before the reload screen can display correctly.
- `sub_4AE4B0` / `sub_4AE4D0` are part of the same respawn stage chain; the split may correspond to a state-machine transition (request → execute).
