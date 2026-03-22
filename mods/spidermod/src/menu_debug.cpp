// ============================================================================
// Debug overlay, 3D overlay, sector table, camera raw data, VM runtime
// ============================================================================

#include "menu_common.h"
#include "addresses.h"
#include "d3d_hook.h"
#include "memory.h"
#include "log.h"
#include <cstdio>

// PropPlacement declared in menu_common.h

static bool showDebugOverlay = false;

// ============================================================================
// 3D Debug Overlay — world-space text labels
// ============================================================================

void Draw3DOverlay() {
    if (!dbg3d_enabled || !IsWorldValid()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // ---- Characters ----
    if (dbg3d_characters) {
        typedef uintptr_t (__cdecl *GetPlayerFn)();
        auto getPlayer = reinterpret_cast<GetPlayerFn>(addr::fn_GetPlayerCharacter);
        uintptr_t currentPlayer = getPlayer();

        uintptr_t cur = mem::Read<uintptr_t>(addr::pCharacterListHead);
        for (int i = 0; cur && i < 64; i++) {
            float px = mem::Read<float>(cur + addr::CHAR_POS_X);
            float py = mem::Read<float>(cur + addr::CHAR_POS_Y);
            float pz = mem::Read<float>(cur + addr::CHAR_POS_Z);

            const char* name = IdentifyCharacter(cur);
            bool isCurrent = (cur == currentPlayer);

            ImU32 color = isCurrent ? IM_COL32(80, 255, 80, 230) :
                                      IM_COL32(255, 255, 100, 200);

            char label[128];
            if (dbg3d_showCoords && dbg3d_showAddrs)
                snprintf(label, sizeof(label), "%s\n(%.0f,%.0f,%.0f)\n[0x%X]", name, px, py, pz, cur);
            else if (dbg3d_showCoords)
                snprintf(label, sizeof(label), "%s\n(%.0f,%.0f,%.0f)", name, px, py, pz);
            else if (dbg3d_showAddrs)
                snprintf(label, sizeof(label), "%s [0x%X]", name, cur);
            else
                snprintf(label, sizeof(label), "%s", name);

            DrawText3D(dl, px, py, pz + 2.0f, label, color);

            float sx, sy;
            if (WorldToScreen(px, py, pz, sx, sy))
                dl->AddCircleFilled(ImVec2(sx, sy), 3.0f, color);

            cur = mem::Read<uintptr_t>(cur + addr::CHAR_NEXT);
        }
    }

    // ---- Props (from PCWB world data in RAM) ----
    if (dbg3d_props || dbg3d_worldChunks) {
        typedef unsigned int (__cdecl *CalcAddrFn)(unsigned int);
        auto calcAddr = reinterpret_cast<CalcAddrFn>(addr::fn_StreamingCalcAddr);

        __try {
            uintptr_t worldBase = static_cast<uintptr_t>(calcAddr(0));
            if (worldBase > 0x10000 && worldBase < 0x7FFFFFFF) {
                uint32_t magic = mem::Read<uint32_t>(worldBase);
                if (magic == 0x42574350) {  // "PCWB"
                    uint32_t pageSize = mem::Read<uint32_t>(worldBase + 0x08);
                    uintptr_t propBase = worldBase + pageSize;
                    uint32_t propCount = mem::Read<uint32_t>(worldBase + 0x50);
                    if (propCount > 64) propCount = 64;

                    // ---- Props with full info ----
                    if (dbg3d_props) {
                        for (uint32_t pi = 0; pi < propCount; pi++) {
                            uintptr_t entry = propBase + pi * 0xA0;

                            uint8_t firstChar = mem::Read<uint8_t>(entry + 0x60);
                            if (firstChar < 0x20 || firstChar > 0x7E) break;

                            char propName[32] = {};
                            for (int ci = 0; ci < 31; ci++) {
                                uint8_t c = mem::Read<uint8_t>(entry + 0x60 + ci);
                                if (c == 0) break;
                                propName[ci] = static_cast<char>(c);
                            }

                            float px = mem::Read<float>(entry + 0x30);
                            float py = mem::Read<float>(entry + 0x34);
                            float pz = mem::Read<float>(entry + 0x38);
                            float pw = mem::Read<float>(entry + 0x3C);
                            if (pw < 0.9f || pw > 1.1f) continue;

                            // Read prop definition
                            uint32_t defPtr = mem::Read<uint32_t>(entry + 0x8C);
                            uint32_t pcrdCount = 0, propType = 0;
                            if (defPtr > 0x1000) {
                                pcrdCount = mem::Read<uint32_t>(defPtr + 4);
                                propType = mem::Read<uint32_t>(defPtr + 12);
                            }
                            const char* typeStr = (propType == 2) ? "ANIM" : "STAT";

                            bool underground = (py < -10.0f);
                            ImU32 color = (propType == 2)  ? IM_COL32(255, 180, 50, 220) :  // orange=animated
                                          underground      ? IM_COL32(255, 100, 255, 220) :  // magenta=underground
                                                             IM_COL32(100, 255, 255, 220);   // cyan=static

                            char label[160];
                            snprintf(label, sizeof(label), "[%d] %s (%s)\n%d PCRDs | (%.1f, %.1f, %.1f)%s",
                                     pi, propName, typeStr, pcrdCount,
                                     px, py, pz, underground ? " [UG]" : "");

                            DrawText3D(dl, px, py, pz + 3.0f, label, color);

                            float sx, sy;
                            if (WorldToScreen(px, py, pz, sx, sy)) {
                                dl->AddCircleFilled(ImVec2(sx, sy), 5.0f, color);
                                dl->AddCircle(ImVec2(sx, sy), 8.0f, color, 0, 2.0f);
                            }
                        }
                    }

                    // ---- World Chunks: trace prop PCRDs via mesh chain ----
                    if (dbg3d_worldChunks) {
                        int chunkCount = 0;
                        for (uint32_t pi = 0; pi < propCount && chunkCount < 256; pi++) {
                            uintptr_t entry = propBase + pi * 0xA0;
                            uint8_t fc = mem::Read<uint8_t>(entry + 0x60);
                            if (fc < 0x20 || fc > 0x7E) break;

                            char pn[32] = {};
                            for (int ci = 0; ci < 31; ci++) {
                                uint8_t c = mem::Read<uint8_t>(entry + 0x60 + ci);
                                if (c == 0) break;
                                pn[ci] = (char)c;
                            }

                            uint32_t defPtr = mem::Read<uint32_t>(entry + 0x8C);
                            if (defPtr < 0x1000) continue;
                            uint32_t pcrdCnt = mem::Read<uint32_t>(defPtr + 4);
                            uint32_t meshList = mem::Read<uint32_t>(defPtr + 8);
                            uint32_t pType = mem::Read<uint32_t>(defPtr + 12);
                            if (pcrdCnt == 0 || pcrdCnt > 1000 || meshList == 0) continue;

                            int subIdx = 0;
                            for (uint32_t mi = 0; mi < pcrdCnt && chunkCount < 256; mi++) {
                                uint32_t blockOff = mem::Read<uint32_t>(meshList + mi * 4);
                                if (blockOff < 0x1000) continue;
                                uint32_t subCnt = mem::Read<uint32_t>(blockOff);
                                uint32_t subList = mem::Read<uint32_t>(blockOff + 16);
                                if (subCnt == 0 || subCnt > 100 || subList == 0) continue;

                                for (uint32_t si = 0; si < subCnt && chunkCount < 256; si++) {
                                    uint32_t subPtr = mem::Read<uint32_t>(subList + si * 4);
                                    if (subPtr < 0x1000) continue;
                                    uint32_t batchPtr = mem::Read<uint32_t>(subPtr + 12);
                                    if (batchPtr < 0x1000) continue;

                                    // batch+44 = PCRD file offset
                                    uint32_t pcrdFileOff = mem::Read<uint32_t>(batchPtr + 44);

                                    // Use calcAddr to resolve PCRD in RAM
                                    uintptr_t pcrdAddr = static_cast<uintptr_t>(calcAddr(pcrdFileOff));
                                    if (pcrdAddr < 0x10000 || pcrdAddr > 0x7FFFFFFF) continue;
                                    uint32_t pcrdMagic = mem::Read<uint32_t>(pcrdAddr);
                                    if (pcrdMagic != 0x44524350) continue; // "PCRD"

                                    uint32_t vc = mem::Read<uint32_t>(pcrdAddr + 0x10);
                                    uint32_t ic = mem::Read<uint32_t>(pcrdAddr + 0x0C);
                                    uint32_t vo = mem::Read<uint32_t>(pcrdAddr + 0x18);

                                    // Resolve vertex data via calcAddr
                                    uintptr_t vtxAddr = static_cast<uintptr_t>(calcAddr(vo));
                                    if (vtxAddr < 0x10000) continue;

                                    float vx = mem::Read<float>(vtxAddr);
                                    float vy = mem::Read<float>(vtxAddr + 4);
                                    float vz = mem::Read<float>(vtxAddr + 8);

                                    ImU32 color = (pType == 2) ? IM_COL32(255, 180, 50, 180) :
                                                                  IM_COL32(80, 200, 255, 180);

                                    char label[128];
                                    snprintf(label, sizeof(label), "%s[%d]\nPCRD@%X v=%d\n(%.1f,%.1f,%.1f)",
                                             pn, subIdx, pcrdFileOff, vc, vx, vy, vz);

                                    DrawText3D(dl, vx, vy, vz + 2.0f, label, color);
                                    chunkCount++;
                                    subIdx++;
                                }
                            }
                        }
                    }
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            // World not loaded or streaming address invalid
        }
    }

    // ---- Sectors ----
    if (dbg3d_sectors) {
        uintptr_t ws = mem::Deref(addr::pWorldState);
        int secCnt = mem::Read<int>(addr::pSectorCount);
        uintptr_t camObj = mem::Deref(addr::pCameraObj);
        int camSector = camObj ? mem::Read<int>(camObj + addr::CAM_SECTOR) : -1;

        if (ws && secCnt > 0 && secCnt <= 64) {
            uintptr_t secArr = mem::Deref(ws + 0x64);
            if (secArr) {
                for (int i = 0; i < secCnt; i++) {
                    uintptr_t sd = mem::Deref(secArr + 4 * i);
                    if (!sd) continue;

                    uint8_t firstByte = mem::Read<uint8_t>(sd);
                    if (firstByte < 0x20 || firstByte > 0x7E) continue;
                    const char* secName = reinterpret_cast<const char*>(sd);

                    float minX = mem::Read<float>(sd + 0x10);
                    float minY = mem::Read<float>(sd + 0x14);
                    float minZ = mem::Read<float>(sd + 0x18);
                    float maxX = mem::Read<float>(sd + 0x20);
                    float maxY = mem::Read<float>(sd + 0x24);
                    float maxZ = mem::Read<float>(sd + 0x28);

                    float cx = (minX + maxX) * 0.5f;
                    float cy = (minY + maxY) * 0.5f;
                    float cz = (minZ + maxZ) * 0.5f;

                    int loadState = mem::Read<int>(addr::pSectorStateArray + i * 12);
                    bool loaded = (loadState == 3);

                    ImU32 color = (i == camSector) ? IM_COL32(100, 200, 255, 220) :
                                  loaded           ? IM_COL32(100, 255, 100, 180) :
                                                     IM_COL32(255, 100, 100, 140);

                    char label[128];
                    if (dbg3d_showCoords)
                        snprintf(label, sizeof(label), "[%d] %s\n(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)",
                                 i, secName, minX, minY, minZ, maxX, maxY, maxZ);
                    else
                        snprintf(label, sizeof(label), "[%d] %s%s",
                                 i, secName, loaded ? "" : " (unloaded)");

                    DrawText3D(dl, cx, cy, cz, label, color);

                    // AABB wireframe
                    float corners[8][3] = {
                        {minX,minY,minZ},{maxX,minY,minZ},{maxX,maxY,minZ},{minX,maxY,minZ},
                        {minX,minY,maxZ},{maxX,minY,maxZ},{maxX,maxY,maxZ},{minX,maxY,maxZ}
                    };
                    float sc[8][2];
                    bool vis[8];
                    for (int j = 0; j < 8; j++)
                        vis[j] = WorldToScreen(corners[j][0], corners[j][1], corners[j][2], sc[j][0], sc[j][1]);

                    int edges[][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
                    ImU32 lineCol = (i == camSector) ? IM_COL32(100, 200, 255, 100) :
                                    loaded           ? IM_COL32(100, 255, 100, 60) :
                                                       IM_COL32(255, 100, 100, 40);
                    for (auto& e : edges) {
                        if (vis[e[0]] && vis[e[1]])
                            dl->AddLine(ImVec2(sc[e[0]][0], sc[e[0]][1]),
                                        ImVec2(sc[e[1]][0], sc[e[1]][1]), lineCol, 1.0f);
                    }
                }
            }
        }
    }

    // ---- VM Placements (sauSetPosition/sauSetRotation captures) ----
    if (dbg3d_vmPlacements) {
        long cnt = g_propPlacementCount;
        if (cnt > 512) cnt = 512;
        for (long i = 0; i < cnt; i++) {
            auto& p = g_propPlacements[i];
            if (!p.hasPos) continue;

            float px = p.pos[0], py = p.pos[1], pz = p.pos[2];

            // Read class name via VM object back-pointer:
            // nativeObj+0xA8 → VM object, vmObj+12 → class name char*
            char objName[48] = "???";
            if (p.name[0]) {
                strncpy(objName, p.name, 47);
                objName[47] = 0;
            } else {
                __try {
                    uintptr_t vmObj = mem::Read<uintptr_t>(p.objPtr + 0xA8);
                    if (vmObj > 0x10000 && vmObj < 0x7FFFFFFF) {
                        uintptr_t nameStr = mem::Read<uintptr_t>(vmObj + 12);
                        if (nameStr > 0x10000 && nameStr < 0x7FFFFFFF) {
                            for (int ci = 0; ci < 47; ci++) {
                                uint8_t c = mem::Read<uint8_t>(nameStr + ci);
                                if (c == 0 || c < 0x20 || c > 0x7E) break;
                                objName[ci] = (char)c;
                                objName[ci+1] = 0;
                            }
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }

            ImU32 color = IM_COL32(50, 255, 50, 230);  // bright green for VM-placed

            char label[160];
            if (p.hasRot)
                snprintf(label, sizeof(label), "VM[%ld] %s\npos(%.1f,%.1f,%.1f)\nrot(%.1f,%.1f,%.1f)\n@0x%X",
                         i, objName, px, py, pz, p.rot[0], p.rot[1], p.rot[2], p.objPtr);
            else
                snprintf(label, sizeof(label), "VM[%ld] %s\npos(%.1f,%.1f,%.1f)\n@0x%X",
                         i, objName, px, py, pz, p.objPtr);

            DrawText3D(dl, px, py, pz + 2.5f, label, color);

            float sx, sy;
            if (WorldToScreen(px, py, pz, sx, sy)) {
                dl->AddCircleFilled(ImVec2(sx, sy), 6.0f, color);
                dl->AddCircle(ImVec2(sx, sy), 10.0f, IM_COL32(50, 255, 50, 150), 0, 2.0f);
            }
        }
    }
}

// ============================================================================
// Debug overlay — top-right corner, always-on when enabled
// ============================================================================

void DrawDebugOverlay() {
    if (!showDebugOverlay) return;
    if (!IsWorldValid()) return;

    int secCnt = mem::Read<int>(addr::pSectorCount);
    uintptr_t camObj = mem::Deref(addr::pCameraObj);
    int camSector = camObj ? mem::Read<int>(camObj + addr::CAM_SECTOR) : -1;

    const char* worldInternal = GetCurrentLevelName();
    const char* worldFriendly = WorldFriendlyName(worldInternal);
    const char* sectorName = (camSector >= 0) ? GetSectorName(camSector) : nullptr;

    int loadedCount = 0, visibleCount = 0;
    uintptr_t visArr = mem::Read<uintptr_t>(addr::pVisibilityArray);
    for (int i = 0; i < secCnt && i < 64; i++) {
        int state = mem::Read<int>(addr::pSectorStateArray + i * 12);
        if (state == 3) loadedCount++;
        if (visArr) {
            uint8_t vis = mem::Read<uint8_t>(visArr + i * 8);
            if (vis) visibleCount++;
        }
    }

    int rqCount = mem::Read<int>(addr::pRenderQueueCount);
    int vmrCount = mem::Read<int>(addr::pVMRenderCount);

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 380, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##debug", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "-- Debug --");
    ImGui::Text("World: %s (%s)", worldFriendly, worldInternal);

    if (camObj) {
        float cx = mem::Read<float>(camObj + addr::CAM_POS_X);
        float cy = mem::Read<float>(camObj + addr::CAM_POS_Y);
        float cz = mem::Read<float>(camObj + addr::CAM_POS_Z);
        ImGui::Text("Cam: (%.1f, %.1f, %.1f)", cx, cy, cz);
    }
    if (sectorName)
        ImGui::Text("Sector: %d \"%s\" (%d total)", camSector, sectorName, secCnt);
    else
        ImGui::Text("Sector: %d / %d", camSector, secCnt);

    uintptr_t ws = mem::Deref(addr::pWorldState);
    if (ws && camSector >= 0 && camSector < secCnt) {
        uintptr_t secArr = mem::Deref(ws + 0x64);
        if (secArr) {
            uintptr_t sd = mem::Deref(secArr + 4 * camSector);
            if (sd) {
                int portals = mem::Read<int>(sd + 0x5C);
                ImGui::Text("Portals: %d | RenderID: %d", portals, mem::Read<int>(sd + 0x80));
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Loaded: %d | Visible: %d | RQ: %d | VMR: %d",
        loadedCount, visibleCount, rqCount, vmrCount);

    ImGui::End();
}

// ============================================================================
// Debug section in main menu — sector table, camera raw, VM runtime
// ============================================================================

// Toggle checkboxes shown in the main Debug section header
void DrawDebugSectionToggles() {
    ImGui::Checkbox("Info Overlay##dbg", &showDebugOverlay);
}

void DrawDebugSection() {
    if (!IsWorldValid()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "World loading...");
        return;
    }

    uintptr_t ws = mem::Deref(addr::pWorldState);
    int secCnt = mem::Read<int>(addr::pSectorCount);
    uintptr_t visArr = mem::Read<uintptr_t>(addr::pVisibilityArray);
    uintptr_t camObj = mem::Deref(addr::pCameraObj);
    int camSector = camObj ? mem::Read<int>(camObj + addr::CAM_SECTOR) : -1;

    const char* worldInternal = GetCurrentLevelName();
    ImGui::Text("World: %s (%s)", WorldFriendlyName(worldInternal), worldInternal);
    ImGui::Text("WorldState: 0x%X | Sectors: %d", ws, secCnt);

    const char* curSectorName = (camSector >= 0) ? GetSectorName(camSector) : nullptr;
    if (curSectorName)
        ImGui::Text("Camera sector: %d \"%s\"", camSector, curSectorName);
    else
        ImGui::Text("Camera sector: %d", camSector);

    // Sector table
    if (secCnt > 0 && secCnt <= 64 && ImGui::BeginTable("sectors", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 20);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Vis", ImGuiTableColumnFlags_WidthFixed, 25);
        ImGui::TableSetupColumn("Ptl", ImGuiTableColumnFlags_WidthFixed, 25);
        ImGui::TableSetupColumn("Frustum", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        uintptr_t secArr = ws ? mem::Deref(ws + 0x64) : 0;

        for (int i = 0; i < secCnt; i++) {
            ImGui::TableNextRow();

            int loadState = mem::Read<int>(addr::pSectorStateArray + i * 12);
            bool loaded = (loadState == 3);
            uint8_t vis = 0;
            uint32_t frustum = 0;
            int portals = 0;
            const char* secName = nullptr;
            if (visArr) {
                vis = mem::Read<uint8_t>(visArr + i * 8);
                frustum = mem::Read<uint32_t>(visArr + i * 8 + 4);
            }
            if (secArr) {
                uintptr_t sd = mem::Deref(secArr + 4 * i);
                if (sd) {
                    portals = mem::Read<int>(sd + 0x5C);
                    secName = reinterpret_cast<const char*>(sd);
                }
            }

            ImVec4 color;
            if (i == camSector)
                color = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);
            else if (loaded && vis)
                color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            else if (loaded)
                color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
            else
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(color, "%d", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(color, "%s", secName ? secName : "?");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(color, "%s", loaded ? "OK" : "--");
            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(color, "%s", vis ? "Y" : "N");
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", portals);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("0x%08X", frustum);
        }
        ImGui::EndTable();
    }

    // Render pipeline stats
    ImGui::Separator();
    int rqCount = mem::Read<int>(addr::pRenderQueueCount);
    int vmrCount = mem::Read<int>(addr::pVMRenderCount);
    ImGui::Text("Render queue: %d entries", rqCount);
    ImGui::Text("VM render: %d entries (cap 5)", vmrCount);

    // Dump world props to file
    if (ImGui::Button("Dump Props to File")) {
        typedef unsigned int (__cdecl *CalcAddrFn)(unsigned int);
        auto calcAddr = reinterpret_cast<CalcAddrFn>(addr::fn_StreamingCalcAddr);
        int propsDumped = 0;

        __try {
            uintptr_t worldBase = static_cast<uintptr_t>(calcAddr(0));
            if (worldBase > 0x10000 && worldBase < 0x7FFFFFFF &&
                mem::Read<uint32_t>(worldBase) == 0x42574350) {  // "PCWB"

                uint32_t pageSize = mem::Read<uint32_t>(worldBase + 0x08);
                uintptr_t propBase = worldBase + pageSize;

                const char* levelName = GetCurrentLevelName();
                char dumpPath[256];
                snprintf(dumpPath, sizeof(dumpPath), "prop_dump_%s.txt", levelName ? levelName : "unknown");

                FILE* f = fopen(dumpPath, "w");
                if (f) {
                    fprintf(f, "# World Props Dump: %s\n", levelName ? levelName : "unknown");
                    fprintf(f, "# name\ttype\tstate\tpos_x\tpos_y\tpos_z\tmat00\tmat01\tmat02\tmat10\tmat11\tmat12\tmat20\tmat21\tmat22\n");

                    // Also read prop def table (stride 24, starts after props)
                    // Count props first
                    int nProps = 0;
                    for (int pi = 0; pi < 128; pi++) {
                        uintptr_t entry = propBase + pi * 0xA0;
                        uint8_t c = mem::Read<uint8_t>(entry + 0x60);
                        if (c < 0x20 || c > 0x7E) break;
                        nProps++;
                    }

                    uintptr_t defBase = propBase + nProps * 0xA0;

                    for (int pi = 0; pi < nProps; pi++) {
                        uintptr_t entry = propBase + pi * 0xA0;

                        // Read name
                        char name[32] = {};
                        for (int ci = 0; ci < 31; ci++) {
                            uint8_t c = mem::Read<uint8_t>(entry + 0x60 + ci);
                            if (c == 0) break;
                            name[ci] = static_cast<char>(c);
                        }

                        // Read 4x4 matrix
                        float mat[4][4];
                        for (int r = 0; r < 4; r++)
                            for (int c = 0; c < 4; c++)
                                mat[r][c] = mem::Read<float>(entry + (r * 4 + c) * 4);

                        // Read prop def entry
                        uint32_t dType = mem::Read<uint32_t>(defBase + pi * 24 + 4);
                        uint32_t dNameOff = mem::Read<uint32_t>(defBase + pi * 24 + 12);
                        char stateName[32] = {};
                        __try {
                            uintptr_t stateAddr = worldBase + (dNameOff - pageSize);
                            // dNameOff is absolute within PCWB in file, translate to RAM
                            // Actually it's stored as absolute offset from file start
                            // In RAM: worldBase + file_offset / pageSize... complex
                            // Simpler: just try reading from calcAddr
                            uintptr_t sAddr = static_cast<uintptr_t>(calcAddr(dNameOff));
                            for (int ci = 0; ci < 31; ci++) {
                                uint8_t c = mem::Read<uint8_t>(sAddr + ci);
                                if (c == 0 || c < 0x20 || c > 0x7E) break;
                                stateName[ci] = static_cast<char>(c);
                            }
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            strncpy(stateName, "?", 2);
                        }

                        const char* typeStr = (dType == 2) ? "ANIMATED" : "STATIC";

                        fprintf(f, "%s\t%s\t%s\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\n",
                                name, typeStr, stateName,
                                mat[3][0], mat[3][1], mat[3][2],
                                mat[0][0], mat[0][1], mat[0][2],
                                mat[1][0], mat[1][1], mat[1][2],
                                mat[2][0], mat[2][1], mat[2][2]);
                        propsDumped++;
                    }
                    fclose(f);
                    log::Write("Dumped %d props to %s", propsDumped, dumpPath);
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            log::Write("Prop dump failed (exception)");
        }

        if (propsDumped > 0)
            log::Write("Props dumped: %d", propsDumped);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(saves prop_dump_<level>.txt)");

    // VM prop placements (sauSetPosition/sauSetRotation captures)
    ImGui::Text("VM Placements: %ld objects", g_propPlacementCount);
    ImGui::SameLine();
    if (ImGui::Button("Dump VM Placements")) {
        const char* levelName = GetCurrentLevelName();
        char path[256];
        snprintf(path, sizeof(path), "vm_placements_%s.txt", levelName ? levelName : "unknown");
        FILE* f = fopen(path, "w");
        if (f) {
            long cnt = g_propPlacementCount;
            if (cnt > MAX_PROP_PLACEMENTS) cnt = MAX_PROP_PLACEMENTS;
            fprintf(f, "# VM sauSetPosition/sauSetRotation captures: %ld objects\n", cnt);
            fprintf(f, "# name\tobj_addr\tpos_x\tpos_y\tpos_z\trot_x\trot_y\trot_z\thas_pos\thas_rot\n");
            for (long i = 0; i < cnt; i++) {
                auto& p = g_propPlacements[i];
                fprintf(f, "%s\t0x%08X\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%d\t%d\n",
                    p.name[0] ? p.name : "???",
                    p.objPtr, p.pos[0], p.pos[1], p.pos[2],
                    p.rot[0], p.rot[1], p.rot[2], p.hasPos, p.hasRot);
            }
            fclose(f);
            log::Write("Dumped %ld VM placements to %s", cnt, path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##vm")) {
        g_propPlacementCount = 0;
    }

    // Geometry instances captured by GeomInstance_Init hook
    {
    // GeomInstance declared in menu_common.h
    ImGui::Text("Geometry Instances: %ld", g_geomInstanceCount);
    ImGui::SameLine();
    if (ImGui::Button("Dump Geom Instances (hooked)")) {
        const char* levelName = GetCurrentLevelName();
        char path[256];
        snprintf(path, sizeof(path), "geom_world_%s.txt", levelName ? levelName : "unknown");
        FILE* gf = fopen(path, "w");
        if (gf) {
            long cnt = g_geomInstanceCount;
            if (cnt > 2048) cnt = 2048;
            fprintf(gf, "# GeomInstance_Init hook captures: %ld instances\n", cnt);
            fprintf(gf, "# idx\tinst_addr\tpos_x\tpos_y\tpos_z\tvc\tic\tflags\tmat (16 floats row-major)\n");
            for (long i = 0; i < cnt; i++) {
                auto& gi = g_geomInstances[i];
                // Read extra metadata from instance struct
                uint32_t vc = 0, ic = 0, flags = 0;
                __try {
                    vc = mem::Read<uint32_t>(gi.addr + 0x70);
                    ic = mem::Read<uint32_t>(gi.addr + 0x74);
                    flags = mem::Read<uint32_t>(gi.addr + 0x7C);
                } __except(EXCEPTION_EXECUTE_HANDLER) {}

                fprintf(gf, "%ld\t0x%08X\t%.4f\t%.4f\t%.4f\t%u\t%u\t0x%08X",
                    i, gi.addr, gi.worldMatrix[12], gi.worldMatrix[13], gi.worldMatrix[14],
                    vc, ic, flags);
                for (int j = 0; j < 16; j++)
                    fprintf(gf, "\t%.6f", gi.worldMatrix[j]);
                fprintf(gf, "\n");
            }
            fclose(gf);
            log::Write("Dumped %ld geometry instances to %s", cnt, path);
        }
    }
    }

    // Dump geometry instances by traversing BSP tree (fallback)
    if (ImGui::Button("Dump Geom BSP (old)")) {
        const char* levelName = GetCurrentLevelName();
        char path[256];
        snprintf(path, sizeof(path), "geom_instances_%s.txt", levelName ? levelName : "unknown");
        int totalInstances = 0;

        __try {
            uintptr_t ws = mem::Read<uintptr_t>(addr::pWorldState);
            int secCnt = mem::Read<int>(addr::pSectorCount);
            FILE* f = fopen(path, "w");
            if (f && ws && secCnt > 0) {
                fprintf(f, "# Geometry instances from BSP tree traversal\n");
                fprintf(f, "# sector\tnode_addr\tslot\tinst_addr\tinst_bytes(128)\n");

                uintptr_t secArr = mem::Read<uintptr_t>(ws + 0x64);
                for (int si = 0; si < secCnt && si < 64; si++) {
                    uintptr_t sd = mem::Read<uintptr_t>(secArr + 4 * si);
                    if (!sd) continue;

                    int geomCount = mem::Read<int>(sd + 0xAC);
                    fprintf(f, "# Sector %d: %d geometry instances\n", si, geomCount);
                    if (geomCount == 0) continue;

                    // BSP tree root at sector+0x60
                    uintptr_t bspRoot = mem::Read<uintptr_t>(sd + 0x60);
                    if (!bspRoot || bspRoot < 0x10000) continue;

                    // Traverse BSP tree — collect ALL geometry instances
                    // Node layout (from sub_57F690):
                    //   +0x44 = child count, +0x20 = children[], +0x60 = instance ptr array
                    uintptr_t stack[1024];
                    int sp = 0;
                    stack[sp++] = bspRoot;

                    while (sp > 0 && totalInstances < 2000) {
                        uintptr_t node = stack[--sp];
                        if (node < 0x10000 || node > 0x7FFFFFFF) continue;

                        // Read instances at this node
                        uintptr_t instArr = mem::Read<uintptr_t>(node + 0x60);
                        if (instArr > 0x10000 && instArr < 0x7FFFFFFF) {
                            for (int ii = 0; ii < 512; ii++) {
                                uintptr_t inst = mem::Read<uintptr_t>(instArr + ii * 4);
                                if (!inst || inst < 0x10000 || inst > 0x7FFFFFFF) break;

                                fprintf(f, "%d\t0x%X\t%d\t0x%X\t", si, node, ii, inst);
                                for (int j = 0; j < 32; j++) {
                                    uint32_t v = mem::Read<uint32_t>(inst + j * 4);
                                    fprintf(f, "%08X ", v);
                                }
                                fprintf(f, "\n");
                                totalInstances++;
                            }
                        }

                        // Push ALL possible children (scan +0x20 through +0x3C for valid ptrs)
                        // Also check +0x44 for explicit child count
                        int childCount = mem::Read<int>(node + 0x44);
                        if (childCount <= 0 || childCount > 16) {
                            // Fallback: scan for pointer-like values at +0x20
                            for (int ci = 0; ci < 8 && sp < 1024; ci++) {
                                uintptr_t child = mem::Read<uintptr_t>(node + 0x20 + ci * 4);
                                if (child > 0x10000 && child < 0x7FFFFFFF && child != node) {
                                    // Verify child looks like a BSP node (has AABB-like data)
                                    float cy = mem::Read<float>(child + 0x04);
                                    if (cy > -10000.0f && cy < 10000.0f)
                                        stack[sp++] = child;
                                }
                            }
                        } else {
                            for (int ci = 0; ci < childCount && sp < 1024; ci++) {
                                uintptr_t child = mem::Read<uintptr_t>(node + 0x20 + ci * 4);
                                if (child > 0x10000 && child < 0x7FFFFFFF)
                                    stack[sp++] = child;
                            }
                        }
                    }
                }

                fclose(f);
                log::Write("Dumped %d geometry instances to %s", totalInstances, path);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            log::Write("Geometry instance dump failed (exception)");
        }
    }

    // === DUMP COMPLETE SCENE — one file with everything ===
    static char dumpStatus[256] = "";
    static float dumpStatusTimer = 0.0f;
    if (ImGui::Button("DUMP SCENE")) {
        const char* levelName = GetCurrentLevelName();
        char path[256];
        snprintf(path, sizeof(path), "scene_%s.txt", levelName ? levelName : "unknown");
        FILE* sf = fopen(path, "w");
        if (sf) {
            // Section 1: Geometry instances (world transforms from hook)
            long giCnt = g_geomInstanceCount;
            if (giCnt > MAX_GEOM_INSTANCES) giCnt = MAX_GEOM_INSTANCES;
            fprintf(sf, "[geometry_instances] %ld\n", giCnt);
            fprintf(sf, "# pcrd_idx\tpos_x\tpos_y\tpos_z\tmat(16)\n");
            for (long i = 0; i < giCnt; i++) {
                auto& gi = g_geomInstances[i];
                int pcrdIdx = gi.flags & 0xFFFF;
                fprintf(sf, "%d\t%.6f\t%.6f\t%.6f", pcrdIdx,
                    gi.worldMatrix[12], gi.worldMatrix[13], gi.worldMatrix[14]);
                for (int j = 0; j < 16; j++)
                    fprintf(sf, "\t%.6f", gi.worldMatrix[j]);
                fprintf(sf, "\n");
            }

            // Section 2: VM placements (prop positions/rotations)
            long vmCnt = g_propPlacementCount;
            if (vmCnt > MAX_PROP_PLACEMENTS) vmCnt = MAX_PROP_PLACEMENTS;
            fprintf(sf, "[vm_placements] %ld\n", vmCnt);
            fprintf(sf, "# name\tobj_addr\tpos_x\tpos_y\tpos_z\trot_x\trot_y\trot_z\thas_pos\thas_rot\n");
            for (long i = 0; i < vmCnt; i++) {
                auto& p = g_propPlacements[i];
                fprintf(sf, "%s\t0x%08X\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%d\t%d\n",
                    p.name[0] ? p.name : "???",
                    p.objPtr, p.pos[0], p.pos[1], p.pos[2],
                    p.rot[0], p.rot[1], p.rot[2], p.hasPos, p.hasRot);
            }

            // Section 3: ALL geometry via world object arrays
            // The PCWB buffer IS the world object at runtime.
            // world+0x08 = draw data count, world+0x28 = streaming array
            // world+0x14 = geom instance count, world+0x34 = streaming array
            // Find PCWB base by scanning backward from a hooked instance
            // with proper validation (magic + version + reasonable file size).
            __try {
                uintptr_t ws = mem::Read<uintptr_t>(addr::pWorldState);
                int secCnt = mem::Read<int>(addr::pSectorCount);
                int totalGeom = 0;

                // Use PCWB base pre-computed during GeomInstance_Init hook
                uintptr_t pcwbBase = g_pcwbBase;

                if (pcwbBase) {
                    uint32_t fileSize = mem::Read<uint32_t>(pcwbBase + 0x0C);

                    // === Section 3a: ALL rendered PCRDs (scan PCWB for rebased vertex pointers) ===
                    // Rendered PCRDs have vo (vertex offset at +0x18) rebased to runtime pointer (> pcwbBase)
                    // Non-rendered have vo as file offset (< fileSize)
                    fprintf(sf, "[rendered_pcrds]\n");
                    fprintf(sf, "# vc\tic\tv0x\tv0y\tv0z\n");

                    int renderedCount = 0;
                    __try {
                        for (uintptr_t scan = 0; scan < fileSize - 0x20; scan += 4) {
                            uintptr_t addr = pcwbBase + scan;
                            uint32_t magic = mem::Read<uint32_t>(addr);
                            if (magic != 0x44524350) continue; // "PCRD"
                            uint32_t ver = mem::Read<uint32_t>(addr + 4);
                            if (ver != 2) continue;

                            uint32_t vc = mem::Read<uint32_t>(addr + 0x10);
                            uint32_t ic = mem::Read<uint32_t>(addr + 0x0C);
                            uintptr_t vo = mem::Read<uintptr_t>(addr + 0x18);

                            if (vc == 0 || vc > 1000000 || ic == 0 || ic > 1000000) continue;

                            // Check: vo is rebased (runtime ptr) = rendered
                            if (vo > pcwbBase) {
                                float v0x = mem::Read<float>(vo);
                                float v0y = mem::Read<float>(vo + 4);
                                float v0z = mem::Read<float>(vo + 8);
                                fprintf(sf, "%u\t%u\t%.6f\t%.6f\t%.6f\n", vc, ic, v0x, v0y, v0z);
                                renderedCount++;
                            }
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        log::Write("Exception during PCRD scan at count=%d", renderedCount);
                    }

                    // === Section 3b: Geometry instance transforms (non-identity, from hook) ===
                    fprintf(sf, "[instance_transforms] %d\n", (int)giCnt);
                    fprintf(sf, "# vc\tic\tv0x\tv0y\tv0z\tmat(16)\n");

                    int giDumped = 0;
                    for (long i = 0; i < giCnt; i++) {
                        auto& gi = g_geomInstances[i];
                        if (gi.addr < 0x10000) continue;

                        __try {
                            uintptr_t pcrdAddr = 0;
                            for (uintptr_t back = 0; back < 0x100000; back += 4) {
                                if (gi.addr - back <= pcwbBase) break;
                                if (mem::Read<uint32_t>(gi.addr - back) == 0x44524350 &&
                                    mem::Read<uint32_t>(gi.addr - back + 4) == 2) {
                                    pcrdAddr = gi.addr - back;
                                    break;
                                }
                            }

                            if (pcrdAddr) {
                                uint32_t vc = mem::Read<uint32_t>(pcrdAddr + 0x10);
                                uint32_t ic = mem::Read<uint32_t>(pcrdAddr + 0x0C);
                                uintptr_t vo = mem::Read<uintptr_t>(pcrdAddr + 0x18);
                                float v0x = 0, v0y = 0, v0z = 0;
                                if (vo > 0x10000) {
                                    v0x = mem::Read<float>(vo);
                                    v0y = mem::Read<float>(vo + 4);
                                    v0z = mem::Read<float>(vo + 8);
                                }

                                fprintf(sf, "%u\t%u\t%.6f\t%.6f\t%.6f",
                                    vc, ic, v0x, v0y, v0z);
                                for (int j = 0; j < 16; j++)
                                    fprintf(sf, "\t%.6f", gi.worldMatrix[j]);
                                fprintf(sf, "\n");
                                giDumped++;
                            }
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    }

                    fprintf(sf, "[sector_info]\n");
                    if (ws && secCnt > 0) {
                        uintptr_t secArr = mem::Read<uintptr_t>(ws + 0x64);
                        for (int si = 0; si < secCnt && si < 64; si++) {
                            uintptr_t sd = mem::Read<uintptr_t>(secArr + 4 * si);
                            if (!sd) continue;
                            totalGeom += mem::Read<int>(sd + 0xAC);
                        }
                    }
                    fprintf(sf, "total_geometry_instances=%d\n", totalGeom);
                    fprintf(sf, "rendered_pcrds=%d\n", renderedCount);
                    fprintf(sf, "instance_transforms=%d\n", giDumped);
                    fprintf(sf, "pcwb_base=0x%X\n", (unsigned)pcwbBase);
                    log::Write("SCENE DUMP: %d rendered PCRDs, %d transforms", renderedCount, giDumped);
                } else {
                    // Fallback: just sector info
                    fprintf(sf, "[sector_info]\n");
                    if (ws && secCnt > 0) {
                        uintptr_t secArr = mem::Read<uintptr_t>(ws + 0x64);
                        for (int si = 0; si < secCnt && si < 64; si++) {
                            uintptr_t sd = mem::Read<uintptr_t>(secArr + 4 * si);
                            if (!sd) continue;
                            totalGeom += mem::Read<int>(sd + 0xAC);
                        }
                    }
                    fprintf(sf, "total_geometry_instances=%d\n", totalGeom);
                    fprintf(sf, "pcwb_base=NOT_FOUND\n");
                    log::Write("SCENE DUMP: PCWB base not found!");
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                log::Write("SCENE DUMP section 3 exception");
            }

            fclose(sf);
            log::Write("SCENE DUMP: %ld geom instances + %ld VM placements -> %s", giCnt, vmCnt, path);
            snprintf(dumpStatus, sizeof(dumpStatus),
                "Saved: %s\n%ld geom instances, %ld VM placements\nPCWB: 0x%X",
                path, giCnt, vmCnt, (unsigned)g_pcwbBase);
            dumpStatusTimer = 10.0f;
        } else {
            snprintf(dumpStatus, sizeof(dumpStatus), "FAILED to open %s", path);
            dumpStatusTimer = 5.0f;
        }
    }
    ImGui::SameLine();
    if (g_pcwbBase)
        ImGui::Text("PCWB: 0x%X | Geom: %ld | VM: %ld",
            (unsigned)g_pcwbBase, g_geomInstanceCount, g_propPlacementCount);
    else
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "No PCWB base (reload level)");
    if (dumpStatusTimer > 0.0f) {
        dumpStatusTimer -= ImGui::GetIO().DeltaTime;
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1), "%s", dumpStatus);
    }

    // === DUMP PROP POSITIONS (JSON) ===
    ImGui::Separator();
    static char propJsonStatus[256] = "";
    static float propJsonTimer = 0.0f;
    static int propJsonMatched = 0, propJsonFallback = 0;

    if (ImGui::Button("DUMP PROP POSITIONS (JSON)")) {
        propJsonMatched = 0;
        propJsonFallback = 0;

        const char* levelName = GetCurrentLevelName();
        if (!levelName) levelName = "unknown";

        // Read PCWB prop table from runtime memory
        typedef unsigned int (__cdecl *CalcAddrFn)(unsigned int);
        auto calcAddr = reinterpret_cast<CalcAddrFn>(addr::fn_StreamingCalcAddr);

        __try {
            uintptr_t worldBase = static_cast<uintptr_t>(calcAddr(0));
            if (worldBase > 0x10000 && worldBase < 0x7FFFFFFF &&
                mem::Read<uint32_t>(worldBase) == 0x42574350) {

                uint32_t pageSize = mem::Read<uint32_t>(worldBase + 0x08);
                uintptr_t propBase = worldBase + pageSize;

                // Count props
                int nProps = 0;
                for (int pi = 0; pi < 128; pi++) {
                    uintptr_t entry = propBase + pi * 0xA0;
                    uint8_t c = mem::Read<uint8_t>(entry + 0x60);
                    if (c < 0x20 || c > 0x7E) break;
                    nProps++;
                }

                char path[256];
                snprintf(path, sizeof(path), "prop_positions_%s.json", levelName);
                FILE* f = fopen(path, "w");
                if (f) {
                    fprintf(f, "{\n  \"level\": \"%s\",\n  \"props\": [\n", levelName);

                    long vmCnt = g_propPlacementCount;
                    if (vmCnt > MAX_PROP_PLACEMENTS) vmCnt = MAX_PROP_PLACEMENTS;

                    for (int pi = 0; pi < nProps; pi++) {
                        uintptr_t entry = propBase + pi * 0xA0;

                        // Read PCWB prop name
                        char pcwbName[64] = {};
                        for (int ci = 0; ci < 63; ci++) {
                            uint8_t c = mem::Read<uint8_t>(entry + 0x60 + ci);
                            if (c == 0) break;
                            pcwbName[ci] = static_cast<char>(c);
                        }

                        // Read PCWB position (fallback)
                        float pcwbPos[3];
                        pcwbPos[0] = mem::Read<float>(entry + 48);  // mat row3 col0
                        pcwbPos[1] = mem::Read<float>(entry + 52);
                        pcwbPos[2] = mem::Read<float>(entry + 56);

                        // Strip Prop_/prop_/PROP_ prefix for matching
                        const char* stripped = pcwbName;
                        if (_strnicmp(stripped, "Prop_", 5) == 0) stripped += 5;
                        else if (_strnicmp(stripped, "PROP_", 5) == 0) stripped += 5;
                        else if (_strnicmp(stripped, "prop_", 5) == 0) stripped += 5;

                        // Search VM placements for a match (case-insensitive substring)
                        float pos[3] = {pcwbPos[0], pcwbPos[1], pcwbPos[2]};
                        float rot[3] = {0, 0, 0};
                        const char* source = "pcwb";
                        bool matched = false;

                        for (long vi = 0; vi < vmCnt; vi++) {
                            auto& vp = g_propPlacements[vi];
                            if (!vp.hasPos) continue;

                            // Try exact match on stripped name
                            if (_stricmp(vp.name, stripped) == 0) {
                                pos[0] = vp.pos[0]; pos[1] = vp.pos[1]; pos[2] = vp.pos[2];
                                if (vp.hasRot) { rot[0] = vp.rot[0]; rot[1] = vp.rot[1]; rot[2] = vp.rot[2]; }
                                source = "vm";
                                matched = true;
                                break;
                            }
                            // Try: VM name contains stripped PCWB name
                            if (strstr(vp.name, stripped) != nullptr) {
                                pos[0] = vp.pos[0]; pos[1] = vp.pos[1]; pos[2] = vp.pos[2];
                                if (vp.hasRot) { rot[0] = vp.rot[0]; rot[1] = vp.rot[1]; rot[2] = vp.rot[2]; }
                                source = "vm";
                                matched = true;
                                break;
                            }
                        }

                        if (matched) propJsonMatched++;
                        else propJsonFallback++;

                        if (pi > 0) fprintf(f, ",\n");
                        fprintf(f, "    {\"name\": \"%s\", \"pos\": [%.4f, %.4f, %.4f], \"rot\": [%.4f, %.4f, %.4f], \"source\": \"%s\"}",
                            pcwbName, pos[0], pos[1], pos[2], rot[0], rot[1], rot[2], source);
                    }

                    fprintf(f, "\n  ]\n}\n");
                    fclose(f);

                    snprintf(propJsonStatus, sizeof(propJsonStatus),
                        "Saved %d props (%d VM-matched, %d PCWB fallback) -> %s",
                        nProps, propJsonMatched, propJsonFallback, path);
                    propJsonTimer = 8.0f;
                    log::Write("%s", propJsonStatus);
                }
            } else {
                snprintf(propJsonStatus, sizeof(propJsonStatus), "PCWB not found in memory");
                propJsonTimer = 5.0f;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            snprintf(propJsonStatus, sizeof(propJsonStatus), "Exception during prop dump");
            propJsonTimer = 5.0f;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(saves prop_positions_<level>.json)");
    if (propJsonTimer > 0.0f) {
        propJsonTimer -= ImGui::GetIO().DeltaTime;
        if (propJsonMatched > 0 || propJsonFallback > 0) {
            ImGui::TextColored(ImVec4(0.3f,1.0f,0.3f,1), "VM matched: %d", propJsonMatched);
            ImGui::SameLine();
            if (propJsonFallback > 0)
                ImGui::TextColored(ImVec4(1.0f,0.6f,0.3f,1), "PCWB fallback: %d", propJsonFallback);
        }
        ImGui::TextColored(ImVec4(0.3f,1.0f,0.3f,1), "%s", propJsonStatus);
    }

    // === IN-ENGINE OBJ WORLD EXPORTER ===
    ImGui::Separator();
    static char exportStatus[256] = "";
    static float exportTimer = 0.0f;

    if (ImGui::Button("EXPORT WORLD OBJ")) {
        if (g_pcwbBase) {
            CreateDirectoryA("world_export", NULL);
            CreateDirectoryA("world_export/textures", NULL);

            FILE* obj = fopen("world_export/world.obj", "w");
            if (obj) {
                fprintf(obj, "# Spiderwick Engine-Native World Export\n");
                fprintf(obj, "# Reads PCWB geometry directly from engine memory\n\n");

                uint32_t fileSize = mem::Read<uint32_t>(g_pcwbBase + 0x0C);
                int vertBase = 0;
                int meshCount = 0;
                int totalVerts = 0;
                int totalTris = 0;

                // Build set of hooked instance PCRD addresses → world matrix
                // For each hooked instance, scan backward to find its PCRD
                struct PcrdTransform { uintptr_t pcrdAddr; float mat[16]; };
                PcrdTransform instTransforms[MAX_GEOM_INSTANCES];
                int instCount = 0;
                long giCntLocal = g_geomInstanceCount;
                if (giCntLocal > MAX_GEOM_INSTANCES) giCntLocal = MAX_GEOM_INSTANCES;

                for (long gi = 0; gi < giCntLocal; gi++) {
                    auto& inst = g_geomInstances[gi];
                    if (inst.addr < 0x10000) continue;
                    __try {
                        for (uintptr_t back = 0; back < 0x100000; back += 4) {
                            if (inst.addr - back <= g_pcwbBase) break;
                            if (mem::Read<uint32_t>(inst.addr - back) == 0x44524350 &&
                                mem::Read<uint32_t>(inst.addr - back + 4) == 2) {
                                instTransforms[instCount].pcrdAddr = inst.addr - back;
                                for (int j = 0; j < 16; j++)
                                    instTransforms[instCount].mat[j] = inst.worldMatrix[j];
                                instCount++;
                                break;
                            }
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }

                // Scan PCWB for ALL rendered PCRDs (rebased vo = rendered)
                __try {
                for (uintptr_t scan = 0; scan < fileSize - 0x20; scan += 4) {
                    uintptr_t addr = g_pcwbBase + scan;
                    if (mem::Read<uint32_t>(addr) != 0x44524350) continue;
                    if (mem::Read<uint32_t>(addr + 4) != 2) continue;

                    uint32_t ic = mem::Read<uint32_t>(addr + 0x0C);
                    uint32_t vc = mem::Read<uint32_t>(addr + 0x10);
                    uintptr_t io = mem::Read<uintptr_t>(addr + 0x14);
                    uintptr_t vo = mem::Read<uintptr_t>(addr + 0x18);
                    uint32_t hs = mem::Read<uint32_t>(addr + 0x08);

                    if (vc == 0 || ic == 0 || vc > 1000000 || ic > 1000000) continue;
                    if (vo <= g_pcwbBase) continue; // not rebased = not rendered

                    uint32_t stride = (hs <= 0x10) ? 32 : 24;

                    // Check if this PCRD has a geometry instance transform
                    float* worldMat = nullptr;
                    for (int ti = 0; ti < instCount; ti++) {
                        if (instTransforms[ti].pcrdAddr == addr) {
                            worldMat = instTransforms[ti].mat;
                            break;
                        }
                    }

                    fprintf(obj, "o mesh_%04d\n", meshCount);

                    // Write vertices
                    for (uint32_t i = 0; i < vc; i++) {
                        uintptr_t vaddr = vo + i * stride;
                        float x = mem::Read<float>(vaddr);
                        float y = mem::Read<float>(vaddr + 4);
                        float z = mem::Read<float>(vaddr + 8);

                        // Apply world transform if this PCRD has a geometry instance
                        if (worldMat) {
                            float wx = x*worldMat[0] + y*worldMat[4] + z*worldMat[8]  + worldMat[12];
                            float wy = x*worldMat[1] + y*worldMat[5] + z*worldMat[9]  + worldMat[13];
                            float wz = x*worldMat[2] + y*worldMat[6] + z*worldMat[10] + worldMat[14];
                            x = wx; y = wy; z = wz;
                        }

                        // Vertex color at +12
                        uint8_t r = mem::Read<uint8_t>(vaddr + 12);
                        uint8_t g = mem::Read<uint8_t>(vaddr + 13);
                        uint8_t b = mem::Read<uint8_t>(vaddr + 14);

                        // Swap Y/Z for OBJ (engine Z-up → OBJ Y-up)
                        fprintf(obj, "v %.6f %.6f %.6f %.4f %.4f %.4f\n",
                            x, z, -y, r/255.0f, g/255.0f, b/255.0f);

                        // UVs at +16
                        float u = mem::Read<float>(vaddr + 16);
                        float vt = mem::Read<float>(vaddr + 20);
                        fprintf(obj, "vt %.6f %.6f\n", u, 1.0f - vt);
                    }

                    // Write faces (triangle strip)
                    for (uint32_t i = 0; i + 2 < ic; i++) {
                        uint16_t i0 = mem::Read<uint16_t>(io + i * 2);
                        uint16_t i1 = mem::Read<uint16_t>(io + (i+1) * 2);
                        uint16_t i2 = mem::Read<uint16_t>(io + (i+2) * 2);
                        if (i0 == i1 || i1 == i2 || i0 == i2) continue;
                        int vb = vertBase + 1;
                        if (i % 2 == 0)
                            fprintf(obj, "f %d/%d %d/%d %d/%d\n",
                                vb+i0, vb+i0, vb+i1, vb+i1, vb+i2, vb+i2);
                        else
                            fprintf(obj, "f %d/%d %d/%d %d/%d\n",
                                vb+i1, vb+i1, vb+i0, vb+i0, vb+i2, vb+i2);
                        totalTris++;
                    }

                    vertBase += vc;
                    totalVerts += vc;
                    meshCount++;
                }
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    log::Write("EXPORT: exception at mesh %d", meshCount);
                }

                fclose(obj);
                snprintf(exportStatus, sizeof(exportStatus),
                    "Exported: %d meshes, %d verts, %d tris, %d transforms\n-> world_export/world.obj",
                    meshCount, totalVerts, totalTris, instCount);
                exportTimer = 15.0f;
                log::Write("EXPORT WORLD: %d meshes, %d verts, %d tris -> world_export/world.obj",
                    meshCount, totalVerts, totalTris);
            }
        } else {
            snprintf(exportStatus, sizeof(exportStatus), "No PCWB base! Reload level first.");
            exportTimer = 5.0f;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Engine-native (reads PCWB memory)");
    if (exportTimer > 0.0f) {
        exportTimer -= ImGui::GetIO().DeltaTime;
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1), "%s", exportStatus);
    }

    // Capture world matrices from D3D9 draw calls
    if (ImGui::Button("Capture World Matrices")) {
        d3dhook::g_capturedCount = 0;
        d3dhook::g_captureFrames = 5;  // capture across 5 frames for full coverage
    }
    if (d3dhook::g_capturedCount > 0) {
        ImGui::SameLine();
        ImGui::Text("%ld matrices", d3dhook::g_capturedCount);

        if (ImGui::Button("Save Matrices to File")) {
            const char* levelName = GetCurrentLevelName();
            char path[256];
            snprintf(path, sizeof(path), "world_matrices_%s.txt", levelName ? levelName : "unknown");
            FILE* f = fopen(path, "w");
            if (f) {
                long count = d3dhook::g_capturedCount;
                if (count > d3dhook::MAX_CAPTURED_MATRICES) count = d3dhook::MAX_CAPTURED_MATRICES;
                fprintf(f, "# WVP (c0-c3) all draw calls: %ld entries\n", count);
                fprintf(f, "# verts\tprims\tc0x\tc0y\tc0z\tc0w\tc1x\tc1y\tc1z\tc1w\tc2x\tc2y\tc2z\tc2w\tc3x\tc3y\tc3z\tc3w\n");
                for (long i = 0; i < count; i++) {
                    auto& c = d3dhook::g_capturedMatrices[i];
                    fprintf(f, "%u\t%u\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\n",
                        c.numVerts, c.primCount,
                        c.mat._11, c.mat._12, c.mat._13, c.mat._14,
                        c.mat._21, c.mat._22, c.mat._23, c.mat._24,
                        c.mat._31, c.mat._32, c.mat._33, c.mat._34,
                        c.mat._41, c.mat._42, c.mat._43, c.mat._44);
                }
                fclose(f);
                log::Write("Saved %ld matrices to %s", count, path);
            }
        }
    }

    // Camera object raw data
    if (camObj && ImGui::TreeNode("Camera Object Raw")) {
        ImGui::Text("camera_obj @ 0x%X", camObj);
        ImGui::Text("+0x6B8 pos: (%.2f, %.2f, %.2f)",
            mem::Read<float>(camObj + 0x6B8),
            mem::Read<float>(camObj + 0x6BC),
            mem::Read<float>(camObj + 0x6C0));
        ImGui::Text("+0x788 sector: %d", mem::Read<int>(camObj + 0x788));
        ImGui::Text("+0x78C flags: 0x%X", mem::Read<uint32_t>(camObj + 0x78C));

        if (ImGui::TreeNode("View Matrix (+0x790)")) {
            for (int r = 0; r < 4; r++)
                ImGui::Text("%.3f  %.3f  %.3f  %.3f",
                    mem::Read<float>(camObj + 0x790 + r * 16 + 0),
                    mem::Read<float>(camObj + 0x790 + r * 16 + 4),
                    mem::Read<float>(camObj + 0x790 + r * 16 + 8),
                    mem::Read<float>(camObj + 0x790 + r * 16 + 12));
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Commit Matrix (+0x834)")) {
            for (int r = 0; r < 4; r++)
                ImGui::Text("%.3f  %.3f  %.3f  %.3f",
                    mem::Read<float>(camObj + 0x834 + r * 16 + 0),
                    mem::Read<float>(camObj + 0x834 + r * 16 + 4),
                    mem::Read<float>(camObj + 0x834 + r * 16 + 8),
                    mem::Read<float>(camObj + 0x834 + r * 16 + 12));
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }

    // Player info
    {
        typedef uintptr_t (__cdecl *GetPlayerFn)();
        auto getPlayer = reinterpret_cast<GetPlayerFn>(addr::fn_GetPlayerCharacter);
        uintptr_t player = getPlayer();
        if (player && ImGui::TreeNode("Player Object")) {
            ImGui::Text("player @ 0x%X", player);
            ImGui::Text("Pos: (%.2f, %.2f, %.2f)",
                mem::Read<float>(player + addr::PLAYER_POS_X),
                mem::Read<float>(player + addr::PLAYER_POS_Y),
                mem::Read<float>(player + addr::PLAYER_POS_Z));
            ImGui::TreePop();
        }
    }

    // VM internals
    if (ImGui::TreeNode("VM Runtime")) {
        static bool vmLogged = false;
        if (!vmLogged) {
            vmLogged = true;
            log::Write("=== VM Runtime dump ===");
            log::Write("  pVMStackBase  @ 0x%X = 0x%X", addr::pVMStackBase, mem::Read<uint32_t>(addr::pVMStackBase));
            log::Write("  pVMStackIndex @ 0x%X = %d", addr::pVMStackIndex, mem::Read<int>(addr::pVMStackIndex));
            log::Write("  pVMReturnStack@ 0x%X = 0x%X", addr::pVMReturnStack, mem::Read<uint32_t>(addr::pVMReturnStack));
            log::Write("  pVMObjBase    @ 0x%X = 0x%X", addr::pVMObjBase, mem::Read<uint32_t>(addr::pVMObjBase));
        }

        ImGui::Text("VMStackBase:  0x%X", mem::Read<uint32_t>(addr::pVMStackBase));
        ImGui::Text("VMStackIndex: %d", mem::Read<int>(addr::pVMStackIndex));
        ImGui::Text("VMRetStack:   0x%X", mem::Read<uint32_t>(addr::pVMReturnStack));
        ImGui::Text("VMObjBase:    0x%X", mem::Read<uint32_t>(addr::pVMObjBase));
        ImGui::Text("CharPool:     0x%X", mem::Read<uint32_t>(addr::pCharacterPool));

        uintptr_t scriptStatePtr = mem::Read<uintptr_t>(addr::pScriptState);
        ImGui::Text("ScriptState:  0x%X", scriptStatePtr);

        if (scriptStatePtr && scriptStatePtr > 0x10000 && scriptStatePtr < 0x7FFFFFFF) {
            uint8_t debugByte = mem::Read<uint8_t>(scriptStatePtr + addr::SCRIPT_DEBUG_OFFSET);
            bool debugEnabled = (debugByte & 1) != 0;
            ImGui::Text("  +0x2E byte: 0x%02X (debugger bit=%d)", debugByte, debugEnabled);

            if (ImGui::Button(debugEnabled ? "Disable Script Debugger" : "Enable Script Debugger")) {
                uint8_t newByte = debugEnabled ? (debugByte & ~1) : (debugByte | 1);
                mem::Write<uint8_t>(scriptStatePtr + addr::SCRIPT_DEBUG_OFFSET, newByte);
                log::Write("Script debugger %s", !debugEnabled ? "ENABLED" : "DISABLED");
            }
        } else {
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "  ScriptState ptr invalid!");
        }

        ImGui::TreePop();
    }
}
