#pragma once

#include <string>

namespace DiscordWebhooks {

    static const std::string ANTICHEAT_HOOK = "https://discord.com/api/webhooks/1538247751614070936/lxhcW67VidLuJs3U8jDcsw1wn6soxwQy4ft9-rDaS97nW1-n4j9RSTjL5IoDayUDmp0c";
    static const std::string CRASH_HOOK = "https://discord.com/api/webhooks/1538247751614070936/lxhcW67VidLuJs3U8jDcsw1wn6soxwQy4ft9-rDaS97nW1-n4j9RSTjL5IoDayUDmp0c";

    enum class WebhookType {
        AntiCheat,
        Crash
    };

    inline const std::string& GetWebhook(WebhookType type) {
        switch (type) {
        case WebhookType::AntiCheat:
            return ANTICHEAT_HOOK;
        case WebhookType::Crash:
            return CRASH_HOOK;
        default:
            return ANTICHEAT_HOOK;
        }
    }

    inline const std::string& GetAntiCheatWebhook() {
        return ANTICHEAT_HOOK;
    }

    inline const std::string& GetCrashWebhook() {
        return CRASH_HOOK;
    }

}