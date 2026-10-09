#include "ProxyMain.h"
#include "DllLoader.h"
#include "UpdateRuntimeCore.h"


// ============================================================
// SINGLETON
// ============================================================

ProxyMain& ProxyMain::Instance()
{
    static ProxyMain instance;
    return instance;
}


// ============================================================
// START
// ============================================================

void ProxyMain::Start(
    HMODULE module)
{
    CreateThread(
        nullptr,
        0,
        MainThread,
        module,
        0,
        nullptr
    );
}


// ============================================================
// MAIN THREAD
// ============================================================

DWORD WINAPI ProxyMain::MainThread(
    LPVOID lpParam)
{
    ProxyMain::Instance().Run(
        static_cast<HMODULE>(lpParam)
    );

    return 0;
}


// ============================================================
// RUN
// ============================================================

void ProxyMain::Run(
    HMODULE module)
{
    DllLoader& loader =
        DllLoader::Instance();

    if (!loader.LoadOriginalDXGI())
    {
        FreeLibraryAndExitThread(
            module,
            0
        );

        return;
    }

    UpdateRuntimeCore::Instance()
        .CheckForUpdates();

    loader.LoadLibraries();

    while (true)
    {
        Sleep(1000);
    }
}


// ============================================================
// DLL MAIN
// ============================================================

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(
            hModule
        );

        ProxyMain::Instance().Start(
            hModule
        );
    }

    return TRUE;
}


// ============================================================
// DXGI EXPORTS
// ============================================================

extern "C"
__declspec(dllexport)
HRESULT WINAPI CreateDXGIFactory(
    REFIID riid,
    void** ppFactory)
{
    return ProxyMain::Instance()
        .CreateDXGIFactory(
            riid,
            ppFactory
        );
}


extern "C"
__declspec(dllexport)
HRESULT WINAPI CreateDXGIFactory1(
    REFIID riid,
    void ppFactory)
{
    return ProxyMain::Instance()
        .CreateDXGIFactory1(
            riid,
            ppFactory
        );
}


extern "C"
__declspec(dllexport)
HRESULT WINAPI CreateDXGIFactory2(
    UINT Flags,
    REFIID riid,
    void ppFactory)
{
    return ProxyMain::Instance()
        .CreateDXGIFactory2(
            Flags,
            riid,
            ppFactory
        );
}


extern "C"
__declspec(dllexport)
HRESULT WINAPI DXGIDeclareAdapterRemovalSupport()
{
    return ProxyMain::Instance()
        .DXGIDeclareAdapterRemovalSupport();
}
