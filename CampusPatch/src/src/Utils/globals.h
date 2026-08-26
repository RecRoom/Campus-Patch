#pragma once
#include "Inc/Includes.h"

uintptr_t GA = 0;
const IMAGE_NT_HEADERS* NtHeaders = nullptr;
uintptr_t Referee = 0;
uint64_t* RetAddr = nullptr;

static bool InitGlobals() {
    GA = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    Referee = (uintptr_t)GetModuleHandleA("Referee.dll");

    if (!GA) {
        printf("[Campus] init: GameAssembly.dll not loaded\n");
        return false;
    }
    if (!Referee) {
        printf("[Campus] init: Referee.dll not loaded\n");
        return false;
    }

    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(GA);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        printf("[Campus] init: GameAssembly has bad DOS header\n");
        return false;
    }

    NtHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(GA + dos->e_lfanew);
    if (NtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        printf("[Campus] init: GameAssembly has bad NT header\n");
        NtHeaders = nullptr;
        return false;
    }

    printf("[Campus] init: GameAssembly=%p Referee=%p SizeOfImage=0x%X\n",
        (void*)GA, (void*)Referee, NtHeaders->OptionalHeader.SizeOfImage);
    return true;
}

template <typename T>
T read(void* obj, std::ptrdiff_t offset) {
    if (!obj) return T{};
    return *(T*)((std::uint8_t*)obj + offset);
}

template <typename T>
void set(void* obj, std::ptrdiff_t offset, T val) {
    if (!obj) return;
    *(T*)((std::uint8_t*)obj + offset) = val;
}

// ReadIl2CppString - THIS IS THE ORIGINAL WORKING VERSION
inline std::string ReadIl2CppString(Il2cppString* il2cppString) {
    if (!il2cppString) {
        return {};
    }

    size_t length = static_cast<size_t>(il2cppString->Length);
    if (length == 0 || length > 4096) {
        return {};
    }

    // String data starts at offset 4 (after the Length int)
    const wchar_t* wstr = reinterpret_cast<const wchar_t*>(reinterpret_cast<uintptr_t>(il2cppString) + 4);

    // Convert to UTF-8
    int requiredSize = WideCharToMultiByte(CP_UTF8, 0, wstr, (int)length, nullptr, 0, nullptr, nullptr);
    if (requiredSize <= 0) {
        return {};
    }

    std::string result(requiredSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, (int)length, &result[0], requiredSize, nullptr, nullptr);
    return result;
}

// Alias for compatibility
inline std::string ReadIl2CppStringOwned(Il2cppString* il2cppString) {
    return ReadIl2CppString(il2cppString);
}