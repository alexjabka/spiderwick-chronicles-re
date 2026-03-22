// ============================================================================
// D3D9 EndScene hook + ImGui integration
// WndProc hook for ImGui input, game input blocking when menu is open
// ============================================================================

#include "d3d_hook.h"
#include "addresses.h"
#include "menu.h"
#include <MinHook.h>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <cstdio>

// Forward-declare ImGui Win32 handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace d3dhook {

EndScene_t OriginalEndScene = nullptr;

// D3D render overrides
bool  dbgNoTextures   = false;
bool  dbgNoShaders    = false;
float dbgAmbientLevel = 0.0f;

// World matrix capture
CapturedMatrix g_capturedMatrices[MAX_CAPTURED_MATRICES] = {};
volatile long  g_capturedCount = 0;
int            g_captureFrames = 0;
D3DMATRIX      g_vpReference = {};
bool           g_vpCaptured = false;

// Device reset state
volatile bool g_resetting = false;

// Reset hook — release ImGui's D3D9 resources (font texture etc.) so Reset() can succeed
typedef HRESULT(WINAPI* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
static Reset_t OrigReset = nullptr;

// Forward declare — implemented after variable declarations
static HRESULT WINAPI HookedReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp);

// DrawIndexedPrimitive hook — forces render states per draw call
typedef HRESULT(WINAPI* DrawIndexedPrimitive_t)(
    IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
static DrawIndexedPrimitive_t OrigDIP = nullptr;

static bool IsIdentityMatrix(const D3DMATRIX& m) {
    return fabsf(m._11 - 1.0f) < 0.001f && fabsf(m._22 - 1.0f) < 0.001f &&
           fabsf(m._33 - 1.0f) < 0.001f && fabsf(m._44 - 1.0f) < 0.001f &&
           fabsf(m._12) < 0.001f && fabsf(m._13) < 0.001f &&
           fabsf(m._21) < 0.001f && fabsf(m._23) < 0.001f &&
           fabsf(m._31) < 0.001f && fabsf(m._32) < 0.001f &&
           fabsf(m._41) < 0.001f && fabsf(m._42) < 0.001f && fabsf(m._43) < 0.001f;
}

static HRESULT WINAPI HookedDrawIndexedPrimitive(
    IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVtx,
    UINT minIdx, UINT numVerts, UINT startIdx, UINT primCount)
{
    // Capture ALL non-zero WVP (c0-c3) — simple, fast, no VB locking
    if (g_captureFrames > 0) {
        float c[16];
        if (SUCCEEDED(dev->GetVertexShaderConstantF(0, c, 4))) {
            bool allZero = true;
            for (int i = 0; i < 16 && allZero; i++)
                if (c[i] != 0.0f) allZero = false;
            if (!allZero) {
                long idx = InterlockedIncrement(&g_capturedCount) - 1;
                if (idx < MAX_CAPTURED_MATRICES) {
                    memcpy(&g_capturedMatrices[idx].mat, c, sizeof(D3DMATRIX));
                    g_capturedMatrices[idx].numVerts = numVerts;
                    g_capturedMatrices[idx].primCount = primCount;
                    g_capturedMatrices[idx].firstVtx[0] = 0;
                    g_capturedMatrices[idx].firstVtx[1] = 0;
                    g_capturedMatrices[idx].firstVtx[2] = 0;
                    g_capturedMatrices[idx].flags = 0;
                }
            }
        }
    }



    if (dbgNoTextures) {
        dev->SetTexture(0, nullptr);
        dev->SetTexture(1, nullptr);
        dev->SetTexture(2, nullptr);
        dev->SetTexture(3, nullptr);
    }
    if (dbgNoShaders) {
        dev->SetVertexShader(nullptr);
        dev->SetPixelShader(nullptr);
        dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    }
    if (dbgAmbientLevel > 0.01f) {
        int a = (int)(dbgAmbientLevel * 255.0f);
        if (a > 255) a = 255;
        dev->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_RGBA(a, a, a, 255));
    }
    return OrigDIP(dev, type, baseVtx, minIdx, numVerts, startIdx, primCount);
}

// ============================================================================
// In-engine OBJ exporter — reads directly from engine's PCWB data structures
// NOT a frame capture — reads the engine's own geometry + transforms
// ============================================================================
bool g_objExportActive = false;
int  g_objExportDraws = 0;
int  g_objExportVerts = 0;

void StartObjExport() {
    // Implemented in menu_debug.cpp via ExportWorldFromMemory()
    g_objExportActive = true;
}

void FinishObjExport() {
    g_objExportActive = false;
}

// ============================================================================
static bool    imguiReady  = false;
static HWND    gameHwnd    = nullptr;
static WNDPROC origWndProc = nullptr;

// ============================================================================
// WndProc hook — feeds input to ImGui, blocks game input when menu is open
// ============================================================================
static LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (imguiReady) {
        // Let ImGui observe all messages for internal state, but NEVER eat them.
        // Window management messages (WM_SIZE, WM_DISPLAYCHANGE, WM_ACTIVATEAPP etc.)
        // must always reach the game for resolution changes and alt-tab to work.
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);

        // Only block input messages when our mod menu is open
        if (menu::visible) {
            switch (msg) {
            case WM_KEYDOWN: case WM_KEYUP:
            case WM_CHAR: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
            case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
                return 0;
            }
        }
    }
    return CallWindowProcA(origWndProc, hwnd, msg, wp, lp);
}

