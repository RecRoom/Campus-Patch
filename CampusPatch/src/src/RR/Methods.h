#pragma once
#include "../Utils/globals.h"
#include <vector>
#include <cstdint>

// Referee anti-tamper checks - all get NOP'd
namespace RR::Methods::Referee {
    uintptr_t Check1 = 0x76E850;
    uintptr_t Check2 = 0x2E6F680;
    uintptr_t Check3 = 0x2E6F670;
    uintptr_t Check4 = 0x2E6F6E0;
}

// TLS certificate validation bypass
namespace RR::Methods::LegacyTlsAuthentication {
    uintptr_t NotifyServerCertificate = 0x77E5E30;
}

// HTTP request handler - redirects ns.rec.net to gameauth.ddns.net
namespace RR::Methods::HTTPManager {
    uintptr_t SendRequest = 0x77E0950;
}

// BestHTTP.HTTPRequest instance methods (RVAs from the runtime dump).
namespace RR::Methods::HTTPRequest {
    // void SetHeader(string name, string value) - replaces any existing value for that header.
    uintptr_t SetHeader = 0x77EE310;
}

// HTTP response data accessors
namespace RR::Methods::HttpResponse {
    uintptr_t get_DataAsText = 0x77F4F80;
    uintptr_t Receive = 0x77F42D0;
}

// System.Uri methods for URL manipulation
namespace RR::Methods::System::Uri {
    uintptr_t ToString = 0x935AF50;
    uintptr_t ctor = 0x935C2A0;
}

// IL2CPP API functions - resolved at runtime
namespace RR::Methods::Il2cpp {
    uintptr_t il2cpp_string_new;
    uintptr_t il2cpp_object_get_class;
    uintptr_t il2cpp_object_new;
}

// Unity Gradient methods - used for UI recoloring
namespace RR::Methods::UnityGradient {
    uintptr_t SetKeys = 0x9987030;
    uintptr_t get_colorKeys = 0x9987110;
    uintptr_t get_alphaKeys = 0x99870D0;
}

// UIGradient setter - hooked to recolor UI to Campus theme
namespace RR::Methods::UIGradient {
    uintptr_t set_Gradient = 0x8BFA6A0;
}

// RecNet.Images - Image loading system from dump
namespace RR::Methods::RecNetImages {
    // Main image loader methods
    uintptr_t LoadByName = 0x761D850;
    uintptr_t LoadRoom = 0x761D8E0;
    uintptr_t LoadInvention = 0x761D360;
    uintptr_t LoadPlayerEvent = 0x761D700;
    uintptr_t LoadAccount = 0x761D5C0;
    uintptr_t LoadAnnouncement = 0x761DB80;
    uintptr_t LoadBytes = 0x761D020;
    uintptr_t StaticConstructor = 0x761DDD0;

    // Async state machine MoveNext methods
    uintptr_t OHAPJOGMJPD_MoveNext = 0x761F400;
    uintptr_t NFHOMKFHHFJ_MoveNext = 0x761ECB0;
    uintptr_t HGKCHKDFPIK_MoveNext = 0x761E410;
    uintptr_t KCJFMDKJCJJ_MoveNext = 0x761E7B0;
    uintptr_t GMMBKKHKLFK_MoveNext = 0x761E060;
    uintptr_t NKJKLKMGNBL_MoveNext = 0x761F050;
    uintptr_t FILCOLNCBFO_MoveNext = 0x761C7B0;
}

// ================================================================
// IMAGE SIGNATURE - the server sends a content-signature header the
// client must accept, so the verifier's result has to be forced true.
//
// The hook approach (Verify) is PROVEN WORKING from the original
// Patches.h. It forces the verification function to always return true.
// ================================================================
namespace RR::Methods::ImageSignature {
    // The actual image signature verification function
    // RVA from the working build - this is what the hook uses
    // If this RVA is wrong for your build, find the correct one
    // by searching for the function that takes (byte[] data, byte[] sig)
    uintptr_t Verify = 0x7CF32F0;
}

// STEAMVR FOV - set_fieldOfView
namespace RR::Methods::SteamVR {
    // set_fieldOfView: [00007FF8571E06F0], RVA: [0xA906F0]
    uintptr_t SetFieldOfView = 0xA906F0;
}

// ================================================================
// PHOTON BACKEND SELECTION
//
// PhotonHost set -> DNS for *.photonengine / exitgames is redirected
// PhotonPort, if non-zero, overrides the connect port on AppSettings
// ================================================================

// Photon.Realtime.AppSettings -- field offsets from the runtime dump
namespace RR::Offsets::AppSettings {
    constexpr int AppIdRealtime = 16;   // Il2CppString*
    constexpr int AppIdChat = 24;   // Il2CppString*
    constexpr int AppIdVoice = 32;   // Il2CppString*
    constexpr int AppVersion = 40;   // Il2CppString*
    constexpr int UseNameServer = 48;   // bool
    constexpr int FixedRegion = 56;   // Il2CppString*
    constexpr int Server = 72;   // Il2CppString*
    constexpr int Port = 80;   // int32
}

// Photon methods - used for ConnectUsingSettings hook and diagnostic tracing
namespace RR::Methods::Photon {
    // ConnectUsingSettings seam - sets the port before connect
    uintptr_t ConnectUsingSettings = 0x757E8C0;

    // Diagnostic tracing (only used when EnableTracing = true)
    uintptr_t SendOperation = 0x7534610;
    uintptr_t EnqueueStatusCallback = 0x752C540;
    uintptr_t Protocol16_DeserializeOperationResponse = 0x75388D0;
    uintptr_t Protocol18_DeserializeOperationResponse = 0x753E610;
    uintptr_t Protocol16_DeserializeEventData = 0x7537E30;
    uintptr_t Protocol18_DeserializeEventData = 0x753E2F0;

    // NOT hooked: EnetPeer_IsTransportEncrypted - measured original=0
    // so datagram encryption was never the cause of Luxon's silence
    uintptr_t EnetPeer_IsTransportEncrypted = 0x7521380;
}

// HHFAEMGHOHN Enum - Image signature validation
enum class HHFAEMGHOHN : int {
    Obscured = 0,
    Time = 1,
    Inject = 2,
    GiftCount = 3,
    Engine = 4,
    UnknownDll = 5,
    ImageSignature = 6,
    AvatarHack = 7,
    NetworkCertificatePublicKey = 100,
    NetworkCertificateIssuer = 101,
    NetworkCertificateMissing = 102,
    NetworkCertificateMismatch = 103,
    AutosaveChecksumMismatch = 150,
    AutosaveSubRoomIdMismatch = 151,
    AutosaveChecksumException = 152,
    Photon_MissingHash = 200,
    Photon_CorruptHash = 201,
    Photon_DifferentHash = 203,
    Photon_ChatPuppeteering = 204,
    AppData_Runtime_LengthMismatch = 300,
    AppData_Runtime_LastWriteTimeMismatch = 301,
    AppData_Runtime_FileModified = 302,
    AppData_Boot_InvalidSignature = 310,
    AppData_Boot_UnableToVerifySignatures = 311,
    Config_MissingHash = 320,
    Config_DifferentHash = 321,
    Photon_InstantiateTool = 400,
    Memory_Hash_Mismatch = 500,
    Driver_Invalid_Signature = 600,
    Native_Memory_Hash_Mismatch = 700,
    Native_Numbers_Mismatch = 701,
    Native_Requirements_Failed = 702,
    Referee_TelemetryEvents = 800,
};