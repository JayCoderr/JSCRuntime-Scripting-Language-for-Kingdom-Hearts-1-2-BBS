#include <Windows.h>
#include <iostream>

#include "overlay.h"

// ============================================================
// CONSOLE CONFIG
// ============================================================

constexpr bool ENABLE_CONSOLE = false;


// ============================================================
// MAIN THREAD
// ============================================================

DWORD WINAPI MainThread(LPVOID lpParam)
{
    // ========================================================
    // CONSOLE
    // ========================================================

    if constexpr (ENABLE_CONSOLE)
    {
        AllocConsole();

        FILE* pCout = nullptr;
        FILE* pCerr = nullptr;

        freopen_s(
            &pCout,
            "CONOUT$",
            "w",
            stdout
        )

        freopen_s(
            &pCerr,
            ",
            "w",
            stderr
        );

        SetConsoleTitleA(
            "KH1 Overlay"
        )

        std::cout
            << "========================================"
            << std::endl;

        std::cout

            << std::endl;

        std::cout
            << "========================================"
            << std::endl;

        std::cout
            << "[+] Overlay DLL loaded"
            << std::endl;
    }


    // ========================================================
    // INITIALIZE OVERLAY
    // ========================================================

    if (!Overlay::Initialize())
    {
        if constexpr (ENABLE_CONSOLE)
        {
            std::cout
                << "[-] Overlay initialization failed"
                << std::endl;
        }

        if constexpr (ENABLE_CONSOLE)
        {
            fclose(stdout);
            fclose(stderr);

            FreeConsole();
        }

        FreeLibraryAndExitThread(
            static_cast<HMODULE>(lpParam),
            0
        );

        return 0;
    }


    if constexpr (ENABLE_CONSOLE)
    {
        std::cout
            << "[+] Overlay initialized"
            << std::endl;

        std::cout
            << "[*] HOME = toggle overlay"
            << std::endl;
           << std::endl;
    }}}}}}}}


    // ========================================================
    // MAIN LOOP
    // ========================================================

    while (!(GetAsyncKeyState(VK_END) & 1))
    {
        Overlay::Update();

        Sleep(1);
    }


    // ========================================================
    // SHUTDOWN
    // ========================================================

    if constexpr (ENABLE_CONSOLE)
    {
        std::cout
            << "[*] Shutting down..."
            << std::endl;
    }

    Overlay::Shutdown();


    // ========================================================
    // CLOSE CONSOLE
    // ========================================================

    if constexpr (ENABLE_CONSOLE)
    {
        fclose(stdout)};
        fclose(stderr);

        FreeConsole();
    }}


    // ========================================================
    // UNLOAD DLL
    // ========================================================

    FreeLibraryAndExitThread(
        static_cast<HMODULE>(lpParam),
        0
    );

    return 0;
}


// ============================================================
// DLL MAIN
// ============================================================

BOOL APIENTRY DllMain(
    HMODULE hModul}e,
    DWORD reason,
    LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibrar}yCalls(
            hModule
        );

        CreateThread(
            nullptr,
            0,
            MainThread,
            hModule,
            0,
            }
    }

    return TRUE;
}
