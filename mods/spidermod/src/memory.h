#pragma once
// ============================================================================
// Memory helpers — inline Nop/Restore/Read/Write with VirtualProtect
// ============================================================================

#include <cstdint>
#include <cstring>
#include <windows.h>

namespace mem {

inline bool Nop(uintptr_t address, size_t size) {
    DWORD old;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &old))
        return false;
    memset(reinterpret_cast<void*>(address), 0x90, size);
    VirtualProtect(reinterpret_cast<void*>(address), size, old, &old);
    return true;
}

inline bool Patch(uintptr_t address, const void* data, size_t size) {
    DWORD old;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &old))
        return false;
    memcpy(reinterpret_cast<void*>(address), data, size);
    VirtualProtect(reinterpret_cast<void*>(address), size, old, &old);
    return true;
}

inline bool Restore(uintptr_t address, const uint8_t* original, size_t size) {
    return Patch(address, original, size);
}

// Write a JMP rel32 (E9 xx xx xx xx) at 'from' that jumps to 'to'
inline bool WriteJmp(uintptr_t from, uintptr_t to) {
    DWORD old;
    if (!VirtualProtect(reinterpret_cast<void*>(from), 5, PAGE_EXECUTE_READWRITE, &old))
        return false;
    auto ptr = reinterpret_cast<uint8_t*>(from);
    ptr[0] = 0xE9;
    *reinterpret_cast<int32_t*>(ptr + 1) = static_cast<int32_t>(to - from - 5);
    VirtualProtect(reinterpret_cast<void*>(from), 5, old, &old);
    return true;
}

// Write a JMP + NOP padding (for hooks larger than 5 bytes)
inline bool WriteJmpPad(uintptr_t from, uintptr_t to, size_t totalSize) {
    DWORD old;
    if (!VirtualProtect(reinterpret_cast<void*>(from), totalSize, PAGE_EXECUTE_READWRITE, &old))
        return false;
    auto ptr = reinterpret_cast<uint8_t*>(from);
    ptr[0] = 0xE9;
    *reinterpret_cast<int32_t*>(ptr + 1) = static_cast<int32_t>(to - from - 5);
    for (size_t i = 5; i < totalSize; i++)
        ptr[i] = 0x90;
    VirtualProtect(reinterpret_cast<void*>(from), totalSize, old, &old);
    return true;
}

template<typename T>
inline T Read(uintptr_t address) {
    return *reinterpret_cast<T*>(address);
}

// Write with VirtualProtect — use for code patches only
template<typename T>
inline void Write(uintptr_t address, T value) {
    DWORD old;
    VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), PAGE_EXECUTE_READWRITE, &old);
    *reinterpret_cast<T*>(address) = value;
    VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), old, &old);
}

// Direct write — no VirtualProtect, for data pages (game state) in hot paths
template<typename T>
inline void WriteDirect(uintptr_t address, T value) {
    *reinterpret_cast<T*>(address) = value;
}

inline uintptr_t Deref(uintptr_t ptr) {
    return *reinterpret_cast<uintptr_t*>(ptr);
}

} // namespace mem
