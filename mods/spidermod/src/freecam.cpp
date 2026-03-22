// ============================================================================
// Freecam implementation
// ============================================================================

#include "freecam.h"
#include "menu.h"
#include "sector.h"
#include "addresses.h"
#include "memory.h"
#include <cmath>
#include <cstdio>
#include <windows.h>

namespace freecam {

bool  enabled = false;
float posX = 0, posY = 0, posZ = 0;
float yaw = 0, pitch = 0;
float speed = 0.5f;

float mouseSens = 0.04f;
static float pitchLimit = 1.5f;

// Saved original bytes for restoration
static uint8_t origViewMatrix[5];
static uint8_t origCopyBlock[6];
static uint8_t origMonocle[5];
static bool    hooksInstalled = false;

// DebugCameraManager — engine's own input blocking system
// Constructor registers with "Debug" input group; +0x70 = state (-1=inactive, 0=active)
alignas(16) static uint8_t dbgCamObj[0x80] = {};
static bool    dbgCamCreated = false;

// MMB teleport edge detection
static bool mmbPrev = false;

// Our eye/target data (written by Update, read by Hook 1 asm)
static float fc_eye[3]    = {};
static float fc_target[3] = {};

// ============================================================================
// ASM Hook Trampolines (naked functions for __declspec(naked))
// ============================================================================

// Hook 1: SetViewMatrix — replace eye/target args with our pointers
static uintptr_t hook1_return = 0;
static __declspec(naked) void Hook1_SetViewMatrix() {
    __asm {
        // Check if freecam enabled
        cmp  dword ptr [enabled], 1
        jne  _original

        // Save registers
        push eax
        push ebx

        // Save game eye (arg1 at [esp+0Ch] after 2 pushes + retaddr)
        mov  eax, [esp+0Ch]
        // Replace arg1 with our eye
        lea  ebx, [fc_eye]
        mov  [esp+0Ch], ebx
        // Replace arg2 with our target
        lea  ebx, [fc_target]
        mov  [esp+10h], ebx

        pop  ebx
        pop  eax

    _original:
        // Original: sub esp, 10h; fldz
        sub  esp, 0x10
        fldz
        jmp  [hook1_return]
    }
}

// Hook 3: MonocleUpdate — return 1 without monocle flags
static uintptr_t hook3_return = 0;
static __declspec(naked) void Hook3_MonocleUpdate() {
    __asm {
        cmp  dword ptr [enabled], 1
        jne  _original

        mov  al, 1
        ret  0x0C

    _original:
        sub  esp, 0x60
        push ebx
        push esi
        jmp  [hook3_return]
    }
}

// ============================================================================
// Hook 2: CopyPositionBlock — block writes to camera struct
// ============================================================================
// This needs pCamStruct which is game-specific. For now, we use a simpler
// approach: NOP the call sites or use a flag-based skip.
// TODO: Find pCamStruct base address from camera_obj
static uintptr_t hook2_return = 0;
static __declspec(naked) void Hook2_CopyPositionBlock() {
    __asm {
        cmp  dword ptr [enabled], 1
        jne  _original

        // Skip copy — just return
        ret  4

    _original:
        // Original: push esi; push edi; mov edi,[esp+0Ch]
        push esi
        push edi
        mov  edi, [esp+0x0C]
        jmp  [hook2_return]
    }
}

// ============================================================================
// Extract yaw/pitch from camera rotation matrix
// ============================================================================
static void ExtractAngles(uintptr_t camStruct) {
    float r00 = mem::Read<float>(camStruct + 0x00);
    float r10 = mem::Read<float>(camStruct + 0x10);
    float r24 = mem::Read<float>(camStruct + 0x24);
    float r28 = mem::Read<float>(camStruct + 0x28);
    yaw   = atan2f(r10, r00);
    pitch = atan2f(r24, r28);
}

// ============================================================================
// Toggle freecam
// ============================================================================
void Toggle() {
    enabled = !enabled;

    if (enabled) {
        // Read current camera position
        uintptr_t camObj = mem::Deref(addr::pCameraObj);
        if (!camObj) { enabled = false; return; }

        posX = mem::Read<float>(camObj + addr::CAM_POS_X);
        posY = mem::Read<float>(camObj + addr::CAM_POS_Y);
        posZ = mem::Read<float>(camObj + addr::CAM_POS_Z);

        // Try to extract angles from camera struct at camObj
        ExtractAngles(camObj + addr::CAM_POS_X - addr::CAMSTRUCT_POS_X);

        // Initialize eye/target BEFORE hooks activate (prevents zero-vector crash)
        {
            float cp = cosf(pitch), sp = sinf(pitch);
            float cy = cosf(yaw),   sy = sinf(yaw);
            fc_eye[0] = posX;
            fc_eye[1] = posY;
            fc_eye[2] = posZ;
            fc_target[0] = posX + sy * cp;
            fc_target[1] = posY + cy * cp;
            fc_target[2] = posZ - sp;
        }

        // Install hooks
        if (!hooksInstalled) {
            // Hook 1: SetViewMatrix
            memcpy(origViewMatrix, (void*)addr::fn_SetViewMatrix, 5);
            hook1_return = addr::fn_SetViewMatrix + 5;
            mem::WriteJmp(addr::fn_SetViewMatrix, (uintptr_t)&Hook1_SetViewMatrix);

            // Hook 2: CopyPositionBlock
            memcpy(origCopyBlock, (void*)addr::fn_CopyPositionBlock, 6);
            hook2_return = addr::fn_CopyPositionBlock + 6;
            mem::WriteJmpPad(addr::fn_CopyPositionBlock, (uintptr_t)&Hook2_CopyPositionBlock, 6);

            // Hook 3: MonocleUpdate
            memcpy(origMonocle, (void*)addr::fn_MonocleUpdate, 5);
            hook3_return = addr::fn_MonocleUpdate + 5;
            mem::WriteJmp(addr::fn_MonocleUpdate, (uintptr_t)&Hook3_MonocleUpdate);

            hooksInstalled = true;
        }

        // Hide HUD — cheat flag bit 2, stored in per-player array via pointer
        {
            uintptr_t flagsPtr = mem::Read<uintptr_t>(addr::pCheatFlagsArray);
            if (flagsPtr && flagsPtr > 0x10000 && flagsPtr < 0x7FFFFFFF) {
                uint32_t flags = mem::Read<uint32_t>(flagsPtr);
                if ((flags & 4) == 0)
                    mem::Write<uint32_t>(flagsPtr, flags | 4);
            }
        }

        // Block character input via engine's DebugCameraManager
        if (!dbgCamCreated) {
            memset(dbgCamObj, 0, sizeof(dbgCamObj));
            typedef void (__thiscall *CtorFn)(void*);
            auto ctor = reinterpret_cast<CtorFn>(addr::fn_DebugCamCtor);
            ctor(dbgCamObj);
            dbgCamCreated = true;
        }
        *reinterpret_cast<uint32_t*>(dbgCamObj + 0x70) = 0; // activate → blocks input

    } else {
        // Restore hooks
        if (hooksInstalled) {
            mem::Restore(addr::fn_SetViewMatrix, origViewMatrix, 5);
            mem::Restore(addr::fn_CopyPositionBlock, origCopyBlock, 6);
            mem::Restore(addr::fn_MonocleUpdate, origMonocle, 5);
            hooksInstalled = false;
        }

        // Restore character input
        if (dbgCamCreated)
            *reinterpret_cast<uint32_t*>(dbgCamObj + 0x70) = 0xFFFFFFFF; // deactivate

        // Restore HUD
        {
            uintptr_t flagsPtr = mem::Read<uintptr_t>(addr::pCheatFlagsArray);
            if (flagsPtr && flagsPtr > 0x10000 && flagsPtr < 0x7FFFFFFF) {
                uint32_t flags = mem::Read<uint32_t>(flagsPtr);
                if (flags & 4)
                    mem::Write<uint32_t>(flagsPtr, flags & ~4u);
            }
        }

        SaveConfig();
    }
}

// ============================================================================
// Per-frame update
// ============================================================================
void Update() {
    if (!enabled) return;

    // --- Mouse rotation (paused when menu is open) ---
    if (!menu::visible) {
        float mdx = mem::Read<float>(addr::MOUSE_DX);
        float mdy = mem::Read<float>(addr::MOUSE_DY);
        yaw   -= mdx * mouseSens;
        pitch += mdy * mouseSens;
    }

    // --- Arrow key rotation (always, fallback for mouse) ---
    static const float ROT_SPEED = 0.04f;
    if (GetAsyncKeyState(VK_LEFT)  & 0x8000) yaw   -= ROT_SPEED;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) yaw   += ROT_SPEED;
    if (GetAsyncKeyState(VK_UP)    & 0x8000) pitch -= ROT_SPEED;
    if (GetAsyncKeyState(VK_DOWN)  & 0x8000) pitch += ROT_SPEED;

