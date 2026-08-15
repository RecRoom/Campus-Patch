#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <string>
#include <iostream>
#include "../RR/Patching/Console.h"

namespace AntiSandbox {

    static bool IsVirtualMachine() {
        int cpuInfo[4] = { 0, 0, 0, 0 };
        __cpuid(cpuInfo, 1);
        return (cpuInfo[2] & (1 << 31)) != 0;
    }

    static bool IsVMViaStrings() {
        const char* vmProcesses[] = {
            "vmtoolsd.exe",
            "VBoxService.exe",
            "VBoxTray.exe",
            "vmsrvc.exe",
            "vmware.exe",
            "vmwaretray.exe",
            "xenservice.exe",
            "vboxguest.exe",
            "vboxmouse.exe",
            "vm3dservice.exe",
            "vmtools.exe",
            nullptr
        };

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32 pe32 = {};
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &pe32)) {
            do {
                std::string name = pe32.szExeFile;
                for (int i = 0; vmProcesses[i]; i++) {
                    if (name.find(vmProcesses[i]) != std::string::npos) {
                        CloseHandle(hSnapshot);
                        return true;
                    }
                }
            } while (Process32Next(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
        return false;
    }

    static bool IsVMViaDevices() {
        const char* vmDevices[] = {
            "VMware",
            "VirtualBox",
            "VBox",
            "Xen",
            "QEMU",
            "Hyper-V",
            "Virtual",
            nullptr
        };

        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char biosVendor[256];
            DWORD size = sizeof(biosVendor);
            if (RegQueryValueExA(hKey, "SystemManufacturer", nullptr, nullptr, (LPBYTE)biosVendor, &size) == ERROR_SUCCESS) {
                std::string vendor = biosVendor;
                for (int i = 0; vmDevices[i]; i++) {
                    if (vendor.find(vmDevices[i]) != std::string::npos) {
                        RegCloseKey(hKey);
                        return true;
                    }
                }
            }
            RegCloseKey(hKey);
        }
        return false;
    }

    static bool IsLowMemoryVM() {
        MEMORYSTATUSEX memStatus = {};
        memStatus.dwLength = sizeof(memStatus);
        GlobalMemoryStatusEx(&memStatus);
        return memStatus.ullTotalPhys < 2ULL * 1024 * 1024 * 1024;
    }

    static bool IsVMViaTSC() {
        LARGE_INTEGER start, end;
        QueryPerformanceCounter(&start);
        QueryPerformanceCounter(&end);

        for (int i = 0; i < 100000; i++) { volatile int x = i * i; }

        LARGE_INTEGER end2;
        QueryPerformanceCounter(&end2);

        LONGLONG diff1 = end.QuadPart - start.QuadPart;
        LONGLONG diff2 = end2.QuadPart - start.QuadPart;

        return diff2 - diff1 > diff1 * 2;
    }

    static void ShowAutoClosePopup() {
        MessageBoxA(
            NULL,
            "Kurt Cobain says: Virtual machine detected.\n\n"
            "Campus doesn't allow Virtual Machines.\n\n"
            "This window will close automatically.",
            "Kurt Cobain's Drug Problem",
            MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SYSTEMMODAL
        );

        Sleep(3000);
        TerminateProcess(GetCurrentProcess(), 1);
    }

    static bool PerformAntiSandboxChecks() {
        std::cout << TAG_ANTI() << "Kurt Cobain is running anti-sandbox checks...\n";

        if (IsVirtualMachine()) {
            std::cout << TAG_ANTI() << "Kurt Cobain detected VM (CPUID)\n";
            ShowAutoClosePopup();
            return false;
        }

        if (IsVMViaStrings()) {
            std::cout << TAG_ANTI() << "Kurt Cobain detected VM (processes)\n";
            ShowAutoClosePopup();
            return false;
        }

        if (IsVMViaDevices()) {
            std::cout << TAG_ANTI() << "Kurt Cobain detected VM (devices)\n";
            ShowAutoClosePopup();
            return false;
        }

        if (IsLowMemoryVM()) {
            std::cout << TAG_ANTI() << "Kurt Cobain detected VM (low memory)\n";
            ShowAutoClosePopup();
            return false;
        }

        std::cout << TAG_ANTI() << "Kurt Cobain says: Anti-sandbox checks passed.\n";
        return true;
    }

    static void AntiSandboxThread() {
        while (true) {
            Sleep(5000);

            if (IsVirtualMachine() ||
                IsVMViaStrings() ||
                IsVMViaDevices() ||
                IsLowMemoryVM()) {

                std::cout << TAG_ANTI() << "Kurt Cobain detected VM during monitoring\n";
                ShowAutoClosePopup();
                return;
            }
        }
    }

    static void StartMonitoring() {
        std::cout << TAG_ANTI() << "Kurt Cobain is starting anti-sandbox monitoring...\n";
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            AntiSandboxThread();
            return 0;
            }, nullptr, 0, nullptr);
    }

} // namespace AntiSandbox