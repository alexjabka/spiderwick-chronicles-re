// ============================================================================
// formats.cpp — Spiderwick Chronicles binary format parsers
// ============================================================================
// See formats.h for format documentation.

#include "formats.h"
#include <zlib.h>
#include <fstream>
#include <cmath>
#include <algorithm>

// ============================================================================
// PCWBFile — low-level readers
// ============================================================================

uint32_t PCWBFile::ReadU32(size_t off) const {
    if (off + 4 > data.size()) return 0;
    uint32_t v;
    memcpy(&v, &data[off], 4);
    return v;
}

float PCWBFile::ReadF32(size_t off) const {
    if (off + 4 > data.size()) return 0.0f;
    float v;
    memcpy(&v, &data[off], 4);
    return v;
}

// ============================================================================
// Decompression — ZLIB/SFZC wrapper used by PCW and ZWD files
// ============================================================================

bool DecompressPCW(const char* path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;

    size_t fileSize = (size_t)f.tellg();
    if (fileSize < 12) return false;
    f.seekg(0, std::ios::beg);

    // Header: 4-byte magic + 4-byte compressed size + 4-byte decompressed size
    char magic[4];
    f.read(magic, 4);
    if (memcmp(magic, "ZLIB", 4) != 0 && memcmp(magic, "SFZC", 4) != 0)
        return false;

    uint32_t compSize, decompSize;
    f.read(reinterpret_cast<char*>(&compSize), 4);
    f.read(reinterpret_cast<char*>(&decompSize), 4);

    if (decompSize == 0 || decompSize > 200 * 1024 * 1024) return false;

    std::vector<uint8_t> compressed(fileSize - 12);
    f.read(reinterpret_cast<char*>(compressed.data()), compressed.size());

    out.resize(decompSize);
    uLongf destLen = decompSize;
    int ret = uncompress(out.data(), &destLen, compressed.data(), (uLong)compressed.size());
    if (ret != Z_OK) return false;

    out.resize(destLen);
    return true;
}

// ============================================================================
// ZWDArchive — AWAD asset container
// ============================================================================

bool ZWDArchive::Load(const char* filepath) {
    this->path = filepath;
    std::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;

    size_t fileSize = (size_t)f.tellg();
    if (fileSize < 16) return false;
    f.seekg(0, std::ios::beg);

    // Check for compression wrapper
    char magic[4];
    f.read(magic, 4);
    f.seekg(0, std::ios::beg);

    if (memcmp(magic, "ZLIB", 4) == 0 || memcmp(magic, "SFZC", 4) == 0) {
        f.close();
        if (!DecompressPCW(filepath, data)) return false;
    } else {
        data.resize(fileSize);
        f.read(reinterpret_cast<char*>(data.data()), fileSize);
    }

    // AWAD header: magic(4) + entry_count(4) + unknown(4) + entries(12 each)
    if (data.size() < 16 || memcmp(data.data(), "AWAD", 4) != 0)
        return false;

    uint32_t entryCount;
    memcpy(&entryCount, &data[4], 4);
    if (entryCount > 100000) return false;

    entries.clear();
    for (uint32_t i = 0; i < entryCount; i++) {
        size_t entryOff = 12 + i * 12;
        if (entryOff + 12 > data.size()) break;

        ZWDEntry e;
        memcpy(&e.nameHash, &data[entryOff], 4);
        memcpy(&e.offset,   &data[entryOff + 4], 4);
        memcpy(&e.size,     &data[entryOff + 8], 4);

        // Detect asset type from magic bytes
        if (e.offset + 4 <= data.size()) {
            const uint8_t* p = &data[e.offset];
            if      (memcmp(p, "PCWB", 4) == 0) e.extension = ".pcwb";
            else if (memcmp(p, "PCIM", 4) == 0) e.extension = ".pcim";
            else if (memcmp(p, "NM40", 4) == 0) e.extension = ".nm40";
            else if (memcmp(p, "SCT\0", 4) == 0) e.extension = ".sct";
            else if (memcmp(p, "DBDB", 4) == 0) e.extension = ".dbdb";
            else if (memcmp(p, "STTL", 4) == 0) e.extension = ".sttl";
            else if (memcmp(p, "AWAD", 4) == 0) e.extension = ".awad";
            else e.extension = ".bin";
        }

        e.name = "0x" + std::to_string(e.nameHash);
        entries.push_back(e);
    }
    return true;
}

const uint8_t* ZWDArchive::GetEntryData(int index) const {
    if (index < 0 || index >= (int)entries.size()) return nullptr;
    if (entries[index].offset >= data.size()) return nullptr;
    return &data[entries[index].offset];
}

int ZWDArchive::FindByHash(uint32_t hash) const {
    for (int i = 0; i < (int)entries.size(); i++) {
        if (entries[i].nameHash == hash) return i;
    }
    return -1;
}

// ============================================================================
// PCWBFile — loading
// ============================================================================

bool PCWBFile::Load(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;

    size_t fileSize = (size_t)f.tellg();
    if (fileSize < 12) return false;
    f.seekg(0, std::ios::beg);

    // Auto-detect: compressed (.pcw) vs raw (.pcwb)
    char magic[4];
    f.read(magic, 4);
    f.seekg(0, std::ios::beg);

    if (memcmp(magic, "ZLIB", 4) == 0 || memcmp(magic, "SFZC", 4) == 0) {
        f.close();
        if (!DecompressPCW(path, data)) return false;
    } else {
        data.resize(fileSize);
        f.read(reinterpret_cast<char*>(data.data()), fileSize);
    }

    if (data.size() < 0x100 || memcmp(data.data(), "PCWB", 4) != 0) return false;
    return Parse();
}

bool PCWBFile::LoadFromBuffer(std::vector<uint8_t>&& buf) {
    data = std::move(buf);
    if (data.size() < 0x100 || memcmp(data.data(), "PCWB", 4) != 0) return false;
    return Parse();
}

bool PCWBFile::Parse() {
    FindPCRDs();
    FindPCIMs();
    BuildTexRefTable();
    BuildPCRDTexMap();
    ParseProps();
    ParseInstanceTransforms();
    return !pcrdOffsets.empty();
}

// ============================================================================
// FindPCRDs — scan for all PCRD geometry chunks
// ============================================================================
// PCRD header layout:
//   +0x00: "PCRD" magic
//   +0x04: version (must be 2)
//   +0x08: hs — vertex format flag (determines stride: hs <= 0x10 → 32, else 24)
//   +0x0C: index count (triangle strip indices)
//   +0x10: vertex count
//   +0x14: index buffer offset (absolute within PCWB)
//   +0x18: vertex buffer offset (absolute within PCWB)

void PCWBFile::FindPCRDs() {
    pcrdOffsets.clear();
    for (size_t pos = 0; pos + 0x1C < data.size(); pos += 4) {
        if (memcmp(&data[pos], "PCRD", 4) == 0 && ReadU32(pos + 4) == 2) {
            uint32_t ic = ReadU32(pos + 0x0C);
            uint32_t vc = ReadU32(pos + 0x10);
            if (ic > 0 && ic < 1000000 && vc > 0 && vc < 1000000) {
                pcrdOffsets.push_back((uint32_t)pos);
            }
        }
    }
}

// ============================================================================
// FindPCIMs — scan for all PCIM texture chunks
// ============================================================================
// PCIM header (193 = 0xC1 bytes):
//   +0x00: "PCIM" magic
//   +0x04: version (must be 2)
//   +0x08: total section size
//   +0x0C: DDS data size
//   +0x10: DDS data offset (absolute or relative to PCIM)
//   +0x9C: texture width
//   +0xA0: texture height

void PCWBFile::FindPCIMs() {
    pcimOffsets.clear();
    for (size_t pos = 0; pos + 0xC1 < data.size(); pos += 4) {
        if (memcmp(&data[pos], "PCIM", 4) != 0) continue;

        uint32_t ver  = ReadU32(pos + 4);
        uint32_t tsz  = ReadU32(pos + 8);
        uint32_t dsz  = ReadU32(pos + 0x0C);
        uint32_t doff = ReadU32(pos + 0x10);

        if (ver != 2 || tsz == 0 || tsz >= data.size() || dsz == 0 || dsz > tsz)
            continue;

        // DDS data can be at absolute offset or immediately after the header
        bool ok = false;
        if (doff + 4 <= data.size() && memcmp(&data[doff], "DDS ", 4) == 0)
            ok = true;
        else if (pos + 0xC1 + 4 <= data.size() && memcmp(&data[pos + 0xC1], "DDS ", 4) == 0)
            ok = true;

        if (ok) {
            pcimOffsets.push_back((uint32_t)pos);
            // Skip past this PCIM to avoid re-scanning its DDS data
            if (doff != (uint32_t)(pos + 0xC1))
                pos += 0xC4 - 4;
            else
                pos += ((0xC1 + dsz + 3) & ~3u) - 4;
        }
    }
}

// ============================================================================
// BuildTexRefTable — texture index → PCIM offset mapping
// ============================================================================
// Two discovery methods:
//   1. Header table at PCWB+0x94: sequential 16-byte entries (texIndex, pcimOffset, 0, 0)
//   2. Fallback: check 16 bytes before each unresolved PCIM for (texIndex, pcimOffset, 0, 0)

void PCWBFile::BuildTexRefTable() {
    texRef.clear();

    // Method 1: Parse header table at PCWB+0x94
    // Each entry is 16 bytes: (texIndex, pcimOffset, 0, 0)
    uint32_t tp = ReadU32(0x94);
    if (tp > 0 && tp + 16 <= data.size()) {
        size_t pos = tp;
        while (pos + 16 <= data.size()) {
            uint32_t ti = ReadU32(pos);
            uint32_t po = ReadU32(pos + 4);
            if (po + 4 <= data.size() && memcmp(&data[po], "PCIM", 4) == 0) {
                texRef[(int)ti] = po;
            } else {
                break;  // End of table
            }
            pos += 16;
        }
    }

    // Method 2: Fallback — check 16 bytes before each PCIM for inline ref entry
    // Pattern: (texIndex, pcimOffset, 0, 0) immediately before the PCIM
    std::set<uint32_t> found;
    for (auto& [k, v] : texRef) found.insert(v);

    for (uint32_t po : pcimOffsets) {
        if (found.count(po)) continue;  // Already mapped
        if (po >= 16) {
            uint32_t pti = ReadU32(po - 16);
            uint32_t ppo = ReadU32(po - 12);
            uint32_t pz1 = ReadU32(po - 8);
            uint32_t pz2 = ReadU32(po - 4);
            if (ppo == po && pz1 == 0 && pz2 == 0) {
                texRef[(int)pti] = po;
                found.insert(po);
            }
        }
    }

    // Dump texRef table for diagnostics
    FILE* trf = fopen("texref_dump.txt", "w");
    if (trf) {
        fprintf(trf, "=== texRef table (texID → PCIM offset) ===\n");
        fprintf(trf, "Total entries: %zu, Total PCIMs: %zu\n\n", texRef.size(), pcimOffsets.size());
        for (auto& [k, v] : texRef)
            fprintf(trf, "  texRef[%d] = PCIM@0x%08X\n", k, v);
        fprintf(trf, "\n=== PCIM array (sequential index → offset) ===\n");
        for (size_t i = 0; i < pcimOffsets.size(); i++)
            fprintf(trf, "  pcimOffsets[%zu] = 0x%08X\n", i, pcimOffsets[i]);
        fclose(trf);
    }
}

