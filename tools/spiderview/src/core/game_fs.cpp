// ============================================================================
// game_fs.cpp — Virtual filesystem: scans game dirs, expands ZWD archives
// ============================================================================

#include "game_fs.h"
#include "format_registry.h"
#include "raylib.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

void GameFileSystem::Discover(const std::string& gd) {
    gameDir = gd;
    root = {};
    root.name = "Game";
    root.isDir = true;

    const char* topDirs[] = {"ww", "na", "us"};
    const char* subDirs[] = {"Audio", "DB", "FMV", "Wads", "Worlds"};

    for (auto& td : topDirs) {
        std::string topPath = gameDir + "/" + td;
        if (!DirectoryExists(topPath.c_str())) continue;

        FSNode top;
        top.name = td;
        top.fullPath = topPath;
        top.isDir = true;

        ScanDirectory(topPath, top, subDirs, 5);
        root.children.push_back(std::move(top));
    }
}

void GameFileSystem::ScanDirectory(const std::string& path, FSNode& parent,
                                    const char* const* subDirs, int subDirCount) {
    for (int si = 0; si < subDirCount; si++) {
        std::string subPath = path + "/" + subDirs[si];
        if (!DirectoryExists(subPath.c_str())) continue;

        FSNode sub;
        sub.name = subDirs[si];
        sub.fullPath = subPath;
        sub.isDir = true;

        FilePathList sf = LoadDirectoryFiles(subPath.c_str());
        for (unsigned i = 0; i < sf.count; i++) {
            if (!IsPathFile(sf.paths[i])) continue;
            FSNode f;
            f.name = GetFileName(sf.paths[i]);
            f.fullPath = sf.paths[i];
            const char* ext = GetFileExtension(sf.paths[i]);
            f.type = AssetTypeFromExt(ext);
            f.extra = ext ? ext : "";
            sub.children.push_back(std::move(f));
        }
        UnloadDirectoryFiles(sf);

        std::sort(sub.children.begin(), sub.children.end(),
            [](const FSNode& a, const FSNode& b) { return a.name < b.name; });

        parent.children.push_back(std::move(sub));
    }
}

void GameFileSystem::ExpandArchive(FSNode& node, AssetCache& cache) {
    if (node.expanded) return;
    node.expanded = true;
    node.children.clear();

    const auto* blob = cache.GetBlob(node.fullPath);
    if (!blob) return;

    ScanArchiveBlob(*blob, node);
    printf("[FS] Expanded %s: %d assets\n", node.name.c_str(), (int)node.children.size());
}

