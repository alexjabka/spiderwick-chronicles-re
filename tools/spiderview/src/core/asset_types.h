#pragma once
// ============================================================================
// asset_types.h — Asset type detection for Spiderwick engine formats
// ============================================================================

#include <cstdint>
#include <cstring>

enum class AssetType {
    Unknown,
    PCW, PCWB, ZWD, AWAD,
    PCIM, PCRD, NM40, SCT,
    DBDB, STTL,
    BIK, SEG, BNK, BIN
};

inline const char* AssetTypeName(AssetType t) {
    switch (t) {
        case AssetType::PCW:  return "PCW";
        case AssetType::PCWB: return "PCWB";
        case AssetType::ZWD:  return "ZWD";
        case AssetType::AWAD: return "AWAD";
        case AssetType::PCIM: return "PCIM";
        case AssetType::PCRD: return "PCRD";
        case AssetType::NM40: return "NM40";
        case AssetType::SCT:  return "SCT";
        case AssetType::DBDB: return "DBDB";
        case AssetType::STTL: return "STTL";
        case AssetType::BIK:  return "BIK";
        case AssetType::SEG:  return "SEG";
        case AssetType::BNK:  return "BNK";
        case AssetType::BIN:  return "BIN";
        default:              return "???";
    }
}

// Detect asset type from first 4 bytes of data
inline AssetType DetectAssetType(const uint8_t* data, uint32_t available) {
    if (available < 4) return AssetType::Unknown;
    if (data[0]=='S'&&data[1]=='C'&&data[2]=='T'&&data[3]==0) return AssetType::SCT;
    if (memcmp(data,"NM40",4)==0) return AssetType::NM40;
    if (memcmp(data,"PCWB",4)==0) return AssetType::PCWB;
    if (memcmp(data,"PCIM",4)==0) return AssetType::PCIM;
    if (memcmp(data,"PCRD",4)==0) return AssetType::PCRD;
    if (memcmp(data,"DBDB",4)==0) return AssetType::DBDB;
    if (memcmp(data,"STTL",4)==0) return AssetType::STTL;
    if (memcmp(data,"AWAD",4)==0) return AssetType::AWAD;
    if (memcmp(data,"SFZC",4)==0) return AssetType::ZWD;
    if (memcmp(data,"ZLIB",4)==0) return AssetType::ZWD;
    if (memcmp(data,"BIKi",4)==0) return AssetType::BIK;
    return AssetType::Unknown;
}

// Detect from file extension
inline AssetType AssetTypeFromExt(const char* ext) {
    if (!ext) return AssetType::Unknown;
    if (strcmp(ext,".pcw")==0||strcmp(ext,".pcwb")==0) return AssetType::PCW;
    if (strcmp(ext,".zwd")==0) return AssetType::ZWD;
    if (strcmp(ext,".sct")==0) return AssetType::SCT;
    if (strcmp(ext,".bik")==0) return AssetType::BIK;
    if (strcmp(ext,".seg")==0) return AssetType::SEG;
    if (strcmp(ext,".bnk")==0) return AssetType::BNK;
    if (strcmp(ext,".bin")==0) return AssetType::BIN;
    return AssetType::Unknown;
}

// Size field offset for known types (-1 = no size field)
inline int AssetSizeFieldOffset(AssetType t) {
    switch (t) {
        case AssetType::SCT:  return 8;  // +8 = dataSize, total = val + 52
        case AssetType::NM40: return -1; // no simple size field, computed from gaps
        case AssetType::PCWB: return 12;
        case AssetType::DBDB: return 8;  // +8 = total size
        case AssetType::STTL: return 12; // +C = total size
        default: return -1;
    }
}

// Can this type be acted on (loaded, previewed, executed)?
inline bool AssetTypeActionable(AssetType t) {
    switch (t) {
        case AssetType::SCT:
        case AssetType::PCWB:
        case AssetType::PCIM:
        case AssetType::PCW:
            return true;
        default:
            return false;
    }
}
