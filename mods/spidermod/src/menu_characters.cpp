// ============================================================================
// Character list, hot-switch, spawn, clone, VM diagnostic
// ============================================================================

#include "menu_common.h"
#include "freecam.h"
#include "addresses.h"
#include "memory.h"
#include "log.h"
#include <cstring>
#include <cstdio>

// Deferred VM spawn (handled in dllmain.cpp HookedCameraSectorUpdate)
extern volatile bool g_pendingVMSpawn;
extern volatile uintptr_t g_vmSpawnSource;

void DrawCharacterList() {
    if (!IsWorldValid()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "World loading...");
        return;
    }

    uintptr_t head = mem::Read<uintptr_t>(addr::pCharacterListHead);
    if (!head) {
        ImGui::TextDisabled("No characters in world");
        return;
    }

    // Count characters
    int count = 0;
    uintptr_t cur = head;
    while (cur && count < 64) {
        count++;
        cur = mem::Read<uintptr_t>(cur + addr::CHAR_NEXT);
    }
    ImGui::Text("Characters: %d", count);

    typedef uintptr_t (__cdecl *GetPlayerFn)();
    auto getPlayer = reinterpret_cast<GetPlayerFn>(addr::fn_GetPlayerCharacter);
    uintptr_t currentPlayer = getPlayer();

    if (ImGui::BeginTable("chars", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 20);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Player", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();

        cur = head;
        for (int i = 0; cur && i < 64; i++) {
            ImGui::TableNextRow();

            const char* name = IdentifyCharacter(cur);
            float px = mem::Read<float>(cur + addr::CHAR_POS_X);
            float py = mem::Read<float>(cur + addr::CHAR_POS_Y);
            float pz = mem::Read<float>(cur + addr::CHAR_POS_Z);
            bool isPlayer = IsPlayerCharacter(cur);
            bool isCurrent = (cur == currentPlayer);
            bool isPlayable = false;
            {
                uintptr_t vt = mem::Read<uintptr_t>(cur);
                if (vt && vt > 0x400000 && vt < 0x7FFFFFFF) {
                    typedef bool (__thiscall *IsPlayableFn)(void*);
                    auto fn = reinterpret_cast<IsPlayableFn>(
                        mem::Read<uintptr_t>(vt + addr::VTBL_IS_PLAYABLE * 4));
                    if (fn) isPlayable = fn(reinterpret_cast<void*>(cur));
                }
            }

            ImVec4 color = isCurrent ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) :
                           isPlayer  ? ImVec4(0.3f, 0.8f, 1.0f, 1.0f) :
                           isPlayable? ImVec4(0.9f, 0.9f, 0.5f, 1.0f) :
                                       ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(color, "%d", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(color, "%s%s", name, isCurrent ? " *" : "");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(color, "%.0f, %.0f, %.0f", px, py, pz);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(color, "%s", isPlayer ? "Yes" : "");
            ImGui::TableSetColumnIndex(4);
            {
                ImGui::PushID(static_cast<int>(cur));
                if (!isCurrent) {
                    if (ImGui::SmallButton("TP")) {
                        if (freecam::enabled) {
                            freecam::posX = px;
                            freecam::posY = py;
                            freecam::posZ = pz + 2.0f;
                        } else {
                            uintptr_t pl = getPlayer();
                            if (pl) {
                                mem::WriteDirect<float>(pl + addr::CHAR_POS_X, px);
                                mem::WriteDirect<float>(pl + addr::CHAR_POS_Y, py);
                                mem::WriteDirect<float>(pl + addr::CHAR_POS_Z, pz);
                            }
                        }
                    }
                    ImGui::SameLine();
                }
                if (!isCurrent) {
                    uintptr_t vt = mem::Read<uintptr_t>(cur);
                    bool canSwitch = false;
                    if (vt && vt > 0x400000 && vt < 0x7FFFFFFF)
                        canSwitch = (mem::Read<uintptr_t>(vt + 116 * 4) == 0x463880);
                    if (canSwitch) {
                        if (ImGui::SmallButton("Play")) {
                            g_pendingSwitchTarget = cur;
                            g_pendingSwitchType = 1;
                            log::Write("TableSwitch: char=0x%X name=%s", cur, name);
                        }
                    }
                }
                ImGui::PopID();
            }
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("0x%X", cur);

            cur = mem::Read<uintptr_t>(cur + addr::CHAR_NEXT);
        }
        ImGui::EndTable();
    }

    // Character switching info
    ImGui::Separator();

    static int playerCharIdx = -2;
    if (playerCharIdx == -2) {
        typedef int (__cdecl *HashLookupFn)(const char*);
        auto hashLookup = reinterpret_cast<HashLookupFn>(addr::fn_HashLookup);
        playerCharIdx = hashLookup("/Player/Character");
    }

    int currentType = 0;
    if (playerCharIdx >= 0) {
        typedef int* (__thiscall *GetValueFn)(void*, int);
        auto getValue = reinterpret_cast<GetValueFn>(addr::fn_GetValuePtr);
        int* valPtr = getValue(reinterpret_cast<void*>(addr::pDataStore), playerCharIdx);
        if (valPtr) currentType = *valPtr;
    }

    const char* typeNames[] = { "???", "Jared", "Mallory", "Simon" };
    const char* curName = (currentType >= 1 && currentType <= 3) ? typeNames[currentType] : "???";
    ImGui::Text("Current: %s  (use [Play] buttons in table above)", curName);

    // ---- VMCall diagnostic ----
    if (ImGui::Button("VM Diagnostic")) {
        const char* allNames[] = {
            "Activate", "ActivateObject", "CheckItem", "CinematicAudioEvent",
            "CinematicEnded", "CinematicGameEvent", "Death", "Deactivate",
            "DoneRotate", "Dying", "EnterCoop", "ExitCoop",
            "FireProjectile", "GiveWeaponsCheat", "HideMonocle", "ItemWasGiven",
            "OnAiAlerted", "OnAiAlertedNearby", "OnAiBelow25pcHealth",
            "OnAiBelow50pcHealth", "OnAiBelow75pcHealth", "OnAiDead", "OnAiDie",
            "OnAiDoAttack", "OnAiUnalerted", "OnAlerted", "OnDamage", "OnDie",
            "OnDispossessed", "OnGroupAiTick", "OnProjectileStrike",
            "OnSpawnChangelingSpritePower", "OnUnalerted", "OnWeaponSwungAt",
            "PropDestroyed", "PropHealed", "Reached", "RemoveMember",
            "ResetHumanTriggers", "SetSegmentVolume", "SoundDonePlaying",
            "SpritePowerEnd", "Stop", "StruckByDamageBoostedWeapon",
            "StruckByGobstone", "StruckByProjectile", "StruckByWeapon",
            "Tick", "Trigger", "onConversationAborted", "onConversationClosed",
            "onConversationOptionFired", "onConversationPlayerSwitch",
            nullptr
        };

        typedef char (__cdecl *VMCallFn)(int, int);
        auto vmCall = reinterpret_cast<VMCallFn>(0x52EB40);

        uint32_t scriptsLoaded = mem::Read<uint32_t>(0xE561F8);
        log::Write("=== VM DIAGNOSTIC === scripts_loaded=0x%X", scriptsLoaded);

        int coopCount = mem::Read<int>(0x730268);
        log::Write("Coop events array: count=%d", coopCount);
        for (int i = 0; i < coopCount && i < 8; i++)
            log::Write("  coop[%d] = 0x%X", i, mem::Read<uint32_t>(0x730270 + i * 4));

        cur = mem::Read<uintptr_t>(addr::pCharacterListHead);
        int ci = 0;
        while (cur && ci < 16) {
            const char* charName = IdentifyCharacter(cur);
            uintptr_t vmObj = mem::Read<uintptr_t>(cur + 0xA8);
            log::Write("\n--- %s (char=0x%X vmObj=0x%X) ---", charName, cur, vmObj);

            if (vmObj && scriptsLoaded) {
                int found = 0;
                for (int i = 0; allNames[i]; i++) {
                    char r = vmCall(static_cast<int>(vmObj),
                                    reinterpret_cast<int>(const_cast<char*>(allNames[i])));
                    if (r) {
                        log::Write("  ** FOUND: \"%s\" = %d **", allNames[i], (int)r);
                        found++;
                    }
                }
                if (!found) log::Write("  (no functions resolved)");
            }

            cur = mem::Read<uintptr_t>(cur + addr::CHAR_NEXT);
            ci++;
        }

        for (int i = 0; i < coopCount && i < 8; i++) {
            uint32_t coopObj = mem::Read<uint32_t>(0x730270 + i * 4);
            if (!coopObj) continue;
            log::Write("\n--- Coop[%d] (vmObj=0x%X) ---", i, coopObj);
            for (int j = 0; allNames[j]; j++) {
                char r = vmCall(static_cast<int>(coopObj),
                                reinterpret_cast<int>(const_cast<char*>(allNames[j])));
                if (r) log::Write("  ** FOUND: \"%s\" = %d **", allNames[j], (int)r);
            }
        }

        log::Write("=== VM DIAGNOSTIC DONE ===");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(check log)");

    // ---- Factory dump ----
    if (ImGui::Button("Dump Factories")) {
        int factoryCount = mem::Read<int>(0x6E8848);
        log::Write("=== FACTORY TABLE: %d entries ===", factoryCount);
        for (int i = 0; i < factoryCount && i < 64; i++) {
            uintptr_t base = 0x6E8868 + i * 12;
            uint32_t data    = mem::Read<uint32_t>(base + 0);
            uint32_t classId = mem::Read<uint32_t>(base + 4);
            uint32_t func    = mem::Read<uint32_t>(base + 8);
            if (classId || func)
                log::Write("  [%d] classId=0x%X func=0x%X data=0x%X", i, classId, func, data);
        }
        log::Write("=== FACTORY DUMP DONE ===");
    }
    ImGui::SameLine();

    // ---- Deep VM / Spawner Diagnostic ----
    if (ImGui::Button("Dump VM Objects")) {
        uint32_t scriptsLoaded = mem::Read<uint32_t>(0xE561F8);
        if (!scriptsLoaded) {
            log::Write("VM: scripts not loaded");
        } else {
            uintptr_t objBase = mem::Read<uintptr_t>(0xE56160);
            log::Write("=== DEEP VM DIAGNOSTIC === VMObjBase=0x%X", objBase);

            cur = mem::Read<uintptr_t>(addr::pCharacterListHead);
            int ci = 0;
            while (cur && ci < 32) {
                const char* charName = IdentifyCharacter(cur);
                uintptr_t vmObj = mem::Read<uintptr_t>(cur + 0xA8);
                uintptr_t spawnerRef = mem::Read<uintptr_t>(cur + 0x22C);

                log::Write("\n  --- %s (char=0x%X) ---", charName, cur);

                // VM Object structure
                if (vmObj) {
                    log::Write("  VM obj=0x%X (offset from base: %d)",
                        vmObj, (int)(vmObj - objBase));
                    // Dump raw VM object (first 32 bytes)
                    log::Write("    +00: %08X %08X %08X %08X",
                        mem::Read<uint32_t>(vmObj+0), mem::Read<uint32_t>(vmObj+4),
                        mem::Read<uint32_t>(vmObj+8), mem::Read<uint32_t>(vmObj+12));
                    log::Write("    +10: %08X %08X %08X %08X",
                        mem::Read<uint32_t>(vmObj+16), mem::Read<uint32_t>(vmObj+20),
                        mem::Read<uint32_t>(vmObj+24), mem::Read<uint32_t>(vmObj+28));

                    uintptr_t classDef = mem::Read<uintptr_t>(vmObj + 0x14);
                    if (classDef) {
                        uint32_t classId = mem::Read<uint32_t>(classDef + 4);
                        log::Write("    classDef=0x%X classId=0x%X", classDef, classId);
                        // Dump class def first 32 bytes
                        log::Write("    classDef +00: %08X %08X %08X %08X",
                            mem::Read<uint32_t>(classDef+0), mem::Read<uint32_t>(classDef+4),
                            mem::Read<uint32_t>(classDef+8), mem::Read<uint32_t>(classDef+12));
                        log::Write("    classDef +10: %08X %08X %08X %08X",
                            mem::Read<uint32_t>(classDef+16), mem::Read<uint32_t>(classDef+20),
                            mem::Read<uint32_t>(classDef+24), mem::Read<uint32_t>(classDef+28));
                    }
                } else {
                    log::Write("  VM obj: NULL");
                }

                // Spawner backref
                if (spawnerRef) {
                    uintptr_t spVt = mem::Read<uintptr_t>(spawnerRef);
                    uintptr_t spVm = mem::Read<uintptr_t>(spawnerRef + 0xA8);
                    log::Write("  Spawner=0x%X vtable=0x%X vmObj=0x%X", spawnerRef, spVt, spVm);
                    if (spVm) {
                        uintptr_t spClassDef = mem::Read<uintptr_t>(spVm + 0x14);
                        uint32_t spClassId = spClassDef ? mem::Read<uint32_t>(spClassDef + 4) : 0;
                        log::Write("    Spawner classDef=0x%X classId=0x%X", spClassDef, spClassId);
                    }
                } else {
                    log::Write("  Spawner: NULL (no backref at +0x22C)");
                }

                cur = mem::Read<uintptr_t>(cur + addr::CHAR_NEXT);
                ci++;
            }

            // Pool state
            uint32_t poolSlot = mem::Read<uint32_t>(0x730798);
            uint32_t poolAvail = mem::Read<uint32_t>(0x7307BC);
            uint32_t poolCount = mem::Read<uint32_t>(0x7307B4);
            uint32_t poolMax   = mem::Read<uint32_t>(0x7307C4);
            uint32_t totalCreated = mem::Read<uint32_t>(0x7307D4);
            log::Write("\n  CharPool: slot=0x%X avail=%d active=%d peak=%d serial=%d",
                poolSlot, poolAvail, poolCount, poolMax, totalCreated);

            // Scan VM pool for objects by magic number 0x004A424F
            log::Write("\n  --- Scanning VM pool (magic=004A424F) ---");
            constexpr uint32_t VM_MAGIC = 0x004A424F;
            // Known classIds: ClPlayerObj=0xF6EA786C, ClCharacterObj=0xF758F803
            int totalVmObjs = 0, unlinkedChar = 0;
            // Scan up to 256KB of VM pool
            for (int scan = 0; scan < 262144; scan += 4) {
                uint32_t val = mem::Read<uint32_t>(objBase + scan);
                if (val != VM_MAGIC) continue;

                uintptr_t candidate = objBase + scan;
                uintptr_t nativeLink = mem::Read<uintptr_t>(candidate + 16);
                uintptr_t cDef = mem::Read<uintptr_t>(candidate + 20);
                totalVmObjs++;

                if (totalVmObjs <= 30) {
                    uint32_t cId = (cDef > 0x100000 && cDef < 0x7FFFFFFF)
                        ? mem::Read<uint32_t>(cDef + 4) : 0;
                    log::Write("    [%d] base+%d native=%s classDef=0x%X classId=0x%08X",
                        totalVmObjs, scan,
                        nativeLink ? "LINKED" : "FREE",
                        cDef, cId);
                }

                // Check for unlinked character templates
                if (nativeLink == 0 && cDef > 0x100000 && cDef < 0x7FFFFFFF) {
                    uint32_t cId = mem::Read<uint32_t>(cDef + 4);
                    if (cId == 0xF6EA786C || cId == 0xF758F803) {
                        log::Write("    *** UNLINKED CHARACTER TEMPLATE at 0x%X classId=0x%X ***",
                            candidate, cId);
                        unlinkedChar++;
                    }
                }
            }
            log::Write("  Total VM objects found: %d, unlinked character templates: %d",
                totalVmObjs, unlinkedChar);

            log::Write("=== DEEP VM DIAGNOSTIC DONE ===");
        }
    }

    // ---- VM Clone Spawn (full pipeline, deferred to game update) ----
    ImGui::Separator();
    if (ImGui::Button("VM Spawn") && IsWorldValid()) {
        typedef uintptr_t (__cdecl *GetPlayerFn2)();
        auto gp2 = reinterpret_cast<GetPlayerFn2>(addr::fn_GetPlayerCharacter);
        uintptr_t source = gp2();
        if (!source) {
            log::Write("VMSpawn: no player");
        } else {
            // Defer to game update context (HookedCameraSectorUpdate)
            g_vmSpawnSource = source;
            g_pendingVMSpawn = true;
            log::Write("VMSpawn: deferred for source=0x%X", source);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("full pipeline (deferred)");
    ImGui::SameLine();
    if (false && IsWorldValid()) {  // OLD direct path — disabled
        typedef uintptr_t (__cdecl *GetPlayerFn2)();
        auto gp2 = reinterpret_cast<GetPlayerFn2>(addr::fn_GetPlayerCharacter);
        uintptr_t source = gp2();
        if (!source) {
            log::Write("VMSpawn: no player");
        } else {
            uintptr_t srcVm = mem::Read<uintptr_t>(source + addr::CHAR_VM_LINK);
            uintptr_t vmBase = mem::Read<uintptr_t>(addr::pVMObjBase);
            if (!srcVm || !vmBase) {
                log::Write("VMSpawn: no VM obj (srcVm=0x%X base=0x%X)", srcVm, vmBase);
            } else {
                log::Write("=== VM CLONE SPAWN ===");
                log::Write("  Source: char=0x%X vmObj=0x%X", source, srcVm);

                // Step 1: VM object size — conservative large copy
                // Can't reliably detect size (magic 004A424F appears as data in locals)
                // ClPlayerObj VM objects are spaced ~7000 bytes apart in the pool
                constexpr int vmObjSize = 8192;
                constexpr uint32_t VM_MAGIC = 0x004A424F;
                log::Write("  VM object copy size: %d bytes (conservative)", vmObjSize);

                // Step 2: Find pool end and write clone WITHIN the VM pool
                // Scan for highest used offset (last magic in pool)
                int lastObjOffset = 0;
                for (int scan = 0; scan < 524288; scan += 4) {
                    __try {
                        if (mem::Read<uint32_t>(vmBase + scan) == VM_MAGIC)
                            lastObjOffset = scan;
                    } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
                }
                // Place clone after last object + generous padding
                int cloneOffset = lastObjOffset + 1024;
                uintptr_t vmCloneAddr = vmBase + cloneOffset;
                log::Write("  Pool: lastObj at +%d, clone at +%d (0x%X)",
                    lastObjOffset, cloneOffset, vmCloneAddr);

                // Verify pool memory is writable at clone location
                bool canWrite = false;
                __try {
                    uint32_t test = mem::Read<uint32_t>(vmCloneAddr);
                    mem::WriteDirect<uint32_t>(vmCloneAddr, test); // write back same value
                    canWrite = true;
                } __except(EXCEPTION_EXECUTE_HANDLER) {}

                if (!canWrite) {
                    log::Write("  FAILED: pool memory not writable at +%d", cloneOffset);
                } else {
                    // Copy VM object into pool memory
                    memcpy(reinterpret_cast<void*>(vmCloneAddr),
                           reinterpret_cast<void*>(srcVm), vmObjSize);
                    void* vmClone = reinterpret_cast<void*>(vmCloneAddr);

                    // Step 3: Clear native link and initialized flag
                    *reinterpret_cast<uintptr_t*>((uintptr_t)vmClone + 16) = 0;  // unlink
                    uint16_t* flagsPtr = reinterpret_cast<uint16_t*>((uintptr_t)vmClone + 26);
                    *flagsPtr &= ~0x4000;  // clear "initialized" so Init runs fresh
                    log::Write("  VM clone at 0x%X, cleared native+initFlag", (uintptr_t)vmClone);

                    // Step 4: Manual pipeline (split for crash diagnosis)
                    uint8_t wasDisabled = mem::Read<uint8_t>(addr::pAllocDisabled);
                    if (wasDisabled) mem::WriteDirect<uint8_t>(addr::pAllocDisabled, 0);

                    uintptr_t newChar = 0;

                    // 4a: GetAssetClassId — inline: *(*(vmObj+20)+4)
                    uintptr_t cloneClassDef = mem::Read<uintptr_t>(vmCloneAddr + 20);
                    uint32_t classId = cloneClassDef ? mem::Read<uint32_t>(cloneClassDef + 4) : 0;
                    log::Write("  4a: classId=0x%X (classDef=0x%X)", classId, cloneClassDef);

                    // 4b: FactoryDispatch — creates native ClPlayerObj
                    uintptr_t nativeRaw = 0;
                    if (classId) {
                        typedef uintptr_t (__cdecl *FactoryFn)(uint32_t);
                        auto factory = reinterpret_cast<FactoryFn>(0x4D6030);
                        __try {
                            nativeRaw = factory(classId);
                            log::Write("  4b: FactoryDispatch = 0x%X", nativeRaw);
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            log::Write("  4b: EXCEPTION in FactoryDispatch!");
                        }
                    }

                    // 4c: VMInitObject — links VM↔native, runs Init script
                    bool vmInitOk = false;
                    if (nativeRaw) {
                        uintptr_t adjusted = nativeRaw - 4;
                        typedef int (__cdecl *VMInitFn)(uintptr_t, uintptr_t);
                        auto vmInit = reinterpret_cast<VMInitFn>(0x52EC30);
                        __try {
                            vmInit(vmCloneAddr, adjusted);
                            newChar = adjusted;
                            vmInitOk = true;
                            log::Write("  4c: VMInitObject OK — native=0x%X", newChar);
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            log::Write("  4c: EXCEPTION in VMInitObject!");
                            newChar = adjusted;
                        }
                    }

                    if (wasDisabled) mem::WriteDirect<uint8_t>(addr::pAllocDisabled, 1);

                    if (newChar && vmInitOk) {
                        // Step 6: Set position (only if Init ran — otherwise no visuals)
                        float px = mem::Read<float>(source + addr::CHAR_POS_X) + 5.0f;
                        float py = mem::Read<float>(source + addr::CHAR_POS_Y);
                        float pz = mem::Read<float>(source + addr::CHAR_POS_Z);

                        // Use vtable[1] (SetPosition) for proper transform update
                        uintptr_t vt = mem::Read<uintptr_t>(newChar);
                        typedef void (__thiscall *SetPosFn)(void*, float*);
                        auto setPos = reinterpret_cast<SetPosFn>(mem::Read<uintptr_t>(vt + 1 * 4));
                        float posVec[4] = { px, py, pz, 0.0f };
                        __try {
                            setPos(reinterpret_cast<void*>(newChar), posVec);
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            // Fallback: direct write
                            mem::WriteDirect<float>(newChar + addr::CHAR_POS_X, px);
                            mem::WriteDirect<float>(newChar + addr::CHAR_POS_Y, py);
                            mem::WriteDirect<float>(newChar + addr::CHAR_POS_Z, pz);
                        }

                        // Step 7: Activate (vtable[2]) — register with render system
                        typedef void (__thiscall *ActivateFn)(void*);
                        auto activate = reinterpret_cast<ActivateFn>(mem::Read<uintptr_t>(vt + 2 * 4));
                        __try {
                            activate(reinterpret_cast<void*>(newChar));
                            log::Write("  vtable[2] Activate done");
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            log::Write("  WARN: Activate exception");
                        }

                        // Step 8: Clear player flag, insert at END of list
                        uint32_t flags = mem::Read<uint32_t>(newChar + addr::CHAR_FLAGS);
                        mem::WriteDirect<uint32_t>(newChar + addr::CHAR_FLAGS, flags & ~0x4u);

                        uintptr_t tail = mem::Read<uintptr_t>(addr::pCharacterListHead);
                        if (!tail) {
                            mem::WriteDirect<uintptr_t>(addr::pCharacterListHead, newChar);
                        } else {
                            for (int li = 0; li < 64; li++) {
                                uintptr_t next = mem::Read<uintptr_t>(tail + addr::CHAR_NEXT);
                                if (!next) break;
                                tail = next;
                            }
                            mem::WriteDirect<uintptr_t>(tail + addr::CHAR_NEXT, newChar);
                        }

                        log::Write("  Render: +0x13C=0x%X +0x138=0x%X +0x9C=0x%X comps=%d",
                            mem::Read<uintptr_t>(newChar + 0x13C),
                            mem::Read<uintptr_t>(newChar + 0x138),
                            mem::Read<uintptr_t>(newChar + 0x9C),
                            mem::Read<uint32_t>(newChar + addr::CHAR_RENDER_COUNT));
                        log::Write("  Children: [3]=0x%X",
                            mem::Read<uintptr_t>(newChar + 0x374));
                        log::Write("  pos=(%.1f, %.1f, %.1f)", px, py, pz);
                        log::Write("=== VM SPAWN COMPLETE: 0x%X ===", newChar);
                    } else if (newChar && !vmInitOk) {
                        log::Write("  VMInit failed — factory obj at 0x%X NOT activated (no visuals)", newChar);
                        log::Write("  (no crash — char not inserted in list)");
                    } else {
                        log::Write("  CreateCharacter_Internal FAILED (returned 0)");
                    }
                }
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("full pipeline");
    ImGui::SameLine();

    // ---- Old Clone Spawn ----
    static float spawnOffsetX = 5.0f;
    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat("Offset X", &spawnOffsetX, 0.5f, -50.0f, 50.0f, "%.1f");
    ImGui::SameLine();

    // Mode: 0 = shared render comps, 1 = fresh render comps (experimental)
    static int spawnMode = 0;
    ImGui::SetNextItemWidth(120);
    const char* modeNames[] = { "Shared Visuals", "Fresh Render" };
    ImGui::Combo("##mode", &spawnMode, modeNames, 2);

    if (ImGui::Button("Spawn Clone") && IsWorldValid()) {
        typedef uintptr_t (__cdecl *GetPlayerFn2)();
        auto gp2 = reinterpret_cast<GetPlayerFn2>(addr::fn_GetPlayerCharacter);
        uintptr_t source = gp2();
        if (!source) {
            log::Write("Spawn: no player");
        } else {
            log::Write("=== GRACEFUL SPAWN (mode=%d) ===", spawnMode);
            constexpr int OBJ_SIZE = 1592;  // ClPlayerObj size

            // Step 1: Allocate via game pool
            uint8_t wasDisabled = mem::Read<uint8_t>(addr::pAllocDisabled);
            if (wasDisabled) mem::WriteDirect<uint8_t>(addr::pAllocDisabled, 0);

            typedef uintptr_t (__cdecl *AllocFn)(int, unsigned int);
            auto gameAlloc = reinterpret_cast<AllocFn>(addr::fn_GamePoolAlloc);
            uintptr_t clone = gameAlloc(OBJ_SIZE, 16);

            if (wasDisabled) mem::WriteDirect<uint8_t>(addr::pAllocDisabled, 1);

            if (!clone) {
                log::Write("  FAILED: allocator returned null");
            } else {
                // Step 2: Copy all data from source character
                memcpy(reinterpret_cast<void*>(clone),
                       reinterpret_cast<void*>(source), OBJ_SIZE);

                // Step 3: Reset minimal per-instance pointers
                mem::WriteDirect<uintptr_t>(clone + addr::CHAR_VM_LINK, 0);        // no VM
                mem::WriteDirect<uintptr_t>(clone + addr::CHAR_NEXT, 0);           // linked list

                if (spawnMode == 0) {
                    // Shared mode: keep ALL render infrastructure shared
                    // Only clear +0x13C so sub_4584C0 creates a NEW render pool entry
                    // Scene nodes (+0x9C/+0xA0), activation node (+0x138) stay shared
                    mem::WriteDirect<uintptr_t>(clone + addr::CHAR_RENDER_OBJ, 0);
                } else {
                    // Fresh mode: clear everything (invisible but functional)
                    mem::WriteDirect<uintptr_t>(clone + addr::CHAR_SCENE_GFX_1, 0);
                    mem::WriteDirect<uintptr_t>(clone + addr::CHAR_SCENE_GFX_2, 0);
                    mem::WriteDirect<uintptr_t>(clone + addr::CHAR_ACTIVATION_NODE, 0);
                    mem::WriteDirect<uintptr_t>(clone + addr::CHAR_RENDER_OBJ, 0);
                }

                // Clear player flag
                uint32_t flags = mem::Read<uint32_t>(clone + addr::CHAR_FLAGS);
                mem::WriteDirect<uint32_t>(clone + addr::CHAR_FLAGS, flags & ~0x4u);

                // Step 4: Set position
                float px = mem::Read<float>(source + addr::CHAR_POS_X) + spawnOffsetX;
                float py = mem::Read<float>(source + addr::CHAR_POS_Y);
                float pz = mem::Read<float>(source + addr::CHAR_POS_Z);
                mem::WriteDirect<float>(clone + addr::CHAR_POS_X, px);
                mem::WriteDirect<float>(clone + addr::CHAR_POS_Y, py);
                mem::WriteDirect<float>(clone + addr::CHAR_POS_Z, pz);

                uint32_t renderCount = mem::Read<uint32_t>(clone + addr::CHAR_RENDER_COUNT);
                log::Write("  Clone=0x%X from=0x%X pos=(%.1f,%.1f,%.1f) renderComps=%d",
                    clone, source, px, py, pz, renderCount);

                // Diagnostic: child components at +0x368 (the REAL render system for characters)
                log::Write("  Child components (+0x368, 6 slots):");
                for (int ci = 0; ci < 6; ci++) {
                    uintptr_t child = mem::Read<uintptr_t>(source + 0x368 + ci * 4);
                    if (child) {
                        uintptr_t childVt = mem::Read<uintptr_t>(child);
                        log::Write("    [%d] 0x%X vtable=0x%X", ci, child, childVt);
                    }
                }
                // Key render fields from source
                log::Write("  SRC +0x13C(renderObj)=0x%X +0x138(actNode)=0x%X",
                    mem::Read<uintptr_t>(source + 0x13C),
                    mem::Read<uintptr_t>(source + 0x138));
                log::Write("  SRC +0x9C(gfx1)=0x%X +0xA0(gfx2)=0x%X +0x1B8(scene)=0x%X",
                    mem::Read<uintptr_t>(source + 0x9C),
                    mem::Read<uintptr_t>(source + 0xA0),
                    mem::Read<uintptr_t>(source + 0x1B8));

                // Step 5: Create own render pool entry via RenderObjSetup
                // This registers the clone in the render pipeline independently
                // All scene/visual data stays shared — only the render pool entry is new
                if (spawnMode == 0) {
                    uintptr_t widget = mem::Read<uintptr_t>(clone + addr::CHAR_WIDGET_PTR);
                    if (widget) {
                        typedef void (__thiscall *RenderSetupFn)(void*, uintptr_t);
                        auto renderSetup = reinterpret_cast<RenderSetupFn>(addr::fn_RenderObjSetup);
                        __try {
                            renderSetup(reinterpret_cast<void*>(clone), widget);
                            log::Write("  NEW RenderObj=0x%X (pool entry for clone)",
                                mem::Read<uintptr_t>(clone + addr::CHAR_RENDER_OBJ));
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            log::Write("  WARN: RenderObjSetup exception");
                        }
                    }
                    // Camera dirty flag
                    typedef int (__cdecl *CamDirtyFn)();
                    reinterpret_cast<CamDirtyFn>(addr::fn_CameraDirtyFlag)();
                } else {
                    log::Write("  Fresh mode — no render setup (invisible clone)");
                }

                // Step 7: Insert at END of character linked list
                // (avoids stealing player identity — getPlayer() finds original first)
                uintptr_t tail = mem::Read<uintptr_t>(addr::pCharacterListHead);
                if (!tail) {
                    mem::WriteDirect<uintptr_t>(addr::pCharacterListHead, clone);
                } else {
                    for (int li = 0; li < 64; li++) {
                        uintptr_t next = mem::Read<uintptr_t>(tail + addr::CHAR_NEXT);
                        if (!next) break;
                        tail = next;
                    }
                    mem::WriteDirect<uintptr_t>(tail + addr::CHAR_NEXT, clone);
                }

                log::Write("=== SPAWN COMPLETE: 0x%X (renderComps=%d) ===", clone, renderCount);
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(check log)");
}
