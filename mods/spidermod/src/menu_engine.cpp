// ============================================================================
// Engine Debug — native debug features built into the Ogre engine
// Sector HUD, cheat flags, dev mode, engine console overlay
// ============================================================================

#include "menu_common.h"
#include "d3d_hook.h"
#include "addresses.h"
#include "memory.h"
#include "log.h"
#include <cstring>
#include <cstdio>

// Console overlay state — persists across menu open/close
static bool showConsoleOverlay = false;
static int  consoleLines       = 64;

// D3D wireframe toggle — applied each frame from EndScene
bool g_dbgWireframe  = false;

// Console history — snapshot the ring buffer so lines survive world loads
static char consoleHistory[256][256] = {};
static int  historyWritePos = 0;      // next slot to write
static int  historyCount    = 0;      // total lines captured (capped at 256)
static int  lastSeenHead    = -1;     // last write head we processed

// Map engine color code letter to ImGui color
static ImVec4 ColorCodeToImVec4(char code) {
    switch (code) {
        case 'g': return ImVec4(0.3f, 1.0f, 0.3f, 1.0f);  // green
        case 'r': return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // red
        case 'y': return ImVec4(1.0f, 1.0f, 0.3f, 1.0f);  // yellow
        case 'v': return ImVec4(0.7f, 0.3f, 1.0f, 1.0f);  // violet
        case 'o': return ImVec4(1.0f, 0.6f, 0.2f, 1.0f);  // orange
        case 'c': return ImVec4(0.3f, 0.8f, 1.0f, 1.0f);  // cyan
        default:  return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);  // white/default
    }
}

// Render a single console line with engine color codes parsed into ImGui colors
// Format: {g colored text} normal text {r error text}
static void RenderColoredLine(const char* src) {
    bool firstSegment = true;
    for (int i = 0; src[i]; ) {
        if (src[i] == '{' && src[i+1] && src[i+2]) {
            char colorCode = src[i+1];
            i += 2;  // skip {X

            // Extract text until closing }
            char segment[256];
            int si = 0;
            while (src[i] && src[i] != '}' && si < 254)
                segment[si++] = src[i++];
            segment[si] = 0;
            if (src[i] == '}') i++;

            if (si > 0) {
                if (!firstSegment) ImGui::SameLine(0, 0);
                ImGui::TextColored(ColorCodeToImVec4(colorCode), "%s", segment);
                firstSegment = false;
            }
        } else {
            // Plain text until next { or end
            char segment[256];
            int si = 0;
            while (src[i] && src[i] != '{' && si < 254)
                segment[si++] = src[i++];
            segment[si] = 0;

            if (si > 0) {
                if (!firstSegment) ImGui::SameLine(0, 0);
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", segment);
                firstSegment = false;
            }
        }
    }
    // Empty line — just advance
    if (firstSegment)
        ImGui::TextUnformatted("");
}

// ============================================================================
// Console overlay — always-on window, independent of menu visibility
// ============================================================================

void DrawConsoleToggle() {
    ImGui::Checkbox("Engine Console##consoleovl", &showConsoleOverlay);
    if (showConsoleOverlay) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::SliderInt("##consolelines", &consoleLines, 5, 256, "%d lines");
    }
}

// Capture new lines from engine ring buffer into our persistent history.
// Called from both EndScene (render) and CameraSectorUpdate (game update)
// for maximum coverage during loads.
void CaptureConsoleLines() {
    int writeHead = mem::Read<int>(addr::pConsoleWriteHead);
    if (writeHead < 0 || writeHead > 255) return;

    if (lastSeenHead == -1) {
        // First run — capture everything currently in buffer
        lastSeenHead = (writeHead + 1) % 256;  // start from oldest
    }

    // Calculate how many new lines appeared since last check
    int newLines = (writeHead - lastSeenHead + 256) % 256;
    if (newLines == 0) return;
    // Ring buffer is 256 — if we missed a full wrap, capture what's left (max 255)
    if (newLines > 255) newLines = 255;

    for (int i = 0; i < newLines; i++) {
        int srcIdx = (lastSeenHead + 1 + i) % 256;
        const char* rawLine = reinterpret_cast<const char*>(
            addr::pConsoleBuffer + srcIdx * 256);

        if (!rawLine[0]) continue;

        // Copy into our persistent history
        memcpy(consoleHistory[historyWritePos], rawLine, 256);
        consoleHistory[historyWritePos][255] = 0;
        historyWritePos = (historyWritePos + 1) % 256;
        if (historyCount < 256) historyCount++;
    }

    lastSeenHead = writeHead;
}

