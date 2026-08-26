#pragma once

#include <iostream>
#include <iomanip>
#include <algorithm>
#include "../Methods.h"
#include "Console.h"
#include "ServerConfiguration.h"
#include "RefereeBypass.h"
#include "Il2cppResolution.h"
#include "SendRequest.h"
#include "UIGradient.h"
#include "RadioPatch.h"
#include "../../Utils/globals.h"
#include "../../Utils/scan.h"
#include "../../Utils/deps/minhook/include/MinHook.h"

namespace RR::Patches {
    void Resolve() {
        RetAddr = PatternScan<uint64_t*>("FF 23", GA, NtHeaders->OptionalHeader.SizeOfImage);
        if (RetAddr) {
            std::cout << "[Campus] RetSpoof gadget (FF 23): 0x" << std::hex << (uintptr_t)RetAddr << std::dec << "\n";
        }
        else {
            std::cout << "[Campus] RetSpoof gadget (FF 23): NOT FOUND! Trying alternative...\n";
            RetAddr = PatternScan<uint64_t*>("FF 25", GA, NtHeaders->OptionalHeader.SizeOfImage);
            if (RetAddr) {
                std::cout << "[Campus] RetSpoof gadget (FF 25): 0x" << std::hex << (uintptr_t)RetAddr << std::dec << "\n";
            }
            else {
                std::cout << "[Campus] WARNING: No RetSpoof gadget found! spoof_call will fail!\n";
                RetAddr = PatternScan<uint64_t*>("E8 ?? ?? ?? ?? FF", GA, NtHeaders->OptionalHeader.SizeOfImage);
                if (RetAddr) {
                    std::cout << "[Campus] Found fallback gadget: 0x" << std::hex << (uintptr_t)RetAddr << std::dec << "\n";
                }
            }
        }

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

        if (!RR::Methods::Il2cpp::il2cpp_string_new ||
            !RR::Methods::Il2cpp::il2cpp_object_get_class ||
            !RR::Methods::Il2cpp::il2cpp_object_new) {
            std::cout << TAG_WARN() << "Some il2cpp functions failed to resolve! URL rewriting may fail.\n";
        }
    }

    static void hook(const std::string& tag, uintptr_t abs, void* detour, void** orig, const char* label) {
        if (!abs) {
            std::cout << tag << label << " - SKIPPED (address is null)\n";
            return;
        }
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

        // HTTP Response handler - GetDataAsText (for response body)
        hook(TAG_HTTP(), GA + RR::Methods::HttpResponse::get_DataAsText,
            (void*)GetDataAsText_H, (void**)&GetDataAsText_O, "HttpResponse.get_DataAsText");
        std::cout << "\n";

        // HTTP Response handler - Receive (for raw response data)
        hook(TAG_HTTP(), GA + RR::Methods::HttpResponse::Receive,
            (void*)Receive_H, (void**)&Receive_O, "HttpResponse.Receive");
        std::cout << "\n";

        // UI Gradient recolor
        hook(TAG_UI(), GA + RR::Methods::UIGradient::set_Gradient,
            (void*)UIGradient_set_Gradient_H, (void**)&UIGradient_set_Gradient_O,
            "UIGradient.set_Gradient");
        std::cout << "\n";

        // ================================================================
        // IMAGE SIGNATURE PATCH - HOOK APPROACH (PROVEN WORKING)
        // ================================================================
        // Forces the image signature verification to always return true.
        // This prevents image signature failures from stalling the request
        // pump and blocking room save data downloads.
        // ================================================================
        {
            uintptr_t verifyAddr = GA + RR::Methods::ImageSignature::Verify;
            if (verifyAddr) {
                MH_STATUS s = MH_CreateHook((void*)verifyAddr, (void*)VerifyImageSig_H, (LPVOID*)&VerifyImageSig_O);
                if (s == MH_OK) {
                    s = MH_EnableHook((void*)verifyAddr);
                    std::cout << "[IMAGES] " << "VerifyImageSig hook @ 0x" << std::hex << verifyAddr << std::dec
                        << " -> " << (s == MH_OK ? CLR_GREEN "OK" CLR_RESET : CLR_RED "FAIL" CLR_RESET) << "\n";
                }
                else {
                    std::cout << "[IMAGES] " << "VerifyImageSig hook creation FAILED (status: " << s << ")\n";
                }
            }
            else {
                std::cout << "[IMAGES] " << "VerifyImageSig address is null - hook skipped\n";
            }
        }

        // Radio Patches - DISABLED
        // RR::Patches::InstallRadioPatches();
        // std::cout << "\n";

        // FOV Patch - DISABLED (linker errors)
        // FOVPatch::InstallFOVPatch();
        // std::cout << "\n";

        std::cout << "\n";
        std::cout << TAG_INIT() << "all hooks installed  |  target = "
            << CLR_YELLOW << CAMPUS_NAMESERVER_HOST << CLR_RESET << "\n";
        std::cout << TAG_INIT() << "radio patches: DISABLED\n";
        std::cout << TAG_INIT() << "FOV patch: DISABLED\n";
        std::cout << TAG_INIT() << "ImageSignature: HOOK (forces true)\n";

        // Verify critical hooks
        if (!SendRequest_O) {
            std::cout << TAG_WARN() << "SendRequest hook failed! URL rewriting will not work.\n";
        }
        if (!GetDataAsText_O) {
            std::cout << TAG_WARN() << "GetDataAsText hook failed! Response parsing will not work.\n";
        }
        if (!VerifyImageSig_O) {
            std::cout << TAG_WARN() << "VerifyImageSig hook failed! Image signatures will not be bypassed.\n";
        }
    }
}