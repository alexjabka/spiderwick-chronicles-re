// ============================================================================
// Registry Redirect — game settings in settings.ini
//
// The game uses RegCreateKeyExW + RegQueryValueExW/RegSetValueExW with
// raw ANSI bytes (Kallis VM bug — calls W API with ANSI data).
//
// We pre-populate the registry from settings.ini before the game reads,
// hook writes to capture changes back to INI, and save on exit.
// ============================================================================

#include "registry_redirect.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <MinHook.h>

static const char* INI_PATH = ".\\settings.ini";

// Registry path the game uses (under HKCU)
static const wchar_t* GAME_REG_PATH = L"Software\\Sierra Entertainment\\The Spiderwick Chronicles";

// Sections and their keys (from spy log analysis)
struct ConfigKey {
    const char* section;    // INI section
    const char* name;       // value name
    const char* defVal;     // default if missing
};

static const ConfigKey g_configKeys[] = {
    // video
    {"video", "width",                "800"},
    {"video", "height",               "600"},
    {"video", "refresh_rate",         "60.00"},
    {"video", "texture_lod_bias",     "0.00"},
    {"video", "texture_scale_factor", "1.00"},
    {"video", "trilinear",            "0"},
    {"video", "contrast",             "1.00"},
    {"video", "brightness",           "-0.00"},
    {"video", "gamma",                "1.00"},
    {"video", "draw_distance",        "800.00"},
    {"video", "wide_screen",          "0"},
    {"video", "high_res",             "1"},
    // audio
    {"audio", "master_volume",        "1.00"},
    {"audio", "music_volume",         "1.00"},
    {"audio", "sfx_volume",           "1.00"},
    {"audio", "speech_volume",        "1.00"},
    {"audio", "ambience_volume",      "1.00"},
    // localization
    {"localization", "time_format",   "1"},
    {"localization", "date_format",   "3"},
};
static const int g_configKeyCount = sizeof(g_configKeys) / sizeof(g_configKeys[0]);

// ============================================================================
// Helper: write raw ANSI string to registry as REG_SZ (matching game's format)
// ============================================================================
static void WriteRawAnsi(HKEY hKey, const wchar_t* valueName, const char* ansiValue) {
    int len = (int)strlen(ansiValue) + 1;  // include null terminator
    RegSetValueExW(hKey, valueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(ansiValue), len);
}

// ============================================================================
// Helper: read raw ANSI bytes from registry REG_SZ value
// ============================================================================
static bool ReadRawAnsi(HKEY hKey, const wchar_t* valueName, char* buf, int bufLen) {
    DWORD type = 0, cbData = bufLen;
    LSTATUS r = RegQueryValueExW(hKey, valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(buf), &cbData);
    if (r != ERROR_SUCCESS) return false;
    // Ensure null-terminated
    if (cbData > 0 && cbData < (DWORD)bufLen)
        buf[cbData] = 0;
    else
        buf[bufLen - 1] = 0;
    return true;
}

// ============================================================================
// Phase 1: PrePopulate — read settings.ini → write to registry
// Called from DllMain (safe: no hooking, just registry writes)
// ============================================================================
void regredirect::PrePopulate() {
    // Check if settings.ini exists
    DWORD attr = GetFileAttributesA(INI_PATH);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return;  // No INI — game uses whatever is in registry

    // Open (or create) each section's registry key and write values
    const char* lastSection = nullptr;
    HKEY hKey = nullptr;

    for (int i = 0; i < g_configKeyCount; i++) {
        const ConfigKey& ck = g_configKeys[i];

        // Open new registry key when section changes
        if (!lastSection || strcmp(lastSection, ck.section) != 0) {
            if (hKey) RegCloseKey(hKey);
            hKey = nullptr;

            wchar_t subKey[256];
            swprintf(subKey, 256, L"%s\\%hs", GAME_REG_PATH, ck.section);

            DWORD disp;
            RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, 0,
                KEY_SET_VALUE, nullptr, &hKey, &disp);

            lastSection = ck.section;
        }

        if (!hKey) continue;

        // Read value from INI
        char val[256];
        GetPrivateProfileStringA(ck.section, ck.name, ck.defVal,
            val, sizeof(val), INI_PATH);

        // Write to registry in raw ANSI format
        wchar_t wName[128];
        swprintf(wName, 128, L"%hs", ck.name);
        WriteRawAnsi(hKey, wName, val);
    }

    if (hKey) RegCloseKey(hKey);
}

// ============================================================================
// Phase 2: Hook RegSetValueExW to capture game writes → settings.ini
// ============================================================================
typedef LSTATUS (WINAPI *RegSetValueExW_t)(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);
typedef LSTATUS (WINAPI *RegCreateKeyExW_t)(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);
typedef LSTATUS (WINAPI *RegCloseKey_t)(HKEY);

static RegSetValueExW_t   Orig_RegSetValueExW   = nullptr;
static RegCreateKeyExW_t  Orig_RegCreateKeyExW  = nullptr;
static RegCloseKey_t      Orig_RegCloseKey      = nullptr;

// Track game key handles to know which section they belong to
#define MAX_TRACKED 32
struct TrackedKey {
    HKEY handle;
    char section[64];
};
static TrackedKey g_tracked[MAX_TRACKED] = {};