// ============================================================================
// BuildPCRDTexMap — PCRD offset → texture index mapping
// ============================================================================
// Uses the "batch sentinel" pattern found in the engine's draw batch structures:
//   batch+0x04: texture index
//   batch+0x08: 0xFFFFFFFF  (sentinel start)
//   batch+0x0C: 0xFFFFFFFF
//   batch+0x10: 0xFFFFFFFF  (sentinel end)
//   batch+0x2C: PCRD offset

void PCWBFile::BuildPCRDTexMap() {
    pcrdTexMap.clear();
    std::set<uint32_t> pcrdSet(pcrdOffsets.begin(), pcrdOffsets.end());

    // Pass 1: collect ALL batch sentinel mappings (PCRD → list of texture indices).
    // Some PCRDs have two batches: base texture (large) + shadow overlay (small).
    std::map<uint32_t, std::vector<int>> allMappings;

    FILE* dumpF = fopen("batch_sentinel_dump.txt", "w");
    if (dumpF) {
        fprintf(dumpF, "PCIM count: %zu\n\n", pcimOffsets.size());

        // Dump PCWB header region 0x00-0x100 for structure analysis
        fprintf(dumpF, "=== PCWB header (0x00-0xFF) ===\n");
        for (size_t r = 0; r < 0x100 && r + 4 <= data.size(); r += 4) {
            if (r % 32 == 0) fprintf(dumpF, "  +0x%02X:", (unsigned)r);
            fprintf(dumpF, " %08X", ReadU32(r));
            if (r % 32 == 28) fprintf(dumpF, "\n");
        }
        fprintf(dumpF, "\n");

        // Dump unexplored header pointer targets
        uint32_t hdrPtrs[] = {
            ReadU32(0x9C), ReadU32(0xA0), ReadU32(0xA4), ReadU32(0xA8)
        };
        const char* hdrNames[] = {"+0x9C", "+0xA0", "+0xA4", "+0xA8"};
        for (int hp = 0; hp < 4; hp++) {
            uint32_t ptr = hdrPtrs[hp];
            if (ptr > 0 && ptr + 256 < data.size()) {
                fprintf(dumpF, "=== Header %s -> 0x%X (256 bytes) ===\n", hdrNames[hp], ptr);
                for (size_t r = ptr; r < ptr + 256 && r + 4 <= data.size(); r += 4) {
                    if ((r - ptr) % 32 == 0) fprintf(dumpF, "  +0x%05X:", (unsigned)r);
                    fprintf(dumpF, " %08X", ReadU32(r));
                    if ((r - ptr) % 32 == 28) fprintf(dumpF, "\n");
                }
                fprintf(dumpF, "\n");
            }
        }
        // Parse draw data table at header+0xA4
        // First 'propCount' entries are 20 bytes (prop draw data)
        // Remaining entries are 8 bytes (count, pointer) = world geometry groups
        {
            uint32_t ddTablePtr = ReadU32(0xA4);
            uint32_t ddCount = ReadU32(0x4C);  // drawDataCount
            uint32_t propCnt = ReadU32(0x50);
            fprintf(dumpF, "=== Draw Data Table (header+0xA4=0x%X, count=%u, props=%u) ===\n",
                ddTablePtr, ddCount, propCnt);

            // Skip prop entries (20 bytes each)
            uint32_t worldStart = ddTablePtr + propCnt * 20;
            uint32_t worldGroupCount = (ddCount > propCnt) ? ddCount - propCnt : 0;

            fprintf(dumpF, "World geometry groups start at 0x%X, count=%u\n\n", worldStart, worldGroupCount);

            for (uint32_t gi = 0; gi < worldGroupCount && gi < 20; gi++) {
                uint32_t gOff = worldStart + gi * 8;
                if (gOff + 8 > data.size()) break;
                uint32_t itemCount = ReadU32(gOff);
                uint32_t listPtr = ReadU32(gOff + 4);
                fprintf(dumpF, "  WorldGroup[%u]: %u items at 0x%X\n", gi, itemCount, listPtr);

                // Dump first 10 items of each group to understand the structure
                if (listPtr > 0 && listPtr + 4 <= data.size()) {
                    // Try to determine item size by looking at the data
                    fprintf(dumpF, "    Raw data (first 128 bytes):\n");
                    for (size_t r = listPtr; r < listPtr + 128 && r + 4 <= data.size(); r += 4) {
                        if ((r - listPtr) % 32 == 0) fprintf(dumpF, "    +%03X:", (unsigned)(r - listPtr));
                        fprintf(dumpF, " %08X", ReadU32(r));
                        if ((r - listPtr) % 32 == 28) fprintf(dumpF, "\n");
                    }
                    fprintf(dumpF, "\n");
                }
            }
        }
    }

    // Track PCRDs that have sentinel entries but ALL are out-of-range
    std::map<uint32_t, std::vector<uint32_t>> allRawMappings; // pcrd → ALL ti values (even invalid)

    for (size_t pos = 0; pos + 12 < data.size(); pos += 4) {
        if (ReadU32(pos) == 0xFFFFFFFF &&
            ReadU32(pos + 4) == 0xFFFFFFFF &&
            ReadU32(pos + 8) == 0xFFFFFFFF) {

            if (pos >= 8 && pos + 48 <= data.size()) {
                uint32_t ti = ReadU32(pos - 4);    // texture index before sentinel
                uint32_t pr = ReadU32(pos + 36);   // PCRD offset after sentinel

                // Filter false positives: sentinel inside DDS texture data
                // Real sentinels have [-8] == 0 and small ti values
                bool likelySentinel = (ReadU32(pos - 8) == 0 && ti < 10000);

                if (likelySentinel && pcrdSet.count(pr)) {
                    allRawMappings[pr].push_back(ti);

                    if (dumpF) {
                        // Dump full 52-byte batch entry: sub_batch starts at pos-8
                        fprintf(dumpF, "sentinel @0x%zX  ti=%u  pcrd=0x%08X  inRange=%s\n",
                            pos, ti, pr, (ti < pcimOffsets.size()) ? "YES" : "NO");
                        // Full batch context: 13 dwords from pos-8 (sub_batch+0)
                        fprintf(dumpF, "  batch:");
                        size_t bstart = pos - 8;
                        for (int b = 0; b < 13 && bstart + b*4 + 4 <= data.size(); b++)
                            fprintf(dumpF, " %08X", ReadU32(bstart + b*4));
                        fprintf(dumpF, "\n");
                    }

                    if (ti < pcimOffsets.size())
                        allMappings[pr].push_back((int)ti);
                }
            }
            pos += 8;
        }
    }

    // Report PCRDs with no valid texture mapping
    if (dumpF) {
        fprintf(dumpF, "\n=== PCRDs with NO valid texture mapping ===\n");
        int unmapped = 0;
        for (auto& kv : allRawMappings) {
            bool hasValid = false;
            for (uint32_t t : kv.second)
                if (t < pcimOffsets.size()) { hasValid = true; break; }
            if (!hasValid) {
                fprintf(dumpF, "  PCRD 0x%08X: ti values =", kv.first);
                for (uint32_t t : kv.second) fprintf(dumpF, " %u", t);
                fprintf(dumpF, "\n");
                unmapped++;
            }
        }
        fprintf(dumpF, "Total unmapped: %d / %zu PCRDs\n", unmapped, pcrdOffsets.size());

        fprintf(dumpF, "\n=== PCRDs with multiple texture mappings ===\n");
        for (auto& kv : allMappings) {
            if (kv.second.size() > 1) {
                fprintf(dumpF, "  PCRD 0x%08X: textures =", kv.first);
                for (int t : kv.second) {
                    size_t ddsOff = pcimOffsets[t] + 0xC1;
                    uint32_t w = 0, h = 0;
                    if (ddsOff + 20 < data.size()) {
                        h = ReadU32(ddsOff + 12);
                        w = ReadU32(ddsOff + 16);
                    }
                    fprintf(dumpF, " %d(%ux%u)", t, w, h);
                }
                fprintf(dumpF, "\n");
            }
        }
    }
    if (dumpF) {
        // Dump PCIM dimensions using PCIM header fields (+0x9C=width, +0xA0=height)
        fprintf(dumpF, "\n=== PCIM texture dimensions ===\n");
        std::set<int> usedTexKeys; // track which texRef keys sentinels use
        for (auto& kv : allMappings)
            for (int ti : kv.second) usedTexKeys.insert(ti);

        for (size_t i = 0; i < pcimOffsets.size(); i++) {
            uint32_t pcim = pcimOffsets[i];
            uint32_t w = 0, h = 0;
            if (pcim + 0xA4 < data.size()) {
                w = ReadU32(pcim + 0x9C);
                h = ReadU32(pcim + 0xA0);
            }
            // Find DDS format
            const char* fmt = "?";
            uint32_t doff = ReadU32(pcim + 0x10);
            size_t ddsOff = (doff + 4 <= data.size() && memcmp(&data[doff], "DDS ", 4) == 0)
                          ? doff : pcim + 0xC1;
            if (ddsOff + 88 < data.size() && memcmp(&data[ddsOff], "DDS ", 4) == 0) {
                uint32_t flags = ReadU32(ddsOff + 80);
                uint32_t fourcc = ReadU32(ddsOff + 84);
                if (flags & 0x4)
                    fmt = (fourcc == 0x31545844) ? "DXT1" :
                          (fourcc == 0x33545844) ? "DXT3" :
                          (fourcc == 0x35545844) ? "DXT5" : "FCC?";
                else {
                    uint32_t bpp = ReadU32(ddsOff + 88);
                    fmt = (bpp == 32) ? "RGBA32" : (bpp == 24) ? "RGB24" : "RAW";
                }
            }
            int texKey = -1;
            for (auto& [k, v] : texRef) { if (v == pcimOffsets[i]) { texKey = k; break; } }
            bool used = usedTexKeys.count(texKey) > 0;
            fprintf(dumpF, "  PCIM[%2zu] texKey=%2d  %4ux%-4u  %-5s  %s  @0x%08X\n",
                i, texKey, w, h, fmt, used ? "USED" : "ORPHAN", pcimOffsets[i]);
        }

        // List orphan textures (in texRef but never used by any sentinel)
        fprintf(dumpF, "\n=== ORPHAN textures (in texRef but no sentinel uses them) ===\n");
        for (auto& [k, v] : texRef) {
            if (usedTexKeys.find(k) == usedTexKeys.end()) {
                uint32_t w = 0, h = 0;
                if (v + 0xA4 < data.size()) { w = ReadU32(v + 0x9C); h = ReadU32(v + 0xA0); }
                fprintf(dumpF, "  texKey=%d  %ux%u  PCIM@0x%08X\n", k, w, h, v);
            }
        }

        // Deep scan: for each PCRD, search ALL occurrences of its offset in the raw PCWB data
        // Scan from start to first PCIM (covers header + sentinels + PCRDs but NOT texture data)
        fprintf(dumpF, "\n=== Deep scan: ALL raw references to each PCRD offset ===\n");

        uint32_t scanEnd = pcimOffsets.empty() ? (uint32_t)data.size() : pcimOffsets[0];

        for (size_t pi = 0; pi < pcrdOffsets.size() && pi < 50; pi++) { // first 50 PCRDs
            uint32_t target = pcrdOffsets[pi];
            std::vector<size_t> refs;
            for (size_t s = 0; s + 4 <= scanEnd; s += 4) {
                if (ReadU32(s) == target) refs.push_back(s);
            }
            if (refs.size() != 1) { // only report PCRDs with unexpected ref count
                fprintf(dumpF, "  PCRD[%zu] @0x%08X: %zu refs in header region", pi, target, refs.size());
                for (size_t r : refs) fprintf(dumpF, " @0x%zX", r);
                fprintf(dumpF, "\n");
                // For each ref, dump surrounding context (±16 bytes)
                for (size_t r : refs) {
                    size_t ctx_start = (r >= 32) ? r - 32 : 0;
                    size_t ctx_end = (r + 48 < scanEnd) ? r + 48 : scanEnd;
                    fprintf(dumpF, "    context @0x%zX:", ctx_start);
                    for (size_t b = ctx_start; b < ctx_end; b += 4) {
                        if (b == r) fprintf(dumpF, " [%08X]", ReadU32(b));
                        else fprintf(dumpF, " %08X", ReadU32(b));
                    }
                    fprintf(dumpF, "\n");
                }
            }
        }

        // Prop texture chain: for each prop sub-mesh, read the texture reference at batchPtr+4
        // This is the engine's sub_batch+4 = relative offset to texture entry (from sub_55A850)
        fprintf(dumpF, "\n=== Prop sub-mesh texture references (batchPtr+4 = texture offset) ===\n");
        {
            uint32_t propCount = ReadU32(0x50);
            uint32_t propTable = ReadU32(0x98);
            if (propCount > 0 && propCount < 1000 && propTable > 0) {
                for (uint32_t pi = 0; pi < propCount; pi++) {
                    uint32_t entry = propTable + pi * 0xA0;
                    if (entry + 0xA0 > data.size()) break;
                    char name[64] = {0};
                    for (int c = 0; c < 63 && entry + 0x60 + c < data.size(); c++) {
                        uint8_t ch = data[entry + 0x60 + c];
                        if (!ch) break;
                        name[c] = (char)ch;
                    }
                    uint32_t defPtr = ReadU32(entry + 0x8C);
                    if (!defPtr || defPtr + 12 > data.size()) continue;
                    uint32_t pcrdCount = ReadU32(defPtr + 4);
                    uint32_t meshListOff = ReadU32(defPtr + 8);
                    if (!pcrdCount || pcrdCount > 10000 || !meshListOff) continue;
                    for (uint32_t mi = 0; mi < pcrdCount; mi++) {
                        uint32_t blockOff = ReadU32(meshListOff + mi * 4);
                        if (blockOff + 20 > data.size()) continue;
                        uint32_t subCount = ReadU32(blockOff);
                        uint32_t smList = ReadU32(blockOff + 16);
                        if (!subCount || subCount > 100 || !smList) continue;
                        for (uint32_t si = 0; si < subCount; si++) {
                            uint32_t smPtr = ReadU32(smList + si * 4);
                            if (smPtr + 16 > data.size()) continue;
                            uint32_t batchPtr = ReadU32(smPtr + 12);
                            if (batchPtr + 52 > data.size()) continue;
                            uint32_t pcrdOff = ReadU32(batchPtr + 44);
                            uint32_t texObjOff = ReadU32(batchPtr + 4); // texture ref (relative?)
                            uint32_t renderType = ReadU32(batchPtr + 26) & 0xFFFF; // short at +26
                            // Also read sentinel-style texIndex: batch sentinel ti is at specific offset
                            // In the engine, the texture ref at batchPtr+4 is relative to prop def base
                            fprintf(dumpF, "  prop[%u]='%s' mesh[%u] sub[%u]: batchPtr=0x%X pcrd=0x%X texOff=0x%X rtype=0x%X\n",
                                pi, name, mi, si, batchPtr, pcrdOff, texObjOff, renderType);
                            // Dump raw batch data (first 52 bytes)
                            fprintf(dumpF, "    batch[0..51]:");
                            for (int b = 0; b < 52 && batchPtr + b + 4 <= data.size(); b += 4)
                                fprintf(dumpF, " %08X", ReadU32(batchPtr + b));
                            fprintf(dumpF, "\n");
                        }
                    }
                }
            }
        }
    }
    if (dumpF) fclose(dumpF);

    // Pass 2: for each PCRD, pick the texture with the LARGEST resolution.
    // Base textures are large (256x256, 512x512), shadow overlays are small (32x32, 64x64).
    for (std::map<uint32_t, std::vector<int>>::iterator it = allMappings.begin(); it != allMappings.end(); ++it) {
        uint32_t pcrd = it->first;
        std::vector<int>& texIndices = it->second;
        int bestTex = texIndices[0];
        int bestArea = 0;

        for (size_t j = 0; j < texIndices.size(); j++) {
            int ti = texIndices[j];
            if (ti >= 0 && ti < (int)pcimOffsets.size()) {
                // Read DDS width/height from PCIM: DDS header at pcimOffset + 0xC1
                size_t ddsOff = pcimOffsets[ti] + 0xC1;
                if (ddsOff + 20 < data.size()) {
                    uint32_t h = ReadU32(ddsOff + 12);
                    uint32_t w = ReadU32(ddsOff + 16);
                    int area = (int)(w * h);
                    if (area > bestArea) {
                        bestArea = area;
                        bestTex = ti;
                    }
                }
            }
        }

        pcrdTexMap[pcrd] = bestTex;
    }
}

