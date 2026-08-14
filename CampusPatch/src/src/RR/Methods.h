#pragma once
#include "../Utils/globals.h"

// All RVAs verified against DUMPER-7 runtime dump (Aug 2026)
// Game-code offsets identical to July 19 2025 reference build

namespace RR::Methods::Referee {
    uintptr_t Check1 = 0x76E850;
    uintptr_t Check2 = 0x2E6F680;
    uintptr_t Check3 = 0x2E6F670;
    uintptr_t Check4 = 0x2E6F6E0;
}

namespace RR::Methods::LegacyTlsAuthentication {
    uintptr_t NotifyServerCertificate = 0x77E5E30;  // NOP -> accept self-signed certs
}

namespace RR::Methods::HTTPManager {
    uintptr_t SendRequest = 0x77E0950;  // Hooked to redirect ns.rec.net -> Campus
}

namespace RR::Methods::RecNetHttp {
    uintptr_t ResponseCallback = 0x7CC2BD0;  // RecNet-managed request callback
}

namespace RR::Methods::HttpResponse {
    uintptr_t get_DataAsText = 0x77F4F80;  // Universal text response getter
    uintptr_t Receive = 0x77F42D0;         // Low-level body reader
}

namespace RR::Methods::System::Uri {
    uintptr_t ToString = 0x935AF50;
    uintptr_t ctor = 0x935C2A0;  // Uri..ctor(string) - single-arg overload
}

namespace RR::Methods::Photon {
    uintptr_t OLPEILEPEAD_JBNCMFDFDLM = 0x757E8C0;     // Realtime connect
    uintptr_t OEOMKFKDCCG_JBNCMFDFDLM = 0x759B710;     // PUN wrapper
    uintptr_t VoiceConnection_ConnectUsingSettings = 0x75F55D0;     // Photon.Voice
    uintptr_t PhotonVoiceNetwork_ConnectUsingSettings = 0x75FB2B0;  // Photon.Voice.PUN
}

namespace RR::Methods::AppSettingsFields {
    constexpr ptrdiff_t AppIdRealtime = 0x10;  // Il2CppString*
    constexpr ptrdiff_t AppIdChat = 0x18;      // Il2CppString*
    constexpr ptrdiff_t AppIdVoice = 0x20;     // Il2CppString*
    constexpr ptrdiff_t AppVersion = 0x28;     // Il2CppString*
}

namespace RR::Methods::Il2cpp {
    uintptr_t il2cpp_string_new;         // Resolved at runtime
    uintptr_t il2cpp_object_get_class;   // Resolved at runtime
    uintptr_t il2cpp_object_new;         // Resolved at runtime
}

namespace RR::Methods::UnityGradient {
    uintptr_t SetKeys = 0x9987030;        // void SetKeys(GradientColorKey[], GradientAlphaKey[])
    uintptr_t get_colorKeys = 0x9987110;  // GradientColorKey[] get_colorKeys()
    uintptr_t get_alphaKeys = 0x99870D0;  // GradientAlphaKey[] get_alphaKeys()
}

namespace RR::Methods::UIGradient {
    uintptr_t set_Gradient = 0x8BFA6A0;   // void set_Gradient(UnityEngine.Gradient)
}

namespace RR::Methods::ContentSig {
    // HTTP-level suppressor in SendRequest_H is active
    // DO NOT hook these - they crash the client
    // Toxic: ANMMDHBAEJA.Invoke @ 0xDA9A40 (breaks delegate Invoke stubs)
    // Toxic: Buffer.VerifySignature @ 0x79F8870 (breaks boot/autosave)
}