static void TrackKey(HKEY handle, const char* section) {
    for (int i = 0; i < MAX_TRACKED; i++) {
        if (!g_tracked[i].handle) {
            g_tracked[i].handle = handle;
            strncpy(g_tracked[i].section, section, 63);
            g_tracked[i].section[63] = 0;
            return;
        }
    }
}

static const char* GetSection(HKEY handle) {
    for (int i = 0; i < MAX_TRACKED; i++)
        if (g_tracked[i].handle == handle)
            return g_tracked[i].section;
    return nullptr;
}

static void UntrackKey(HKEY handle) {
    for (int i = 0; i < MAX_TRACKED; i++) {
        if (g_tracked[i].handle == handle) {
            g_tracked[i].handle = nullptr;
            g_tracked[i].section[0] = 0;
            return;
        }
    }
}

static bool ExtractSection(const wchar_t* path, char* section, int maxLen) {
    if (!path) return false;
    // Convert to narrow
    char narrow[512];
    WideCharToMultiByte(CP_ACP, 0, path, -1, narrow, sizeof(narrow), nullptr, nullptr);
    // Look for "The Spiderwick Chronicles\"
    const char* marker = strstr(narrow, "The Spiderwick Chronicles");
    if (!marker) return false;
    const char* after = marker + strlen("The Spiderwick Chronicles");
    if (*after == '\\') after++;
    strncpy(section, after, maxLen - 1);
    section[maxLen - 1] = 0;
    // Remove trailing backslash
    int len = (int)strlen(section);
    if (len > 0 && section[len-1] == '\\') section[len-1] = 0;
    return section[0] != 0;  // must have a section (video/audio/localization)
}

static LSTATUS WINAPI Hook_RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved,
    LPWSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSA,
    PHKEY phkResult, LPDWORD lpdwDisposition)
{
    LSTATUS r = Orig_RegCreateKeyExW(hKey, lpSubKey, Reserved, lpClass,
        dwOptions, samDesired, lpSA, phkResult, lpdwDisposition);

    // Track game config keys
    if (r == ERROR_SUCCESS && phkResult && hKey == HKEY_CURRENT_USER) {
        char section[64];
        if (ExtractSection(lpSubKey, section, sizeof(section)))
            TrackKey(*phkResult, section);
    }
    return r;
}

static LSTATUS WINAPI Hook_RegSetValueExW(HKEY hKey, LPCWSTR lpValueName,
    DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData)
{
    // Always pass through to real registry
    LSTATUS r = Orig_RegSetValueExW(hKey, lpValueName, Reserved, dwType, lpData, cbData);

    // If this is a game config key, also save to INI
    const char* section = GetSection(hKey);
    if (section && lpValueName && dwType == REG_SZ && lpData && cbData > 0) {
        // Convert value name to ANSI
        char nameA[128];
        WideCharToMultiByte(CP_ACP, 0, lpValueName, -1, nameA, sizeof(nameA), nullptr, nullptr);

        // Data is raw ANSI bytes (game's format)
        char valA[256];
        int copyLen = cbData < sizeof(valA) ? cbData : sizeof(valA) - 1;
        memcpy(valA, lpData, copyLen);
        valA[copyLen] = 0;

        WritePrivateProfileStringA(section, nameA, valA, INI_PATH);
    }

    return r;
}

static LSTATUS WINAPI Hook_RegCloseKey(HKEY hKey) {
    UntrackKey(hKey);
    return Orig_RegCloseKey(hKey);
}

void regredirect::InstallHooks() {
    HMODULE advapi = GetModuleHandleA("advapi32.dll");
    if (!advapi) return;

    auto hookOne = [&](const char* name, LPVOID detour, LPVOID* orig) {
        auto proc = GetProcAddress(advapi, name);
        if (proc) {
            MH_CreateHook(proc, detour, orig);
            MH_EnableHook(proc);
        }
    };

    // Only hook the functions we need for write capture
    hookOne("RegCreateKeyExW", (LPVOID)Hook_RegCreateKeyExW, (LPVOID*)&Orig_RegCreateKeyExW);
    hookOne("RegSetValueExW",  (LPVOID)Hook_RegSetValueExW,  (LPVOID*)&Orig_RegSetValueExW);
    hookOne("RegCloseKey",     (LPVOID)Hook_RegCloseKey,     (LPVOID*)&Orig_RegCloseKey);
}

// ============================================================================
// Phase 3: SaveToINI — dump all game registry values to settings.ini
// Called on exit (DLL_PROCESS_DETACH)
// ============================================================================
void regredirect::SaveToINI() {
    const char* lastSection = nullptr;
    HKEY hKey = nullptr;

    for (int i = 0; i < g_configKeyCount; i++) {
        const ConfigKey& ck = g_configKeys[i];

        if (!lastSection || strcmp(lastSection, ck.section) != 0) {
            if (hKey) RegCloseKey(hKey);
            hKey = nullptr;

            wchar_t subKey[256];
            swprintf(subKey, 256, L"%s\\%hs", GAME_REG_PATH, ck.section);

            RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey);
            lastSection = ck.section;
        }

        if (!hKey) continue;

        wchar_t wName[128];
        swprintf(wName, 128, L"%hs", ck.name);

        char val[256] = {};
        if (ReadRawAnsi(hKey, wName, val, sizeof(val)) && val[0]) {
            WritePrivateProfileStringA(ck.section, ck.name, val, INI_PATH);
        }
    }

    if (hKey) RegCloseKey(hKey);
}
