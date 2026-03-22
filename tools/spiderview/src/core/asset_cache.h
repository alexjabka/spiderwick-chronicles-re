#pragma once
// ============================================================================
// asset_cache.h — LRU cache for decompressed ZWD archive blobs
// ============================================================================
// Avoids re-decompressing 10-76MB ZWDs on every operation.
// GetBlob() returns a pointer to the cached decompressed data.

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

class AssetCache {
public:
    // Returns decompressed data for a ZWD path. Cached after first call.
    // Returns nullptr if decompression fails.
    const std::vector<uint8_t>* GetBlob(const std::string& path);

    // Get a sub-range within a cached blob
    struct Span { const uint8_t* data = nullptr; uint32_t size = 0; };
    Span GetAsset(const std::string& archivePath, uint32_t offset, uint32_t size);

    void SetMaxBytes(size_t max) { maxBytes = max; }
    void Evict(const std::string& path);
    void Clear();
    size_t GetUsedBytes() const { return usedBytes; }
    int GetCachedCount() const { return (int)cache.size(); }

private:
    struct Entry {
        std::vector<uint8_t> data;
        uint64_t lastAccess = 0;
    };
    std::unordered_map<std::string, Entry> cache;
    size_t maxBytes = 512 * 1024 * 1024; // 512MB default
    size_t usedBytes = 0;
    uint64_t accessCounter = 0;

    void EnsureSpace(size_t needed);
};
