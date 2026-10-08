#include "overlay.h"
#include "menuStructure.h"

#include <Windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wincodec.h>
#include <vector>
#include <iostream>
#include <cstring>
#include <winhttp.h>
#include <string>
#include <fstream>
#include <limits>
#include <d2d1.h>
#include <dwrite.h>
#include <filesystem>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "winhttp.lib")

namespace
{
    HWND g_gameWindow = nullptr;
    HWND g_overlayWindow = nullptr;

    // ========================================================
    // DX11
    // ========================================================

    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;

    // DirectComposition uses IDXGISwapChain1
    IDXGISwapChain1* g_swapChain = nullptr;

    ID3D11RenderTargetView* g_renderTarget = nullptr;
    ID3D11ShaderResourceView* g_testTexture = nullptr;

    ID3D11SamplerState* g_testSampler = nullptr;

    ID3D11Texture2D* g_testTextureResource = nullptr;

    struct GifFrame
    {
        std::vector<BYTE> pixels;
        UINT width = 0;
        UINT height = 0;
        UINT delayMs = 100;
    };

    std::vector<GifFrame> g_gifFrames;

    size_t g_gifCurrentFrame = 0;
    ULONGLONG g_gifLastFrameTime = 0;

    UINT g_gifWidth = 0;
    UINT g_gifHeightAnim = 0;
    // ========================================================
    // SHADERS
    // ========================================================

    ID3D11VertexShader* g_vertexShader = nullptr;
    ID3D11PixelShader* g_pixelShader = nullptr;

    ID3D11InputLayout* g_inputLayout = nullptr;

    ID3D11Buffer* g_vertexBuffer = nullptr;

    // ========================================================
    // DIRECT COMPOSITION
    // ========================================================

    IDCompositionDevice* g_dcompDevice = nullptr;
    IDCompositionTarget* g_dcompTarget = nullptr;
    IDCompositionVisual* g_dcompVisual = nullptr;

    // ========================================================
    // STATE
    // ========================================================

    bool g_visible = false;

    const char* WINDOW_CLASS =
        "KH1_External_DX11_Overlay";

    struct Vertex
    {
        float x;
        float y;

        float r;
        float g;
        float b;
        float a;

        float u;
        float v;
    };
}


// ============================================================
// FIND KH1 WINDOW
// ============================================================

bool FindGameWindow()
{
    DWORD currentPid =
        GetCurrentProcessId();

    std::cout
        << "========================================"
        << std::endl;

    std::cout
        << "[FIND KH1 WINDOW]"
        << std::endl;

    std::cout
        << "[*] Current PID: "
        << currentPid
        << std::endl;

    std::cout
        << "========================================"
        << std::endl;

    struct EnumData
    {
        DWORD processId;
        HWND foundWindow;
    };

    // ============================================================
    // WAIT FOR GAME WINDOW
    // ============================================================

    constexpr int MAX_ATTEMPTS = 100;

    for (int attempt = 0;
        attempt < MAX_ATTEMPTS;
        attempt++)
    {
        EnumData data{};

        data.processId =
            currentPid;

        data.foundWindow =
            nullptr;

        EnumWindows(
            [](HWND hWnd, LPARAM lParam) -> BOOL
            {
                EnumData* data =
                    reinterpret_cast<EnumData*>(lParam);

                DWORD windowPid = 0;

                GetWindowThreadProcessId(
                    hWnd,
                    &windowPid
                );

                if (windowPid != data->processId)
                {
                    return TRUE;
                }

                char title[512]{};

                GetWindowTextA(
                    hWnd,
                    title,
                    sizeof(title)
                );

                if (_stricmp(
                    title,
                    "KINGDOM HEARTS - HD 1.5+2.5 Remix -") != 0)
                {
                    return TRUE;
                }

                data->foundWindow =
                    hWnd;

                return FALSE;
            },
            reinterpret_cast<LPARAM>(&data)
        );

        if (data.foundWindow)
        {
            g_gameWindow =
                data.foundWindow;

            break;
        }

        // ========================================================
        // WINDOW DOESN'T EXIST YET
        // ========================================================

        Sleep(100);
    }

    // ============================================================
    // FINAL CHECK
    // ============================================================

    if (!g_gameWindow)
    {
        std::cout
            << "[-] KH1 game window not found."
            << std::endl;

        return false;
    }

    // ============================================================
    // VERIFY PID
    // ============================================================

    DWORD verifiedPid = 0;

    GetWindowThreadProcessId(
        g_gameWindow,
        &verifiedPid
    );

    if (verifiedPid != currentPid)
    {
        g_gameWindow =
            nullptr;

        std::cout
            << "[-] Window PID verification failed."
            << std::endl;

        return false;
    }

    // ============================================================
    // GET WINDOW INFORMATION
    // ============================================================

    char className[512]{};
    char title[512]{};

    GetClassNameA(
        g_gameWindow,
        className,
        sizeof(className)
    );

    GetWindowTextA(
        g_gameWindow,
        title,
        sizeof(title)
    );

    RECT rect{};

    GetWindowRect(
        g_gameWindow,
        &rect
    );

    // ============================================================
    // OUTPUT
    // ============================================================

    std::cout
        << "[+] KH1 WINDOW FOUND"
        << std::endl;

    std::cout
        << "[+] HWND: "
        << g_gameWindow
        << std::endl;

    std::cout
        << "[+] PID: "
        << verifiedPid
        << std::endl;

    std::cout
        << "[+] Class: "
        << className
        << std::endl;

    std::cout
        << "[+] Title: "
        << title
        << std::endl;

    std::cout
        << "[+] Size: "
        << (rect.right - rect.left)
        << " x "
        << (rect.bottom - rect.top)
        << std::endl;

    return true;
}


// ============================================================
// CREATE OVERLAY WINDOW
// ============================================================

bool CreateOverlayWindow()
{
    HINSTANCE hInstance =
        GetModuleHandleA(nullptr);

    WNDCLASSEXA wc{};

    wc.cbSize =
        sizeof(WNDCLASSEXA);

    wc.style =
        CS_HREDRAW |
        CS_VREDRAW;

    wc.lpfnWndProc =
        [](HWND hWnd,
            UINT msg,
            WPARAM wParam,
            LPARAM lParam) -> LRESULT
        {
            switch (msg)
            {
            case WM_NCHITTEST:
                return HTTRANSPARENT;

            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;

            case WM_SETCURSOR:
                return TRUE;

            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                return 0;

            case WM_ERASEBKGND:
                return 1;

            case WM_ACTIVATE:
                return 0;

            case WM_SETFOCUS:
                return 0;

            case WM_KILLFOCUS:
                return 0;

            case WM_DESTROY:
                return 0;
            }

            return DefWindowProcA(
                hWnd,
                msg,
                wParam,
                lParam
            );
        };

    wc.lpszClassName =
        WINDOW_CLASS;

    if (!RegisterClassExA(&wc))
    {
        DWORD error =
            GetLastError();

        if (error != ERROR_CLASS_ALREADY_EXISTS)
        {
            std::cout
                << "[-] RegisterClassEx failed: "
                << error
                << std::endl;

            return false;
        }
    }

    RECT rect{};

    if (!GetWindowRect(
        g_gameWindow,
        &rect))
    {
        std::cout
            << "[-] GetWindowRect failed."
            << std::endl;

        return false;
    }

    int width =
        rect.right -
        rect.left;

    int height =
        rect.bottom -
        rect.top;

    // ========================================================
    // DCOMP WINDOW
    //
    // KH1 is the owner of the overlay.
    //
    // DO NOT ADD:
    //
    //     WS_EX_TRANSPARENT
    //     WS_EX_LAYERED
    //
    // ========================================================

    g_overlayWindow =
        CreateWindowExA(
            WS_EX_NOACTIVATE |
            WS_EX_TOOLWINDOW,

            WINDOW_CLASS,

            "KH1 External DX11 Overlay",

            WS_POPUP,

            rect.left,
            rect.top,

            width,
            height,

            g_gameWindow,
            nullptr,

            hInstance,
            nullptr
        );

    if (!g_overlayWindow)
    {
        std::cout
            << "[-] CreateWindowEx failed: "
            << GetLastError()
            << std::endl;

        return false;
    }

    // ========================================================
    // MAKE SURE THE OVERLAY IS NOT TOPMOST
    //
    // This is done once when the HWND is created.
    // ========================================================

    if (!SetWindowPos(
        g_overlayWindow,
        HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE |
        SWP_NOSIZE |
        SWP_NOACTIVATE))
    {
        std::cout
            << "[-] SetWindowPos(HWND_NOTOPMOST) failed: "
            << GetLastError()
            << std::endl;
    }

    // ========================================================
    // SHOW WITHOUT ACTIVATING
    // ========================================================

    ShowWindow(
        g_overlayWindow,
        SW_SHOWNOACTIVATE
    );

    UpdateWindow(
        g_overlayWindow
    );

    // ========================================================
    // DO NOT CALL:
    //
    // SetForegroundWindow()
    // SetFocus()
    //
    // The overlay remains non-activating.
    // ========================================================

    std::cout
        << "[+] Overlay HWND created"
        << std::endl;

    std::cout
        << "[+] Overlay HWND: "
        << g_overlayWindow
        << std::endl;

    std::cout
        << "[+] Overlay owner: "
        << g_gameWindow
        << std::endl;

    return true;
}

