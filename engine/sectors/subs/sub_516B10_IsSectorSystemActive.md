# sub_516B10 — IsSectorSystemActive
**Address:** Spiderwick.exe+116B10 (absolute: 0x516B10)
**Status:** FULLY REVERSED

## Purpose
Gate function: returns true if the sector visibility system is active in mode 0.
Used as condition before calling PerformRoomCulling in UpdateVisibility.

## Pseudocode
bool IsSectorSystemActive() {
    void *mgr = *(void**)0xE416C4;
    void *state = mgr[0x80];
    if (state == NULL) return false;
    return (state[0x20] == 0);
}

## Assembly
516B10: mov eax, dword_E416C4
516B15: mov eax, [eax+80h]
516B1B: test eax, eax
516B1D: jz short ret_false
516B1F: cmp dword ptr [eax+20h], 0
516B23: jnz short ret_false
516B25: mov al, 1
516B27: retn
516B28: xor al, al    ; ret_false
516B2A: retn

## Paired with sub_516E30
- sub_516B10: returns true when state[0x20] == 0
- sub_516E30: returns true when state[0x20] == 1
- Together: "is sector system in mode 0 or 1?"
- UpdateVisibility: if (either returns true) -> call PerformRoomCulling

## WARNING
Do NOT patch these functions globally — they're called from multiple systems.
Crashes observed when forced to return false (v3 experiment).
Patch the JUMPS in UpdateVisibility (+88B51, +88B5A) instead.
