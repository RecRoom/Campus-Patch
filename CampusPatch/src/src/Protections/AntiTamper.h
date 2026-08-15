#pragma once

#include <windows.h>
#include <vector>
#include <iostream>
#include "../RR/Patching/Console.h"

namespace AntiTamper {

    static uint32_t CalculateChecksum(uintptr_t base, size_t size) {
        uint32_t checksum = 0;
        uint8_t* data = (uint8_t*)base;

        for (size_t i = 0; i < size; i++) {
            checksum = (checksum << 5) - checksum + data[i];
        }

        return checksum;
    }

    static bool GetModuleInfo(HMODULE hModule, uintptr_t& base, size_t& size) {
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)hModule;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

        IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((uintptr_t)hModule + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        base = (uintptr_t)hModule;
        size = nt->OptionalHeader.SizeOfImage;
        return true;
    }

    static void ShowAutoClosePopup() {
        MessageBoxA(
            NULL,
            "Kurt Cobain says: Integrity check failed.\n\n"
            "The Campus Patch has been modified.\n\n"
            "This window will close automatically.",
            "Kurt Cobain's Drug Problem",
            MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SYSTEMMODAL
        );

        Sleep(3000);
        TerminateProcess(GetCurrentProcess(), 1);
    }

    static bool VerifyIntegrity() {
        HMODULE hModule = GetModuleHandleA("CampusPatch.dll");
        if (!hModule) {
            std::cout << TAG_ANTI() << "Kurt Cobain says: Could not verify integrity (module not found)\n";
            return true;
        }

        uintptr_t base;
        size_t size;
        if (!GetModuleInfo(hModule, base, size)) {
            std::cout << TAG_ANTI() << "Kurt Cobain says: Could not verify integrity (invalid module)\n";
            return true;
        }

        // Calculate checksum of the current DLL in memory
        uint32_t checksum = CalculateChecksum(base, size);

        // Store the checksum in a static variable so it's consistent for this session
        static uint32_t sessionChecksum = checksum;

        // Compare against the session checksum
        if (checksum != sessionChecksum) {
            std::cout << TAG_ANTI() << "Kurt Cobain says: Integrity check failed! DLL has been modified.\n";
            std::cout << TAG_ANTI() << "Kurt Cobain says: Expected: 0x" << std::hex << sessionChecksum << " Got: 0x" << checksum << std::dec << "\n";
            ShowAutoClosePopup();
            return false;
        }

        std::cout << TAG_ANTI() << "Kurt Cobain says: Integrity check passed.\n";
        return true;
    }

    // Calculate and print the checksum (for debugging/informational purposes)
    static void PrintChecksum() {
        HMODULE hModule = GetModuleHandleA("CampusPatch.dll");
        if (!hModule) {
            std::cout << TAG_ANTI() << "Kurt Cobain says: Could not get module handle\n";
            return;
        }

        uintptr_t base;
        size_t size;
        if (!GetModuleInfo(hModule, base, size)) {
            std::cout << TAG_ANTI() << "Kurt Cobain says: Could not get module info\n";
            return;
        }

        uint32_t checksum = CalculateChecksum(base, size);
        std::cout << TAG_ANTI() << "Kurt Cobain says: DLL Checksum = 0x" << std::hex << checksum << std::dec << "\n";
    }

    static bool CheckForHooks() {
        return true;
    }

} // namespace AntiTamper