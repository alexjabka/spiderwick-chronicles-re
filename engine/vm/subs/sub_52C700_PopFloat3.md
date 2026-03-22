# sub_52C700 — PopFloat3

## Identity

| Field | Value |
|---|---|
| Address | `0x52C700` |
| Calling Convention | `cdecl` |
| Parameters | `float* out` — output buffer for 3 floats |
| Return Value | `void` |
| Module | engine/vm |

## Purpose

Pops 3 float values from the Kallis VM argument stack and writes them to the output buffer as a vec3. This is a native function (not a VM thunk).

## Operation

```c
void PopFloat3(float *out)
{
    out[0] = PopFloat();  // sub_52C6A0
    out[1] = PopFloat();
    out[2] = PopFloat();
}
```

Note: Because the stack is popped in order, the values are read in reverse push order (last pushed = first popped). Callers must account for this.

## Called By

- `PopVec3` (sub_52C770) — wrapper that pops vec3 into a specified buffer
- `sauSpawnObj` (sub_44C730) — pops position and rotation vectors

## Related

- [sub_52C6A0_PopFloat.md](sub_52C6A0_PopFloat.md) — PopFloat (called 3 times)
- [sub_52C770_PopVec3.md](sub_52C770_PopVec3.md) — PopVec3 wrapper
- [../../objects/subs/sub_44C730_sauSpawnObj.md](../../objects/subs/sub_44C730_sauSpawnObj.md) — sauSpawnObj handler
- [../KALLIS_VM.md](../KALLIS_VM.md) — VM architecture overview