// ============================================================================
// ParseProps — extract prop instances from PCWB header
// ============================================================================
// Prop chain: prop_entry+0x8C → def_ptr → mesh_list → block → submesh → batch → PCRD
// This multi-level indirection matches how the engine references renderable geometry.

void PCWBFile::ParseProps() {
    props.clear();

    uint32_t propCount = ReadU32(0x50);
    uint32_t propTable = ReadU32(0x98);
    const uint32_t STRIDE = 0xA0;

    if (propCount == 0 || propCount > 1000 || propTable == 0) return;

    for (uint32_t pi = 0; pi < propCount; pi++) {
        uint32_t entry = propTable + pi * STRIDE;
        if (entry + STRIDE > data.size()) break;

        PropInfo prop;

        // Row-major 4x4 transform matrix (D3D9 convention: world = vertex * M)
        for (int j = 0; j < 16; j++)
            prop.matrix[j] = ReadF32(entry + j * 4);

        // Translation is in the last row: matrix[12..14]
        prop.position[0] = prop.matrix[12];
        prop.position[1] = prop.matrix[13];
        prop.position[2] = prop.matrix[14];

        // Null-terminated ASCII name at +0x60
        memset(prop.name, 0, sizeof(prop.name));
        size_t nameOff = entry + 0x60;
        for (int i = 0; i < 63 && nameOff + i < data.size(); i++) {
            uint8_t c = data[nameOff + i];
            if (c == 0) break;
            prop.name[i] = (char)c;
        }

        // Apply prop position override from JSON (if available)
        auto ovIt = propOverrides.find(std::string(prop.name));
        if (ovIt != propOverrides.end()) {
            prop.matrix[12] = ovIt->second.pos[0];
            prop.matrix[13] = ovIt->second.pos[1];
            prop.matrix[14] = ovIt->second.pos[2];
            prop.position[0] = ovIt->second.pos[0];
            prop.position[1] = ovIt->second.pos[1];
            prop.position[2] = ovIt->second.pos[2];
        }

        // Prop definition pointer at +0x8C
        uint32_t defPtr = ReadU32(entry + 0x8C);
        if (defPtr == 0 || defPtr + 12 > data.size()) continue;

        uint32_t pcrdCount   = ReadU32(defPtr + 4);
        uint32_t meshListOff = ReadU32(defPtr + 8);
        if (pcrdCount == 0 || pcrdCount > 10000 || meshListOff == 0) continue;

        prop.type = (defPtr + 16 <= data.size()) ? (int)ReadU32(defPtr + 12) : 0;

        // Walk the mesh chain: mesh_list[i] → block → submesh_list[j] → batch → PCRD
        for (uint32_t mi = 0; mi < pcrdCount; mi++) {
            uint32_t blockPtrOff = meshListOff + mi * 4;
            if (blockPtrOff + 4 > data.size()) break;

            uint32_t blockOff = ReadU32(blockPtrOff);
            if (blockOff + 20 > data.size()) continue;

            uint32_t subCount      = ReadU32(blockOff);
            uint32_t submeshListPtr = ReadU32(blockOff + 16);
            if (subCount == 0 || subCount > 100 || submeshListPtr == 0) continue;

            for (uint32_t si = 0; si < subCount; si++) {
                uint32_t smEntry = submeshListPtr + si * 4;
                if (smEntry + 4 > data.size()) break;

                uint32_t submeshPtr = ReadU32(smEntry);
                if (submeshPtr + 16 > data.size()) continue;

                uint32_t batchPtr = ReadU32(submeshPtr + 12);
                if (batchPtr + 48 > data.size()) continue;

                uint32_t pcrdOff = ReadU32(batchPtr + 44);
                if (pcrdOff + 4 <= data.size() &&
                    memcmp(&data[pcrdOff], "PCRD", 4) == 0) {
                    prop.pcrdOffsets.push_back(pcrdOff);
                }
            }
        }

        props.push_back(prop);
    }
}

// ============================================================================
// LoadPropPositions — load prop position overrides from JSON
// ============================================================================
//
// Searches for prop_positions_<level>.json and prop_positions.json next to the exe.
// JSON format (from SpiderMod DUMP PROP POSITIONS button):
//   {"level":"GroundsD","props":[
//     {"name":"Prop_Garden04","pos":[8.64,2.01,24.01],"rot":[0,0,0],"source":"vm"},
//     ...]}
//
// When a PCWB prop name matches a JSON entry, its transform translation is overridden.

