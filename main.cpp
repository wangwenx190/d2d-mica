/*
 * MIT License
 *
 * Copyright (C) 2022 by wangwenx190 (Yuhang Zhao)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <Windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <wrl\client.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <d2d1_2.h>
#include <d2d1effects_2.h>
#include <wincodec.h>
#include <timeapi.h>
#include <uxtheme.h>
#include <clocale>
#include <string>
#include <iostream>
#include "resource.h"

static constexpr const float sc_blurRadius = 80.0f;
static constexpr const float sc_noiseOpacity = 0.02f;
static constexpr const D2D1_COLOR_F sc_darkThemeColor = { 0.125f, 0.125f, 0.125f, 1.0f };
static constexpr const float sc_darkThemeTintOpacity = 0.8f;
static constexpr const D2D1_COLOR_F sc_lightThemeColor = { 0.953f, 0.953f, 0.953f, 1.0f };
static constexpr const float sc_lightThemeTintOpacity = 0.5f;
static constexpr const float sc_luminosityOpacity = 1.0f;

static constexpr const wchar_t WINDOW_CLASS_NAME[] = L"org.wangwenx190.D2DMica.WindowClass\0";
static constexpr const wchar_t WINDOW_TITLE[] = L"Direct2D Mica Material\0";

static HINSTANCE g_hInstance = nullptr;
static HWND g_hWnd = nullptr;
static UINT g_dpi = 0;
static std::wstring g_wallpaperFilePath = {};
static bool g_darkModeEnabled = false;
static LONG g_x1 = 0;
static LONG g_x2 = 0;
static LONG g_y1 = 0;
static LONG g_y2 = 0;

static Microsoft::WRL::ComPtr<ID2D1Factory2> g_D2DFactory = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Device1> g_D2DDevice = nullptr;
static Microsoft::WRL::ComPtr<ID2D1DeviceContext1> g_D2DContext = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Bitmap1> g_D2DTargetBitmap = nullptr;
static D2D1_BITMAP_PROPERTIES1 g_D2DBitmapProperties = {};

static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DWallpaperBitmapSourceEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DNoiseBitmapSourceEffect = nullptr;

static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DTintColorEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DTintOpacityEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DLuminosityColorEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DLuminosityOpacityEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DLuminosityBlendEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DLuminosityColorBlendEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DGaussianBlurEffect = nullptr;

static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DNoiseBorderEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DNoiseOpacityEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DNoiseBlendEffectOuter = nullptr;

static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DFinalBrushEffect = nullptr;

static Microsoft::WRL::ComPtr<IWICImagingFactory2> g_WICFactory = nullptr;

static Microsoft::WRL::ComPtr<IWICBitmapDecoder> g_WICWallpaperDecoder = nullptr;
static Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> g_WICWallpaperFrame = nullptr;
static Microsoft::WRL::ComPtr<IWICStream> g_WICWallpaperStream = nullptr;
static Microsoft::WRL::ComPtr<IWICFormatConverter> g_WICWallpaperConverter = nullptr;
static Microsoft::WRL::ComPtr<IWICBitmapScaler> g_WICWallpaperScaler = nullptr;

static Microsoft::WRL::ComPtr<IWICBitmapDecoder> g_WICNoiseDecoder = nullptr;
static Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> g_WICNoiseFrame = nullptr;
static Microsoft::WRL::ComPtr<IWICStream> g_WICNoiseStream = nullptr;
static Microsoft::WRL::ComPtr<IWICFormatConverter> g_WICNoiseConverter = nullptr;
static Microsoft::WRL::ComPtr<IWICBitmapScaler> g_WICNoiseScaler = nullptr;

static Microsoft::WRL::ComPtr<ID3D11Device> g_D3D11Device = nullptr;
static Microsoft::WRL::ComPtr<ID3D11DeviceContext> g_D3D11Context = nullptr;
static Microsoft::WRL::ComPtr<ID3D11Texture2D> g_D3D11Texture = nullptr;

static Microsoft::WRL::ComPtr<IDXGIDevice1> g_DXGIDevice = nullptr;
static Microsoft::WRL::ComPtr<IDXGIAdapter> g_DXGIAdapter = nullptr;
static Microsoft::WRL::ComPtr<IDXGIFactory2> g_DXGIFactory = nullptr;
static Microsoft::WRL::ComPtr<IDXGISurface> g_DXGISurface = nullptr;
static Microsoft::WRL::ComPtr<IDXGISwapChain1> g_DXGISwapChain = nullptr;
static DXGI_SWAP_CHAIN_DESC1 g_DXGISwapChainDesc = {};

static D3D_FEATURE_LEVEL g_D3DFeatureLevel = D3D_FEATURE_LEVEL_1_0_CORE;

[[nodiscard]] static inline std::wstring GetWallpaperFilePath()
{
    wchar_t buffer[MAX_PATH] = { L'\0' };
    SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, buffer, 0);
    return buffer;
}

[[nodiscard]] static inline bool ShouldAppsUseDarkMode()
{
    HKEY key = nullptr;
    RegOpenKeyExW(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", 0, KEY_READ, &key);
    DWORD value = 0;
    DWORD size = sizeof(value);
    RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);
    return (value == 0);
}

static inline void UpdateWindowTheme()
{
    const BOOL dark = g_darkModeEnabled ? TRUE : FALSE;
    DwmSetWindowAttribute(g_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    SetWindowTheme(g_hWnd, (g_darkModeEnabled ? L"DarkMode_Explorer" : L"Explorer"), nullptr);
}

static inline void UpdateWallpaperBitmap()
{
    g_WICFactory->CreateDecoderFromFilename(g_wallpaperFilePath.c_str(),
        nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &g_WICWallpaperDecoder);
    g_WICWallpaperDecoder->GetFrame(0, &g_WICWallpaperFrame);
    g_WICFactory->CreateFormatConverter(&g_WICWallpaperConverter);
    g_WICWallpaperConverter->Initialize(g_WICWallpaperFrame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
    g_D2DWallpaperBitmapSourceEffect->SetValue(D2D1_BITMAPSOURCE_PROP_WIC_BITMAP_SOURCE, g_WICWallpaperConverter.Get());
}

static inline void UpdateBrushAppearance()
{
    g_D2DTintColorEffect->SetValue(D2D1_FLOOD_PROP_COLOR, (g_darkModeEnabled ? sc_darkThemeColor : sc_lightThemeColor));
    g_D2DTintOpacityEffect->SetValue(D2D1_OPACITY_PROP_OPACITY, (g_darkModeEnabled ? sc_darkThemeTintOpacity : sc_lightThemeTintOpacity));
    g_D2DLuminosityColorEffect->SetValue(D2D1_FLOOD_PROP_COLOR, (g_darkModeEnabled ? sc_darkThemeColor : sc_lightThemeColor));
    g_D2DLuminosityOpacityEffect->SetValue(D2D1_OPACITY_PROP_OPACITY, sc_luminosityOpacity);
}

static inline void D2DDraw(const bool force)
{
    if (!IsWindowVisible(g_hWnd) || IsMinimized(g_hWnd)) {
        return;
    }
    const int sizeFrameWidth = GetSystemMetricsForDpi(SM_CXSIZEFRAME, g_dpi);
    const int sizeFrameHeight = GetSystemMetricsForDpi(SM_CYSIZEFRAME, g_dpi);
    const int paddedBorderWidth = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, g_dpi);
    const int resizeBorderWidth = sizeFrameWidth + paddedBorderWidth;
    const int resizeBorderHeight = sizeFrameHeight + paddedBorderWidth;
    const int captionHeight = GetSystemMetricsForDpi(SM_CYCAPTION, g_dpi);
    const int titleBarHeight = captionHeight + resizeBorderHeight;
    RECT windowRect = {};
    GetWindowRect(g_hWnd, &windowRect);
    const LONG x1 = windowRect.left + resizeBorderWidth;
    const LONG x2 = windowRect.right - resizeBorderWidth;
    const LONG y1 = windowRect.top + titleBarHeight;
    const LONG y2 = windowRect.bottom - resizeBorderHeight;
    if (!force && (g_x1 == x1) && (g_x2 == x2) && (g_y1 == y1) && (g_y2 == y2)) {
        return;
    }
    g_x1 = x1;
    g_x2 = x2;
    g_y1 = y1;
    g_y2 = y2;
    const D2D1_RECT_F rect = { float(g_x1), float(g_y1), float(g_x2), float(g_y2) };
    g_D2DContext->BeginDraw();
    g_D2DContext->DrawImage(g_D2DFinalBrushEffect.Get(), nullptr, &rect, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    g_D2DContext->EndDraw();
    DXGI_PRESENT_PARAMETERS params;
    SecureZeroMemory(&params, sizeof(params));
    // Without this step, nothing will be visible to the user.
    g_DXGISwapChain->Present1(0, DXGI_PRESENT_ALLOW_TEARING, &params);
    // Try to reduce flicker as much as possible.
    DwmFlush();
}

[[nodiscard]] static inline LRESULT CALLBACK WndProc
    (const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg) {
    case WM_PAINT:
        return 0;
    case WM_SETTINGCHANGE: {
        bool forceRedraw = false;
        if ((wParam == 0) && (lParam != 0) // lParam sometimes may be NULL.
            && (std::wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0)) {
            const bool dark = ShouldAppsUseDarkMode();
            if (g_darkModeEnabled != dark) {
                g_darkModeEnabled = dark;
                UpdateWindowTheme();
                UpdateBrushAppearance();
                forceRedraw = true;
                std::wcout << L"The system theme has changed. Current theme: "
                           << (g_darkModeEnabled ? L"dark" : L"light") << std::endl;
            }
        }
        if (wParam == SPI_SETDESKWALLPAPER) {
            const std::wstring filePath = GetWallpaperFilePath();
            if (g_wallpaperFilePath != filePath) {
                g_wallpaperFilePath = filePath;
                UpdateWallpaperBitmap();
                forceRedraw = true;
                std::wcout << L"The desktop wallpaper has changed. Current wallpaper file path: "
                           << g_wallpaperFilePath << std::endl;
            }
        }
        if (forceRedraw) {
            D2DDraw(true);
        }
    } break;
    case WM_WINDOWPOSCHANGED:
        D2DDraw(false);
        break;
    case WM_DPICHANGED: {
        const UINT dpiX = LOWORD(wParam);
        const UINT dpiY = HIWORD(wParam);
        if ((g_dpi != dpiX) || (g_dpi != dpiY)) {
            g_dpi = dpiX;
            const auto rect = reinterpret_cast<LPRECT>(lParam);
            SetWindowPos(hWnd, nullptr, rect->left, rect->top, rect->right - rect->left,
                rect->bottom - rect->top, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
            std::wcout << L"The window DPI has changed. Current DPI: " << g_dpi << std::endl;
            return 0;
        }
    } break;
    case WM_NCCALCSIZE: {
        // Remember to call the default window procedure first, otherwise
        // the window frame (including the title bar) will gone!
        DefWindowProcW(hWnd, WM_NCCALCSIZE, wParam, lParam);
        // Dirty hack to workaround the resize flicker caused by DWM.
        LARGE_INTEGER freq = {};
        QueryPerformanceFrequency(&freq);
        TIMECAPS tc = {};
        timeGetDevCaps(&tc, sizeof(tc));
        const UINT ms_granularity = tc.wPeriodMin;
        timeBeginPeriod(ms_granularity);
        LARGE_INTEGER now0 = {};
        QueryPerformanceCounter(&now0);
        // ask DWM where the vertical blank falls
        DWM_TIMING_INFO dti;
        SecureZeroMemory(&dti, sizeof(dti));
        dti.cbSize = sizeof(dti);
        DwmGetCompositionTimingInfo(nullptr, &dti);
        LARGE_INTEGER now1 = {};
        QueryPerformanceCounter(&now1);
        // - DWM told us about SOME vertical blank
        //   - past or future, possibly many frames away
        // - convert that into the NEXT vertical blank
        const LONGLONG period = dti.qpcRefreshPeriod;
        const LONGLONG dt = dti.qpcVBlank - now1.QuadPart;
        LONGLONG w = 0, m = 0;
        if (dt >= 0) {
            w = dt / period;
        } else {
            // reach back to previous period
            // - so m represents consistent position within phase
            w = -1 + dt / period;
        }
        m = dt - (period * w);
        const double m_ms = (double(1000) * double(m) / double(freq.QuadPart));
        Sleep(DWORD(std::round(m_ms)));
        timeEndPeriod(ms_granularity);
        // Return WVR_REDRAW can make the window resizing look less broken.
        return WVR_REDRAW;
    }
    case WM_WINDOWPOSCHANGING: {
        // Tell Windows to discard the entire contents of the client area, as re-using
        // parts of the client area would lead to jitter during resize.
        const auto windowPos = reinterpret_cast<LPWINDOWPOS>(lParam);
        windowPos->flags |= SWP_NOCOPYBITS;
    } break;
    case WM_ERASEBKGND:
        return 1; // Return non-zero to avoid flickering during window resizing.
    case WM_CLOSE: {
        DestroyWindow(hWnd);
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

EXTERN_C int WINAPI wWinMain
    (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_hInstance = hInstance;

    std::setlocale(LC_ALL, "en_US.UTF-8");

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    g_darkModeEnabled = ShouldAppsUseDarkMode();
    g_wallpaperFilePath = GetWallpaperFilePath();

    std::wcout << L"Current system theme: " << (g_darkModeEnabled ? L"dark" : L"light") << std::endl;
    std::wcout << L"Current desktop wallpaper file path: " << g_wallpaperFilePath << std::endl;

    WNDCLASSEXW wcex;
    SecureZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = g_hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    RegisterClassExW(&wcex);

    g_hWnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, WINDOW_CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, g_hInstance, nullptr);

    g_dpi = GetDpiForWindow(g_hWnd);

    std::wcout << L"Current window DPI: " << g_dpi << std::endl;

    UpdateWindowTheme();

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(g_D2DFactory.GetAddressOf()));
    // This array defines the set of DirectX hardware feature levels this app supports.
    // The ordering is important and you should preserve it.
    // Don't forget to declare your app's minimum required feature level in its
    // description. All apps are assumed to support 9.1 unless otherwise stated.
    static constexpr const D3D_FEATURE_LEVEL featureLevels[] = {
        //D3D_FEATURE_LEVEL_12_2, // Requires at least Windows 11.
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1
    };
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, g_D3D11Device.GetAddressOf(),
        &g_D3DFeatureLevel, g_D3D11Context.GetAddressOf());
    g_D3D11Device.As(&g_DXGIDevice);
    g_D2DFactory->CreateDevice(g_DXGIDevice.Get(), g_D2DDevice.GetAddressOf());
    g_D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, g_D2DContext.GetAddressOf());
    SecureZeroMemory(&g_DXGISwapChainDesc, sizeof(g_DXGISwapChainDesc));
    g_DXGISwapChainDesc.Width = GetSystemMetricsForDpi(SM_CXSCREEN, g_dpi);
    g_DXGISwapChainDesc.Height = GetSystemMetricsForDpi(SM_CYSCREEN, g_dpi);
    g_DXGISwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    g_DXGISwapChainDesc.SampleDesc.Count = 1;
    g_DXGISwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    g_DXGISwapChainDesc.BufferCount = 2;
    g_DXGISwapChainDesc.Scaling = DXGI_SCALING_NONE;
    g_DXGISwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    g_DXGISwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    g_DXGIDevice->GetAdapter(g_DXGIAdapter.GetAddressOf());
    g_DXGIAdapter->GetParent(IID_PPV_ARGS(g_DXGIFactory.GetAddressOf()));
    g_DXGIFactory->CreateSwapChainForHwnd(g_D3D11Device.Get(), g_hWnd,
        &g_DXGISwapChainDesc, nullptr, nullptr, g_DXGISwapChain.GetAddressOf());
    g_DXGIDevice->SetMaximumFrameLatency(1);
    g_DXGISwapChain->GetBuffer(0, IID_PPV_ARGS(g_D3D11Texture.GetAddressOf()));
    SecureZeroMemory(&g_D2DBitmapProperties, sizeof(g_D2DBitmapProperties));
    g_D2DBitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        float(g_dpi), float(g_dpi));
    g_DXGISwapChain->GetBuffer(0, IID_PPV_ARGS(g_DXGISurface.GetAddressOf()));
    g_D2DContext->CreateBitmapFromDxgiSurface(g_DXGISurface.Get(),
        &g_D2DBitmapProperties, g_D2DTargetBitmap.GetAddressOf());
    g_D2DContext->SetTarget(g_D2DTargetBitmap.Get());

    CoCreateInstance(CLSID_WICImagingFactory2, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(g_WICFactory.GetAddressOf()));

    g_D2DContext->CreateEffect(CLSID_D2D1BitmapSource, g_D2DWallpaperBitmapSourceEffect.GetAddressOf());
    UpdateWallpaperBitmap();

    const HRSRC noiseResourceHandle = FindResourceW(g_hInstance, MAKEINTRESOURCEW(IDB_NOISE_BITMAP), L"PNG");
    const DWORD noiseResourceDataSize = SizeofResource(g_hInstance, noiseResourceHandle);
    const HGLOBAL noiseResourceDataHandle = LoadResource(g_hInstance, noiseResourceHandle);
    const LPVOID noiseResourceData = LockResource(noiseResourceDataHandle);

    g_WICFactory->CreateStream(g_WICNoiseStream.GetAddressOf());
    g_WICNoiseStream->InitializeFromMemory(static_cast<WICInProcPointer>(noiseResourceData), noiseResourceDataSize);
    g_WICFactory->CreateDecoderFromStream(g_WICNoiseStream.Get(), nullptr,
        WICDecodeMetadataCacheOnLoad, &g_WICNoiseDecoder);
    g_WICNoiseDecoder->GetFrame(0, &g_WICNoiseFrame);
    g_WICFactory->CreateFormatConverter(&g_WICNoiseConverter);
    g_WICNoiseConverter->Initialize(g_WICNoiseFrame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
    g_D2DContext->CreateEffect(CLSID_D2D1BitmapSource, g_D2DNoiseBitmapSourceEffect.GetAddressOf());
    g_D2DNoiseBitmapSourceEffect->SetValue(D2D1_BITMAPSOURCE_PROP_WIC_BITMAP_SOURCE, g_WICNoiseConverter.Get());

    g_D2DContext->CreateEffect(CLSID_D2D1Flood, g_D2DTintColorEffect.GetAddressOf());
    g_D2DContext->CreateEffect(CLSID_D2D1Opacity, g_D2DTintOpacityEffect.GetAddressOf());
    g_D2DTintOpacityEffect->SetInputEffect(0, g_D2DTintColorEffect.Get());
    g_D2DContext->CreateEffect(CLSID_D2D1GaussianBlur, g_D2DGaussianBlurEffect.GetAddressOf());
    g_D2DGaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    g_D2DGaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_DIRECTIONALBLUR_OPTIMIZATION_SPEED);
    g_D2DGaussianBlurEffect->SetInputEffect(0, g_D2DWallpaperBitmapSourceEffect.Get());

    // Apply luminosity:

    // Luminosity Color
    g_D2DContext->CreateEffect(CLSID_D2D1Flood, g_D2DLuminosityColorEffect.GetAddressOf());
    g_D2DContext->CreateEffect(CLSID_D2D1Opacity, g_D2DLuminosityOpacityEffect.GetAddressOf());
    g_D2DLuminosityOpacityEffect->SetInputEffect(0, g_D2DLuminosityColorEffect.Get());

    // Luminosity blend
    g_D2DContext->CreateEffect(CLSID_D2D1Blend, g_D2DLuminosityBlendEffect.GetAddressOf());
    g_D2DLuminosityBlendEffect->SetValue(D2D1_BLEND_PROP_MODE, D2D1_BLEND_MODE_LUMINOSITY);
    g_D2DLuminosityBlendEffect->SetInputEffect(0, g_D2DGaussianBlurEffect.Get());
    g_D2DLuminosityBlendEffect->SetInputEffect(1, g_D2DLuminosityOpacityEffect.Get());

    // Apply tint:

    // Color blend
    g_D2DContext->CreateEffect(CLSID_D2D1Blend, g_D2DLuminosityColorBlendEffect.GetAddressOf());
    g_D2DLuminosityColorBlendEffect->SetValue(D2D1_BLEND_PROP_MODE, D2D1_BLEND_MODE_COLOR);
    g_D2DLuminosityColorBlendEffect->SetInputEffect(0, g_D2DLuminosityBlendEffect.Get());
    g_D2DLuminosityColorBlendEffect->SetInputEffect(1, g_D2DTintOpacityEffect.Get());

    // Create noise with alpha and wrap:
    // Noise image BorderEffect (infinitely tiles noise image)
    g_D2DContext->CreateEffect(CLSID_D2D1Border, g_D2DNoiseBorderEffect.GetAddressOf());
    g_D2DNoiseBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_X, D2D1_BORDER_EDGE_MODE_WRAP);
    g_D2DNoiseBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_Y, D2D1_BORDER_EDGE_MODE_WRAP);
    g_D2DNoiseBorderEffect->SetInputEffect(0, g_D2DNoiseBitmapSourceEffect.Get());
    // OpacityEffect applied to wrapped noise
    g_D2DContext->CreateEffect(CLSID_D2D1Opacity, g_D2DNoiseOpacityEffect.GetAddressOf());
    g_D2DNoiseOpacityEffect->SetInputEffect(0, g_D2DNoiseBorderEffect.Get());

    // Blend noise on top of tint
    g_D2DContext->CreateEffect(CLSID_D2D1Composite, g_D2DNoiseBlendEffectOuter.GetAddressOf());
    g_D2DNoiseBlendEffectOuter->SetValue(D2D1_COMPOSITE_PROP_MODE, D2D1_COMPOSITE_MODE_SOURCE_OVER);
    g_D2DNoiseBlendEffectOuter->SetInputEffect(0, g_D2DLuminosityColorBlendEffect.Get());
    g_D2DNoiseBlendEffectOuter->SetInputEffect(1, g_D2DNoiseOpacityEffect.Get());

    //
    g_D2DGaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sc_blurRadius);
    g_D2DNoiseOpacityEffect->SetValue(D2D1_OPACITY_PROP_OPACITY, sc_noiseOpacity);
    UpdateBrushAppearance();

    g_D2DFinalBrushEffect = g_D2DNoiseBlendEffectOuter;

    ShowWindow(g_hWnd, nShowCmd);
    UpdateWindow(g_hWnd);

    MSG message = {};

    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    DestroyWindow(g_hWnd);
    UnregisterClassW(WINDOW_CLASS_NAME, g_hInstance);
    CoUninitialize();

    return static_cast<int>(message.wParam);
}
