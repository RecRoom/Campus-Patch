#include "DiscordRPC_Presence.h"
#include "../RR/Patching/Console.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <ctime>
#include <chrono>
#include <cstring>
#include "../Discord/discord-rpc-win/discord-rpc/win64-dynamic/include/discord_rpc.h"

namespace DiscordRPC {

    static std::thread g_thread;
    static std::atomic<bool> g_running{ false };
    static std::atomic<bool> g_initialized{ false };
    static std::atomic<bool> g_discordReady{ false };

    // Discord Application ID
    static const char* CLIENT_ID = "1535967316729266206";

    void UpdatePresence() {
        if (!g_initialized || !g_discordReady) {
            std::cout << TAG_DISCORD() << "Not ready - can't update presence\n";
            return;
        }

        DiscordRichPresence discordPresence{};
        memset(&discordPresence, 0, sizeof(discordPresence));
        discordPresence.state = "On Campus";
        discordPresence.details = "Playing Campus";
        discordPresence.startTimestamp = time(nullptr);
        discordPresence.largeImageKey = "campus";
        discordPresence.largeImageText = "Campus";
        discordPresence.smallImageText = "Campus";
        discordPresence.partyId = "ae488379-351d-4a4f-ad32-2b9b01c91657";
        discordPresence.joinSecret = "MTI4NzM0OjFpMmhuZToxMjMxMjM= ";

        Discord_UpdatePresence(&discordPresence);
        std::cout << TAG_DISCORD() << "Presence updated: On Campus - Playing Campus\n";
    }

    void CallbackThread() {
        std::cout << TAG_DISCORD() << "Callback thread started\n";

        int callbacksRun = 0;
        while (g_running) {
            if (g_initialized) {
                Discord_RunCallbacks();
                callbacksRun++;

                if (callbacksRun % 50 == 0) {
                    std::cout << TAG_DISCORD() << "Callbacks running (" << callbacksRun << ")\n";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << TAG_DISCORD() << "Callback thread stopped\n";
    }

    void Start() {
        if (g_initialized) {
            std::cout << TAG_DISCORD() << "Already initialized\n";
            return;
        }

        std::cout << TAG_DISCORD() << "Starting...\n";

        DiscordEventHandlers handlers{};
        handlers.ready = [](const DiscordUser* user) {
            std::cout << TAG_DISCORD() << "Ready! User: " << user->username << "\n";
            g_discordReady = true;
            UpdatePresence();
            };
        handlers.disconnected = [](int errorCode, const char* message) {
            std::cout << TAG_DISCORD() << "Disconnected (code: " << errorCode << ")\n";
            g_discordReady = false;
            };
        handlers.errored = [](int errorCode, const char* message) {
            std::cout << TAG_DISCORD() << "Error (code: " << errorCode << ") - " << (message ? message : "unknown") << "\n";
            g_discordReady = false;
            };

        Discord_Initialize(CLIENT_ID, &handlers, 1, nullptr);
        g_initialized = true;
        std::cout << TAG_DISCORD() << "Initialized, waiting for ready event...\n";

        g_running = true;
        g_thread = std::thread(CallbackThread);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        Discord_RunCallbacks();

        std::cout << TAG_DISCORD() << "Started\n";
    }

    void Shutdown() {
        std::cout << TAG_DISCORD() << "Shutting down...\n";

        g_running = false;
        if (g_thread.joinable()) {
            g_thread.join();
        }

        Discord_Shutdown();
        g_initialized = false;
        g_discordReady = false;
        std::cout << TAG_DISCORD() << "Shutdown complete\n";
    }

} // namespace DiscordRPC