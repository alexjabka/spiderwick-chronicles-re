// ============================================================================
// Shared utility functions for menu sub-modules
// ============================================================================

#include "menu_common.h"
#include "addresses.h"
#include "memory.h"
#include "log.h"
#include <cstring>
#include <cstdio>

// 3D overlay state
bool dbg3d_enabled     = false;
bool dbg3d_characters  = true;
bool dbg3d_sectors     = true;
bool dbg3d_showCoords  = false;
bool dbg3d_showAddrs   = false;
bool dbg3d_props       = false;
bool dbg3d_worldChunks = false;
bool dbg3d_vmPlacements = false;

// ============================================================================
// World state helpers
// ============================================================================

const char* WorldFriendlyName(const char* internal) {
    if (!internal || !internal[0]) return "Unknown";
    if (!_stricmp(internal, "MansionD"))  return "Mansion (Day)";
    if (!_stricmp(internal, "MnAttack"))  return "Mansion (Attack)";
    if (!_stricmp(internal, "GroundsD"))  return "Grounds (Day)";
    if (!_stricmp(internal, "GoblCamp"))  return "Goblin Camp";
    if (!_stricmp(internal, "ThimbleT"))  return "Thimbletack's Lair";
    if (!_stricmp(internal, "FrstRoad"))  return "Forest Road";
    if (!_stricmp(internal, "DeepWood"))  return "Deep Woods";
    if (!_stricmp(internal, "Tnl2Town"))  return "Tunnel to Town";
    if (!_stricmp(internal, "MGArena1"))  return "Minigame Arena 1";
    if (!_stricmp(internal, "MGArena2"))  return "Minigame Arena 2";
    if (!_stricmp(internal, "MGArena3"))  return "Minigame Arena 3";
    if (!_stricmp(internal, "MGArena4"))  return "Minigame Arena 4";
    if (!_stricmp(internal, "Shell"))     return "Main Menu";
    if (!_stricmp(internal, "Common"))    return "Common (shared)";
    return internal;
}

bool IsWorldValid() {
    uintptr_t ws = mem::Deref(addr::pWorldState);
    if (!ws) return false;
    int secCnt = mem::Read<int>(addr::pSectorCount);
    if (secCnt <= 0 || secCnt > 64) return false;
    uintptr_t secArr = mem::Deref(ws + 0x64);
    if (!secArr) return false;
    return true;
}

const char* GetSectorName(int sectorIdx) {
    uintptr_t ws = mem::Deref(addr::pWorldState);
    if (!ws) return nullptr;
    int secCnt = mem::Read<int>(addr::pSectorCount);
    if (sectorIdx < 0 || sectorIdx >= secCnt) return nullptr;
    uintptr_t secArr = mem::Deref(ws + 0x64);
    if (!secArr) return nullptr;
    uintptr_t sd = mem::Deref(secArr + 4 * sectorIdx);
    if (!sd) return nullptr;
    uint8_t firstByte = mem::Read<uint8_t>(sd);
    if (firstByte < 0x20 || firstByte > 0x7E) return nullptr;
    return reinterpret_cast<const char*>(sd);
}

const char* GetCurrentLevelName() {
    const char* lvl = reinterpret_cast<const char*>(addr::pCurrentLevelName);
    if (lvl && lvl[0]) return lvl;
    const char* sav = reinterpret_cast<const char*>(addr::pCurrentWorldName);
    if (sav && sav[0]) return sav;
    return "Unknown";
}

// ============================================================================
// World-to-Screen — engine's own VP matrix at CameraRender+0x448
// ============================================================================

bool WorldToScreen(float wx, float wy, float wz, float& sx, float& sy) {
    if (!g_matricesValid || g_vpWidth <= 0 || g_vpHeight <= 0) return false;

    const float* M = g_vpMatrix;

    float cx = wx*M[0] + wy*M[4] + wz*M[8]  + M[12];
    float cy = wx*M[1] + wy*M[5] + wz*M[9]  + M[13];
    float cz = wx*M[2] + wy*M[6] + wz*M[10] + M[14];
    float cw = wx*M[3] + wy*M[7] + wz*M[11] + M[15];

    if (cw <= 0.001f) return false;

    float ndx = cx / cw;
    float ndy = cy / cw;
    float ndz = cz / cw;

    if (ndz < 0.0f || ndz > 1.0f) return false;
    if (ndx < -1.5f || ndx > 1.5f || ndy < -1.5f || ndy > 1.5f) return false;

    sx = (ndx + 1.0f) * 0.5f * (float)g_vpWidth;
    sy = (1.0f - ndy) * 0.5f * (float)g_vpHeight;
    return true;
}

