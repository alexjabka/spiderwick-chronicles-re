#pragma once
// ============================================================================
// format_registry.h — Self-describing format handler registry
//
// Each game format (NM40, DBDB, STTL, PCIM, etc.) registers a handler that
// describes how to detect, inspect, size, view, and export it.
// Adding a new format = writing one handler struct. Core code never changes.
//
// Pattern: CodeWalker FileTypes / AssetStudio ClassIDType / Noesis register()
// ============================================================================

#include "asset_types.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// Byte-reading helpers (shared by all handlers)
// ============================================================================

inline uint32_t ReadU32LE(const uint8_t* d) {
    return d[0] | (d[1]<<8) | (d[2]<<16) | (d[3]<<24);
}
inline uint16_t ReadU16LE(const uint8_t* d) {
    return d[0] | (d[1]<<8);
}
inline float ReadF32LE(const uint8_t* d) {
    uint32_t v = ReadU32LE(d);
    float f; memcpy(&f, &v, 4);
    return f;
}

// ============================================================================
// FormatHandler — describes one asset format's capabilities
// ============================================================================

struct FormatHandler {
    AssetType   type;
    const char* name;       // Display name: "NM40", "DBDB", "SCT"
    const char* exportExt;  // Export file extension: ".nm40", ".dds"

    // --- Capabilities (NULL = not supported) ---

    // Detect: return true if data starts with this format's magic
    bool (*detect)(const uint8_t* data, uint32_t avail);

    // Info: one-line summary for archive tree (e.g. "17 bones 128v 8192f")
    std::string (*info)(const uint8_t* data, uint32_t avail);

    // CalcSize: return total asset size from header fields, or -1 for gap-based
    int (*calcSize)(const uint8_t* data, uint32_t avail);

    // View: generate detailed text for Data tab (pure data parsing, no GPU/VM)
    std::string (*view)(const uint8_t* data, uint32_t size);

    // Is this type openable in a special viewer? (3D scene, inline texture, script)
    bool actionable;
};

// ============================================================================
// FormatRegistry — central lookup for all registered formats
// ============================================================================

class FormatRegistry {
public:
    static FormatRegistry& Instance() {
        static FormatRegistry r;
        return r;
    }

    void Register(const FormatHandler& h) { handlers_.push_back(h); }

    // Detect format from raw bytes (returns Unknown if no match)
    AssetType Detect(const uint8_t* data, uint32_t avail) const {
        if (avail < 4) return AssetType::Unknown;
        for (auto& h : handlers_)
            if (h.detect && h.detect(data, avail)) return h.type;
        return AssetType::Unknown;
    }

    // Get handler for a known type (returns nullptr if unregistered)
    const FormatHandler* Get(AssetType type) const {
        for (auto& h : handlers_)
            if (h.type == type) return &h;
        return nullptr;
    }

    // Convenience accessors with fallbacks
    const char* GetName(AssetType type) const {
        auto* h = Get(type);
        return h ? h->name : "???";
    }

    const char* GetExportExt(AssetType type) const {
        auto* h = Get(type);
        return h ? h->exportExt : ".bin";
    }

    bool IsActionable(AssetType type) const {
        auto* h = Get(type);
        return h ? h->actionable : false;
    }

    const std::vector<FormatHandler>& All() const { return handlers_; }

private:
    FormatRegistry() = default;
    std::vector<FormatHandler> handlers_;
};

// Call once at startup (defined in format_handlers.cpp)
void RegisterAllFormats();