// ============================================================
// CREATE RENDER TARGET
// ============================================================

bool CreateRenderTarget()
{
    ID3D11Texture2D* backBuffer =
        nullptr;

    HRESULT hr =
        g_swapChain->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(
                &backBuffer
                )
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] SwapChain GetBuffer failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    hr =
        g_device->CreateRenderTargetView(
            backBuffer,
            nullptr,
            &g_renderTarget
        );

    backBuffer->Release();

    if (FAILED(hr))
    {
        std::cout
            << "[-] CreateRenderTargetView failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    return true;
}


// ============================================================
// CREATE DIRECTCOMPOSITION
// ============================================================

bool CreateDirectComposition()
{
    if (!g_device ||
        !g_swapChain ||
        !g_overlayWindow)
    {
        return false;
    }

    // ========================================================
    // GET DXGI DEVICE
    // ========================================================

    IDXGIDevice* dxgiDevice =
        nullptr;

    HRESULT hr =
        g_device->QueryInterface(
            __uuidof(IDXGIDevice),
            reinterpret_cast<void**>(
                &dxgiDevice
                )
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] QueryInterface IDXGIDevice failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    // ========================================================
    // CREATE DCOMPOSITION DEVICE
    // ========================================================

    hr =
        DCompositionCreateDevice(
            dxgiDevice,
            __uuidof(IDCompositionDevice),
            reinterpret_cast<void**>(
                &g_dcompDevice
                )
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] DCompositionCreateDevice failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        dxgiDevice->Release();

        return false;
    }

    // ========================================================
    // CREATE COMPOSITION TARGET
    // ========================================================

    hr =
        g_dcompDevice->CreateTargetForHwnd(
            g_overlayWindow,
            TRUE,
            &g_dcompTarget
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] CreateTargetForHwnd failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        dxgiDevice->Release();

        return false;
    }

    // ========================================================
    // CREATE VISUAL
    // ========================================================

    hr =
        g_dcompDevice->CreateVisual(
            &g_dcompVisual
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] CreateVisual failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        dxgiDevice->Release();

        return false;
    }

    // ========================================================
    // PUT DX11 SWAPCHAIN INTO VISUAL
    // ========================================================

    hr =
        g_dcompVisual->SetContent(
            g_swapChain
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] Visual SetContent failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        dxgiDevice->Release();

        return false;
    }

    // ========================================================
    // PUT VISUAL INTO WINDOW
    // ========================================================

    hr =
        g_dcompTarget->SetRoot(
            g_dcompVisual
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] SetRoot failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        dxgiDevice->Release();

        return false;
    }

    // ========================================================
    // COMMIT
    // ========================================================

    hr =
        g_dcompDevice->Commit();

    dxgiDevice->Release();

    if (FAILED(hr))
    {
        std::cout
            << "[-] DirectComposition Commit failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    std::cout
        << "[+] DirectComposition initialized"
        << std::endl;

    return true;
}


// ============================================================
// CREATE DX11
// ============================================================

bool CreateDX11()
{
    // ========================================================
    // OVERLAY SIZE
    //
    // The DX11 swapchain matches the overlay HWND.
    // ========================================================

    RECT overlayRect{};

    GetWindowRect(
        g_overlayWindow,
        &overlayRect
    );

    const UINT width =
        overlayRect.right - overlayRect.left;

    const UINT height =
        overlayRect.bottom - overlayRect.top;

    // ========================================================
    // CREATE DEVICE
    // ========================================================

    D3D_FEATURE_LEVEL featureLevel{};

    const D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr =
        D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,

            D3D11_CREATE_DEVICE_BGRA_SUPPORT,

            levels,
            ARRAYSIZE(levels),

            D3D11_SDK_VERSION,

            &g_device,
            &featureLevel,
            &g_context
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] D3D11CreateDevice failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    std::cout
        << "[+] External DX11 device created"
        << std::endl;

    std::cout
        << "[+] Feature level: 0x"
        << std::hex
        << featureLevel
        << std::dec
        << std::endl;

    // ========================================================
    // GET DXGI DEVICE
    // ========================================================

    IDXGIDevice* dxgiDevice =
        nullptr;

    hr =
        g_device->QueryInterface(
            __uuidof(IDXGIDevice),
            reinterpret_cast<void**>(
                &dxgiDevice
                )
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] QueryInterface IDXGIDevice failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    // ========================================================
    // GET ADAPTER
    // ========================================================

    IDXGIAdapter* adapter =
        nullptr;

    hr =
        dxgiDevice->GetAdapter(
            &adapter
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] GetAdapter failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        dxgiDevice->Release();

        return false;
    }

    // ========================================================
    // GET FACTORY 2
    // ========================================================

    IDXGIFactory2* factory =
        nullptr;

    hr =
        adapter->GetParent(
            __uuidof(IDXGIFactory2),
            reinterpret_cast<void**>(
                &factory
                )
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] GetParent IDXGIFactory2 failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        adapter->Release();
        dxgiDevice->Release();

        return false;
    }

    // ========================================================
    // COMPOSITION SWAPCHAIN
    //
    // The swapchain is now ONLY 300x200.
    //
    // It no longer covers the entire KH1 window.
    // ========================================================

    DXGI_SWAP_CHAIN_DESC1 swapDesc{};

    swapDesc.Width =
        width;

    swapDesc.Height =
        height;

    swapDesc.Format =
        DXGI_FORMAT_B8G8R8A8_UNORM;

    swapDesc.Stereo =
        FALSE;

    swapDesc.SampleDesc.Count =
        1;

    swapDesc.SampleDesc.Quality =
        0;

    swapDesc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT;

    swapDesc.BufferCount =
        2;

    swapDesc.Scaling =
        DXGI_SCALING_STRETCH;

    swapDesc.SwapEffect =
        DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    swapDesc.AlphaMode =
        DXGI_ALPHA_MODE_PREMULTIPLIED;

    swapDesc.Flags =
        0;

    hr =
        factory->CreateSwapChainForComposition(
            g_device,
            &swapDesc,
            nullptr,
            &g_swapChain
        );

    factory->Release();
    adapter->Release();
    dxgiDevice->Release();

    if (FAILED(hr))
    {
        std::cout
            << "[-] CreateSwapChainForComposition failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    std::cout
        << "[+] Transparent DX11 composition swapchain created"
        << std::endl;

    std::cout
        << "[+] Swapchain size: "
        << width
        << "x"
        << height
        << std::endl;

    // ========================================================
    // CREATE RENDER TARGET
    // ========================================================

    if (!CreateRenderTarget())
    {
        return false;
    }

    std::cout
        << "[+] Render target created"
        << std::endl;

    // ========================================================
    // DIRECTCOMPOSITION
    // ========================================================

    if (!CreateDirectComposition())
    {
        return false;
    }

    return true;
}


// ============================================================
// CREATE SIMPLE SHADERS
// ============================================================

