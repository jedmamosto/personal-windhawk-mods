#pragma once

#ifndef BATTERY_DASHBOARD_HPP
#define BATTERY_DASHBOARD_HPP

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
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>

namespace battery_bento {

using Microsoft::WRL::ComPtr;

// ============================================================================
// Apple System Design Tokens & Color Palette
// ============================================================================

namespace tokens {
    // Apple System Green (#34C759 / rgb(52, 199, 89)) - Canonical charging & healthy state
    constexpr D2D1_COLOR_F kAppleGreen = { 52.0f / 255.0f, 199.0f / 255.0f, 89.0f / 255.0f, 1.0f };

    // Apple System Amber (#FF9500 / rgb(255, 149, 0)) - Low battery warning (<= 20%)
    constexpr D2D1_COLOR_F kAppleAmber = { 255.0f / 255.0f, 149.0f / 255.0f, 0.0f / 255.0f, 1.0f };

    // Apple System Red (#FF3B30 / rgb(255, 59, 48)) - Critical battery warning (<= 10%)
    constexpr D2D1_COLOR_F kAppleRed = { 255.0f / 255.0f, 59.0f / 255.0f, 48.0f / 255.0f, 1.0f };

    // Apple Dark Card Surface: Subdued translucent glass depth
    constexpr D2D1_COLOR_F kCardBase = { 1.0f, 1.0f, 1.0f, 0.06f };
    constexpr D2D1_COLOR_F kCardBaseRaised = { 1.0f, 1.0f, 1.0f, 0.09f };

    // Layered Depth Hairline Border: 1px subtle separation without dark drop shadows
    constexpr D2D1_COLOR_F kCardHairline = { 1.0f, 1.0f, 1.0f, 0.08f };
    constexpr D2D1_COLOR_F kDividerHairline = { 1.0f, 1.0f, 1.0f, 0.06f };

    // Level Bar Background Track
    constexpr D2D1_COLOR_F kTrackBackground = { 1.0f, 1.0f, 1.0f, 0.10f };

    // Typographic Color Tokens (WCAG 2.1 AA Compliant on dark surfaces)
    constexpr D2D1_COLOR_F kTextPrimary = { 1.0f, 1.0f, 1.0f, 0.96f };
    constexpr D2D1_COLOR_F kTextSecondary = { 1.0f, 1.0f, 1.0f, 0.60f };
    constexpr D2D1_COLOR_F kTextTertiary = { 1.0f, 1.0f, 1.0f, 0.35f };
}

// ============================================================================
// Data Structures for Battery & Connected Peripherals
// ============================================================================

enum class BentoDeviceCategory {
    Generic = 0,
    Headphones,
    Speaker,
    Mouse,
    Keyboard,
    Phone,
};

struct BentoAccessory {
    std::wstring name;
    BentoDeviceCategory category = BentoDeviceCategory::Generic;
    int batteryPercent = -1; // -1 = unknown / unavailable
    bool connected = true;
};

struct BentoBatteryData {
    int percent = 100;
    bool charging = false;
    bool low = false;
    float powerRateWatts = 0.0f;
    int secondsRemaining = -1;
    int secondsToFull = -1;
    int healthPercent = -1;  // DesignedCapacity vs FullChargeCapacity
    int cycleCount = -1;
    std::wstring powerSchemeName = L"Balanced";
    std::vector<BentoAccessory> accessories;
};

// ============================================================================
// Math & Utility Helpers
// ============================================================================

template <typename T>
inline constexpr T BentoClamp(T val, T minVal, T maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

inline D2D1_COLOR_F BentoWithAlpha(const D2D1_COLOR_F& c, float a) {
    return D2D1::ColorF(c.r, c.g, c.b, BentoClamp(a, 0.0f, 1.0f));
}

inline const wchar_t* GetCategoryGlyph(BentoDeviceCategory category) {
    switch (category) {
        case BentoDeviceCategory::Headphones:
            return L"\uE7F6"; // Segoe Headphones
        case BentoDeviceCategory::Speaker:
            return L"\uE7F5"; // Segoe Speaker
        case BentoDeviceCategory::Mouse:
            return L"\uE962"; // Segoe Mouse
        case BentoDeviceCategory::Keyboard:
            return L"\uE92E"; // Segoe Keyboard
        case BentoDeviceCategory::Phone:
            return L"\uE8EA"; // Segoe CellPhone
        case BentoDeviceCategory::Generic:
        default:
            return L"\uE702"; // Segoe Bluetooth standard glyph
    }
}

// ============================================================================
// Direct2D Drawing Primitives
// ============================================================================

// Draws a sleek Apple-style rounded card with a 1px hairline border
inline void DrawBentoCard(
    ID2D1RenderTarget* target,
    D2D1_RECT_F rect,
    float radius,
    D2D1_COLOR_F fillColor,
    float opacity = 1.0f)
{
    if (!target) return;

    // Fill surface
    ComPtr<ID2D1SolidColorBrush> fillBrush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            BentoWithAlpha(fillColor, fillColor.a * opacity), &fillBrush)) && fillBrush) {
        target->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), fillBrush.Get());
    }

    // Hairline outline (Layered Depth Hairline Invariant)
    ComPtr<ID2D1SolidColorBrush> borderBrush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            BentoWithAlpha(tokens::kCardHairline, tokens::kCardHairline.a * opacity), &borderBrush)) && borderBrush) {
        target->DrawRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), borderBrush.Get(), 1.0f);
    }
}

