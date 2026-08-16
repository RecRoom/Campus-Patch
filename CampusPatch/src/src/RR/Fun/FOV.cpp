#include "FOV.h"
#include <iostream>
#include <cstdio>
#include "../../Utils/deps/minhook/include/MinHook.h"

// Forward declarations - these are defined in globals.h and main.obj
extern uintptr_t GA;
extern const IMAGE_NT_HEADERS* NtHeaders;
extern uintptr_t Referee;
extern uint64_t* RetAddr;

// Forward declare the SteamVR method namespace
namespace RR::Methods::SteamVR {
    extern uintptr_t SetFieldOfView;
}

// FOV Manager - Implementation

FOVManager& FOVManager::Get() {
    static FOVManager instance;
    return instance;
}

FOVManager::FOVManager() : m_setFOVFn(nullptr) {}

void* FOVManager::GetSteamVRInstance() {
    HMODULE hSteamVR = GetModuleHandleA("SteamVR.dll");
    if (hSteamVR) {
        void** instancePtr = (void**)((uintptr_t)hSteamVR + 0x8);
        if (instancePtr && *instancePtr) {
            return *instancePtr;
        }
    }
    return nullptr;
}

float FOVManager::GetFOV() {
    void* instance = GetSteamVRInstance();
    if (instance) {
        constexpr uintptr_t FOV_OFFSET = 0x34;
        float fov = FOV::DEFAULT_FOV;

        __try {
            fov = *(float*)((uintptr_t)instance + FOV_OFFSET);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return FOV::DEFAULT_FOV;
        }

        if (fov >= 60.0f && fov <= 120.0f) {
            printf("[FOV] GetFOV from SteamVR: %.1f\n", fov);
            return fov;
        }
    }

    return FOV::DEFAULT_FOV;
}

void FOVManager::SetFOV(float fov) {
    printf("[FOV] SetFOV called with: %.1f\n", fov);

    if (fov > FOV::MAX_ALLOWED_FOV) fov = FOV::MAX_ALLOWED_FOV;
    if (fov < FOV::MIN_ALLOWED_FOV) fov = FOV::MIN_ALLOWED_FOV;

    void* instance = GetSteamVRInstance();
    if (instance) {
        if (!m_setFOVFn) {
            uintptr_t addr = GA + RR::Methods::SteamVR::SetFieldOfView;
            m_setFOVFn = (SetFOVFn)addr;
        }

        if (m_setFOVFn) {
            printf("[FOV] Calling SteamVR SetFieldOfView with: %.1f\n", fov);
            __try {
                m_setFOVFn(instance, fov);
                printf("[FOV] SetFOV completed successfully\n");
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                printf("[FOV] Exception in SetFieldOfView\n");
            }
            return;
        }
    }

    printf("[FOV] SteamVR not available - FOV change may not work in desktop mode\n");
    printf("[FOV] Note: FOV changes only work when SteamVR is active\n");
}

void FOVManager::IncreaseFOV() {
    float current = GetFOV();
    float newFOV = current + FOV::FOV_STEP;
    if (newFOV > FOV::MAX_ALLOWED_FOV) {
        printf("[FOV] Already at max FOV: %.1f\n", FOV::MAX_ALLOWED_FOV);
        return;
    }
    SetFOV(newFOV);
}

void FOVManager::DecreaseFOV() {
    float current = GetFOV();
    float newFOV = current - FOV::FOV_STEP;
    if (newFOV < FOV::MIN_ALLOWED_FOV) {
        printf("[FOV] Already at min FOV: %.1f\n", FOV::MIN_ALLOWED_FOV);
        return;
    }
    SetFOV(newFOV);
}

bool FOVManager::DetectFOVHack() {
    float fov = GetFOV();

    if (fov > FOV::MAX_ALLOWED_FOV) {
        printf("[ANTI] [FOV] HACK DETECTED! FOV: %.1f (max: %.1f)\n", fov, FOV::MAX_ALLOWED_FOV);
        return true;
    }

    if (fov < FOV::MIN_ALLOWED_FOV) {
        printf("[ANTI] [FOV] HACK DETECTED! FOV: %.1f (min: %.1f)\n", fov, FOV::MIN_ALLOWED_FOV);
        return true;
    }

    return false;
}

void FOVManager::ResetFOV() {
    SetFOV(FOV::DEFAULT_FOV);
    printf("[FOV] Reset FOV to %.1f\n", FOV::DEFAULT_FOV);
}

// FOV Keybinds - Implementation

namespace FOVKeybinds {

