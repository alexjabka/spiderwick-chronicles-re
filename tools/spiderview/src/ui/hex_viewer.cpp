// ============================================================================
// hex_viewer.cpp — Hex dump viewer for raw asset data
// ============================================================================

#include "hex_viewer.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

void ui::DrawHexView(const uint8_t* data, uint32_t size, const std::string& title) {
    if (!data || size == 0) {
        ImGui::TextDisabled("No data");
        return;
    }

    ImGui::Text("%s  (%u bytes / 0x%X)", title.c_str(), size, size);
    ImGui::Separator();

    // Controls
    static int bytesPerRow = 16;
    static int startOffset = 0;
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("Offset##hex", &startOffset, 0x100, 0x1000);
    if (startOffset < 0) startOffset = 0;
    if (startOffset >= (int)size) startOffset = size > 0x100 ? size - 0x100 : 0;
    startOffset &= ~0xF; // align to 16

    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    if (ImGui::BeginCombo("##bpr", bytesPerRow == 16 ? "16" : "32")) {
        if (ImGui::Selectable("16", bytesPerRow == 16)) bytesPerRow = 16;
        if (ImGui::Selectable("32", bytesPerRow == 32)) bytesPerRow = 32;
        ImGui::EndCombo();
    }

    ImGui::BeginChild("##hexdata", ImVec2(0, 0), ImGuiChildFlags_None,
                       ImGuiWindowFlags_HorizontalScrollbar);

    // Use monospace rendering
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    int visibleRows = (int)(ImGui::GetContentRegionAvail().y / ImGui::GetTextLineHeight());
    int totalRows = (size + bytesPerRow - 1) / bytesPerRow;

    // Clipper for large data
    ImGuiListClipper clipper;
    clipper.Begin(totalRows);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            uint32_t rowOff = (uint32_t)row * bytesPerRow;
            if (rowOff >= size) break;

            char line[256];
            int pos = snprintf(line, sizeof(line), "%08X  ", rowOff);

            // Hex bytes
            for (int col = 0; col < bytesPerRow; col++) {
                uint32_t idx = rowOff + col;
                if (idx < size)
                    pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[idx]);
                else
                    pos += snprintf(line + pos, sizeof(line) - pos, "   ");
                if (col == 7) line[pos++] = ' '; // extra space at midpoint
            }

            // ASCII
            pos += snprintf(line + pos, sizeof(line) - pos, " |");
            for (int col = 0; col < bytesPerRow; col++) {
                uint32_t idx = rowOff + col;
                if (idx < size) {
                    uint8_t c = data[idx];
                    line[pos++] = (c >= 32 && c < 127) ? (char)c : '.';
                } else {
                    line[pos++] = ' ';
                }
            }
            line[pos++] = '|';
            line[pos] = 0;

            ImGui::TextUnformatted(line);
        }
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}
