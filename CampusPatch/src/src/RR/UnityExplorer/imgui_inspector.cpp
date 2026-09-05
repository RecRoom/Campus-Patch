#include "imgui_inspector.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"
#include "../../Utils/globals.h"
#include "../../Utils/scan.h"
#include "../Methods.h"
#include "../Patching/Il2CppApi.h"
#include "../UI/CampusMenu.h"
#include "../UI/DevPanel.h"
#include <d3d11.h>
#include <cstdio>
#include <string>
#include <vector>

// ================================================================================
// ImGui Unity inspector
//
// REWRITTEN 2026-09-01. The previous version crashed the game a few seconds after
// launch, and there were four separate reasons:
//
//  1. It called RR::Methods::UnityEngine::GameObject_Find / Transform_GetChild /
//     GameObject_get_transform. PatchInstaller resolved all ten of those slots with
//     ONE shared byte pattern ("48 89 5C 24 08 57 48 83 EC 20 48 8B 05"), and
//     PatternScan returns the first match - so every slot held the same address,
//     pointing at some unrelated function. Calling it with GameObject.Find's
//     signature was the crash. Render() runs every frame from the Present hook,
//     hence "starts, then dies a few seconds later".
//
//  2. Object layout was read with hardcoded offsets (transform+0x10, GO+0x60,
//     GO+0x30, transform+0x38). IL2CPP field offsets move between builds, so those
//     are wrong on any version but the one they were dumped from.
//
//  3. il2cpp_class_get_name was looked up from UnityPlayer.dll. It is exported by
//     GameAssembly.dll - the lookup returned null and component types never showed.
//
//  4. It entered managed code from the D3D Present thread without attaching that
//     thread to the IL2CPP runtime.
//
// Everything now goes through Il2CppApi: methods are resolved from metadata BY
// NAME, so there are no offsets to maintain and this works across game updates.
// ================================================================================

namespace ImGuiInspector {

    static bool g_visible = false;   // inspector off by default; dev panel is the main UI now
    static bool g_initialized = false;

    // ---- resolved Unity methods (all by name, once) ----------------------------

    struct UnityFns {
        void* (*GameObject_Find)(void*)            = nullptr;  // static, (string) -> GameObject
        void* (*GameObject_get_transform)(void*)   = nullptr;
        void* (*Transform_get_childCount)(void*)   = nullptr;  // returns int in RAX
        void* (*Transform_GetChild)(void*, int)    = nullptr;
        void* (*Transform_get_gameObject)(void*)   = nullptr;
        void* (*Object_get_name)(void*)            = nullptr;  // UnityEngine.Object.get_name
        bool  resolved                             = false;
        bool  ok                                   = false;
    };
    static UnityFns g_fn;

    static void ResolveUnity() {
        if (g_fn.resolved) return;
        g_fn.resolved = true;

        if (!Il2CppApi::Init()) return;

        using F1 = void* (*)(void*);
        using F2 = void* (*)(void*, int);

        g_fn.GameObject_Find          = (F1)Il2CppApi::FindUnityMethod("UnityEngine", "GameObject", "Find", 1);
        g_fn.GameObject_get_transform = (F1)Il2CppApi::FindUnityMethod("UnityEngine", "GameObject", "get_transform", 0);
        g_fn.Transform_get_childCount = (F1)Il2CppApi::FindUnityMethod("UnityEngine", "Transform", "get_childCount", 0);
        g_fn.Transform_GetChild       = (F2)Il2CppApi::FindUnityMethod("UnityEngine", "Transform", "GetChild", 1);
        g_fn.Transform_get_gameObject = (F1)Il2CppApi::FindUnityMethod("UnityEngine", "Component", "get_gameObject", 0);
        g_fn.Object_get_name          = (F1)Il2CppApi::FindUnityMethod("UnityEngine", "Object", "get_name", 0);

        g_fn.ok = g_fn.GameObject_Find && g_fn.GameObject_get_transform
               && g_fn.Transform_get_childCount && g_fn.Transform_GetChild;
    }

    // ---- helpers ---------------------------------------------------------------

    // Standard il2cpp x64 string layout: [klass][monitor][int32 length][char16 data].
    // NOT the legacy (length @ +0, data @ +4) layout globals.h's ReadIl2CppString
    // assumes - that one reads garbage on this runtime.
    static std::string ReadManagedString(void* s) {
        if (!s) return {};
        int32_t len = *reinterpret_cast<int32_t*>((uintptr_t)s + 0x10);
        if (len <= 0 || len > 4096) return {};

        auto data = reinterpret_cast<const wchar_t*>((uintptr_t)s + 0x14);
        int need = WideCharToMultiByte(CP_UTF8, 0, data, len, nullptr, 0, nullptr, nullptr);
        if (need <= 0) return {};

        std::string out(need, 0);
        WideCharToMultiByte(CP_UTF8, 0, data, len, &out[0], need, nullptr, nullptr);
        return out;
    }