bool CreateShaders()
{
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    const char* vertexShader =
        R"(
        struct VS_IN
        {
            float2 pos   : POSITION;
            float4 color : COLOR;
            float2 uv    : TEXCOORD;
        };

        struct VS_OUT
        {
            float4 pos   : SV_POSITION;
            float4 color : COLOR;
            float2 uv    : TEXCOORD;
        };

        VS_OUT main(VS_IN input)
        {
            VS_OUT output;

            output.pos = float4(
                input.pos.x,
                input.pos.y,
                0.0f,
                1.0f
            );

            output.color = input.color;
            output.uv = input.uv;

            return output;
        }
        )";

    const char* pixelShader =
        R"(
    Texture2D tex : register(t0);
    SamplerState samp : register(s0);

    struct PS_IN
    {
        float4 pos : SV_POSITION;
        float4 color : COLOR;
        float2 uv : TEXCOORD;
    };

    float4 main(PS_IN input) : SV_TARGET
    {
        float4 pixel = tex.Sample(samp, input.uv);

        pixel.a *= input.color.a;

        return pixel;
    }
    )";

    typedef HRESULT(WINAPI* D3DCompileFunc)(
        LPCVOID,
        SIZE_T,
        LPCSTR,
        const D3D_SHADER_MACRO*,
        ID3DInclude*,
        LPCSTR,
        LPCSTR,
        UINT,
        UINT,
        ID3DBlob**,
        ID3DBlob**
        );

    HMODULE d3dCompiler =
        LoadLibraryA("d3dcompiler_47.dll");

    if (!d3dCompiler)
    {
        std::cout
            << "[-] d3dcompiler_47.dll not found"
            << std::endl;

        return false;
    }

    D3DCompileFunc D3DCompile =
        reinterpret_cast<D3DCompileFunc>(
            GetProcAddress(
                d3dCompiler,
                "D3DCompile"
            )
            );

    if (!D3DCompile)
    {
        FreeLibrary(d3dCompiler);
        return false;
    }

    // ========================================================
    // VERTEX SHADER
    // ========================================================

    HRESULT hr =
        D3DCompile(
            vertexShader,
            strlen(vertexShader),
            nullptr,
            nullptr,
            nullptr,
            "main",
            "vs_4_0",
            0,
            0,
            &vsBlob,
            &errorBlob
        );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::cout
                << "[-] Vertex shader compile error:"
                << std::endl
                << static_cast<const char*>(
                    errorBlob->GetBufferPointer()
                    )
                << std::endl;

            errorBlob->Release();
        }

        FreeLibrary(d3dCompiler);
        return false;
    }

    if (errorBlob)
    {
        errorBlob->Release();
        errorBlob = nullptr;
    }

    // ========================================================
    // PIXEL SHADER
    // ========================================================

    hr =
        D3DCompile(
            pixelShader,
            strlen(pixelShader),
            nullptr,
            nullptr,
            nullptr,
            "main",
            "ps_4_0",
            0,
            0,
            &psBlob,
            &errorBlob
        );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::cout
                << "[-] Pixel shader compile error:"
                << std::endl
                << static_cast<const char*>(
                    errorBlob->GetBufferPointer()
                    )
                << std::endl;

            errorBlob->Release();
        }

        vsBlob->Release();
        FreeLibrary(d3dCompiler);

        return false;
    }

    if (errorBlob)
    {
        errorBlob->Release();
        errorBlob = nullptr;
    }

    // ========================================================
    // CREATE VERTEX SHADER
    // ========================================================

    hr =
        g_device->CreateVertexShader(
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            nullptr,
            &g_vertexShader
        );

    if (FAILED(hr))
    {
        vsBlob->Release();
        psBlob->Release();
        FreeLibrary(d3dCompiler);

        return false;
    }

    // ========================================================
    // CREATE PIXEL SHADER
    // ========================================================

    hr =
        g_device->CreatePixelShader(
            psBlob->GetBufferPointer(),
            psBlob->GetBufferSize(),
            nullptr,
            &g_pixelShader
        );

    if (FAILED(hr))
    {
        vsBlob->Release();
        psBlob->Release();
        FreeLibrary(d3dCompiler);

        return false;
    }

    // ========================================================
    // INPUT LAYOUT
    // ========================================================

    D3D11_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            0,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },

        {
            "COLOR",
            0,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            0,
            8,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },

        {
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            24,
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        }
    };

    hr =
        g_device->CreateInputLayout(
            inputLayout,
            ARRAYSIZE(inputLayout),
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            &g_inputLayout
        );

    vsBlob->Release();
    psBlob->Release();

    FreeLibrary(d3dCompiler);

    if (FAILED(hr))
    {
        std::cout
            << "[-] CreateInputLayout failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    std::cout
        << "[+] DX11 test shaders created"
        << std::endl;

    return true;
}

