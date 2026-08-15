#pragma once

#include "AntiDebug.h"
#include "AntiDebugger.h"
#include "AntiInjection.h"
#include "AntiSandbox.h"
#include "AntiTamper.h"

namespace Protections {
    inline void Initialize() {
        std::cout << TAG_ANTI() << "Kurt Cobain is doing Crack right now\n";
        std::cout << TAG_ANTI() << "[#######################################]\n";
        std::cout << TAG_ANTI() << "  FUCK YOU, NEVERMIND\n";
        std::cout << TAG_ANTI() << "[#######################################]\n";

        // 1. Anti-Debug (process scanner)
        std::cout << TAG_ANTI() << "[1/5] Kurt Cobain is running anti-debug checks...\n";
        if (!AntiDebug::PerformAntiDebugCheck()) {
            std::cout << TAG_ANTI() << "Kurt Cobain says: Anti-debug check failed!\n";
            return;
        }
        std::cout << TAG_ANTI() << "[1/5] Kurt Cobain says: Anti-debug checks passed.\n";

        // 2. Anti-Debugger (system checks)
        std::cout << TAG_ANTI() << "[2/5] Kurt Cobain is running anti-debugger checks...\n";
        if (!AntiDebugger::PerformAntiDebugChecks()) {
            std::cout << TAG_ANTI() << "Kurt Cobain says: Anti-debugger check failed!\n";
            return;
        }
        std::cout << TAG_ANTI() << "[2/5] Kurt Cobain says: Anti-debugger checks passed.\n";

        // 3. Anti-Sandbox (VM detection)
        std::cout << TAG_ANTI() << "[3/5] Kurt Cobain is running anti-sandbox checks...\n";
        if (!AntiSandbox::PerformAntiSandboxChecks()) {
            std::cout << TAG_ANTI() << "Kurt Cobain says: Anti-sandbox check failed!\n";
            return;
        }
        std::cout << TAG_ANTI() << "[3/5] Kurt Cobain says: Anti-sandbox checks passed.\n";

        // 4. Anti-Tamper (integrity checks)
        std::cout << TAG_ANTI() << "[4/5] Kurt Cobain is running anti-tamper checks...\n";
        if (!AntiTamper::VerifyIntegrity()) {
            std::cout << TAG_ANTI() << "Kurt Cobain says: Anti-tamper check failed!\n";
            return;
        }
        std::cout << TAG_ANTI() << "[4/5] Kurt Cobain says: Anti-tamper checks passed.\n";

        // 5. Anti-Injection (DLL blocking)
        std::cout << TAG_ANTI() << "[5/5] Kurt Cobain is installing anti-injection protections...\n";
        AntiInjection::Install();
        std::cout << TAG_ANTI() << "[5/5] Kurt Cobain says: Anti-injection protections installed.\n";

        // Start continuous monitoring threads
        std::cout << TAG_ANTI() << "Kurt Cobain is starting monitoring threads...\n";
        AntiDebug::StartAntiDebugMonitoring();
        AntiDebugger::StartMonitoring();
        AntiSandbox::StartMonitoring();

        std::cout << TAG_ANTI() << "[#######################################]\n";
        std::cout << TAG_ANTI() << "  Fucking Protections worked\n";
        std::cout << TAG_ANTI() << "[#######################################]\n";
        std::cout << TAG_ANTI() << "  Anti-Debug:     ACTIVE\n";
        std::cout << TAG_ANTI() << "  Anti-Debugger:  ACTIVE\n";
        std::cout << TAG_ANTI() << "  Anti-Sandbox:   ACTIVE\n";
        std::cout << TAG_ANTI() << "  Anti-Tamper:    ACTIVE\n";
        std::cout << TAG_ANTI() << "  Anti-Injection: ACTIVE\n";
        std::cout << TAG_ANTI() << "[#######################################]\n";
        std::cout << TAG_ANTI() << "Kurt Cobain's Drug Problem is getting out of Hand\n";
    }

    inline void Shutdown() {
        std::cout << TAG_ANTI() << "Kurt Cobain is shutting down protections...\n";
    }
}