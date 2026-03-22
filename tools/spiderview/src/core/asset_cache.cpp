// ============================================================================
// asset_cache.cpp — LRU decompression cache for ZWD archives
// ============================================================================

#include "asset_cache.h"
#include "../formats.h"
#include <cstdio>

const std::vector<uint8_t>* AssetCache::GetBlob(const std::string& path) {
    auto it = cache.find(path);
    if (it != cache.end()) {
        it->second.lastAccess = ++accessCounter;
        return &it->second.data;
    }

    // Decompress
    std::vector<uint8_t> blob;
    if (!DecompressPCW(path.c_str(), blob)) {
        printf("[Cache] Failed to decompress: %s\n", path.c_str());
        return nullptr;
    }

    size_t blobSize = blob.size();
    EnsureSpace(blobSize);

    Entry& e = cache[path];
    e.data = std::move(blob);
    e.lastAccess = ++accessCounter;
    usedBytes += blobSize;

    printf("[Cache] Loaded %.1f MB from %s (%d cached, %.0f MB used)\n",
           blobSize / (1024.f*1024.f), path.c_str(), (int)cache.size(), usedBytes / (1024.f*1024.f));
    return &e.data;
}

AssetCache::Span AssetCache::GetAsset(const std::string& archivePath, uint32_t offset, uint32_t size) {
    const auto* blob = GetBlob(archivePath);
    if (!blob || offset + size > (uint32_t)blob->size()) return {};
    return { blob->data() + offset, size };
}

void AssetCache::Evict(const std::string& path) {
    auto it = cache.find(path);
    if (it != cache.end()) {
        usedBytes -= it->second.data.size();
        cache.erase(it);
    }
}

void AssetCache::Clear() {
    cache.clear();
    usedBytes = 0;
}

void AssetCache::EnsureSpace(size_t needed) {
    while (usedBytes + needed > maxBytes && !cache.empty()) {
        // Evict least recently used
        auto oldest = cache.begin();
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.lastAccess < oldest->second.lastAccess)
                oldest = it;
        }
        printf("[Cache] Evicting %s (%.1f MB)\n",
               oldest->first.c_str(), oldest->second.data.size() / (1024.f*1024.f));
        usedBytes -= oldest->second.data.size();
        cache.erase(oldest);
    }
}
