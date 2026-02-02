#include <borealis/platforms/driver/d3d11.hpp>
#include <borealis/core/logger.hpp>
#define NANOVG_D3D11_IMPLEMENTATION
#include <nanovg_d3d11.h>
#include <versionhelpers.h>
#ifdef __ALLOW_TEARING__
#include <dxgi1_6.h>
#endif
#ifdef __GLFW__
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(__SDL2__)
#include <SDL_syswm.h>
#endif
#ifdef __WINRT__
#include <windows.ui.core.h>
#include <winrt/Windows.Graphics.Display.h>
#endif

namespace brls
{

static const int SwapChainBufferCount    = 2;
static const DXGI_SAMPLE_DESC sampleDesc = { 1, 0 };

#ifdef __GLFW__
D3D11Context::D3D11Context(GLFWwindow* window, int width, int height)
{
    this->hWnd = glfwGetWin32Window(window);
    this->initDX(this->hWnd, nullptr, width, height);
}
#elif defined(__SDL2__)
D3D11Context::D3D11Context(SDL_Window* window, int width, int height)
{
    SDL_SysWMinfo wi;
    SDL_GetVersion(&wi.version);
    SDL_GetWindowWMInfo(window, &wi);
#ifdef __WINRT__
    // winrt 代码需要特别编译
    ABI::Windows::UI::Core::ICoreWindow* coreWindow = nullptr;
    if (FAILED(wi.info.winrt.window->QueryInterface(&coreWindow)))
    {
        return;
    }
    this->initDX(nullptr, coreWindow, width, height);
#else
    this->hWnd = wi.info.win.window;
    this->initDX(this->hWnd, nullptr, width, height);
#endif
}
#endif

D3D11Context::~D3D11Context()
{
    this->unInitDX();
}

bool D3D11Context::initDX(HWND hWnd, IUnknown* coreWindow, int width, int height)
{
    HRESULT hr = S_OK;

    IDXGIDevice* dxgiDevice    = nullptr;
    IDXGIAdapter* dxgiAdapter  = nullptr;
    IDXGIFactory2* dxgiFactory = nullptr;

    static const D3D_DRIVER_TYPE driverAttempts[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };

    static const D3D_FEATURE_LEVEL levelAttempts[] = {
        D3D_FEATURE_LEVEL_11_1, // Direct3D 11.1 SM 6
        D3D_FEATURE_LEVEL_11_0, // Direct3D 11.0 SM 5
        D3D_FEATURE_LEVEL_10_1, // Direct3D 10.1 SM 4
        D3D_FEATURE_LEVEL_10_0, // Direct3D 10.0 SM 4
        D3D_FEATURE_LEVEL_9_3, // Direct3D 9.3  SM 3
        D3D_FEATURE_LEVEL_9_2, // Direct3D 9.2  SM 2
        D3D_FEATURE_LEVEL_9_1, // Direct3D 9.1  SM 2
    };

    for (size_t driver = 0; driver < ARRAYSIZE(driverAttempts); driver++)
    {
        hr = D3D11CreateDevice(
            nullptr,
            driverAttempts[driver],
            nullptr,
            0,
            levelAttempts,
            ARRAYSIZE(levelAttempts),
            D3D11_SDK_VERSION,
            &this->device,
            nullptr,
            &this->deviceContext);

        if (SUCCEEDED(hr))
        {
            break;
        }
    }
    if (SUCCEEDED(hr))
    {
        hr = this->device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    }
    if (SUCCEEDED(hr))
    {
        hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    }
    if (SUCCEEDED(hr))
    {
        hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    }

    if (SUCCEEDED(hr))
    {
        DXGI_SWAP_CHAIN_DESC1 swapDesc;
        ZeroMemory(&swapDesc, sizeof(swapDesc));
        swapDesc.SampleDesc.Count   = sampleDesc.Count;
        swapDesc.SampleDesc.Quality = sampleDesc.Quality;
        swapDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.Stereo             = FALSE;
        swapDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount        = SwapChainBufferCount;
        swapDesc.Flags              = 0;
        swapDesc.Scaling            = DXGI_SCALING_STRETCH;
        if (IsWindows10OrGreater()) {
            swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        } else {
            swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        }
#ifdef __WINRT__
        if (IsWindows8OrGreater())
        {
            swapDesc.Scaling = DXGI_SCALING_NONE;
        }
        else
        {
            swapDesc.Scaling = DXGI_SCALING_STRETCH;
        }
#endif
#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
        swapDesc.Scaling    = DXGI_SCALING_STRETCH;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
#endif
        // this->sd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        if (coreWindow)
        {
            hr = dxgiFactory->CreateSwapChainForCoreWindow(
                this->device,
                coreWindow,
                &swapDesc,
                nullptr,
                &this->swapChain);
        }
        else
        {
            hr = dxgiFactory->CreateSwapChainForHwnd(
                this->device,
                hWnd,
                &swapDesc,
                nullptr,
                nullptr,
                &this->swapChain);
        }
    }
    D3D_API_RELEASE(dxgiDevice);
    D3D_API_RELEASE(dxgiAdapter);
    D3D_API_RELEASE(dxgiFactory);

    if (FAILED(hr))
    {
        Logger::error("Failed init D3D11 {:#010x}", hr);
        this->unInitDX();

        char errorText[256] = { 0 };
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      nullptr,
                      hr,
                      0,
                      errorText,
                      sizeof(errorText),
                      nullptr);
        MessageBox(hWnd, errorText, "Init D3D11 Failed", MB_ICONERROR);
        ExitProcess(hr);
        return false;
    }

