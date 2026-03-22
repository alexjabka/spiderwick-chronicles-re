#pragma once
// ============================================================================
// D3D9 EndScene hook + ImGui lifecycle
// ============================================================================

#include <d3d9.h>

namespace d3dhook {

using EndScene_t = HRESULT(WINAPI*)(IDirect3DDevice9*);
extern EndScene_t OriginalEndScene;

// D3D render overrides — applied per draw call via DrawIndexedPrimitive hook
extern bool  dbgNoTextures;
extern bool  dbgNoShaders;     // disables vertex+pixel shaders (forces fixed-function)
extern float dbgAmbientLevel;  // 0 = default, >0 = override (works best with No Shaders)

// World matrix capture — captures WVP + VB identity from DIP calls
struct CapturedMatrix {
    D3DMATRIX mat;
    UINT numVerts;
    UINT primCount;
    float firstVtx[3];  // first vertex position from locked VB (non-VP only)
    UINT flags;          // 1 = has firstVtx
    DWORD vbPtr;         // vertex buffer address (unique per VB)
    UINT  vbOffset;      // offset within VB (identifies geometry within shared VBs)
    UINT  vbStride;      // vertex stride
    INT   baseVtx;       // base vertex index from DIP call
};
constexpr int MAX_CAPTURED_MATRICES = 4096;
extern CapturedMatrix g_capturedMatrices[MAX_CAPTURED_MATRICES];
extern volatile long  g_capturedCount;
extern int            g_captureFrames;   // >0 = capture for N more frames
extern D3DMATRIX      g_vpReference;     // VP matrix (auto-detected from first draw)
extern bool           g_vpCaptured;      // true once VP reference is set

// Device reset guard — true while D3D device is being reset (resolution change etc.)
extern volatile bool g_resetting;

// Hook installation / removal
bool Install(EndScene_t callback);
void Remove();

// Device state — returns false if device is lost/resetting (skip rendering)
bool CheckDeviceReady(IDirect3DDevice9* dev);

// ImGui lifecycle — call InitImGui once from EndScene, then NewFrame/EndFrame each frame
void InitImGui(IDirect3DDevice9* dev);
void ShutdownImGui();
bool IsImGuiReady();
void NewFrame();
void EndFrame();

// --- In-engine OBJ world exporter ---
extern bool g_objExportActive;   // true during export frame
extern int  g_objExportDraws;    // draw calls captured
extern int  g_objExportVerts;    // total vertices written

void StartObjExport();           // call from menu button
void FinishObjExport();          // call from EndScene after frame

} // namespace d3dhook
