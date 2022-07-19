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
#include <dwmapi.h>
#include <wrl\client.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <d2d1_2.h>
#include <d2d1effects_2.h>
#include <wincodec.h>
#include <clocale>
#include <iostream>
#include "resource.h"

static constexpr const D2D1_COLOR_F sc_defaultTintColor = { 1.0f, 1.0f, 1.0f, 0.8f };
static constexpr const float sc_defaultTintOpacity = 1.0f;
static constexpr const float sc_blurRadius = 30.0f;
static constexpr const float sc_noiseOpacity = 0.02f;
static constexpr const D2D1_COLOR_F sc_exclusionColor = { 1.0f, 1.0f, 1.0f, 0.1f };
static constexpr const float sc_saturation = 1.25f;
static constexpr const D2D1_COLOR_F sc_darkThemeColor = { 0.125f, 0.125f, 0.125f, 1.0f };
static constexpr const float sc_darkThemeTintOpacity = 0.8f;
static constexpr const D2D1_COLOR_F sc_lightThemeColor = { 0.953f, 0.953f, 0.953f, 1.0f };
static constexpr const float sc_lightThemeTintOpacity = 0.5f;

static constexpr const wchar_t WINDOW_CLASS_NAME[] = L"org.wangwenx190.d2dmica.windowclass\0";
static constexpr const wchar_t WINDOW_TITLE[] = L"Direct2D Mica Material\0";

static HINSTANCE g_hInstance = nullptr;
static HWND g_hWnd = nullptr;
static UINT g_dpi = 0;
static std::wstring g_wallpaperFilePath = {};
static bool g_darkModeEnabled = false;

static Microsoft::WRL::ComPtr<ID2D1Factory2> g_D2DFactory = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Device1> g_D2DDevice = nullptr;
static Microsoft::WRL::ComPtr<ID2D1DeviceContext1> g_D2DContext = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Bitmap1> g_D2DTargetBitmap = nullptr;
static D2D1_BITMAP_PROPERTIES1 g_D2DBitmapProperties = {};

static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DWallpaperBitmapSourceEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DNoiseBitmapSourceEffect = nullptr;

static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DTintColorEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DFallbackColorEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DLuminosityColorEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DLuminosityBlendEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DLuminosityColorBlendEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DGaussianBlurEffect = nullptr;

static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DNoiseBorderEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DNoiseOpacityEffect = nullptr;
static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DNoiseBlendEffectOuter = nullptr;

static Microsoft::WRL::ComPtr<ID2D1Effect> g_D2DFadeInOutEffect = nullptr;
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
    return false;
}

static inline void UpdateBrushAppearance()
{
    g_D2DLuminosityColorEffect->SetValue(D2D1_FLOOD_PROP_COLOR, WINRTCOLOR_TO_D2DCOLOR4F(q_ptr->GetEffectiveLuminosityColor()));
    g_D2DTintColorEffect->SetValue(D2D1_FLOOD_PROP_COLOR, WINRTCOLOR_TO_D2DCOLOR4F(q_ptr->GetEffectiveTintColor()));
    g_D2DFallbackColorEffect->SetValue(D2D1_FLOOD_PROP_COLOR, WINRTCOLOR_TO_D2DCOLOR4F(q_ptr->GetFallbackColor()));
}