void UpdateGif()
{
    if (!g_testTextureResource)
        return;

    if (g_gifFrames.size() <= 1)
        return;

    ULONGLONG now = GetTickCount64();

    GifFrame& currentFrame = g_gifFrames[g_gifCurrentFrame];

    if ((now - g_gifLastFrameTime) < currentFrame.delayMs)
        return;

    g_gifCurrentFrame++;

    if (g_gifCurrentFrame >= g_gifFrames.size())
        g_gifCurrentFrame = 0;

    GifFrame& nextFrame = g_gifFrames[g_gifCurrentFrame];

    if (nextFrame.width != g_gifWidth ||
        nextFrame.height != g_gifHeightAnim)
    {
        return;
    }

    g_context->UpdateSubresource(
        g_testTextureResource,
        0,
        nullptr,
        nextFrame.pixels.data(),
        g_gifWidth * 4,
        0
    );

    g_gifLastFrameTime = now;
}
bool DownloadImageFromUrl(
    const std::string& url,
    std::vector<BYTE>& data
)
{
    data.clear();

    // ========================================================
    // BUILD LOCAL IMAGE PATH
    // ========================================================

    std::wstring wideUrl(
        url.begin(),
        url.end()
    );

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);

    wchar_t hostName[512]{};
    wchar_t urlPath[4096]{};
    wchar_t extraInfo[4096]{};

    components.lpszHostName = hostName;
    components.dwHostNameLength = _countof(hostName);

    components.lpszUrlPath = urlPath;
    components.dwUrlPathLength = _countof(urlPath);

    components.lpszExtraInfo = extraInfo;
    components.dwExtraInfoLength = _countof(extraInfo);

    if (!WinHttpCrackUrl(
        wideUrl.c_str(),
        0,
        0,
        &components))
    {
        std::cout
            << "[-] WinHttpCrackUrl failed: "
            << GetLastError()
            << std::endl;

        return false;
    }

    std::wcout
        << L"[IMAGE] Host: "
        << hostName
        << std::endl;

    std::wcout
        << L"[IMAGE] Path: "
        << urlPath
        << std::endl;

    if (extraInfo[0] != L'\0')
    {
        std::wcout
            << L"[IMAGE] Extra: "
            << extraInfo
            << std::endl;
    }

    // ========================================================
    // GET FILENAME FROM URL
    // ========================================================

    std::wstring filename = urlPath;

    size_t slashPos =
        filename.find_last_of(L'/');

    if (slashPos != std::wstring::npos)
    {
        filename =
            filename.substr(slashPos + 1);
    }

    if (filename.empty())
    {
        std::cout
            << "[-] Could not determine image filename"
            << std::endl;

        return false;
    }

    // Remove query string if it somehow ended up in filename.
    size_t queryPos =
        filename.find(L'?');

    if (queryPos != std::wstring::npos)
    {
        filename =
            filename.substr(0, queryPos);
    }

    if (filename.empty())
    {
        std::cout
            << "[-] Image filename is empty"
            << std::endl;

        return false;
    }

    // ========================================================
    // RAW/IMAGES DIRECTORY
    // ========================================================

    std::filesystem::path imageDirectory =
        std::filesystem::current_path() /
        "raw" /
        "images";

    std::error_code fsError;

    std::filesystem::create_directories(
        imageDirectory,
        fsError
    );

    if (fsError)
    {
        std::cout
            << "[-] Failed to create image directory: "
            << fsError.message()
            << std::endl;

        return false;
    }

    std::filesystem::path localImagePath =
        imageDirectory / filename;

    // ========================================================
    // USE LOCAL COPY IF IT ALREADY EXISTS
    // ========================================================

    if (std::filesystem::exists(
        localImagePath,
        fsError) &&
        !fsError &&
        std::filesystem::is_regular_file(
            localImagePath,
            fsError) &&
        !fsError)
    {
        std::ifstream file(
            localImagePath,
            std::ios::binary
        );

        if (file)
        {
            file.seekg(
                0,
                std::ios::end
            );

            std::streamsize fileSize =
                file.tellg();

            file.seekg(
                0,
                std::ios::beg
            );

            if (fileSize > 0)
            {
                data.resize(
                    static_cast<size_t>(fileSize)
                );

                if (file.read(
                    reinterpret_cast<char*>(
                        data.data()
                        ),
                    fileSize))
                {
                    std::cout
                        << "[IMAGE] Using local copy: "
                        << localImagePath.string()
                        << std::endl;

                    std::cout
                        << "[IMAGE] Loaded "
                        << data.size()
                        << " bytes"
                        << std::endl;

                    return true;
                }
            }
        }

        std::cout
            << "[IMAGE] Local copy exists but could not be read. "
            << "Downloading again."
            << std::endl;

        data.clear();
    }

    // ========================================================
    // DOWNLOAD FROM URL
    // ========================================================

    std::cout
        << "[IMAGE] Downloading: "
        << url
        << std::endl;

    HINTERNET hSession =
        WinHttpOpen(
            L"Mozilla/5.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );

    if (!hSession)
    {
        std::cout
            << "[-] WinHttpOpen failed: "
            << GetLastError()
            << std::endl;

        return false;
    }

    WinHttpSetTimeouts(
        hSession,
        10000,
        10000,
        30000,
        120000
    );

    HINTERNET hConnect =
        WinHttpConnect(
            hSession,
            hostName,
            components.nPort,
            0
        );

    if (!hConnect)
    {
        std::cout
            << "[-] WinHttpConnect failed: "
            << GetLastError()
            << std::endl;

        WinHttpCloseHandle(hSession);

        return false;
    }

    DWORD flags = 0;

    if (components.nScheme ==
        INTERNET_SCHEME_HTTPS)
    {
        flags |= WINHTTP_FLAG_SECURE;
    }

    // ========================================================
    // BUILD REQUEST PATH
    // ========================================================

    std::wstring requestPath =
        urlPath;

    if (extraInfo[0] != L'\0')
    {
        requestPath += extraInfo;
    }

    HINTERNET hRequest =
        WinHttpOpenRequest(
            hConnect,
            L"GET",
            requestPath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags
        );

    if (!hRequest)
    {
        std::cout
            << "[-] WinHttpOpenRequest failed: "
            << GetLastError()
            << std::endl;

        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return false;
    }

    // ========================================================
    // REDIRECTS
    // ========================================================

    DWORD redirectPolicy =
        WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;

    WinHttpSetOption(
        hRequest,
        WINHTTP_OPTION_REDIRECT_POLICY,
        &redirectPolicy,
        sizeof(redirectPolicy)
    );

    // ========================================================
    // REQUEST
    // ========================================================

    const wchar_t* headers =
        L"Accept: image/avif,image/webp,image/apng,image/*,*/*\r\n";

    BOOL result =
        WinHttpSendRequest(
            hRequest,
            headers,
            static_cast<DWORD>(-1L),
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0
        );

    if (!result)
    {
        std::cout
            << "[-] WinHttpSendRequest failed: "
            << GetLastError()
            << std::endl;

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return false;
    }

    // ========================================================
    // RECEIVE RESPONSE
    // ========================================================

    result =
        WinHttpReceiveResponse(
            hRequest,
            nullptr
        );

    if (!result)
    {
        std::cout
            << "[-] WinHttpReceiveResponse failed: "
            << GetLastError()
            << std::endl;

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return false;
    }

    // ========================================================
    // HTTP STATUS
    // ========================================================

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);

    if (!WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE |
        WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX))
    {
        std::cout
            << "[-] Could not query HTTP status: "
            << GetLastError()
            << std::endl;

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return false;
    }

    std::cout
        << "[IMAGE] HTTP status: "
        << statusCode
        << std::endl;

    if (statusCode < 200 ||
        statusCode >= 300)
    {
        std::cout
            << "[-] HTTP request returned: "
            << statusCode
            << std::endl;

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return false;
    }

    // ========================================================
    // DOWNLOAD DATA
    // ========================================================

    while (true)
    {
        DWORD available = 0;

        if (!WinHttpQueryDataAvailable(
            hRequest,
            &available))
        {
            std::cout
                << "[-] WinHttpQueryDataAvailable failed: "
                << GetLastError()
                << std::endl;

            data.clear();
            break;
        }

        if (available == 0)
        {
            break;
        }

        const size_t oldSize =
            data.size();

        data.resize(
            oldSize + available
        );

        DWORD downloaded = 0;

        if (!WinHttpReadData(
            hRequest,
            data.data() + oldSize,
            available,
            &downloaded))
        {
            std::cout
                << "[-] WinHttpReadData failed: "
                << GetLastError()
                << std::endl;

            data.clear();
            break;
        }

        if (downloaded == 0)
        {
            data.resize(oldSize);
            break;
        }

        data.resize(
            oldSize + downloaded
        );
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // ========================================================
    // VERIFY DOWNLOAD
    // ========================================================

    if (data.empty())
    {
        std::cout
            << "[-] Downloaded image is empty"
            << std::endl;

        return false;
    }

    std::cout
        << "[IMAGE] Downloaded "
        << data.size()
        << " bytes"
        << std::endl;

    // ========================================================
    // SAVE LOCAL COPY
    // ========================================================

    std::ofstream outputFile(
        localImagePath,
        std::ios::binary
    );

    if (!outputFile)
    {
        std::cout
            << "[-] Failed to save image: "
            << localImagePath.string()
            << std::endl;

        // The download itself succeeded, so still return true.
    }
    else
    {
        outputFile.write(
            reinterpret_cast<const char*>(
                data.data()
                ),
            static_cast<std::streamsize>(
                data.size()
                )
        );

        outputFile.close();

        if (outputFile.good())
        {
            std::cout
                << "[IMAGE] Saved local copy: "
                << localImagePath.string()
                << std::endl;
        }
        else
        {
            std::cout
                << "[-] Failed while writing local image"
                << std::endl;
        }
    }

    // ========================================================
    // DEBUG HEADER
    // ========================================================

    if (data.size() >= 16)
    {
        std::cout
            << "[IMAGE] Header: ";

        for (size_t i = 0; i < 16; ++i)
        {
            printf(
                "%02X ",
                data[i]
            );
        }

        std::cout
            << std::endl;
    }

    return true;
}
bool LoadImageData(
    const std::string& source,
    std::vector<BYTE>& imageData
)
{
    imageData.clear();

    if (source.rfind("http://", 0) == 0 ||
        source.rfind("https://", 0) == 0)
    {
        return DownloadImageFromUrl(
            source,
            imageData
        );
    }

    std::ifstream file(
        source,
        std::ios::binary
    );

    if (!file)
    {
        std::cout
            << "[-] Could not open local image: "
            << source
            << std::endl;

        return false;
    }

    file.seekg(
        0,
        std::ios::end
    );

    const std::streamsize size =
        file.tellg();

    if (size <= 0)
    {
        std::cout
            << "[-] Local image is empty"
            << std::endl;

        return false;
    }

    file.seekg(
        0,
        std::ios::beg
    );

    imageData.resize(
        static_cast<size_t>(size)
    );

    if (!file.read(
        reinterpret_cast<char*>(
            imageData.data()
            ),
        size
    ))
    {
        imageData.clear();

        std::cout
            << "[-] Could not read local image: "
            << source
            << std::endl;

        return false;
    }

    return true;
}

// ============================================================
// CREATE TEST RECTANGLE
// ============================================================