void PCWBFile::LoadPropPositions(const char* levelName) {
    propOverrides.clear();
    if (!levelName) return;

    // Try level-specific file first, then generic
    char path1[256], path2[256];
    snprintf(path1, sizeof(path1), "prop_positions_%s.json", levelName);
    snprintf(path2, sizeof(path2), "data/prop_positions_%s.json", levelName);
    const char* paths[] = {path1, path2, "prop_positions.json", "data/prop_positions.json"};

    for (auto* path : paths) {
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz <= 0 || sz > 1000000) { fclose(f); continue; }
        std::string json(sz, 0);
        fread(&json[0], 1, sz, f); fclose(f);

        // Simple JSON parser: find each {"name":"...", "pos":[x,y,z], "rot":[x,y,z]}
        size_t pos = 0;
        int count = 0;
        while ((pos = json.find("\"name\"", pos)) != std::string::npos) {
            // Extract name value
            size_t ns = json.find('"', pos + 7);
            if (ns == std::string::npos) break;
            size_t ne = json.find('"', ns + 1);
            if (ne == std::string::npos) break;
            std::string name = json.substr(ns + 1, ne - ns - 1);

            // Extract pos array
            size_t pp = json.find("\"pos\"", ne);
            if (pp == std::string::npos || pp > ne + 200) { pos = ne; continue; }
            size_t pb = json.find('[', pp);
            if (pb == std::string::npos) { pos = ne; continue; }

            PropOverride ov = {};
            char* end = nullptr;
            ov.pos[0] = strtof(json.c_str() + pb + 1, &end);
            if (end) ov.pos[1] = strtof(end + 1, &end);
            if (end) ov.pos[2] = strtof(end + 1, &end);

            // Extract rot array (optional)
            size_t rp = json.find("\"rot\"", pp);
            if (rp != std::string::npos && rp < pp + 200) {
                size_t rb = json.find('[', rp);
                if (rb != std::string::npos) {
                    ov.rot[0] = strtof(json.c_str() + rb + 1, &end);
                    if (end) ov.rot[1] = strtof(end + 1, &end);
                    if (end) ov.rot[2] = strtof(end + 1, &end);
                }
            }

            propOverrides[name] = ov;
            count++;
            pos = ne + 1;
        }
        if (count > 0)
            printf("[Props] Loaded %d prop overrides from %s\n", count, path);
        break; // use first file found
    }
}

// ============================================================================
// ParseInstanceTransforms — extract world matrices from streaming batch entries
// ============================================================================
//
// The PCWB streaming section contains batch entries for each PCRD. Each batch
// entry includes a 4x4 world transform matrix (row-major, D3D9 convention)
// and a 3×FFFFFFFF sentinel pattern with a PCRD offset reference at sentinel+0x24.
//
// Multiple batch entries (sentinels) share the same preceding matrix. This
// function scans the streaming data for all matrices and sentinels, sorts them
// by file offset, and assigns each sentinel's PCRD to the most recent matrix.
//
// Matrix identification: w=1.0 (0x3F800000) at offset+60, rotation values
// in range [-1.1, 1.1] on the diagonal, valid position in [-2000, 2000].

void PCWBFile::ParseInstanceTransforms() {
    pcrdWorldMatrix.clear();

    if (data.size() < 0x18) return;
    uint32_t geomDataOff = ReadU32(0x14);
    if (geomDataOff == 0 || geomDataOff >= data.size()) return;


    // Step 1: Find all 3×FFFFFFFF sentinels with valid PCRD reference at +0x24
    struct SentinelEntry {
        uint32_t sentinelOff;
        uint32_t pcrdOff;
    };
    std::vector<SentinelEntry> sentinels;
    sentinels.reserve(4096);

    for (uint32_t pos = geomDataOff; pos + 12 < (uint32_t)data.size(); pos += 4) {
        if (ReadU32(pos) == 0xFFFFFFFF && ReadU32(pos + 4) == 0xFFFFFFFF &&
            ReadU32(pos + 8) == 0xFFFFFFFF) {
            uint32_t refOff = pos + 0x24;
            if (refOff + 4 > data.size()) continue;
            uint32_t pcrdOff = ReadU32(refOff);
            if (pcrdOff >= data.size() || pcrdOff + 8 > data.size()) continue;
            if (memcmp(&data[pcrdOff], "PCRD", 4) != 0) continue;
            sentinels.push_back({pos, pcrdOff});
        }
    }

    // Step 2: For each sentinel, scan BACKWARDS (up to previous sentinel or 0x200)
    // looking for a valid 4x4 matrix. This avoids false positives from vertex data.
    // A valid matrix has: w=1.0 at +60, non-zero 3x3, w-column=0, reasonable row mags.
    auto isValidMatrix = [&](uint32_t pos) -> bool {
        if (pos + 64 > data.size()) return false;
        if (ReadU32(pos + 60) != 0x3F800000) return false; // w != 1.0

        float tx = ReadF32(pos + 48), ty = ReadF32(pos + 52), tz = ReadF32(pos + 56);
        if (std::abs(tx) > 2000.0f || std::abs(ty) > 2000.0f || std::abs(tz) > 2000.0f) return false;

        // Column 3 of rows 0-2 must be zero
        if (std::abs(ReadF32(pos + 12)) > 0.01f) return false;
        if (std::abs(ReadF32(pos + 28)) > 0.01f) return false;
        if (std::abs(ReadF32(pos + 44)) > 0.01f) return false;

        // Check 3x3 block: reject all-zero, check row magnitudes.
        // Engine uses large scale factors (up to ~30x for fence pieces etc.),
        // so allow squared row magnitude up to 10000 (scale ~100x).
        bool allZero = true;
        for (int i = 0; i < 3; i++) {
            float r0 = ReadF32(pos + i*16), r1 = ReadF32(pos + i*16 + 4), r2 = ReadF32(pos + i*16 + 8);
            float mag = r0*r0 + r1*r1 + r2*r2;
            if (mag > 1e-6f) allZero = false;
            if (mag > 10000.0f) return false; // scale > 100x is unreasonable
        }
        return !allZero;
    };

    // Step 3: Walk sentinels in order. Group consecutive sentinels that are
    // close together (< 0x100 apart) — these share one matrix from before
    // the first sentinel in the group. Reset between groups so matrices
    // from instanced objects don't bleed into terrain.
    float currentMatrix[16] = {};
    bool hasMatrix = false;

    for (size_t si = 0; si < sentinels.size(); si++) {
        uint32_t sent = sentinels[si].sentinelOff;

        // Detect group boundary: large gap from previous sentinel = new group
        if (si > 0 && sent - sentinels[si-1].sentinelOff > 0x100) {
            hasMatrix = false; // reset — don't propagate across groups
        }

        // Scan backwards from sentinel for a matrix (limited range)
        uint32_t searchStart = (si > 0) ? sentinels[si-1].sentinelOff + 0x30 : geomDataOff;
        if (sent > searchStart + 0x200) searchStart = sent - 0x200;
        if (searchStart < geomDataOff) searchStart = geomDataOff;

        for (uint32_t pos = sent - 4; pos >= searchStart && pos < sent; pos -= 4) {
            if (isValidMatrix(pos)) {
                for (int i = 0; i < 16; i++)
                    currentMatrix[i] = ReadF32(pos + i * 4);
                hasMatrix = true;
                break;
            }
        }

        if (hasMatrix) {
            pcrdWorldMatrix[sentinels[si].pcrdOff].assign(currentMatrix, currentMatrix + 16);
        }
    }
}

// ============================================================================
// ParsePCRD — extract mesh data from a PCRD chunk
// ============================================================================
//
// Vertex format (stride determined by hs field):
//   Stride 24 (hs > 0x10): position(3×float) + color(4×u8) + uv(2×float)
//   Stride 32 (hs ≤ 0x10): same + 8 bytes extra (normals/tangent)
//
// Index format: uint16 triangle strip with degenerate separators.
// Conversion to triangle list skips degenerate triangles and alternates
// winding order for odd-indexed triangles (standard strip convention).
//
// Validation: rejects PCRDs with out-of-bounds offsets, NaN/Inf positions,
// positions > 2000 units from origin, invalid UVs, or out-of-range indices.