    static bool g_running = false;
    static std::thread g_keyThread;

    static bool IsKeyDown(int key) {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    }

    static bool IsShiftHeld() {
        return IsKeyDown(VK_LSHIFT) || IsKeyDown(VK_RSHIFT);
    }

    static void KeyListenerThread() {
        printf("[FOV] Keybind listener started - Shift+[+] to increase, Shift+[-] to decrease\n");
        printf("[FOV] Current FOV: %.1f\n", FOVManager::Get().GetFOV());
        printf("[FOV] NOTE: FOV changes only work when SteamVR is active\n");

        bool lastPlusState = false;
        bool lastMinusState = false;
        bool lastZeroState = false;

        while (g_running) {
            bool plusDown = IsKeyDown(VK_OEM_PLUS) || IsKeyDown(VK_ADD);
            if (IsShiftHeld() && plusDown && !lastPlusState) {
                printf("[FOV] Shift++ detected - increasing FOV\n");
                FOVManager::Get().IncreaseFOV();
            }
            lastPlusState = plusDown;

            bool minusDown = IsKeyDown(VK_OEM_MINUS) || IsKeyDown(VK_SUBTRACT);
            if (IsShiftHeld() && minusDown && !lastMinusState) {
                printf("[FOV] Shift+- detected - decreasing FOV\n");
                FOVManager::Get().DecreaseFOV();
            }
            lastMinusState = minusDown;

            bool zeroDown = IsKeyDown('0');
            if (IsShiftHeld() && zeroDown && !lastZeroState) {
                printf("[FOV] Shift+0 detected - resetting FOV\n");
                FOVManager::Get().ResetFOV();
            }
            lastZeroState = zeroDown;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void StartKeyListener() {
        if (g_running) {
            printf("[FOV] Key listener already running\n");
            return;
        }

        g_running = true;
        g_keyThread = std::thread(KeyListenerThread);
        g_keyThread.detach();
    }

    void StopKeyListener() {
        g_running = false;
        if (g_keyThread.joinable()) {
            g_keyThread.join();
        }
    }
}

// FOV Patch - Implementation

namespace FOVPatch {

    void (*Original_SetFOV)(void*, float);

    void SetFOV_Hook(void* instance, float value) {
        printf("[FOV Hook] Original SetFOV called with: %.1f\n", value);

        if (value > FOV::MAX_ALLOWED_FOV) {
            printf("[ANTI] [FOV] BLOCKED FOV HACK! %.1f -> clamping to %.1f\n", value, FOV::MAX_ALLOWED_FOV);
            value = FOV::MAX_ALLOWED_FOV;
        }
        if (value < FOV::MIN_ALLOWED_FOV) {
            printf("[ANTI] [FOV] BLOCKED FOV HACK! %.1f -> clamping to %.1f\n", value, FOV::MIN_ALLOWED_FOV);
            value = FOV::MIN_ALLOWED_FOV;
        }

        Original_SetFOV(instance, value);
    }

    void InstallFOVPatch() {
        printf("[INIT] [FOV] Installing FOV patch...\n");

        if (!GA) {
            printf("[INIT] [FOV] GameAssembly.dll not loaded\n");
            return;
        }

        HMODULE hSteamVR = GetModuleHandleA("SteamVR.dll");
        if (hSteamVR) {
            printf("[INIT] [FOV] SteamVR.dll loaded at: 0x%p\n", hSteamVR);
        }
        else {
            printf("[INIT] [FOV] SteamVR.dll NOT loaded\n");
            printf("[INIT] [FOV] FOV features will only work in VR mode\n");
        }

        uintptr_t addr = GA + RR::Methods::SteamVR::SetFieldOfView;
        printf("[INIT] [FOV] SetFieldOfView address: 0x%p\n", (void*)addr);

        MH_STATUS s = MH_CreateHook(
            (void*)addr,
            (void*)SetFOV_Hook,
            (LPVOID*)&Original_SetFOV
        );

        if (s == MH_OK) {
            s = MH_EnableHook((void*)addr);
            printf("[INIT] [FOV] set_fieldOfView hook -> OK\n");
        }
        else {
            printf("[INIT] [FOV] set_fieldOfView hook -> FAILED (status: %d)\n", s);
        }

        FOVKeybinds::StartKeyListener();
        printf("[INIT] [FOV] Keybinds: Shift+[+] increase, Shift+[-] decrease, Shift+[0] reset\n");
        printf("[INIT] [FOV] NOTE: FOV changes only work when SteamVR is active\n");
    }
}