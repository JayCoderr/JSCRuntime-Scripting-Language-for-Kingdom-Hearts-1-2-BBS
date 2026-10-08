#pragma once

#include <Windows.h>
#include <unknwn.h>

class ProxyMain
{
public:

    static ProxyMain& Instance();

    void Start(HMODULE module);

    HRESULT CreateDXGIFactory(
        REFIID riid,
        void** ppFactory);

    HRESULT CreateDXGIFactory1(
        REFIID riid,
        void** ppFactory);

    HRESULT CreateDXGIFactory2(
        UINT Flags,
        REFIID riid,
        void** ppFactory);

    HRESULT DXGIDeclareAdapterRemovalSupport();

private:

    ProxyMain() = default;
    ~ProxyMain() = default;

    ProxyMain(const ProxyMain&) = delete;
    ProxyMain& operator=(const ProxyMain&) = delete;

    static DWORD WINAPI MainThread(
        LPVOID lpParam);

    void Run(
        HMODULE module);
};