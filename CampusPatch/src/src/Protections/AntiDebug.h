#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <tlhelp32.h>
#include <iostream>
#include "../RR/Patching/Console.h"

namespace AntiDebug {

    // List of process names to detect (HTTP debuggers, cheat tools, etc.)
    static const std::vector<std::string> kBlacklistedProcesses = {
        // HTTP Debuggers / Proxies
        "HTTPDebugger",
        "HTTPDebuggerUI",
        "HTTPDebuggerSvc",
        "Fiddler",
        "FiddlerEverywhere",
        "Charles",
        "BurpSuite",
        "Wireshark",
        "Tcpdump",
        "HTTPTool",
        "Proxifier",
        "SocksCap",
        "ProxyCap",

        // Cheat Tools
        "CheatEngine",
        "cheatengine-x86_64",
        "DBG",
        "x64dbg",
        "x32dbg",
        "OllyDbg",
        "IDA",
        "ida64",
        "Radare2",
        "Ghidra",

        // Memory Editors
        "ArtMoney",
        "ScanMem",
        "MHS",
        "TSearch",

        // DLL Injectors
        "ExtremeInjector",
        "Xenos",
        "Injector",
        "DLLInject",

        // Other
        "ProcessHacker",
        "ProcessMonitor",
        "API Monitor",
        "WinDbg",
        "Sysinternals"
    };

    static bool IsProcessRunning(const std::string& processName) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return false;
        }

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &pe32)) {
            do {
                std::string currentProcess = pe32.szExeFile;
                if (currentProcess.find(processName) != std::string::npos) {
                    CloseHandle(hSnapshot);
                    return true;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
        return false;
    }

    static void ShowAutoClosePopup(const std::string& processName) {
        std::string message = "Kurt Cobain says: An unauthorized application was detected.\n\n";
        message += "Process: " + processName + "\n";
        message += "Campus doesn't allow unauthorized applications.\n\n";
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

    static bool PerformAntiDebugCheck() {
        std::cout << TAG_ANTI() << "Kurt Cobain is running anti-debug checks...\n";

        for (const auto& process : kBlacklistedProcesses) {
            if (IsProcessRunning(process)) {
                std::cout << TAG_ANTI() << "Kurt Cobain detected: " << process << "\n";
                ShowAutoClosePopup(process);
                return false;
            }
        }

        std::cout << TAG_ANTI() << "Kurt Cobain says: Anti-debug checks passed.\n";
        return true;
    }

    static void AntiDebugThread() {
        while (true) {
            Sleep(5000);

            for (const auto& process : kBlacklistedProcesses) {
                if (IsProcessRunning(process)) {
                    std::cout << TAG_ANTI() << "Kurt Cobain detected: " << process << "\n";
                    ShowAutoClosePopup(process);
                    return;
                }
            }
        }
    }

    static void StartAntiDebugMonitoring() {
        std::cout << TAG_ANTI() << "Kurt Cobain is starting anti-debug monitoring...\n";
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            AntiDebugThread();
            return 0;
            }, nullptr, 0, nullptr);
    }

} // namespace AntiDebug