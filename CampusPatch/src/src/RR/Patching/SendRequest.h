#pragma once

#include <iostream>
#include <string>
#include "../Methods.h"
#include "Console.h"
#include "ServerConfiguration.h"
#include "HttpDebug.h"
#include "RefereeBypass.h"
#include "../../Utils/globals.h"

// SendRequest Hook - Redirects ns.rec.net to gameauth.ddns.net
void (*SendRequest_O)(void*);

void SendRequest_H(void* request) {
    void* uri = read<void*>(request, 16);
    if (uri) {
        using ToStringFn = Il2cppString * (*)(void*);
        auto toStringFn = reinterpret_cast<ToStringFn>(GA + RR::Methods::System::Uri::ToString);

        Il2cppString* Uri = spoof_call(RetAddr, toStringFn, uri);
        std::string url = ReadIl2CppStringOwned(Uri);

        std::cout << TAG_NET() << "URI: " << url << "\n";

        const std::string find = "ns.rec.net";
        auto findPos = url.find(find);

        if (findPos != std::string::npos) {
            // Drop hile reports
            if (url.find("/api/PlayerReporting/v1/hile") != std::string::npos ||
                url.find("/api/PlayerReporting/v1/referee") != std::string::npos) {
                std::cout << TAG_HILE() << "DROPPED " << CLR_DIM << url << CLR_RESET << "\n";
                return;
            }

            // Redirect to Campus
            url.replace(findPos, find.length(), CAMPUS_NAMESERVER_HOST);

            using Il2cppStringNewFn = Il2cppString * (*)(const char*);
            auto strNew = reinterpret_cast<Il2cppStringNewFn>(GA + RR::Methods::Il2cpp::il2cpp_string_new);
            Il2cppString* NewStr = spoof_call(RetAddr, strNew, url.c_str());

            using ObjectGetClassFn = void* (*)(void*);
            auto getClass = reinterpret_cast<ObjectGetClassFn>(GA + RR::Methods::Il2cpp::il2cpp_object_get_class);
            void* UriClass = spoof_call(RetAddr, getClass, uri);

            using ObjectNewFn = void* (*)(void*);
            auto objNew = reinterpret_cast<ObjectNewFn>(GA + RR::Methods::Il2cpp::il2cpp_object_new);
            void* NewUri = spoof_call(RetAddr, objNew, UriClass);

            if (NewUri && NewStr) {
                using UriCtor = void (*)(void*, Il2cppString*);
                auto uriCtor = reinterpret_cast<UriCtor>(GA + RR::Methods::System::Uri::ctor);
                spoof_call(RetAddr, uriCtor, NewUri, NewStr);
                set<void*>(request, 16, NewUri);
                std::cout << TAG_NET() << "REWRITTEN: " << url << "\n";
            }
        }
    }
    return SendRequest_O(request);
}

// RecNet Response - DISABLED (breaks the game)
void (*RecNetResponse_O)(void*, void*, void*);
void RecNetResponse_H(void* request, void* response, void* promise) {
    return RecNetResponse_O(request, response, promise);
}

// HTTP Response Hooks - DISABLED (break the game)
static std::string HttpResponseUrl(void* httpResponse) { return "(disabled)"; }

thread_local bool g_inReceive = false;
bool (*Receive_O)(void*, int32_t, bool);
bool Receive_H(void* self, int32_t forceReadRawContentLength, bool readPayloadData) {
    return Receive_O(self, forceReadRawContentLength, readPayloadData);
}

thread_local bool g_inGetDataAsText = false;
Il2cppString* (*GetDataAsText_O)(void*);
Il2cppString* GetDataAsText_H(void* self) {
    return GetDataAsText_O(self);
}