bool CreateRectangle(
    const char* imageSource,
    float x,
    float y,
    float width,
    float height,
    float alpha,
    int sort
)
{
    if (!g_context ||
        !g_device ||
        !g_renderTarget ||
        !imageSource ||
        !g_testSampler ||
        !g_vertexShader ||
        !g_pixelShader ||
        !g_inputLayout ||
        !g_overlayWindow)
    {
        return false;
    }

    // ========================================================
    // IMAGE CACHE
    // ========================================================

    struct ImageCache
    {
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* textureView = nullptr;

        UINT width = 0;
        UINT height = 0;

        std::vector<GifFrame> frames;

        size_t currentFrame = 0;

        ULONGLONG lastFrameTime = 0;

        bool loaded = false;

        ~ImageCache()
        {
            if (textureView)
            {
                textureView->Release();
                textureView = nullptr;
            }

            if (texture)
            {
                texture->Release();
                texture = nullptr;
            }
        }
    };

    static std::unordered_map<
        std::string,
        ImageCache
    > imageCache;

    const std::string source =
        imageSource;

    // ========================================================
    // FIND IMAGE IN CACHE
    // ========================================================

    auto cacheIt =
        imageCache.find(source);

    // ========================================================
    // LOAD IMAGE IF NOT CACHED
    // ========================================================

    if (cacheIt == imageCache.end())
    {
        auto result =
            imageCache.emplace(
                source,
                ImageCache{}
            );

        cacheIt =
            result.first;

        ImageCache& image =
            cacheIt->second;

        // ====================================================
        // LOAD IMAGE
        // ====================================================

        std::vector<BYTE> imageData;

        if (!LoadImageData(
            imageSource,
            imageData))
        {
            std::cout
                << "[-] Failed to load image: "
                << imageSource
                << std::endl;

            imageCache.erase(cacheIt);

            return false;
        }

        if (imageData.empty())
        {
            std::cout
                << "[-] Image data is empty"
                << std::endl;

            imageCache.erase(cacheIt);

            return false;
        }

        std::cout
            << "[IMAGE] Loaded "
            << imageData.size()
            << " bytes"
            << std::endl;

        // ====================================================
        // WIC FACTORY
        // ====================================================

        IWICImagingFactory* wicFactory = nullptr;

        HRESULT hr =
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&wicFactory)
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] CreateRectangle: WIC factory failed: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            imageCache.erase(cacheIt);

            return false;
        }

        // ====================================================
        // WIC STREAM
        // ====================================================

        IWICStream* wicStream = nullptr;

        hr =
            wicFactory->CreateStream(
                &wicStream
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] CreateRectangle: CreateStream failed: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            wicFactory->Release();

            imageCache.erase(cacheIt);

            return false;
        }

        if (imageData.size() > 0xFFFFFFFFULL)
        {
            std::cout
                << "[-] Image is too large"
                << std::endl;

            wicStream->Release();
            wicFactory->Release();

            imageCache.erase(cacheIt);

            return false;
        }

        hr =
            wicStream->InitializeFromMemory(
                imageData.data(),
                static_cast<DWORD>(imageData.size())
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] Could not initialize WIC stream: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            wicStream->Release();
            wicFactory->Release();

            imageCache.erase(cacheIt);

            return false;
        }

        // ====================================================
        // WIC DECODER
        // ====================================================

        IWICBitmapDecoder* decoder = nullptr;

        hr =
            wicFactory->CreateDecoderFromStream(
                wicStream,
                nullptr,
                WICDecodeMetadataCacheOnLoad,
                &decoder
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] Could not create WIC decoder: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            wicStream->Release();
            wicFactory->Release();

            imageCache.erase(cacheIt);

            return false;
        }

        UINT frameCount = 0;

        hr =
            decoder->GetFrameCount(
                &frameCount
            );

        if (FAILED(hr) ||
            frameCount == 0)
        {
            std::cout
                << "[-] Could not get image frame count"
                << std::endl;

            decoder->Release();
            wicStream->Release();
            wicFactory->Release();

            imageCache.erase(cacheIt);

            return false;
        }

        std::cout
            << "[IMAGE] Frame count: "
            << frameCount
            << std::endl;

        // ====================================================
        // DECODE ALL FRAMES
        // ====================================================

        for (UINT frameIndex = 0;
            frameIndex < frameCount;
            ++frameIndex)
        {
            IWICBitmapFrameDecode* frame = nullptr;

            hr =
                decoder->GetFrame(
                    frameIndex,
                    &frame
                );

            if (FAILED(hr))
            {
                std::cout
                    << "[-] Could not get frame "
                    << frameIndex
                    << ": 0x"
                    << std::hex
                    << hr
                    << std::dec
                    << std::endl;

                continue;
            }

            UINT imageWidth = 0;
            UINT imageHeight = 0;

            hr =
                frame->GetSize(
                    &imageWidth,
                    &imageHeight
                );

            if (FAILED(hr) ||
                imageWidth == 0 ||
                imageHeight == 0)
            {
                frame->Release();
                continue;
            }

            if (frameIndex == 0)
            {
                image.width =
                    imageWidth;

                image.height =
                    imageHeight;
            }

            IWICFormatConverter* converter = nullptr;

            hr =
                wicFactory->CreateFormatConverter(
                    &converter
                );

            if (FAILED(hr))
            {
                frame->Release();
                continue;
            }

            hr =
                converter->Initialize(
                    frame,
                    GUID_WICPixelFormat32bppBGRA,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0.0,
                    WICBitmapPaletteTypeCustom
                );

            if (FAILED(hr))
            {
                converter->Release();
                frame->Release();
                continue;
            }

            const UINT stride =
                imageWidth * 4;

            const UINT bufferSize =
                stride * imageHeight;

            std::vector<BYTE> pixels(
                bufferSize
            );

            hr =
                converter->CopyPixels(
                    nullptr,
                    stride,
                    bufferSize,
                    pixels.data()
                );

            if (SUCCEEDED(hr))
            {
                GifFrame gifFrame{};

                gifFrame.width =
                    imageWidth;

                gifFrame.height =
                    imageHeight;

                gifFrame.delayMs =
                    100;

                gifFrame.pixels =
                    std::move(pixels);

                image.frames.push_back(
                    std::move(gifFrame)
                );
            }

            converter->Release();
            frame->Release();
        }

        decoder->Release();
        wicStream->Release();
        wicFactory->Release();

        // ====================================================
        // VERIFY FRAMES
        // ====================================================

        if (image.frames.empty())
        {
            std::cout
                << "[-] No image frames decoded"
                << std::endl;

            imageCache.erase(cacheIt);

            return false;
        }

        std::cout
            << "[IMAGE] Decoded frames: "
            << image.frames.size()
            << std::endl;

        std::cout
            << "[IMAGE] Animation size: "
            << image.width
            << "x"
            << image.height
            << std::endl;

        // ====================================================
        // CREATE DYNAMIC TEXTURE
        // ====================================================

        D3D11_TEXTURE2D_DESC textureDesc{};

        textureDesc.Width =
            image.width;

        textureDesc.Height =
            image.height;

        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;

        textureDesc.Format =
            DXGI_FORMAT_B8G8R8A8_UNORM;

        textureDesc.SampleDesc.Count = 1;

        textureDesc.Usage =
            D3D11_USAGE_DYNAMIC;

        textureDesc.BindFlags =
            D3D11_BIND_SHADER_RESOURCE;

        textureDesc.CPUAccessFlags =
            D3D11_CPU_ACCESS_WRITE;

        hr =
            g_device->CreateTexture2D(
                &textureDesc,
                nullptr,
                &image.texture
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] Could not create animated texture: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            imageCache.erase(cacheIt);

            return false;
        }

        // ====================================================
        // CREATE SHADER RESOURCE VIEW
        // ====================================================

        hr =
            g_device->CreateShaderResourceView(
                image.texture,
                nullptr,
                &image.textureView
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] Could not create shader resource view: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            imageCache.erase(cacheIt);

            return false;
        }

        // ====================================================
        // RESET ANIMATION
        // ====================================================

        image.currentFrame = 0;

        image.lastFrameTime =
            GetTickCount64();

        image.loaded = true;

        std::cout
            << "[IMAGE] Texture created"
            << std::endl;
    }

    // ========================================================
    // GET CACHED IMAGE
    // ========================================================

    ImageCache& image =
        cacheIt->second;

    if (!image.loaded ||
        !image.texture ||
        !image.textureView ||
        image.frames.empty())
    {
        return false;
    }

    // ========================================================
    // UPDATE ANIMATION
    // ========================================================

    const ULONGLONG now =
        GetTickCount64();

    if (image.currentFrame >=
        image.frames.size())
    {
        image.currentFrame = 0;
    }

    UINT delay =
        image.frames[
            image.currentFrame
        ].delayMs;

    if (delay == 0)
    {
        delay = 100;
    }

    if (now -
        image.lastFrameTime >=
        delay)
    {
        image.currentFrame++;

        if (image.currentFrame >=
            image.frames.size())
        {
            image.currentFrame = 0;
        }

        image.lastFrameTime =
            now;
    }

    // ========================================================
    // COPY CURRENT FRAME TO GPU
    // ========================================================

    const GifFrame& currentFrame =
        image.frames[
            image.currentFrame
        ];

    if (currentFrame.width != image.width ||
        currentFrame.height != image.height)
    {
        return false;
    }

    const size_t requiredPixels =
        static_cast<size_t>(
            image.width
            ) *
        static_cast<size_t>(
            image.height
            ) *
        4;

    if (currentFrame.pixels.size() <
        requiredPixels)
    {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};

    HRESULT hr =
        g_context->Map(
            image.texture,
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped
        );

    if (SUCCEEDED(hr))
    {
        const UINT rowBytes =
            image.width * 4;

        for (UINT row = 0;
            row < image.height;
            ++row)
        {
            memcpy(
                static_cast<BYTE*>(
                    mapped.pData
                    ) +
                row * mapped.RowPitch,

                currentFrame.pixels.data() +
                row * rowBytes,

                rowBytes
            );
        }

        g_context->Unmap(
            image.texture,
            0
        );
    }

    // ========================================================
    // GET OVERLAY SIZE
    // ========================================================

    RECT rect{};

    if (!GetClientRect(
        g_overlayWindow,
        &rect))
    {
        std::cout
            << "[-] CreateRectangle: GetClientRect failed"
            << std::endl;

        return false;
    }

    const float screenWidth =
        static_cast<float>(
            rect.right -
            rect.left
            );

    const float screenHeight =
        static_cast<float>(
            rect.bottom -
            rect.top
            );

    if (screenWidth <= 0.0f ||
        screenHeight <= 0.0f ||
        width <= 0.0f ||
        height <= 0.0f)
    {
        return false;
    }

    // ========================================================
    // ALPHA
    // ========================================================

    if (alpha < 0.0f)
        alpha = 0.0f;

    if (alpha > 1.0f)
        alpha = 1.0f;

    // ========================================================
    // PIXEL COORDINATES
    // ========================================================

    const float x0 = x;
    const float y0 = y;

    const float x1 = x + width;
    const float y1 = y + height;

    // ========================================================
    // PIXELS -> NDC
    // ========================================================

    const float ndcLeft =
        (x0 / screenWidth) * 2.0f - 1.0f;

    const float ndcRight =
        (x1 / screenWidth) * 2.0f - 1.0f;

    const float ndcTop =
        1.0f - (y0 / screenHeight) * 2.0f;

    const float ndcBottom =
        1.0f - (y1 / screenHeight) * 2.0f;

    // ========================================================
    // VERTICES
    // ========================================================

    Vertex vertices[4] =
    {
        {
            ndcLeft,
            ndcTop,
            1.0f,
            1.0f,
            1.0f,
            alpha,
            0.0f,
            0.0f
        },

        {
            ndcRight,
            ndcTop,
            1.0f,
            1.0f,
            1.0f,
            alpha,
            1.0f,
            0.0f
        },

        {
            ndcLeft,
            ndcBottom,
            1.0f,
            1.0f,
            1.0f,
            alpha,
            0.0f,
            1.0f
        },

        {
            ndcRight,
            ndcBottom,
            1.0f,
            1.0f,
            1.0f,
            alpha,
            1.0f,
            1.0f
        }
    };

    // ========================================================
    // VERTEX BUFFER
    // ========================================================

    D3D11_BUFFER_DESC bufferDesc{};

    bufferDesc.Usage =
        D3D11_USAGE_DEFAULT;

    bufferDesc.ByteWidth =
        sizeof(vertices);

    bufferDesc.BindFlags =
        D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData{};

    vertexData.pSysMem =
        vertices;

    ID3D11Buffer* vertexBuffer = nullptr;

    hr =
        g_device->CreateBuffer(
            &bufferDesc,
            &vertexData,
            &vertexBuffer
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] CreateRectangle: CreateBuffer failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    // ========================================================
    // BIND
    // ========================================================

    UINT stride =
        sizeof(Vertex);

    UINT offset = 0;

    g_context->IASetInputLayout(
        g_inputLayout
    );

    g_context->IASetVertexBuffers(
        0,
        1,
        &vertexBuffer,
        &stride,
        &offset
    );

    g_context->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
    );

    g_context->VSSetShader(
        g_vertexShader,
        nullptr,
        0
    );

    g_context->PSSetShader(
        g_pixelShader,
        nullptr,
        0
    );

    g_context->PSSetShaderResources(
        0,
        1,
        &image.textureView
    );

    g_context->PSSetSamplers(
        0,
        1,
        &g_testSampler
    );

    // ========================================================
    // DRAW
    // ========================================================

    g_context->Draw(
        4,
        0
    );

    // ========================================================
    // UNBIND
    // ========================================================

    ID3D11ShaderResourceView* nullSRV =
        nullptr;

    g_context->PSSetShaderResources(
        0,
        1,
        &nullSRV
    );

    // ========================================================
    // CLEANUP
    // ========================================================

    vertexBuffer->Release();

    return true;
}

