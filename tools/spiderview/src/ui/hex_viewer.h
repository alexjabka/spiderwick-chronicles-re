#pragma once
#include <cstdint>
#include <string>

namespace ui {
    // Draw a hex viewer for a memory region. Call within an ImGui window/child.
    // Returns true if still open.
    void DrawHexView(const uint8_t* data, uint32_t size, const std::string& title);
}
