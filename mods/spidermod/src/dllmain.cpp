// ============================================================================
// SpiderMod — ASI mod for The Spiderwick Chronicles (2008)
// DllMain + EndScene hook loop (ImGui-based)
// ============================================================================

#include "d3d_hook.h"
#include "menu.h"
#include "menu_common.h"
#include "freecam.h"
#include "addresses.h"
#include "memory.h"
#include "registry_redirect.h"
#include "log.h"
#include <MinHook.h>
#include <windows.h>

// Avoid math.h — conflicts with log namespace
extern "C" float sinf(float);
extern "C" float cosf(float);
extern "C" float atan2f(float, float);
static inline float my_tanf(float x) { return sinf(x) / cosf(x); }

// ============================================================================
// sauSetPosition / sauSetRotation capture — logs every VM prop placement
// ============================================================================
// PropPlacement struct declared in menu_common.h
PropPlacement g_propPlacements[MAX_PROP_PLACEMENTS] = {};
volatile long g_propPlacementCount = 0;
bool g_propCaptureActive = true;  // capture during level load

typedef int (__thiscall *sauSetPosition_t)(void*);
static sauSetPosition_t OrigSetPosition = nullptr;

typedef int (__thiscall *sauSetRotation_t)(void*);
static sauSetRotation_t OrigSetRotation = nullptr;

// VMPopVec3 — pops 3 floats from VM eval stack
typedef void (__cdecl *VMPopVec3_t)(float* out);
static auto pVMPopVec3 = reinterpret_cast<VMPopVec3_t>(addr::fn_VMPopVec3);

// ============================================================================
// sauCharacterInit capture — logs texture name strings from VM stack
// ============================================================================
typedef int (__thiscall *sauCharacterInit_t)(void*);
static sauCharacterInit_t OrigCharacterInit = nullptr;

// Engine's VMPopString
typedef void (__cdecl *VMPopString_t)(void* outPtr);
static auto pVMPopString = reinterpret_cast<VMPopString_t>(addr::fn_VMPopString);

// Engine's HashString
typedef unsigned int (__cdecl *HashString_t)(const char*);
static auto pHashString = reinterpret_cast<HashString_t>(addr::fn_HashString);

// ============================================================================
// Asset factory hooks — capture PCIM + NM40 creation with AWAD offsets
// ============================================================================
// When the engine loads assets from AWAD, it calls the factory for each type.
// The factory receives a pointer INTO the decompressed AWAD data.
// By tracking which AWAD buffer each pointer belongs to, we can compute the
// data offset and match it against the AWAD TOC.

// ============================================================================
// RENDERFUNC_NOAM_DIFFUSE hook — capture render-time texture bindings
// __cdecl(renderCmd*). Standard calling convention, no naked asm needed.
// renderCmd[5] = renderCtx, *(renderCtx+4) = obj with textures at +108/+112
// ============================================================================
// RENDERFUNC_NOAM_DIFFUSE hook — marks when character rendering begins
typedef int (__cdecl *RenderFuncNoamDiffuse_t)(uint32_t* renderCmd);
static RenderFuncNoamDiffuse_t OrigRenderFuncNoamDiffuse = nullptr;
static volatile long g_skinLogCount = 0;
static volatile bool g_inNoamRender = false;
static uint32_t g_curNoamMesh = 0;  // current NM40 mesh being rendered