ParsedMesh PCWBFile::ParsePCRD(uint32_t pcrdOff, const float* wm) const {
    ParsedMesh mesh;

    if (pcrdOff + 0x1C > data.size()) return mesh;
    if (memcmp(&data[pcrdOff], "PCRD", 4) != 0) return mesh;

    uint32_t hs     = ReadU32(pcrdOff + 0x08);
    uint32_t ic     = ReadU32(pcrdOff + 0x0C);
    uint32_t vc     = ReadU32(pcrdOff + 0x10);
    uint32_t idxOff = ReadU32(pcrdOff + 0x14);
    uint32_t vtxOff = ReadU32(pcrdOff + 0x18);

    // Stride detection: hs often equals the stride directly.
    // Known values: 0x18(24), 0x1C(28), 0x10(→32), 0x0C(→24 or 32).
    // Strategy: try hs as stride first, then 24, then 32. Pick whichever
    // produces valid vertex data at the last position.
    auto isValidVtx = [&](size_t off) -> bool {
        if (off + 24 > data.size()) return false;
        float x = ReadF32(off), y = ReadF32(off + 4), z = ReadF32(off + 8);
        return !std::isnan(x) && !std::isinf(x) && std::abs(x) < 2000.0f &&
               !std::isnan(y) && !std::isinf(y) && std::abs(y) < 2000.0f &&
               !std::isnan(z) && !std::isinf(z) && std::abs(z) < 2000.0f;
    };
    uint32_t stride = 0;
    for (uint32_t tryStride : {hs, 24u, 32u, 28u}) {
        if (tryStride < 24 || tryStride > 64) continue;
        if (vtxOff + (size_t)(vc - 1) * tryStride + 24 > data.size()) continue;
        if (isValidVtx(vtxOff) && isValidVtx(vtxOff + (size_t)(vc - 1) * tryStride)) {
            stride = tryStride;
            break;
        }
    }
    if (stride == 0) return mesh; // no valid stride found

    // Basic bounds validation
    if (vc == 0 || ic == 0) return mesh;
    if (vc > 65535 || ic > 200000) return mesh;
    if (vtxOff > data.size() || idxOff > data.size()) return mesh;
    if (vtxOff + (size_t)vc * stride > data.size()) return mesh;
    if (idxOff + (size_t)ic * 2 > data.size()) return mesh;

    // Position sanity — game worlds fit within ~1000 units of origin
    const float POS_LIMIT = 2000.0f;

    // Validate first vertex position + UV
    float px = ReadF32(vtxOff), py = ReadF32(vtxOff + 4), pz = ReadF32(vtxOff + 8);
    if (std::isnan(px) || std::isinf(px) || std::abs(px) > POS_LIMIT) return mesh;
    if (std::isnan(py) || std::isinf(py) || std::abs(py) > POS_LIMIT) return mesh;
    if (std::isnan(pz) || std::isinf(pz) || std::abs(pz) > POS_LIMIT) return mesh;

    // UV validation relaxed: bad UVs are clamped per-vertex below
    // instead of rejecting the entire mesh (prevents holes in the world).

    // Validate last vertex (catches wrong stride)
    {
        size_t lastOff = vtxOff + (size_t)(vc - 1) * stride;
        float lx = ReadF32(lastOff), ly = ReadF32(lastOff + 4), lz = ReadF32(lastOff + 8);
        if (std::isnan(lx) || std::isinf(lx) || std::abs(lx) > POS_LIMIT) return mesh;
        if (std::isnan(ly) || std::isinf(ly) || std::abs(ly) > POS_LIMIT) return mesh;
        if (std::isnan(lz) || std::isinf(lz) || std::abs(lz) > POS_LIMIT) return mesh;
    }

    // Validate all indices are within vertex count
    for (uint32_t i = 0; i < ic; i++) {
        uint16_t idx;
        memcpy(&idx, &data[idxOff + i * 2], 2);
        if (idx >= vc) return mesh;
    }

    // --- Extract vertices ---
    mesh.positions.reserve(vc * 3);
    mesh.texcoords.reserve(vc * 2);
    mesh.colors.reserve(vc * 4);
    float ySum = 0.0f;

    for (uint32_t i = 0; i < vc; i++) {
        size_t off = vtxOff + (size_t)i * stride;

        float x = ReadF32(off);
        float y = ReadF32(off + 4);
        float z = ReadF32(off + 8);

        if (std::isnan(x) || std::isinf(x) || std::abs(x) > POS_LIMIT) return ParsedMesh();
        if (std::isnan(y) || std::isinf(y) || std::abs(y) > POS_LIMIT) return ParsedMesh();
        if (std::isnan(z) || std::isinf(z) || std::abs(z) > POS_LIMIT) return ParsedMesh();

        uint8_t r = data[off + 12];
        uint8_t g = data[off + 13];
        uint8_t b = data[off + 14];
        uint8_t a = data[off + 15];

        float u, v;
        memcpy(&u, &data[off + 16], 4);
        memcpy(&v, &data[off + 20], 4);

        // Optional world matrix (D3D9 row-vector: world = [x y z 1] * M)
        if (wm) {
            float wx = x * wm[0] + y * wm[4] + z * wm[8]  + wm[12];
            float wy = x * wm[1] + y * wm[5] + z * wm[9]  + wm[13];
            float wz = x * wm[2] + y * wm[6] + z * wm[10] + wm[14];
            x = wx; y = wy; z = wz;
        }

        ySum += y;

        // Store in engine coordinates (Z-up). Caller converts to display space.
        mesh.positions.push_back(x);
        mesh.positions.push_back(y);
        mesh.positions.push_back(z);

        // Clamp garbage UVs (some PCRDs have valid geometry but corrupt UVs)
        if (std::isnan(u) || std::isinf(u) || std::abs(u) > 100.0f) u = 0.0f;
        if (std::isnan(v) || std::isinf(v) || std::abs(v) > 100.0f) v = 0.0f;

        // Engine UVs used as-is — no V flip needed (DDS data is top-down,
        // matching the engine's UV convention where V=0 is top)
        mesh.texcoords.push_back(u);
        mesh.texcoords.push_back(v);

        mesh.colors.push_back(r);
        mesh.colors.push_back(g);
        mesh.colors.push_back(b);
        mesh.colors.push_back(a);
    }

    // --- Convert triangle strip → triangle list ---
    std::vector<uint16_t> raw(ic);
    for (uint32_t i = 0; i < ic; i++)
        memcpy(&raw[i], &data[idxOff + i * 2], 2);

    mesh.indices.reserve(ic); // approximate
    for (uint32_t i = 0; i + 2 < ic; i++) {
        uint16_t i0 = raw[i], i1 = raw[i + 1], i2 = raw[i + 2];
        if (i0 == i1 || i1 == i2 || i0 == i2) continue; // skip degenerate
        if (i % 2 == 0) {
            mesh.indices.push_back(i0);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
        } else {
            // Odd triangles: swap first two to maintain consistent winding
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i0);
            mesh.indices.push_back(i2);
        }
    }

    if (mesh.indices.empty()) return ParsedMesh();

    mesh.vertexCount   = (int)vc;
    mesh.triangleCount = (int)(mesh.indices.size() / 3);
    mesh.centerY       = ySum / (float)vc;

    return mesh;
}

// ============================================================================
// ExtractDDS — get raw DDS texture data from a PCIM chunk
// ============================================================================

bool PCWBFile::ExtractDDS(uint32_t pcimOff,
                          const uint8_t** outData, uint32_t* outSize,
                          uint32_t* outW, uint32_t* outH) const
{
    if (pcimOff + 0xC1 > data.size()) return false;
    if (memcmp(&data[pcimOff], "PCIM", 4) != 0) return false;

    uint32_t dsz  = ReadU32(pcimOff + 0x0C);
    uint32_t doff = ReadU32(pcimOff + 0x10);
    *outW = ReadU32(pcimOff + 0x9C);
    *outH = ReadU32(pcimOff + 0xA0);

    // Try absolute offset first
    if (doff + dsz <= data.size() && memcmp(&data[doff], "DDS ", 4) == 0) {
        *outData = &data[doff];
        *outSize = dsz;
        return true;
    }

    // Fallback: DDS immediately after 0xC1-byte PCIM header
    uint32_t rel = pcimOff + 0xC1;
    if (rel + dsz <= data.size() && memcmp(&data[rel], "DDS ", 4) == 0) {
        *outData = &data[rel];
        *outSize = dsz;
        return true;
    }

    return false;
}

// ============================================================================
// GetHeaderInfo — extract lighting/fog/environment from PCWB header
// ============================================================================

PCWBHeaderInfo PCWBFile::GetHeaderInfo() const {
    PCWBHeaderInfo info;
    if (data.size() < 0xF0) return info;

    info.fileSize      = ReadU32(0x0C);
    info.sectorCount   = ReadU32(0x30);
    info.drawDataCount = ReadU32(0x4C);
    info.propCount     = ReadU32(0x50);

    // Light direction at +0xB8 (decimal 184/188/192)
    info.lightDir[0] = ReadF32(0xB8);
    info.lightDir[1] = ReadF32(0xBC);
    info.lightDir[2] = ReadF32(0xC0);

    // Fog params at +0xC4 (decimal 196/200/204)
    info.fogParams[0] = ReadF32(0xC4);
    info.fogParams[1] = ReadF32(0xC8);
    info.fogParams[2] = ReadF32(0xCC);

    // Dump a wider range for exploration
    for (int i = 0; i < 16; i++)
        info.headerFloats[i] = ReadF32(0xB0 + i * 4);

    info.valid = true;
    return info;
}

// ============================================================================
// GetPCRDTexture — lookup texture index for a PCRD
// ============================================================================

int PCWBFile::GetPCRDTexture(uint32_t pcrdOffset) const {
    auto it = pcrdTexMap.find(pcrdOffset);
    return (it != pcrdTexMap.end()) ? it->second : -1;
}

// ============================================================================
// ParseNM40Mesh — extract renderable mesh from NM40 data with embedded PCRD
// ============================================================================
//
// NM40 vertex stride = 52 bytes (probed from runtime data):
//   +0:  position  (3×float, 12 bytes)
//   +12: normal    (3×float, 12 bytes)
//   +24: tangentW  (1×float, 4 bytes)  — NOT texcoord!
//   +28: texcoord  (2×float, 8 bytes)  — UV offset found by probing vertex data
//   +36: blendWt   (4×float, 16 bytes)
//
// The NM40 data (from AWAD TOC) includes the header, embedded PCRD, and
// vertex/index buffers — all in one contiguous block.
//
// Embedded PCRD header (at ~+0xC0-0x158):
//   "PCRD"(4) + ver(4) + stride(4) + idxCount(4) + vtxCount(4) + idxOff(4) + vtxOff(4)
//   idxOff and vtxOff are relative to the NM40 start.

static float ReadF32At(const uint8_t* d, uint32_t off) {
    float f; memcpy(&f, d + off, 4); return f;
}
static uint32_t ReadU32At(const uint8_t* d, uint32_t off) {
    return d[off] | (d[off+1]<<8) | (d[off+2]<<16) | (d[off+3]<<24);
}

static char s_nm40Error[256] = {0};
const char* GetNM40ParseError() { return s_nm40Error; }

