// ============================================================================
// panels.cpp — All ImGui UI panels for SpiderView
// ============================================================================

#include "panels.h"
#include "hex_viewer.h"
#include "imgui.h"
#include "core/format_registry.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <unordered_map>

// ============================================================================
// Helpers
// ============================================================================

static ImVec4 TypeColor(AssetType t) {
    switch (t) {
        case AssetType::SCT:  return {0.3f,1.0f,0.3f,1};
        case AssetType::NM40: return {0.4f,0.7f,1.0f,1};
        case AssetType::PCW:
        case AssetType::PCWB: return {1.0f,0.85f,0.3f,1};
        case AssetType::PCIM: return {0.75f,0.55f,1.0f,1};
        case AssetType::PCRD: return {0.65f,0.45f,0.9f,1};
        case AssetType::DBDB: return {1.0f,0.5f,0.5f,1};
        case AssetType::STTL: return {0.6f,0.85f,0.6f,1};
        case AssetType::ZWD:  return {0.5f,0.8f,1.0f,1};
        case AssetType::BIK:  return {0.9f,0.5f,0.5f,1};
        case AssetType::SEG:
        case AssetType::BNK:  return {0.9f,0.6f,0.9f,1};
        default:              return {0.5f,0.5f,0.5f,1};
    }
}

static ImVec4 DimColor(ImVec4 c) { return {c.x*0.5f, c.y*0.5f, c.z*0.5f, 1}; }

// Tab auto-switch: -1=none, 0=Props, 1=Script, 2=VM, 3=Data, 4=Hex
static int s_switchToTab = -1;

// ============================================================================
// Unified asset actions (used by archive tree, search, context menus)
// ============================================================================

static void OpenAsset(App& app, const std::string& zwdPath, FSNode& archiveNode, int childIdx) {
    auto& a = archiveNode.children[childIdx];
    switch (a.type) {
        case AssetType::SCT:
            app.LoadSCTFromArchive(zwdPath, a.offset, a.size);
            app.BuildScriptView();
            s_switchToTab = 1;
            break;
        case AssetType::PCIM:
            // If an NM40 model is loaded, apply texture to it; otherwise show preview
            if (!app.scene.objects.empty() && app.renderSettings.normalLighting)
                app.ApplyTextureToModel(zwdPath, a.offset, a.size);
            else
                app.PreviewTexture(zwdPath, a.offset, archiveNode, childIdx);
            break;
        case AssetType::NM40:
            // 3D preview in viewport + data view in Data tab
            app.PreviewModel(zwdPath, a.offset, a.size, archiveNode);
            app.ViewAsset(zwdPath, a.offset, a.size, a.type);
            s_switchToTab = 3;
            break;
        default: {
            const FormatHandler* h = FormatRegistry::Instance().Get(a.type);
            if (h && h->view) {
                app.ViewAsset(zwdPath, a.offset, a.size, a.type);
                s_switchToTab = 3;
            } else {
                app.HexViewAsset(zwdPath, a.offset, a.size, a.type);
                s_switchToTab = 4;
            }
            break;
        }
    }
}

static void AssetContextMenu(App& app, const std::string& zwdPath, FSNode& archiveNode, int childIdx) {
    auto& a = archiveNode.children[childIdx];
    if (ImGui::BeginPopupContextItem("##ctx")) {
        if (ImGui::MenuItem("Export"))
            app.ExportAsset(zwdPath, a.offset, a.size, a.type,
                FormatRegistry::Instance().GetName(a.type));
        if (ImGui::MenuItem("Hex View")) {
            app.HexViewAsset(zwdPath, a.offset, a.size, a.type);
            s_switchToTab = 4;
        }
        if (a.type == AssetType::SCT) {
            if (ImGui::MenuItem("Run Script")) {
                app.LoadSCTFromArchive(zwdPath, a.offset, a.size);
                if (app.vm.loaded) app.RunAllInits();
            }
        }
        ImGui::EndPopup();
    }
}

// ============================================================================
// Archive contents — grouped by type, double-click to open
// ============================================================================