// Draws a hardware-accelerated Direct2D lightning bolt path centered at point `c`
inline void DrawLightningBolt(
    ID2D1RenderTarget* target,
    ID2D1Factory* factory,
    D2D1_POINT_2F c,
    float s,
    ID2D1Brush* brush)
{
    if (!target || !brush) return;

    ComPtr<ID2D1Factory> d2dFactory = factory;
    if (!d2dFactory) {
        target->GetFactory(&d2dFactory);
    }
    if (!d2dFactory) return;

    ComPtr<ID2D1PathGeometry> bolt;
    if (FAILED(d2dFactory->CreatePathGeometry(&bolt)) || !bolt) return;

    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(bolt->Open(&sink)) || !sink) return;

    // Dedicated bold 15px lightning bolt glyph
    sink->BeginFigure(D2D1::Point2F(c.x + 0.08f * s, c.y - 0.42f * s), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(c.x - 0.24f * s, c.y + 0.02f * s));
    sink->AddLine(D2D1::Point2F(c.x - 0.02f * s, c.y + 0.02f * s));
    sink->AddLine(D2D1::Point2F(c.x - 0.12f * s, c.y + 0.42f * s));
    sink->AddLine(D2D1::Point2F(c.x + 0.22f * s, c.y - 0.02f * s));
    sink->AddLine(D2D1::Point2F(c.x + 0.02f * s, c.y - 0.02f * s));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    target->FillGeometry(bolt.Get(), brush);
}

// ============================================================================
// 2x3 Battery & Power Dashboard Grid (Mirroring Hardware Monitor)
// ============================================================================

enum class BatteryGlyphKind {
    Battery = 0,
    Peripheral,
    PowerFlow,
    Time,
    PowerMode,
    Health
};

struct BatteryGridCard {
    BatteryGlyphKind glyphKind = BatteryGlyphKind::Battery;
    BentoDeviceCategory accessoryCategory = BentoDeviceCategory::Generic;
    std::wstring label;
    std::wstring value;
    float fraction = -1.0f; // negative means no load bar
    D2D1_COLOR_F tint = tokens::kAppleGreen;
};