static int __cdecl HookedRenderFuncNoamDiffuse(uint32_t* renderCmd) {
    // Mark that we're inside NOAM character rendering
    uint32_t meshAddr = 0;
    if (renderCmd) {
        __try {
            uint32_t ctx = renderCmd[5];
            if (ctx) meshAddr = *(uint32_t*)(ctx + 60);
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    g_curNoamMesh = meshAddr;
    g_inNoamRender = true;
    int result = OrigRenderFuncNoamDiffuse(renderCmd);
    g_inNoamRender = false;
    g_curNoamMesh = 0;
    return result;
}

// IDirect3DDevice9::SetTexture hook — track current stage 0 texture
typedef HRESULT (WINAPI *SetTexture_t)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
static SetTexture_t OrigSetTexture = nullptr;
static uint32_t g_curTexture = 0;  // last texture set on stage 0

static HRESULT WINAPI HookedSetTexture(IDirect3DDevice9* dev, DWORD stage,
                                        IDirect3DBaseTexture9* tex) {
    if (stage == 0)
        g_curTexture = (uint32_t)tex;
    return OrigSetTexture(dev, stage, tex);
}

// IDirect3DDevice9::DrawIndexedPrimitive hook — capture (texture, geometry) pairs
typedef HRESULT (WINAPI *DrawIndexedPrimitive_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE,
    INT, UINT, UINT, UINT, UINT);
static DrawIndexedPrimitive_t OrigDrawIndexedPrimitive = nullptr;

// Dedup table: store unique (texture, vtxCount, idxCount) combos
struct DrawEntry { uint32_t tex; uint32_t vtxCount; uint32_t primCount; uint32_t mesh; };
static DrawEntry g_drawLog[2048];
static volatile long g_drawLogCount = 0;
static bool g_drawLogFlushed = false;

static HRESULT WINAPI HookedDrawIndexedPrimitive(IDirect3DDevice9* dev,
    D3DPRIMITIVETYPE type, INT baseVtx, UINT minVtxIdx, UINT numVtx,
    UINT startIdx, UINT primCount) {
    // Log unique (texture, numVtx, primCount) combos — covers both characters and props
    if (g_curTexture && g_drawLogCount < 2048) {
        // Check for duplicate
        long cnt = g_drawLogCount;
        bool found = false;
        for (long i = 0; i < cnt; i++) {
            if (g_drawLog[i].tex == g_curTexture &&
                g_drawLog[i].vtxCount == numVtx &&
                g_drawLog[i].primCount == primCount) {
                found = true;
                break;
            }
        }
        if (!found) {
            long idx = InterlockedIncrement(&g_drawLogCount) - 1;
            if (idx < 2048) {
                g_drawLog[idx] = {g_curTexture, numVtx, primCount,
                                  g_inNoamRender ? g_curNoamMesh : 0};
            }
        }
    }
    return OrigDrawIndexedPrimitive(dev, type, baseVtx, minVtxIdx, numVtx, startIdx, primCount);
}

// Flush draw log to file (called periodically from EndScene)
static void FlushDrawLog() {
    if (g_drawLogFlushed || g_drawLogCount == 0) return;
    long cnt = g_drawLogCount;
    if (cnt < 20) return;  // wait until we have enough data

    g_drawLogFlushed = true;
    FILE* f = fopen("spiderview_texmap.txt", "a");
    if (!f) return;

    fprintf(f, "\n=== DRAW LOG: %d unique (tex, vtx, prim) combos ===\n", (int)cnt);
    for (long i = 0; i < cnt && i < 2048; i++) {
        auto& e = g_drawLog[i];
        // Get texture dimensions
        D3DSURFACE_DESC desc = {};
        IDirect3DTexture9* t = (IDirect3DTexture9*)e.tex;
        __try { t->GetLevelDesc(0, &desc); } __except(EXCEPTION_EXECUTE_HANDLER) {}

        if (e.mesh)
            fprintf(f, "DRAW tex=0x%08X %ux%u vtx=%u prim=%u noam=0x%08X\n",
                    e.tex, desc.Width, desc.Height, e.vtxCount, e.primCount, e.mesh);
        else
            fprintf(f, "DRAW tex=0x%08X %ux%u vtx=%u prim=%u\n",
                    e.tex, desc.Width, desc.Height, e.vtxCount, e.primCount);
    }
    fclose(f);
}

// IDirect3DDevice9::CreateTexture hook — maps D3D handles to PCIM creation
typedef HRESULT (WINAPI *CreateTexture_t)(IDirect3DDevice9*, UINT, UINT, UINT,
    DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
static CreateTexture_t OrigCreateTexture = nullptr;
static volatile long g_createTexLogCount = 0;

// Track last PCIM_CREATE to correlate with CreateTexture
static uint32_t g_lastPcimPtr = 0;
static uint32_t g_lastPcimW = 0;
static uint32_t g_lastPcimH = 0;

static HRESULT WINAPI HookedCreateTexture(IDirect3DDevice9* dev, UINT w, UINT h,
    UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
    IDirect3DTexture9** ppTex, HANDLE* shared) {
    HRESULT hr = OrigCreateTexture(dev, w, h, levels, usage, fmt, pool, ppTex, shared);

    // Match with last PCIM_CREATE by dimensions
    if (SUCCEEDED(hr) && ppTex && *ppTex && g_lastPcimPtr &&
        w == g_lastPcimW && h == g_lastPcimH && g_createTexLogCount < 500) {
        InterlockedIncrement(&g_createTexLogCount);

        FILE* f = fopen("spiderview_texmap.txt", "a");
        if (f) {
            fprintf(f, "TEX_MAP pcim=0x%08X %ux%u → d3d=0x%08X\n",
                    g_lastPcimPtr, w, h, (uint32_t)*ppTex);
            fclose(f);
        }
        g_lastPcimPtr = 0;  // consumed
    }
    return hr;
}

typedef void* (__cdecl *PCIM_Factory_t)(int, int);
static PCIM_Factory_t OrigPCIMFactory = nullptr;

// AWAD base address — found by scanning backwards from asset data
static uint32_t g_awadBase = 0;

// Track PCIM creations for correlation with render-time texture capture
struct AssetCreate { uint32_t dataPtr; uint16_t bones; uint32_t w, h; };
static AssetCreate g_lastPCIMs[64] = {};
static int g_pcimCount = 0;

static void* __cdecl HookedPCIMFactory(int a1, int a2) {
    __try {
        if (a2 && !IsBadReadPtr((void*)a2, 0xA4) && *(uint32_t*)a2 == 0x4D494350) {
            uint32_t w = *(uint32_t*)(a2 + 0x9C);
            uint32_t h = *(uint32_t*)(a2 + 0xA0);
            // Log large PCIMs (likely character textures)
            if (w >= 256 || h >= 256) {
                FILE* f = fopen("spiderview_texmap.txt", "a");
                if (f) {
                    fprintf(f, "PCIM_CREATE ptr=0x%08X %ux%u\n", (uint32_t)a2, w, h);
                    fclose(f);
                }
            }
            if (g_pcimCount < 64) {
                g_lastPCIMs[g_pcimCount] = {(uint32_t)a2, 0, w, h};
                g_pcimCount++;
            }
            // Track for CreateTexture correlation
            g_lastPcimPtr = (uint32_t)a2;
            g_lastPcimW = w;
            g_lastPcimH = h;

            // Find AWAD base by scanning backwards for "AWAD" magic (0x44415741)
            if (!g_awadBase) {
                for (uint32_t scan = (uint32_t)a2 & ~0xFFF; scan > (uint32_t)a2 - 0x4000000 && scan > 0x10000; scan -= 0x1000) {
                    if (!IsBadReadPtr((void*)scan, 8) && *(uint32_t*)scan == 0x44415741) {
                        g_awadBase = scan;
                        FILE* bf = fopen("spiderview_texmap.txt", "a");
                        if (bf) { fprintf(bf, "AWAD_BASE=0x%08X\n", scan); fclose(bf); }
                        break;
                    }
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return OrigPCIMFactory(a1, a2);
}

// NM40_Factory and NoamFigure_Init hooks REMOVED — their data has been captured.
// NM40 info available from ClNoamActor_Init; texA/texB confirmed secondary/NULL.
// RENDERFUNC_NOAM_SKIN hook now captures actual render-time texture bindings.

// ClNoamActor_Init hook — captures the full descriptor array (NM40 ref + texture hashes)
typedef int (__thiscall *ClNoamActor_Init_t)(void*, void* a2, int a3, int a4);
static ClNoamActor_Init_t OrigNoamActorInit = nullptr;

static int __fastcall HookedNoamActorInit(void* thisObj, void* /*edx*/, void* a2, int a3, int a4) {
    __try {
        uint32_t* desc = (uint32_t*)a2;
        uint32_t meshAssetObj = desc[1];
        uint32_t texAObj      = desc[13]; // texture asset object ptr (NOT hash)
        uint32_t texBObj      = desc[14];

        uint32_t nm40DataPtr = 0;
        uint16_t numBones = 0;
        if (meshAssetObj && !IsBadReadPtr((void*)meshAssetObj, 8)) {
            nm40DataPtr = *(uint32_t*)(meshAssetObj + 4);
            if (nm40DataPtr && !IsBadReadPtr((void*)nm40DataPtr, 0x40) &&
                *(uint32_t*)nm40DataPtr == 0x30344D4E)
                numBones = *(uint16_t*)(nm40DataPtr + 8);
        }

        // Read PCIM data pointer from texture asset objects
        uint32_t texADataPtr = 0, texBDataPtr = 0;
        if (texAObj && !IsBadReadPtr((void*)texAObj, 8))
            texADataPtr = *(uint32_t*)(texAObj + 4);
        if (texBObj && !IsBadReadPtr((void*)texBObj, 8))
            texBDataPtr = *(uint32_t*)(texBObj + 4);

        // Compute AWAD-relative offset: texADataPtr - nm40DataPtr base
        // Both come from the same AWAD, so their difference = offset difference
        int32_t texARelative = (texADataPtr && nm40DataPtr) ? (int32_t)(texADataPtr - nm40DataPtr) : 0;

        char buf[512];
        snprintf(buf, sizeof(buf),
            "ClNoamActor_Init: nm40=0x%08X(%dbones) texA=0x%08X(off=%+d) texB=0x%08X",
            nm40DataPtr, numBones, texADataPtr, texARelative, texBDataPtr);
        log::Write(buf);

        // Dump ALL 22 descriptor slots to find which contains the main diffuse PCIM
        FILE* f = fopen("spiderview_texmap.txt", "a");
        if (f) {
            fprintf(f, "DESC nm40=0x%08X bones=%d\n", nm40DataPtr, numBones);
            // Dump all 22 slots — each is an asset object ptr, *(ptr+4) = data ptr
            for (int si = 0; si < 22; si++) {
                uint32_t slotObj = desc[si];
                uint32_t dataPtr = 0;
                uint32_t magic = 0;
                uint32_t w = 0, h = 0;
                if (slotObj && !IsBadReadPtr((void*)slotObj, 8)) {
                    dataPtr = *(uint32_t*)(slotObj + 4);
                    if (dataPtr && !IsBadReadPtr((void*)dataPtr, 0xA4)) {
                        magic = *(uint32_t*)dataPtr;
                        if (magic == 0x4D494350) { // PCIM
                            w = *(uint32_t*)(dataPtr + 0x9C);
                            h = *(uint32_t*)(dataPtr + 0xA0);
                        }
                    }
                }
                if (slotObj)
                    fprintf(f, "  [%2d] obj=0x%08X data=0x%08X magic=%c%c%c%c",
                            si, slotObj, dataPtr,
                            (magic>>0)&0xFF ? (magic>>0)&0xFF : '.',
                            (magic>>8)&0xFF ? (magic>>8)&0xFF : '.',
                            (magic>>16)&0xFF ? (magic>>16)&0xFF : '.',
                            (magic>>24)&0xFF ? (magic>>24)&0xFF : '.');
                else
                    fprintf(f, "  [%2d] NULL", si);
                if (w && h) fprintf(f, " %ux%u", w, h);
                fprintf(f, "\n");
            }
            fclose(f);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        log::Write("ClNoamActor_Init: exception");
    }

    return OrigNoamActorInit(thisObj, a2, a3, a4);
}

static int __fastcall HookedCharacterInit(void* thisObj, void* /*edx*/) {
    // Save VM stack index so we can peek then restore
    uint32_t savedIdx = *(uint32_t*)addr::pVMStackIndex;

    char* texB = nullptr;
    char* texA = nullptr;
    char* templ = nullptr;
    pVMPopString(&texB);
    pVMPopString(&texA);
    pVMPopString(&templ);

    __try {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "sauCharacterInit: template='%s' texA='%s'(0x%08X) texB='%s'(0x%08X)",
            templ ? templ : "NULL",
            texA ? texA : "NULL", texA ? pHashString(texA) : 0,
            texB ? texB : "NULL", texB ? pHashString(texB) : 0);
        log::Write(buf);

        FILE* f = fopen("spiderview_texmap.txt", "a");
        if (f) {
            fprintf(f, "CHAR %s texA=%s(0x%08X) texB=%s(0x%08X)\n",
                templ ? templ : "?",
                texA ? texA : "?", texA ? pHashString(texA) : 0,
                texB ? texB : "?", texB ? pHashString(texB) : 0);
            fclose(f);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        log::Write("sauCharacterInit: exception");
    }

    *(uint32_t*)addr::pVMStackIndex = savedIdx;
    return OrigCharacterInit(thisObj);
}

static int __fastcall HookedSetPosition(void* thisObj, void* /*edx*/) {
    if (g_propCaptureActive) {
        // Peek at VM stack BEFORE original function pops it
        // Read the position that will be set
        float vec[4] = {};
        // We need to call original (which pops the vec3), but first save the obj ptr
        uintptr_t objAddr = reinterpret_cast<uintptr_t>(thisObj);

        int result = OrigSetPosition(thisObj);

        // After SetPosition, read the actual position from the object (+0x68)
        float px = mem::Read<float>(objAddr + 0x68);
        float py = mem::Read<float>(objAddr + 0x6C);
        float pz = mem::Read<float>(objAddr + 0x70);

        long idx = InterlockedIncrement(&g_propPlacementCount) - 1;
        if (idx < MAX_PROP_PLACEMENTS) {
            // Check if we already have this object
            bool found = false;
            for (long j = 0; j < idx; j++) {
                if (g_propPlacements[j].objPtr == objAddr) {
                    g_propPlacements[j].pos[0] = px;
                    g_propPlacements[j].pos[1] = py;
                    g_propPlacements[j].pos[2] = pz;
                    g_propPlacements[j].hasPos = true;
                    InterlockedDecrement(&g_propPlacementCount); // undo
                    found = true;
                    break;
                }
            }
            if (!found) {
                auto& p = g_propPlacements[idx];
                p.objPtr = objAddr;
                p.pos[0] = px; p.pos[1] = py; p.pos[2] = pz;
                p.hasPos = true;
                p.hasRot = false;
                p.name[0] = 0;
                // Read class name via VM object back-pointer:
                // nativeObj+0xA8 → VM object, vmObj+12 → class name char*
                __try {
                    uintptr_t vmObj = mem::Read<uintptr_t>(objAddr + 0xA8);
                    if (vmObj > 0x10000 && vmObj < 0x7FFFFFFF) {
                        uintptr_t nameStr = mem::Read<uintptr_t>(vmObj + 12);
                        if (nameStr > 0x10000 && nameStr < 0x7FFFFFFF) {
                            for (int ci = 0; ci < 47; ci++) {
                                uint8_t c = mem::Read<uint8_t>(nameStr + ci);
                                if (c == 0 || c < 0x20 || c > 0x7E) break;
                                p.name[ci] = (char)c;
                                p.name[ci+1] = 0;
                            }
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
        }
        return result;
    }
    return OrigSetPosition(thisObj);
}

static int __fastcall HookedSetRotation(void* thisObj, void* /*edx*/) {
    if (g_propCaptureActive) {
        uintptr_t objAddr = reinterpret_cast<uintptr_t>(thisObj);
        int result = OrigSetRotation(thisObj);

        // Read rotation from object (+0x78)
        float rx = mem::Read<float>(objAddr + 0x78);
        float ry = mem::Read<float>(objAddr + 0x7C);
        float rz = mem::Read<float>(objAddr + 0x80);

        // Find or create entry
        long cnt = g_propPlacementCount;
        if (cnt > MAX_PROP_PLACEMENTS) cnt = MAX_PROP_PLACEMENTS;
        for (long j = 0; j < cnt; j++) {
            if (g_propPlacements[j].objPtr == objAddr) {
                g_propPlacements[j].rot[0] = rx;
                g_propPlacements[j].rot[1] = ry;
                g_propPlacements[j].rot[2] = rz;
                g_propPlacements[j].hasRot = true;
                return result;
            }
        }
        // New entry (rotation without prior position)
        long idx = InterlockedIncrement(&g_propPlacementCount) - 1;
        if (idx < MAX_PROP_PLACEMENTS) {
            auto& p = g_propPlacements[idx];
            p.objPtr = objAddr;
            p.pos[0] = p.pos[1] = p.pos[2] = 0;
            p.rot[0] = rx; p.rot[1] = ry; p.rot[2] = rz;
            p.hasPos = false;
            p.hasRot = true;
            p.name[0] = 0;
        }
        return result;
    }
    return OrigSetRotation(thisObj);
}

// ============================================================================
// Geometry instance capture — hook GeomInstance_Init (sub_5851D0)
// Called once per geometry instance during sector load (487 times for GroundsD)
// ecx = instance pointer with 4x4 world matrix at +0x00
// ============================================================================
// GeomInstance struct in menu_common.h
GeomInstance g_geomInstances[MAX_GEOM_INSTANCES] = {};
volatile long g_geomInstanceCount = 0;
bool g_geomCaptureActive = true;
uintptr_t g_pcwbBase = 0;

typedef int (__thiscall *GeomInstanceInit_t)(void*);
static GeomInstanceInit_t OrigGeomInstanceInit = nullptr;

static int __fastcall HookedGeomInstanceInit(void* thisObj, void* /*edx*/) {
    int result = OrigGeomInstanceInit(thisObj);

    if (g_geomCaptureActive) {
        long idx = InterlockedIncrement(&g_geomInstanceCount) - 1;
        if (idx < MAX_GEOM_INSTANCES) {
            uintptr_t a = reinterpret_cast<uintptr_t>(thisObj);
            auto& gi = g_geomInstances[idx];
            gi.addr = a;
            gi.vc = 0; gi.ic = 0; gi.flags = 0;
            __try {
                for (int i = 0; i < 16; i++)
                    gi.worldMatrix[i] = mem::Read<float>(a + i * 4);
                gi.vc = mem::Read<uint32_t>(a + 0x70);
                gi.ic = mem::Read<uint32_t>(a + 0x74);
                gi.flags = mem::Read<uint32_t>(a + 0x7C);

                // On first capture: find PCWB base by scanning backward for PCRD then PCWB
                if (idx == 0 && g_pcwbBase == 0) {
                    // First: find nearest PCRD header backward (confirms we're in PCWB data)
                    uintptr_t pcrdAddr = 0;
                    for (uintptr_t scan = a; scan > a - 0x100000 && scan > 0x10000; scan -= 4) {
                        if (mem::Read<uint32_t>(scan) == 0x44524350 && mem::Read<uint32_t>(scan + 4) == 2) {
                            pcrdAddr = scan;
                            break;
                        }
                    }
                    // Then: scan further back for PCWB magic
                    if (pcrdAddr) {
                        for (uintptr_t scan = pcrdAddr; scan > pcrdAddr - 0x2000000 && scan > 0x10000; scan -= 4) {
                            if (mem::Read<uint32_t>(scan) == 0x42574350) { // PCWB
                                uint32_t ver = mem::Read<uint32_t>(scan + 4);
                                uint32_t fsize = mem::Read<uint32_t>(scan + 0x0C);
                                if (ver <= 20 && fsize > 0x100000 && fsize < 0x10000000) {
                                    if (a - scan < fsize) { // instance is inside this buffer
                                        g_pcwbBase = scan;
                                        log::Write("PCWB base found: 0x%X (size=0x%X, inst offset=0x%X)",
                                                   scan, fsize, (unsigned)(a - scan));
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                for (int i = 0; i < 16; i++)
                    gi.worldMatrix[i] = 0.0f;
            }
        }
    }
    return result;
}

// ============================================================================
// Deferred hot-switch — executes from game update context (not EndScene)
// ============================================================================
volatile int g_pendingSwitchType = 0;   // 0=none, 1=Jared, 2=Simon, 3=Mallory
volatile uintptr_t g_pendingSwitchTarget = 0;  // direct char address (if set, overrides type lookup)

// Deferred VM spawn — must run from game update context for proper render setup
volatile bool g_pendingVMSpawn = false;
volatile uintptr_t g_vmSpawnSource = 0;  // source character to clone from

// Auto-load level — skip Shell, load directly into specified level
char g_autoLoadLevel[64] = {};  // e.g. "GroundsD", "" = disabled
bool g_autoLoadTriggered = false;

// Camera overrides (applied each frame from update hook)
bool  g_cameraOverride = false;
float g_cameraFov      = 45.0f;   // degrees
float g_cameraNear     = 2.0f;
float g_cameraFar      = 1024.0f;

// VP matrix + viewport — read directly from CameraRender object each frame
float g_vpMatrix[16]   = {};
bool  g_matricesValid  = false;
short g_vpWidth = 0, g_vpHeight = 0;

// Forward declare (defined after HookedCameraSectorUpdate)
static void SaveHashNames();

// Original CameraSectorUpdate function pointer (set by MinHook)
typedef int (__cdecl *CameraSectorUpdateFn)(float);
static CameraSectorUpdateFn OrigCameraSectorUpdate = nullptr;

static int __cdecl HookedCameraSectorUpdate(float arg0) {
    int switchType = g_pendingSwitchType;
    uintptr_t directTarget = g_pendingSwitchTarget;

    if (switchType >= 1 && switchType <= 3) {
        g_pendingSwitchType = 0;
        g_pendingSwitchTarget = 0;

        uintptr_t targetChar = directTarget;  // use direct address if set by table button

        if (!targetChar) {
            // Fallback: find by widget hash (legacy button path)
            typedef int (__cdecl *HashFn)(const char*);
            auto hashStr = reinterpret_cast<HashFn>(addr::fn_HashString);
            const char* widgetName = nullptr;
            if (switchType == 1) widgetName = "Jared";
            else if (switchType == 2) widgetName = "Mallory";
            else if (switchType == 3) widgetName = "Simon";
            int targetHash = hashStr(widgetName);

            uintptr_t cur = mem::Read<uintptr_t>(addr::pCharacterListHead);
            for (int i = 0; cur && i < 64; i++) {
                uintptr_t widget = mem::Read<uintptr_t>(cur + addr::CHAR_WIDGET_PTR);
                if (widget && widget > 0x10000 && widget < 0x7FFFFFFF) {
                    int hash = mem::Read<int>(widget + addr::WIDGET_NAME_HASH);
                    if (hash == targetHash) { targetChar = cur; break; }
                }
                cur = mem::Read<uintptr_t>(cur + addr::CHAR_NEXT);
            }
        }

        if (!targetChar) {
            log::Write("HotSwitch[update]: target not found for type %d", switchType);
        } else {
            // Get current player before switch
            typedef uintptr_t (__cdecl *GetPlayerFn)();
            auto getPlayer = reinterpret_cast<GetPlayerFn>(addr::fn_GetPlayerCharacter);
            uintptr_t oldPlayer = getPlayer();

            log::Write("HotSwitch[update]: type=%d target=0x%X old=0x%X", switchType, targetChar, oldPlayer);

            // Step 1: Call OnDispossessed on current player to properly deactivate it
            if (oldPlayer && oldPlayer != targetChar) {
                uintptr_t oldVm = mem::Read<uintptr_t>(oldPlayer + 0xA8);
                uint32_t scriptsLoaded = mem::Read<uint32_t>(0xE561F8);
                if (oldVm && scriptsLoaded) {
                    typedef char (__cdecl *VMCallFn)(int, int);
                    auto vmCall = reinterpret_cast<VMCallFn>(0x52EB40);
                    char r = vmCall(static_cast<int>(oldVm),
                                    reinterpret_cast<int>(const_cast<char*>("OnDispossessed")));
                    log::Write("  VMCall(old, \"OnDispossessed\") = %d", (int)r);
                }
            }

            // Step 2: Patch jz→jmp at 0x4638DE to force native path (not .kallis coop)
            constexpr uintptr_t PATCH_ADDR = 0x4638DE;
            DWORD oldProt;
            VirtualProtect(reinterpret_cast<LPVOID>(PATCH_ADDR), 1, PAGE_EXECUTE_READWRITE, &oldProt);
            uint8_t origByte = *reinterpret_cast<uint8_t*>(PATCH_ADDR);
            *reinterpret_cast<uint8_t*>(PATCH_ADDR) = 0xEB;  // jz(74) → jmp(EB)

            // Step 3: Call vtable[116] with type=1 ALWAYS
            // sub_53A020(type) resolves the player SLOT. On MansionD only slot 1 exists.
            // Passing type=1 ensures slot lookup succeeds → P1 input transfer happens.
            // The function activates `this` (targetChar) regardless of type param.
            uintptr_t vtable = mem::Read<uintptr_t>(targetChar);
            if (vtable && vtable > 0x400000 && vtable < 0x7FFFFFFF) {
                typedef void (__thiscall *SetPlayerTypeFn)(void*, int);
                auto setType = reinterpret_cast<SetPlayerTypeFn>(
                    mem::Read<uintptr_t>(vtable + 116 * 4));
                if (setType) {
                    setType(reinterpret_cast<void*>(targetChar), 1);  // always type=1 for slot lookup
                    log::Write("  vtable[116] called (native path, slot=1)");
                }
            }

            // Restore original byte
            *reinterpret_cast<uint8_t*>(PATCH_ADDR) = origByte;
            VirtualProtect(reinterpret_cast<LPVOID>(PATCH_ADDR), 1, oldProt, &oldProt);
        }
    }

    // Deferred VM spawn — steal VM object from NPC, full pipeline
    if (g_pendingVMSpawn) {
        g_pendingVMSpawn = false;
        uintptr_t source = g_vmSpawnSource;
        g_vmSpawnSource = 0;

        if (source) {
            log::Write("[UpdateCtx] VM Spawn starting...");

            // Find a donor: non-player character with a VM object
            uintptr_t donorChar = 0, donorVm = 0;
            uintptr_t walkCur = mem::Read<uintptr_t>(addr::pCharacterListHead);
            for (int wi = 0; walkCur && wi < 32; wi++) {
                if (walkCur != source) {
                    uintptr_t vm = mem::Read<uintptr_t>(walkCur + addr::CHAR_VM_LINK);
                    if (vm) {
                        uintptr_t wvt = mem::Read<uintptr_t>(walkCur);
                        typedef bool (__thiscall *IsPlayerFn)(void*);
                        auto isPlayer = reinterpret_cast<IsPlayerFn>(
                            mem::Read<uintptr_t>(wvt + addr::VTBL_IS_PLAYER * 4));
                        if (!isPlayer(reinterpret_cast<void*>(walkCur))) {
                            donorChar = walkCur;
                            donorVm = vm;
                            break;
                        }
                        if (!donorChar) { donorChar = walkCur; donorVm = vm; }
                    }
                }
                walkCur = mem::Read<uintptr_t>(walkCur + addr::CHAR_NEXT);
            }

            if (!donorVm) {
                log::Write("[UpdateCtx] No donor VM found!");
            } else {
                // Detach VM from donor character
                log::Write("[UpdateCtx] Donor: char=0x%X vmObj=0x%X", donorChar, donorVm);
                mem::WriteDirect<uintptr_t>(donorChar + addr::CHAR_VM_LINK, 0);
                mem::WriteDirect<uintptr_t>(donorVm + 16, 0);  // clear native link

                // Enable allocator
                uint8_t wasDis = mem::Read<uint8_t>(addr::pAllocDisabled);
                if (wasDis) mem::WriteDirect<uint8_t>(addr::pAllocDisabled, 0);

                // GetAssetClassId inline
                uintptr_t cDef = mem::Read<uintptr_t>(donorVm + 20);
                uint32_t classId = cDef ? mem::Read<uint32_t>(cDef + 4) : 0;
                log::Write("[UpdateCtx] classId=0x%X classDef=0x%X", classId, cDef);

                // FactoryDispatch — creates the native object for this class
                uintptr_t nativeRaw = 0;
                if (classId) {
                    typedef uintptr_t (__cdecl *FactoryFn)(uint32_t);
                    nativeRaw = reinterpret_cast<FactoryFn>(0x4D6030)(classId);
                    log::Write("[UpdateCtx] Factory=0x%X", nativeRaw);
                }

                // VMInitObject — links donor VM to new native, runs Init
                bool vmOk = false;
                uintptr_t newChar = 0;
                if (nativeRaw) {
                    newChar = nativeRaw - 4;
                    typedef int (__cdecl *VMInitFn)(uintptr_t, uintptr_t);
                    __try {
                        reinterpret_cast<VMInitFn>(0x52EC30)(donorVm, newChar);
                        vmOk = true;
                        log::Write("[UpdateCtx] VMInitObject OK!");
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        log::Write("[UpdateCtx] VMInitObject EXCEPTION");
                        // Undo damage: clear native's VM link
                        mem::WriteDirect<uintptr_t>(newChar + addr::CHAR_VM_LINK, 0);
                    }
                }

                if (wasDis) mem::WriteDirect<uint8_t>(addr::pAllocDisabled, 1);

                if (newChar && vmOk) {
                    // CRITICAL: clear linked list next pointer before insert!
                    mem::WriteDirect<uintptr_t>(newChar + addr::CHAR_NEXT, 0);

                    // Position via vtable[1]
                    float px = mem::Read<float>(source + addr::CHAR_POS_X) + 5.0f;
                    float py = mem::Read<float>(source + addr::CHAR_POS_Y);
                    float pz = mem::Read<float>(source + addr::CHAR_POS_Z);
                    uintptr_t vt = mem::Read<uintptr_t>(newChar);
                    float posVec[4] = { px, py, pz, 0.0f };
                    typedef void (__thiscall *SetPosFn)(void*, float*);
                    reinterpret_cast<SetPosFn>(mem::Read<uintptr_t>(vt + 4))(
                        reinterpret_cast<void*>(newChar), posVec);

                    // vtable[14] = sauActivateObj path (full activation with render setup)
                    typedef void (__thiscall *ActivateFn)(void*);
                    auto fullActivate = reinterpret_cast<ActivateFn>(
                        mem::Read<uintptr_t>(vt + 14 * 4));
                    __try {
                        fullActivate(reinterpret_cast<void*>(newChar));
                        log::Write("[UpdateCtx] vtable[14] FullActivate done");
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        log::Write("[UpdateCtx] vtable[14] EXCEPTION, fallback to vtable[2]");
                        auto lightActivate = reinterpret_cast<ActivateFn>(
                            mem::Read<uintptr_t>(vt + 2 * 4));
                        lightActivate(reinterpret_cast<void*>(newChar));
                    }

                    // Clear player flag
                    uint32_t fl = mem::Read<uint32_t>(newChar + addr::CHAR_FLAGS);
                    mem::WriteDirect<uint32_t>(newChar + addr::CHAR_FLAGS, fl & ~0x4u);

                    // Check if FullActivate already inserted into list
                    bool alreadyInList = false;
                    uintptr_t chk = mem::Read<uintptr_t>(addr::pCharacterListHead);
                    for (int ci2 = 0; chk && ci2 < 64; ci2++) {
                        if (chk == newChar) { alreadyInList = true; break; }
                        chk = mem::Read<uintptr_t>(chk + addr::CHAR_NEXT);
                    }
                    if (alreadyInList) {
                        log::Write("[UpdateCtx] Already in char list — skip insert");
                    } else {
                        mem::WriteDirect<uintptr_t>(newChar + addr::CHAR_NEXT, 0);
                        uintptr_t tail = mem::Read<uintptr_t>(addr::pCharacterListHead);
                        for (int ti = 0; tail && ti < 64; ti++) {
                            uintptr_t nx = mem::Read<uintptr_t>(tail + addr::CHAR_NEXT);
                            if (!nx) break;
                            tail = nx;
                        }
                        if (tail) mem::WriteDirect<uintptr_t>(tail + addr::CHAR_NEXT, newChar);
                        log::Write("[UpdateCtx] Inserted at end of list");
                    }

                        log::Write("[UpdateCtx] SPAWN OK: 0x%X at (%.1f,%.1f,%.1f)", newChar, px, py, pz);
                        log::Write("[UpdateCtx]   +13C=0x%X +138=0x%X +9C=0x%X +1B8=0x%X",
                            mem::Read<uintptr_t>(newChar + 0x13C),
                            mem::Read<uintptr_t>(newChar + 0x138),
                            mem::Read<uintptr_t>(newChar + 0x9C),
                            mem::Read<uintptr_t>(newChar + 0x1B8));
                        log::Write("[UpdateCtx]   children: %X %X %X %X %X %X",
                            mem::Read<uintptr_t>(newChar + 0x368),
                            mem::Read<uintptr_t>(newChar + 0x36C),
                            mem::Read<uintptr_t>(newChar + 0x370),
                            mem::Read<uintptr_t>(newChar + 0x374),
                            mem::Read<uintptr_t>(newChar + 0x378),
                            mem::Read<uintptr_t>(newChar + 0x37C));
                        log::Write("[UpdateCtx]   flags=0x%X renderComps=%d vmLink=0x%X",
                            mem::Read<uint32_t>(newChar + 0x1CC),
                            mem::Read<uint32_t>(newChar + 0x4D8),
                            mem::Read<uintptr_t>(newChar + 0xA8));
                } else if (newChar) {
                    log::Write("[UpdateCtx] FAILED — restoring donor VM to original");
                    // Restore donor
                    mem::WriteDirect<uintptr_t>(donorVm + 16, donorChar);
                    mem::WriteDirect<uintptr_t>(donorChar + addr::CHAR_VM_LINK, donorVm);
                }
            }
        }
    }

    // Read VP matrix + viewport from CameraRender object (engine's own pre-computed data)
    {
        uintptr_t camObj = mem::Deref(addr::pCameraObj);
        if (camObj) {
            for (int i = 0; i < 16; i++)
                g_vpMatrix[i] = mem::Read<float>(camObj + 0x448 + i * 4);
            g_vpWidth  = mem::Read<short>(camObj + 0x6C8);
            g_vpHeight = mem::Read<short>(camObj + 0x6CA);
            g_matricesValid = (g_vpWidth > 0 && g_vpHeight > 0);
        }
    }


    // Apply camera overrides every frame — call SetFovAndClip to rebuild projection
    if (g_cameraOverride) {
        uintptr_t camObj = mem::Deref(addr::pCameraObj);
        if (camObj) {
            float fovRad = g_cameraFov * 3.14159265f / 180.0f;
            float aspectH = mem::Read<float>(0xD6F314);  // g_AspectRatioH
            float aspectV = mem::Read<float>(0xD6F31C);  // g_AspectRatioV
            if (aspectH < 0.1f) aspectH = 1.333f;
            if (aspectV < 0.1f) aspectV = 1.0f;

            typedef int (__thiscall *SetFovClipFn)(void*, float, float, float, float, float);
            auto setFovClip = reinterpret_cast<SetFovClipFn>(0x5293D0);
            setFovClip(reinterpret_cast<void*>(camObj), fovRad, aspectH, aspectV,
                       g_cameraNear, g_cameraFar);
        }
    }

    // Auto-load level: skip Shell, load directly into specified level
    if (g_autoLoadLevel[0] && !g_autoLoadTriggered) {
        // Check if we're in Shell (state 5) or just finished init
        int gameState = mem::Read<int>(addr::pGameState);
        if (gameState == 5) {  // ClGameModePlay = Shell menu
            g_autoLoadTriggered = true;
            log::Write("Auto-loading level: %s (skipping Shell)", g_autoLoadLevel);

            typedef void  (__cdecl *SetStorageFlagFn)();
            typedef char  (__cdecl *SetLoadFlagFn)(char);
            typedef int   (__stdcall *SetNarrativeFn)(const char*);
            typedef int   (__cdecl *LoadWorldFn)(const char*, int);
            typedef int   (__cdecl *SetGameStateFn)(int);

            auto setStorage   = reinterpret_cast<SetStorageFlagFn>(addr::fn_SetStorageFlag);
            auto setLoadFlag  = reinterpret_cast<SetLoadFlagFn>(addr::fn_SetLoadFlag);
            auto setNarrative = reinterpret_cast<SetNarrativeFn>(addr::fn_SetNarrativeFile);
            auto loadWorld    = reinterpret_cast<LoadWorldFn>(addr::fn_LoadWorld);
            auto setGameState = reinterpret_cast<SetGameStateFn>(addr::fn_SetGameState);

            __try {
                setStorage();
                setLoadFlag(1);
                setNarrative("nf01");
                loadWorld(g_autoLoadLevel, 0);
                setGameState(6);  // -> ClGameModeTransition
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                log::Write("Auto-load FAILED (exception)");
            }
        }
    }

    // Capture engine console lines from game update context (better coverage during loads)
    CaptureConsoleLines();

    // Auto-save hash→name table every ~5 seconds (game hangs on exit)
    { static DWORD lastSave = 0;
      DWORD now = GetTickCount();
      if (now - lastSave > 5000) { SaveHashNames(); lastSave = now; }
    }

    return OrigCameraSectorUpdate(arg0);
}

// ============================================================================
// World pause — freeze/resume game clock
// ============================================================================
bool worldPaused = false;

void SetWorldPaused(bool pause) {
    worldPaused = pause;
    typedef int (__cdecl *RemoveClockFn)(int);
    typedef int (__cdecl *SetClockFn)(float);
    auto removeEntry = reinterpret_cast<RemoveClockFn>(addr::fn_RemoveClockEntry);
    auto setSpeed    = reinterpret_cast<SetClockFn>(addr::fn_SetClockSpeed);

    int oldEntry = mem::Read<int>(addr::pClockEntryId);
    removeEntry(oldEntry);
    float speed = pause ? 0.0f : 1.0f;
    int newEntry = setSpeed(speed);
    mem::WriteDirect<int>(addr::pClockEntryId, newEntry);
    log::Write("World %s (clock speed=%.1f)", pause ? "PAUSED" : "RESUMED", speed);
}

// ============================================================================
// F12 Screenshot — capture D3D9 backbuffer to BMP
// ============================================================================
static void TakeScreenshot(IDirect3DDevice9* dev) {
    // Create screens directory
    CreateDirectoryA("screens", nullptr);

    // Generate filename with timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    char filename[256];
    snprintf(filename, sizeof(filename), "screens\\shot_%04d%02d%02d_%02d%02d%02d.bmp",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    // Get backbuffer
    IDirect3DSurface9* backbuf = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuf))) return;

    D3DSURFACE_DESC desc;
    backbuf->GetDesc(&desc);

    // Create offscreen surface for GetRenderTargetData
    IDirect3DSurface9* offscreen = nullptr;
    if (FAILED(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height,
               desc.Format, D3DPOOL_SYSTEMMEM, &offscreen, nullptr))) {
        backbuf->Release();
        return;
    }

    dev->GetRenderTargetData(backbuf, offscreen);
    backbuf->Release();

    // Lock and write BMP
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(offscreen->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
        FILE* f = fopen(filename, "wb");
        if (f) {
            int w = desc.Width, h = desc.Height;
            int rowBytes = w * 3;
            int padding = (4 - (rowBytes % 4)) % 4;
            int dataSize = (rowBytes + padding) * h;

            // BMP header
            uint8_t bmpHeader[54] = {};
            bmpHeader[0] = 'B'; bmpHeader[1] = 'M';
            *(uint32_t*)(bmpHeader + 2) = 54 + dataSize;
            *(uint32_t*)(bmpHeader + 10) = 54;
            *(uint32_t*)(bmpHeader + 14) = 40;
            *(int32_t*)(bmpHeader + 18) = w;
            *(int32_t*)(bmpHeader + 22) = h;
            *(uint16_t*)(bmpHeader + 26) = 1;
            *(uint16_t*)(bmpHeader + 28) = 24;
            *(uint32_t*)(bmpHeader + 34) = dataSize;
            fwrite(bmpHeader, 1, 54, f);

            // Pixel data (BMP is bottom-up)
            uint8_t pad[4] = {};
            for (int y = h - 1; y >= 0; y--) {
                uint8_t* row = (uint8_t*)lr.pBits + y * lr.Pitch;
                for (int x = 0; x < w; x++) {
                    // D3DFMT_X8R8G8B8 → BGR
                    uint8_t bgr[3] = { row[x*4+0], row[x*4+1], row[x*4+2] };
                    fwrite(bgr, 1, 3, f);
                }
                if (padding) fwrite(pad, 1, padding, f);
            }
            fclose(f);
            log::Write("Screenshot: %s (%dx%d)", filename, w, h);
        }
        offscreen->UnlockRect();
    }
    offscreen->Release();
}

// ============================================================================
// Hotkey checks — all via GetAsyncKeyState (proven to work with DirectInput)
// ============================================================================
static IDirect3DDevice9* g_lastDevice = nullptr;

static void CheckHotkeys() {
    // INSERT = toggle menu
    static bool prevInsert = false;
    bool curInsert = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (curInsert && !prevInsert) {
        bool wasVisible = menu::visible;
        menu::visible = !menu::visible;
        if (wasVisible && !menu::visible)
            freecam::SaveConfig();
    }
    prevInsert = curInsert;

    // HOME = toggle freecam
    static bool prevHome = false;
    bool curHome = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
    if (curHome && !prevHome) {
        freecam::Toggle();
        log::Write("FreeCam toggled: %s", freecam::enabled ? "ON" : "OFF");
    }
    prevHome = curHome;

    // PAUSE/BREAK = world pause/resume
    static bool prevPause = false;
    bool curPause = (GetAsyncKeyState(VK_PAUSE) & 0x8000) != 0;
    if (curPause && !prevPause)
        SetWorldPaused(!worldPaused);
    prevPause = curPause;

    // F12 = screenshot
    static bool prevF12 = false;
    bool curF12 = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
    if (curF12 && !prevF12 && g_lastDevice)
        TakeScreenshot(g_lastDevice);
    prevF12 = curF12;
}

// EndScene hook — runs every frame
static void InstallD3DHooks(IDirect3DDevice9* dev) {
    static bool installed = false;
    if (installed) return;

    void** vtable = *(void***)dev;
    DWORD oldProt;

    // Hook SetTexture (vtable index 65) — capture texture bindings during rendering
    if (VirtualProtect(&vtable[65], sizeof(void*), PAGE_READWRITE, &oldProt)) {
        OrigSetTexture = (SetTexture_t)vtable[65];
        vtable[65] = (void*)&HookedSetTexture;
        VirtualProtect(&vtable[65], sizeof(void*), oldProt, &oldProt);
        log::Write("SetTexture hooked (vtable[65])");
    }

    // Hook CreateTexture (vtable index 23) — map PCIM data → D3D texture handles
    if (VirtualProtect(&vtable[23], sizeof(void*), PAGE_READWRITE, &oldProt)) {
        OrigCreateTexture = (CreateTexture_t)vtable[23];
        vtable[23] = (void*)&HookedCreateTexture;
        VirtualProtect(&vtable[23], sizeof(void*), oldProt, &oldProt);
        log::Write("CreateTexture hooked (vtable[23])");
    }

    // Hook DrawIndexedPrimitive (vtable index 82) — capture texture→geometry mapping
    if (VirtualProtect(&vtable[82], sizeof(void*), PAGE_READWRITE, &oldProt)) {
        OrigDrawIndexedPrimitive = (DrawIndexedPrimitive_t)vtable[82];
        vtable[82] = (void*)&HookedDrawIndexedPrimitive;
        VirtualProtect(&vtable[82], sizeof(void*), oldProt, &oldProt);
        log::Write("DrawIndexedPrimitive hooked (vtable[82])");
    }

    installed = true;
}

static HRESULT WINAPI HookedEndScene(IDirect3DDevice9* dev) {
    // Check device state — skip everything if device is lost/resetting
    if (!d3dhook::CheckDeviceReady(dev))
        return d3dhook::OriginalEndScene(dev);

    g_lastDevice = dev;
    InstallD3DHooks(dev);
    d3dhook::InitImGui(dev);
    CheckHotkeys();

    // Zero game mouse deltas when menu is open
    if (menu::visible) {
        mem::WriteDirect<float>(addr::MOUSE_DX, 0.0f);
        mem::WriteDirect<float>(addr::MOUSE_DY, 0.0f);
    }

    freecam::Update();
    FlushDrawLog();

    // D3D wireframe toggle — persists because engine never resets FILLMODE
    dev->SetRenderState(D3DRS_FILLMODE, g_dbgWireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID);

    if (d3dhook::IsImGuiReady()) {
        d3dhook::NewFrame();
        menu::Render();
        d3dhook::EndFrame();
    }

    return d3dhook::OriginalEndScene(dev);
}

// ============================================================================
// HashString hook — captures asset name → hash pairs for name dictionary
// Writes unique pairs to hash_names.txt on DLL_PROCESS_DETACH
// ============================================================================
// Hook HashAndLookup (0x41E830) — captures asset name→hash at the point of WAD lookup
typedef int (__cdecl *HashAndLookupFn)(const char*);
static HashAndLookupFn OrigHashAndLookup = nullptr;

#define HASH_TABLE_SIZE 8192
struct HashEntry { unsigned int hash; char name[128]; };
static HashEntry g_hashTable[HASH_TABLE_SIZE] = {};
static volatile long g_hashCount = 0;
static CRITICAL_SECTION g_hashCS;

// Inline hash to capture the hash value (function only returns index, not hash)
static unsigned int InlineHash(const char* str) {
    unsigned int result = 0;
    if (str) {
        for (const unsigned char* p = (const unsigned char*)str; *p; p++) {
            unsigned char c = *p;
            result = result + c + ((result << (c & 7)) & 0xFFFFFFFF);
        }
    }
    return result;
}

static int __cdecl HookedHashAndLookup(const char* str) {
    if (str && str[0]) {
        unsigned int h = InlineHash(str);
        EnterCriticalSection(&g_hashCS);
        bool found = false;
        long count = g_hashCount;
        for (long i = 0; i < count; i++) {
            if (g_hashTable[i].hash == h) { found = true; break; }
        }
        if (!found && count < HASH_TABLE_SIZE) {
            g_hashTable[count].hash = h;
            strncpy(g_hashTable[count].name, str, 127);
            g_hashTable[count].name[127] = 0;
            InterlockedIncrement(&g_hashCount);
        }
        LeaveCriticalSection(&g_hashCS);
    }
    return OrigHashAndLookup(str);
}

// Save continuously — game hangs on exit so DLL_PROCESS_DETACH is unreliable
static volatile long g_lastSavedCount = 0;

static void SaveHashNames() {
    long count = g_hashCount;
    if (count == 0 || count == g_lastSavedCount) return;
    FILE* f = fopen("hash_names.txt", "w");
    if (!f) return;
    fprintf(f, "# Spiderwick HashString capture: %ld unique entries\n", count);
    for (long i = 0; i < count; i++) {
        fprintf(f, "0x%08X\t%s\n", g_hashTable[i].hash, g_hashTable[i].name);
    }
    fclose(f);
    g_lastSavedCount = count;
}

// Mod init thread
static DWORD WINAPI ModThread(LPVOID) {
    Sleep(3000);

    if (!d3dhook::Install(HookedEndScene)) {
        MessageBoxA(nullptr, "Failed to hook EndScene", "SpiderMod", MB_OK);
        return 0;
    }

    // Hook CameraSectorUpdate for deferred hot-switch (runs in game update context)
    if (MH_CreateHook(reinterpret_cast<LPVOID>(addr::fn_CameraSectorUpdate),
                       reinterpret_cast<LPVOID>(&HookedCameraSectorUpdate),
                       reinterpret_cast<LPVOID*>(&OrigCameraSectorUpdate)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(addr::fn_CameraSectorUpdate));
        log::Write("CameraSectorUpdate hooked for deferred switch");
    }

    // Hook HashAndLookup to capture asset name → hash pairs at WAD lookup point
    if (MH_CreateHook(reinterpret_cast<LPVOID>(addr::fn_HashAndLookup),
                       reinterpret_cast<LPVOID>(&HookedHashAndLookup),
                       reinterpret_cast<LPVOID*>(&OrigHashAndLookup)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(addr::fn_HashAndLookup));
        log::Write("HashAndLookup hooked for asset name capture");
    }

    // GeomInstance_Init already hooked in DLL_PROCESS_ATTACH (before level loads)
    log::Write("GeomInstance_Init hooked for world matrix capture (%ld captured so far)", g_geomInstanceCount);

    // Hook sauSetPosition/sauSetRotation — capture VM prop placements
    if (MH_CreateHook(reinterpret_cast<LPVOID>(addr::fn_sauSetPosition),
                       reinterpret_cast<LPVOID>(&HookedSetPosition),
                       reinterpret_cast<LPVOID*>(&OrigSetPosition)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(addr::fn_sauSetPosition));
        log::Write("sauSetPosition hooked for prop placement capture");
    }
    if (MH_CreateHook(reinterpret_cast<LPVOID>(addr::fn_sauSetRotation),
                       reinterpret_cast<LPVOID>(&HookedSetRotation),
                       reinterpret_cast<LPVOID*>(&OrigSetRotation)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(addr::fn_sauSetRotation));
        log::Write("sauSetRotation hooked for prop placement capture");
    }

    // sauCharacterInit hook — capture texture name strings for SpiderView
    if (MH_CreateHook(reinterpret_cast<LPVOID>(addr::fn_sauCharacterInit),
                       reinterpret_cast<LPVOID>(&HookedCharacterInit),
                       reinterpret_cast<LPVOID*>(&OrigCharacterInit)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(addr::fn_sauCharacterInit));
        log::Write("sauCharacterInit hooked for texture capture");
    }

    // ClNoamActor_Init hook — capture descriptor array (NM40 + texture bindings)
    if (MH_CreateHook(reinterpret_cast<LPVOID>(addr::fn_ClNoamActorInit),
                       reinterpret_cast<LPVOID>(&HookedNoamActorInit),
                       reinterpret_cast<LPVOID*>(&OrigNoamActorInit)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(addr::fn_ClNoamActorInit));
        log::Write("ClNoamActor_Init hooked for NM40→texture mapping");
    }

    // PCIM factory hook — capture PCIM data pointers for correlation with render textures
    if (MH_CreateHook(reinterpret_cast<LPVOID>(addr::fn_PCIMFactory),
                       reinterpret_cast<LPVOID>(&HookedPCIMFactory),
                       reinterpret_cast<LPVOID*>(&OrigPCIMFactory)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(addr::fn_PCIMFactory));
        log::Write("PCIM_Factory hooked");
    }

    // RENDERFUNC_NOAM_DIFFUSE — capture render-time texture bindings (__cdecl)
    {
        MH_STATUS st = MH_CreateHook(reinterpret_cast<LPVOID>(addr::fn_RenderFuncNoamDiffuse),
                           reinterpret_cast<LPVOID>(&HookedRenderFuncNoamDiffuse),
                           reinterpret_cast<LPVOID*>(&OrigRenderFuncNoamDiffuse));
        if (st == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(addr::fn_RenderFuncNoamDiffuse));
            log::Write("RENDERFUNC_NOAM_DIFFUSE hooked OK");
        } else {
            log::Write("RENDERFUNC_NOAM_DIFFUSE hook FAILED, status=%d", (int)st);
        }
    }

    // Clear previous texture map file
    { FILE* f = fopen("spiderview_texmap.txt", "w"); if (f) fclose(f); }

    // Registry write hooks — capture game config writes → settings.ini
    regredirect::InstallHooks();

    freecam::LoadConfig();

    // Read auto-load level from spidermod.cfg
    {
        FILE* cfg = fopen("spidermod.cfg", "r");
        if (cfg) {
            char line[128];
            while (fgets(line, sizeof(line), cfg)) {
                char val[64];
                if (sscanf(line, " autoload = %63s", val) == 1) {
                    strncpy(g_autoLoadLevel, val, 63);
                    g_autoLoadLevel[63] = 0;
                    log::Write("Auto-load level set: %s", g_autoLoadLevel);
                }
            }
            fclose(cfg);
        }
    }

    // Config.txt is read by the engine at boot (before ASI ModThread runs)
    // To skip Shell: create Config.txt with "LEVEL GroundsD" before launching
    // No runtime generation needed — engine handles it natively

    log::Write("SpiderMod initialized — EndScene + Update hooked, config loaded");

    while (true) Sleep(1000);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        InitializeCriticalSection(&g_hashCS);
        MH_Initialize();
        // Pre-populate registry from settings.ini BEFORE game reads config
        // Safe in DllMain — just registry writes, no hooking
        regredirect::PrePopulate();

        // Hook GeomInstance_Init EARLY — before level loads (must be in DLL_PROCESS_ATTACH)
        if (MH_CreateHook(reinterpret_cast<LPVOID>(addr::fn_GeomInstanceInit),
                           reinterpret_cast<LPVOID>(&HookedGeomInstanceInit),
                           reinterpret_cast<LPVOID*>(&OrigGeomInstanceInit)) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(addr::fn_GeomInstanceInit));
        }

        // Patch "Config.txt" → "Launch.txt" in memory before engine reads it
        // String is at 0x63992C (10 chars + null, fits any name ≤10 chars)
        {
            DWORD oldProt;
            void* strAddr = reinterpret_cast<void*>(0x63992C);
            if (VirtualProtect(strAddr, 11, PAGE_READWRITE, &oldProt)) {
                memcpy(strAddr, "Launch.txt", 11);  // 10 chars + null
                VirtualProtect(strAddr, 11, oldProt, &oldProt);
            }
        }

        // Patch NOAM viewer init: NOP out call to sub_48F060 (loads dev-only animId.sau)
        // Without this patch, VIEWER noam crashes because ww/dev/animId.sau doesn't exist
        {
            DWORD oldProt;
            void* patchAddr = reinterpret_cast<void*>(0x48FA6A);
            if (VirtualProtect(patchAddr, 5, PAGE_EXECUTE_READWRITE, &oldProt)) {
                memset(patchAddr, 0x90, 5);  // 5x NOP
                VirtualProtect(patchAddr, 5, oldProt, &oldProt);
            }
        }

        CreateThread(nullptr, 0, ModThread, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        // Save captured hash→name pairs
        SaveHashNames();
        // Save all game settings to settings.ini before exit
        regredirect::SaveToINI();
        // Restore MissionStart string if modified
        mem::Write<uint8_t>(addr::pMissionStartString, 'M');
        freecam::SaveConfig();
        d3dhook::Remove();
        MH_Uninitialize();
    }
    return TRUE;
}
