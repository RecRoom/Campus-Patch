#include "UIThemes.h"
#include <chrono>
#include <fstream>
#include <string>
#include <sstream>
#include <windows.h>

namespace UIThemes {

    // 6 themes, each with 4 gradient stops
    const Color Themes[6][4] = {
        // Theme 0: Campus (original)
        {
            {0.475f, 0.106f, 0.553f, 1.0f},  // Purple
            {0.749f, 0.161f, 0.400f, 1.0f},  // Magenta
            {0.976f, 0.365f, 0.141f, 1.0f},  // Orange
            {0.980f, 0.600f, 0.071f, 1.0f}   // Gold
        },
        // Theme 1: Cyberpunk
        {
            {0.000f, 1.000f, 1.000f, 1.0f},  // Cyan
            {1.000f, 0.000f, 1.000f, 1.0f},  // Magenta
            {0.000f, 0.000f, 1.000f, 1.0f},  // Blue
            {0.500f, 0.000f, 1.000f, 1.0f}   // Violet
        },
        // Theme 2: Nature
        {
            {0.000f, 0.800f, 0.000f, 1.0f},  // Green
            {0.600f, 1.000f, 0.000f, 1.0f},  // Lime
            {0.000f, 0.800f, 0.600f, 1.0f},  // Teal
            {0.000f, 0.400f, 1.000f, 1.0f}   // Sky Blue
        },
        // Theme 3: Sunset
        {
            {1.000f, 0.000f, 0.000f, 1.0f},  // Red
            {1.000f, 0.500f, 0.000f, 1.0f},  // Orange
            {1.000f, 1.000f, 0.000f, 1.0f},  // Yellow
            {1.000f, 0.000f, 1.000f, 1.0f}   // Pink
        },
        // Theme 4: Monochrome (grayscale)
        {
            {1.0f, 1.0f, 1.0f, 1.0f},        // White
            {0.5f, 0.5f, 0.5f, 1.0f},        // Gray
            {0.2f, 0.2f, 0.2f, 1.0f},        // Dark Gray
            {0.0f, 0.0f, 0.0f, 1.0f}         // Black
        },
        // Theme 5: Dark Mode
        {
            {0.18f, 0.18f, 0.20f, 1.0f},     // Dark Gray
            {0.10f, 0.10f, 0.20f, 1.0f},     // Dark Blue
            {0.08f, 0.08f, 0.16f, 1.0f},     // Darker Blue
            {0.04f, 0.04f, 0.08f, 1.0f}      // Almost Black
        }
    };

    const char* ThemeNames[6] = {
        "Campus",
        "Cyberpunk",
        "Nature",
        "Sunset",
        "Monochrome",
        "Dark Mode"
    };

    std::atomic<int> CurrentTheme{ 0 };

    static bool g_running = false;
    static std::thread g_keyThread;

    static bool IsKeyDown(int key) {
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    }

    static bool IsCtrlHeld() {
        return IsKeyDown(VK_CONTROL) || IsKeyDown(VK_LCONTROL) || IsKeyDown(VK_RCONTROL);
    }

    static bool IsShiftHeld() {
        return IsKeyDown(VK_SHIFT) || IsKeyDown(VK_LSHIFT) || IsKeyDown(VK_RSHIFT);
    }

    // ================================================================
    // Config File Path
    // ================================================================
    static std::string GetConfigPath() {
        char dllPath[MAX_PATH];
        GetModuleFileNameA(GetModuleHandleA(nullptr), dllPath, MAX_PATH);
        std::string path(dllPath);
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            path = path.substr(0, pos + 1);
        }
        return path + "CampusPatch.cfg";
    }

    void LoadThemeFromConfig() {
        std::string configPath = GetConfigPath();
        std::ifstream file(configPath);
        int savedTheme = 0;
        if (file.is_open()) {
            std::string line;
            if (std::getline(file, line)) {
                size_t pos = line.find("Theme=");
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 6);
                    std::stringstream ss(value);
                    ss >> savedTheme;
                    if (savedTheme < 0 || savedTheme >= 6) savedTheme = 0;
                }
            }
            file.close();
        }
        CurrentTheme.store(savedTheme);
        printf("[UIThemes] Loaded theme from config: %s\n", ThemeNames[savedTheme]);
    }

    void SaveThemeToConfig(int themeIndex) {
        std::string configPath = GetConfigPath();
        std::ofstream file(configPath, std::ios::trunc);
        if (file.is_open()) {
            file << "Theme=" << themeIndex << "\n";
            file.close();
            printf("[UIThemes] Saved theme to config: %s\n", ThemeNames[themeIndex]);
        }
        else {
            printf("[UIThemes] Failed to save theme to config\n");
        }
    }

    // ================================================================
    // Force a UI redraw by resizing the game window
    // ================================================================
    static void ForceRedraw() {
        HWND hWnd = FindWindowA(nullptr, "Campus");
        if (!hWnd) hWnd = FindWindowA(nullptr, "Rec Room");
        if (!hWnd) hWnd = FindWindowA(nullptr, "Unity Player");
        if (!hWnd) return;

        RECT rect;
        if (!GetWindowRect(hWnd, &rect)) return;
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;

        // Resize by 1 pixel and back to force a redraw
        SetWindowPos(hWnd, NULL, 0, 0, w + 1, h, SWP_NOMOVE | SWP_NOZORDER);
        SetWindowPos(hWnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
    }

    // ================================================================
    // Key listener thread – now saves and forces redraw
    // ================================================================
    static void KeyListenerThread() {
        printf("[UIThemes] Key listener started – Ctrl+Shift+G to cycle themes\n");
        printf("[UIThemes] Current Theme: %s\n", ThemeNames[CurrentTheme.load()]);

        bool lastGState = false;

        while (g_running) {
            bool gDown = IsKeyDown('G');
            if (IsCtrlHeld() && IsShiftHeld() && gDown && !lastGState) {
                int oldTheme = CurrentTheme.load();
                int newTheme = (oldTheme + 1) % 6;
                CurrentTheme.store(newTheme);
                printf("[UIThemes] Switched Theme: %s → %s\n", ThemeNames[oldTheme], ThemeNames[newTheme]);
                SaveThemeToConfig(newTheme);

                // Force UI redraw – this will make the white line go through our hooks
                ForceRedraw();
                printf("[UIThemes] UI redraw forced – the white line should now match the theme.\n");
            }
            lastGState = gDown;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void StartThemeListener() {
        if (g_running) return;
        g_running = true;
        g_keyThread = std::thread(KeyListenerThread);
        g_keyThread.detach();
    }

    const Color* GetThemeColors(int index) {
        if (index < 0 || index >= 6) return Themes[0];
        return Themes[index];
    }

}