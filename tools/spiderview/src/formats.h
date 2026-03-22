#pragma once
// ============================================================================
// formats.h — Spiderwick Chronicles binary format parsers
// ============================================================================
//
// Parses the engine's native binary formats from .pcw/.pcwb world files
// and .zwd asset archives. Ported from spiderwick_world_export.py.
//
// File format overview:
//   PCW  = ZLIB-compressed wrapper around PCWB (magic "ZLIB" or "SFZC")
//   PCWB = World geometry container (magic "PCWB", version 10)
//          Contains: header → props → PCRDs (geometry) → PCIMs (textures)
//   PCRD = Renderable geometry chunk (triangle strip, version 2)
//   PCIM = Texture wrapper around DDS data (193-byte header + DDS)
//   ZWD  = ZLIB-compressed AWAD asset archive
//   AWAD = Asset WAD container (hash-indexed entries)
//
// Coordinate system: engine uses left-handed Z-up (X=right, Y=forward, Z=up).
// Caller must convert to display space (e.g. raylib Y-up) during rendering.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>

// ============================================================================
// ParsedMesh — CPU-side mesh data extracted from a single PCRD chunk
// ============================================================================
struct ParsedMesh {
    std::vector<float>    positions;  // interleaved [x,y,z, ...], 3 per vertex
    std::vector<float>    normals;    // interleaved [nx,ny,nz,...], 3 per vertex (display-space Y-up)
    std::vector<float>    texcoords;  // interleaved [u,v, ...],   2 per vertex
    std::vector<uint8_t>  colors;     // interleaved [r,g,b,a,...], 4 per vertex
    std::vector<uint32_t> indices;    // triangle list (converted from strip)
    int   vertexCount   = 0;
    int   triangleCount = 0;
    float centerY       = 0.0f;      // average Y position (engine space)
    int   batchIndex    = -1;        // sub-batch index within NM40 (-1 = merged)
    int   boneCount     = 0;         // bone palette size for this batch
    int   submeshIndex  = -1;        // submesh group from NM40 descriptor table (-1 = unknown)

    // Skinning data (NM40 only, populated by ParseNM40Batches)
    std::vector<uint8_t>  blendIndices;  // 4 per vertex (global bone indices after palette remap)
    std::vector<float>    blendWeights;  // 4 per vertex

    bool Valid() const { return vertexCount > 0 && !indices.empty(); }
};

// NM40Bone — per-bone data extracted from NM40 vertex blend indices
struct NM40Bone {
    float position[3] = {0,0,0};  // centroid of weighted vertices (engine space)
    int   parentIndex = -1;        // parent bone (-1 = root)
    int   vertexCount = 0;         // vertices referencing this bone
};

// Multi-batch NM40 result — one ParsedMesh per sub-batch
struct NM40MeshResult {
    std::vector<ParsedMesh> batches;  // individual sub-batch meshes
    ParsedMesh merged;                // all batches merged (for single-texture fallback)
    std::vector<NM40Bone> bones;      // per-bone positions from vertex data
    int numBones = 0;

    // Bone bind-pose matrices (3 per bone × 16 floats each, from boneTransOff)
    // Matrix 0: local transform (bone-to-parent)
    // Matrix 1: world transform (bone-to-root, rest pose)
    // Matrix 2: inverse bind matrix (for GPU skinning)
    std::vector<float> boneLocalMatrices;   // numBones * 16
    std::vector<float> boneWorldMatrices;   // numBones * 16
    std::vector<float> boneInvBindMatrices; // numBones * 16

    bool Valid() const { return !batches.empty() || merged.Valid(); }
};

// ============================================================================
// PropInfo — placed prop instance from PCWB header table
// ============================================================================
//
// PCWB header at +0x50 = prop count, +0x98 = prop table pointer.
// Each entry is 0xA0 (160) bytes:
//   +0x00: 4x4 transform matrix (row-major, D3D9 row-vector convention)
//   +0x40: AABB min/max (unused here)
//   +0x60: name (null-terminated ASCII, up to 64 chars)
//   +0x8C: pointer to prop definition → mesh_list → blocks → batches → PCRDs
//
struct PropInfo {
    float    matrix[16];    // 4x4 row-major (D3D9: world = vertex * M)
    float    position[3];   // extracted from matrix[12..14]
    char     name[64];
    std::vector<uint32_t> pcrdOffsets; // all PCRDs this prop references
    int      type = 0;      // from def+12: 1=STATIC, 2=ANIMATED
};

// ============================================================================
// ZWDEntry / ZWDArchive — asset archive (.zwd files)
// ============================================================================
//
// ZWD = ZLIB/SFZC compressed AWAD.
// AWAD header: "AWAD" magic, entry count, then 12-byte entries (hash, offset, size).
//
struct ZWDEntry {
    uint32_t    nameHash;
    uint32_t    offset;
    uint32_t    size;
    std::string name;       // resolved developer name (if known), else hex hash
    std::string extension;  // detected from magic bytes at offset
};

class ZWDArchive {
public:
    std::vector<uint8_t> data;     // decompressed AWAD data
    std::vector<ZWDEntry> entries;
    std::string path;              // source file path

    bool Load(const char* path);
    const uint8_t* GetEntryData(int index) const;
    int FindByHash(uint32_t hash) const;
};