ParsedMesh ParseNM40Mesh(const uint8_t* nm40, uint32_t nm40Size) {
    ParsedMesh mesh;
    s_nm40Error[0] = 0;

    if (nm40Size < 0x40 || memcmp(nm40, "NM40", 4) != 0) {
        snprintf(s_nm40Error, sizeof(s_nm40Error), "Not NM40 data (size=%u)", nm40Size);
        return mesh;
    }

    // --- Step 1: Find LOD0 PCRD via mesh table chain (engine replica) ---
    // Engine fixup path: NM40+0x34 → meshTable → subBatch[0] → renderData → PCRD
    // This finds the EXACT PCRD the engine renders, not a random PCRD magic match.
    uint32_t pcrdOff = 0;
    uint32_t meshTblOff = ReadU32At(nm40, 0x34);
    uint16_t numMeshTblEntries = nm40[0x24] | (nm40[0x25] << 8);

    if (meshTblOff > 0 && meshTblOff + 8 <= nm40Size && numMeshTblEntries > 0) {
        // Mesh table entry: [+0 uint16 unk, +2 uint16 subBatchCount, +4 uint32 subBatchArrayPtr]
        uint32_t subBatchArrOff = ReadU32At(nm40, meshTblOff + 4);
        if (subBatchArrOff > 0 && subBatchArrOff + 16 <= nm40Size) {
            // Sub-batch entry: [+0..+3 unk, +4 uint16 boneCount, +6 unk, +8 bonePalette, +12 renderDataPtr]
            uint32_t renderDataOff = ReadU32At(nm40, subBatchArrOff + 12);
            if (renderDataOff > 0 && renderDataOff + 28 <= nm40Size) {
                // Verify PCRD magic at renderData offset
                if (memcmp(nm40 + renderDataOff, "PCRD", 4) == 0) {
                    pcrdOff = renderDataOff;
                    printf("[NM40] PCRD via mesh table chain: meshTbl=+0x%X → subBatch=+0x%X → PCRD=+0x%X\n",
                           meshTblOff, subBatchArrOff, pcrdOff);
                }
            }
        }
    }

    // Fallback: scan for PCRD magic (if mesh table traversal failed)
    if (pcrdOff == 0) {
        for (uint32_t pos = 0x40; pos + 28 < nm40Size && pos < 0x10000; pos += 4) {
            if (memcmp(nm40 + pos, "PCRD", 4) == 0) {
                pcrdOff = pos;
                printf("[NM40] PCRD via magic scan: +0x%X (fallback)\n", pcrdOff);
                break;
            }
        }
    }

    if (pcrdOff == 0) {
        snprintf(s_nm40Error, sizeof(s_nm40Error), "No PCRD found (meshTbl=0x%X, scan=%u bytes)",
                 meshTblOff, std::min(nm40Size, 0x10000u));
        return mesh;
    }

    // --- Step 2: Collect ALL sub-batch PCRDs across ALL mesh table entries ---
    // Each sub-batch has its own PCRD with separate vertex/index data (body parts).
    // Engine renders ALL of them per frame. We merge into one ParsedMesh.
    struct PCRDBatch { uint32_t off, stride, vc, ic, vtxOff, idxOff; };
    std::vector<PCRDBatch> batches;

    for (uint16_t ei = 0; ei < numMeshTblEntries; ei++) {
        uint32_t entOff = meshTblOff + ei * 8;
        if (entOff + 8 > nm40Size) break;
        uint16_t subCount = nm40[entOff+2] | (nm40[entOff+3] << 8);
        uint32_t subArr = ReadU32At(nm40, entOff + 4);
        if (subArr == 0 || subArr + subCount * 16 > nm40Size) continue;
        for (uint16_t si = 0; si < subCount; si++) {
            uint32_t rd = ReadU32At(nm40, subArr + si * 16 + 12);
            if (rd == 0 || rd + 28 > nm40Size || memcmp(nm40+rd,"PCRD",4) != 0) continue;
            PCRDBatch b;
            b.off    = rd;
            b.stride = ReadU32At(nm40, rd+8);
            b.ic     = ReadU32At(nm40, rd+12);
            b.vc     = ReadU32At(nm40, rd+16);
            b.idxOff = ReadU32At(nm40, rd+20);
            b.vtxOff = ReadU32At(nm40, rd+24);
            if (b.vc==0||b.ic<3||b.vc>100000||b.ic>600000) continue;
            if (b.stride<24||b.stride>128) continue;
            if (b.vtxOff+(size_t)b.vc*b.stride>nm40Size) continue;
            if (b.idxOff+(size_t)b.ic*2>nm40Size) continue;
            bool dup = false;
            for (auto& p : batches) if (p.off==rd) { dup=true; break; }
            if (!dup) batches.push_back(b);
        }
    }
    // Fallback: single PCRD from scan
    if (batches.empty() && pcrdOff > 0) {
        PCRDBatch b;
        b.off=pcrdOff; b.stride=ReadU32At(nm40,pcrdOff+8);
        b.ic=ReadU32At(nm40,pcrdOff+12); b.vc=ReadU32At(nm40,pcrdOff+16);
        b.idxOff=ReadU32At(nm40,pcrdOff+20); b.vtxOff=ReadU32At(nm40,pcrdOff+24);
        if (b.vc>0&&b.ic>=3) batches.push_back(b);
    }
    if (batches.empty()) {
        snprintf(s_nm40Error, sizeof(s_nm40Error), "No valid PCRD batches");
        return mesh;
    }

    uint32_t totalVc=0, totalIc=0;
    for (auto& b : batches) { totalVc+=b.vc; totalIc+=b.ic; }
    printf("[NM40] Merging %d batches: %u verts, %u indices\n",
           (int)batches.size(), totalVc, totalIc);

    // --- Step 2b: Parse D3D vertex declaration to find element offsets ---
    // Vertex declaration at NM40+0x40: array of D3DVERTEXELEMENT9 (8 bytes each)
    // {Stream(u16), Offset(u16), Type(u8), Method(u8), Usage(u8), UsageIndex(u8)}
    // Usage: 0=POSITION, 3=NORMAL, 5=TEXCOORD, 12=BLENDWEIGHT, 13=BLENDINDICES
    uint32_t posOff = 0, uvOff_decl = 0, normOff = 0;
    uint8_t normType = 0; // 0=unknown, 2=FLOAT3, 6=SHORT4N, etc.
    bool foundUV = false, foundNorm = false;
    {
        uint32_t declStart = 0x40;
        for (int di = 0; di < 20 && declStart + di*8 + 8 <= nm40Size; di++) {
            uint32_t eoff = declStart + di * 8;
            uint16_t stream = nm40[eoff] | (nm40[eoff+1]<<8);
            uint16_t offset = nm40[eoff+2] | (nm40[eoff+3]<<8);
            uint8_t  type   = nm40[eoff+4];
            uint8_t  usage  = nm40[eoff+6];
            if (stream == 0xFF && offset == 0xFF) break; // D3DDECL_END
            if (usage == 0 && !posOff) posOff = offset;  // POSITION
            if (usage == 5 && !foundUV) { uvOff_decl = offset; foundUV = true; }  // TEXCOORD
            if (usage == 3 && !foundNorm) { normOff = offset; normType = type; foundNorm = true; }  // NORMAL
        }
    }
    if (!foundUV) {
        // Engine vertex layout (stride=52): pos(3f)+normal(3f)+uv(2f)+blendIdx(4B)+blendWt(4f)
        // UVs at +24 as FLOAT2, confirmed by spiderwick_unpack.py OBJ export
        uvOff_decl = 24;
        printf("[NM40] Using UV offset +24 (engine standard)\n");
    } else {
        printf("[NM40] Vertex decl: pos=+%u, norm=+%u(type=%u), uv=+%u\n",
               posOff, normOff, normType, uvOff_decl);
    }

    // --- Step 3: Extract and merge vertex/index data from all batches ---
    mesh.positions.reserve(totalVc*3);
    mesh.normals.reserve(totalVc*3);
    mesh.texcoords.reserve(totalVc*2);
    mesh.colors.reserve(totalVc*4);
    mesh.indices.reserve(totalIc);
    float ySum = 0.0f;
    uint32_t vOff = 0; // cumulative vertex offset for index rebasing

    for (auto& b : batches) {
        uint32_t uvOff = uvOff_decl;
        bool hasN = foundNorm;
        for (uint32_t i = 0; i < b.vc; i++) {
            uint32_t o = b.vtxOff + i * b.stride;
            float x=ReadF32At(nm40,o), y=ReadF32At(nm40,o+4), z=ReadF32At(nm40,o+8);
            ySum += y;
            mesh.positions.push_back(x); mesh.positions.push_back(y); mesh.positions.push_back(z);
            float u=ReadF32At(nm40,o+uvOff), v=ReadF32At(nm40,o+uvOff+4);
            if (std::isnan(u)||std::isinf(u)||std::abs(u)>100.f) u=0.f;
            if (std::isnan(v)||std::isinf(v)||std::abs(v)>100.f) v=0.f;
            mesh.texcoords.push_back(u); mesh.texcoords.push_back(v);
            if (hasN) {
                // Normals at +12 as FLOAT3 (confirmed by spiderwick_unpack.py)
                float nx=ReadF32At(nm40,o+12), ny=ReadF32At(nm40,o+16), nz=ReadF32At(nm40,o+20);
                mesh.normals.push_back(nx); mesh.normals.push_back(nz); mesh.normals.push_back(-ny);
                float dnx=nx, dny=nz, dnz=-ny;
                mesh.colors.push_back((uint8_t)(fmaxf(0,fminf(1,dnx*.5f+.5f))*255.f));
                mesh.colors.push_back((uint8_t)(fmaxf(0,fminf(1,dny*.5f+.5f))*255.f));
                mesh.colors.push_back((uint8_t)(fmaxf(0,fminf(1,dnz*.5f+.5f))*255.f));
                mesh.colors.push_back(255);
            } else {
                mesh.normals.push_back(0); mesh.normals.push_back(1); mesh.normals.push_back(0);
                mesh.colors.push_back(nm40[o+12]); mesh.colors.push_back(nm40[o+13]);
                mesh.colors.push_back(nm40[o+14]); mesh.colors.push_back(nm40[o+15]);
            }
        }
        // Indices (triangle list, rebased)
        uint32_t ic = (b.ic / 3) * 3;
        for (uint32_t i = 0; i < ic; i++) {
            uint16_t idx; memcpy(&idx, nm40+b.idxOff+i*2, 2);
            if (idx < b.vc) mesh.indices.push_back(idx + vOff);
        }
        vOff += b.vc;
    }

    if (mesh.indices.empty()) {
        snprintf(s_nm40Error, sizeof(s_nm40Error), "No triangles from %d batches", (int)batches.size());
        return ParsedMesh();
    }
    mesh.vertexCount   = (int)totalVc;
    mesh.triangleCount = (int)(mesh.indices.size() / 3);
    mesh.centerY       = ySum / (float)totalVc;
    printf("[NM40] OK: %d verts, %d tris from %d batches\n",
           mesh.vertexCount, mesh.triangleCount, (int)batches.size());
    return mesh;
}