void GameFileSystem::ScanArchiveBlob(const std::vector<uint8_t>& blob, FSNode& parent) {
    const uint8_t* d = blob.data();
    uint32_t sz = (uint32_t)blob.size();
    auto& reg = FormatRegistry::Instance();

    // Store decompressed size as parent extra
    char buf[32]; snprintf(buf, sizeof(buf), "%.1f MB", sz / (1024.f*1024.f));
    parent.extra = buf;

    // ---------------------------------------------------------------
    // Parse AWAD Table of Contents for correct asset boundaries.
    // AWAD header: "AWAD"(4) + version(4) + entryCount(4)
    // TOC: entryCount × {nameHash(4), entryPtr(4)}
    // Each entryPtr points to: {typeHash(4), dataOffset(4)}
    // Size = gap to next entry (sorted by dataOffset).
    // ---------------------------------------------------------------
    if (sz >= 12 && memcmp(d, "AWAD", 4) == 0) {
        uint32_t count = d[8]|(d[9]<<8)|(d[10]<<16)|(d[11]<<24);
        if (count > 0 && count < 100000 && 12 + count * 8 < sz) {
            // Parse TOC entries — store nameHash, typeHash, and dataOffset
            // The AWAD uses both nameHash+typeHash for asset lookup (same name, different type)
            struct TocEntry { uint32_t nameHash, typeHash, dataOff; };
            std::vector<TocEntry> toc;
            toc.reserve(count);
            for (uint32_t i = 0; i < count; i++) {
                uint32_t tocBase = 12 + i * 8;
                uint32_t nameHash = d[tocBase]|(d[tocBase+1]<<8)|(d[tocBase+2]<<16)|(d[tocBase+3]<<24);
                uint32_t entPtr   = d[tocBase+4]|(d[tocBase+5]<<8)|(d[tocBase+6]<<16)|(d[tocBase+7]<<24);
                if (entPtr + 8 > sz) continue;
                uint32_t typeHash = d[entPtr]|(d[entPtr+1]<<8)|(d[entPtr+2]<<16)|(d[entPtr+3]<<24);
                uint32_t dataOff  = d[entPtr+4]|(d[entPtr+5]<<8)|(d[entPtr+6]<<16)|(d[entPtr+7]<<24);
                if (dataOff >= sz) continue;
                toc.push_back({nameHash, typeHash, dataOff});
            }

            // Dump NM40↔PCIM relationship analysis
            {
                struct TocInfo { uint32_t nameHash, typeHash, dataOff; };
                std::vector<TocInfo> nm40s, pcimEntries;
                for (auto& e : toc) {
                    if (e.typeHash == 0x0000BB12) nm40s.push_back({e.nameHash, e.typeHash, e.dataOff});
                    if (e.typeHash == 0x01F1096F) pcimEntries.push_back({e.nameHash, e.typeHash, e.dataOff});
                }
                // Log first few NM40 and PCIM hashes for analysis
                printf("[AWAD] === NM40 nameHashes (%d) ===\n", (int)nm40s.size());
                for (auto& e : nm40s)
                    printf("[AWAD]   NM40 hash=0x%08X off=0x%X\n", e.nameHash, e.dataOff);
                printf("[AWAD] === PCIM nameHashes (%d, first 20) ===\n", (int)pcimEntries.size());
                for (int i = 0; i < (int)pcimEntries.size() && i < 20; i++)
                    printf("[AWAD]   PCIM hash=0x%08X off=0x%X\n", pcimEntries[i].nameHash, pcimEntries[i].dataOff);
            }

            // Search AWAD for character PCIM textures using OUR hash function
            // (matches engine's HashString at 0x405380 — verified by decompile)
            {
                std::unordered_map<uint32_t, uint32_t> pcimByHash;
                for (auto& e : toc)
                    if (e.typeHash == 0x01F1096F)
                        pcimByHash[e.nameHash] = e.dataOff;

                const char* charNames[] = {
                    "Mallory","Jared","Simon","GoblinB","Goblin","BullGoblin",
                    "Helen","MrTibbs","StraySod","Thimbletack","HogSqueal",
                    "RedCap","FireSalamander","DarkJared","DarkSimon","DarkMallory",
                    "Lucinda","Actor","SpriteAIObject",nullptr
                };

                int found = 0;
                for (int i = 0; charNames[i]; i++) {
                    // Compute hash using the verified engine algorithm
                    uint32_t h = 0;
                    for (const char* p = charNames[i]; *p; ++p)
                        h += (uint8_t)*p + (h << ((uint8_t)*p & 7));
                    if (pcimByHash.count(h)) {
                        printf("[AWAD] PCIM '%s' = 0x%08X at offset 0x%X\n",
                               charNames[i], h, pcimByHash[h]);
                        found++;
                    }
                }
                printf("[AWAD] %d character PCIMs found (of %d total PCIMs)\n",
                       found, (int)pcimByHash.size());
            }

            // Sort by data offset
            std::sort(toc.begin(), toc.end(), [](const TocEntry& a, const TocEntry& b) {
                return a.dataOff < b.dataOff;
            });

            // Build FSNodes with correct sizes
            for (size_t i = 0; i < toc.size(); i++) {
                uint32_t off = toc[i].dataOff;
                uint32_t nextOff = (i + 1 < toc.size()) ? toc[i+1].dataOff : sz;
                uint32_t size = nextOff - off;
                if (off + 4 > sz) continue;

                AssetType t = reg.Detect(d + off, std::min(size, sz - off));
                if (t == AssetType::Unknown) continue;

                const FormatHandler* handler = reg.Get(t);
                if (!handler) continue;

                FSNode node;
                node.type = t;
                node.offset = off;
                node.size = size;
                node.nameHash = toc[i].nameHash;
                node.typeHash = toc[i].typeHash;
                node.name = handler->name;

                // Info: ask handler for one-line summary
                if (handler->info)
                    node.extra = handler->info(d + off, std::min(size, sz - off));

                parent.children.push_back(std::move(node));
            }

            printf("[FS] Parsed AWAD TOC: %d entries → %d recognized assets\n",
                   (int)toc.size(), (int)parent.children.size());
            return;
        }
    }

    // Fallback: magic-scan on 4-byte boundaries (for non-AWAD blobs)
    for (uint32_t i = 0; i + 8 < sz; i += 4) {
        AssetType t = reg.Detect(d + i, sz - i);
        if (t == AssetType::Unknown) continue;

        const FormatHandler* handler = reg.Get(t);
        if (!handler) continue;

        FSNode node;
        node.type = t;
        node.offset = i;
        node.name = handler->name;

        if (handler->calcSize) {
            int s = handler->calcSize(d + i, sz - i);
            if (s > 0) node.size = (uint32_t)s;
        }
        if (handler->info)
            node.extra = handler->info(d + i, sz - i);

        parent.children.push_back(std::move(node));
    }

    std::sort(parent.children.begin(), parent.children.end(),
        [](const FSNode& a, const FSNode& b) { return a.offset < b.offset; });

    for (int i = 0; i < (int)parent.children.size(); i++) {
        if (parent.children[i].size == 0 && parent.children[i].offset > 0) {
            uint32_t nextOff = sz;
            for (int j = i + 1; j < (int)parent.children.size(); j++) {
                if (parent.children[j].offset > parent.children[i].offset) {
                    nextOff = parent.children[j].offset;
                    break;
                }
            }
            parent.children[i].size = nextOff - parent.children[i].offset;
        }
    }
}