    this->GetDpiForWindow = (UINT(WINAPI*)(HWND))GetProcAddress(GetModuleHandleW(L"USER32.DLL"), "GetDpiForWindow");

    return true;
}

void D3D11Context::unInitDX()
{
    // Detach RTs
    if (this->deviceContext)
    {
        ID3D11RenderTargetView* viewList[1] = { nullptr };
        this->deviceContext->OMSetRenderTargets(1, viewList, nullptr);
    }
    D3D_API_RELEASE(this->deviceContext);
    D3D_API_RELEASE(this->device);
    D3D_API_RELEASE(this->swapChain);
    D3D_API_RELEASE(this->renderTargetView);
    D3D_API_RELEASE(this->depthStencilView);
}

double D3D11Context::getScaleFactor()
{
#ifdef __WINRT__
    static auto displayInformation = winrt::Windows::Graphics::Display::DisplayInformation::GetForCurrentView();

    return (unsigned int)displayInformation.LogicalDpi() / 96.0f;
#else
    if (this->GetDpiForWindow)
    {
        return this->GetDpiForWindow(this->hWnd) / 96.0;
    }

    HDC hdc = GetDC(nullptr);
    if (hdc)
    {
        int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(nullptr, hdc);
        return dpi / 96.0;
    }

    return 1.0;
#endif
}

bool D3D11Context::onFramebufferSize(int width, int height, bool init)
{
    HRESULT hr = S_OK;

    ID3D11Texture2D* backBuffer         = nullptr;
    ID3D11Texture2D* depthStencil       = nullptr;
    ID3D11RenderTargetView* viewList[1] = { nullptr };
    this->deviceContext->OMSetRenderTargets(1, viewList, nullptr);

    D3D_API_RELEASE(this->renderTargetView);
    D3D_API_RELEASE(this->depthStencilView);

    if (!init)
    {
        hr = this->swapChain->ResizeBuffers(SwapChainBufferCount, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
            return false;
        }
    }

    hr = this->swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr))
    {
        return false;
    }

    hr = this->device->CreateRenderTargetView(backBuffer, nullptr, &this->renderTargetView);
    D3D_API_RELEASE(backBuffer);
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC texDesc;
    texDesc.ArraySize          = 1;
    texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
    texDesc.CPUAccessFlags     = 0;
    texDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    texDesc.Height             = (UINT)height;
    texDesc.Width              = (UINT)width;
    texDesc.MipLevels          = 1;
    texDesc.MiscFlags          = 0;
    texDesc.SampleDesc.Count   = sampleDesc.Count;
    texDesc.SampleDesc.Quality = sampleDesc.Quality;
    texDesc.Usage              = D3D11_USAGE_DEFAULT;

    hr = this->device->CreateTexture2D(&texDesc, nullptr, &depthStencil);
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc;
    depthViewDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthViewDesc.ViewDimension      = (sampleDesc.Count > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Flags              = 0;
    depthViewDesc.Texture2D.MipSlice = 0;

    hr = this->device->CreateDepthStencilView(depthStencil, &depthViewDesc, &this->depthStencilView);
    D3D_API_RELEASE(depthStencil);
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_VIEWPORT viewport;
    viewport.Width    = (float)width;
    viewport.Height   = (float)height;
    viewport.MaxDepth = 1.0f;
    viewport.MinDepth = 0.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    this->deviceContext->RSSetViewports(1, &viewport);

    return true;
}

void D3D11Context::clear(NVGcolor color)
{
    float clearColor[4] = { color.r, color.g, color.b, color.a };
    this->deviceContext->ClearRenderTargetView(this->renderTargetView, clearColor);
    this->deviceContext->ClearDepthStencilView(this->depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
}

void D3D11Context::beginFrame()
{
    this->deviceContext->OMSetRenderTargets(1, &this->renderTargetView, this->depthStencilView);
}

void D3D11Context::endFrame()
{
    // https://learn.microsoft.com/zh-cn/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present
    DXGI_PRESENT_PARAMETERS presentParameters;
    ZeroMemory(&presentParameters, sizeof(DXGI_PRESENT_PARAMETERS));
    this->swapChain->Present1(interval, 0, &presentParameters);
}

void D3D11Context::setSwapInterval(int value)
{
    if (value < 0 || value > 4)
        return;
    this->interval = value;
}

}
