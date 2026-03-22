# Render Toggle Globals

## Overview
Collection of engine render toggle functions and their associated global variables. These control rim lighting, fog, and snow systems. All are simple setter functions that write to global state read by the renderer each frame.

---

## Rim Light System

### sub_596500 -- SetRimLightEnable
- **Address:** `0x00596500`
- **Global:** `byte_728BA0` (uint8)
- **Effect:** Enables/disables rim lighting on characters and objects.

### sub_596510 -- SetRimLightModulation
- **Address:** `0x00596510`
- **Global:** `byte_728BA1` (uint8)
- **Effect:** Enables/disables rim light modulation (dynamic intensity variation).

### sub_596520 -- SetRimLightZOffset
- **Address:** `0x00596520`
- **Global:** `flt_728BA4` (float)
- **Effect:** Sets the Z-axis offset for rim light calculation.

### sub_596530 -- SetRimLightXOffset
- **Address:** `0x00596530`
- **Global:** `flt_1345648` (float)
- **Effect:** Sets the X-axis offset for rim light calculation.

### sub_596540 -- SetRimLightColor
- **Address:** `0x00596540`
- **Globals:**
  - `dword_728BA8` -- Red component
  - `dword_728BAC` -- Green component
  - `dword_728BB0` -- Blue component
- **Effect:** Sets the RGB color of the rim light. Components stored as separate DWORDs (likely float or int 0-255).

### Rim Light Globals Summary

| Address | Name | Type | Description |
|---------|------|------|-------------|
| `byte_728BA0` | RimLightEnable | `uint8` | 0=off, 1=on |
| `byte_728BA1` | RimLightModulation | `uint8` | 0=static, 1=modulated |
| `flt_728BA4` | RimLightZOffset | `float` | Z offset for rim calculation |
| `flt_1345648` | RimLightXOffset | `float` | X offset for rim calculation |
| `dword_728BA8` | RimLightColorR | `DWORD` | Red channel |
| `dword_728BAC` | RimLightColorG | `DWORD` | Green channel |
| `dword_728BB0` | RimLightColorB | `DWORD` | Blue channel |

---

## Fog System

**IMPORTANT:** Fog in this engine is **SOFTWARE-based**, not D3D hardware fog. The fog effect is computed via a lookup table (LUT) texture built by `sub_550BB0`. This means D3D fog render states have no effect -- fog must be controlled through these engine globals.

### sub_551090 -- SetFogEnable
- **Address:** `0x00551090`
- **Global:** `byte_E794F9` (uint8)
- **Effect:** Enables/disables the software fog system.

### sub_550FA0 -- SetFogColor
- **Address:** `0x00550FA0`
- **Globals:**
  - `dword_E794D8` -- Red component
  - `dword_E794AC` -- Green component (note: not contiguous with R)
  - `dword_E794A8` -- Blue component (note: shares range with FogStart, see below)
- **Note:** The non-contiguous layout of color components suggests the fog struct has other members interleaved.

### sub_551060 -- SetFogStart
- **Address:** `0x00551060`
- **Global:** `flt_E794A8` (float)
- **Effect:** Sets the fog start distance (for linear fog mode).

### sub_5510A0 -- SetFogDensity
- **Address:** `0x005510A0`
- **Global:** `flt_E794A0` (float)
- **Effect:** Sets the fog density (for exponential fog modes).

### Fog Type
- **Global:** `dword_E794B0` (DWORD)
- **Values:**

| Value | Mode | Description |
|-------|------|-------------|
| 0 | Linear | Fog increases linearly between start and end distances |
| 1 | Exp | Exponential fog falloff |
| 2 | Exp2 | Squared exponential fog falloff |
| 3 | Custom | Engine-specific custom fog curve |

### Fog LUT Builder
- **Function:** `sub_550BB0`
- **Purpose:** Builds the fog lookup table texture from the current fog parameters. Must be called after changing fog globals for the changes to take visual effect.

### Fog Globals Summary

| Address | Name | Type | Description |
|---------|------|------|-------------|
| `byte_E794F9` | FogEnable | `uint8` | 0=off, 1=on |
| `dword_E794D8` | FogColorR | `DWORD` | Red channel |
| `dword_E794AC` | FogColorG | `DWORD` | Green channel |
| `dword_E79498` | FogColorB | `DWORD` | Blue channel |
| `flt_E794A8` | FogStart | `float` | Start distance (linear mode) |
| `flt_E794A0` | FogDensity | `float` | Density (exp/exp2 modes) |
| `dword_E794B0` | FogType | `DWORD` | 0=linear, 1=exp, 2=exp2, 3=custom |

---

## Snow System

### sub_563EA0 -- SetSnowEnable
- **Address:** `0x00563EA0`
- **Global:** `byte_726F78` (uint8)
- **Effect:** Enables/disables the snow particle effect.

| Address | Name | Type | Description |
|---------|------|------|-------------|
| `byte_726F78` | SnowEnable | `uint8` | 0=off, 1=on |

---

## Notes
- All setter functions are simple single-instruction writes (e.g., `mov [global], param`). No validation, no side effects.
- The fog system's software LUT approach means fog parameters are baked into a texture each time they change. Modifying globals at runtime requires calling `sub_550BB0` to rebuild the LUT for changes to be visible.
- Rim light and snow toggles take effect immediately on the next frame.
- These globals are typically set by the Kallis script system during level loads and cutscenes, but can be overridden at runtime for debug or mod purposes.
