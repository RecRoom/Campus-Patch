#pragma once
#include <string>

namespace DiscordRPC {
    void Start();
    void Shutdown();
    void UpdatePresence();
    void SetCurrentRoom(const std::string& roomName);
    void ParseRoomFromResponse(const std::string& responseBody);
    void SetRoomFromUrl(const std::string& url);
}