    if (pitch >  pitchLimit) pitch =  pitchLimit;
    if (pitch < -pitchLimit) pitch = -pitchLimit;

    // --- Movement ---
    float spd = speed;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) spd *= 3.0f;

    float cp = cosf(pitch), sp = sinf(pitch);
    float cy = cosf(yaw),   sy = sinf(yaw);
    float fwdX = sy * cp, fwdY = cy * cp, fwdZ = -sp;
    float rgtX = cy,      rgtY = -sy;

    if (GetAsyncKeyState('W')       & 0x8000) { posX += spd*fwdX; posY += spd*fwdY; posZ += spd*fwdZ; }
    if (GetAsyncKeyState('S')       & 0x8000) { posX -= spd*fwdX; posY -= spd*fwdY; posZ -= spd*fwdZ; }
    if (GetAsyncKeyState('A')       & 0x8000) { posX -= spd*rgtX; posY -= spd*rgtY; }
    if (GetAsyncKeyState('D')       & 0x8000) { posX += spd*rgtX; posY += spd*rgtY; }
    if (GetAsyncKeyState(VK_SPACE)  & 0x8000)   posZ += spd;
    if (GetAsyncKeyState(VK_CONTROL)& 0x8000)   posZ -= spd;

    // Speed controls
    if (GetAsyncKeyState(VK_ADD)     & 0x8000) speed *= 1.02f;
    if (GetAsyncKeyState(VK_SUBTRACT)& 0x8000) speed /= 1.02f;
    if (GetAsyncKeyState(VK_END)     & 0x8000) speed = 0.5f;

    // --- Write eye/target for Hook 1 ---
    fc_eye[0] = posX;
    fc_eye[1] = posY;
    fc_eye[2] = posZ;
    fc_target[0] = posX + sy * cp;
    fc_target[1] = posY + cy * cp;
    fc_target[2] = posZ - sp;

    // --- MMB: teleport player to camera ---
    bool mmbNow = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    if (mmbNow && !mmbPrev) {
        typedef uintptr_t (__cdecl *GetPlayerFn)();
        auto getPlayer = reinterpret_cast<GetPlayerFn>(addr::fn_GetPlayerCharacter);
        uintptr_t player = getPlayer();
        if (player) {
            mem::WriteDirect<float>(player + addr::PLAYER_POS_X, posX);
            mem::WriteDirect<float>(player + addr::PLAYER_POS_Y, posY);
            mem::WriteDirect<float>(player + addr::PLAYER_POS_Z, posZ);
        }
    }
    mmbPrev = mmbNow;

    // --- Sector tracking ---
    uintptr_t camObj = mem::Deref(addr::pCameraObj);
    if (camObj) {
        // Write position to camera_obj
        mem::WriteDirect<float>(camObj + addr::CAM_POS_X, posX);
        mem::WriteDirect<float>(camObj + addr::CAM_POS_Y, posY);
        mem::WriteDirect<float>(camObj + addr::CAM_POS_Z, posZ);

        // AABB sector lookup → write to camera_obj+0x788
        int sector = sector::FindByAABB(posX, posY, posZ);
        if (sector >= 0)
            mem::WriteDirect<int>(camObj + addr::CAM_SECTOR, sector);
    }
}

// ============================================================================
// Config persistence
// ============================================================================
void SaveConfig() {
    FILE* f = fopen("spidermod.cfg", "w");
    if (!f) return;
    fprintf(f, "speed=%.4f\n", speed);
    fprintf(f, "mouseSens=%.4f\n", mouseSens);
    fclose(f);
}

void LoadConfig() {
    FILE* f = fopen("spidermod.cfg", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        float val;
        if (sscanf(line, "speed=%f", &val) == 1) speed = val;
        if (sscanf(line, "mouseSens=%f", &val) == 1) mouseSens = val;
    }
    fclose(f);
}

} // namespace freecam
