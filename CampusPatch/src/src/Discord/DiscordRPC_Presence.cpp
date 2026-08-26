#include "DiscordRPC_Presence.h"
#include "../RR/Patching/Console.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <ctime>
#include <chrono>
#include <cstring>
#include <string>
#include <regex>
#include <sstream>
#include <mutex>
#include <windows.h>
#include <winhttp.h>
#include "json.hpp"
#include "../Discord/discord-rpc-win/discord-rpc/win64-dynamic/include/discord_rpc.h"

using json = nlohmann::json;  // Add this line

#pragma comment(lib, "winhttp.lib")

namespace DiscordRPC {

    static std::thread g_thread;
    static std::atomic<bool> g_running{ false };
    static std::atomic<bool> g_initialized{ false };
    static std::atomic<bool> g_discordReady{ false };
    static std::string g_currentRoom = "Unknown Room";
    static std::mutex g_roomMutex;

    // Store presence strings to keep them alive
    static std::string g_cachedDetails;
    static std::string g_cachedState;
    static std::mutex g_presenceMutex;

    // Discord Application ID
    static const char* CLIENT_ID = "1535967316729266206";

    // Extract room ID from URL
    static std::string ExtractRoomIdFromUrl(const std::string& url) {
        std::smatch match;
        std::regex roomIdRegex("/Room_server/rooms/([0-9]+)");
        if (std::regex_search(url, match, roomIdRegex) && match.size() > 1) {
            return match[1].str();
        }
        return "";
    }

    // Fetch room name from server
    static std::string FetchRoomName(const std::string& roomId) {
        if (roomId.empty()) return "";

        std::wstring host = L"gameauth.ddns.net";
        std::wstring path = L"/Room_server/rooms/" + std::wstring(roomId.begin(), roomId.end()) + L"/";

        HINTERNET hSession = WinHttpOpen(L"DiscordRPC/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (!hSession) return "";

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "";
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        // Disable certificate verification for simplicity
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));

        if (!WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        std::string response;
        DWORD bytesRead = 0;
        char buffer[4096];
        while (WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Parse JSON
        try {
            auto jsonData = json::parse(response);
            if (jsonData.contains("Name") && jsonData["Name"].is_string()) {
                return jsonData["Name"].get<std::string>();
            }
        }
        catch (const std::exception& e) {
            std::cout << TAG_DISCORD() << "JSON parse error: " << e.what() << "\n";
        }

        return "";
    }

    bool IsRunning() { return g_running.load(); }

    void SetRoomFromUrl(const std::string& url) {
        // SendRequest.h invokes this on a DETACHED thread. If Shutdown() has
        // already run (process exit, DLL unload), g_currentRoom / g_roomMutex /
        // g_cachedDetails may be mid-destruction, and touching them races the
        // CRT and crashes the process. Bail early -- and wrap the whole thing
        // in a catch so any WinHTTP exception during teardown doesn't propagate
        // out of a detached thread (which is std::terminate).
        if (!g_running.load()) return;
        try {
            std::string roomId = ExtractRoomIdFromUrl(url);
            if (roomId.empty()) return;
            std::string roomName = FetchRoomName(roomId);
            if (!g_running.load()) return; // may have shut down during the HTTPS wait
            if (!roomName.empty()) {
                SetCurrentRoom("^" + roomName);
            }
            else {
                SetCurrentRoom("Room " + roomId);  // Fallback
            }
        }
        catch (...) {
            // Swallow -- we're on a detached thread; letting an exception
            // escape becomes std::terminate.
        }
    }

    void SetCurrentRoom(const std::string& roomName) {
        if (roomName.find("visited") != std::string::npos ||
            roomName.find("by") != std::string::npos ||
            roomName.find("unknown") != std::string::npos ||
            roomName.find("null") != std::string::npos ||
            roomName.find("undefined") != std::string::npos ||
            roomName.find("skip") != std::string::npos ||
            roomName.find("take") != std::string::npos ||
            roomName.find("includeCCUs") != std::string::npos ||
            roomName.empty() ||
            roomName.length() > 50 ||
            roomName.find("?") != std::string::npos) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(g_roomMutex);
            g_currentRoom = roomName;
        }
        // Was: called UpdatePresence() while g_roomMutex was held. UpdatePresence
        // ALSO locks g_roomMutex -> std::mutex is non-recursive -> the second lock
        // hangs the calling thread forever. That thread was usually a detached
        // WinHTTP fetch thread from SendRequest.h, so a leaked thread + a leaked
        // WinHTTP session per room change. Release the lock first, then update.
        if (g_initialized && g_discordReady) {
            UpdatePresence();
        }
    }

