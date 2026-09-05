#pragma once
#include <atomic>
#include <thread>
#include <windows.h>
#include <iostream>
#include "Console.h"

namespace UIThemes {

    struct Color {
        float r, g, b, a;
    };

    // 6 themes, each with 4 gradient stops
    extern const Color Themes[6][4];
    extern const char* ThemeNames[6];
    extern std::atomic<int> CurrentTheme;

    void StartThemeListener();
    const Color* GetThemeColors(int index);

    // Config persistence
    void LoadThemeFromConfig();
    void SaveThemeToConfig(int themeIndex);

}