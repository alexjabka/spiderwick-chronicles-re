// ============================================================================
// dinput8.dll proxy — loads spidermod.asi, forwards DirectInput8Create
// ============================================================================

#include <windows.h>
#include <cstdio>

// Real dinput8 function pointer
typedef HRESULT(WINAPI* DirectInput8Create_t)(
    HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

static HMODULE hRealDinput8 = nullptr;
static HMODULE hSpidermod   = nullptr;
static DirectInput8Create_t pRealCreate = nullptr;

// The only export the game uses from dinput8.dll
extern "C" __declspec(dllexport)
HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
    if (!pRealCreate) return E_FAIL;
    return pRealCreate(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        // Log file for debugging
        char logPath[MAX_PATH];
        GetModuleFileNameA(hModule, logPath, MAX_PATH);
        char* ls = strrchr(logPath, '\\');
        if (ls) { *(ls + 1) = '\0'; strcat_s(logPath, "spidermod.log"); }
        FILE* log = nullptr;
        fopen_s(&log, logPath, "w");

        // Load real dinput8.dll from system directory
        char sysDir[MAX_PATH];
        GetSystemDirectoryA(sysDir, MAX_PATH);
        strcat_s(sysDir, "\\dinput8.dll");
        hRealDinput8 = LoadLibraryA(sysDir);
        if (hRealDinput8)
            pRealCreate = (DirectInput8Create_t)GetProcAddress(hRealDinput8, "DirectInput8Create");
        if (log) fprintf(log, "Real dinput8: %s -> %p\n", sysDir, hRealDinput8);

        // Load spidermod.asi from same directory as this DLL
        char modDir[MAX_PATH];
        GetModuleFileNameA(hModule, modDir, MAX_PATH);
        char* lastSlash = strrchr(modDir, '\\');
        if (lastSlash) {
            *(lastSlash + 1) = '\0';
            strcat_s(modDir, "spidermod.asi");
            hSpidermod = LoadLibraryA(modDir);
            if (log) fprintf(log, "SpiderMod: %s -> %p\n", modDir, hSpidermod);
        }

        if (log) { fclose(log); }
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (hSpidermod)   FreeLibrary(hSpidermod);
        if (hRealDinput8) FreeLibrary(hRealDinput8);
    }
    return TRUE;
}