[[nodiscard]] static inline LRESULT CALLBACK WndProc
    (const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg) {
    case WM_PAINT: {
        g_D2DContext->BeginDraw();
        //g_D2DContext->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)); // todo: check: fully transparent color.
        g_D2DContext->DrawImage(g_D2DFinalBrushEffect.Get());
        g_D2DContext->EndDraw();
        DXGI_PRESENT_PARAMETERS params;
        SecureZeroMemory(&params, sizeof(params));
        // Without this step, nothing will be visible to the user.
        g_DXGISwapChain->Present1(1, 0, &params);
        // Try to reduce flicker as much as possible.
        DwmFlush();
        return 0;
    }
    case WM_SETTINGCHANGE: {
        if ((wParam == 0) && (lParam != 0) // lParam sometimes may be NULL.
            && (std::wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0)) {
            UpdateBrushAppearance();
        }
        if (wParam == SPI_SETDESKWALLPAPER) {
        }
    } break;
    case WM_DPICHANGED: {
        g_dpi = LOWORD(wParam);
        const auto rect = reinterpret_cast<LPRECT>(lParam);
        SetWindowPos(hWnd, nullptr, rect->left, rect->top, rect->right - rect->left,
            rect->bottom - rect->top, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        return 0;
    }
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

    g_wallpaperFilePath = GetWallpaperFilePath();

    WNDCLASSEXW wcex;
    SecureZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = g_hInstance;
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    RegisterClassExW(&wcex);

    g_hWnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, WINDOW_CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, g_hInstance, nullptr);

    g_dpi = GetDpiForWindow(g_hWnd);

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(g_D2DFactory.GetAddressOf()));
    // This array defines the set of DirectX hardware feature levels this app supports.
    // The ordering is important and you should preserve it.
    // Don't forget to declare your app's minimum required feature level in its
    // description. All apps are assumed to support 9.1 unless otherwise stated.
    static constexpr const D3D_FEATURE_LEVEL featureLevels[] = {
        //D3D_FEATURE_LEVEL_12_2,
        //D3D_FEATURE_LEVEL_12_1,
        //D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1
    };
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, g_D3D11Device.GetAddressOf(),
        &g_D3DFeatureLevel, g_D3D11Context.GetAddressOf());
    g_D3D11Device.As(&g_DXGIDevice);
    g_D2DFactory->CreateDevice(g_DXGIDevice.Get(), g_D2DDevice.GetAddressOf());
    g_D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, g_D2DContext.GetAddressOf());
    SecureZeroMemory(&g_DXGISwapChainDesc, sizeof(g_DXGISwapChainDesc));
    g_DXGISwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    g_DXGISwapChainDesc.SampleDesc.Count = 1;
    g_DXGISwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    g_DXGISwapChainDesc.BufferCount = 2;
    g_DXGISwapChainDesc.Scaling = DXGI_SCALING_NONE;
    g_DXGISwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    g_DXGIDevice->GetAdapter(g_DXGIAdapter.GetAddressOf());
    g_DXGIAdapter->GetParent(IID_PPV_ARGS(g_DXGIFactory.GetAddressOf()));
    g_DXGIFactory->CreateSwapChainForHwnd(g_D3D11Device.Get(), g_hWnd,
        &g_DXGISwapChainDesc, nullptr, nullptr, g_DXGISwapChain.GetAddressOf());
    g_DXGIDevice->SetMaximumFrameLatency(1);
    g_DXGISwapChain->GetBuffer(0, IID_PPV_ARGS(g_D3D11Texture.GetAddressOf()));
    SecureZeroMemory(&g_D2DBitmapProperties, sizeof(g_D2DBitmapProperties));
    const auto dpiF = float(g_dpi);
    g_D2DBitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiF, dpiF);
    g_DXGISwapChain->GetBuffer(0, IID_PPV_ARGS(g_DXGISurface.GetAddressOf()));
    g_D2DContext->CreateBitmapFromDxgiSurface(g_DXGISurface.Get(),
        &g_D2DBitmapProperties, g_D2DTargetBitmap.GetAddressOf());
    g_D2DContext->SetTarget(g_D2DTargetBitmap.Get());

    CoCreateInstance(CLSID_WICImagingFactory2, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(g_WICFactory.GetAddressOf()));

    g_WICFactory->CreateDecoderFromFilename(g_wallpaperFilePath.c_str(),
        nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &g_WICWallpaperDecoder);
    g_WICWallpaperDecoder->GetFrame(0, &g_WICWallpaperFrame);
    g_WICFactory->CreateFormatConverter(&g_WICWallpaperConverter);
    g_WICWallpaperConverter->Initialize(g_WICWallpaperFrame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
    g_D2DContext->CreateEffect(CLSID_D2D1BitmapSource, g_D2DWallpaperBitmapSourceEffect.GetAddressOf());
    g_D2DWallpaperBitmapSourceEffect->SetValue(D2D1_BITMAPSOURCE_PROP_WIC_BITMAP_SOURCE, g_WICWallpaperConverter.Get());

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
    g_D2DContext->CreateEffect(CLSID_D2D1GaussianBlur, g_D2DGaussianBlurEffect.GetAddressOf());
    g_D2DGaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    g_D2DGaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_DIRECTIONALBLUR_OPTIMIZATION_QUALITY);
    g_D2DGaussianBlurEffect->SetInputEffect(0, g_D2DWallpaperBitmapSourceEffect.Get());

    // Apply luminosity:

    // Luminosity Color
    g_D2DContext->CreateEffect(CLSID_D2D1Flood, g_D2DLuminosityColorEffect.GetAddressOf());

    // Luminosity blend
    g_D2DContext->CreateEffect(CLSID_D2D1Blend, g_D2DLuminosityBlendEffect.GetAddressOf());
    g_D2DLuminosityBlendEffect->SetValue(D2D1_BLEND_PROP_MODE, D2D1_BLEND_MODE_LUMINOSITY);
    g_D2DLuminosityBlendEffect->SetInputEffect(0, g_D2DGaussianBlurEffect.Get());
    g_D2DLuminosityBlendEffect->SetInputEffect(1, g_D2DLuminosityColorEffect.Get());

    // Apply tint:

    // Color blend
    g_D2DContext->CreateEffect(CLSID_D2D1Blend, g_D2DLuminosityColorBlendEffect.GetAddressOf());
    g_D2DLuminosityColorBlendEffect->SetValue(D2D1_BLEND_PROP_MODE, D2D1_BLEND_MODE_COLOR);
    g_D2DLuminosityColorBlendEffect->SetInputEffect(0, g_D2DLuminosityBlendEffect.Get());
    g_D2DLuminosityColorBlendEffect->SetInputEffect(1, g_D2DTintColorEffect.Get());

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

    // Fallback color
    g_D2DContext->CreateEffect(CLSID_D2D1Flood, g_D2DFallbackColorEffect.GetAddressOf());

    // CrossFade with the fallback color. Weight = 0 means full fallback, 1 means full acrylic.
    g_D2DContext->CreateEffect(CLSID_D2D1CrossFade, g_D2DFadeInOutEffect.GetAddressOf());
    g_D2DFadeInOutEffect->SetValue(D2D1_CROSSFADE_PROP_WEIGHT, 1.0f);
    g_D2DFadeInOutEffect->SetInputEffect(0, g_D2DFallbackColorEffect.Get());
    g_D2DFadeInOutEffect->SetInputEffect(1, g_D2DNoiseBlendEffectOuter.Get());

    //
    g_D2DGaussianBlurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, sc_blurRadius);
    g_D2DNoiseOpacityEffect->SetValue(D2D1_OPACITY_PROP_OPACITY, sc_noiseOpacity);
    UpdateBrushAppearance();

    g_D2DFinalBrushEffect = g_D2DFadeInOutEffect;

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
