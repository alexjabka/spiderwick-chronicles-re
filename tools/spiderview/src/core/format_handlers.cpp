// ============================================================================
// format_handlers.cpp — Format handler implementations for all known types
//
// To add a new format:
//   1. Write detect/info/calcSize/view functions
//   2. Add one Register() call in RegisterAllFormats()
//   That's it. No other files need changing.
// ============================================================================

#include "format_registry.h"
#include <cstdio>

// ============================================================================
// SCT (Script)
// ============================================================================

static bool SCT_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && d[0]=='S' && d[1]=='C' && d[2]=='T' && d[3]==0;
}

static std::string SCT_Info(const uint8_t* d, uint32_t n) {
    if (n < 8) return "";
    char buf[16]; snprintf(buf, sizeof(buf), "v%d", ReadU32LE(d + 4));
    return buf;
}

static int SCT_CalcSize(const uint8_t* d, uint32_t n) {
    if (n < 12) return -1;
    return (int)(ReadU32LE(d + 8) + 52);
}

// SCT view goes through VM (Script tab), not Data tab — so view=nullptr

// ============================================================================
// NM40 (Skinned Model)
// ============================================================================

static bool NM40_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && memcmp(d, "NM40", 4) == 0;
}

static std::string NM40_Info(const uint8_t* d, uint32_t n) {
    if (n < 0x30) return "";
    uint32_t bones = ReadU16LE(d + 0x08);

    // Scan for embedded PCRD to get accurate vertex/index counts
    uint32_t vc = 0, ic = 0;
    for (uint32_t pos = 0x40; pos + 28 < n && pos < 0x10000; pos += 4) {
        if (memcmp(d + pos, "PCRD", 4) == 0) {
            ic = ReadU32LE(d + pos + 12);
            vc = ReadU32LE(d + pos + 16);
            break;
        }
    }

    char buf[64];
    if (vc > 0)
        snprintf(buf, sizeof(buf), "%d bones %dv %df", bones, vc, ic / 3);
    else
        snprintf(buf, sizeof(buf), "%d bones", bones);
    return buf;
}

