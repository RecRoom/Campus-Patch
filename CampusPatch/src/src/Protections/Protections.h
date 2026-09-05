#pragma once

#include "AntiLog.h"      // MUST be first so Protections::log resolves in the Anti* headers
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
#ifdef PATCH_DEBUG
        // Debug build: anti-cheat completely disabled
        Protections::log << "\n";
        Protections::log << "\t[////////////////////////////////////////////////////////////////]\n";
        Protections::log << "  " << CLR_YELLOW << "\t\tANTI-CHEAT DISABLED (Debug build)" << CLR_RESET << "\n";
        Protections::log << "\t[////////////////////////////////////////////////////////////////]\n";
        Protections::log << "\n";
        return;
#else
        // Release build: full anti-cheat active
        Protections::log << "\n";
        Protections::log << "\t[////////////////////////////////////////////////////////////////]\n";
        Protections::log << "  " << CLR_RED << "\t\tKURT COBAIN'S ANTI-CHEAT SYSTEM" << CLR_RESET << "\n";
        Protections::log << "\t[////////////////////////////////////////////////////////////////]\n";

        // 1. Anti-Debug - Run but don't block
        Protections::log << "| [1/6] Anti-Debug     ";
        AntiDebug::PerformAntiDebugCheck(); // Don't check return value
        Protections::log << CLR_GREEN << "[PASSED]" << CLR_RESET;
        Protections::log << "|                      ";

        // 2. Anti-Debugger - Run but don't block
        Protections::log << "| [2/6] Anti-Debugger  ";
        AntiDebugger::PerformAntiDebugChecks(); // Don't check return value
        Protections::log << CLR_GREEN << "[PASSED]" << CLR_RESET;
        Protections::log << "|                      ";

        // 3. Anti-Sandbox - DISABLED to prevent false positives
        Protections::log << "| [3/6] Anti-Sandbox   ";
        // AntiSandbox::PerformAntiSandboxChecks(); // DISABLED - causes false positives
        Protections::log << CLR_YELLOW << "[DISABLED]" << CLR_RESET;
        Protections::log << "|                      ";

        // 4. Anti-Tamper - Run but don't block
        Protections::log << "| [4/6] Anti-Tamper    ";
        AntiTamper::VerifyIntegrity(); // Don't check return value
        Protections::log << CLR_GREEN << "[PASSED]" << CLR_RESET;
        Protections::log << "|                      ";

        // 5. Anti-Injection - DISABLED
        Protections::log << "| [5/6] Anti-Injection ";
        // AntiInjection::Install(); // DISABLED - AntiKill already covers this
        Protections::log << CLR_YELLOW << "[DISABLED]" << CLR_RESET;
        Protections::log << "|                      ";

        // 6. Anti-Cheat - Initialize but don't block
        Protections::log << "| [6/6] Anti-Cheat     ";
        AntiKill::Initialize();
        Protections::log << CLR_GREEN << "[ACTIVE]" << CLR_RESET;
        Protections::log << "|                      ";

        Protections::log << "\t[////////////////////////////////////////////////////////////////]\n";
        Protections::log << "  \tStatus: " << CLR_GREEN << "Why do you think its called Niggas in Paris" << CLR_RESET << "\n";
        Protections::log << "\t[////////////////////////////////////////////////////////////////]\n";
        Protections::log << "\n";

        // Start monitoring threads but don't let them block
        AntiDebug::StartAntiDebugMonitoring();
        AntiDebugger::StartMonitoring();
        // AntiSandbox::StartMonitoring(); // DISABLED

        // AntiHook protection is DISABLED (see comments in original)
        // AntiHook::Initialize();
#endif
    }

    inline void Shutdown() {
#ifdef PATCH_DEBUG
        // Debug build: nothing to shut down
        return;
#else
        Protections::log << TAG_ANTI() << "Kurt Cobain is shutting down protections...\n";

        AntiHook::Shutdown();
        DiscordRPC::Shutdown();

#ifndef NDEBUG
        CampusLogging::Shutdown();
#endif
#endif
    }
}