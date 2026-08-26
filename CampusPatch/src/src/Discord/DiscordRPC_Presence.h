#pragma once
#include <string>

namespace DiscordRPC {
    // True between Start() and Shutdown(). SendRequest.h consults this before
    // firing a detached WinHTTP thread so we don't spawn work into an object
    // graph that's being torn down (Shutdown -> Discord_Shutdown -> static
    // destructors would otherwise race the detached thread's std::string /
    // std::mutex use and crash the process on exit).
    bool IsRunning();
    void Start();
    void Shutdown();
    void UpdatePresence();
    void SetCurrentRoom(const std::string& roomName);
    void ParseRoomFromResponse(const std::string& responseBody);
    void SetRoomFromUrl(const std::string& url);
}