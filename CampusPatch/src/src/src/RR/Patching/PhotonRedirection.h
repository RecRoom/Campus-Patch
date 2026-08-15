#pragma once

#include <iostream>
#include <mutex>
#include "../Methods.h"
#include "Console.h"
#include "ServerConfiguration.h"
#include "../../Utils/globals.h"

// Photon Redirect - Rewrite AppSettings to Campus values
static Il2cppString* g_campusAppIdRealtime = nullptr;
static Il2cppString* g_campusAppIdVoice = nullptr;
static Il2cppString* g_campusAppIdChat = nullptr;
static Il2cppString* g_campusAppVersion = nullptr;
static std::once_flag g_campusPhotonStringsOnce;

static void EnsureCampusPhotonStrings()
{
    std::call_once(g_campusPhotonStringsOnce, [] {
        if (!RR::Methods::Il2cpp::il2cpp_string_new) return;

        using Il2cppStringNewFn = Il2cppString * (*)(const char*);
        auto strNew = reinterpret_cast<Il2cppStringNewFn>(GA + RR::Methods::Il2cpp::il2cpp_string_new);

        g_campusAppIdRealtime = spoof_call(RetAddr, strNew, CAMPUS_APP_ID_REALTIME);
        g_campusAppIdVoice = spoof_call(RetAddr, strNew, CAMPUS_APP_ID_VOICE);
        g_campusAppIdChat = spoof_call(RetAddr, strNew, CAMPUS_APP_ID_CHAT);
        g_campusAppVersion = spoof_call(RetAddr, strNew, CAMPUS_APP_VERSION);
        });
}

static void RewriteAppSettings(void* appSettings)
{
    if (!appSettings) return;
    EnsureCampusPhotonStrings();
    if (!g_campusAppIdRealtime) {
        std::cout << "[Campus/Photon] AppSettings NOT rewritten - il2cpp_string_new not resolved\n";
        return;
    }

    void* curRt = read<void*>(appSettings, RR::Methods::AppSettingsFields::AppIdRealtime);
    void* curVc = read<void*>(appSettings, RR::Methods::AppSettingsFields::AppIdVoice);
    void* curChat = read<void*>(appSettings, RR::Methods::AppSettingsFields::AppIdChat);
    void* curVer = read<void*>(appSettings, RR::Methods::AppSettingsFields::AppVersion);
    if (curRt == g_campusAppIdRealtime &&
        curVc == g_campusAppIdVoice &&
        curChat == g_campusAppIdChat &&
        curVer == g_campusAppVersion) {
        return;
    }

    if (g_campusAppIdRealtime) set<void*>(appSettings, RR::Methods::AppSettingsFields::AppIdRealtime, g_campusAppIdRealtime);
    if (g_campusAppIdVoice) set<void*>(appSettings, RR::Methods::AppSettingsFields::AppIdVoice, g_campusAppIdVoice);
    if (g_campusAppIdChat) set<void*>(appSettings, RR::Methods::AppSettingsFields::AppIdChat, g_campusAppIdChat);
    if (g_campusAppVersion) set<void*>(appSettings, RR::Methods::AppSettingsFields::AppVersion, g_campusAppVersion);

    std::cout << TAG_PHOTON() << "AppSettings redirected -> "
        << CLR_YELLOW << CAMPUS_APP_ID_REALTIME << CLR_RESET
        << CLR_DIM " ver " CLR_RESET << CAMPUS_APP_VERSION << "\n";
}

// Photon Hook Functions
bool (*RealtimeConnect_O)(void*, void*);
bool RealtimeConnect_H(void* self, void* appSettings) {
    RewriteAppSettings(appSettings);
    return RealtimeConnect_O(self, appSettings);
}

bool (*PunConnect_O)(void*, void*, bool);
bool PunConnect_H(void* self, void* appSettings, bool arg) {
    RewriteAppSettings(appSettings);
    return PunConnect_O(self, appSettings, arg);
}

bool (*VoiceConnect_O)(void*, void*);
bool VoiceConnect_H(void* self, void* appSettings) {
    RewriteAppSettings(appSettings);
    return VoiceConnect_O(self, appSettings);
}

bool (*VoicePUNConnect_O)(void*, void*);
bool VoicePUNConnect_H(void* self, void* appSettings) {
    RewriteAppSettings(appSettings);
    return VoicePUNConnect_O(self, appSettings);
}