bool CreateText(
    const char* text,
    void (*function)(),
    float x,
    float y,
    float fontSize,
    float alpha,
    int sort
)
{
    if (!g_context ||
        !g_device ||
        !g_renderTarget ||
        !text ||
        !g_overlayWindow)
    {
        return false;
    }

    if (!*text)
    {
        return false;
    }

    RECT rect{};

    if (!GetClientRect(
        g_overlayWindow,
        &rect))
    {
        std::cout
            << "[-] CreateText: GetClientRect failed"
            << std::endl;

        return false;
    }

    const float screenWidth =
        static_cast<float>(
            rect.right -
            rect.left
            );

    const float screenHeight =
        static_cast<float>(
            rect.bottom -
            rect.top
            );

    if (screenWidth <= 0.0f ||
        screenHeight <= 0.0f ||
        fontSize <= 0.0f)
    {
        return false;
    }

    if (alpha < 0.0f)
        alpha = 0.0f;

    if (alpha > 1.0f)
        alpha = 1.0f;

    static ID2D1Factory* d2dFactory = nullptr;
    static IDWriteFactory* writeFactory = nullptr;
    static ID2D1RenderTarget* d2dRenderTarget = nullptr;
    static ID2D1SolidColorBrush* textBrush = nullptr;

    // ========================================================
    // CREATE DIRECT2D / DIRECTWRITE
    // ========================================================

    if (!d2dFactory)
    {
        HRESULT hr =
            D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED,
                &d2dFactory
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] CreateText: D2D factory failed: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            return false;
        }
    }

    if (!writeFactory)
    {
        HRESULT hr =
            DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(
                    &writeFactory
                    )
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] CreateText: DirectWrite factory failed: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            return false;
        }
    }

    // ========================================================
    // CREATE D2D TARGET
    // ========================================================

    if (!d2dRenderTarget)
    {
        IDXGISurface* surface = nullptr;

        IDXGISwapChain* swapChain = nullptr;

        if (!g_swapChain)
        {
            return false;
        }

        HRESULT hr =
            g_swapChain->GetBuffer(
                0,
                IID_PPV_ARGS(&surface)
            );

        if (FAILED(hr))
        {
            return false;
        }

        D2D1_RENDER_TARGET_PROPERTIES props =
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(
                    DXGI_FORMAT_UNKNOWN,
                    D2D1_ALPHA_MODE_PREMULTIPLIED
                )
            );

        hr =
            d2dFactory->CreateDxgiSurfaceRenderTarget(
                surface,
                &props,
                &d2dRenderTarget
            );

        surface->Release();

        if (FAILED(hr))
        {
            std::cout
                << "[-] CreateText: D2D render target failed: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            return false;
        }
    }

    // ========================================================
    // CREATE BRUSH
    // ========================================================

    if (!textBrush)
    {
        HRESULT hr =
            d2dRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(
                    1.0f,
                    1.0f,
                    1.0f,
                    alpha
                ),
                &textBrush
            );

        if (FAILED(hr))
        {
            return false;
        }
    }
    else
    {
        textBrush->SetOpacity(alpha);
    }

    // ========================================================
    // CREATE TEXT FORMAT
    // ========================================================

    IDWriteTextFormat* textFormat = nullptr;

    HRESULT hr =
        writeFactory->CreateTextFormat(
            L"Arial",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            fontSize,
            L"",
            &textFormat
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] CreateText: CreateTextFormat failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    // ========================================================
    // CONVERT UTF-8 -> WIDE
    // ========================================================

    int wideLength =
        MultiByteToWideChar(
            CP_UTF8,
            0,
            text,
            -1,
            nullptr,
            0
        );

    if (wideLength <= 0)
    {
        textFormat->Release();
        return false;
    }

    std::vector<wchar_t> wideText(
        wideLength
    );

    MultiByteToWideChar(
        CP_UTF8,
        0,
        text,
        -1,
        wideText.data(),
        wideLength
    );

    // ========================================================
    // DRAW RECTANGLE
    // ========================================================

    D2D1_RECT_F textRect =
        D2D1::RectF(
            x,
            y,
            screenWidth,
            screenHeight
        );

    // ========================================================
    // DRAW TEXT
    // ========================================================

    d2dRenderTarget->BeginDraw();

    d2dRenderTarget->DrawText(
        wideText.data(),
        static_cast<UINT32>(
            wideLength - 1
            ),
        textFormat,
        textRect,
        textBrush
    );

    hr =
        d2dRenderTarget->EndDraw();

    textFormat->Release();

    if (FAILED(hr))
    {
        std::cout
            << "[-] CreateText: EndDraw failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    return true;
}