static std::string NM40_View(const uint8_t* d, uint32_t n) {
    if (n < 0x40) return "NM40: insufficient data";

    uint32_t version      = ReadU32LE(d + 0x04);
    uint16_t numBones     = ReadU16LE(d + 0x08);
    uint16_t numSkelBones = ReadU16LE(d + 0x0A);
    float scaleX = ReadF32LE(d + 0x0C), scaleY = ReadF32LE(d + 0x10);
    float scaleZ = ReadF32LE(d + 0x14), scaleW = ReadF32LE(d + 0x18);
    float lodDist   = ReadF32LE(d + 0x1C);
    uint8_t flags   = d[0x20];
    uint16_t numSub = ReadU16LE(d + 0x26);
    uint32_t vtxBuf = ReadU32LE(d + 0x28);
    uint32_t idxBuf = ReadU32LE(d + 0x2C);
    uint32_t vtxDeclOff = ReadU32LE(d + 0x30);
    uint32_t meshTblOff = ReadU32LE(d + 0x34);
    uint32_t bonePalOff = ReadU32LE(d + 0x38);
    uint32_t subTblOff  = ReadU32LE(d + 0x3C);

    // Scan for embedded PCRD for accurate vertex/index counts
    int vertCount = 0, idxCount = 0, lodCount = 0;
    uint32_t pcrdStride = 0;
    for (uint32_t pos = 0x40; pos + 28 < n && pos < 0x10000; pos += 4) {
        if (memcmp(d + pos, "PCRD", 4) == 0) {
            if (lodCount == 0) {
                pcrdStride = ReadU32LE(d + pos + 8);
                idxCount   = ReadU32LE(d + pos + 12);
                vertCount  = ReadU32LE(d + pos + 16);
            }
            lodCount++;
        }
    }

    std::string out;
    char buf[512];
    snprintf(buf, sizeof(buf),
        "=== NM40 v%u ===\n"
        "Bones:        %u (skel: %u)\n"
        "Scale:        %.3f, %.3f, %.3f (w=%.3f)\n"
        "LOD dist:     %.1f\n"
        "Flags:        0x%02X\n"
        "Submeshes:    %u\n"
        "Vertices:     %d (stride=%u)\n"
        "Triangles:    %d (indices=%d)\n"
        "LODs:         %d (PCRDs found)\n"
        "AWAD size:    %u bytes\n\n",
        version, numBones, numSkelBones,
        scaleX, scaleY, scaleZ, scaleW,
        lodDist, flags, numSub,
        vertCount, pcrdStride, idxCount / 3, idxCount, lodCount, n);
    out += buf;

    // Offset table — these are fixup'd to pointers at runtime (engine 0x20BC000)
    snprintf(buf, sizeof(buf),
        "--- Offset Table (NM40-relative, fixup'd) ---\n"
        "+0x28 idxDataOff:  +0x%X\n"
        "+0x2C idxBufSize:  %u bytes\n"
        "+0x30 vtxDeclOff:  +0x%X\n"
        "+0x34 meshTblOff:  +0x%X\n"
        "+0x38 bonePalOff:  +0x%X\n"
        "+0x3C subTblOff:   +0x%X\n\n",
        vtxBuf, idxBuf, vtxDeclOff, meshTblOff, bonePalOff, subTblOff);
    out += buf;

    // Vertex format
    if (vtxDeclOff > 0 && vtxDeclOff + 8 <= n) {
        out += "--- Vertex Format ---\n";
        const char* usageNames[] = {"POSITION","BLENDWEIGHT","BLENDINDICES","NORMAL","PSIZE",
            "TEXCOORD","TANGENT","BINORMAL","TESSFACTOR","POSITIONT","COLOR","FOG","DEPTH","SAMPLE"};
        const char* typeNames[] = {"FLOAT1","FLOAT2","FLOAT3","FLOAT4","D3DCOLOR","UBYTE4","SHORT2",
            "SHORT4","UBYTE4N","SHORT2N","SHORT4N","USHORT2N","USHORT4N","UDEC3","DEC3N","FLOAT16_2","FLOAT16_4"};
        for (int vi = 0; vi < 16; vi++) {
            uint32_t vp = vtxDeclOff + vi * 8;
            if (vp + 8 > n) break;
            uint16_t stream = ReadU16LE(d + vp);
            uint16_t voff   = ReadU16LE(d + vp + 2);
            uint8_t vtype   = d[vp + 4];
            uint8_t usage   = d[vp + 6];
            uint8_t uidx    = d[vp + 7];
            if (stream == 0xFF || stream == 0xFFFF) break;
            const char* un = (usage < 14) ? usageNames[usage] : "?";
            const char* tn = (vtype < 17) ? typeNames[vtype] : "?";
            snprintf(buf, sizeof(buf), "  +%u: %s%s (%s)\n",
                voff, un, uidx > 0 ? std::to_string(uidx).c_str() : "", tn);
            out += buf;
        }
        out += "\n";
    }

    // LOD table
    if (lodCount > 0) {
        out += "--- LOD Table ---\n";
        for (int li = 0; li < lodCount; li++) {
            uint32_t lp = meshTblOff + li * 16;
            uint32_t numRemap = ReadU32LE(d + lp + 4);
            uint32_t remapOff = ReadU32LE(d + lp + 8);
            uint32_t pcrdOff  = ReadU32LE(d + lp + 12);
            snprintf(buf, sizeof(buf), "  LOD %d: PCRD=+0x%X  bones=%u  remap=+0x%X\n",
                li, pcrdOff, numRemap, remapOff);
            out += buf;
            if (pcrdOff + 0x34 <= n && memcmp(d + pcrdOff, "PCRD", 4) == 0) {
                uint32_t pcrdVtxSz = ReadU32LE(d + pcrdOff + 0x10);
                uint32_t pcrdIdxSz = ReadU32LE(d + pcrdOff + 0x14);
                snprintf(buf, sizeof(buf), "         %d verts  %d tris\n",
                    (int)(pcrdVtxSz / 52), (int)(pcrdIdxSz / 6));
                out += buf;
            }
        }
    }
    return out;
}

// ============================================================================
// PCWB (World Binary)
// ============================================================================

static bool PCWB_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && memcmp(d, "PCWB", 4) == 0;
}

static int PCWB_CalcSize(const uint8_t* d, uint32_t n) {
    if (n < 16) return -1;
    return (int)ReadU32LE(d + 12);
}

// ============================================================================
// PCIM (Texture)
// ============================================================================

static bool PCIM_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && memcmp(d, "PCIM", 4) == 0;
}

static std::string PCIM_Info(const uint8_t* d, uint32_t n) {
    if (n < 0xA4) return "";
    uint32_t w = ReadU32LE(d + 0x9C);
    uint32_t h = ReadU32LE(d + 0xA0);
    if (w > 0 && w < 8192 && h > 0 && h < 8192) {
        char buf[32]; snprintf(buf, sizeof(buf), "%ux%u", w, h);
        return buf;
    }
    return "";
}

// PCIM preview is GPU-based (inline texture) — view=nullptr

