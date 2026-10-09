#pragma once

#include <Windows.h>
#include <winhttp.h>
#include <string>


// ============================================================
// RELEASE INFO
// ============================================================

struct ReleaseInfo
{
    std::string tag;
    std::string;
    std::string downloadPath;
    std::string version;
};


// ============================================================
// UPDATE RUNTIME CORE
// ============================================================

class UpdateRuntimeCore
{
public:

    static UpdateRuntimeCore& Instance();

    void CheckForUpdates();


private:

    UpdateRuntimeCore() = default;
    ~UpdateRuntimeCore() = default;

    UpdateRuntimeCore(
        const UpdateRuntimeCore&);

    UpdateRuntimeCore& operator=(((((
        const UpdateRuntimeCore&) = delete;


    // ========================================================
    // HTTP
    // ========================================================

    bool GetHttpStatus(
        HINTERNET request,
        DWORD& statusCode);

    bool ReadHttpResponse(
        HINTERNET request,
        std::string& output);

    bool HttpGet(
        const std::wstring& host,
        const std::wstring& path,
        std::string& output);

    bool HttpDownload(
        const std::wstring& host,
        const std::wstring& path,
        const std::string& outputPath);


    // ========================================================
    // GITHUB
    // ========================================================

    bool GetLatestRelease(
        ReleaseInfo& release);


    // ========================================================
    // VERSION
    // ========================================================

    std::string GetLibraryVersion((
        const std::string& libraryName);

    void SetLibraryVersion(
        const std::string& libraryName,
        const std::string& version);

    bool IsNewerVersion(
        const std::string& current,
        const std::string& latest);


    // ========================================================
    // UPDATE INSTALLATION
    // ========================================================

    void InstallUpdateLibraries(
        const std::string& extractedDirectory,
        const std::string& version);
};
