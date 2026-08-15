#pragma once

#include <iostream>
#include <iomanip>
#include "../Methods.h"
#include "Console.h"
#include "ServerConfiguration.h"
#include "RefereeBypass.h"
#include "Il2cppResolution.h"
#include "SendRequest.h"
#include "UIGradient.h"
#include "PhotonRedirection.h"
#include "../../Utils/globals.h"
#include "../../Utils/deps/minhook/include/MinHook.h"

// Patch Installer
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

    // FIXED: hook function now accepts std::string instead of const char*
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

        // Referee bypass
        hook(TAG_REFEREE(), Referee + RR::Methods::Referee::Check1, (void*)nop_H, (void**)&nop_O, "Check1");
        hook(TAG_REFEREE(), Referee + RR::Methods::Referee::Check2, (void*)nop_H, (void**)&nop_O, "Check2");
        hook(TAG_REFEREE(), Referee + RR::Methods::Referee::Check3, (void*)nop_H, (void**)&nop_O, "Check3");
        hook(TAG_REFEREE(), Referee + RR::Methods::Referee::Check4, (void*)nop_H, (void**)&nop_O, "Check4");
        std::cout << "\n";

        // TLS bypass
        hook(TAG_TLS(), GA + RR::Methods::LegacyTlsAuthentication::NotifyServerCertificate,
            (void*)nop_H, (void**)&nop_O, "NotifyServerCertificate (accept self-signed)");
        std::cout << "\n";

        // URI rewrite
        hook(TAG_HTTP(), GA + RR::Methods::HTTPManager::SendRequest,
            (void*)SendRequest_H, (void**)&SendRequest_O, "HTTPManager.SendRequest (URI rewrite)");

        // DISABLED - RecNet response callback breaks the game
        // hook(TAG_HTTP(), GA + RR::Methods::RecNetHttp::ResponseCallback,
        //      (void*)RecNetResponse_H, (void**)&RecNetResponse_O, "RecNet response callback");

        // DISABLED - HTTP response hooks break the game
        // hook(TAG_HTTP(), GA + RR::Methods::HttpResponse::Receive,
        //      (void*)Receive_H, (void**)&Receive_O, "HTTPResponse.Receive");
        // hook(TAG_HTTP(), GA + RR::Methods::HttpResponse::get_DataAsText,
        //      (void*)GetDataAsText_H, (void**)&GetDataAsText_O, "HTTPResponse.get_DataAsText");
        std::cout << "\n";

        // Gradient recolor
        hook(TAG_UI(), GA + RR::Methods::UIGradient::set_Gradient,
            (void*)UIGradient_set_Gradient_H, (void**)&UIGradient_set_Gradient_O,
            "UIGradient.set_Gradient (recolor)");
        std::cout << "\n";

        // Photon redirect
        hook(TAG_PHOTON(), GA + RR::Methods::Photon::OLPEILEPEAD_JBNCMFDFDLM,
            (void*)RealtimeConnect_H, (void**)&RealtimeConnect_O, "Realtime connect");
        hook(TAG_PHOTON(), GA + RR::Methods::Photon::OEOMKFKDCCG_JBNCMFDFDLM,
            (void*)PunConnect_H, (void**)&PunConnect_O, "PUN connect");
        hook(TAG_PHOTON(), GA + RR::Methods::Photon::VoiceConnection_ConnectUsingSettings,
            (void*)VoiceConnect_H, (void**)&VoiceConnect_O, "Voice connect");
        hook(TAG_PHOTON(), GA + RR::Methods::Photon::PhotonVoiceNetwork_ConnectUsingSettings,
            (void*)VoicePUNConnect_H, (void**)&VoicePUNConnect_O, "Voice PUN connect");
        std::cout << "\n";

        std::cout << TAG_INIT() << "all hooks installed  |  target = "
            << CLR_YELLOW << CAMPUS_NAMESERVER_HOST << CLR_RESET << "\n";
    }
}