// ============================================================================
// PCRD (Render Data)
// ============================================================================

static bool PCRD_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && memcmp(d, "PCRD", 4) == 0;
}

// ============================================================================
// DBDB (Database)
// ============================================================================

static bool DBDB_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && memcmp(d, "DBDB", 4) == 0;
}

static std::string DBDB_Info(const uint8_t* d, uint32_t n) {
    if (n < 0x10) return "";
    uint32_t ver = ReadU32LE(d + 0x04);
    uint32_t cnt = ReadU32LE(d + 0x0C);
    char buf[32]; snprintf(buf, sizeof(buf), "v%d %d records", ver, cnt);
    return buf;
}

static int DBDB_CalcSize(const uint8_t* d, uint32_t n) {
    if (n < 12) return -1;
    return (int)ReadU32LE(d + 0x08);
}

static std::string DBDB_View(const uint8_t* d, uint32_t n) {
    if (n < 0x20) return "DBDB: insufficient data";

    uint32_t version     = ReadU32LE(d + 0x04);
    uint32_t totalSize   = ReadU32LE(d + 0x08);
    uint32_t recordCount = ReadU32LE(d + 0x0C);
    uint32_t recordOff   = ReadU32LE(d + 0x10);
    uint32_t strRefCount = ReadU32LE(d + 0x14);
    uint32_t dataBlobOff = ReadU32LE(d + 0x18);

    std::string out;
    char buf[512];
    snprintf(buf, sizeof(buf),
        "=== DBDB v%u ===\n"
        "Total size:    %u (0x%X)\n"
        "Records:       %u\n"
        "Record data:   +0x%X\n"
        "String refs:   %u\n"
        "Data blob:     +0x%X\n\n",
        version, totalSize, totalSize, recordCount, recordOff, strRefCount, dataBlobOff);
    out += buf;

    // Parse records
    uint32_t pos = recordOff;
    for (uint32_t r = 0; r < recordCount && pos + 4 <= n; r++) {
        uint32_t fieldCount = ReadU32LE(d + pos); pos += 4;
        snprintf(buf, sizeof(buf), "--- Record %u (%u fields) ---\n", r, fieldCount);
        out += buf;
        for (uint32_t fi = 0; fi < fieldCount && pos + 8 <= n; fi++) {
            uint32_t nameHash = ReadU32LE(d + pos);
            uint32_t rawValue = ReadU32LE(d + pos + 4);
            pos += 8;
            float asFloat; memcpy(&asFloat, &rawValue, 4);
            bool isStr = false;
            std::string strVal;
            if (dataBlobOff > 0 && rawValue >= dataBlobOff && rawValue < n) {
                const uint8_t* sp = d + rawValue;
                uint32_t maxLen = n - rawValue, slen = 0;
                while (slen < maxLen && slen < 256 && sp[slen] >= 0x20 && sp[slen] < 0x7F) slen++;
                if (slen > 1 && sp[slen] == 0) { strVal.assign((const char*)sp, slen); isStr = true; }
            }
            if (isStr)
                snprintf(buf, sizeof(buf), "  [%08X] = \"%s\"\n", nameHash, strVal.c_str());
            else if (asFloat > 0.001f && asFloat < 100000.f && rawValue != 0 && rawValue != 1)
                snprintf(buf, sizeof(buf), "  [%08X] = %u (0x%X) float=%.3f\n", nameHash, rawValue, rawValue, asFloat);
            else
                snprintf(buf, sizeof(buf), "  [%08X] = %u (0x%X)\n", nameHash, rawValue, rawValue);
            out += buf;
        }
    }

    // Data blob strings
    if (dataBlobOff > 0 && dataBlobOff < n) {
        out += "\n=== Data Blob Strings ===\n";
        uint32_t bp = dataBlobOff;
        int strCount = 0;
        while (bp < n && strCount < 500) {
            while (bp < n && d[bp] == 0) bp++;
            if (bp >= n) break;
            uint32_t start = bp;
            while (bp < n && d[bp] >= 0x20 && d[bp] < 0x7F) bp++;
            if (bp > start && bp < n && d[bp] == 0) {
                snprintf(buf, sizeof(buf), "  +0x%X: \"%.*s\"\n", start, (int)(bp - start), d + start);
                out += buf;
                strCount++;
            }
            bp++;
        }
    }
    return out;
}

// ============================================================================
// STTL (Subtitle List)
// ============================================================================

static bool STTL_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && memcmp(d, "STTL", 4) == 0;
}

