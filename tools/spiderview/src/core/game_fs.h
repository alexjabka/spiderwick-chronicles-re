#pragma once
// ============================================================================
// game_fs.h — Virtual filesystem over game directories + ZWD archives
// ============================================================================

#include "asset_types.h"
#include "asset_cache.h"
#include <string>
#include <vector>

struct FSNode {
    std::string name;
    std::string fullPath;     // disk path (empty for virtual/embedded assets)
    AssetType   type = AssetType::Unknown;
    uint32_t    offset = 0;   // offset within parent archive (0 for disk files)
    uint32_t    size = 0;
    uint32_t    nameHash = 0; // AWAD TOC nameHash (for asset lookup)
    uint32_t    typeHash = 0; // AWAD TOC typeHash (0xBB12=NM40, 0x1F1096F=PCIM)
    std::string extra;        // "v13", "256x128", etc.
    bool        isDir = false;
    bool        expanded = false; // for ZWD: have we scanned contents?
    std::vector<FSNode> children;
};

class GameFileSystem {
public:
    void Discover(const std::string& gameDir);

    // Expand a ZWD node: decompress and populate children with embedded assets
    void ExpandArchive(FSNode& node, AssetCache& cache);

    FSNode& GetRoot() { return root; }
    const std::string& GetGameDir() const { return gameDir; }

private:
    FSNode root;
    std::string gameDir;

    void ScanDirectory(const std::string& path, FSNode& parent, const char* const* subDirs, int subDirCount);
    void ScanArchiveBlob(const std::vector<uint8_t>& blob, FSNode& parent);
};