// ============================================================================
// ParseNM40Batches — returns individual meshes per sub-batch for multi-material
// ============================================================================
NM40MeshResult ParseNM40Batches(const uint8_t* nm40, uint32_t nm40Size) {
    NM40MeshResult result;

    // Get the merged mesh first (reuse existing parser)
    result.merged = ParseNM40Mesh(nm40, nm40Size);
    if (!result.merged.Valid()) return result;

    // Re-parse to extract individual batches with bone info
    uint32_t meshTblOff = ReadU32At(nm40, 0x34);
    uint16_t numMeshTblEntries = nm40[0x24] | (nm40[0x25] << 8);
    if (meshTblOff == 0 || numMeshTblEntries == 0) {
        // Single batch fallback
        result.merged.batchIndex = 0;
        result.batches.push_back(result.merged);
        return result;
    }

    // ====================================================================
    // Mesh table entry → material group mapping
    // ====================================================================
    // Engine's submesh descriptor bitmasks (NM40_GetSubmeshDesc) live in a
    // runtime C++ class wrapper, NOT in the raw file data. The subTblOff
    // (+0x3C) points to bone palette entries (startBone, boneCount) × numSub,
    // NOT submesh bitmasks.
    //
    // For texture assignment we use the mesh table entry index directly:
    //   Entry 0 = body geometry    → desc[2] main diffuse (1024x1024)
    //   Entry 1+ = head/face/hair  → desc[4] detail texture (512x512)
    //
    // This matches the engine's MaterialBuilder which creates per-submesh
    // materials, where the first material group covers body bones and
    // subsequent groups cover head/face/hair bones.
    printf("[NM40] meshTblOff=+0x%X, numEntries=%d (entry 0=body, 1+=head)\n",
           meshTblOff, numMeshTblEntries);

    int batchIdx = 0;
    for (uint16_t ei = 0; ei < numMeshTblEntries; ei++) {
        uint32_t entOff = meshTblOff + ei * 8;
        if (entOff + 8 > nm40Size) break;
        uint16_t subCount = nm40[entOff+2] | (nm40[entOff+3] << 8);
        uint32_t subArr = ReadU32At(nm40, entOff + 4);
        if (subArr == 0 || subArr + subCount * 16 > nm40Size) continue;

        // Mesh table entry index determines material group:
        //   Entry 0 = body (desc[2]), Entry 1+ = head/face/hair (desc[4])
        int entrySubmesh = (int)ei;

        for (uint16_t si = 0; si < subCount; si++) {
            uint32_t subOff = subArr + si * 16;
            uint16_t boneCount = nm40[subOff+4] | (nm40[subOff+5] << 8);
            uint32_t rd = ReadU32At(nm40, subOff + 12);
            if (rd == 0 || rd + 28 > nm40Size || memcmp(nm40+rd,"PCRD",4) != 0) continue;

            uint32_t stride = ReadU32At(nm40, rd+8);
            uint32_t ic     = ReadU32At(nm40, rd+12);
            uint32_t vc     = ReadU32At(nm40, rd+16);
            uint32_t idxOff = ReadU32At(nm40, rd+20);
            uint32_t vtxOff = ReadU32At(nm40, rd+24);
            if (vc==0||ic<3||stride<24||stride>128) continue;
            if (vtxOff+(size_t)vc*stride>nm40Size) continue;
            if (idxOff+(size_t)ic*2>nm40Size) continue;

            // Check for duplicate PCRD
            bool dup = false;
            for (auto& b : result.batches)
                if (b.batchIndex >= 0 && b.vertexCount == (int)vc) {
                    // Compare first vertex position
                    if (b.positions.size() >= 3 && vtxOff + 12 <= nm40Size) {
                        float bx = b.positions[0], by = b.positions[1], bz = b.positions[2];
                        float nx = ReadF32At(nm40,vtxOff), ny = ReadF32At(nm40,vtxOff+4), nz = ReadF32At(nm40,vtxOff+8);
                        if (bx==nx && by==ny && bz==nz) { dup=true; break; }
                    }
                }
            if (dup) continue;

            ParsedMesh batch;
            batch.batchIndex = batchIdx;
            batch.boneCount = boneCount;
            batch.submeshIndex = entrySubmesh;

            uint32_t uvOff = 24; // engine standard
            bool hasN = (stride >= 52);

            float ySum = 0;
            for (uint32_t i = 0; i < vc; i++) {
                uint32_t o = vtxOff + i * stride;
                float x=ReadF32At(nm40,o), y=ReadF32At(nm40,o+4), z=ReadF32At(nm40,o+8);
                ySum += y;
                batch.positions.push_back(x); batch.positions.push_back(y); batch.positions.push_back(z);
                float u=ReadF32At(nm40,o+uvOff), v=ReadF32At(nm40,o+uvOff+4);
                if (std::isnan(u)||std::isinf(u)||std::abs(u)>100.f) u=0.f;
                if (std::isnan(v)||std::isinf(v)||std::abs(v)>100.f) v=0.f;
                batch.texcoords.push_back(u); batch.texcoords.push_back(v);
                if (hasN) {
                    float nx=ReadF32At(nm40,o+12), ny=ReadF32At(nm40,o+16), nz=ReadF32At(nm40,o+20);
                    batch.normals.push_back(nx); batch.normals.push_back(nz); batch.normals.push_back(-ny);
                    float dnx=nx, dny=nz, dnz=-ny;
                    batch.colors.push_back((uint8_t)(fmaxf(0,fminf(1,dnx*.5f+.5f))*255.f));
                    batch.colors.push_back((uint8_t)(fmaxf(0,fminf(1,dny*.5f+.5f))*255.f));
                    batch.colors.push_back((uint8_t)(fmaxf(0,fminf(1,dnz*.5f+.5f))*255.f));
                    batch.colors.push_back(255);
                } else {
                    batch.normals.push_back(0); batch.normals.push_back(1); batch.normals.push_back(0);
                    batch.colors.push_back(nm40[o+12]); batch.colors.push_back(nm40[o+13]);
                    batch.colors.push_back(nm40[o+14]); batch.colors.push_back(nm40[o+15]);
                }
            }
            uint32_t tic = (ic / 3) * 3;
            for (uint32_t i = 0; i < tic; i++) {
                uint16_t idx; memcpy(&idx, nm40+idxOff+i*2, 2);
                if (idx < vc) batch.indices.push_back(idx);
            }
            batch.vertexCount = (int)vc;
            batch.triangleCount = (int)(batch.indices.size() / 3);
            batch.centerY = (vc > 0) ? ySum / (float)vc : 0;

            if (batch.Valid()) {
                printf("[NM40] Batch %d: %d verts, %d tris, %d bones, submesh=%d\n",
                       batchIdx, batch.vertexCount, batch.triangleCount, batch.boneCount, entrySubmesh);
                result.batches.push_back(std::move(batch));
                batchIdx++;
            }
        }
    }

    printf("[NM40] ParseNM40Batches: %d individual batches\n", (int)result.batches.size());

    // ====================================================================
    // Extract bone positions from vertex blend data
    // ====================================================================
    // For each bone index, compute the weighted centroid of all vertices
    // that reference it. This gives approximate joint positions for
    // armature visualization without needing the bind pose matrices.
    //
    // Vertex format (stride=52):
    //   +32: BLENDIDX (4×uint8) — bone palette indices
    //   +36: BLENDWT  (4×float) — blend weights (sum ≈ 1.0)
    uint16_t numBones = nm40[0x08] | (nm40[0x09] << 8);
    result.numBones = numBones;
    if (numBones > 0 && numBones <= 256) {
        result.bones.resize(numBones);

        // Accumulate weighted positions per bone from ALL batches
        std::vector<float> boneWeightSum(numBones, 0.0f);

        for (auto& batch : result.batches) {
            if (!batch.Valid()) continue;
            // Re-read vertex data with blend info
            // Find this batch's PCRD to get vtxOff and stride
            for (uint16_t ei = 0; ei < numMeshTblEntries; ei++) {
                uint32_t entOff = meshTblOff + ei * 8;
                if (entOff + 8 > nm40Size) continue;
                uint16_t subCount = nm40[entOff+2] | (nm40[entOff+3] << 8);
                uint32_t subArr = ReadU32At(nm40, entOff + 4);
                if (subArr == 0 || subArr + subCount * 16 > nm40Size) continue;

                for (uint16_t si = 0; si < subCount; si++) {
                    uint32_t rd = ReadU32At(nm40, subArr + si * 16 + 12);
                    if (rd == 0 || rd + 28 > nm40Size || memcmp(nm40+rd,"PCRD",4) != 0) continue;

                    uint32_t stride = ReadU32At(nm40, rd+8);
                    uint32_t vc     = ReadU32At(nm40, rd+16);
                    uint32_t vtxOff = ReadU32At(nm40, rd+24);
                    if (stride < 52 || vc != (uint32_t)batch.vertexCount) continue;
                    if (vtxOff + (size_t)vc * stride > nm40Size) continue;

                    // Read bone palette for this sub-batch (maps local → global bone indices)
                    uint16_t palCount = nm40[subArr + si*16 + 4] | (nm40[subArr + si*16 + 5] << 8);
                    uint32_t palOff = ReadU32At(nm40, subArr + si*16 + 8);
                    bool hasPalette = (palOff > 0 && palOff + palCount <= nm40Size);

                    // Populate blend data in the batch's ParsedMesh
                    batch.blendIndices.resize(vc * 4, 0);
                    batch.blendWeights.resize(vc * 4, 0.0f);

                    for (uint32_t vi = 0; vi < vc; vi++) {
                        uint32_t voff = vtxOff + vi * stride;
                        float px = ReadF32At(nm40, voff);
                        float py = ReadF32At(nm40, voff+4);
                        float pz = ReadF32At(nm40, voff+8);

                        // Blend indices at +32, weights at +36
                        for (int bi = 0; bi < 4; bi++) {
                            uint8_t localIdx = nm40[voff + 32 + bi];
                            float weight = ReadF32At(nm40, voff + 36 + bi * 4);

                            // Map local bone index to global via palette
                            int globalIdx = localIdx;
                            if (hasPalette && localIdx < palCount)
                                globalIdx = nm40[palOff + localIdx];

                            // Store in ParsedMesh blend data
                            batch.blendIndices[vi * 4 + bi] = (uint8_t)globalIdx;
                            batch.blendWeights[vi * 4 + bi] = weight;

                            if (weight < 0.01f) continue;

                            if (globalIdx >= 0 && globalIdx < numBones) {
                                result.bones[globalIdx].position[0] += px * weight;
                                result.bones[globalIdx].position[1] += py * weight;
                                result.bones[globalIdx].position[2] += pz * weight;
                                boneWeightSum[globalIdx] += weight;
                                result.bones[globalIdx].vertexCount++;
                            }
                        }
                    }
                    goto next_batch; // found matching PCRD for this batch
                }
            }
            next_batch:;
        }

        // Normalize to get centroids
        int activeBones = 0;
        for (int bi = 0; bi < numBones; bi++) {
            if (boneWeightSum[bi] > 0.0f) {
                result.bones[bi].position[0] /= boneWeightSum[bi];
                result.bones[bi].position[1] /= boneWeightSum[bi];
                result.bones[bi].position[2] /= boneWeightSum[bi];
                activeBones++;
            }
        }

        // ================================================================
        // Parse bone hierarchy from NM40 bonePalette structure
        // ================================================================
        // The bonePalette at bonePalOff (+0x38) contains sub-offsets to:
        //   +0x0C: bone transforms (192 bytes/bone = 3 matrices, relative to bonePal)
        //   +0x10: bone remap table (relative to bonePal)
        //   +0x14: mesh header extension (relative to bonePal)
        // Reversed from Kallis runtime accessor functions (sub_53A650, sub_53A660,
        // NM40_GetMeshHeader) and NM40_PointerFixup (0x20BC000).
        uint32_t bonePalOff = ReadU32At(nm40, 0x38);
        bool foundParents = false;
        if (bonePalOff > 0 && bonePalOff + 0x18 <= nm40Size) {
            // bonePal+8/+12/+16 are NM40-relative offsets (fixup code adds NM40 base)
            uint32_t boneTransOff  = ReadU32At(nm40, bonePalOff + 8);
            uint32_t boneRemapOff  = ReadU32At(nm40, bonePalOff + 12);
            uint32_t meshHdrExtOff = ReadU32At(nm40, bonePalOff + 16);

            printf("[NM40] BonePal derived offsets (NM40-relative):\n");
            printf("[NM40]   boneTransforms: +0x%X\n", boneTransOff);
            printf("[NM40]   boneRemap:      +0x%X\n", boneRemapOff);
            printf("[NM40]   meshHeaderExt:  +0x%X\n", meshHdrExtOff);

            // Dump first bone transform entry (192 bytes) to determine internal layout
            if (boneTransOff + 192 <= nm40Size) {
                printf("[NM40]   First bone transform (192 bytes):");
                for (int di = 0; di < 192; di++) {
                    if (di % 16 == 0) printf("\n    +%03X:", boneTransOff + di);
                    printf(" %02X", nm40[boneTransOff + di]);
                }
                printf("\n");
                // Interpret as 3 matrices (4x4 float each = 64 bytes)
                for (int mi = 0; mi < 3; mi++) {
                    printf("[NM40]   Matrix %d:\n", mi);
                    for (int r = 0; r < 4; r++) {
                        printf("    ");
                        for (int c = 0; c < 4; c++)
                            printf("%8.3f ", ReadF32At(nm40, boneTransOff + mi*64 + r*16 + c*4));
                        printf("\n");
                    }
                }
            }

            // Parse bone parent indices from boneRemap data
            // Format: stride 4 per bone, (uint16 parentIndex, uint16 flags)
            // parentIndex == boneIndex means root (self-reference)
            if (boneRemapOff + numBones * 4 <= nm40Size) {
                printf("[NM40]   Bone hierarchy (stride=4, uint16 parent + uint16 flags):\n");
                bool valid = true;
                int rootCount = 0;
                for (int bi = 0; bi < numBones; bi++) {
                    uint16_t parent = nm40[boneRemapOff + bi*4] | (nm40[boneRemapOff + bi*4 + 1] << 8);
                    uint16_t flags  = nm40[boneRemapOff + bi*4 + 2] | (nm40[boneRemapOff + bi*4 + 3] << 8);
                    if (parent == bi) {
                        result.bones[bi].parentIndex = -1; // self-ref = root
                        rootCount++;
                    } else if (parent < numBones) {
                        result.bones[bi].parentIndex = (int)parent;
                    } else {
                        valid = false;
                        break;
                    }
                    if (bi < 20)
                        printf("[NM40]     bone[%d] parent=%d flags=0x%04X\n", bi,
                               result.bones[bi].parentIndex, flags);
                }
                if (valid && rootCount >= 1 && rootCount <= 5) {
                    printf("[NM40]   Bone hierarchy VALID: %d roots, %d bones\n", rootCount, numBones);
                    foundParents = true;
                } else {
                    printf("[NM40]   Bone data invalid (roots=%d, valid=%d)\n", rootCount, valid);
                    for (int bi = 0; bi < numBones; bi++)
                        result.bones[bi].parentIndex = -1;
                }
            }

            // Read meshHeaderExt for submesh count
            if (meshHdrExtOff + 72 <= nm40Size) {
                uint32_t smCount = ReadU32At(nm40, meshHdrExtOff + 0x40);
                uint32_t smDescOff = ReadU32At(nm40, meshHdrExtOff + 0x44);
                printf("[NM40]   meshHeaderExt: submeshCount=%d, descTableOff=0x%X\n",
                       smCount, smDescOff);
            }

            // Try reading bone parent indices from the 192-byte bone entries
            // (fallback if hierarchy wasn't found in boneRemap)
            if (boneTransOff + numBones * 192 <= nm40Size) {
                // Try parent as int16 at each even offset within the 192-byte stride
                for (int poff = 0; poff <= 190; poff += 2) {
                    bool valid = true;
                    int rootCount = 0, fwdCount = 0;
                    for (int bi = 0; bi < numBones; bi++) {
                        int16_t p = (int16_t)(nm40[boneTransOff + bi*192 + poff] |
                                             (nm40[boneTransOff + bi*192 + poff + 1] << 8));
                        if (p == -1) rootCount++;
                        else if (p < 0 || p >= numBones) { valid = false; break; }
                        else if (bi > 0 && p < bi) fwdCount++;
                    }
                    if (!valid || rootCount != 1 || fwdCount < numBones / 2) continue;

                    printf("[NM40]   FOUND parent indices in bone transform: stride=192, offset=%d\n", poff);
                    for (int bi = 0; bi < numBones; bi++) {
                        int16_t p = (int16_t)(nm40[boneTransOff + bi*192 + poff] |
                                             (nm40[boneTransOff + bi*192 + poff + 1] << 8));
                        result.bones[bi].parentIndex = p;
                    }
                    foundParents = true;
                    for (int bi = 0; bi < std::min((int)numBones, 15); bi++)
                        printf("[NM40]     bone[%d] parent=%d\n", bi, result.bones[bi].parentIndex);
                    break;
                }
            }

            if (!foundParents) {
                printf("[NM40]   No parent indices found in bone transforms\n");
                for (int bi = 0; bi < numBones; bi++)
                    result.bones[bi].parentIndex = -1;
            }
        } else {
            for (int bi = 0; bi < numBones; bi++)
                result.bones[bi].parentIndex = -1;
        }

        printf("[NM40] Bones: %d total, %d active (with vertices)\n", numBones, activeBones);

        // Extract bone bind-pose matrices (3 × 4x4 per bone, 192 bytes each)
        if (bonePalOff > 0 && bonePalOff + 0x18 <= nm40Size) {
            uint32_t boneTransOff2 = ReadU32At(nm40, bonePalOff + 8);
            if (boneTransOff2 + numBones * 192 <= nm40Size) {
                result.boneLocalMatrices.resize(numBones * 16);
                result.boneWorldMatrices.resize(numBones * 16);
                result.boneInvBindMatrices.resize(numBones * 16);
                for (int bi = 0; bi < numBones; bi++) {
                    uint32_t base = boneTransOff2 + bi * 192;
                    for (int j = 0; j < 16; j++) {
                        result.boneLocalMatrices[bi*16+j]   = ReadF32At(nm40, base + j*4);
                        result.boneWorldMatrices[bi*16+j]    = ReadF32At(nm40, base + 64 + j*4);
                        result.boneInvBindMatrices[bi*16+j]  = ReadF32At(nm40, base + 128 + j*4);
                    }
                }
                printf("[NM40] Extracted %d bone matrices (local+world+invBind)\n", numBones);
            }
        }
    }

    return result;
}

