# sub_52C770 -- PopVec3

**Address:** `0x52C770` (`Spiderwick.exe+0EC770`) | **Calling convention:** cdecl

## Purpose

Pops a vec3 (3 consecutive float values) from the Kallis VM argument stack into a caller-provided `float[3]` buffer. This is a wrapper around `PopFloat3` (sub_52C700) that copies the results into the output buffer.

## Decompiled

```c
float* __cdecl PopVec3(float *out) {
    float tmp[3];           // local buffer on stack
    PopFloat3(tmp);         // sub_52C700: pops 3 floats from arg stack
    out[0] = tmp[0];        // copy X
    out[1] = tmp[1];        // copy Y
    out[2] = tmp[2];        // copy Z
    return out;
}
```

## Disassembly

```asm
52C770  ; --- prologue ---
52C777  call    sub_52C700              ; PopFloat3(tmp) — pops 3 floats into [esp+0..esp+8]
52C784  mov     eax, out
        fld     dword ptr [esp+0]       ; load tmp[0]
        fstp    dword ptr [eax]         ; out[0] = tmp[0]
52C78A  fld     dword ptr [esp+4]       ; load tmp[1]
        fstp    dword ptr [eax+4]       ; out[1] = tmp[1]
52C791  fld     dword ptr [esp+8]       ; load tmp[2]
        fstp    dword ptr [eax+8]       ; out[2] = tmp[2]
52C797  retn
```

## Key Addresses

| Address | Description |
|---------|-------------|
| `0x52C770` | Function entry point |
| `0x52C700` | `PopFloat3` -- called to pop 3 floats |
| `0xE56168` | `dword_E56168` -- arg stack base (used by PopFloat3) |
| `0xE56200` | `dword_E56200` -- arg stack index (decremented 3 times) |

## Notes

- Pops 3 values from the arg stack in order. The first pop becomes `out[0]` (X), second becomes `out[1]` (Y), third becomes `out[2]` (Z).
- The intermediate copy through a local `tmp[3]` buffer is because `PopFloat3` writes to its own stack frame; this wrapper then copies out to the caller's buffer.
- Used whenever a script function passes a 3D position or rotation as arguments.
- After this call, `dword_E56200` has been decremented by 3.

## Called By

- Script handlers that receive position/rotation vec3 arguments (e.g., spawn functions, set-position functions)

## Related

- [sub_52C700_PopFloat3.md](sub_52C700_PopFloat3.md) -- PopFloat3 (called by this function)
- [sub_52C6A0_PopFloat.md](sub_52C6A0_PopFloat.md) -- PopFloat (single float pop)
- [sub_52C610_PopArg.md](sub_52C610_PopArg.md) -- PopArg (integer/generic pop)
- [../VM_STACK_OPERATIONS.md](../VM_STACK_OPERATIONS.md) -- Stack operations overview
- [../KALLIS_VM.md](../KALLIS_VM.md) -- VM architecture overview
