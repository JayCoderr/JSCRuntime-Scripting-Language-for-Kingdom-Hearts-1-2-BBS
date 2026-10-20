
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <string>

#include <MinHook.h>

#include "overlay.h"
#include "DebugFunctionAddress.h"

// ============================================================
// CONSOLE CONFIG
// ============================================================

constexpr bool ENABLE_CONSOLE = true;

// ============================================================
// RUNTIME STATE
// ============================================================

static std::atomic<bool> g_ShutdownRequested{ false };
static HANDLE g_MainThreadHandle = nullptr;
static HANDLE g_ShutdownEvent = nullptr;
static bool g_OwnsMinHook = false;

// ============================================================
// SHUTDOWN EXPORT
// ============================================================

extern "C" __declspec(dllexport)
BOOL ShutdownRuntime(DWORD timeoutMs)
{
    g_ShutdownRequested.store(true);

    HANDLE shutdownEvent = g_ShutdownEvent;

    if (shutdownEvent == nullptr)
        return FALSE;

    return WaitForSingleObject(
        shutdownEvent,
        timeoutMs) == WAIT_OBJECT_0;
}

// ============================================================
// INITIALIZE RUNTIME HOOKS
// Call only after a valid level has been detected.
// ============================================================

bool InitializeRuntimeHooks()
{
    struct RuntimeHook
    {
        const char* name;
        bool (*initialize)();
    };

    static const RuntimeHook hooks[] =
    {
        { "EntityPoolFunctionHook", InitializeEntityPoolFunctionHook },

        // Add more hooks here:
        // { "AnotherHook", InitializeAnotherHook },
    };

    for (const auto& hook : hooks)
    {
        if (!hook.initialize())
        {
            if constexpr (ENABLE_CONSOLE)
            {
                std::cout
                    << "[-] Failed to initialize "
                    << hook.name
                    << std::endl;
            }

            return false;
        }

        if constexpr (ENABLE_CONSOLE)
        {
            std::cout
                << "[+] Initialized "
                << hook.name
                << std::endl;
        }
    }

    return true;
}

// ============================================================
// INITIALIZE CONSOLE
// ============================================================

static bool InitializeConsole()
{
    if constexpr (!ENABLE_CONSOLE)
        return true;

    if (!GetConsoleWindow())
    {
        if (!AllocConsole())
        {
            OutputDebugStringA(
                "[KH1 Overlay] AllocConsole failed.\n");
            return false;
        }
    }

    FILE* pCout = nullptr;
    FILE* pCerr = nullptr;
    FILE* pCin = nullptr;

    errno_t outResult =
        freopen_s(&pCout, "CONOUT$", "w", stdout);

    errno_t errResult =
        freopen_s(&pCerr, "CONOUT$", "w", stderr);

    errno_t inResult =
        freopen_s(&pCin, "CONIN$", "r", stdin);

    if (outResult != 0 || errResult != 0)
    {
        OutputDebugStringA(
            "[KH1 Overlay] Failed to redirect console streams.\n");
        return false;
    }

    std::ios::sync_with_stdio(true);

    SetConsoleTitleA("KH1 Overlay");

    HWND consoleWindow = GetConsoleWindow();

    if (consoleWindow)
        ShowWindow(consoleWindow, SW_SHOW);

    std::cout << "========================================" << std::endl;
    std::cout << " KH1&2&BBS OVERLAY" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "[+] Overlay DLL loaded" << std::endl;
    std::cout << "[+] Console initialized" << std::endl;

    return true;
}

// ============================================================
// MAIN THREAD
// ============================================================

