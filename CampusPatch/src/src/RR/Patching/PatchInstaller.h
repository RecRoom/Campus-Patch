#pragma once

#include <iostream>
#include <iomanip>
#include <algorithm>  // Added for std::transform
#include "../Methods.h"
#include "Console.h"
#include "ServerConfiguration.h"
#include "RefereeBypass.h"
#include "Il2cppResolution.h"
#include "SendRequest.h"
#include "UIGradient.h"
#include "RadioPatch.h"
#include "../../Utils/globals.h"
#include "../../Utils/scan.h"  // Added for PatternScan
#include "../../Utils/deps/minhook/include/MinHook.h"

namespace RR::Patches {
    void Resolve() {
        RetAddr = PatternScan<uint64_t*>("FF 23", GA, NtHeaders->OptionalHeader.SizeOfImage);
        std::cout << "[Campus] RetSpoof gadget (FF 23): 0x" << std::hex << (uintptr_t)RetAddr << std::dec << "\n";
        auto assm = GetModuleHandleA("GameAssembly.dll");

        auto tryExport = [&](const char* name) -> uintptr_t {
            void* p = spoof_call(RetAddr, GetProcAddress, assm, name);
            return p ? (uintptr_t)p - GA : 0;
            };
        RR::Methods::Il2cpp::il2cpp_string_new = tryExport("il2cpp_string_new");
        RR::Methods::Il2cpp::il2cpp_object_get_class = tryExport("il2cpp_object_get_class");
        RR::Methods::Il2cpp::il2cpp_object_new = tryExport("il2cpp_object_new");

        auto fallback = [&](const char* name, uintptr_t& slot) {
            if (slot) return;
            void* p = UnityPlayerScan::find_il2cpp_api(name);
            if (p) slot = (uintptr_t)p - GA;
            };
        fallback("il2cpp_string_new", RR::Methods::Il2cpp::il2cpp_string_new);
        fallback("il2cpp_object_get_class", RR::Methods::Il2cpp::il2cpp_object_get_class);
        fallback("il2cpp_object_new", RR::Methods::Il2cpp::il2cpp_object_new);

        auto mark = [](uintptr_t v) {
            return v ? (CLR_GREEN "OK" CLR_RESET) : (CLR_RED "FAIL" CLR_RESET);
            };
        std::cout << TAG_INIT() << "il2cpp: string_new " << mark(RR::Methods::Il2cpp::il2cpp_string_new)
            << "  object_get_class " << mark(RR::Methods::Il2cpp::il2cpp_object_get_class)
            << "  object_new " << mark(RR::Methods::Il2cpp::il2cpp_object_new)
            << "\n";
    }

    static void hook(const std::string& tag, uintptr_t abs, void* detour, void** orig, const char* label) {
        MH_STATUS s = MH_CreateHook((void*)abs, detour, (LPVOID*)orig);
        if (s == MH_OK) s = MH_EnableHook((void*)abs);
        std::cout << tag << label
            << CLR_DIM " @ 0x" << std::hex << abs << std::dec << CLR_RESET
            << "  -> " << (s == MH_OK ? CLR_GREEN "OK" CLR_RESET
                : CLR_ERR "FAIL" CLR_RESET) << "\n";
    }

    void Patch() {
        if (MH_Initialize() != MH_OK) {
            std::cout << "[Campus] MH_Initialize FAILED\n";
            return;
        }

        std::cout << "\n";

        // Referee anti-tamper checks - NOP all 4
        hook(TAG_REFEREE(), Referee + RR::Methods::Referee::Check1, (void*)nop_H, (void**)&nop_O, "Check1");
        hook(TAG_REFEREE(), Referee + RR::Methods::Referee::Check2, (void*)nop_H, (void**)&nop_O, "Check2");
        hook(TAG_REFEREE(), Referee + RR::Methods::Referee::Check3, (void*)nop_H, (void**)&nop_O, "Check3");
        hook(TAG_REFEREE(), Referee + RR::Methods::Referee::Check4, (void*)nop_H, (void**)&nop_O, "Check4");
        std::cout << "\n";

        // TLS certificate validation bypass
        hook(TAG_TLS(), GA + RR::Methods::LegacyTlsAuthentication::NotifyServerCertificate,
            (void*)nop_H, (void**)&nop_O, "NotifyServerCertificate");
        std::cout << "\n";

        // HTTP request handler - redirects ns.rec.net to gameauth.ddns.net
        hook(TAG_HTTP(), GA + RR::Methods::HTTPManager::SendRequest,
            (void*)SendRequest_H, (void**)&SendRequest_O, "HTTPManager.SendRequest");
        std::cout << "\n";

        // UI Gradient recolor
        hook(TAG_UI(), GA + RR::Methods::UIGradient::set_Gradient,
            (void*)UIGradient_set_Gradient_H, (void**)&UIGradient_set_Gradient_O,
            "UIGradient.set_Gradient");
        std::cout << "\n";

        // IMAGE SIGNATURE PATCH - DISABLED
        std::cout << "[IMAGES] " << "[*] ImageSignature patch: DISABLED\n";

        // Radio Patches - DISABLED
        // RR::Patches::InstallRadioPatches();
        // std::cout << "\n";

        // FOV Patch - DISABLED (linker errors)
        // FOVPatch::InstallFOVPatch();
        // std::cout << "\n";

        std::cout << "\n";
        std::cout << TAG_INIT() << "all hooks installed  |  target = "
            << CLR_YELLOW << CAMPUS_NAMESERVER_HOST << CLR_RESET << "\n";
        std::cout << TAG_INIT() << "image signature patch: DISABLED\n";
        std::cout << TAG_INIT() << "radio patches: DISABLED\n";
        std::cout << TAG_INIT() << "FOV patch: DISABLED\n";
    }
}