static void DrawArchiveContents(App& app, FSNode& archiveNode) {
    std::unordered_map<int, std::vector<int>> groups;
    for (int j = 0; j < (int)archiveNode.children.size(); j++)
        groups[(int)archiveNode.children[j].type].push_back(j);

    ImGui::TextDisabled("%s", archiveNode.extra.c_str());

    // Top types: individual selectable rows
    AssetType topTypes[] = {AssetType::SCT, AssetType::PCWB, AssetType::DBDB, AssetType::STTL};
    for (auto tt : topTypes) {
        auto git = groups.find((int)tt);
        if (git == groups.end()) continue;
        for (int j : git->second) {
            auto& a = archiveNode.children[j];
            ImGui::PushID(j);
            char sz[16] = "";
            if (a.size > 1024) snprintf(sz, sizeof(sz), "%dKB", a.size/1024);
            else if (a.size > 0) snprintf(sz, sizeof(sz), "%uB", a.size);
            char label[128];
            snprintf(label, sizeof(label), "%-4s  %-6s  %s",
                FormatRegistry::Instance().GetName(tt), sz, a.extra.c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, TypeColor(tt));
            if (ImGui::Selectable(label, false, ImGuiSelectableFlags_AllowDoubleClick))
                if (ImGui::IsMouseDoubleClicked(0))
                    OpenAsset(app, archiveNode.fullPath, archiveNode, j);
            ImGui::PopStyleColor();
            AssetContextMenu(app, archiveNode.fullPath, archiveNode, j);
            ImGui::PopID();
        }
    }

    // NM40 — collapsible
    if (groups.count((int)AssetType::NM40)) {
        auto& idx = groups[(int)AssetType::NM40];
        ImGui::PushStyleColor(ImGuiCol_Text, TypeColor(AssetType::NM40));
        char hdr[64]; snprintf(hdr, sizeof(hdr), "NM40 — %d models###nm40", (int)idx.size());
        bool open = ImGui::TreeNode(hdr);
        ImGui::PopStyleColor();
        if (open) {
            for (int j : idx) {
                auto& a = archiveNode.children[j];
                ImGui::PushID(j + 0x60000);
                char sz[16] = "";
                if (a.size > 1024) snprintf(sz, sizeof(sz), "%dKB", a.size/1024);
                char label[128];
                snprintf(label, sizeof(label), "%-6s %s", sz, a.extra.c_str());
                if (ImGui::Selectable(label, false, ImGuiSelectableFlags_AllowDoubleClick))
                    if (ImGui::IsMouseDoubleClicked(0))
                        OpenAsset(app, archiveNode.fullPath, archiveNode, j);
                AssetContextMenu(app, archiveNode.fullPath, archiveNode, j);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    // PCIM — collapsible with inline preview
    if (groups.count((int)AssetType::PCIM)) {
        auto& idx = groups[(int)AssetType::PCIM];
        ImGui::PushStyleColor(ImGuiCol_Text, TypeColor(AssetType::PCIM));
        char hdr[64]; snprintf(hdr, sizeof(hdr), "PCIM — %d textures###pcim", (int)idx.size());
        bool open = ImGui::TreeNode(hdr);
        ImGui::PopStyleColor();
        if (open) {
            // Export All DDS button
            if (ImGui::SmallButton("Export All DDS")) {
                const auto* blob = app.cache.GetBlob(archiveNode.fullPath);
                if (blob) {
                    // Create output directory: game_dir/export/WadName/
                    std::string wadBase = archiveNode.name;
                    size_t dot = wadBase.rfind('.');
                    if (dot != std::string::npos) wadBase = wadBase.substr(0, dot);
                    std::string outDir = app.fs.GetGameDir() + "/export/" + wadBase;
                    MakeDirectory(outDir.c_str());

                    int exported = 0;
                    for (int j : idx) {
                        auto& a = archiveNode.children[j];
                        if (a.offset + 0xC1 + 128 >= (uint32_t)blob->size()) continue;
                        const uint8_t* pcim = blob->data() + a.offset;

                        // Find DDS within PCIM
                        uint32_t ddsOff = 0;
                        if (a.size > 0x14) {
                            uint32_t rel = pcim[0x10]|(pcim[0x11]<<8)|(pcim[0x12]<<16)|(pcim[0x13]<<24);
                            if (rel > 0 && rel < a.size && a.offset + rel + 128 < (uint32_t)blob->size() &&
                                memcmp(blob->data() + a.offset + rel, "DDS ", 4) == 0)
                                ddsOff = rel;
                        }
                        if (!ddsOff && a.offset + 0xC1 + 128 < (uint32_t)blob->size() &&
                            memcmp(pcim + 0xC1, "DDS ", 4) == 0)
                            ddsOff = 0xC1;
                        if (!ddsOff) continue;

                        uint32_t ddsSize = a.size - ddsOff;
                        // Read dimensions for filename
                        int w = 0, h = 0;
                        if (a.offset + 0xA4 <= (uint32_t)blob->size()) {
                            w = pcim[0x9C]|(pcim[0x9D]<<8)|(pcim[0x9E]<<16)|(pcim[0x9F]<<24);
                            h = pcim[0xA0]|(pcim[0xA1]<<8)|(pcim[0xA2]<<16)|(pcim[0xA3]<<24);
                        }

                        char fname[128];
                        snprintf(fname, sizeof(fname), "%08X_%dx%d.dds", a.nameHash, w, h);
                        std::string path = outDir + "/" + fname;
                        FILE* f = fopen(path.c_str(), "wb");
                        if (f) {
                            fwrite(pcim + ddsOff, 1, ddsSize, f);
                            fclose(f);
                            exported++;
                        }
                    }
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Exported %d DDS to export/%s/", exported, wadBase.c_str());
                    app.view.statusMsg = msg;
                    printf("[Export] %s\n", msg);
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%d)", (int)idx.size());

            for (int j : idx) {
                auto& a = archiveNode.children[j];
                ImGui::PushID(j);
                bool sel = (app.view.previewTex.id > 0 &&
                    app.view.previewName == std::to_string(a.offset));
                char label[64]; snprintf(label, sizeof(label), "%s##t%d",
                    a.extra.empty() ? "?" : a.extra.c_str(), j);
                if (ImGui::Selectable(label, sel)) {
                    if (sel) app.view.ClearPreview();
                    else app.PreviewTexture(archiveNode.fullPath, a.offset, archiveNode, j);
                }
                if (sel && app.view.previewTex.id > 0) {
                    float th = 64.0f;
                    float sc = th / fmaxf(1.0f, (float)app.view.previewH);
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16);
                    ImGui::Image((ImTextureID)(intptr_t)app.view.previewTex.id,
                        ImVec2(app.view.previewW * sc, app.view.previewH * sc));
                }
                AssetContextMenu(app, archiveNode.fullPath, archiveNode, j);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    // PCRD — count only
    if (groups.count((int)AssetType::PCRD))
        ImGui::TextColored(ImVec4(0.45f,0.45f,0.45f,1), "PCRD — %d chunks", (int)groups[(int)AssetType::PCRD].size());
    if (groups.count((int)AssetType::AWAD))
        ImGui::TextColored(ImVec4(0.45f,0.45f,0.45f,1), "AWAD — container");
}

// ============================================================================
// Search
// ============================================================================

struct SearchResult {
    std::string archiveName, archivePath;
    int childIdx;
    AssetType type;
    uint32_t offset, size;
    std::string extra;
    FSNode* archiveNode = nullptr; // pointer to parent ZWD node
};

static void CollectSearchResults(FSNode& root, AssetType filterType, const char* filterText,
                                  std::vector<SearchResult>& results) {
    for (auto& child : root.children) {
        if (child.isDir) {
            CollectSearchResults(child, filterType, filterText, results);
        } else if (child.type == AssetType::ZWD && child.expanded) {
            for (int j = 0; j < (int)child.children.size(); j++) {
                auto& a = child.children[j];
                if (filterType != AssetType::Unknown && a.type != filterType) continue;
                if (filterText[0]) {
                    const char* tn = FormatRegistry::Instance().GetName(a.type);
                    if (!strstr(tn, filterText) && !strstr(a.extra.c_str(), filterText)) continue;
                }
                results.push_back({child.name, child.fullPath, j, a.type, a.offset, a.size, a.extra, &child});
            }
        }
    }
}

// ============================================================================
// Recursive file tree
// ============================================================================

static void DrawFSTree(App& app, FSNode& node) {
    for (auto& child : node.children) {
        ImGui::PushID(&child);

        if (child.isDir) {
            ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_SpanAvailWidth;
            if (child.children.empty()) fl |= ImGuiTreeNodeFlags_Leaf;
            bool open = ImGui::TreeNodeEx(child.name.c_str(), fl, "%s/ (%d)",
                child.name.c_str(), (int)child.children.size());
            if (open) { DrawFSTree(app, child); ImGui::TreePop(); }
        }
        else if (child.type == AssetType::ZWD) {
            ImGui::PushStyleColor(ImGuiCol_Text, TypeColor(AssetType::ZWD));
            bool open = ImGui::TreeNodeEx(child.name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
            ImGui::PopStyleColor();
            if (open) {
                if (!child.expanded) app.fs.ExpandArchive(child, app.cache);
                DrawArchiveContents(app, child);
                ImGui::TreePop();
            }
        }
        else {
            bool actionable = FormatRegistry::Instance().IsActionable(child.type);
            ImVec4 col = TypeColor(child.type);
            if (!actionable) col = DimColor(col);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_NoTreePushOnOpen|ImGuiTreeNodeFlags_SpanAvailWidth;
            ImGui::TreeNodeEx(child.name.c_str(), fl);
            ImGui::PopStyleColor();
            if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0)) {
                if (child.type == AssetType::PCW)
                    app.LoadWorld(child.fullPath.c_str());
            }
            if (ImGui::IsItemHovered()) {
                if (child.type == AssetType::PCW) ImGui::SetTooltip("Double-click to load world");
                else if (child.type == AssetType::BIK) ImGui::SetTooltip("Bink video (not yet supported)");
                else if (child.type == AssetType::SEG || child.type == AssetType::BNK)
                    ImGui::SetTooltip("Audio (not yet supported)");
            }
        }
        ImGui::PopID();
    }
}

// ============================================================================
// LEFT PANEL
// ============================================================================

void ui::DrawLeftPanel(App& app) {
    int sh = GetScreenHeight();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(360, (float)sh), ImGuiCond_Always);
    ImGui::Begin("##left", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);

    if (!app.view.statusMsg.empty()) {
        ImGui::TextColored(ImVec4(0.4f,1,0.4f,1), "%s", app.view.statusMsg.c_str());
        ImGui::Separator();
    }

    if (app.cache.GetCachedCount() > 0) {
        ImGui::TextDisabled("Cache: %d archives, %.0f MB",
            app.cache.GetCachedCount(), app.cache.GetUsedBytes()/(1024.f*1024.f));
    }

    if (ImGui::BeginTabBar("LeftTabs")) {
        if (ImGui::BeginTabItem("Game Files")) {
            static char searchBuf[128] = {0};
            static int searchType = 0;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100);
            ImGui::InputTextWithHint("##search", "Search assets...", searchBuf, sizeof(searchBuf));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90);
            const char* typeLabels[] = {"All","SCT","NM40","PCIM","DBDB","STTL","PCWB","PCRD"};
            ImGui::Combo("##stype", &searchType, typeLabels, 8);

            if (searchBuf[0] || searchType > 0) {
                AssetType filterTypes[] = {AssetType::Unknown, AssetType::SCT, AssetType::NM40,
                    AssetType::PCIM, AssetType::DBDB, AssetType::STTL, AssetType::PCWB, AssetType::PCRD};
                std::vector<SearchResult> results;
                CollectSearchResults(app.fs.GetRoot(), filterTypes[searchType], searchBuf, results);

                ImGui::TextDisabled("%d results", (int)results.size());
                ImGui::BeginChild("##searchresults");
                for (int i = 0; i < (int)results.size() && i < 500; i++) {
                    auto& sr = results[i];
                    ImGui::PushID(i + 0xA0000);
                    char label[128];
                    snprintf(label, sizeof(label), "%-4s  %s  %s",
                        FormatRegistry::Instance().GetName(sr.type),
                        sr.extra.c_str(), sr.archiveName.c_str());
                    ImGui::PushStyleColor(ImGuiCol_Text, TypeColor(sr.type));
                    if (ImGui::Selectable(label, false, ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            const FormatHandler* h = FormatRegistry::Instance().Get(sr.type);
                            if (sr.type == AssetType::SCT) {
                                app.LoadSCTFromArchive(sr.archivePath, sr.offset, sr.size);
                                app.BuildScriptView();
                                s_switchToTab = 1;
                            } else if (sr.type == AssetType::NM40 && sr.archiveNode) {
                                app.PreviewModel(sr.archivePath, sr.offset, sr.size, *sr.archiveNode);
                                app.ViewAsset(sr.archivePath, sr.offset, sr.size, sr.type);
                                s_switchToTab = 3;
                            } else if (h && h->view) {
                                app.ViewAsset(sr.archivePath, sr.offset, sr.size, sr.type);
                                s_switchToTab = 3;
                            } else {
                                app.HexViewAsset(sr.archivePath, sr.offset, sr.size, sr.type);
                                s_switchToTab = 4;
                            }
                        }
                    }
                    ImGui::PopStyleColor();
                    if (ImGui::BeginPopupContextItem("##srctx")) {
                        if (ImGui::MenuItem("Export"))
                            app.ExportAsset(sr.archivePath, sr.offset, sr.size, sr.type,
                                FormatRegistry::Instance().GetName(sr.type));
                        if (ImGui::MenuItem("Hex View")) {
                            app.HexViewAsset(sr.archivePath, sr.offset, sr.size, sr.type);
                            s_switchToTab = 4;
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
            } else {
                ImGui::BeginChild("##filetree");
                DrawFSTree(app, app.fs.GetRoot());
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Scene")) {
            ImGui::Text("%d objs  %d tex  %dK tris",
                app.scene.GetObjectCount(), app.scene.GetTextureCount(),
                app.scene.GetTotalTriangles()/1000);
            static char filter[128] = {0};
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##flt", "Filter...", filter, sizeof(filter));
            ImGui::BeginChild("##objtree");
            for (int i = 0; i < (int)app.scene.objects.size(); i++) {
                auto& obj = app.scene.objects[i];
                if (filter[0] && !strstr(obj.name.c_str(), filter)) continue;
                ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (i == app.view.selectedObject) fl |= ImGuiTreeNodeFlags_Selected;
                if (!obj.visible) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1));
                ImGui::TreeNodeEx((void*)(intptr_t)i, fl, "%s", obj.name.c_str());
                if (!obj.visible) ImGui::PopStyleColor();
                if (ImGui::IsItemClicked()) app.view.selectedObject = i;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Render")) {
            auto& s = app.renderSettings;
            ImGui::Checkbox("Grid", &s.showGrid);
            ImGui::SameLine(); ImGui::Checkbox("Wire", &s.showWireframe);
            ImGui::SameLine(); ImGui::Checkbox("BBox", &s.showBBoxes);
            ImGui::Separator();

            if (s.normalLighting) {
                ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1), "NM40 Model");
                ImGui::Checkbox("Textures", &s.layerTextures);
                ImGui::SameLine();
                ImGui::Checkbox("Armature", &s.showArmature);
                if (s.showArmature && !app.scene.bones.empty())
                    ImGui::TextDisabled("  %d bones", (int)app.scene.bones.size());
                ImGui::Separator();
                ImGui::Checkbox("Dir Light", &s.dirLightEnable);
                if (s.dirLightEnable) {
                    ImGui::SliderFloat("Yaw##dl", &s.dirLightYaw, -3.14f, 3.14f);
                    ImGui::SliderFloat("Pitch##dl", &s.dirLightPitch, -1.5f, 1.5f);
                    ImGui::SliderFloat("Intensity##dl", &s.dirLightIntensity, 0.0f, 2.0f);
                }
                ImGui::SliderFloat("Ambient", &s.ambientLevel, 0.0f, 1.0f);
            } else {
                ImGui::Checkbox("Textures", &s.layerTextures);
                ImGui::SameLine(); ImGui::Checkbox("VertColor", &s.layerVertColor);
                ImGui::Checkbox("Lighting", &s.layerLighting);
                if (s.layerLighting) {
                    ImGui::SliderFloat("Boost", &s.lightBoost, 0.5f, 4.0f);
                    ImGui::SliderFloat("Ambient", &s.ambientLevel, 0.0f, 1.0f);
                }
                ImGui::Separator();
                ImGui::Checkbox("Dir Light", &s.dirLightEnable);
                if (s.dirLightEnable) {
                    ImGui::SliderFloat("Yaw##dl", &s.dirLightYaw, -3.14f, 3.14f);
                    ImGui::SliderFloat("Pitch##dl", &s.dirLightPitch, -1.5f, 1.5f);
                    ImGui::SliderFloat("Intensity##dl", &s.dirLightIntensity, 0.0f, 2.0f);
                }
                ImGui::Separator();
                ImGui::Checkbox("Fog", &s.fogEnable);
                if (s.fogEnable) {
                    ImGui::ColorEdit3("Color##fog", s.fogColor);
                    ImGui::SliderFloat("Start##fog", &s.fogStart, 0.f, 2000.f);
                    ImGui::SliderFloat("End##fog", &s.fogEnd, 0.f, 2000.f);
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

// ============================================================================
// RIGHT PANEL
// ============================================================================

void ui::DrawRightPanel(App& app) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float pw = 340;
    ImGui::SetNextWindowPos(ImVec2(sw - pw, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(pw, (float)sh), ImGuiCond_Always);
    ImGui::Begin("##right", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);

    // Export section — always visible at top
    {
        bool hasModel = !app.view.materialSlots.empty();
        bool hasWorld = app.scene.GetObjectCount() > 0 && !app.scene.pcwb.data.empty();
        if (hasModel || hasWorld) {
            if (hasModel && ImGui::Button("Export FBX")) app.ExportCurrentModelFBX();
            if (hasModel && hasWorld) ImGui::SameLine();
            if (hasWorld && ImGui::Button("Export OBJ")) app.ExportCurrentWorldOBJ();
            ImGui::Separator();
        }
    }

    if (ImGui::BeginTabBar("RightTabs")) {
        ImGuiTabItemFlags dataTabFlags   = (s_switchToTab == 3) ? ImGuiTabItemFlags_SetSelected : 0;
        ImGuiTabItemFlags scriptTabFlags = (s_switchToTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        ImGuiTabItemFlags hexTabFlags    = (s_switchToTab == 4) ? ImGuiTabItemFlags_SetSelected : 0;
        ImGuiTabItemFlags matTabFlags    = (s_switchToTab == 5) ? ImGuiTabItemFlags_SetSelected : 0;
        s_switchToTab = -1;

        // =================================================================
        // Materials tab — Unity/Blender-style material slots + texture picker
        // =================================================================
        if (!app.view.materialSlots.empty()) {
            if (ImGui::BeginTabItem("Materials", nullptr, matTabFlags)) {
                // Model info header — shows hashes for mapping
                if (app.view.nm40Hash) {
                    ImGui::TextColored(ImVec4(0.5f,0.8f,1.0f,1), "NM40: 0x%08X", app.view.nm40Hash);
                    for (int si = 0; si < (int)app.view.materialSlots.size(); si++) {
                        auto& s = app.view.materialSlots[si];
                        if (s.pcimHash)
                            ImGui::TextDisabled("  %s: 0x%08X", s.name.c_str(), s.pcimHash);
                    }
                    ImGui::Separator();
                }
                // --- Material Slots ---
                for (int si = 0; si < (int)app.view.materialSlots.size(); si++) {
                    auto& slot = app.view.materialSlots[si];
                    ImGui::PushID(si);
                    bool selected = (si == app.view.selectedSlot);

                    // Slot header with colored indicator
                    ImVec4 col = (si == 0) ? ImVec4(0.3f,0.7f,1.0f,1.0f)
                                           : ImVec4(0.9f,0.6f,0.3f,1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(col.x*0.3f, col.y*0.3f, col.z*0.3f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(col.x*0.4f, col.y*0.4f, col.z*0.4f, 0.7f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(col.x*0.5f, col.y*0.5f, col.z*0.5f, 0.9f));

                    char hdr[64];
                    snprintf(hdr, sizeof(hdr), "%s##slot%d", slot.name.c_str(), si);
                    bool open = ImGui::CollapsingHeader(hdr, selected ? ImGuiTreeNodeFlags_DefaultOpen : 0);
                    ImGui::PopStyleColor(3);

                    // Click header to select slot
                    if (ImGui::IsItemClicked()) app.view.selectedSlot = si;

                    // Show selection indicator
                    if (selected) {
                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 8);
                        ImGui::TextColored(col, ">");
                    }

                    if (open) {
                        // Texture preview (or empty state)
                        if (slot.texId > 0) {
                            float maxW = ImGui::GetContentRegionAvail().x - 16;
                            float prevW = fminf(maxW, 96.0f);
                            float prevH = prevW * (float)slot.texHeight / fmaxf((float)slot.texWidth, 1.0f);
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
                            ImGui::Image((ImTextureID)(intptr_t)slot.texId, ImVec2(prevW, prevH));
                            ImGui::SameLine();
                            ImGui::BeginGroup();
                            ImGui::TextColored(col, "%dx%d", slot.texWidth, slot.texHeight);
                            ImGui::TextDisabled("%d meshes", (int)slot.objectIndices.size());
                            if (ImGui::SmallButton("Clear")) {
                                // Reset to default material
                                slot.texId = 0; slot.texWidth = 0; slot.texHeight = 0;
                            }
                            ImGui::EndGroup();
                        } else {
                            ImGui::TextDisabled("  No texture assigned");
                        }

                        // Inline texture picker (only for selected slot)
                        if (selected && !app.view.lastArchivePath.empty()) {
                            ImGui::Spacing();
                            ImGui::Separator();
                            ImGui::TextDisabled("Textures (click to assign):");

                            FSNode* archNode = app.view.lastArchiveNode;
                            const auto* blob = archNode ? app.cache.GetBlob(app.view.lastArchivePath) : nullptr;
                            if (archNode && blob) {
                                // Collect and sort PCIMs by area (largest first)
                                struct PcimEntry { int idx; uint32_t off, sz; int w, h; };
                                static std::vector<PcimEntry> sorted_pcims;
                                static FSNode* last_sorted_node = nullptr;
                                if (last_sorted_node != archNode) {
                                    sorted_pcims.clear();
                                    for (int ci = 0; ci < (int)archNode->children.size(); ci++) {
                                        auto& c = archNode->children[ci];
                                        if (c.type != AssetType::PCIM) continue;
                                        if (c.offset + 0xA4 >= (uint32_t)blob->size()) continue;
                                        const uint8_t* pd = blob->data() + c.offset;
                                        int pw = pd[0x9C]|(pd[0x9D]<<8)|(pd[0x9E]<<16)|(pd[0x9F]<<24);
                                        int ph = pd[0xA0]|(pd[0xA1]<<8)|(pd[0xA2]<<16)|(pd[0xA3]<<24);
                                        if (pw > 0 && pw < 8192 && ph > 0 && ph < 8192)
                                            sorted_pcims.push_back({ci, c.offset, c.size, pw, ph});
                                    }
                                    std::stable_sort(sorted_pcims.begin(), sorted_pcims.end(),
                                        [](const PcimEntry& a, const PcimEntry& b) { return a.w*a.h > b.w*b.h; });
                                    last_sorted_node = archNode;
                                }

                                // Show sorted PCIMs as a scrollable list with thumbnails
                                float thumbSz = 48.0f;
                                ImGui::BeginChild("##picker", ImVec2(0, 0), false);
                                for (auto& pe : sorted_pcims) {
                                    ImGui::PushID(20000 + pe.idx);

                                    // Lazy-load thumbnail
                                    unsigned int thumbId = 0;
                                    auto it = app.view.thumbCache.find(pe.off);
                                    if (it != app.view.thumbCache.end()) {
                                        thumbId = it->second;
                                    } else if (ImGui::IsRectVisible(ImVec2(thumbSz, thumbSz))) {
                                        // Load on demand when visible
                                        const uint8_t* pd = blob->data() + pe.off;
                                        uint32_t ddsOff = 0xC1;
                                        if (pe.off + ddsOff + 4 < (uint32_t)blob->size()) {
                                            const uint8_t* dds = pd + ddsOff;
                                            int ddsSize = pe.sz > ddsOff ? pe.sz - ddsOff : 0;
                                            if (ddsSize > 128) {
                                                Image img = LoadImageFromMemory(".dds", dds, ddsSize);
                                                if (img.data) {
                                                    Texture2D tex = LoadTextureFromImage(img);
                                                    thumbId = tex.id;
                                                    UnloadImage(img);
                                                }
                                            }
                                        }
                                        app.view.thumbCache[pe.off] = thumbId;
                                        app.view.thumbW[pe.off] = pe.w;
                                        app.view.thumbH[pe.off] = pe.h;
                                    }

                                    // Row: [thumbnail] [dimensions] — clickable
                                    bool clicked = false;
                                    ImGui::BeginGroup();
                                    if (thumbId > 0) {
                                        char ibid[16]; snprintf(ibid, sizeof(ibid), "##ib%d", pe.idx);
                                        clicked = ImGui::ImageButton(ibid, (ImTextureID)(intptr_t)thumbId,
                                            ImVec2(thumbSz, thumbSz));
                                    } else {
                                        char lbl[32]; snprintf(lbl, sizeof(lbl), "%dx%d", pe.w, pe.h);
                                        clicked = ImGui::Button(lbl, ImVec2(thumbSz, thumbSz));
                                    }
                                    ImGui::EndGroup();
                                    ImGui::SameLine();
                                    ImGui::BeginGroup();
                                    ImGui::Text("%dx%d", pe.w, pe.h);
                                    ImGui::TextDisabled("0x%X", pe.off);
                                    ImGui::EndGroup();

                                    if (clicked) {
                                        app.ApplyTextureToModel(app.view.lastArchivePath, pe.off, pe.sz);
                                    }
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::SetTooltip("hash=0x%08X\n%dx%d\nClick to assign to %s",
                                            archNode->children[pe.idx].nameHash, pe.w, pe.h, slot.name.c_str());
                                    }

                                    ImGui::PopID();
                                }
                                ImGui::EndChild();
                            }
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }
        }

        if (ImGui::BeginTabItem("Properties")) {
            if (app.view.selectedObject >= 0 && app.view.selectedObject < (int)app.scene.objects.size()) {
                auto& obj = app.scene.objects[app.view.selectedObject];
                ImGui::Text("%s", obj.name.c_str());
                ImGui::SameLine(); ImGui::Checkbox("Vis", &obj.visible);
                if (obj.meshIndex >= 0 && obj.meshIndex < (int)app.scene.meshes.size()) {
                    auto& m = app.scene.meshes[obj.meshIndex];
                    ImGui::Text("%d verts  %d tris", m.vertexCount, m.triangleCount);
                }
                ImGui::Text("PCRD 0x%X  tex %d", obj.pcrdOffset, obj.texRefKey);
                if (obj.materialIndex >= 0 && obj.materialIndex < (int)app.scene.materials.size()) {
                    unsigned int texId = app.scene.materials[obj.materialIndex].maps[MATERIAL_MAP_DIFFUSE].texture.id;
                    for (auto& td : app.scene.texDiagnostics)
                        if (td.texIndex == obj.texRefKey) {
                            ImGui::Text("%dx%d %s", td.width, td.height, td.format.c_str()); break;
                        }
                    if (texId > 0) ImGui::Image((ImTextureID)(intptr_t)texId, ImVec2(128,128));
                }
            } else { ImGui::TextDisabled("Click an object"); }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Script", nullptr, scriptTabFlags)) {
            if (app.view.scriptText.empty()) { ImGui::TextDisabled("Double-click an SCT to view"); }
            else {
                if (ImGui::Button("Clear")) app.view.scriptText.clear();
                ImGui::Separator();
                ImGui::BeginChild("##sv");
                ImGui::TextUnformatted(app.view.scriptText.c_str(),
                    app.view.scriptText.c_str() + app.view.scriptText.size());
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("VM")) {
            if (!app.vm.loaded) { ImGui::TextDisabled("No script loaded"); }
            else {
                ImGui::Text("%d cls  %d fn  %d ops  %d calls",
                    (int)app.vm.classes.size(), (int)app.vm.functions.size(),
                    app.vm.totalOpsExecuted, app.vm.totalCallsMade);
                if (ImGui::Button("Run Inits")) app.RunAllInits();
                ImGui::SameLine();
                if (ImGui::Button("Clear")) { app.vm.execLog.clear(); app.vm.totalOpsExecuted=0; app.vm.totalCallsMade=0; }
                ImGui::Separator();
                ImGui::BeginChild("##vml");
                for (auto& e : app.vm.execLog) {
                    ImGui::TextColored(ImVec4(0.5f,0.85f,0.5f,1), "%s", e.funcName.c_str());
                    if (!e.detail.empty()) { ImGui::SameLine(); ImGui::Text("%s", e.detail.c_str()); }
                }
                if ((int)app.vm.execLog.size() >= app.vm.maxLogEntries)
                    ImGui::TextColored(ImVec4(1,0.5f,0.5f,1), "...");
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Data", nullptr, dataTabFlags)) {
            if (app.view.dataText.empty()) {
                ImGui::TextDisabled("Double-click an asset to view");
            } else {
                ImGui::Text("%s", app.view.dataTitle.c_str());
                if (ImGui::Button("Clear")) { app.view.dataText.clear(); app.view.dataTitle.clear(); }
                ImGui::SameLine();
                if (ImGui::Button("Copy")) ImGui::SetClipboardText(app.view.dataText.c_str());
                ImGui::Separator();
                ImGui::BeginChild("##dv");
                ImGui::TextUnformatted(app.view.dataText.c_str(),
                    app.view.dataText.c_str() + app.view.dataText.size());
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Hex", nullptr, hexTabFlags)) {
            if (app.view.hexData.empty()) {
                ImGui::TextDisabled("Right-click an asset -> Hex View");
            } else {
                if (ImGui::Button("Clear")) { app.view.hexData.clear(); app.view.hexTitle.clear(); }
                ImGui::Separator();
                ui::DrawHexView(app.view.hexData.data(), (uint32_t)app.view.hexData.size(), app.view.hexTitle);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
