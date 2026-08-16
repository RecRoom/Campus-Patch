#include "RR/Patching/Patches.h"
#include "Protections/Protections.h"
#include "Utils/logging.h"
#include "Discord/DiscordRPC_Presence.h"
#include <DbgHelp.h>
#include <algorithm>
#include <vector>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "user32.lib")

static HMODULE g_selfModule = nullptr;
static char g_crashDumpDir[MAX_PATH] = {};
static bool g_antiCheatStarted = false;
static bool g_consoleMinimized = true;  // Start as minimized

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

static HWND GetGameWindow()
{
    HWND hWnd = FindWindowA(nullptr, "Rec Room");

    if (!hWnd)
        hWnd = FindWindowA(nullptr, "Unity Player");

    if (!hWnd)
        hWnd = FindWindowA(nullptr, "Campus");

    return hWnd;
}

static void SetCampusWindowTitle()
{
    HWND hWnd = GetGameWindow();

    if (hWnd)
        SetWindowTextA(hWnd, "Campus");
}

static bool IsGameReady()
{
    HWND hWnd = GetGameWindow();

    if (!hWnd || !IsWindowVisible(hWnd))
        return false;

    RECT rect;
    if (!GetWindowRect(hWnd, &rect))
        return false;

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    if (width > 400 && height > 300)
        return true;

    return false;
}


// Thread to listen for Shift+ESC key combination

static DWORD WINAPI KeyListenerThread(LPVOID)
{
    std::cout << TAG_INIT() << "Key listener started - Press Shift+ESC to toggle console\n";
    std::cout << TAG_INIT() << "Console started minimized - Press Shift+ESC to show it\n";

    while (true) {
        // Check if Shift is held down
        bool shiftHeld = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;

        // Check if ESC is pressed
        bool escPressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

        if (shiftHeld && escPressed) {
            HWND hWnd = GetConsoleWindow();
            if (hWnd) {
                if (g_consoleMinimized) {
                    // Restore the console
                    ShowWindow(hWnd, SW_RESTORE);
                    SetForegroundWindow(hWnd);
                    g_consoleMinimized = false;
                    std::cout << TAG_INIT() << "Console restored\n";
                }
                else {
                    // Minimize the console
                    ShowWindow(hWnd, SW_MINIMIZE);
                    g_consoleMinimized = true;
                    std::cout << TAG_INIT() << "Console minimized\n";
                }
            }
            // Wait for keys to be released to prevent multiple toggles
            Sleep(300);
        }
        Sleep(50);
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

    while (waitCount < 600) {
        HWND gameWindow = GetGameWindow();
        if (gameWindow) {
            SetWindowTextA(gameWindow, "Campus");
        }

        if (IsGameReady()) {
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

    if (!gameReady) {
        std::cout << TAG_INIT() << "[!] Game not detected after 60s, starting anti-cheat anyway...\n";
    }
    else {
        Sleep(1000);
        std::cout << TAG_INIT() << "[+] Game is ready! Starting anti-cheat...\n";
    }

    std::cout << "\n";
    std::cout << "\t[////////////////////////////////////////////////////////////////]\n";
    std::cout << "       \tKurt Cobain just fucking hates you, go suck a fat one\n";
    std::cout << "\t[////////////////////////////////////////////////////////////////]\n";
    std::cout << "\n";

    Protections::Initialize();
    g_antiCheatStarted = true;

    DiscordRPC::Start();

    return 0;
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

    std::cout << TAG_INIT() << "Starting anti-cheat starter thread...\n";
    HANDLE hThread = CreateThread(nullptr, 0, AntiCheatStarterThread, nullptr, 0, nullptr);
    if (hThread) CloseHandle(hThread);

    // Start key listener thread for Shift+ESC
    HANDLE hKeyThread = CreateThread(nullptr, 0, KeyListenerThread, nullptr, 0, nullptr);
    if (hKeyThread) CloseHandle(hKeyThread);

    return 0;
}


// CONSOLE WITH WORKING MINIMIZE BUTTON AND TRANSPARENCY
static void CreateConsole()
{
    SetEnvironmentVariableA("STEAMVR_ENABLED", "0");
    SetEnvironmentVariableA("OPENVR_ENABLED", "0");
    SetEnvironmentVariableA("DISABLE_VR", "1");
    SetEnvironmentVariableA("VR_DISABLED", "1");

    // Allocate console
    AllocConsole();

    // Set console buffer size
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE && hConsole != nullptr) {
        COORD bufferSize = { 200, 9999 };
        SetConsoleScreenBufferSize(hConsole, bufferSize);
        SMALL_RECT windowSize = { 0, 0, 190, 55 };
        SetConsoleWindowInfo(hConsole, TRUE, &windowSize);
    }

    // Redirect stdout/stderr
    FILE* fOut = nullptr;
    FILE* fErr = nullptr;
    freopen_s(&fOut, "CONOUT$", "w", stdout);
    freopen_s(&fErr, "CONOUT$", "w", stderr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // Set console title
    SetConsoleTitleA("Vanessa");

    // Get console window handle
    HWND hWnd = GetConsoleWindow();
    if (hWnd) {

        // Add minimize/maximize buttons to console

        LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);

        // These are the styles needed for a proper window with minimize button
        style = WS_OVERLAPPEDWINDOW |  // Standard overlapped window
            WS_CAPTION |           // Title bar
            WS_SYSMENU |           // System menu (minimize/maximize/close buttons)
            WS_MINIMIZEBOX |       // Minimize button
            WS_MAXIMIZEBOX |       // Maximize button
            WS_THICKFRAME;         // Resizable border

        SetWindowLongPtr(hWnd, GWL_STYLE, style);

        // Force the window to update its frame
        SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        // Enable transparency
        SetWindowLong(hWnd, GWL_EXSTYLE,
            GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOOLWINDOW);
        SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 215, LWA_ALPHA);

        // Make window larger
        SetWindowPos(hWnd, HWND_TOPMOST, 100, 50, 1400, 900, SWP_SHOWWINDOW);


        // START THE CONSOLE AS MINIMIZED

        ShowWindow(hWnd, SW_MINIMIZE);
        g_consoleMinimized = true;

        // Refresh the window to show the new buttons
        RedrawWindow(hWnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
    }

    // Enable virtual terminal processing for ANSI colors
    if (hConsole != INVALID_HANDLE_VALUE && hConsole != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(hConsole, &mode)) {
            SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    // Print banner (will be visible when console is restored)
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
        if (g_antiCheatStarted) {
            Protections::Shutdown();
        }
        FreeConsole();
    }
    return TRUE;
}