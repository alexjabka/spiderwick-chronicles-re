#pragma once
// ============================================================================
// ImGui menu — call Render() between NewFrame and EndFrame
// ============================================================================

namespace menu {

extern bool visible;

// Draw all ImGui windows (menu + freecam HUD overlay)
void Render();

} // namespace menu
