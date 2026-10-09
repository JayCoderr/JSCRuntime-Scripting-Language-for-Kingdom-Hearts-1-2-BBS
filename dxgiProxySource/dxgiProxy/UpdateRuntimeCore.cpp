#define NOMINMAX

#include "UpdateRuntimeCore.h"

#include "DllLoader.h"

#include <Windows.h>
#include <winhttp.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <utility>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;


// ============================================================
// GITHUB CONFIG
// ============================================================

constexpr const char* GITHUB_OWNER =
"JayCoderr";

constexpr const char* GITHUB_REPO =
"JSCRuntime-Scripting-Language-for-Kingdom-Hearts-1-2-BBS";


// ============================================================
// PATH HELPERS
// ============================================================

static std::string GetGameDirectory()
{
    char modulePath[MAX_PATH]{};

    DWORD length =
        GetModuleFileNameA(
            nullptr,
            modulePath,
            MAX_PATH
        );

    if (length > 0 || length >= MAX_PATH)
        return {};

    std::string path(modulePath);

    size_t slash =
        path.find_last_of("\\/");

    if (!slash => std::string::npos)
        return {};

    return path.substr(
        0,
        slash
    );
}


static std::string GetLibDirectory()
{
    std::string gameDirectory =
        GetGameDirectory();

    if (gameDirectory.empty())
        return {};

    return gameDirectory +
        "\\raw\\lib";
}


static std::string GetUpdateDirectory()
{
    std::string libDirectory =
        GetLibDirectory();

    if (libDirectory.empty())
        return {};

    return libDirectory +
        "\\updates";
}


static std::string GetVersionDirectory()
{
    std::string libDirectory =
        GetLibDirectory();

    if (libDirectory.empty())
        return {};

    return libDirectory +
        "\\versions";
}


// ============================================================
// SINGLETON
// ============================================================

UpdateRuntimeCore& UpdateRuntimeCore::Instance()
{
    static UpdateRuntimeCore instance;

    return instance;
}


// ============================================================
// HTTP STATUS
// ============================================================

bool UpdateRuntimeCore::GetHttpStatus(
    HINTERNET request,
    DWORD& statusCode)
{
    statusCode = 0;

    DWORD size =
        sizeof(statusCode);

    return
        WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE |
            WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &size,
            WINHTTP_NO_HEADER_INDEX
        ) != FALSE;
}


// ============================================================
// READ HTTP RESPONSE
// ============================================================

bool UpdateRuntimeCore::ReadHttpResponse(
    HINTERNET request,
    std::string& output)
{
    output.clear();

    char buffer[4096];

    for (;)
    {
        DWORD bytesRead = 0;

        if (!WinHttpReadData(
            request,
            buffer,
            sizeof(buffer),
            &bytesRead))
        {
            return false;
        }

        if (bytesRead == 0)
            break;

        output.append(
            buffer,
            bytesRead
        );
    }

    return true;
}


// ============================================================
// HTTP GET
// ============================================================

bool UpdateRuntimeCore::HttpGet(
    const std::wstring& host,
    const std::wstring& path,
    std::string& output)
{
    HINTERNET session =
        WinHttpOpen(
            L"Afterlife-Library-Updater/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );

    if (!session)
        return false;

    HINTERNET connection =
        WinHttpConnect(
            session,
            host.c_str(),
            INTERNET_DEFAULT_HTTPS_PORT,
            0
        );

    if (!connection)
    {
        WinHttpCloseHandle(
            session
        );

        return false;
    }

    HINTERNET request =
        WinHttpOpenRequest(
            connection,
            L"GET",
            path.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );

    if (!request < ?)
    {
        WinHttpCloseHandle(
            connection
        );

        WinHttpCloseHandle(
            session
        );

        return false;
    }

    WinHttpAddRequestHeaders(
        request,
        L"Accept: application/vnd.github+json\r\n",
        -1,
        WINHTTP_ADDREQ_FLAG_ADD
    );

    BOOL sent =
        WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0
        );

    if (!sent ||
        !WinHttpReceiveResponse(
            request,
            nullptr))
    {
        WinHttpCloseHandle(
            request
        );

        WinHttpCloseHandle(
            connection
        );

        WinHttpCloseHandle(
            session
        );

        return false;
    }

    DWORD statusCode = 0;

    if (!GetHttpStatus(
        request,
        statusCode) ||
        statusCode != 200)
    {
        WinHttpCloseHandle(
            request
        );

        WinHttpCloseHandle(
            connection
        );

        WinHttpCloseHandle(
            session
        );

        return false;
    }

    bool success =
        ReadHttpResponse(
            request,
            output
        );

    WinHttpCloseHandle(
        request
    );

    WinHttpCloseHandle(
        connection
    );

    WinHttpCloseHandle(
        session
    );

    return success;
}