DWORD WINAPI MainThread(LPVOID lpParam)
{
    (void)lpParam;

    OutputDebugStringA("[KH1 Overlay] MainThread entered.\n");

    InitializeConsole();

    bool overlayInitialized = false;

    // ========================================================
    // INITIALIZE MINHOOK ONCE
    // ========================================================

    MH_STATUS mhStatus = MH_Initialize();

    if (mhStatus == MH_OK)
    {
        g_OwnsMinHook = true;

        if constexpr (ENABLE_CONSOLE)
            std::cout << "[+] MinHook initialized" << std::endl;
    }
    else if (mhStatus == MH_ERROR_ALREADY_INITIALIZED)
    {
        if constexpr (ENABLE_CONSOLE)
            std::cout << "[*] MinHook was already initialized" << std::endl;
    }
    else
    {
        if constexpr (ENABLE_CONSOLE)
        {
            std::cout
                << "[-] MinHook initialization failed: "
                << MH_StatusToString(mhStatus)
                << std::endl;
        }

        goto shutdown;
    }

    // ========================================================
    // WAIT FOR A VALID LEVEL
    // Do not shut down just because the game is in its menus.
    // ========================================================

    if constexpr (ENABLE_CONSOLE)
        std::cout << "[*] Waiting for a valid level..." << std::endl;

    while (!g_ShutdownRequested.load() &&
        !(GetAsyncKeyState(VK_END) & 1))
    {
        const std::string area = GetCurrentLevelArea();

        if (!area.empty() && area != "Unknown")
        {
            if constexpr (ENABLE_CONSOLE)
            {
                std::cout
                    << "[+] Valid level detected: "
                    << area
                    << std::endl;
            }

            break;
        }

        Sleep(500);
    }

    if (g_ShutdownRequested.load() ||
        (GetAsyncKeyState(VK_END) & 1))
    {
        goto shutdown;
    }

    // ========================================================
    // INITIALIZE RUNTIME HOOKS ONCE
    // ========================================================

    if (!InitializeRuntimeHooks())
    {
        if constexpr (ENABLE_CONSOLE)
            std::cout << "[-] Runtime hook initialization failed" << std::endl;

        goto shutdown;
    }

    // ========================================================
    // INITIALIZE OVERLAY
    // ========================================================

    if (!Overlay::Initialize())
    {
        if constexpr (ENABLE_CONSOLE)
            std::cout << "[-] Overlay initialization failed" << std::endl;

        goto shutdown;
    }

    overlayInitialized = true;

    if constexpr (ENABLE_CONSOLE)
    {
        std::cout << "[+] Overlay initialized" << std::endl;
        std::cout << "[*] HOME = toggle overlay" << std::endl;
        std::cout << "[*] END = unload overlay" << std::endl;
        std::cout << "[*] Waiting loop is no longer active" << std::endl;
    }

    // ========================================================
    // MAIN LOOP
    // ========================================================

    while (!g_ShutdownRequested.load() &&
        !(GetAsyncKeyState(VK_END) & 1))
    {
        Overlay::Update();
        Sleep(1);
    }

shutdown:

    // ========================================================
    // SHUTDOWN OVERLAY
    // ========================================================

    if constexpr (ENABLE_CONSOLE)
        std::cout << "[*] Shutting down..." << std::endl;

    if (overlayInitialized)
        Overlay::Shutdown();

    // ========================================================
    // SHUTDOWN MINHOOK
    // ========================================================

    if (g_OwnsMinHook)
    {
        if constexpr (ENABLE_CONSOLE)
            std::cout << "[*] Shutting down MinHook..." << std::endl;

        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        g_OwnsMinHook = false;

        if constexpr (ENABLE_CONSOLE)
            std::cout << "[+] MinHook shut down" << std::endl;
    }

    // ========================================================
    // CLOSE CONSOLE
    // ========================================================

    if constexpr (ENABLE_CONSOLE)
    {
        std::cout << "[+] Runtime shutdown complete" << std::endl;

        std::cout.flush();
        std::cerr.flush();

        FreeConsole();
    }

    // ========================================================
    // SIGNAL SHUTDOWN COMPLETE
    // ========================================================

    if (g_ShutdownEvent != nullptr)
        SetEvent(g_ShutdownEvent);

    return 0;
}

// ============================================================
// DLL MAIN
// ============================================================

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID lpReserved)
{
    (void)lpReserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        g_ShutdownRequested.store(false);

        g_ShutdownEvent = CreateEventA(
            nullptr,
            TRUE,
            FALSE,
            nullptr);

        if (g_ShutdownEvent == nullptr)
            return FALSE;

        g_MainThreadHandle = CreateThread(
            nullptr,
            0,
            MainThread,
            hModule,
            0,
            nullptr);

        if (g_MainThreadHandle == nullptr)
        {
            CloseHandle(g_ShutdownEvent);
            g_ShutdownEvent = nullptr;
            return FALSE;
        }
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        // Do not wait for the worker thread inside DllMain.
        // Waiting under the loader lock can deadlock.
        if (g_MainThreadHandle != nullptr)
        {
            CloseHandle(g_MainThreadHandle);
            g_MainThreadHandle = nullptr;
        }

        if (g_ShutdownEvent != nullptr)
        {
            CloseHandle(g_ShutdownEvent);
            g_ShutdownEvent = nullptr;
        }
    }

    return TRUE;
}
