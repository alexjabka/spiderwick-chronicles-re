# sub_44FE90 — GetCharacterManager

**Address:** `0x44FE90` (Spiderwick.exe+4FE90)
**Returns:** `0x730754` — pointer to ClCharacterManagerImpl singleton
**Convention:** cdecl (no args)

## Purpose

Singleton getter for ClCharacterManagerImpl. Initializes vtable on first call.

## Decompiled (Hex-Rays)

```c
int *GetCharacterManager()
{
  if ( (dword_730758 & 1) == 0 )
  {
    dword_730758 |= 1u;
    g_CharacterManager = (int)&ClCharacterManagerImpl::`vftable';
  }
  return &g_CharacterManager;  // always 0x730754
}
```

## Globals

| Address | Name | Description |
|---------|------|-------------|
| `0x730754` | g_CharacterManager | Singleton instance (static, not heap) |
| `0x730758` | g_CharManagerInitFlag | Init flag (bit 0) |

## Vtable (0x629C5C)

| Index | Address | Description |
|-------|---------|-------------|
| [0] | sub_44FE20 | FindCharacterByPositionAndRadius |
| [1] | sub_44FE00 | FindCharacterByPosition |
