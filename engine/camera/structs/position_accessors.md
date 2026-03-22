# Camera Position Accessors — Who Reads/Writes +3B8 (Camera X)

**Address tracked:** `[pCamStruct]+3B8` = Camera X position
**Absolute example:** `21497610`

Key relationship: `EBP/EDX = pCamStruct + 0x388`, and position X = `[base+0x30]`
within this sub-structure. So `+3B8 = +0x388 + 0x30`.

## Writers (Find what WRITES)

| Instruction | Address | Count | Status |
|-------------|---------|-------|--------|
| `fstp [eax-14]` (sub_5A7DC0 via sub_4356F0) | 005A7DF7 | ~5500 | **BLOCKED by sub_4356F0 hook** |

**Only ONE writer exists.** Our hook blocks it. Position is fully protected.

## Readers (Find what ACCESSES)

### High Frequency (every frame)

| Instruction | Address (exe+offset) | Count | Context | Register |
|-------------|---------------------|-------|---------|----------|
| `fld [esi-1C]` | 005A7CB8 (+1A7CB8) | 2659 | sub_5A7CB8 area — reads position as source | ESI = pos+0x1C |
| `fld [edx-1C]` | 005A7DF4 (+1A7DF4) | 2545 | sub_5A7DC0 (generic memcpy) — source read | EDX = pos+0x1C |
| `fld [ebp+30]` | 0043E560 (+3E560) | 751 | Camera pipeline — reads position for orbit calc | EBP = pCam+0x388 |
| `fld [ebp+30]` | 0043ED36 (+3ED36) | 751 | Camera pipeline (later) — reads position again | EBP = pCam+0x388 |

### Low Frequency (monocle only, count=1)

| Instruction | Address (exe+offset) | Count | Context | Register |
|-------------|---------------------|-------|---------|----------|
| `fmul [edx+30]` | 00438AAF (+38AAF) | 1 | Monocle code — matrix multiply with position | EDX = pCam+0x388 |
| `fmul [edx+30]` | 00438B1F (+38B1F) | 1 | Monocle code — matrix multiply | EDX = pCam+0x388 |
| `fmul [edx+30]` | 00438BA2 (+38BA2) | 1 | Monocle code — matrix multiply | EDX = pCam+0x388 |
| `fmul [edx+30]` | 00438C2D (+38C2D) | 1 | Monocle code — matrix multiply | EDX = pCam+0x388 |

## Key Insight

The camera pipeline at +3E560 and +3ED36 **READS** position (not writes).
This means the pipeline uses our position for its calculations (orbit, collision)
but doesn't overwrite it. Combined with sub_4356F0 hook blocking the only writer,
our Lua-written position persists across frames.

## Sub-structure Layout

```
pCamStruct + 0x388 = base of position sub-structure
  +0x00..+0x3C  = first 16 floats (copied by sub_5A7DC0 first call)
  +0x30         = Camera X (= pCamStruct+3B8)
  +0x34         = Camera Y (= pCamStruct+3BC)
  +0x38         = Camera Z (= pCamStruct+3C0)
  +0x40..+0x7C  = next 16 floats (copied by sub_5A7DC0 second call)
```

## Related
- Writer analysis: [sub_4356F0_PositionWriter.md](../subs/sub_4356F0_PositionWriter.md) (hooked to block writes)
- Generic memcpy: [sub_5A7DC0_Memcpy16Floats.md](../subs/sub_5A7DC0_Memcpy16Floats.md) (reader + writer)
- Camera pipeline: [camera_pipeline_3E547.md](../subs/camera_pipeline_3E547.md) (reader)
- Monocle code at +38AAF..+38C2D (TODO: analyze in IDA)