    static std::string ObjectName(void* unityObj) {
        if (!unityObj || !g_fn.Object_get_name) return "(unnamed)";
        void* s = g_fn.Object_get_name(unityObj);
        std::string n = ReadManagedString(s);
        return n.empty() ? "(unnamed)" : n;
    }

    static const char* ClassNameOf(void* obj) {
        if (!obj || !Il2CppApi::object_get_class || !Il2CppApi::class_get_name) return nullptr;
        void* k = Il2CppApi::object_get_class(obj);
        return k ? Il2CppApi::class_get_name(k) : nullptr;
    }

    // ---- field inspection ------------------------------------------------------
    //
    // Walks the object's class (and its bases) through il2cpp_class_get_fields and
    // shows name, declared type and the raw bytes at the field's offset. Offsets
    // come from the runtime, so nothing here is version-pinned.
    static void DrawFields(void* obj) {
        if (!obj) return;
        if (!Il2CppApi::class_get_fields || !Il2CppApi::field_get_name
            || !Il2CppApi::field_get_offset) {
            ImGui::TextDisabled("field API unavailable");
            return;
        }

        void* klass = Il2CppApi::object_get_class ? Il2CppApi::object_get_class(obj) : nullptr;

        for (int depth = 0; klass && depth < 8; ++depth) {
            const char* kn = Il2CppApi::class_get_name ? Il2CppApi::class_get_name(klass) : "?";
            ImGui::SeparatorText(kn ? kn : "?");

            void* iter = nullptr;
            while (void* field = Il2CppApi::class_get_fields(klass, &iter)) {
                const char* fname = Il2CppApi::field_get_name(field);
                if (!fname) continue;

                size_t off = Il2CppApi::field_get_offset(field);

                // Type name is heap-allocated by the runtime and must be freed.
                std::string tname = "?";
                if (Il2CppApi::field_get_type && Il2CppApi::type_get_name) {
                    if (void* t = Il2CppApi::field_get_type(field)) {
                        if (char* tn = Il2CppApi::type_get_name(t)) {
                            tname = tn;
                            if (Il2CppApi::il2cpp_free) Il2CppApi::il2cpp_free(tn);
                        }
                    }
                }

                // Static fields report an offset relative to the class's static
                // storage, not the instance - reading them off `obj` would be
                // nonsense, so show them without a value.
                if (off == 0 || off > 0x10000) {
                    ImGui::Text("%-28s %-24s @0x%llX", fname, tname.c_str(),
                                (unsigned long long)off);
                    continue;
                }

                uint64_t raw = *reinterpret_cast<uint64_t*>((uintptr_t)obj + off);
                ImGui::Text("%-28s %-24s @0x%-4llX = 0x%llX", fname, tname.c_str(),
                            (unsigned long long)off, (unsigned long long)raw);
            }

            klass = Il2CppApi::class_get_parent ? Il2CppApi::class_get_parent(klass) : nullptr;
        }
    }

    // ---- hierarchy -------------------------------------------------------------

    static void DrawTransformNode(void* transform, int depth = 0) {
        if (!transform || depth > 32) return;   // depth cap: a cyclic parent would hang the frame

        void* go = g_fn.Transform_get_gameObject ? g_fn.Transform_get_gameObject(transform) : nullptr;
        std::string name = ObjectName(go ? go : transform);

        const char* cls = ClassNameOf(go ? go : transform);

        char label[512];
        snprintf(label, sizeof(label), "%s  [%s]  0x%llX##%p",
                 name.c_str(),
                 cls ? cls : "?",
                 (unsigned long long)(uintptr_t)transform,
                 transform);

        if (!ImGui::TreeNode(label)) return;

        if (go && ImGui::TreeNode("fields##go")) {
            DrawFields(go);
            ImGui::TreePop();
        }

        // childCount comes back in RAX; the thunk is typed void* so truncate it.
        int childCount = 0;
        if (g_fn.Transform_get_childCount)
            childCount = (int)(intptr_t)g_fn.Transform_get_childCount(transform);

        if (childCount > 0 && childCount < 100000 && g_fn.Transform_GetChild) {
            for (int i = 0; i < childCount; i++) {
                void* child = g_fn.Transform_GetChild(transform, i);
                if (child) DrawTransformNode(child, depth + 1);
            }
        }

        ImGui::TreePop();
    }

