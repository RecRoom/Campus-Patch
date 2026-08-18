#pragma once

#include <iostream>
#include "../Methods.h"
#include "Console.h"
#include "../../Utils/globals.h"
#include "../../Utils/deps/spoofcall/RetSpoof.hpp"

// Gradient Recolor - Original to Campus purple/magenta/orange/gold
namespace CampusGradient {
    struct Color { float r, g, b, a; };

    static constexpr Color Purple{ 0.475f, 0.106f, 0.553f, 1.0f };
    static constexpr Color Magenta{ 0.749f, 0.161f, 0.400f, 1.0f };
    static constexpr Color Orange{ 0.976f, 0.365f, 0.141f, 1.0f };
    static constexpr Color Gold{ 0.980f, 0.600f, 0.071f, 1.0f };

    static Color rampAt(float t) {
        const Color stops[4] = { Purple, Magenta, Orange, Gold };
        if (t <= 0.0f) return stops[0];
        if (t >= 1.0f) return stops[3];
        float f = t * 3.0f;
        int i = (int)f;
        float u = f - (float)i;
        return Color{
            stops[i].r * (1.0f - u) + stops[i + 1].r * u,
            stops[i].g * (1.0f - u) + stops[i + 1].g * u,
            stops[i].b * (1.0f - u) + stops[i + 1].b * u,
            1.0f,
        };
    }

    static void rgbToHsv(float r, float g, float b, float& h, float& s, float& v) {
        float mx = r; if (g > mx) mx = g; if (b > mx) mx = b;
        float mn = r; if (g < mn) mn = g; if (b < mn) mn = b;
        v = mx;
        float d = mx - mn;
        s = (mx == 0.0f) ? 0.0f : d / mx;
        if (d == 0.0f) { h = 0.0f; return; }
        if (mx == r) h = fmodf((g - b) / d, 6.0f);
        else if (mx == g) h = (b - r) / d + 2.0f;
        else h = (r - g) / d + 4.0f;
        h /= 6.0f;
        if (h < 0.0f) h += 1.0f;
    }

    static bool isRecRoomOrange(float r, float g, float b) {
        float h, s, v;
        rgbToHsv(r, g, b, h, s, v);
        return (h >= 0.02f && h <= 0.07f && s >= 0.70f && v >= 0.60f) ||
            (h >= 0.05f && h <= 0.10f && s >= 0.45f && s <= 0.80f && v >= 0.85f);
    }
}

void (*UIGradient_set_Gradient_O)(void*, void*);

void UIGradient_set_Gradient_H(void* self, void* gradient) {
    if (gradient) {
        // Cast the function pointers with proper types
        typedef void* (*GetKeysFn)(void*);
        typedef void (*SetKeysFn)(void*, void*, void*);

        GetKeysFn getColorKeys = (GetKeysFn)(GA + RR::Methods::UnityGradient::get_colorKeys);
        GetKeysFn getAlphaKeys = (GetKeysFn)(GA + RR::Methods::UnityGradient::get_alphaKeys);
        SetKeysFn setKeys = (SetKeysFn)(GA + RR::Methods::UnityGradient::SetKeys);

        // Explicitly cast RetAddr to const void* for spoof_call
        void* colorKeys = spoof_call((const void*)RetAddr, getColorKeys, gradient);
        if (colorKeys) {
            uint64_t len = read<uint64_t>(colorKeys, 24);
            if (len > 0 && len <= 32) {
                uint8_t* keyData = (uint8_t*)colorKeys + 32;
                constexpr size_t STRIDE = 20;

                bool looksOrange = false;
                for (uint64_t i = 0; i < len; i++) {
                    float r = *(float*)(keyData + i * STRIDE + 0);
                    float g = *(float*)(keyData + i * STRIDE + 4);
                    float b = *(float*)(keyData + i * STRIDE + 8);
                    if (CampusGradient::isRecRoomOrange(r, g, b)) { looksOrange = true; break; }
                }

                if (looksOrange) {
                    void* alphaKeys = spoof_call((const void*)RetAddr, getAlphaKeys, gradient);
                    for (uint64_t i = 0; i < len; i++) {
                        float t = (len > 1) ? (float)i / (float)(len - 1) : 0.5f;
                        auto c = CampusGradient::rampAt(t);
                        *(float*)(keyData + i * STRIDE + 0) = c.r;
                        *(float*)(keyData + i * STRIDE + 4) = c.g;
                        *(float*)(keyData + i * STRIDE + 8) = c.b;
                        *(float*)(keyData + i * STRIDE + 12) = c.a;
                    }
                    spoof_call((const void*)RetAddr, setKeys, gradient, colorKeys, alphaKeys);
                }
            }
        }
    }
    UIGradient_set_Gradient_O(self, gradient);
}