inline void DrawGlyphIcon(
    ID2D1RenderTarget* target,
    ID2D1Factory* factory,
    IDWriteTextFormat* iconFormat,
    const BatteryGridCard& card,
    const BentoBatteryData& data,
    D2D1_POINT_2F c,
    float s,
    ID2D1Brush* brush,
    float scale)
{
    if (!target || !brush) return;

    switch (card.glyphKind) {
        case BatteryGlyphKind::Battery: {
            // Card 1: Always draws a distinct 15px battery silhouette (with body, terminal cap, and level), not a lightning bolt
            (void)data;
            const D2D1_RECT_F body = D2D1::RectF(c.x - 0.38f * s, c.y - 0.22f * s, c.x + 0.26f * s, c.y + 0.22f * s);
            target->DrawRoundedRectangle(D2D1::RoundedRect(body, 1.5f * scale, 1.5f * scale), brush, 1.1f);
            target->FillRectangle(D2D1::RectF(c.x + 0.26f * s, c.y - 0.08f * s, c.x + 0.35f * s, c.y + 0.08f * s), brush);
            if (card.fraction > 0.05f) {
                const float fillW = (body.right - 1.5f * scale) - (body.left + 1.5f * scale);
                const D2D1_RECT_F inner = D2D1::RectF(
                    body.left + 1.5f * scale, body.top + 1.5f * scale,
                    body.left + 1.5f * scale + fillW * BentoClamp(card.fraction, 0.0f, 1.0f), body.bottom - 1.5f * scale);
                target->FillRectangle(inner, brush);
            }
            break;
        }
        case BatteryGlyphKind::Peripheral: {
            if (iconFormat) {
                D2D1_MATRIX_3X2_F oldTransform;
                target->GetTransform(&oldTransform);
                // Card 2 (Bluetooth / Peripheral): Scaled to match the 15px visual bounding box of other glyphs (no oversized vertical stretch)
                const float iconScale = (s * 0.72f) / 16.0f;
                target->SetTransform(D2D1::Matrix3x2F::Scale(iconScale, iconScale, c) * oldTransform);

                iconFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                iconFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                const wchar_t* glyph = GetCategoryGlyph(card.accessoryCategory);
                const D2D1_RECT_F r = D2D1::RectF(c.x - 8.0f, c.y - 8.0f, c.x + 8.0f, c.y + 8.0f);
                target->DrawTextW(glyph, static_cast<UINT32>(wcslen(glyph)), iconFormat, r, brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
                iconFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                iconFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

                target->SetTransform(oldTransform);
            } else {
                // Vector fallback matching 15px bounding box
                const float hw = 0.20f * s;
                const float hh = 0.36f * s;
                target->DrawLine(D2D1::Point2F(c.x, c.y - hh), D2D1::Point2F(c.x, c.y + hh), brush, 1.1f * scale);
                target->DrawLine(D2D1::Point2F(c.x - hw, c.y - hh * 0.45f), D2D1::Point2F(c.x + hw, c.y + hh * 0.45f), brush, 1.1f * scale);
                target->DrawLine(D2D1::Point2F(c.x + hw, c.y + hh * 0.45f), D2D1::Point2F(c.x, c.y + hh), brush, 1.1f * scale);
                target->DrawLine(D2D1::Point2F(c.x, c.y - hh), D2D1::Point2F(c.x + hw, c.y - hh * 0.45f), brush, 1.1f * scale);
                target->DrawLine(D2D1::Point2F(c.x + hw, c.y - hh * 0.45f), D2D1::Point2F(c.x - hw, c.y + hh * 0.45f), brush, 1.1f * scale);
            }
            break;
        }
        case BatteryGlyphKind::PowerFlow: {
            // Card 2 (Power Flow): Draws dedicated bold 15px lightning bolt glyph distinct from Card 1
            DrawLightningBolt(target, factory, c, s, brush);
            break;
        }
        case BatteryGlyphKind::Time: {
            // Clock face with hands
            target->DrawEllipse(D2D1::Ellipse(c, 0.38f * s, 0.38f * s), brush, 1.1f);
            target->FillEllipse(D2D1::Ellipse(c, 0.08f * s, 0.08f * s), brush);
            target->DrawLine(c, D2D1::Point2F(c.x, c.y - 0.22f * s), brush, 1.1f);
            target->DrawLine(c, D2D1::Point2F(c.x + 0.20f * s, c.y), brush, 1.1f);
            break;
        }
        case BatteryGlyphKind::PowerMode: {
            // Performance dial / speed gauge
            target->DrawEllipse(D2D1::Ellipse(c, 0.38f * s, 0.38f * s), brush, 1.1f);
            target->FillEllipse(D2D1::Ellipse(c, 0.08f * s, 0.08f * s), brush);
            target->DrawLine(c, D2D1::Point2F(c.x + 0.22f * s, c.y - 0.20f * s), brush, 1.2f);
            break;
        }
        case BatteryGlyphKind::Health: {
            // Shield outline
            const float hw = 0.32f * s;
            const float topY = c.y - 0.32f * s;
            const float midY = c.y + 0.08f * s;
            const float botY = c.y + 0.38f * s;
            target->DrawLine(D2D1::Point2F(c.x - hw, topY), D2D1::Point2F(c.x + hw, topY), brush, 1.1f);
            target->DrawLine(D2D1::Point2F(c.x + hw, topY), D2D1::Point2F(c.x + hw, midY), brush, 1.1f);
            target->DrawLine(D2D1::Point2F(c.x + hw, midY), D2D1::Point2F(c.x, botY), brush, 1.1f);
            target->DrawLine(D2D1::Point2F(c.x, botY), D2D1::Point2F(c.x - hw, midY), brush, 1.1f);
            target->DrawLine(D2D1::Point2F(c.x - hw, midY), D2D1::Point2F(c.x - hw, topY), brush, 1.1f);

            // Small checkmark inside shield
            target->DrawLine(D2D1::Point2F(c.x - 0.14f * s, c.y - 0.02f * s), D2D1::Point2F(c.x - 0.03f * s, c.y + 0.10f * s), brush, 1.1f);
            target->DrawLine(D2D1::Point2F(c.x - 0.03f * s, c.y + 0.10f * s), D2D1::Point2F(c.x + 0.16f * s, c.y - 0.12f * s), brush, 1.1f);
            break;
        }
    }
}

// ----------------------------------------------------------------------------
// Primary Bento Grid Orchestration Function (2x3 Grid)
// ----------------------------------------------------------------------------

inline void DrawBatteryBentoGrid(
    ID2D1RenderTarget* target,
    ID2D1Factory* factory,
    IDWriteTextFormat* boldTextFormat,
    IDWriteTextFormat* textFormat,
    IDWriteTextFormat* smallTextFormat,
    IDWriteTextFormat* iconFormat,
    D2D1_RECT_F rect,
    float scale,
    const BentoBatteryData& data,
    D2D1_COLOR_F cardFill = tokens::kCardBase,
    float opacity = 1.0f)
{
    if (!target) return;

    // Header layout (Mirroring Hardware Monitor)
    const float padX = 24.0f * scale;

    // Left-aligned header title
    if (boldTextFormat) {
        ComPtr<ID2D1SolidColorBrush> titleBrush;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                BentoWithAlpha(tokens::kTextPrimary, 0.96f * opacity), &titleBrush)) && titleBrush) {
            boldTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            boldTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            const wchar_t* kHeaderTitle = L"Battery & Power";
            const D2D1_RECT_F titleRect = D2D1::RectF(
                rect.left + padX, rect.top + 16.0f * scale,
                rect.right - padX, rect.top + 32.0f * scale);

            target->DrawTextW(kHeaderTitle, static_cast<UINT32>(wcslen(kHeaderTitle)), boldTextFormat,
                               titleRect, titleBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
            boldTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        }
    }

    // ------------------------------------------------------------------------
    // Prepare the 6 Cards for 2x3 Grid
    // ------------------------------------------------------------------------
    BatteryGridCard cards[6];

    // --- CARD 0: Battery Level (Top-Left) ---
    cards[0].glyphKind = BatteryGlyphKind::Battery;
    cards[0].label = data.charging
        ? (data.percent >= 100 ? L"FULLY CHARGED" : L"CHARGING")
        : (data.percent <= 20 ? L"LOW BATTERY" : L"BATTERY");
    wchar_t batVal[32] = {};
    if (data.charging && data.percent >= 100) {
        swprintf_s(batVal, L"100%% Full");
    } else if (data.charging) {
        swprintf_s(batVal, L"%d%% (AC)", data.percent);
    } else {
        swprintf_s(batVal, L"%d%%", data.percent);
    }
    cards[0].value = batVal;
    cards[0].fraction = BentoClamp(data.percent / 100.0f, 0.0f, 1.0f);
    cards[0].tint = data.charging ? tokens::kAppleGreen
        : ((data.percent <= 10) ? tokens::kAppleRed
        : ((data.percent <= 20) ? tokens::kAppleAmber : tokens::kAppleGreen));

    // --- CARD 1: Connected Peripherals (Top-Right) ---
    cards[1].glyphKind = BatteryGlyphKind::Peripheral;
    cards[1].accessoryCategory = BentoDeviceCategory::Generic;
    const BentoAccessory* primaryAccessory = nullptr;
    size_t activeAccCount = 0;
    for (const auto& acc : data.accessories) {
        if (acc.connected && !acc.name.empty()) {
            if (!primaryAccessory) primaryAccessory = &acc;
            activeAccCount++;
        }
    }

    if (primaryAccessory) {
        cards[1].accessoryCategory = primaryAccessory->category;
        if (activeAccCount > 1) {
            wchar_t accLbl[48] = {};
            swprintf_s(accLbl, L"PERIPHERAL (+%zu)", activeAccCount - 1);
            cards[1].label = accLbl;
        } else {
            cards[1].label = L"PERIPHERAL";
        }

        wchar_t accVal[64] = {};
        if (primaryAccessory->batteryPercent >= 0) {
            swprintf_s(accVal, L"%s (%d%%)", primaryAccessory->name.c_str(), primaryAccessory->batteryPercent);
            cards[1].fraction = BentoClamp(primaryAccessory->batteryPercent / 100.0f, 0.0f, 1.0f);
            cards[1].tint = (primaryAccessory->batteryPercent <= 10) ? tokens::kAppleRed
                : ((primaryAccessory->batteryPercent <= 20) ? tokens::kAppleAmber : tokens::kAppleGreen);
        } else {
            swprintf_s(accVal, L"%s", primaryAccessory->name.c_str());
            cards[1].fraction = -1.0f;
            cards[1].tint = tokens::kAppleGreen;
        }
        cards[1].value = accVal;
    } else {
        cards[1].label = L"PERIPHERAL";
        cards[1].value = L"No Devices";
        cards[1].fraction = -1.0f;
        cards[1].tint = tokens::kTextTertiary;
    }

    // --- CARD 2: Power Flow / Wattage Rate (Mid-Left) ---
    cards[2].glyphKind = BatteryGlyphKind::PowerFlow;
    cards[2].label = L"POWER FLOW";
    wchar_t flowVal[48] = {};
    if (std::abs(data.powerRateWatts) > 0.05f) {
        if (data.powerRateWatts > 0.0f) {
            if (data.powerRateWatts >= 35.0f) {
                swprintf_s(flowVal, L"+%.0fW Fast Chg", data.powerRateWatts);
            } else {
                swprintf_s(flowVal, L"+%.1fW Flow", data.powerRateWatts);
            }
        } else {
            swprintf_s(flowVal, L"%.1fW Flow", data.powerRateWatts);
        }
    } else if (data.charging) {
        wcscpy_s(flowVal, L"AC Power");
    } else {
        wcscpy_s(flowVal, L"Normal Flow");
    }
    cards[2].value = flowVal;
    cards[2].fraction = -1.0f; // Wattage flow has no natural ceiling
    cards[2].tint = (data.powerRateWatts >= 35.0f) ? tokens::kAppleGreen
        : ((data.powerRateWatts > 0.0f) ? tokens::kAppleGreen : tokens::kAppleAmber);

    // --- CARD 3: Time Remaining / Time to Full (Mid-Right) ---
    cards[3].glyphKind = BatteryGlyphKind::Time;
    cards[3].label = data.charging ? L"TIME UNTIL FULL" : L"TIME REMAINING";
    wchar_t timeVal[48] = {};
    if (data.charging) {
        if (data.percent >= 100) {
            wcscpy_s(timeVal, L"Fully Charged");
        } else if (data.secondsToFull > 0) {
            const int h = data.secondsToFull / 3600;
            const int m = (data.secondsToFull % 3600) / 60;
            if (h > 0) swprintf_s(timeVal, L"%dh %dm to full", h, m);
            else swprintf_s(timeVal, L"%dm to full", m);
        } else {
            wcscpy_s(timeVal, L"Calculating...");
        }
    } else {
        if (data.secondsRemaining > 0) {
            const int h = data.secondsRemaining / 3600;
            const int m = (data.secondsRemaining % 3600) / 60;
            if (h > 0) swprintf_s(timeVal, L"%dh %dm left", h, m);
            else swprintf_s(timeVal, L"%dm left", m);
        } else if (data.healthPercent < 0 && data.percent == 100) {
            wcscpy_s(timeVal, L"AC Connected");
        } else {
            wcscpy_s(timeVal, L"On Battery");
        }
    }
    cards[3].value = timeVal;
    cards[3].fraction = -1.0f;
    cards[3].tint = tokens::kAppleGreen;

    // --- CARD 4: Power Mode (Bot-Left) ---
    cards[4].glyphKind = BatteryGlyphKind::PowerMode;
    cards[4].label = L"POWER MODE";
    wchar_t modeVal[48] = {};
    const wchar_t* rawScheme = data.powerSchemeName.empty() ? L"Balanced" : data.powerSchemeName.c_str();
    if (wcsstr(rawScheme, L"Mode") != nullptr || wcsstr(rawScheme, L"mode") != nullptr) {
        swprintf_s(modeVal, L"%s", rawScheme);
    } else {
        swprintf_s(modeVal, L"%s Mode", rawScheme);
    }
    cards[4].value = modeVal;

    // Power Mode is a categorical state (keep fraction = -1.0f; no progress bar)
    cards[4].fraction = -1.0f;
    if (wcsstr(modeVal, L"Turbo") || wcsstr(modeVal, L"High")) {
        cards[4].tint = tokens::kAppleAmber;
    } else if (wcsstr(modeVal, L"Silent")) {
        cards[4].tint = D2D1::ColorF(76.0f / 255.0f, 201.0f / 255.0f, 240.0f / 255.0f, 1.0f);
    } else {
        cards[4].tint = tokens::kAppleGreen;
    }

    // --- CARD 5: Battery Health (Bot-Right) [ZERO CYCLE COUNT] ---
    cards[5].glyphKind = BatteryGlyphKind::Health;
    cards[5].label = L"BATTERY HEALTH";
    wchar_t healthVal[48] = {};
    if (data.healthPercent > 0) {
        swprintf_s(healthVal, L"%d%% Capacity", data.healthPercent);
        cards[5].fraction = BentoClamp(data.healthPercent / 100.0f, 0.0f, 1.0f);
        cards[5].tint = (data.healthPercent >= 80) ? tokens::kAppleGreen
            : ((data.healthPercent >= 60) ? tokens::kAppleAmber : tokens::kAppleRed);
    } else if (data.healthPercent < 0 && data.percent == 100) {
        wcscpy_s(healthVal, L"Desktop Power");
        cards[5].fraction = 1.0f;
        cards[5].tint = tokens::kAppleGreen;
    } else {
        wcscpy_s(healthVal, L"Normal (AC)");
        cards[5].fraction = -1.0f;
        cards[5].tint = tokens::kAppleGreen;
    }
    cards[5].value = healthVal;

    // ------------------------------------------------------------------------
    // Render 2x3 Grid of Cards (Exact Hardware Monitor Geometry)
    // ------------------------------------------------------------------------
    const float colGap = 9.0f * scale;
    const float colW = (rect.right - rect.left - padX * 2.0f - colGap) * 0.5f;
    const float rowH = 38.0f * scale;
    const float rowGap = 6.0f * scale;
    const float gridTop = rect.top + 38.0f * scale;

    for (int i = 0; i < 6; ++i) {
        const BatteryGridCard& m = cards[i];
        const int col = i % 2;
        const int row = i / 2;

        const float cardLeft = rect.left + padX + static_cast<float>(col) * (colW + colGap);
        const float cardTop = gridTop + static_cast<float>(row) * (rowH + rowGap);
        const D2D1_RECT_F card = D2D1::RectF(cardLeft, cardTop, cardLeft + colW, cardTop + rowH);

        DrawBentoCard(target, card, 9.0f * scale, cardFill, opacity);

        const bool hasBar = (m.fraction >= 0.0f);

        // Glyph (15px, centered at cardLeft + 16px, cardTop + 15px)
        ComPtr<ID2D1SolidColorBrush> glyphBrush;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                BentoWithAlpha(m.tint, 0.88f * opacity), &glyphBrush)) && glyphBrush) {
            DrawGlyphIcon(target, factory, iconFormat, m, data,
                          D2D1::Point2F(card.left + 16.0f * scale, card.top + 15.0f * scale),
                          15.0f * scale, glyphBrush.Get(), scale);
        }

        const float textLeft = card.left + 30.0f * scale;
        const float textRight = card.right - 9.0f * scale;

        // Label above value (matches Hardware Monitor mutedBrush_ 0.60f opacity without double-compounding)
        if (smallTextFormat) {
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            smallTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

            ComPtr<ID2D1SolidColorBrush> mutedBrush;
            if (SUCCEEDED(target->CreateSolidColorBrush(
                    D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.60f * opacity), &mutedBrush)) && mutedBrush) {
                const D2D1_RECT_F lblRect = D2D1::RectF(
                    textLeft, card.top + 2.0f * scale, textRight, card.top + 15.0f * scale);
                target->DrawTextW(m.label.c_str(), static_cast<UINT32>(m.label.size()), smallTextFormat,
                                   lblRect, mutedBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }

        // Value text
        if (textFormat) {
            textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

            ComPtr<ID2D1SolidColorBrush> textBrush;
            if (SUCCEEDED(target->CreateSolidColorBrush(
                    BentoWithAlpha(tokens::kTextPrimary, 0.95f * opacity), &textBrush)) && textBrush) {
                const float valueBottom = hasBar ? (card.top + 30.0f * scale) : (card.bottom - 4.0f * scale);
                const D2D1_RECT_F valRect = D2D1::RectF(
                    textLeft, card.top + 15.0f * scale, textRight, valueBottom);
                target->DrawTextW(m.value.c_str(), static_cast<UINT32>(m.value.size()), textFormat,
                                   valRect, textBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
            textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }

        // Bottom Load Bar
        if (hasBar) {
            const D2D1_RECT_F track = D2D1::RectF(
                textLeft, card.bottom - 6.0f * scale, textRight, card.bottom - 3.5f * scale);
            ComPtr<ID2D1SolidColorBrush> trackBrush;
            if (SUCCEEDED(target->CreateSolidColorBrush(
                    BentoWithAlpha(tokens::kTrackBackground, 0.12f * opacity), &trackBrush)) && trackBrush) {
                target->FillRoundedRectangle(D2D1::RoundedRect(track, 1.25f * scale, 1.25f * scale), trackBrush.Get());
            }

            const float span = (track.right - track.left) * m.fraction;
            if (span > 0.5f) {
                ComPtr<ID2D1SolidColorBrush> fillBrush;
                if (SUCCEEDED(target->CreateSolidColorBrush(
                        BentoWithAlpha(m.tint, 0.95f * opacity), &fillBrush)) && fillBrush) {
                    target->FillRoundedRectangle(
                        D2D1::RoundedRect(
                            D2D1::RectF(track.left, track.top, track.left + span, track.bottom),
                            1.25f * scale, 1.25f * scale),
                        fillBrush.Get());
                }
            }
        }
    }
}

// ============================================================================
// Modern C++ Template Integration Adapters
// Decouples companion header from main mod globals for Wave 2 1-line integration
// ============================================================================

template <typename TBattery, typename TAccessoryList>
inline void DrawBatteryBentoGrid(
    ID2D1RenderTarget* target,
    ID2D1Factory* factory,
    IDWriteTextFormat* boldTextFormat,
    IDWriteTextFormat* textFormat,
    IDWriteTextFormat* smallTextFormat,
    IDWriteTextFormat* iconFormat,
    D2D1_RECT_F rect,
    float scale,
    const TBattery& battery,
    const TAccessoryList& accessories,
    D2D1_COLOR_F cardFill = tokens::kCardBase,
    float opacity = 1.0f)
{
    BentoBatteryData data;
    data.percent = battery.percent;
    data.charging = battery.charging;
    data.low = battery.low;
    data.powerRateWatts = battery.powerRateWatts;
    data.secondsRemaining = battery.secondsRemaining;
    data.secondsToFull = battery.secondsToFull;
    data.healthPercent = battery.healthPercent;
    data.cycleCount = battery.cycleCount;
    data.powerSchemeName = battery.powerSchemeName;

    for (const auto& item : accessories) {
        BentoAccessory b;
        b.name = item.name;
        b.category = static_cast<BentoDeviceCategory>(item.category);
        b.batteryPercent = item.batteryPercent;
        b.connected = item.connected;
        data.accessories.push_back(std::move(b));
    }

    DrawBatteryBentoGrid(target, factory, boldTextFormat, textFormat, smallTextFormat,
                         iconFormat, rect, scale, data, cardFill, opacity);
}

template <typename TState>
inline void DrawBatteryBentoGridFromState(
    ID2D1RenderTarget* target,
    ID2D1Factory* factory,
    IDWriteTextFormat* boldTextFormat,
    IDWriteTextFormat* textFormat,
    IDWriteTextFormat* smallTextFormat,
    IDWriteTextFormat* iconFormat,
    D2D1_RECT_F rect,
    float scale,
    const TState& state,
    D2D1_COLOR_F cardFill = tokens::kCardBase,
    float opacity = 1.0f)
{
    BentoBatteryData data;
    data.percent = state.battery.percent;
    data.charging = state.battery.charging;
    data.low = state.battery.low;
    data.powerRateWatts = state.battery.powerRateWatts;
    data.secondsRemaining = state.battery.secondsRemaining;
    data.secondsToFull = state.battery.secondsToFull;
    data.healthPercent = state.battery.healthPercent;
    data.cycleCount = state.battery.cycleCount;
    data.powerSchemeName = state.battery.powerSchemeName;

    if (state.bluetoothDevice.connected && !state.bluetoothDevice.deviceName.empty()) {
        BentoAccessory b;
        b.name = state.bluetoothDevice.deviceName;
        b.category = static_cast<BentoDeviceCategory>(state.bluetoothDevice.category);
        b.batteryPercent = state.bluetoothDevice.batteryPercent;
        b.connected = true;
        data.accessories.push_back(std::move(b));
    }

    DrawBatteryBentoGrid(target, factory, boldTextFormat, textFormat, smallTextFormat,
                         iconFormat, rect, scale, data, cardFill, opacity);
}

} // namespace battery_bento

namespace battery = battery_bento;

#endif // BATTERY_DASHBOARD_HPP
