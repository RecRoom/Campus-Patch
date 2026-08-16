#pragma once
#include "Inc/Includes.h"

// These are NOT initialized at DLL load time. Suspended-start injection would
// run GetModuleHandle when the game hasn't loaded modules yet, causing NULL
// dereferences. Instead we set these in InitGlobals() from InitThread().

uintptr_t GA = 0;
const IMAGE_NT_HEADERS* NtHeaders = nullptr;
uintptr_t Referee = 0;
uint64_t* RetAddr = nullptr;

// Called from InitThread after GameAssembly and Referee are loaded.
// Verifies both modules exist and GameAssembly has valid PE headers.

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
    return *(T*)((std::uint8_t*)obj + offset);
}

template <typename T>
void set(void* obj, std::ptrdiff_t offset, T val) {
    *(T*)((std::uint8_t*)obj + offset) = val;
}

// FIXED: ReadIl2CppStringOwned - safely reads IL2CPP string
// The raw ReadIl2CppString has been removed to prevent memory leaks
inline std::string ReadIl2CppStringOwned(Il2cppString* il2cppString) {
    if (!il2cppString) {
        return {};
    }

    size_t length = static_cast<size_t>(il2cppString->Length);
    const size_t bufferSize = (length + 1) * sizeof(wchar_t);
    char* internalString = new char[bufferSize];
    size_t convertedChars = 0;

    errno_t result = wcstombs_s(&convertedChars, internalString, bufferSize,
        reinterpret_cast<wchar_t*>(il2cppString) + 0xA, length);

    if (result != 0) {
        delete[] internalString;
        return {};
    }

    std::string out(internalString);
    delete[] internalString;
    return out;
}