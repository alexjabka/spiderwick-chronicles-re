#pragma once
// ============================================================================
// Freecam module — toggle/update free camera
// ============================================================================

namespace freecam {

extern bool  enabled;
extern float posX, posY, posZ;
extern float yaw, pitch;
extern float speed;
extern float mouseSens;

// Toggle freecam on/off — installs/removes hooks
void Toggle();

// Per-frame update — movement, rotation, sector tracking
// Call from EndScene hook (runs in game thread!)
void Update();

// Config persistence (speed, mouseSens → spidermod.cfg)
void SaveConfig();
void LoadConfig();

} // namespace freecam