bool CreateTestResources()
{
    if (!CreateShaders())
    {
        return false;
    }

    // ========================================================
    // RESET IMAGE STATE
    // ========================================================

    g_gifFrames.clear();
    g_gifCurrentFrame = 0;
    g_gifLastFrameTime = GetTickCount64();

    g_gifWidth = 0;
    g_gifHeightAnim = 0;

    // ========================================================
    // CREATE SAMPLER
    // ========================================================

    D3D11_SAMPLER_DESC samplerDesc{};

    samplerDesc.Filter =
        D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    samplerDesc.AddressU =
        D3D11_TEXTURE_ADDRESS_CLAMP;

    samplerDesc.AddressV =
        D3D11_TEXTURE_ADDRESS_CLAMP;

    samplerDesc.AddressW =
        D3D11_TEXTURE_ADDRESS_CLAMP;

    samplerDesc.ComparisonFunc =
        D3D11_COMPARISON_NEVER;

    samplerDesc.MinLOD =
        0.0f;

    samplerDesc.MaxLOD =
        D3D11_FLOAT32_MAX;

    HRESULT hr =
        g_device->CreateSamplerState(
            &samplerDesc,
            &g_testSampler
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] CreateSamplerState failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return false;
    }

    // ========================================================
    // VERIFY OVERLAY SIZE
    // ========================================================

    RECT clientRect{};

    if (!GetClientRect(
        g_overlayWindow,
        &clientRect))
    {
        std::cout
            << "[-] GetClientRect failed"
            << std::endl;

        return false;
    }

    const float screenWidth =
        static_cast<float>(
            clientRect.right -
            clientRect.left
            );

    const float screenHeight =
        static_cast<float>(
            clientRect.bottom -
            clientRect.top
            );

    if (screenWidth <= 0.0f ||
        screenHeight <= 0.0f)
    {
        std::cout
            << "[-] Invalid overlay dimensions: "
            << screenWidth
            << "x"
            << screenHeight
            << std::endl;

        return false;
    }

    // ========================================================
    // DEBUG
    // ========================================================

    std::cout
        << "[IMAGE] Overlay size: "
        << screenWidth
        << "x"
        << screenHeight
        << std::endl;

    return true;
}
// ============================================================
// DRAW TEST
// ============================================================

void DrawTest()
{
    if (!g_context ||
        !g_renderTarget ||
        !g_overlayWindow)
    {
        return;
    }

    // ========================================================
    // GET ACTUAL OVERLAY SIZE
    // ========================================================

    RECT clientRect{};

    if (!GetClientRect(
        g_overlayWindow,
        &clientRect))
    {
        std::cout
            << "[-] GetClientRect failed in DrawTest"
            << std::endl;

        return;
    }

    const float screenWidth =
        static_cast<float>(
            clientRect.right -
            clientRect.left
            );

    const float screenHeight =
        static_cast<float>(
            clientRect.bottom -
            clientRect.top
            );

    if (screenWidth <= 0.0f ||
        screenHeight <= 0.0f)
    {
        return;
    }

    // ========================================================
    // CLEAR
    // ========================================================

    const float clearColor[4] =
    {
        0.0f,
        0.0f,
        0.0f,
        0.0f
    };

    g_context->ClearRenderTargetView(
        g_renderTarget,
        clearColor
    );

    // ========================================================
    // VIEWPORT
    // ========================================================

    D3D11_VIEWPORT viewport{};

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;

    viewport.Width =
        screenWidth;

    viewport.Height =
        screenHeight;

    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    g_context->RSSetViewports(
        1,
        &viewport
    );

    // ========================================================
    // RENDER TARGET
    // ========================================================

    g_context->OMSetRenderTargets(
        1,
        &g_renderTarget,
        nullptr
    );

    // ========================================================
    // BLEND STATE
    // ========================================================

    static ID3D11BlendState* blendState = nullptr;

    if (!blendState)
    {
        D3D11_BLEND_DESC blendDesc{};

        blendDesc.AlphaToCoverageEnable =
            FALSE;

        blendDesc.IndependentBlendEnable =
            FALSE;

        blendDesc.RenderTarget[0].BlendEnable =
            TRUE;

        blendDesc.RenderTarget[0].SrcBlend =
            D3D11_BLEND_SRC_ALPHA;

        blendDesc.RenderTarget[0].DestBlend =
            D3D11_BLEND_INV_SRC_ALPHA;

        blendDesc.RenderTarget[0].BlendOp =
            D3D11_BLEND_OP_ADD;

        blendDesc.RenderTarget[0].SrcBlendAlpha =
            D3D11_BLEND_ONE;

        blendDesc.RenderTarget[0].DestBlendAlpha =
            D3D11_BLEND_INV_SRC_ALPHA;

        blendDesc.RenderTarget[0].BlendOpAlpha =
            D3D11_BLEND_OP_ADD;

        blendDesc.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_ALL;

        HRESULT hr =
            g_device->CreateBlendState(
                &blendDesc,
                &blendState
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] CreateBlendState failed: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            return;
        }
    }

    g_context->OMSetBlendState(
        blendState,
        nullptr,
        0xFFFFFFFF
    );

    // ========================================================
    // SCRIPT
    // ========================================================

    static bool scriptLoaded = false;

    static std::filesystem::path scriptFile;

    static std::filesystem::file_time_type
        scriptLastWriteTime{};

    try
    {
        // ====================================================
        // FIND SCRIPT
        // ====================================================

        if (!scriptLoaded)
        {
            const std::filesystem::path
                processDirectory =
                std::filesystem::current_path();

            const std::filesystem::path
                scriptDirectory =
                processDirectory /
                "raw" /
                "scripts";

            if (!std::filesystem::exists(
                scriptDirectory))
            {
                std::cout
                    << "[-] Script directory not found: "
                    << scriptDirectory.string()
                    << std::endl;

                return;
            }

            if (!std::filesystem::is_directory(
                scriptDirectory))
            {
                std::cout
                    << "[-] Script path is not a directory: "
                    << scriptDirectory.string()
                    << std::endl;

                return;
            }

            for (const auto& entry :
                std::filesystem::directory_iterator(
                    scriptDirectory))
            {
                if (!entry.is_regular_file())
                    continue;

                scriptFile =
                    entry.path();

                break;
            }

            if (scriptFile.empty())
            {
                std::cout
                    << "[-] No script file found in: "
                    << scriptDirectory.string()
                    << std::endl;

                return;
            }

            // =================================================
            // FIRST LOAD
            // =================================================

            if (!menuStructure::Load(
                scriptFile.string().c_str()))
            {
                std::cout
                    << "[-] Failed to load script: "
                    << scriptFile.string()
                    << std::endl;

                scriptFile.clear();

                return;
            }

            scriptLastWriteTime =
                std::filesystem::last_write_time(
                    scriptFile
                );

            scriptLoaded = true;

            std::cout
                << "[Script] Loaded: "
                << scriptFile.string()
                << std::endl;
        }

        // ====================================================
        // HOT RELOAD
        // ====================================================

        if (scriptLoaded &&
            !scriptFile.empty())
        {
            if (!std::filesystem::exists(
                scriptFile))
            {
                std::cout
                    << "[-] Script file no longer exists: "
                    << scriptFile.string()
                    << std::endl;

                scriptLoaded = false;
                scriptFile.clear();

                return;
            }

            const auto currentWriteTime =
                std::filesystem::last_write_time(
                    scriptFile
                );

            if (currentWriteTime !=
                scriptLastWriteTime)
            {
                std::cout
                    << "[Script] File changed, reloading: "
                    << scriptFile.string()
                    << std::endl;

                if (menuStructure::Load(
                    scriptFile.string().c_str()))
                {
                    scriptLastWriteTime =
                        currentWriteTime;

                    std::cout
                        << "[Script] Reloaded successfully"
                        << std::endl;
                }
                else
                {
                    std::cout
                        << "[-] Script reload failed"
                        << std::endl;
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        std::cout
            << "[-] Script filesystem error: "
            << e.what()
            << std::endl;

        return;
    }

    // ========================================================
    // EXECUTE SCRIPT DRAW FUNCTION
    // ========================================================

    menuStructure::Call("Draw");
}
// ============================================================
// INITIALIZE
// ============================================================

bool Overlay::Initialize()
{
    if (!FindGameWindow())
    {
        return false;
    }

    if (!CreateOverlayWindow())
    {
        return false;
    }

    if (!CreateDX11())
    {
        return false;
    }

    if (!CreateTestResources())
    {
        return false;
    }

    std::cout
        << "[+] External DX11 transparent overlay ready"
        << std::endl;

    return true;
}

// ============================================================
// Resize
// ============================================================

void Overlay::Resize(int width, int height)
{
    if (!g_swapChain ||
        width <= 0 ||
        height <= 0)
    {
        return;
    }

    if (g_renderTarget)
    {
        g_renderTarget->Release();
        g_renderTarget = nullptr;
    }

    HRESULT hr =
        g_swapChain->ResizeBuffers(
            0,
            width,
            height,
            DXGI_FORMAT_UNKNOWN,
            0
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] ResizeBuffers failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return;
    }

    if (!CreateRenderTarget())
    {
        std::cout
            << "[-] Failed to recreate render target"
            << std::endl;

        return;
    }

    //std::cout
    //    << "[+] Overlay resized: "
    //    << width
    //    << "x"
    //    << height
    //    << std::endl;
}

// ============================================================
// UPDATE
// ============================================================
void DebugReadbackPixel()
{
    if (!g_swapChain ||
        !g_device ||
        !g_context)
    {
        return;
    }

    static ID3D11Texture2D* stagingTexture = nullptr;
    static UINT stagingWidth = 0;
    static UINT stagingHeight = 0;

    DXGI_SWAP_CHAIN_DESC1 swapDesc{};

    HRESULT hr =
        g_swapChain->GetDesc1(
            &swapDesc
        );

    if (FAILED(hr))
    {
        return;
    }

    // ========================================================
    // CREATE CPU-READABLE COPY OF SWAPCHAIN
    // ========================================================

    if (!stagingTexture ||
        stagingWidth != swapDesc.Width ||
        stagingHeight != swapDesc.Height)
    {
        if (stagingTexture)
        {
            stagingTexture->Release();
            stagingTexture = nullptr;
        }

        D3D11_TEXTURE2D_DESC desc{};

        desc.Width =
            swapDesc.Width;

        desc.Height =
            swapDesc.Height;

        desc.MipLevels =
            1;

        desc.ArraySize =
            1;

        desc.Format =
            swapDesc.Format;

        desc.SampleDesc.Count =
            1;

        desc.SampleDesc.Quality =
            0;

        desc.Usage =
            D3D11_USAGE_STAGING;

        desc.BindFlags =
            0;

        desc.CPUAccessFlags =
            D3D11_CPU_ACCESS_READ;

        desc.MiscFlags =
            0;

        hr =
            g_device->CreateTexture2D(
                &desc,
                nullptr,
                &stagingTexture
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] Create staging texture failed: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;

            return;
        }

        stagingWidth =
            swapDesc.Width;

        stagingHeight =
            swapDesc.Height;
    }

    // ========================================================
    // GET CURRENT BACK BUFFER
    // ========================================================

    ID3D11Texture2D* backBuffer =
        nullptr;

    hr =
        g_swapChain->GetBuffer(
            0,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(
                &backBuffer
                )
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] Readback GetBuffer failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return;
    }

    // ========================================================
    // COPY GPU PIXELS TO CPU-READABLE TEXTURE
    // ========================================================

    g_context->CopyResource(
        stagingTexture,
        backBuffer
    );

    backBuffer->Release();

    // ========================================================
    // MAP
    // ========================================================

    D3D11_MAPPED_SUBRESOURCE mapped{};

    hr =
        g_context->Map(
            stagingTexture,
            0,
            D3D11_MAP_READ,
            0,
            &mapped
        );

    if (FAILED(hr))
    {
        std::cout
            << "[-] Readback Map failed: 0x"
            << std::hex
            << hr
            << std::dec
            << std::endl;

        return;
    }

    // ========================================================
    // READ CENTER PIXEL
    //
    // Swapchain is BGRA.
    // ========================================================

    const UINT x =
        swapDesc.Width / 2;

    const UINT y =
        swapDesc.Height / 2;

    const BYTE* row =
        static_cast<const BYTE*>(
            mapped.pData
            ) +
        (static_cast<size_t>(y) *
            mapped.RowPitch);

    const BYTE* pixel =
        row +
        (static_cast<size_t>(x) * 4);

    const int B =
        pixel[0];

    const int G =
        pixel[1];

    const int R =
        pixel[2];

    const int A =
        pixel[3];

    g_context->Unmap(
        stagingTexture,
        0
    );

    //std::cout
    //    << "[READBACK] "
    //    << "R=" << R
    //    << " G=" << G
    //    << " B=" << B
    //    << " A=" << A
    //    << std::endl;
}

