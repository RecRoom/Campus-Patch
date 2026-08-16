#pragma once

#include "AntiDebug.h"
#include "AntiDebugger.h"
#include "AntiInjection.h"
#include "AntiSandbox.h"
#include "AntiTamper.h"
#include "AntiKill.h"
#include "AntiHook.h"
#include "../Utils/logging.h"
#include "../Discord/DiscordRPC_Presence.h"

namespace Protections {
    inline void Initialize() {
        std::cout << "\n";
        std::cout << "\t[////////////////////////////////////////////////////////////////]\n";
        std::cout << "  " << CLR_RED << "\t\tKURT COBAIN'S ANTI-CHEAT SYSTEM" << CLR_RESET << "\n";
        std::cout << "\t[////////////////////////////////////////////////////////////////]\n";

        // 1. Anti-Debug
        std::cout << "| [1/6] Anti-Debug     ";
        if (AntiDebug::PerformAntiDebugCheck()) {
            std::cout << CLR_GREEN << "[PASSED]" << CLR_RESET;
        }
        else {
            std::cout << CLR_RED << "[FAILED]" << CLR_RESET;
        }
        std::cout << "                                             |\n";

        // 2. Anti-Debugger
        std::cout << "| [2/6] Anti-Debugger  ";
        if (AntiDebugger::PerformAntiDebugChecks()) {
            std::cout << CLR_GREEN << "[PASSED]" << CLR_RESET;
        }
        else {
            std::cout << CLR_RED << "[FAILED]" << CLR_RESET;
        }
        std::cout << "                                             |\n";

        // 3. Anti-Sandbox
        std::cout << "| [3/6] Anti-Sandbox   ";
        if (AntiSandbox::PerformAntiSandboxChecks()) {
            std::cout << CLR_GREEN << "[PASSED]" << CLR_RESET;
        }
        else {
            std::cout << CLR_RED << "[FAILED]" << CLR_RESET;
        }
        std::cout << "                                             |\n";

        // 4. Anti-Tamper
        std::cout << "| [4/6] Anti-Tamper    ";
        if (AntiTamper::VerifyIntegrity()) {
            std::cout << CLR_GREEN << "[PASSED]" << CLR_RESET;
        }
        else {
            std::cout << CLR_RED << "[FAILED]" << CLR_RESET;
        }
        std::cout << "                                             |\n";

        // 5. Anti-Injection - DISABLED (causes crashes/deadlocks)
        std::cout << "| [5/6] Anti-Injection ";
        // AntiInjection::Install();  // DISABLED - AntiKill already covers this
        std::cout << CLR_YELLOW << "[DISABLED]" << CLR_RESET;
        std::cout << "                                          |\n";

        // 6. Anti-Cheat
        std::cout << "| [6/6] Anti-Cheat     ";
        AntiKill::Initialize();
        std::cout << CLR_GREEN << "[ACTIVE]" << CLR_RESET;
        std::cout << "                                             |\n";

        std::cout << "\t[////////////////////////////////////////////////////////////////]\n";
        std::cout << "  \tStatus: " << CLR_GREEN << "Why do you think its called Niggas in Paris" << CLR_RESET << "\n";
        std::cout << "\t[////////////////////////////////////////////////////////////////]\n";
        std::cout << "\n";

        AntiDebug::StartAntiDebugMonitoring();
        AntiDebugger::StartMonitoring();
        AntiSandbox::StartMonitoring();
    }

    inline void Shutdown() {
        std::cout << TAG_ANTI() << "Kurt Cobain is shutting down protections...\n";

        AntiHook::Shutdown();
        DiscordRPC::Shutdown();

        CampusLogging::Shutdown();
    }
}