// ============================================================================
// PCWBHeaderInfo — lighting/fog/environment from PCWB header
// ============================================================================
struct PCWBHeaderInfo {
    float lightDir[3];     // directional light direction (engine Z-up coords)
    float fogParams[3];    // fog parameters (start, end, density or RGB)
    float headerFloats[16]; // raw floats from +0xB0..+0xEF for exploration
    uint32_t sectorCount;
    uint32_t drawDataCount;
    uint32_t propCount;
    uint32_t fileSize;
    bool valid = false;
};

// ============================================================================
// PCWBFile — world geometry container (.pcwb / .pcw files)
// ============================================================================
//
// PCWB header layout (confirmed via IDA + file analysis):
//   +0x00: "PCWB" magic
//   +0x04: version (must be 10)
//   +0x08: alignment (typically 4096)
//   +0x0C: total file size
//   +0x30: sector count
//   +0x4C: draw data entry count
//   +0x50: prop count
//   +0x64: BSP nodes array pointer (one root per sector)
//   +0x94: texture reference table pointer
//   +0x98: prop table pointer
//
// Texture mapping uses two methods:
//   1. Header table at +0x94: sequential (texIndex, pcimOffset) pairs
//   2. Batch sentinel pattern: 3×FFFFFFFF at batch+8, texIndex at batch+4,
//      PCRD offset at batch+0x2C (44 bytes from batch start)
//
class PCWBFile {
public:
    std::vector<uint8_t> data;     // raw PCWB data (kept alive for DDS pointers)

    // Discovered sections (populated by Parse)
    std::vector<uint32_t> pcrdOffsets;  // all validated PCRD chunk offsets
    std::vector<uint32_t> pcimOffsets;  // all validated PCIM chunk offsets
    std::vector<PropInfo>  props;       // parsed prop instances

    // Texture mapping (populated by Parse)
    std::unordered_map<int, uint32_t>  texRef;      // texIndex → pcimOffset
    std::unordered_map<uint32_t, int>  pcrdTexMap;   // pcrdOffset → texIndex

    // Prop position overrides from JSON (loaded before Parse)
    // Keyed by prop name (e.g., "Prop_Garden04"), values are position + rotation
    struct PropOverride { float pos[3]; float rot[3]; };
    std::unordered_map<std::string, PropOverride> propOverrides;

    // Load prop position overrides from JSON files
    void LoadPropPositions(const char* levelName);

    // Geometry instance transforms (populated by Parse)
    // Maps streaming PCRD offsets to their world transform matrix.
    // Discovered from batch entry structure: each batch has a 4x4 matrix
    // (identified by w=1.0 at +60) followed by sentinel entries (3×FFFFFFFF
    // with PCRD ref at +0x24). Multiple sentinels share the preceding matrix.
    std::unordered_map<uint32_t, std::vector<float>> pcrdWorldMatrix; // pcrdOffset → float[16]

    // --- Loading ---
    bool Load(const char* path);                     // .pcwb or .pcw (auto-detects)
    bool LoadFromBuffer(std::vector<uint8_t>&& buf); // from pre-decompressed data

    // --- Geometry ---
    ParsedMesh ParsePCRD(uint32_t offset, const float* worldMatrix = nullptr) const;

    // --- Textures ---
    bool ExtractDDS(uint32_t pcimOff, const uint8_t** outData,
                    uint32_t* outSize, uint32_t* outW, uint32_t* outH) const;
    int GetPCRDTexture(uint32_t pcrdOffset) const;

    // --- Header info ---
    PCWBHeaderInfo GetHeaderInfo() const;

    // --- Helpers (public for scene.cpp access) ---
    uint32_t ReadU32(size_t off) const;
    float    ReadF32(size_t off) const;

private:
    bool Parse();          // master parse: calls all sub-parsers below
    void FindPCRDs();      // scan for PCRD magic + validate header fields
    void FindPCIMs();      // scan for PCIM magic + validate DDS presence
    void BuildTexRefTable(); // build texIndex → pcimOffset from header table
    void BuildPCRDTexMap();  // build pcrdOffset → texIndex from batch sentinels
    void ParseProps();       // parse prop table → trace mesh chains → collect PCRDs
    void ParseInstanceTransforms(); // extract world matrices from batch entries in streaming data
};

// ============================================================================
// Decompression
// ============================================================================
// PCW/ZWD files use a simple wrapper: 4-byte magic ("ZLIB"/"SFZC") +
// 4-byte compressed size + 4-byte decompressed size + zlib stream.
// Uses real zlib (not raylib's sinfl which doesn't support zlib headers).
//
bool DecompressPCW(const char* path, std::vector<uint8_t>& out);

// ============================================================================
// NM40 mesh extraction (standalone, no PCWBFile needed)
// ============================================================================
// Parses an NM40 model's embedded PCRD to extract renderable geometry.
// NM40 vertex stride is 52: pos(12) + normal(12) + UV(8) + blendIdx(4) + blendWeight(16)
//
// Takes the full NM40 data (header + embedded PCRD + vertex/index buffers).
// The size must be the AWAD TOC entry size, not the gap-based header-only size.
// Internally scans for "PCRD" magic, reads PCRD header for counts/offsets.
ParsedMesh ParseNM40Mesh(const uint8_t* nm40Data, uint32_t nm40Size);
NM40MeshResult ParseNM40Batches(const uint8_t* nm40Data, uint32_t nm40Size);
std::vector<ParsedMesh> ParseNM40AllMeshes(const uint8_t* nm40Data, uint32_t nm40Size);
const char* GetNM40ParseError();