void DrawConsoleOverlay() {
    // Always capture, even when overlay is hidden (so we don't miss lines)
    CaptureConsoleLines();

    if (!showConsoleOverlay) return;
    if (historyCount == 0) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(10, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.55f,
        (consoleLines + 1) * ImGui::GetTextLineHeightWithSpacing() + 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGui::Begin("Engine Console##overlay", &showConsoleOverlay,
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

    // Show last N lines from our persistent history
    int displayCount = (consoleLines < historyCount) ? consoleLines : historyCount;

    if (ImGui::BeginChild("##cscroll", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for (int i = 0; i < displayCount; i++) {
            int idx = (historyWritePos - displayCount + i + 256) % 256;
            if (consoleHistory[idx][0])
                RenderColoredLine(consoleHistory[idx]);
        }

        // Auto-scroll to bottom when new content arrives
        static int prevCount = 0;
        if (historyCount != prevCount) {
            ImGui::SetScrollHereY(1.0f);
            prevCount = historyCount;
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

// ============================================================================
// Cheats section
// ============================================================================

void DrawCheatsSection() {
    // Dev Mode
    {
        uint8_t devMode = mem::Read<uint8_t>(addr::pDevCheatsEnabled);
        bool devOn = (devMode != 0);
        if (ImGui::Checkbox("Dev Mode##devmode", &devOn))
            mem::WriteDirect<uint8_t>(addr::pDevCheatsEnabled, devOn ? 1 : 0);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enables dev-only cheats (sprite/guard spawns)");
    }

    // Cheat Flags
    uintptr_t flagsBase = mem::Read<uintptr_t>(addr::pCheatFlagsArray);
    if (flagsBase && flagsBase > 0x10000 && flagsBase < 0x7FFFFFFF) {
        uint32_t flags = mem::Read<uint32_t>(flagsBase);

        auto cheatToggle = [&](const char* label, int bit) {
            bool on = (flags & (1 << bit)) != 0;
            if (ImGui::Checkbox(label, &on)) {
                uint32_t newFlags = on ? (flags | (1 << bit)) : (flags & ~(1 << bit));
                mem::WriteDirect<uint32_t>(flagsBase, newFlags);
                flags = newFlags;
            }
        };

        cheatToggle("Invulnerability##cheat", 1);
        ImGui::SameLine();
        cheatToggle("Infinite Ammo##cheat", 11);
        cheatToggle("Combat Weapons##cheat", 5);
        ImGui::SameLine();
        cheatToggle("Field Guide##cheat", 13);
        cheatToggle("Hide HUD##cheat", 2);
        ImGui::SameLine();
        if (ImGui::Button("Heal##cheat"))
            mem::WriteDirect<uint32_t>(flagsBase, flags | (1 << 6));
    } else {
        ImGui::TextDisabled("Cheat system not initialized");
    }
}

// ============================================================================
// Rendering section — engine globals + D3D overrides
// ============================================================================

void DrawRenderSection() {

    // Fog (software LUT-based, not D3D hardware fog)
    {
        uint8_t fogOn = mem::Read<uint8_t>(addr::pFogEnable);
        bool fog = (fogOn != 0);
        if (ImGui::Checkbox("Fog##render", &fog))
            mem::WriteDirect<uint8_t>(addr::pFogEnable, fog ? 1 : 0);

        if (fog) {
            ImGui::SameLine();
            float density = mem::Read<float>(addr::pFogDensity);
            ImGui::SetNextItemWidth(80);
            if (ImGui::SliderFloat("Density##fog", &density, 0.0f, 10.0f, "%.2f"))
                mem::WriteDirect<float>(addr::pFogDensity, density);

            float start = mem::Read<float>(addr::pFogStart);
            ImGui::SetNextItemWidth(120);
            if (ImGui::SliderFloat("Fog Start##fog", &start, 0.0f, 500.0f, "%.0f"))
                mem::WriteDirect<float>(addr::pFogStart, start);

            int fogType = mem::Read<int>(addr::pFogType);
            const char* fogTypes[] = { "Linear", "Exp", "Exp2", "Custom" };
            ImGui::SetNextItemWidth(100);
            if (ImGui::Combo("Fog Type##fog", &fogType, fogTypes, 4))
                mem::WriteDirect<int>(addr::pFogType, fogType);

            // Fog color
            int fr = mem::Read<int>(addr::pFogColorR);
            int fg = mem::Read<int>(addr::pFogColorG);
            int fb = mem::Read<int>(addr::pFogColorB);
            float fogCol[3] = { fr / 255.0f, fg / 255.0f, fb / 255.0f };
            if (ImGui::ColorEdit3("Fog Color##fog", fogCol, ImGuiColorEditFlags_NoInputs)) {
                mem::WriteDirect<int>(addr::pFogColorR, (int)(fogCol[0] * 255.0f));
                mem::WriteDirect<int>(addr::pFogColorG, (int)(fogCol[1] * 255.0f));
                mem::WriteDirect<int>(addr::pFogColorB, (int)(fogCol[2] * 255.0f));
            }
        }
    }

    // Snow
    {
        uint8_t snowOn = mem::Read<uint8_t>(addr::pSnowEnable);
        bool snow = (snowOn != 0);
        if (ImGui::Checkbox("Snow##render", &snow))
            mem::WriteDirect<uint8_t>(addr::pSnowEnable, snow ? 1 : 0);
    }

    // Rim light
    {
        uint8_t rimOn = mem::Read<uint8_t>(addr::pRimLightEnable);
        bool rim = (rimOn != 0);
        if (ImGui::Checkbox("Rim Light##render", &rim))
            mem::WriteDirect<uint8_t>(addr::pRimLightEnable, rim ? 1 : 0);

        if (rim) {
            uint8_t rimMod = mem::Read<uint8_t>(addr::pRimLightModulation);
            bool mod = (rimMod != 0);
            ImGui::SameLine();
            if (ImGui::Checkbox("Modulation##rim", &mod))
                mem::WriteDirect<uint8_t>(addr::pRimLightModulation, mod ? 1 : 0);

            float zOff = mem::Read<float>(addr::pRimLightZOffset);
            ImGui::SetNextItemWidth(120);
            if (ImGui::SliderFloat("Z Offset##rim", &zOff, -10.0f, 10.0f, "%.2f"))
                mem::WriteDirect<float>(addr::pRimLightZOffset, zOff);

            int rr = mem::Read<int>(addr::pRimLightColorR);
            int rg = mem::Read<int>(addr::pRimLightColorG);
            int rb = mem::Read<int>(addr::pRimLightColorB);
            float rimCol[3] = { rr / 255.0f, rg / 255.0f, rb / 255.0f };
            if (ImGui::ColorEdit3("Rim Color##rim", rimCol, ImGuiColorEditFlags_NoInputs)) {
                mem::WriteDirect<int>(addr::pRimLightColorR, (int)(rimCol[0] * 255.0f));
                mem::WriteDirect<int>(addr::pRimLightColorG, (int)(rimCol[1] * 255.0f));
                mem::WriteDirect<int>(addr::pRimLightColorB, (int)(rimCol[2] * 255.0f));
            }
        }
    }

    // ---- D3D Render Overrides (per draw call via DIP hook) ----
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "D3D Override");
    ImGui::Checkbox("Wireframe##d3d", &g_dbgWireframe);
    ImGui::SameLine();
    ImGui::Checkbox("No Textures##d3d", &d3dhook::dbgNoTextures);
    ImGui::Checkbox("No Shaders##d3d", &d3dhook::dbgNoShaders);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("Ambient##d3d", &d3dhook::dbgAmbientLevel, 0.0f, 1.0f, "%.2f");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 = engine default\n>0 = force ambient light");
}
