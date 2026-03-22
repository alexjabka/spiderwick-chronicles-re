#pragma once
// ============================================================================
// Registry Redirect — game settings live in settings.ini, not Windows Registry
//
// Strategy:
//   PrePopulate()   — DllMain: read INI → write to registry (raw ANSI format)
//   InstallHooks()  — ModThread: hook writes to capture changes → save to INI
//   SaveToINI()     — DLL_PROCESS_DETACH: dump final registry state → INI
// ============================================================================

namespace regredirect {
    void PrePopulate();    // Phase 1: INI → registry (call from DllMain)
    void InstallHooks();   // Phase 2: hook writes (call from ModThread)
    void SaveToINI();      // Phase 3: registry → INI (call on exit)
}
