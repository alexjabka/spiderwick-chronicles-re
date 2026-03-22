# sub_44F7C0 — FindClosestCharacter

**Address:** `0x44F7C0` (Spiderwick.exe+4F7C0)
**Args:** `(float* position)` — 3D position to search from
**Returns:** Closest ClCharacterObj* (or NULL)
**Convention:** cdecl

## Purpose

Iterates the global character linked list starting from `[0x7307D8]`.
For each character, computes squared distance to the given position.
Returns the character with the smallest distance.

## Decompiled (Hex-Rays)

```c
int __cdecl FindClosestCharacter(float *a1)
{
  int v1; // ecx
  double v2; // st7
  int i; // esi

  v1 = g_CharacterListHead;
  v2 = 1.0e30;
  for ( i = 0; v1; v1 = *(_DWORD *)(v1 + 1440) )  // +0x5A0 = next
  {
    float dx = *(float *)(v1 + 104) - *a1;       // +0x68
    float dy = *(float *)(v1 + 108) - a1[1];     // +0x6C
    float dz = *(float *)(v1 + 112) - a1[2];     // +0x70
    float dist = dx*dx + dy*dy + dz*dz;
    if ( dist < v2 )
    {
      v2 = dist;
      i = v1;
    }
  }
  return i;
}
```

## Key Offsets Used

- `character+0x68/0x6C/0x70` — position XYZ
- `character+0x5A0` — next in linked list

## Related

- sub_44F820 (FindClosestCharacterInRadius) — same but with max radius
- sub_44F890 (GetPlayerCharacter) — finds controlled character instead
