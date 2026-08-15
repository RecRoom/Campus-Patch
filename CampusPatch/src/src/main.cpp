#include "RR/Patching/Patches.h"
#include "Protections/Protections.h"
#include "Utils/logging.h"
#include "Discord/DiscordRPC_Presence.h"
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

static HMODULE g_selfModule = nullptr;
static char g_crashDumpDir[MAX_PATH] = {};

static LONG WINAPI CrashDumpFilter(EXCEPTION_POINTERS* ep) {
    if (g_crashDumpDir[0] == 0) return EXCEPTION_CONTINUE_SEARCH;

    SYSTEMTIME st; GetLocalTime(&st);
    char path[MAX_PATH];
    _snprintf_s(path, sizeof(path), _TRUNCATE,
        "%s\\crash_%04d%02d%02d_%02d%02d%02d.dmp",
        g_crashDumpDir,
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);

    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return EXCEPTION_CONTINUE_SEARCH;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
        MiniDumpNormal, &mei, nullptr, nullptr);
    CloseHandle(f);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void InstallCrashHandler(HMODULE self) {
    char dllPath[MAX_PATH] = {};
    if (!GetModuleFileNameA(self, dllPath, MAX_PATH)) return;
    char* slash = strrchr(dllPath, '\\');
    if (slash) *slash = 0;
    _snprintf_s(g_crashDumpDir, sizeof(g_crashDumpDir), _TRUNCATE, "%s\\Logs", dllPath);
    CreateDirectoryA(g_crashDumpDir, nullptr);
    SetUnhandledExceptionFilter(CrashDumpFilter);
}

static DWORD WINAPI InitThread(LPVOID)
{
    for (int i = 0; i < 200; i++) {
        if (GetModuleHandleA("GameAssembly.dll") && GetModuleHandleA("Referee.dll")) break;
        Sleep(100);
    }
    if (!InitGlobals()) {
        printf("[Campus] init failed - aborting (no hooks installed)\n");
        return 1;
    }

    Protections::Initialize();

    CampusLogging::Init(g_selfModule);

    ULONGLONG t0 = GetTickCount64();
    RR::Patches::Resolve();
    ULONGLONG t1 = GetTickCount64();
    RR::Patches::Patch();
    ULONGLONG t2 = GetTickCount64();

    std::cout << TAG_INIT() << "timing: Resolve " << (t1 - t0) << "ms, Patch "
        << (t2 - t1) << "ms, total " << (t2 - t0) << "ms\n";

    DiscordRPC::Start();

    return 0;
}

static void CreateConsole()
{
    SetEnvironmentVariableA("STEAMVR_ENABLED", "0");
    SetEnvironmentVariableA("OPENVR_ENABLED", "0");
    SetEnvironmentVariableA("DISABLE_VR", "1");
    SetEnvironmentVariableA("VR_DISABLED", "1");

    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "NUL", "w", stderr);
    SetConsoleTitleA("Campus - CampusPatch2025");

    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE && h != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode))
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    printf("\x1b[1m\x1b[38;5;120m[CampusPatch]\x1b[0m loaded - target: "
        "\x1b[38;5;227mgameauth.ddns.net\x1b[0m  "
        "\x1b[38;5;240m(build " __DATE__ " " __TIME__ ")\x1b[0m\n");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_selfModule = hModule;
        InstallCrashHandler(hModule);
        CreateConsole();
        HANDLE h = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        DiscordRPC::Shutdown();
    }
    return TRUE;
}