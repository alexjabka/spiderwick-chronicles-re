#pragma once
// ============================================================================
// Sector utilities — AABB lookup, shared by freecam + render patches
// ============================================================================

namespace sector {

// Find which sector contains point (x,y,z) via AABB test.
// Returns sector index, or nearest sector if outside all AABBs.
// Returns -1 if world state is unavailable.
int FindByAABB(float x, float y, float z);

} // namespace sector