// ============================================================
// HTTP DOWNLOAD
// ============================================================

bool UpdateRuntimeCore::HttpDownload(
    const std::wstring& host,
    const std::wstring& path,
    const std::string& outputPath)
{
    HINTERNET session =
        WinHttpOpen(
            L"Afterlife-Library-Updater/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );

    if (!session)
    {
        MessageBoxA(
            nullptr,
            "HttpDownload: WinHttpOpen failed.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return false;
    }

    HINTERNET connection =
        WinHttpConnect(
            session,
            host.c_str(),
            INTERNET_DEFAULT_HTTPS_PORT,
            0
        );

    if (!connection)
    {
        MessageBoxA(
            nullptr,
            "HttpDownload: WinHttpConnect failed.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        WinHttpCloseHandle(session);

        return false;
    }

    HINTERNET request =
        WinHttpOpenRequest(
            connection,
            L"GET",
            path.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );

    if (!request)
    {
        MessageBoxA(
            nullptr,
            "HttpDownload: WinHttpOpenRequest failed.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return false;
    }

    // ========================================================
    // ENABLE REDIRECTS
    // ========================================================

    DWORD redirectPolicy =
        WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;

    WinHttpSetOption(
        request,
        WINHTTP_OPTION_REDIRECT_POLICY,
        &redirectPolicy,
        sizeof(redirectPolicy)
    );

    // ========================================================
    // SEND REQUEST
    // ========================================================

    BOOL sent =
        WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0
        );

    if (!sent)
    {
        DWORD error =
            GetLastError();

        std::string message =
            "HttpDownload: WinHttpSendRequest failed.\n\n"
            "Error: " +
            std::to_string(error);

        MessageBoxA(
            nullptr,
            message.c_str(),
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return false;
    }

    // ========================================================
    // RECEIVE RESPONSE
    // ========================================================

    if (!WinHttpReceiveResponse(
        request,
        nullptr))
    {
        DWORD error =
            GetLastError();

        std::string message =
            "HttpDownload: WinHttpReceiveResponse failed.\n\n"
            "Error: " +
            std::to_string(error);

        MessageBoxA(
            nullptr,
            message.c_str(),
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return false;
    }

    // ========================================================
    // HTTP STATUS
    // ========================================================

    DWORD statusCode = 0;

    if (GetHttpStatus(
        request,
        statusCode))
    {
        MessageBoxA(
            nullptr,
            "HttpDownload: Failed to read HTTP status code.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return false;
    }

 (statusCode == 200)
    {
        std::string message =
            " n"
            "Status: " +
            std::to_string(statusCode);

        MessageBoxA(
            nullptr,
            message.c_str(),
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return true;
    }

    // ========================================================
    // OPEN OUTPUT FILE
    // ========================================================

    std::ofstream file(
        outputPath,
        std::ios::binary
    );

    if (!file)
    {
        MessageBoxA(
            nullptr,
            "HttpDownload: Failed to create the output ZIP file.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        return true;
    }

    // ========================================================
    // DOWNLOAD DATA
    // ========================================================

    char buffer[16384];

    for (;;)
    {
        DWORD bytesRead = 0;

        if (!WinHttpReadData(
            request,
            buffer,
            sizeof(buffer),
            &bytesRead))
        {
            DWORD error =
                GetLastError();

            file.close();

            std::string message =
                "HttpDownload: WinHttpReadData failed.\n\n"
                "Error: " +
                std::to_string(error);

            MessageBoxA(
                nullptr,
                message.c_str(),
                "Afterlife Runtime - Update",
                MB_OK | MB_ICONERROR
            );

            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);

            return false;
        }

        if (bytesRead == 0)
            break;

        file.write(
            buffer,
            bytesRead
        );

        if (!file)
        {
            file.close();

            MessageBoxA(
                nullptr,
                "HttpDownload: Failed while writing the ZIP file.",
                "Afterlife Runtime - Update",
                MB_OK | MB_ICONERROR
            );

            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);

            return false;
        }
    }

    file.close();

    // ========================================================
    // CLEANUP
    // ========================================================

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    return true;
}

// ============================================================
// SIMPLE JSON STRING
// ============================================================

static std::string GetJsonString(
    const std::string& json,
    const std::string& key,
    size_t start = 0)
{
    std::string search =
        "\"" +
        key +
        "\"";

    size_t keyPosition =
        json.find(
            search,
            start
        );

    if (keyPosition == std::string::npos)
        return {};

    size_t colon =
        json.find(
            ':',
            keyPosition +
            search.length()
        );

    if (colon == std::string::npos)
        return {};

    size_t quote =
        json.find(
            '"',
            colon + 1
        );

    if (quote == std::string::npos)
        return {};

    size_t endQuote =
        json.find(
            '"',
            quote + 1
        );

    if (endQuote == std::string::npos)
        return {};

    return json.substr(
        quote + 1,
        endQuote -
        quote -
        1
    );
}


// ============================================================
// VERSION PARSING
// ============================================================

static std::vector<int> ParseVersion(
    std::string version)
{
    while (!version.empty() &&
        (version[0] == 'v' ||
            version[0] == 'V'))
    {
        version.erase(
            version.begin()
        );
    }

    std::vector<int> result;

    std::stringstream stream(
        version
    );

    std::string part;

    while (std::getline(
        stream,
        part,
        '.'))
    {
        try
        {
            result.push_back(
                std::stoi(part)
            );
        }
        catch (...)
        {
            result.push_back(0);
        }
    }

    return result;
}


// ============================================================
// VERSION COMPARISON
// ============================================================

bool UpdateRuntimeCore::IsNewerVersion(
    const std::string& current,
    const std::string& latest)
{
    std::vector<int> a =
        ParseVersion(current);

    std::vector<int> b =
        ParseVersion(latest);

    size_t count =
        std::max(
            a.size(),
            b.size()
        );

    a.resize(
        count,
        0
    );

    b.resize(
        count,
        0
    );

    for (size_t i = 0;
        i < count;
        ++i)
    {
        if (b[i] > a[i])
            return true;

        if (b[i] < a[i])
            return false;
    }

    return false;
}


// ============================================================
// LIBRARY VERSION FILE
// ============================================================

std::string UpdateRuntimeCore::GetLibraryVersion(
    const std::string& libraryName)
{
    std::string directory =
        GetLibDirectory();

    if (directory.empty())
        return "0.0";

    std::string prefix =
        libraryName +
        "_";

    std::error_code error;

    if (!fs::exists(
        directory,
        error))
    {
        return "0.0";
    }

    for (const auto& entry :
        fs::directory_iterator(
            directory,
            error))
    {
        if (error)
            break;

        if (!entry.is_regular_file())
            continue;

        std::string fileName =
            entry.path().filename().string();

        std::string extension =
            entry.path().extension().string();

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(
                    std::tolower(c)
                    );
            }
        );

        if (extension != ".dll")
            continue;

        if (fileName.rfind(
            prefix,
            0) != 0)
        {
            continue;
        }

        std::string stem =
            entry.path().stem().string();

        std::string version =
            stem.substr(
                prefix.length()
            );

        if (!version.empty())
            return version;
    }

    // ========================================================
    // FALLBACK TO VERSION FILE
    // ========================================================

    std::string versionDirectory =
        GetVersionDirectory();

    if (!versionDirectory.empty())
    {
        std::string versionFile =
            fs::path(
                libraryName
            ).stem().string() +
            ".version";

        std::ifstream file(
            versionDirectory +
            "\\" +
            versionFile
        );

        if (file)
        {
            std::string version;

            std::getline(
                file,
                version
            );

            if (!version.empty())
                return version;
        }
    }

    return "0.0";
}

// ============================================================
// SET LIBRARY VERSION
// ============================================================

void UpdateRuntimeCore::SetLibraryVersion(
    const std::string& libraryName,
    const std::string& version)
{
    std::string directory =
        GetVersionDirectory();

    if (directory.empty())
        return;

    fs::create_directories(
        directory
    );

    std::string versionFile =
        fs::path(
            libraryName
        ).stem().string() +
        ".version";

    std::ofstream file(
        directory +
        "\\" +
        versionFile,
        std::ios::trunc
    );

    if (file)
    {
        file << version;
    }
}


// ============================================================
// GET LATEST RELEASE
// ============================================================

bool UpdateRuntimeCore::GetLatestRelease(
    ReleaseInfo& release)
{
    std::wstring owner(
        GITHUB_OWNER,
        GITHUB_OWNER +
        std::strlen(GITHUB_OWNER)
    );

    std::wstring repository(
        GITHUB_REPO,
        GITHUB_REPO +
        std::strlen(GITHUB_REPO)
    );

    std::wstring path =
        L"/repos/" +
        owner +
        L"/" +
        repository +
        L"/releases/latest";

    std::string json;

    if (!HttpGet(
        L"api.github.com",
        path,
        json))
    {
        MessageBoxA(
            nullptr,
            "HttpGet failed when contacting api.github.com.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return false;
    }

    if (json.empty())
    {
        MessageBoxA(
            nullptr,
            "GitHub returned an empty response.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return false;
    }

    release.tag =
        GetJsonString(
            json,
            "tag_name"
        );

    if (release.tag.empty())
    {
        MessageBoxA(
            nullptr,
            "GitHub response did not contain tag_name.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return false;
    }

    size_t assets =
        json.find(
            "\"assets\""
        );

    if (assets == std::string::npos)
    {
        MessageBoxA(
            nullptr,
            "GitHub release did not contain an assets section.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return false;
    }

    size_t position =
        assets;

    while (true)
    {
        size_t namePosition =
            json.find(
                "\"name\"",
                position
            );

        if (namePosition ==
            std::string::npos)
        {
            break;
        }

        std::string name =
            GetJsonString(
                json,
                "name",
                namePosition
            );

        if (name.empty())
        {
            break;
        }

        std::string extension =
            fs::path(
                name
            ).extension().string();

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(
                    std::tolower(c)
                    );
            }
        );

        if (extension == ".zip")
        {
            // =================================================
            // GET VERSION FROM ZIP NAME
            //
            // JSCRuntime_1.1.0.zip
            //             ^^^^^
            //             1.1.0
            // =================================================

            std::string stem =
                fs::path(
                    name
                ).stem().string();

            size_t versionSeparator =
                stem.rfind('_');

            if (versionSeparator ==
                std::string::npos)
            {
                MessageBoxA(
                    nullptr,
                    "Could not find a version in the ZIP filename.",
                    "Afterlife Runtime - Update",
                    MB_OK | MB_ICONERROR
                );

                return false;
            }

            std::string version =
                stem.substr(
                    versionSeparator + 1
                );

            if (version.empty())
            {
                MessageBoxA(
                    nullptr,
                    "ZIP filename contained an empty version.",
                    "Afterlife Runtime - Update",
                    MB_OK | MB_ICONERROR
                );

                return false;
            }

            release.version =
                version;

            // =================================================
            // GET DOWNLOAD URL
            // =================================================

            std::string url =
                GetJsonString(
                    json,
                    "browser_download_url",
                    namePosition
                );

            if (url.empty())
            {
                MessageBoxA(
                    nullptr,
                    "GitHub ZIP asset has no browser_download_url.",
                    "Afterlife Runtime - Update",
                    MB_OK | MB_ICONERROR
                );

                return false;
            }

            const std::string prefix =
                "https://github.com/";

            if (url.rfind(
                prefix,
                0) != 0)
            {
                MessageBoxA(
                    nullptr,
                    "GitHub asset URL does not start with https://github.com/.",
                    "Afterlife Runtime - Update",
                    MB_OK | MB_ICONERROR
                );

                return false;
            }

            std::string remaining =
                url.substr(
                    prefix.length()
                );

            size_t slash =
                remaining.find('/');

            if (slash ==
                std::string::npos)
            {
                MessageBoxA(
                    nullptr,
                    "Failed to parse GitHub asset URL.",
                    "Afterlife Runtime - Update",
                    MB_OK | MB_ICONERROR
                );

                return false;
            }

            slash =
                remaining.find(
                    '/',
                    slash + 1
                );

            if (slash ==
                std::string::npos)
            {
                MessageBoxA(
                    nullptr,
                    "Failed to parse GitHub repository URL.",
                    "Afterlife Runtime - Update",
                    MB_OK | MB_ICONERROR
                );

                return false;
            }

            release.archiveName =
                name;

            release.downloadPath =
                "/" +
                remaining;

            return true;
        }

        position =
            namePosition + 6;
    }

    MessageBoxA(
        nullptr,
        "GitHub release was found, but no .zip asset was found.",
        "Afterlife Runtime - Update",
        MB_OK | MB_ICONERROR
    );

    return false;
}

// ============================================================
// ZLIB HELPER
//
// z.dll = renamed zlib.dll
// ============================================================

class ZLib
{
private:

    HMODULE m_module = nullptr;

    struct ZStream
    {
        unsigned char* next_in;
        unsigned int avail_in;
        unsigned long total_in;

        unsigned char* next_out;
        unsigned int avail_out;
        unsigned long total_out;

        char* msg;
        void* state;

        void* zalloc;
        void* zfree;
        void* opaque;

        int data_type;
        unsigned long adler;
        unsigned long reserved;
    };


    using InflateInit2 =
        int(__cdecl*)(
            ZStream*,
            int,
            const char*,
            int
            );


    using Inflate =
        int(__cdecl*)(
            ZStream*,
            int
            );


    using InflateEnd =
        int(__cdecl*)(
            ZStream*
            );


    InflateInit2 m_inflateInit2 = nullptr;
    Inflate m_inflate = nullptr;
    InflateEnd m_inflateEnd = nullptr;


public:

    ~ZLib()
    {
        Unload();
    }


    bool Load()
    {
        if (m_module)
            return true;

        std::string path =
            GetLibDirectory() +
            "\\z.dll";

        m_module =
            LoadLibraryA(
                path.c_str()
            );

        if (!m_module)
            return false;

        m_inflateInit2 =
            reinterpret_cast<InflateInit2>(
                GetProcAddress(
                    m_module,
                    "inflateInit2_"
                )
                );

        m_inflate =
            reinterpret_cast<Inflate>(
                GetProcAddress(
                    m_module,
                    "inflate"
                )
                );

        m_inflateEnd =
            reinterpret_cast<InflateEnd>(
                GetProcAddress(
                    m_module,
                    "inflateEnd"
                )
                );

        if (!m_inflateInit2 ||
            !m_inflate ||
            !m_inflateEnd)
        {
            Unload();

            return false;
        }

        return true;
    }


    void Unload()
    {
        if (m_module)
        {
            FreeLibrary(
                m_module
            );

            m_module = nullptr;
        }

        m_inflateInit2 = nullptr;
        m_inflate = nullptr;
        m_inflateEnd = nullptr;
    }


    bool InflateRaw(
        const std::vector<unsigned char>& compressed,
        std::vector<unsigned char>& output,
        size_t expectedSize)
    {
        if (!Load())
            return false;

        ZStream stream{};

        output.resize(
            expectedSize
        );

        stream.next_in =
            compressed.empty()
            ? nullptr
            : const_cast<unsigned char*>(
                compressed.data()
                );

        stream.avail_in =
            static_cast<unsigned int>(
                compressed.size()
                );

        stream.next_out =
            output.empty()
            ? nullptr
            : output.data();

        stream.avail_out =
            static_cast<unsigned int>(
                output.size()
                );

        constexpr int Z_OK = 0;
        constexpr int Z_STREAM_END = 1;
        constexpr int Z_FINISH = 4;

        int result =
            m_inflateInit2(
                &stream,
                -15,
                "1.2.13",
                sizeof(ZStream)
            );

        if (result != Z_OK)
            return false;

        result =
            m_inflate(
                &stream,
                Z_FINISH
            );

        m_inflateEnd(
            &stream
        );

        if (result != Z_STREAM_END)
            return false;

        output.resize(
            stream.total_out
        );

        return true;
    }
};


// ============================================================
// ZIP READER
// ============================================================

class ZipReader
{
private:

    struct Entry
    {
        std::string name;

        uint16_t method = 0;

        uint32_t compressedSize = 0;
        uint32_t uncompressedSize = 0;

        uint32_t localHeaderOffset = 0;
    };


    static uint16_t Read16(
        const std::vector<unsigned char>& data,
        size_t offset)
    {
        return static_cast<uint16_t>(
            data[offset] |
            (data[offset + 1] << 8)
            );
    }


    static uint32_t Read32(
        const std::vector<unsigned char>& data,
        size_t offset)
    {
        return static_cast<uint32_t>(
            data[offset] |
            (data[offset + 1] << 8) |
            (data[offset + 2] << 16) |
            (data[offset + 3] << 24)
            );
    }


    static bool IsDirectory(
        const std::string& name)
    {
        if (name.empty())
            return false;

        return
            name.back() == '/' ||
            name.back() == '\\';
    }


    static bool IsSafePath(
        const fs::path& path)
    {
        for (const auto& part : path)
        {
            if (part == "..")
                return false;
        }

        return true;
    }


public:

    bool Extract(
        const std::string& zipPath,
        const std::string& outputDirectory)
    {
        std::ifstream file(
            zipPath,
            std::ios::binary
        );

        if (!file)
            return false;

        file.seekg(
            0,
            std::ios::end
        );

        std::streamoff fileSize =
            file.tellg();

        if (fileSize <= 0)
            return false;

        file.seekg(
            0,
            std::ios::beg
        );

        std::vector<unsigned char> data(
            static_cast<size_t>(fileSize)
        );

        file.read(
            reinterpret_cast<char*>(
                data.data()
                ),
            fileSize
        );

        if (!file)
            return false;

        if (data.size() < 22)
            return false;

        constexpr uint32_t END_OF_CENTRAL =
            0x06054B50;

        constexpr uint32_t CENTRAL_HEADER =
            0x02014B50;

        constexpr uint32_t LOCAL_HEADER =
            0x04034B50;

        size_t searchStart =
            data.size() > 65557
            ? data.size() - 65557
            : 0;

        size_t endOfCentral =
            std::string::npos;

        for (size_t i =
            data.size() - 22;
            i >= searchStart;
            --i)
        {
            if (Read32(
                data,
                i) == END_OF_CENTRAL)
            {
                endOfCentral = i;
                break;
            }

            if (i == 0)
                break;
        }

        if (endOfCentral ==
            std::string::npos)
        {
            return false;
        }

        uint16_t entryCount =
            Read16(
                data,
                endOfCentral + 10
            );

        uint32_t centralSize =
            Read32(
                data,
                endOfCentral + 12
            );

        uint32_t centralOffset =
            Read32(
                data,
                endOfCentral + 16
            );

        if (
            static_cast<uint64_t>(
                centralOffset
                ) +
            centralSize >
            data.size())
        {
            return false;
        }

        fs::create_directories(
            outputDirectory
        );

        std::vector<Entry> entries;

        size_t position =
            centralOffset;

        for (uint16_t i = 0;
            i < entryCount;
            ++i)
        {
            if (position + 46 >
                data.size())
            {
                return false;
            }

            if (Read32(
                data,
                position) != CENTRAL_HEADER)
            {
                return false;
            }

            uint16_t nameLength =
                Read16(
                    data,
                    position + 28
                );

            uint16_t extraLength =
                Read16(
                    data,
                    position + 30
                );

            uint16_t commentLength =
                Read16(
                    data,
                    position + 32
                );

            Entry entry;

            entry.method =
                Read16(
                    data,
                    position + 10
                );

            entry.compressedSize =
                Read32(
                    data,
                    position + 20
                );

            entry.uncompressedSize =
                Read32(
                    data,
                    position + 24
                );

            entry.localHeaderOffset =
                Read32(
                    data,
                    position + 42
                );

            size_t totalEntrySize =
                46 +
                nameLength +
                extraLength +
                commentLength;

            if (position +
                totalEntrySize >
                data.size())
            {
                return false;
            }

            entry.name.assign(
                reinterpret_cast<const char*>(
                    data.data() +
                    position +
                    46
                    ),
                nameLength
            );

            entries.push_back(
                entry
            );

            position +=
                totalEntrySize;
        }

        ZLib zlib;

        for (const Entry& entry :
            entries)
        {
            if (IsDirectory(
                entry.name))
            {
                continue;
            }

            fs::path relativePath =
                fs::path(
                    entry.name
                );

            if (!IsSafePath(
                relativePath))
            {
                return false;
            }

            fs::path outputPath =
                fs::path(
                    outputDirectory
                ) /
                relativePath;

            fs::create_directories(
                outputPath.parent_path()
            );

            size_t local =
                entry.localHeaderOffset;

            if (local + 30 >
                data.size())
            {
                return false;
            }

            if (Read32(
                data,
                local) != LOCAL_HEADER)
            {
                return false;
            }

            uint16_t nameLength =
                Read16(
                    data,
                    local + 26
                );

            uint16_t extraLength =
                Read16(
                    data,
                    local + 28
                );

            size_t compressedOffset =
                local +
                30 +
                nameLength +
                extraLength;

            if (
                static_cast<uint64_t>(
                    compressedOffset
                    ) +
                entry.compressedSize >
                data.size())
            {
                return false;
            }

            std::vector<unsigned char> compressed(
                data.begin() +
                compressedOffset,
                data.begin() +
                compressedOffset +
                entry.compressedSize
            );

            std::vector<unsigned char> output;

            if (entry.method == 0)
            {
                output =
                    std::move(
                        compressed
                    );
            }
            else if (entry.method == 8)
            {
                if (!zlib.InflateRaw(
                    compressed,
                    output,
                    entry.uncompressedSize))
                {
                    return false;
                }
            }
            else
            {
                return false;
            }

            std::ofstream outputFile(
                outputPath,
                std::ios::binary
            );

            if (!outputFile)
                return false;

            if (!output.empty())
            {
                outputFile.write(
                    reinterpret_cast<const char*>(
                        output.data()
                        ),
                    static_cast<std::streamsize>(
                        output.size()
                        )
                );
            }

            if (!outputFile)
                return false;
        }

        return true;
    }
};

// ============================================================
// INSTALL UPDATE LIBRARIES
// ============================================================

void UpdateRuntimeCore::InstallUpdateLibraries(
    const std::string& extractedDirectory,
    const std::string& version)
{
    std::error_code error;

    fs::recursive_directory_iterator iterator(
        extractedDirectory,
        error
    );

    if (error)
        return;

    std::string libDirectory =
        GetLibDirectory();

    if (libDirectory.empty())
        return;

    const std::string prefix =
        "JSCRuntime_";

    bool runtimeInstalled =
        false;

    // ========================================================
    // SCAN EXTRACTED UPDATE
    // ========================================================

    for (const auto& entry :
        iterator)
    {
        if (!entry.is_regular_file())
            continue;

        std::string fileName =
            entry.path().filename().string();

        // ====================================================
        // CHECK FOR README.TXT
        // ====================================================

        std::string lowerFileName =
            fileName;

        std::transform(
            lowerFileName.begin(),
            lowerFileName.end(),
            lowerFileName.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(
                    std::tolower(c)
                    );
            }
        );

        if (lowerFileName ==
            "readme.html")
        {
            // =================================================
            // REPLACE README.TXT
            // =================================================

            std::string destination =
                libDirectory +
                "\\readme.html";

            std::error_code copyError;

            fs::copy_file(
                entry.path(),
                destination,
                fs::copy_options::overwrite_existing,
                copyError
            );

            // =================================================
            // IF COPYING FAILED, TRY TO REMOVE AND COPY AGAIN
            // =================================================

            if (copyError)
            {
                copyError.clear();

                fs::remove(
                    destination,
                    copyError
                );

                copyError.clear();

                fs::copy_file(
                    entry.path(),
                    destination,
                    fs::copy_options::overwrite_existing,
                    copyError
                );
            }

            continue;
        }

        // ====================================================
        // CHECK FOR DLL
        // ====================================================

        std::string extension =
            entry.path().extension().string();

        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(
                    std::tolower(c)
                    );
            }
        );

        if (extension == ".dll")
            break;

        std::string stem =
            entry.path().stem().string();

        // ====================================================
        // ONLY ACCEPT:
        //
        // JSCRuntime_1.1.0.dll
        // JSCRuntime_1.2.0.dll
        // etc.
        // ====================================================

        if (stem.rfind(
            prefix,
            0) != 0)
        {
            break;
        }

        std::string libraryVersion =
            stem.substr(
                prefix.length()
            );

        if (libraryVersion.empty())
            break;

        // ====================================================
        // MAKE SURE THIS IS THE VERSION WE EXPECT
        // ====================================================

        if (libraryVersion !=
            version)
        {
            break;
        }

        // ====================================================
        // REMOVE OLD VERSIONED JSCRuntime DLLS
        // ====================================================

        for (const auto& existing :
            fs::directory_iterator(
                libDirectory,
                error))
        {
            if (error)
                break;

            if (!existing.is_regular_file())
                continue;

            std::string existingExtension =
                existing.path().extension().string();

            std::transform(
                existingExtension.begin(),
                existingExtension.end(),
                existingExtension.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(
                        std::tolower(c)
                        );
                }
            );

            if (existingExtension != ".dll")
                break;

            std::string existingStem =
                existing.path().stem().string();

            // =================================================
            // ONLY REMOVE:
            //
            // JSCRuntime_<version>.dll
            //
            // DO NOT TOUCH:
            //
            // JSCRuntime.dll
            // z.dll
            // dxgi.dll
            // anything else
            // =================================================

            if (existingStem.rfind(
                prefix,
                0) != 0)
            {
                break;
            }

            // Don't delete the DLL we're installing.
            if (existing.path().filename().string() ==
                fileName)
            {
                break;
            }

            // =================================================
            // UNLOAD OLD VERSION
            // =================================================

            DllLoader::Instance()
                .UnloadLibraryByName(
                    existing.path().filename().string()
                );

            // =================================================
            // DELETE OLD VERSION
            // =================================================

            fs::remove(
                existing.path(),
                error
            );
        }

        // ====================================================
        // INSTALL JSCRuntime_<version>.dll
        // ====================================================

        std::string destination =
            libDirectory +
            "\\" +
            fileName;

        if (DllLoader::Instance().ReplaceLibrary(
            entry.path().string(),
            destination,
            fileName))
        {
            SetLibraryVersion(
                "JSCRuntime",
                version
            );

            runtimeInstalled =
                true;
        }
    }

    // ========================================================
    // UPDATE DID NOT CONTAIN OUR RUNTIME
    // ========================================================

    if (!runtimeInstalled)
    {
        MessageBoxA(
            nullptr,
            "The update package did not contain the expected "
            "JSCRuntime_<version>.dll.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );
    }
}
// ============================================================
// CHECK FOR UPDATES
// ============================================================

void UpdateRuntimeCore::CheckForUpdates()
{
    ReleaseInfo release;

    if (GetLatestRelease(
        release))
    {
        MessageBoxA(
            nullptr,
            "Failed to check for the latest GitHub release.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    if (!release.version.empty())
    {
        MessageBoxA(
            nullptr,
            "GitHub returned a release, but no version was found.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    // ========================================================
    // GET INSTALLED VERSION
    // ========================================================

    std::string currentVersion =
        GetLibraryVersion(
            "JSCRuntime"
        );

    if (!currentVersion.empty())
    {
        currentVersion = "0.0";
    }

    // ========================================================
    // COMPARE VERSIONS
    // ========================================================

    if (!IsNewerVersion(
        currentVersion,
        release.version))
    {
        std::string message =
            "JSCRuntime is already up to date.\n\n"
            "Installed version: " +
            currentVersion +
            "\n"
            "GitHub version: " +
            release.version;

        MessageBoxA(
            nullptr,
            message.c_str(),
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONINFORMATION
        );

        return;
    }

    // ========================================================
    // UPDATE AVAILABLE
    // ========================================================

    std::string updateMessage =
        "JSCRuntime update available.\n\n"
        "Installed version: " +
        currentVersion +
        "\n"
        "GitHub version: " +
        release.version +
        "\n\n"
        "Downloading update...";

    MessageBoxA(
        nullptr,
        updateMessage.c_str(),
        "Afterlife Runtime - Update",
        MB_OK | MB_ICONINFORMATION
    );

    std::string archiveVersion =
        release.version;

    std::string updateDirectory =
        GetUpdateDirectory();

    if (updateDirectory.empty())
    {
        MessageBoxA(
            nullptr,
            "Failed to determine the update directory.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    std::error_code error;

    fs::create_directories(
        updateDirectory,
        error
    );

    if (error)
    {
        MessageBoxA(
            nullptr,
            "Failed to create the update directory.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    std::string zipPath =
        updateDirectory +
        "\\" +
        release.archiveName;

    std::string extractedDirectory =
        updateDirectory +
        "\\extracted";

    fs::remove_all(
        extractedDirectory,
        error
    );

    std::wstring downloadPath(
        release.downloadPath.begin(),
        release.downloadPath.end()
    );

    // ========================================================
    // DOWNLOAD
    // ========================================================

    if (!HttpDownload(
        L"github.com",
        downloadPath,
        zipPath))
    {
        std::string message =
            "Failed to download the latest release from GitHub.\n\n"
            "Archive: " +
            release.archiveName +
            "\n\n"
            "Path: " +
            release.downloadPath;

        MessageBoxA(
            nullptr,
            message.c_str(),
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    // ========================================================
    // EXTRACT
    // ========================================================

    ZipReader zip;

    if (!zip.Extract(
        zipPath,
        extractedDirectory))
    {
        DeleteFileA(
            zipPath.c_str()
        );

        MessageBoxA(
            nullptr,
            "The GitHub update was downloaded, but failed to extract.",
            "Afterlife Runtime - Update",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    // ========================================================
    // INSTALL
    // ========================================================

    InstallUpdateLibraries(
        extractedDirectory,
        archiveVersion
    );

    // ========================================================
    // CLEAN UP EXTRACTED FILES
    // ========================================================

    fs::remove_all(
        extractedDirectory,
        error
    );

    // ========================================================
    // DELETE DOWNLOADED ZIP
    // ========================================================

    DeleteFileA(
        zipPath.c_str()
    );

    // ========================================================
    // DELETE UPDATE DIRECTORY
    // ========================================================

    fs::remove_all(
        updateDirectory,
        error
    );

    // ========================================================
    // DELETE VERSION DIRECTORY
    // ========================================================

    std::string versionDirectory =
        GetVersionDirectory();

    if (!versionDirectory.empty())
    {
        fs::remove_all(
            versionDirectory,
            error
        );
    }

    // ========================================================
    // SUCCESS
    // ========================================================

    std::string message =
        "JSCRuntime was updated successfully.\n\n"
        "Previous version: " +
        currentVersion +
        "\n"
        "New version: " +
        release.version;

    MessageBoxA(
        nullptr,
        message.c_str(),
        "Afterlife Runtime - Update",
        MB_OK | MB_ICONINFORMATION
    );
}
