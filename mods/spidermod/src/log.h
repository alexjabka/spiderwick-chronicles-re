#pragma once
// ============================================================================
// Debug log — appends to spidermod.log in game directory
// ============================================================================

#include <cstdio>
#include <cstdarg>

namespace log {

inline void Write(const char* fmt, ...) {
    FILE* f = fopen("spidermod.log", "a");
    if (!f) return;
    // Timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
}

} // namespace log
