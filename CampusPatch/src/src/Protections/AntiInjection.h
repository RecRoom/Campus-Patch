#pragma once

#include <windows.h>
#include <set>
#include <string>
#include <iostream>
#include "../RR/Patching/Console.h"

namespace AntiInjection {

    static const std::set<std::string> kAllowedDLLs = {
        "GameAssembly.dll",
        "UnityPlayer.dll",
        "Referee.dll",
        "kernel32.dll",
        "user32.dll",
        "ntdll.dll",
        "CampusPatch.dll"
    };

    static bool IsDLLAllowed(const std::string& dllName) {
        for (const auto& allowed : kAllowedDLLs) {
            if (dllName.find(allowed) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    static void ShowAutoClosePopup(const std::string& dllName) {
        std::string message = "Kurt Cobain says: An unauthorized DLL was detected.\n\n";
        message += "DLL: " + dllName + "\n";
        message += "Campus doesn't allow Unauthorized DLLs.\n\n";
        message += "This window will close automatically.";

        MessageBoxA(
            NULL,
            message.c_str(),
            "Kurt Cobain's Drug Problem",
            MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SYSTEMMODAL
        );

        Sleep(3000);
        TerminateProcess(GetCurrentProcess(), 1);
    }

    static HMODULE WINAPI LoadLibraryHook(LPCSTR lpLibFileName) {
        std::string dllName = lpLibFileName;

        size_t slash = dllName.find_last_of("\\/");
        if (slash != std::string::npos) {
            dllName = dllName.substr(slash + 1);
        }

        if (!IsDLLAllowed(dllName)) {
            std::cout << TAG_ANTI() << "Kurt Cobain blocked unauthorized DLL: " << dllName << "\n";
            ShowAutoClosePopup(dllName);
            return NULL;
        }

        return LoadLibraryA(lpLibFileName);
    }

    static HMODULE WINAPI LoadLibraryWHook(LPCWSTR lpLibFileName) {
        char dllName[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, lpLibFileName, -1, dllName, MAX_PATH, NULL, NULL);

        std::string dllNameStr = dllName;

        size_t slash = dllNameStr.find_last_of("\\/");
        if (slash != std::string::npos) {
            dllNameStr = dllNameStr.substr(slash + 1);
        }

        if (!IsDLLAllowed(dllNameStr)) {
            std::cout << TAG_ANTI() << "Kurt Cobain blocked unauthorized DLL (W): " << dllNameStr << "\n";
            ShowAutoClosePopup(dllNameStr);
            return NULL;
        }

        return LoadLibraryW(lpLibFileName);
    }

    static void Install() {
        std::cout << TAG_ANTI() << "Kurt Cobain is installing anti-injection protections...\n";
        std::cout << TAG_ANTI() << "Allowed DLLs: ";
        for (const auto& dll : kAllowedDLLs) {
            std::cout << dll << " ";
        }
        std::cout << "\n";

        std::cout << TAG_ANTI() << "Kurt Cobain says: Anti-injection protections installed.\n";
    }

} // namespace AntiInjection