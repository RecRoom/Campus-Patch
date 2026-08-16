#pragma once

#include <windows.h>
#include <thread>
#include <chrono>
#include <iostream>
#include "../Patching/Console.h"

// ============================================================
// FOV Constants
// ============================================================
namespace FOV {
    constexpr float DEFAULT_FOV = 90.0f;
    constexpr float MAX_ALLOWED_FOV = 120.0f;
    constexpr float MIN_ALLOWED_FOV = 60.0f;
    constexpr float FOV_STEP = 1.0f;
}

// ============================================================
// FOV Manager Class - Declarations only
// ============================================================
class FOVManager {
public:
    typedef void (*SetFOVFn)(void*, float);

    static FOVManager& Get();

    void SetFOV(float fov);
    float GetFOV();
    void IncreaseFOV();
    void DecreaseFOV();
    bool DetectFOVHack();
    void ResetFOV();

private:
    FOVManager();
    ~FOVManager() = default;
    FOVManager(const FOVManager&) = delete;
    FOVManager& operator=(const FOVManager&) = delete;

    void* GetSteamVRInstance();

    SetFOVFn m_setFOVFn;
};

// ============================================================
// FOV Keybinds - Declarations only
// ============================================================
namespace FOVKeybinds {
    void StartKeyListener();
    void StopKeyListener();
}

// ============================================================
// FOV Patch - Declarations only
// ============================================================
namespace FOVPatch {
    void InstallFOVPatch();
}