// ============================================================================
// EndScene address resolution
// ============================================================================
static uintptr_t GetEndSceneAddr() {
    // Try game's real D3D device first
    uintptr_t devPtr = *reinterpret_cast<uintptr_t*>(addr::pD3DDevice);
    if (devPtr) {
        uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(devPtr);
        if (vtable)
            return vtable[42]; // EndScene
    }

    // Fallback: temporary device
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "SpiderD3DTemp";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("SpiderD3DTemp", "", WS_OVERLAPPED,
        0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        DestroyWindow(hwnd);
        UnregisterClassA("SpiderD3DTemp", wc.hInstance);
        return 0;
    }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);

    uintptr_t esAddr = 0;
    if (dev) {
        uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(dev);
        esAddr = vtable[42];
        dev->Release();
    }
    d3d->Release();
    DestroyWindow(hwnd);
    UnregisterClassA("SpiderD3DTemp", wc.hInstance);
    return esAddr;
}

// ============================================================================
// Hook install / remove
// ============================================================================
bool Install(EndScene_t callback) {
    uintptr_t target = GetEndSceneAddr();
    if (!target) return false;

    if (MH_CreateHook(reinterpret_cast<void*>(target),
                      reinterpret_cast<void*>(callback),
                      reinterpret_cast<void**>(&OriginalEndScene)) != MH_OK)
        return false;

    if (MH_EnableHook(reinterpret_cast<void*>(target)) != MH_OK)
        return false;

    uintptr_t devPtr = *reinterpret_cast<uintptr_t*>(addr::pD3DDevice);
    if (devPtr) {
        uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(devPtr);
        if (vtable) {
            // Hook Reset (vtable[16]) — release ImGui D3D9 resources before device reset
            uintptr_t resetAddr = vtable[16];
            if (resetAddr) {
                MH_CreateHook(reinterpret_cast<void*>(resetAddr),
                              reinterpret_cast<void*>(&HookedReset),
                              reinterpret_cast<void**>(&OrigReset));
                MH_EnableHook(reinterpret_cast<void*>(resetAddr));
            }

            // Hook DrawIndexedPrimitive (vtable[82]) for per-draw-call render overrides
            uintptr_t dipAddr = vtable[82];
            if (dipAddr) {
                MH_CreateHook(reinterpret_cast<void*>(dipAddr),
                              reinterpret_cast<void*>(&HookedDrawIndexedPrimitive),
                              reinterpret_cast<void**>(&OrigDIP));
                MH_EnableHook(reinterpret_cast<void*>(dipAddr));
            }
        }
    }

    return true;
}

void Remove() {
    MH_DisableHook(MH_ALL_HOOKS);
    ShutdownImGui();
}

// ============================================================================
// ImGui lifecycle
// ============================================================================
void InitImGui(IDirect3DDevice9* dev) {
    if (imguiReady) return;

    // Get game window from device
    D3DDEVICE_CREATION_PARAMETERS cp;
    if (FAILED(dev->GetCreationParameters(&cp))) return;
    gameHwnd = cp.hFocusWindow;
    if (!gameHwnd) return;

    // Create context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // no imgui.ini

    // Font
    ImFontConfig fontCfg;
    fontCfg.SizePixels = 16.0f;
    io.Fonts->AddFontDefault(&fontCfg);

    // Style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 6.0f;
    style.FrameRounding    = 3.0f;
    style.GrabRounding     = 3.0f;
    style.ScrollbarRounding = 3.0f;

    // Init backends
    ImGui_ImplWin32_Init(gameHwnd);
    ImGui_ImplDX9_Init(dev);

    // Hook WndProc for input
    origWndProc = (WNDPROC)SetWindowLongA(gameHwnd, GWL_WNDPROC, (LONG)HookedWndProc);

    imguiReady = true;
}

void ShutdownImGui() {
    if (!imguiReady) return;

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // Restore WndProc
    if (gameHwnd && origWndProc)
        SetWindowLongA(gameHwnd, GWL_WNDPROC, (LONG)origWndProc);

    imguiReady = false;
}

bool IsImGuiReady() { return imguiReady; }

// ============================================================================
// Reset hook — release ImGui D3D9 resources so Reset() can succeed
// ============================================================================
static HRESULT WINAPI HookedReset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    g_resetting = true;

    // Release ImGui D3D9 resources (font texture, VB, IB) so Reset can succeed.
    // Protected with SEH in case ImGui state is already corrupt.
    if (imguiReady) {
        __try {
            ImGui_ImplDX9_InvalidateDeviceObjects();
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }

    HRESULT hr = OrigReset(dev, pp);

    if (FAILED(hr) && imguiReady) {
        // Reset failed — full teardown, InitImGui will rebuild on next EndScene
        ShutdownImGui();
    }

    g_resetting = false;
    return hr;
}

// Check D3D device cooperative level — handles device lost/reset gracefully.
// Returns true if device is OK to render, false if lost/resetting.
bool CheckDeviceReady(IDirect3DDevice9* dev) {
    HRESULT hr = dev->TestCooperativeLevel();
    if (hr == D3D_OK) {
        g_resetting = false;
        return true;
    }
    // Device is lost or needs reset — tear down ImGui if it's up
    if (!g_resetting && imguiReady) {
        g_resetting = true;
        ShutdownImGui();
    }
    g_resetting = true;
    return false;
}

void NewFrame() {
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();

    // Manual mouse input — DirectInput exclusive mode blocks WM_MOUSE* messages
    // GetCursorPos + GetAsyncKeyState always work regardless of DirectInput
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = menu::visible; // software cursor when menu open

    POINT pos;
    if (GetCursorPos(&pos) && gameHwnd) {
        ScreenToClient(gameHwnd, &pos);
        io.AddMousePosEvent(static_cast<float>(pos.x), static_cast<float>(pos.y));
    }
    io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);

    ImGui::NewFrame();
}

void EndFrame() {
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    // Multi-frame capture: decrement frame counter at end of each frame
    if (g_captureFrames > 0)
        --g_captureFrames;
}

} // namespace d3dhook
