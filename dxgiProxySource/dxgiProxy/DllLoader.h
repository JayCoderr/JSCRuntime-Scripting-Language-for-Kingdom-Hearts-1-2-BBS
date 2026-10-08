#pragma once

#include <Windows.h>

#include <string>
#include <vector>

struct LoadedLibrary
{
    std::string name;
    std::string path;
    HMODULE handle = nullptr;
};


using CreateDXGIFactory_t =
HRESULT(WINAPI*)(REFIID, void**);

using CreateDXGIFactory1_t =
HRESULT(WINAPI*)(REFIID, void**);

using CreateDXGIFactory2_t =
HRESULT(WINAPI*)(UINT, REFIID, void**);

using DXGIDeclareAdapterRemovalSupport_t =
HRESULT(WINAPI*)();


class DllLoader
{
public:

    static DllLoader& Instance();

    bool LoadOriginalDXGI();

    void LoadLibraries();

    bool LoadLibraryFromPath(
        const std::string& path);

    LoadedLibrary* FindLoadedLibrary(
        const std::string& name);

    bool UnloadLibraryByName(
        const std::string& name);

    bool ReplaceLibrary(
        const std::string& sourcePath,
        const std::string& destinationPath,
        const std::string& fileName);

    CreateDXGIFactory_t GetCreateDXGIFactory() const
    {
        return m_CreateDXGIFactory;
    }

    CreateDXGIFactory1_t GetCreateDXGIFactory1() const
    {
        return m_CreateDXGIFactory1;
    }

    CreateDXGIFactory2_t GetCreateDXGIFactory2() const
    {
        return m_CreateDXGIFactory2;
    }

    DXGIDeclareAdapterRemovalSupport_t
        GetDXGIDeclareAdapterRemovalSupport() const
    {
        return m_DXGIDeclareAdapterRemovalSupport;
    }

private:

    DllLoader() = default;
    ~DllLoader() = default;

    DllLoader(const DllLoader&) = delete;
    DllLoader& operator=(const DllLoader&) = delete;

    HMODULE m_originalDXGI = nullptr;

    CreateDXGIFactory_t
        m_CreateDXGIFactory = nullptr;

    CreateDXGIFactory1_t
        m_CreateDXGIFactory1 = nullptr;

    CreateDXGIFactory2_t
        m_CreateDXGIFactory2 = nullptr;

    DXGIDeclareAdapterRemovalSupport_t
        m_DXGIDeclareAdapterRemovalSupport = nullptr;

    std::vector<LoadedLibrary> m_loadedLibraries;
};