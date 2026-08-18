#include "RR/Patching/Patches.h"
#include "Protections/Protections.h"
#include "Utils/logging.h"
#include "Discord/DiscordRPC_Presence.h"
#include <DbgHelp.h>
#include <algorithm>
#include <vector>
#include <atomic>
#include <winhttp.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winhttp.lib")

static HMODULE g_selfModule = nullptr;
static char g_crashDumpDir[MAX_PATH] = {};
static bool g_antiCheatStarted = false;
static bool g_consoleMinimized = true;
static std::atomic<bool> g_keyListenerRunning{ true };
static std::atomic<bool> g_gameWindowFound{ false };
static HANDLE g_keyThread = nullptr;
static CRITICAL_SECTION g_windowCS;

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

static HWND SafeFindWindow(const char* title) {
    __try {
        return FindWindowA(nullptr, title);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static HWND SafeGetGameWindowRaw() {
    __try {
        HWND hWnd = FindWindowA(nullptr, "Rec Room");
        if (!hWnd)
            hWnd = FindWindowA(nullptr, "Unity Player");
        if (!hWnd)
            hWnd = FindWindowA(nullptr, "Campus");
        return hWnd;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static bool SafeIsWindow(HWND hWnd) {
    __try {
        return IsWindow(hWnd) != FALSE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SafeSetWindowText(HWND hWnd, const char* text) {
    __try {
        SetWindowTextA(hWnd, text);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SafeGetWindowRect(HWND hWnd, RECT* rect) {
    __try {
        return GetWindowRect(hWnd, rect) != FALSE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool SafeIsWindowVisible(HWND hWnd) {
    __try {
        return IsWindowVisible(hWnd) != FALSE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void SafeShowWindow(HWND hWnd, int cmd) {
    __try {
        ShowWindow(hWnd, cmd);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void SafeSetForegroundWindow(HWND hWnd) {
    __try {
        SetForegroundWindow(hWnd);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void SafeSetWindowTopmost(HWND hWnd) {
    __try {
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static void SafeSetWindowNotTopmost(HWND hWnd) {
    __try {
        SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static HWND GetGameWindow()
{
    EnterCriticalSection(&g_windowCS);
    HWND result = SafeGetGameWindowRaw();
    LeaveCriticalSection(&g_windowCS);
    return result;
}

static void SetCampusWindowTitle()
{
    HWND hWnd = GetGameWindow();
    if (hWnd && SafeIsWindow(hWnd)) {
        if (SafeSetWindowText(hWnd, "Campus")) {
            g_gameWindowFound = true;
        }
    }
}

static void SimulateAltEnter()
{
    HWND hWnd = GetGameWindow();
    if (!hWnd || !SafeIsWindow(hWnd)) return;

    INPUT ip[4] = {};
    int index = 0;

    ip[index].type = INPUT_KEYBOARD;
    ip[index].ki.wVk = VK_MENU;
    index++;

    ip[index].type = INPUT_KEYBOARD;
    ip[index].ki.wVk = VK_RETURN;
    index++;

    ip[index].type = INPUT_KEYBOARD;
    ip[index].ki.wVk = VK_RETURN;
    ip[index].ki.dwFlags = KEYEVENTF_KEYUP;
    index++;

    ip[index].type = INPUT_KEYBOARD;
    ip[index].ki.wVk = VK_MENU;
    ip[index].ki.dwFlags = KEYEVENTF_KEYUP;
    index++;

    SendInput(index, ip, sizeof(INPUT));
    std::cout << TAG_INIT() << "Simulated Alt+Enter\n";
}

static bool IsGameReady()
{
    HWND hWnd = GetGameWindow();
    if (!hWnd || !SafeIsWindow(hWnd) || !SafeIsWindowVisible(hWnd))
        return false;

    RECT rect;
    if (!SafeGetWindowRect(hWnd, &rect))
        return false;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    return (width > 400 && height > 300);
}

static DWORD WINAPI KeyListenerThread(LPVOID)
{
    std::cout << TAG_INIT() << "Key listener started\n";
    std::cout << TAG_INIT() << "Shift+ESC - Toggle console\n";
    std::cout << TAG_INIT() << "F11 - Toggle fullscreen\n";
    std::cout << TAG_INIT() << "F12 - Toggle always on top\n";

    bool shiftHeld = false;
    bool f11Pressed = false;
    bool f12Pressed = false;
    bool isTopmost = true;

    while (g_keyListenerRunning) {
        bool shiftNow = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
        bool escNow = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        bool f11Now = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
        bool f12Now = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;

        if (shiftNow && escNow && !shiftHeld) {
            HWND hWnd = GetConsoleWindow();
            if (hWnd && SafeIsWindow(hWnd)) {
                if (g_consoleMinimized) {
                    SafeShowWindow(hWnd, SW_RESTORE);
                    SafeSetForegroundWindow(hWnd);
                    g_consoleMinimized = false;
                    std::cout << TAG_INIT() << "Console restored\n";
                }
                else {
                    SafeShowWindow(hWnd, SW_MINIMIZE);
                    g_consoleMinimized = true;
                    std::cout << TAG_INIT() << "Console minimized\n";
                }
            }
            shiftHeld = true;
            Sleep(200);
        }
        else if (!shiftNow) {
            shiftHeld = false;
        }

        if (f11Now && !f11Pressed) {
            SimulateAltEnter();
            f11Pressed = true;
            Sleep(200);
        }
        else if (!f11Now) {
            f11Pressed = false;
        }

        if (f12Now && !f12Pressed) {
            HWND gameWnd = GetGameWindow();
            if (gameWnd && SafeIsWindow(gameWnd)) {
                isTopmost = !isTopmost;
                if (isTopmost) {
                    SafeSetWindowTopmost(gameWnd);
                    std::cout << TAG_INIT() << "Game set to TOPMOST\n";
                }
                else {
                    SafeSetWindowNotTopmost(gameWnd);
                    std::cout << TAG_INIT() << "Game removed from TOPMOST\n";
                }
            }
            f12Pressed = true;
            Sleep(200);
        }
        else if (!f12Now) {
            f12Pressed = false;
        }

        Sleep(30);
    }
    return 0;
}

static DWORD WINAPI AntiCheatStarterThread(LPVOID) {
    std::cout << "\n";
    std::cout << TAG_INIT() << "Waiting for game to fully load...\n";
    std::cout << TAG_INIT() << "Anti-cheat will start automatically when login page appears.\n";
    std::cout << "\n";

    int waitCount = 0;
    bool gameReady = false;

    while (waitCount < 600 && g_keyListenerRunning) {
        HWND gameWindow = GetGameWindow();
        if (gameWindow && SafeIsWindow(gameWindow)) {
            if (!g_gameWindowFound) {
                if (SafeSetWindowText(gameWindow, "Campus")) {
                    g_gameWindowFound = true;
                    std::cout << TAG_INIT() << "Window renamed to Campus\n";
                }
            }
            static bool topmostSet = false;
            if (!topmostSet) {
                SafeSetWindowTopmost(gameWindow);
                topmostSet = true;
                std::cout << TAG_INIT() << "Game set to TOPMOST\n";
            }
        }

        // Check if game is ready (only after 20 seconds to avoid false positives)
        if (waitCount > 200 && IsGameReady()) {
            gameReady = true;
            std::cout << "\n";
            std::cout << TAG_INIT() << "[+] Game window detected! Login page is visible.\n";
            break;
        }

        if (waitCount % 50 == 0 && waitCount > 0) {
            std::cout << TAG_INIT() << "Waiting for game... " << (waitCount / 10) << "s elapsed\n";
        }

        Sleep(100);
        waitCount++;
    }

    // Always start anti-cheat, even if game isn't ready yet
    std::cout << "\n";
    std::cout << "\t[////////////////////////////////////////////////////////////////]\n";
    std::cout << "       \t\tKurt Cobain just fucking hates you, go suck a fat one\n";
    std::cout << "\t[////////////////////////////////////////////////////////////////]\n";
    std::cout << "\n";

    // Initialize protections (this will run anti-debug checks but shouldn't block)
    Protections::Initialize();
    g_antiCheatStarted = true;

    // Start Discord RPC
    DiscordRPC::Start();

    return 0;
}

static void TestServerConnection() {
    std::cout << TAG_INIT() << "Testing connection to " << CAMPUS_NAMESERVER_HOST << "...\n";

    HINTERNET hSession = WinHttpOpen(L"Test/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (hSession) {
        std::wstring host = L"gameauth.ddns.net";
        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/", NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
            if (hRequest) {
                // Disable certificate verification for testing
                DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));

                if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0)) {
                    std::cout << TAG_INIT() << "[+] Connection test to gameauth.ddns.net SUCCESSFUL\n";
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return;
                }
                else {
                    DWORD error = GetLastError();
                    std::cout << TAG_INIT() << "[-] Connection test to gameauth.ddns.net FAILED (error: " << error << ")\n";
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }
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

    CampusLogging::Init(g_selfModule);

    ULONGLONG t0 = GetTickCount64();
    RR::Patches::Resolve();
    ULONGLONG t1 = GetTickCount64();
    RR::Patches::Patch();
    ULONGLONG t2 = GetTickCount64();

    std::cout << "\n";
    std::cout << "\t[////////////////////////////////////////////////////////////////]\n";
    std::cout << "\t                      I miss Vanessa so badly\n";
    std::cout << "\t[////////////////////////////////////////////////////////////////]\n";
    std::cout << "\n";
    std::cout << TAG_INIT() << "Target Server: " << CLR_YELLOW << "gameauth.ddns.net" << CLR_RESET << "\n";
    std::cout << TAG_INIT() << "Build Date:   " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << TAG_INIT() << "Patches:      Referee [OK]  TLS [OK]  HTTP [OK]  UI [OK]\n";
    std::cout << TAG_INIT() << "ImageSig:     DISABLED\n";
    std::cout << TAG_INIT() << "Radio:        DISABLED\n";
    std::cout << TAG_INIT() << "FOV:          DISABLED\n";
    std::cout << TAG_INIT() << "Timing:       Resolve " << (t1 - t0) << "ms, Patch " << (t2 - t1) << "ms, Total " << (t2 - t0) << "ms\n";
    std::cout << "\n";
    std::cout << TAG_INIT() << "[+] Patches applied successfully!\n";
    std::cout << "\n";

    // Test server connection
    TestServerConnection();

    std::cout << TAG_INIT() << "Starting anti-cheat starter thread...\n";
    HANDLE hThread = CreateThread(nullptr, 0, AntiCheatStarterThread, nullptr, 0, nullptr);
    if (hThread) CloseHandle(hThread);

    std::cout << TAG_INIT() << "Starting key listener thread...\n";
    g_keyThread = CreateThread(nullptr, 0, KeyListenerThread, nullptr, 0, nullptr);
    if (g_keyThread) CloseHandle(g_keyThread);

    return 0;
}

static void CreateConsole()
{
    SetEnvironmentVariableA("STEAMVR_ENABLED", "0");
    SetEnvironmentVariableA("OPENVR_ENABLED", "0");
    SetEnvironmentVariableA("DISABLE_VR", "1");
    SetEnvironmentVariableA("VR_DISABLED", "1");

    AllocConsole();

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE && hConsole != nullptr) {
        COORD bufferSize = { 200, 9999 };
        SetConsoleScreenBufferSize(hConsole, bufferSize);
        SMALL_RECT windowSize = { 0, 0, 190, 50 };
        SetConsoleWindowInfo(hConsole, TRUE, &windowSize);
    }

    FILE* fOut = nullptr;
    FILE* fErr = nullptr;
    freopen_s(&fOut, "CONOUT$", "w", stdout);
    freopen_s(&fErr, "CONOUT$", "w", stderr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    SetConsoleTitleA("Vanessa");

    HWND hWnd = GetConsoleWindow();
    if (hWnd) {
        __try {
            HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            mi.cbSize = sizeof(MONITORINFO);
            if (GetMonitorInfoA(hMonitor, &mi)) {
                SetWindowPos(hWnd, HWND_TOPMOST,
                    mi.rcWork.left, mi.rcWork.top,
                    mi.rcWork.right - mi.rcWork.left,
                    mi.rcWork.bottom - mi.rcWork.top,
                    SWP_SHOWWINDOW);
            }
            else {
                SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 1400, 900, SWP_SHOWWINDOW);
            }

            LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
            style = WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU |
                WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
            SetWindowLongPtr(hWnd, GWL_STYLE, style);

            SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

            ShowWindow(hWnd, SW_MINIMIZE);
            g_consoleMinimized = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    if (hConsole != INVALID_HANDLE_VALUE && hConsole != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(hConsole, &mode)) {
            SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    printf("\x1b[1m\x1b[38;5;120m[CampusPatch]\x1b[0m loaded - target: "
        "\x1b[38;5;227mgameauth.ddns.net\x1b[0m  "
        "\x1b[38;5;240m(build " __DATE__ " " __TIME__ ")\x1b[0m\n");
}

static void CleanupThreads()
{
    g_keyListenerRunning = false;

    if (g_keyThread) {
        WaitForSingleObject(g_keyThread, 2000);
        CloseHandle(g_keyThread);
        g_keyThread = nullptr;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        InitializeCriticalSection(&g_windowCS);
        DisableThreadLibraryCalls(hModule);
        g_selfModule = hModule;
        InstallCrashHandler(hModule);
        CreateConsole();
        HANDLE h = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (g_antiCheatStarted) {
            CleanupThreads();
            Protections::Shutdown();
            DiscordRPC::Shutdown();
            CampusLogging::Shutdown();
        }
        DeleteCriticalSection(&g_windowCS);
        FreeConsole();
    }
    return TRUE;
}