    // ---- lifecycle -------------------------------------------------------------

    void Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
        if (g_initialized) return;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(device, context);

        g_initialized = true;
    }

    void Shutdown() {
        if (!g_initialized) return;
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_initialized = false;
    }

    void ToggleVisibility() { g_visible = !g_visible; }
    bool IsVisible() { return g_visible; }

    void Render() {
        if (!g_initialized) return;

        // One ImGui frame serves both the inspector and the sprite menu. Bail only
        // when nothing at all is on screen, otherwise hiding the inspector would
        // silently take the menu with it.
        const bool wantInspector = g_visible;
        const bool wantMenu = CampusMenu::IsVisible();
        const bool wantDev = DevPanel::IsVisible();
        if (!wantInspector && !wantMenu && !wantDev) return;

        // Render() runs on the D3D present thread. Managed calls from a thread the
        // runtime has not attached corrupt state, so attach once before touching
        // anything in the game. Cheap after the first call.
        static bool s_attached = false;
        if (!s_attached) s_attached = Il2CppApi::AttachCurrentThread();

        ResolveUnity();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (wantInspector) {
        ImGui::Begin("Unity Explorer", &g_visible);

        if (!s_attached) {
            ImGui::TextColored(ImVec4(1, .4f, .4f, 1),
                "Could not attach this thread to the IL2CPP runtime.");
            // Say WHICH step failed - the bare message above sent us looking at the
            // thread attach when the real cause was GameAssembly's exports being
            // stripped, so nothing had resolved in the first place.
            ImGui::TextWrapped("Reason: %s",
                Il2CppApi::g_attachError[0] ? Il2CppApi::g_attachError : "unknown");
            ImGui::Separator();
            ImGui::TextDisabled("The full resolution table is in the console.");
            ImGui::TextDisabled("Ctrl+Shift+M opens the Campus menu, which does not need il2cpp.");

            // Retry each frame: at the login screen the runtime may not be up yet, and
            // a one-shot attempt would leave the inspector dead for the whole session.
            s_attached = Il2CppApi::AttachCurrentThread();
        }
        else if (!g_fn.ok) {
            ImGui::TextColored(ImVec4(1, .4f, .4f, 1), "Unity methods not resolved.");
            ImGui::TextDisabled("GameObject.Find      %s", g_fn.GameObject_Find ? "ok" : "MISSING");
            ImGui::TextDisabled("GameObject.transform %s", g_fn.GameObject_get_transform ? "ok" : "MISSING");
            ImGui::TextDisabled("Transform.childCount %s", g_fn.Transform_get_childCount ? "ok" : "MISSING");
            ImGui::TextDisabled("Transform.GetChild   %s", g_fn.Transform_GetChild ? "ok" : "MISSING");
            ImGui::Separator();
            ImGui::TextWrapped("These come from il2cpp metadata, not offsets. If they are "
                               "missing the runtime is not up yet - the game is still loading.");
        }
        else {
            // Type any root object's name; the tree is walked live from there.
            static char rootName[128] = "Canvas";
            ImGui::InputText("root object", rootName, sizeof(rootName));
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Exact GameObject name, as GameObject.Find takes it.");

            ImGui::Separator();

            using StrNewFn = void* (*)(const char*);
            auto strNew = RR::Methods::Il2cpp::il2cpp_string_new
                ? (StrNewFn)(GA + RR::Methods::Il2cpp::il2cpp_string_new)
                : nullptr;

            if (!strNew) {
                ImGui::Text("il2cpp_string_new unavailable.");
            }
            else {
                // Try what the user typed, then a few usual suspects.
                const char* candidates[] = { rootName, "Canvas", "UICanvas", "Main Canvas",
                                             "ExplorerCanvas", "AppNavMenu" };
                void* rootGO = nullptr;
                const char* found = nullptr;

                for (const char* rn : candidates) {
                    if (!rn || !*rn) continue;
                    if (void* s = strNew(rn)) {
                        rootGO = g_fn.GameObject_Find(s);
                        if (rootGO) { found = rn; break; }
                    }
                }

                if (rootGO) {
                    ImGui::Text("root: %s", found);
                    if (void* rootT = g_fn.GameObject_get_transform(rootGO))
                        DrawTransformNode(rootT);
                    else
                        ImGui::Text("root has no transform.");
                }
                else {
                    ImGui::Text("No object matched. Is the game past the loading screen?");
                }
            }
        }

        ImGui::End();
        }   // wantInspector

        // Drawn inside the same frame, after the inspector, so it composites on top.
        CampusMenu::Render();
        DevPanel::Render();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}