static std::string STTL_Info(const uint8_t* d, uint32_t n) {
    if (n < 0x10) return "";
    uint32_t ver   = ReadU32LE(d + 0x04);
    uint32_t count = ReadU32LE(d + 0x08);
    char buf[32]; snprintf(buf, sizeof(buf), "v%d %d entries", ver, count);
    return buf;
}

static int STTL_CalcSize(const uint8_t* d, uint32_t n) {
    if (n < 0x10) return -1;
    return (int)ReadU32LE(d + 0x0C);
}

static std::string STTL_View(const uint8_t* d, uint32_t n) {
    if (n < 0x10) return "STTL: insufficient data";

    uint32_t version    = ReadU32LE(d + 0x04);
    uint32_t entryCount = ReadU32LE(d + 0x08);
    uint32_t totalSize  = ReadU32LE(d + 0x0C);

    std::string out;
    char buf[512];
    snprintf(buf, sizeof(buf),
        "=== STTL (SubTitle List) v%u ===\n"
        "Entries:    %u\n"
        "Total size: %u\n\n"
        "%-12s %-10s %-10s %-5s %-5s %-5s\n"
        "%-12s %-10s %-10s %-5s %-5s %-5s\n",
        version, entryCount, totalSize,
        "StringID", "Start", "End", "Loc", "Line", "Spkr",
        "--------", "-----", "---", "---", "----", "----");
    out += buf;

    uint32_t pos = 0x10;
    for (uint32_t i = 0; i < entryCount && pos + 16 <= n; i++) {
        uint32_t stringID = ReadU32LE(d + pos);
        float startTime   = ReadF32LE(d + pos + 4);
        float endTime     = ReadF32LE(d + pos + 8);
        uint8_t localized = d[pos + 12];
        uint8_t lineSlot  = d[pos + 13];
        uint8_t speaker   = d[pos + 14];
        pos += 16;
        snprintf(buf, sizeof(buf), "%08X     %-10.2f %-10.2f %-5d %-5d %-5d\n",
            stringID, startTime, endTime, localized, lineSlot, speaker);
        out += buf;
    }
    return out;
}

// ============================================================================
// AWAD (Archive container)
// ============================================================================

static bool AWAD_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && memcmp(d, "AWAD", 4) == 0;
}

static std::string AWAD_Info(const uint8_t* d, uint32_t n) {
    if (n < 8) return "";
    char buf[32]; snprintf(buf, sizeof(buf), "%u entries", ReadU32LE(d + 4));
    return buf;
}

// ============================================================================
// ZWD (Compressed archive — SFZC or ZLIB magic)
// ============================================================================

static bool ZWD_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && (memcmp(d, "SFZC", 4) == 0 || memcmp(d, "ZLIB", 4) == 0);
}

// ============================================================================
// BIK (Bink Video)
// ============================================================================

static bool BIK_Detect(const uint8_t* d, uint32_t n) {
    return n >= 4 && memcmp(d, "BIKi", 4) == 0;
}

// ============================================================================
// Registration — the single place where all formats are wired up
// ============================================================================

void RegisterAllFormats() {
    auto& r = FormatRegistry::Instance();

    //                 type              name    ext      detect        info        calcSize       view        actionable
    r.Register({AssetType::SCT,         "SCT",  ".sct",  SCT_Detect,  SCT_Info,   SCT_CalcSize,  nullptr,    true });
    r.Register({AssetType::NM40,        "NM40", ".nm40", NM40_Detect, NM40_Info,  nullptr,       NM40_View,  true });
    r.Register({AssetType::PCWB,        "PCWB", ".pcwb", PCWB_Detect, nullptr,    PCWB_CalcSize, nullptr,    true });
    r.Register({AssetType::PCIM,        "PCIM", ".dds",  PCIM_Detect, PCIM_Info,  nullptr,       nullptr,    true });
    r.Register({AssetType::PCRD,        "PCRD", ".pcrd", PCRD_Detect, nullptr,    nullptr,       nullptr,    false});
    r.Register({AssetType::DBDB,        "DBDB", ".dbdb", DBDB_Detect, DBDB_Info,  DBDB_CalcSize, DBDB_View,  false});
    r.Register({AssetType::STTL,        "STTL", ".sttl", STTL_Detect, STTL_Info,  STTL_CalcSize, STTL_View,  false});
    r.Register({AssetType::AWAD,        "AWAD", ".awad", AWAD_Detect, AWAD_Info,  nullptr,       nullptr,    false});
    r.Register({AssetType::ZWD,         "ZWD",  ".zwd",  ZWD_Detect,  nullptr,    nullptr,       nullptr,    false});
    r.Register({AssetType::BIK,         "BIK",  ".bik",  BIK_Detect,  nullptr,    nullptr,       nullptr,    false});
}