    void UpdatePresence() {
        if (!g_initialized || !g_discordReady) {
            return;
        }

        std::lock_guard<std::mutex> roomLock(g_roomMutex);
        std::lock_guard<std::mutex> presenceLock(g_presenceMutex);

        // Cache strings so they stay alive
        g_cachedDetails = "In " + g_currentRoom;
        g_cachedState = "Playing Campus";

        DiscordRichPresence discordPresence{};
        memset(&discordPresence, 0, sizeof(discordPresence));
        discordPresence.details = g_cachedDetails.c_str();
        discordPresence.state = g_cachedState.c_str();
        discordPresence.startTimestamp = time(nullptr);
        discordPresence.largeImageKey = "campus";
        discordPresence.largeImageText = "Campus";
        discordPresence.smallImageText = "Campus";
        discordPresence.partyId = "ae488379-351d-4a4f-ad32-2b9b01c91657";
        discordPresence.joinSecret = "MTI4NzM0OjFpMmhuZToxMjMxMjM= ";

        Discord_UpdatePresence(&discordPresence);
    }

    void ParseRoomFromResponse(const std::string& responseBody) {
        // Try to parse room data directly from response
        try {
            auto jsonData = json::parse(responseBody);
            if (jsonData.contains("RoomId") && jsonData["RoomId"].is_number()) {
                std::string roomId = std::to_string(jsonData["RoomId"].get<int>());
                std::string roomName = jsonData.contains("Name") ? jsonData["Name"].get<std::string>() : "";
                if (!roomName.empty()) {
                    SetCurrentRoom("^" + roomName);
                }
                else {
                    SetCurrentRoom("Room " + roomId);
                }
                return;
            }
        }
        catch (...) {
            // Not JSON or missing fields, continue with regex parsing
        }

        // Fallback to regex parsing
        std::smatch match;

        std::regex roomNamePattern("\"roomName\"\\s*:\\s*\"([^\"]+)\"");
        if (std::regex_search(responseBody, match, roomNamePattern) && match.size() > 1) {
            std::string room = match[1].str();
            if (!room.empty()) {
                SetCurrentRoom("^" + room);
                return;
            }
        }

        std::regex roomIdPattern("\"roomId\"\\s*:\\s*([0-9]+)");
        if (std::regex_search(responseBody, match, roomIdPattern) && match.size() > 1) {
            std::string roomId = match[1].str();
            if (!roomId.empty()) {
                SetCurrentRoom("Room " + roomId);
                return;
            }
        }

        std::regex namePattern("\"name\"\\s*:\\s*\"([^\"]+)\"");
        if (std::regex_search(responseBody, match, namePattern) && match.size() > 1) {
            std::string name = match[1].str();
            if (!name.empty() && name.find("visited") == std::string::npos) {
                SetCurrentRoom("^" + name);
                return;
            }
        }
    }

    void CallbackThread() {
        while (g_running) {
            if (g_initialized) {
                Discord_RunCallbacks();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void Start() {
        if (g_initialized) {
            return;
        }

        // Use static handlers that persist
        static DiscordEventHandlers handlers{};
        static bool handlersSet = false;

        if (!handlersSet) {
            handlers.ready = [](const DiscordUser* user) {
                g_discordReady = true;
                UpdatePresence();
                };
            handlers.disconnected = [](int errorCode, const char* message) {
                g_discordReady = false;
                };
            handlers.errored = [](int errorCode, const char* message) {
                g_discordReady = false;
                };
            handlersSet = true;
        }

        Discord_Initialize(CLIENT_ID, &handlers, 1, nullptr);
        g_initialized = true;

        g_running = true;
        g_thread = std::thread(CallbackThread);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        Discord_RunCallbacks();
    }

    void Shutdown() {
        g_running = false;
        if (g_thread.joinable()) {
            g_thread.join();
        }

        Discord_Shutdown();
        g_initialized = false;
        g_discordReady = false;
    }

} // namespace DiscordRPC