void Overlay::Update()
{
    if (!g_gameWindow ||
        !g_overlayWindow ||
        !g_swapChain)
    {
        return;
    }

    // ========================================================
    // HOME
    // ========================================================

    if (GetAsyncKeyState(VK_HOME) & 1)
    {
        g_visible =
            !g_visible;

        std::cout
            << "[*] Overlay: "
            << (
                g_visible
                ? "ON"
                : "OFF"
                )
            << std::endl;

        if (g_visible)
        {
            ShowWindow(
                g_overlayWindow,
                SW_SHOWNOACTIVATE
            );
        }
        else
        {
            ShowWindow(
                g_overlayWindow,
                SW_HIDE
            );
        }
    }

    // ========================================================
    // FOLLOW KH1 WINDOW
    // ========================================================

    static int g_overlayWidth = 0;
    static int g_overlayHeight = 0;

    RECT gameRect{};

    if (GetWindowRect(
        g_gameWindow,
        &gameRect))
    {
        if (g_visible)
        {
            SetWindowPos(
                g_overlayWindow,
                g_gameWindow,
                gameRect.left + 100,
                gameRect.top + 75,
                350,
                480,
                SWP_NOACTIVATE |
                SWP_SHOWWINDOW
            );

            RECT overlayRect{};

            if (GetWindowRect(
                g_overlayWindow,
                &overlayRect))
            {
                int overlayWidth =
                    overlayRect.right -
                    overlayRect.left;

                int overlayHeight =
                    overlayRect.bottom -
                    overlayRect.top;

                if (overlayWidth != g_overlayWidth ||
                    overlayHeight != g_overlayHeight)
                {
                    Resize(
                        overlayWidth,
                        overlayHeight
                    );

                    g_overlayWidth =
                        overlayWidth;

                    g_overlayHeight =
                        overlayHeight;
                }
            }
        }
        else
        {
            ShowWindow(
                g_overlayWindow,
                SW_HIDE
            );
        }
    }

    // ========================================================
    // DRAW
    // ========================================================

    if (g_visible)
    {
        UpdateGif();

        DrawTest();

        DebugReadbackPixel();

        HRESULT hr =
            g_swapChain->Present(
                1,
                0
            );

        if (FAILED(hr))
        {
            std::cout
                << "[-] Present failed: 0x"
                << std::hex
                << hr
                << std::dec
                << std::endl;
        }
    }

    // ========================================================
    // WINDOW MESSAGES
    // ========================================================

    MSG msg{};

    while (PeekMessageA(
        &msg,
        g_overlayWindow,
        0,
        0,
        PM_REMOVE))
    {
        TranslateMessage(
            &msg
        );

        DispatchMessageA(
            &msg
        );
    }
}
// ============================================================
// SHUTDOWN
// ============================================================

void Overlay::Shutdown()
{
    // ========================================================
    // DX11 RESOURCES
    // ========================================================

    if (g_vertexBuffer)
    {
        g_vertexBuffer->Release();
        g_vertexBuffer = nullptr;
    }

    if (g_inputLayout)
    {
        g_inputLayout->Release();
        g_inputLayout = nullptr;
    }

    if (g_pixelShader)
    {
        g_pixelShader->Release();
        g_pixelShader = nullptr;
    }

    if (g_vertexShader)
    {
        g_vertexShader->Release();
        g_vertexShader = nullptr;
    }

    if (g_renderTarget)
    {
        g_renderTarget->Release();
        g_renderTarget = nullptr;
    }

    // ========================================================
    // DIRECTCOMPOSITION
    //
    // Release these before the swapchain/device.
    // ========================================================

    if (g_dcompTarget)
    {
        g_dcompTarget->Release();
        g_dcompTarget = nullptr;
    }

    if (g_dcompVisual)
    {
        g_dcompVisual->Release();
        g_dcompVisual = nullptr;
    }

    if (g_dcompDevice)
    {
        g_dcompDevice->Release();
        g_dcompDevice = nullptr;
    }

    // ========================================================
    // SWAPCHAIN
    // ========================================================

    if (g_swapChain)
    {
        g_swapChain->Release();
        g_swapChain = nullptr;
    }

    // ========================================================
    // DEVICE CONTEXT
    // ========================================================

    if (g_context)
    {
        g_context->Release();
        g_context = nullptr;
    }

    // ========================================================
    // DEVICE
    // ========================================================

    if (g_device)
    {
        g_device->Release();
        g_device = nullptr;
    }

    // ========================================================
    // WINDOW
    // ========================================================

    if (g_overlayWindow)
    {
        DestroyWindow(
            g_overlayWindow
        );

        g_overlayWindow = nullptr;
    }

    g_gameWindow =
        nullptr;

    UnregisterClassA(
        WINDOW_CLASS,
        GetModuleHandleA(nullptr)
    );

    std::cout
        << "[+] Overlay shutdown"
        << std::endl;
}