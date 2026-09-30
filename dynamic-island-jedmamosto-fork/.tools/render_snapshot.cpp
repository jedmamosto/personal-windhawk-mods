#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <wincodec.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <iostream>
#include <vector>
#include <string>

// Include isolated battery dashboard header
#include "../battery_dashboard.hpp"

using Microsoft::WRL::ComPtr;

int wmain(int argc, wchar_t* argv[]) {
    (void)argc;
    (void)argv;

    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::wcerr << L"[ERROR] Failed to initialize COM: 0x" << std::hex << hr << std::endl;
        return 1;
    }

    const UINT width = 380;
    const UINT height = 180;
    const wchar_t* outFilename = L"snapshot_battery.png";

    // 1. Create WIC Imaging Factory
    ComPtr<IWICImagingFactory> wicFactory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr) || !wicFactory) {
        std::wcerr << L"[ERROR] Failed to create WICImagingFactory: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 2. Create WIC Memory Bitmap
    ComPtr<IWICBitmap> wicBitmap;
    hr = wicFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &wicBitmap);
    if (FAILED(hr) || !wicBitmap) {
        std::wcerr << L"[ERROR] Failed to create WICBitmap: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 3. Create Direct2D Factory
    ComPtr<ID2D1Factory> d2dFactory;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory.GetAddressOf());
    if (FAILED(hr) || !d2dFactory) {
        std::wcerr << L"[ERROR] Failed to create D2D1Factory: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 4. Create Direct2D WIC Render Target
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1RenderTarget> renderTarget;
    hr = d2dFactory->CreateWicBitmapRenderTarget(wicBitmap.Get(), props, &renderTarget);
    if (FAILED(hr) || !renderTarget) {
        std::wcerr << L"[ERROR] Failed to create WicBitmapRenderTarget: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 5. Create DirectWrite Factory & Formats
    ComPtr<IDWriteFactory> dwriteFactory;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &dwriteFactory);
    if (FAILED(hr) || !dwriteFactory) {
        std::wcerr << L"[ERROR] Failed to create DWriteFactory: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    ComPtr<IDWriteTextFormat> boldTextFormat;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &boldTextFormat);

    ComPtr<IDWriteTextFormat> textFormat;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &textFormat);

    ComPtr<IDWriteTextFormat> smallTextFormat;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 9.0f, L"en-us", &smallTextFormat);

    ComPtr<IDWriteTextFormat> iconFormat;
    dwriteFactory->CreateTextFormat(L"Segoe Fluent Icons", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"en-us", &iconFormat);

    // 6. Setup Sample Battery & Peripheral State
    battery_bento::BentoBatteryData data;
    data.percent = 78;
    data.charging = true;
    data.powerRateWatts = 65.0f;
    data.secondsToFull = 2100; // 35m
    data.healthPercent = 92;
    data.powerSchemeName = L"Silent Mode (G-Helper)";

    battery_bento::BentoAccessory earbuds;
    earbuds.name = L"Galaxy Buds2";
    earbuds.category = battery_bento::BentoDeviceCategory::Headphones;
    earbuds.batteryPercent = 85;
    earbuds.connected = true;
    data.accessories.push_back(earbuds);

    // 7. Render Battery Bento to WIC Memory Target
    renderTarget->BeginDraw();
    renderTarget->Clear(D2D1::ColorF(0.07f, 0.07f, 0.08f, 1.0f)); // Clean dark container preview

    const D2D1_RECT_F islandRect = D2D1::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    battery_bento::DrawBatteryBentoGrid(
        renderTarget.Get(),
        d2dFactory.Get(),
        boldTextFormat.Get(),
        textFormat.Get(),
        smallTextFormat.Get(),
        iconFormat.Get(),
        islandRect,
        1.0f,
        data,
        battery_bento::tokens::kCardBase,
        1.0f
    );

    hr = renderTarget->EndDraw();
    if (FAILED(hr)) {
        std::wcerr << L"[ERROR] Direct2D EndDraw failed: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 8. Encode to PNG via WIC Stream
    ComPtr<IWICStream> stream;
    hr = wicFactory->CreateStream(&stream);
    if (FAILED(hr) || !stream) {
        std::wcerr << L"[ERROR] Failed to create WIC Stream: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    hr = stream->InitializeFromFilename(outFilename, GENERIC_WRITE);
    if (FAILED(hr)) {
        std::wcerr << L"[ERROR] Failed to initialize stream for " << outFilename << L": 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    ComPtr<IWICBitmapEncoder> encoder;
    hr = wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr) || !encoder) {
        std::wcerr << L"[ERROR] Failed to create PNG Encoder: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        std::wcerr << L"[ERROR] Failed to initialize encoder: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    ComPtr<IWICBitmapFrameEncode> frame;
    hr = encoder->CreateNewFrame(&frame, nullptr);
    if (FAILED(hr) || !frame) {
        std::wcerr << L"[ERROR] Failed to create encoder frame: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    hr = frame->Initialize(nullptr);
    hr = frame->SetSize(width, height);
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppPBGRA;
    hr = frame->SetPixelFormat(&pixelFormat);
    hr = frame->WriteSource(wicBitmap.Get(), nullptr);
    hr = frame->Commit();
    hr = encoder->Commit();

    if (SUCCEEDED(hr)) {
        std::wcout << L"[SUCCESS] Rendered Direct2D snapshot to: " << outFilename << std::endl;
    } else {
        std::wcerr << L"[ERROR] Failed to commit PNG frame: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    CoUninitialize();
    return 0;
}