// Parse ALL embedded PCRDs from an NM40 model (multiple LODs/submeshes)
static ParsedMesh ParseNM40SinglePCRD(const uint8_t* nm40, uint32_t nm40Size, uint32_t pcrdOff) {
    ParsedMesh mesh;

    uint32_t stride = ReadU32At(nm40, pcrdOff + 8);
    uint32_t ic     = ReadU32At(nm40, pcrdOff + 12);
    uint32_t vc     = ReadU32At(nm40, pcrdOff + 16);
    uint32_t idxOff = ReadU32At(nm40, pcrdOff + 20);
    uint32_t vtxOff = ReadU32At(nm40, pcrdOff + 24);

    if (vc == 0 || ic < 3 || vc > 100000 || ic > 600000) return mesh;
    if (vtxOff + (size_t)vc * stride > nm40Size) return mesh;
    if (idxOff + (size_t)ic * 2 > nm40Size) return mesh;

    // Validate boundaries
    auto validVtx = [&](uint32_t off) -> bool {
        if (off + 12 > nm40Size) return false;
        float x = ReadF32At(nm40, off), y = ReadF32At(nm40, off+4), z = ReadF32At(nm40, off+8);
        return !std::isnan(x) && !std::isinf(x) && std::abs(x) < 50000.f &&
               !std::isnan(y) && !std::isinf(y) && std::abs(y) < 50000.f &&
               !std::isnan(z) && !std::isinf(z) && std::abs(z) < 50000.f;
    };
    if (!validVtx(vtxOff) || !validVtx(vtxOff + (vc-1) * stride)) return mesh;

    for (uint32_t i = 0; i < ic; i++) {
        uint16_t idx; memcpy(&idx, nm40 + idxOff + i * 2, 2);
        if (idx >= vc) return mesh;
    }

    uint32_t uvOff = (stride >= 52) ? 24 : 16;
    bool hasNormals = (stride >= 52);

    mesh.positions.reserve(vc * 3);
    mesh.normals.reserve(vc * 3);
    mesh.texcoords.reserve(vc * 2);
    mesh.colors.reserve(vc * 4);
    float ySum = 0.0f;

    for (uint32_t i = 0; i < vc; i++) {
        uint32_t off = vtxOff + i * stride;
        float x = ReadF32At(nm40, off), y = ReadF32At(nm40, off+4), z = ReadF32At(nm40, off+8);
        ySum += y;
        mesh.positions.push_back(x); mesh.positions.push_back(y); mesh.positions.push_back(z);

        float u = ReadF32At(nm40, off + uvOff), v = ReadF32At(nm40, off + uvOff + 4);
        if (std::isnan(u)||std::isinf(u)||std::abs(u)>100.f) u = 0.f;
        if (std::isnan(v)||std::isinf(v)||std::abs(v)>100.f) v = 0.f;
        mesh.texcoords.push_back(u); mesh.texcoords.push_back(v);

        if (hasNormals) {
            float nx = ReadF32At(nm40, off + 12), ny = ReadF32At(nm40, off + 16), nz = ReadF32At(nm40, off + 20);
            mesh.normals.push_back(nx); mesh.normals.push_back(nz); mesh.normals.push_back(-ny);
            float dnx = nx, dny = nz, dnz = -ny;
            mesh.colors.push_back((uint8_t)(fmaxf(0,fminf(1,dnx*0.5f+0.5f))*255.f));
            mesh.colors.push_back((uint8_t)(fmaxf(0,fminf(1,dny*0.5f+0.5f))*255.f));
            mesh.colors.push_back((uint8_t)(fmaxf(0,fminf(1,dnz*0.5f+0.5f))*255.f));
            mesh.colors.push_back(255);
        } else {
            mesh.normals.push_back(0); mesh.normals.push_back(1); mesh.normals.push_back(0);
            mesh.colors.push_back(nm40[off+12]); mesh.colors.push_back(nm40[off+13]);
            mesh.colors.push_back(nm40[off+14]); mesh.colors.push_back(nm40[off+15]);
        }
    }

    std::vector<uint16_t> raw(ic);
    for (uint32_t i = 0; i < ic; i++) memcpy(&raw[i], nm40 + idxOff + i * 2, 2);

    mesh.indices.reserve(ic);
    for (uint32_t i = 0; i + 2 < ic; i++) {
        uint16_t i0 = raw[i], i1 = raw[i+1], i2 = raw[i+2];
        if (i0 == i1 || i1 == i2 || i0 == i2) continue;
        if (i % 2 == 0) { mesh.indices.push_back(i0); mesh.indices.push_back(i1); mesh.indices.push_back(i2); }
        else            { mesh.indices.push_back(i1); mesh.indices.push_back(i0); mesh.indices.push_back(i2); }
    }
    if (mesh.indices.empty()) return ParsedMesh();

    mesh.vertexCount   = (int)vc;
    mesh.triangleCount = (int)(mesh.indices.size() / 3);
    mesh.centerY       = ySum / (float)vc;
    return mesh;
}

std::vector<ParsedMesh> ParseNM40AllMeshes(const uint8_t* nm40, uint32_t nm40Size) {
    std::vector<ParsedMesh> result;
    s_nm40Error[0] = 0;

    if (nm40Size < 0x40 || memcmp(nm40, "NM40", 4) != 0) {
        snprintf(s_nm40Error, sizeof(s_nm40Error), "Not NM40 data (size=%u)", nm40Size);
        return result;
    }

    // Scan for ALL embedded PCRDs
    for (uint32_t pos = 0x40; pos + 28 < nm40Size; pos += 4) {
        if (memcmp(nm40 + pos, "PCRD", 4) != 0) continue;

        ParsedMesh mesh = ParseNM40SinglePCRD(nm40, nm40Size, pos);
        if (mesh.Valid()) {
            printf("[NM40] PCRD #%d at +0x%X: %d verts, %d tris\n",
                   (int)result.size(), pos, mesh.vertexCount, mesh.triangleCount);
            result.push_back(std::move(mesh));
        }
    }

    if (result.empty())
        snprintf(s_nm40Error, sizeof(s_nm40Error), "No valid PCRDs found in NM40 (%u bytes)", nm40Size);

    return result;
}
