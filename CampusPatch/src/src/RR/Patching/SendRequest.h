#pragma once

#include <iostream>
#include <string>
#include "../Methods.h"
#include "Console.h"
#include "ServerConfiguration.h"
#include "HttpDebug.h"
#include "RefereeBypass.h"
#include "../../Utils/globals.h"
#include "../../Utils/deps/spoofcall/RetSpoof.hpp"
#include "../../Utils/Inc/Includes.h"

// Forward declaration — ensures Il2cppString resolves at global scope in this TU
// even if IntelliSense/compile order doesn't pick it up from Includes.h.
// Safe no-op if it's already fully declared upstream (this is just a forward decl).
struct Il2cppString;

// SendRequest Hook - Redirects ns.rec.net to gameauth.ddns.net
void (*SendRequest_O)(void*);

void SendRequest_H(void* request) {
    void* uri = read<void*>(request, 16);
    if (!uri) {
        return SendRequest_O(request);
    }

    using ToStringFn = ::Il2cppString* (*)(void*);
    auto toStringFn = reinterpret_cast<ToStringFn>(GA + RR::Methods::System::Uri::ToString);

    ::Il2cppString* Uri = spoof_call((const void*)RetAddr, toStringFn, uri);
    if (!Uri) {
        return SendRequest_O(request);
    }

    std::string url = ReadIl2CppStringOwned(Uri);
    std::cout << TAG_NET() << "URI: " << url << "\n";

    // ============================================================
    // Normal ns.rec.net redirect for game servers
    // ============================================================
    const std::string find = "ns.rec.net";
    auto findPos = url.find(find);

    if (findPos != std::string::npos) {
        // Drop hile reports
        if (url.find("/api/PlayerReporting/v1/hile") != std::string::npos ||
            url.find("/api/PlayerReporting/v1/referee") != std::string::npos) {
            std::cout << TAG_HILE() << "DROPPED " << CLR_DIM << url << CLR_RESET << "\n";
            return;
        }

        std::string newUrl = url;
        newUrl.replace(findPos, find.length(), CAMPUS_NAMESERVER_HOST);

        // For image URLs, remove the signature parameter
        bool isImage = url.find("/img/") != std::string::npos ||
            url.find(".png") != std::string::npos ||
            url.find(".jpg") != std::string::npos ||
            url.find(".jpeg") != std::string::npos;

        if (isImage) {
            size_t sigPos = newUrl.find("&sig=");
            if (sigPos == std::string::npos) {
                sigPos = newUrl.find("?sig="); // handles sig being the first/only query param
            }
            if (sigPos != std::string::npos) {
                newUrl = newUrl.substr(0, sigPos);
                std::cout << TAG_IMAGES() << "[!] Removed signature from URL\n";
            }
        }

        std::cout << TAG_NET() << "REWRITTEN: " << newUrl << "\n";

        using Il2cppStringNewFn = ::Il2cppString* (*)(const char*);
        auto strNew = reinterpret_cast<Il2cppStringNewFn>(GA + RR::Methods::Il2cpp::il2cpp_string_new);
        ::Il2cppString* NewStr = spoof_call((const void*)RetAddr, strNew, newUrl.c_str());

        if (NewStr) {
            using ObjectGetClassFn = void* (*)(void*);
            auto getClass = reinterpret_cast<ObjectGetClassFn>(GA + RR::Methods::Il2cpp::il2cpp_object_get_class);
            void* UriClass = spoof_call((const void*)RetAddr, getClass, uri);

            if (UriClass) {
                using ObjectNewFn = void* (*)(void*);
                auto objNew = reinterpret_cast<ObjectNewFn>(GA + RR::Methods::Il2cpp::il2cpp_object_new);
                void* NewUri = spoof_call((const void*)RetAddr, objNew, UriClass);

                if (NewUri) {
                    using UriCtor = void (*)(void*, ::Il2cppString*);
                    auto uriCtor = reinterpret_cast<UriCtor>(GA + RR::Methods::System::Uri::ctor);
                    spoof_call((const void*)RetAddr, uriCtor, NewUri, NewStr);
                    set<void*>(request, 16, NewUri);
                    std::cout << TAG_NET() << "[+] Host swap applied — request will go to " << newUrl << "\n";
                }
                else {
                    std::cout << TAG_WARN() << "il2cpp_object_new returned null — REWRITTEN was logged but swap NOT applied. Original request still points at ns.rec.net.\n";
                }
            }
            else {
                std::cout << TAG_WARN() << "il2cpp_object_get_class returned null — REWRITTEN was logged but swap NOT applied. Original request still points at ns.rec.net.\n";
            }
        }
        else {
            std::cout << TAG_WARN() << "il2cpp_string_new returned null — REWRITTEN was logged but swap NOT applied. Original request still points at ns.rec.net.\n";
        }
    }

    return SendRequest_O(request);
}

// Hook for HTTP Receive
bool (*Receive_O)(void*, int32_t, bool) = nullptr;
bool Receive_H(void* self, int32_t forceReadRawContentLength, bool readPayloadData) {
    if (Receive_O) {
        return Receive_O(self, forceReadRawContentLength, readPayloadData);
    }
    return false;
}

// Hook for GetDataAsText - Fixed declaration
// NOTE: not currently wired up via MH_CreateHook in PatchInstaller.h.
// You'll need the real function address before this fires — but once
// hooked, this will log every response body coming back from the
// server, which is what you're currently missing visibility into.
::Il2cppString* (*GetDataAsText_O)(void*) = nullptr;
::Il2cppString* GetDataAsText_H(void* self) {
    ::Il2cppString* result = nullptr;
    if (GetDataAsText_O) {
        result = GetDataAsText_O(self);
    }
    if (result) {
        std::string body = ReadIl2CppStringOwned(result);
        std::cout << TAG_NET() << "RESPONSE: " << (body.size() > 500 ? body.substr(0, 500) + "...(truncated)" : body) << "\n";
    }
    else {
        std::cout << TAG_WARN() << "GetDataAsText returned null response\n";
    }
    return result;
}

// RecNet Response
void (*RecNetResponse_O)(void*, void*, void*) = nullptr;
void RecNetResponse_H(void* request, void* response, void* promise) {
    if (RecNetResponse_O) {
        RecNetResponse_O(request, response, promise);
    }
}

thread_local bool g_inReceive = false;
thread_local bool g_inGetDataAsText = false;