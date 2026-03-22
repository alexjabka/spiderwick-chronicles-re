# Monocle Overlay Flag
**Write instruction:** `Spiderwick.exe+C27FD` (absolute: `004C27FD`)
**Flag address:** `Spiderwick.exe+2E77C4` (absolute: `006E77C4`, static)

## What it does
Writes EAX to `[006E77C4]` — controls the monocle HUD darkening overlay.
NOPing this instruction disables the overlay permanently.

## Instruction
```
004C27FD: A3 C4776E00    mov [006E77C4], eax    (5 bytes)
```

## Context
```asm
004C27F3: 0F8C 47FEFFFF  jl Spiderwick.exe+C2640
004C27F9: 8B 44 24 60    mov eax, [esp+60]        ; overlay state value
004C27FD: A3 C4776E00    mov [006E77C4], eax      ; ← WRITE OVERLAY FLAG
004C2802: E8 29F80A00    call Spiderwick.exe+172030
004C2807: 5F             pop edi
```

## Found By
CE scan: Byte → Unknown initial → Shift press (Changed) → Shift release (Changed).
Two bytes at 006E77C4 and 006E77C5 toggle with monocle.
"Find what writes" on 006E77C5 → found this instruction.

## Also Found
- `006E77C5` (`Spiderwick.exe+2E77C5`) — adjacent byte, also toggles with monocle

## Status: FOUND, NOT YET INTEGRATED
NOPing disables overlay. Need to study parent function for mouse rotation logic.

## Next Step
Decompile the function containing +C27FD to understand monocle camera rotation.

## Related
- Parent function contains monocle handler logic (TODO: decompile)
- sub_438A90 (matrix multiply) is called by monocle code
- Monocle accesses camera position at +38AAF..+38C2D
