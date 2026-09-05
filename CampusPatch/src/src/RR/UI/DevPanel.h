#pragma once

#include "../imgui/imgui.h"
#include "../Patching/CustomNotify.h"
#include "../Patching/CustomChat.h"
#include "../Patching/CustomSpawn.h"
#include "../../Utils/globals.h"
#include <cstring>
#include <string>

// ================================================================================
// Dev panel  (DEBUG DLL ONLY - gated behind campus_debug.txt via DebugTools)
//
// A small ImGui window of developer conveniences:
//   - Notification: type a message, show it as the game's own toast on this client.
//   - Chat: type a message, send it into room chat - everyone in YOUR room sees it,
//     because chat is room-scoped by nature.
//
// Rendering is folded into the inspector's existing ImGui frame (imgui_inspector.cpp),
// so there is no second Present hook.
// ================================================================================

namespace DevPanel {

    // Hidden until INSERT is pressed (main.cpp key listener). The console
    // minimize key is the only key left (see main.cpp).
    inline bool g_visible = false;

    inline void ToggleVisibility() { g_visible = !g_visible; }
    inline bool IsVisible() { return g_visible; }

    // Persist the controls across frames.
    inline char  g_msg[256]   = "Welcome to Campus!";
    inline int   g_priority   = 2;      // 0 Minor 1 Medium 2 Major 3 Vital
    inline float g_duration   = 5.0f;
    inline char  g_chat[256]  = "";
    inline char  g_prefab[128] = "Whip";
    inline float g_scale      = 1.0f;
    inline std::string g_lastResult;  // status line

    inline void ReadyDot(bool ready, const char* readyText, const char* notText) {
        if (ready) ImGui::TextColored(ImVec4(.5f, 1, .5f, 1), "%s", readyText);
        else       ImGui::TextColored(ImVec4(1, .7f, .3f, 1), "%s", notText);
    }

    inline void TabNotifyChat() {
        // ---- notification (this client) ----
        ImGui::SeparatorText("Notification (you)");
        ImGui::InputText("message", g_msg, sizeof(g_msg));
        const char* prio[] = { "Minor", "Medium", "Major", "Vital" };
        ImGui::Combo("priority", &g_priority, prio, IM_ARRAYSIZE(prio));
        ImGui::SliderFloat("duration (s)", &g_duration, 1.0f, 15.0f, "%.1f");
        if (ImGui::Button("Show notification")) {
            bool ok = CustomNotify::Show(g_msg, (CustomNotify::Priority)g_priority, g_duration);
            g_lastResult = ok ? "notification shown" : "not shown (manager not ready?)";
        }
        ImGui::SameLine();
        ReadyDot(CustomNotify::g_manager != nullptr, "ready", "not captured yet");

        // ---- chat (everyone in your room) ----
        ImGui::SeparatorText("Chat (everyone in your room)");
        bool enter = ImGui::InputText("chat", g_chat, sizeof(g_chat),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool click = ImGui::Button("Send");
        if ((enter || click) && g_chat[0]) {
            bool ok = CustomChat::Send(g_chat);
            g_lastResult = ok ? "chat sent" : "chat not sent (open chat in-game once?)";
            if (ok) g_chat[0] = '\0';
        }
        ReadyDot(CustomChat::g_chatMenu != nullptr, "chat ready", "open chat in-game once");
    }

    inline void TabAssetSpawn() {
        ImGui::SeparatorText("Spawn a Tool by prefab name");
        ImGui::TextWrapped("Prefab names, e.g. Whip, PaintballShotgun, "
                           "Quest_Dracula1_Blunderbuss.");
        bool enter = ImGui::InputText("prefab", g_prefab, sizeof(g_prefab),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SliderFloat("scale", &g_scale, 0.1f, 5.0f, "%.2f");
        ImGui::SameLine();
        bool click = ImGui::Button("Spawn");
        if ((enter || click) && g_prefab[0]) {
            bool ok = CustomSpawn::Spawn(g_prefab, g_scale);
            g_lastResult = ok ? "spawned" : "spawn faulted (bad prefab name?)";
        }
        ImGui::TextDisabled("Spawns near the world origin for now - positioning on/in "
                            "front of you is a follow-up.");
    }

    inline void TabPlayers() {
        ImGui::SeparatorText("Players in room");
        ImGui::TextWrapped("Not wired yet - the room player list needs the game's player "
                           "manager, which isn't cleanly located in the dump. Coming next.");
    }

    inline void Render() {
        if (!g_visible) return;

        ImGui::SetNextWindowSize(ImVec2(460, 340), ImGuiCond_FirstUseEver);
        ImGui::Begin("Campus Dev Panel", &g_visible);

        if (ImGui::BeginTabBar("devtabs")) {
            if (ImGui::BeginTabItem("Notify & Chat")) { TabNotifyChat(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Asset Spawn"))   { TabAssetSpawn(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Players"))       { TabPlayers();    ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }

        if (!g_lastResult.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("%s", g_lastResult.c_str());
        }

        ImGui::End();
    }
}