void DrawText3D(ImDrawList* dl, float wx, float wy, float wz,
                const char* text, ImU32 color) {
    float sx, sy;
    if (WorldToScreen(wx, wy, wz, sx, sy))
        dl->AddText(ImVec2(sx, sy), color, text);
}

// ============================================================================
// Character identification — widget hash lookup with RTTI fallback
// ============================================================================

static struct {
    int jared, mallory, simon, thimbletack;
    bool computed;
} charHashes = {};

static void EnsureCharHashes() {
    if (charHashes.computed) return;
    typedef int (__cdecl *HashFn)(const char*);
    auto hashStr = reinterpret_cast<HashFn>(addr::fn_HashString);
    charHashes.jared       = hashStr("Jared");
    charHashes.mallory     = hashStr("Mallory");
    charHashes.simon       = hashStr("Simon");
    charHashes.thimbletack = hashStr("ThimbleTack");
    charHashes.computed    = true;
    log::Write("CharHashes: Jared=0x%X Mallory=0x%X Simon=0x%X Thimble=0x%X",
        charHashes.jared, charHashes.mallory, charHashes.simon, charHashes.thimbletack);
}

static struct {
    struct Entry { int hash; const char* name; };
    Entry table[34];
    int count;
    bool built;
} nameTable = {};

static void EnsureNameTable() {
    if (nameTable.built) return;
    nameTable.built = true;
    typedef int (__cdecl *HashFn)(const char*);
    auto hashStr = reinterpret_cast<HashFn>(addr::fn_HashString);

    const char* names[] = {
        "Jared", "Mallory", "Simon", "ThimbleTack",
        "Goblin", "BullGoblin", "Redcap", "Hogsqueal",
        "Cockroach", "LeatherWing", "Griffin", "MoleTroll",
        "RiverTroll", "FireSalamander", "Cockatrice", "Mulgarath",
        "Boggart", "Sprite", "WillOWisp", "StoneGolem",
        "Elf", "Dwarf", "Phoenix", "StraySod",
        "GoblinArcher", "GoblinWarrior", "GoblinShaman",
        "GoblinScout", "GoblinBrute", "GoblinChief",
        "Sentry", "Guard", "Boss", "Minion",
        nullptr
    };
    nameTable.count = 0;
    for (int i = 0; names[i] && nameTable.count < 34; i++) {
        nameTable.table[nameTable.count].hash = hashStr(names[i]);
        nameTable.table[nameTable.count].name = names[i];
        nameTable.count++;
    }
}

static thread_local char identBuf[32];

const char* IdentifyCharacter(uintptr_t charObj) {
    EnsureCharHashes();
    EnsureNameTable();
    uintptr_t widget = mem::Read<uintptr_t>(charObj + addr::CHAR_WIDGET_PTR);
    if (!widget || widget < 0x10000 || widget > 0x7FFFFFFF) {
        // No widget — RTTI class name from vtable
        uintptr_t vt = mem::Read<uintptr_t>(charObj);
        if (vt && vt > 0x400000 && vt < 0x7FFFFFFF) {
            uintptr_t rtti = mem::Read<uintptr_t>(vt - 4);
            if (rtti && rtti > 0x400000 && rtti < 0x7FFFFFFF) {
                uintptr_t tdPtr = mem::Read<uintptr_t>(rtti + 12);
                if (tdPtr && tdPtr > 0x400000 && tdPtr < 0x7FFFFFFF) {
                    const char* raw = reinterpret_cast<const char*>(tdPtr + 8);
                    if (raw[0] == '.' && raw[1] == '?' && raw[2] == 'A' && raw[3] == 'V') {
                        const char* name = raw + 4;
                        static thread_local char rttiBuf[32];
                        int len = 0;
                        while (name[len] && name[len] != '@' && len < 30) {
                            rttiBuf[len] = name[len];
                            len++;
                        }
                        rttiBuf[len] = 0;
                        return rttiBuf;
                    }
                }
            }
        }
        return "???";
    }
    int hash = mem::Read<int>(widget + addr::WIDGET_NAME_HASH);

    for (int i = 0; i < nameTable.count; i++) {
        if (nameTable.table[i].hash == hash)
            return nameTable.table[i].name;
    }

    snprintf(identBuf, sizeof(identBuf), "NPC#%04X", (unsigned)hash & 0xFFFF);
    return identBuf;
}

bool IsPlayerCharacter(uintptr_t charObj) {
    uintptr_t vtable = mem::Read<uintptr_t>(charObj);
    if (!vtable || vtable < 0x400000 || vtable > 0x7FFFFFFF) return false;
    typedef bool (__thiscall *IsPlayerFn)(void*);
    auto fn = reinterpret_cast<IsPlayerFn>(mem::Read<uintptr_t>(vtable + addr::VTBL_IS_PLAYER * 4));
    if (!fn) return false;
    return fn(reinterpret_cast<void*>(charObj));
}
