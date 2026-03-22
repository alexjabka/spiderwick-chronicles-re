// ============================================================================
// Sector utilities
// ============================================================================

#include "sector.h"
#include "addresses.h"
#include "memory.h"

namespace sector {

int FindByAABB(float x, float y, float z) {
    uintptr_t ws = mem::Deref(addr::pWorldState);
    if (!ws) return -1;

    uintptr_t secArr = mem::Deref(ws + 0x64);
    if (!secArr) return -1;

    int secCnt = mem::Read<int>(addr::pSectorCount);
    if (secCnt <= 0 || secCnt > 64) return -1;

    // Exact match
    for (int i = 0; i < secCnt; i++) {
        uintptr_t sd = mem::Deref(secArr + 4 * i);
        if (!sd) continue;
        float minX = mem::Read<float>(sd + 0x10);
        float minY = mem::Read<float>(sd + 0x14);
        float minZ = mem::Read<float>(sd + 0x18);
        float maxX = mem::Read<float>(sd + 0x20);
        float maxY = mem::Read<float>(sd + 0x24);
        float maxZ = mem::Read<float>(sd + 0x28);
        if (x >= minX && x <= maxX && y >= minY && y <= maxY && z >= minZ && z <= maxZ)
            return i;
    }

    // Fallback: nearest sector center
    float bestDist = 1e30f;
    int bestSector = 0;
    for (int i = 0; i < secCnt; i++) {
        uintptr_t sd = mem::Deref(secArr + 4 * i);
        if (!sd) continue;
        float cx = (mem::Read<float>(sd + 0x10) + mem::Read<float>(sd + 0x20)) * 0.5f;
        float cy = (mem::Read<float>(sd + 0x14) + mem::Read<float>(sd + 0x24)) * 0.5f;
        float cz = (mem::Read<float>(sd + 0x18) + mem::Read<float>(sd + 0x28)) * 0.5f;
        float dx = x - cx, dy = y - cy, dz = z - cz;
        float d = dx*dx + dy*dy + dz*dz;
        if (d < bestDist) { bestDist = d; bestSector = i; }
    }
    return bestSector;
}

} // namespace sector
