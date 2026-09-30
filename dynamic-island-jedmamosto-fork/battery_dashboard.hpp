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
    constexpr D2D1_COLOR_F kTextSecondary = { 1.0f, 1.0f, 1.0f, 0.54f };
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

// Draws a subtle horizontal 1px hairline divider inside cards
inline void DrawHairlineDivider(
    ID2D1RenderTarget* target,
    float x0,
    float x1,
    float y,
    float opacity = 1.0f)
{
    if (!target) return;
    ComPtr<ID2D1SolidColorBrush> lineBrush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            BentoWithAlpha(tokens::kDividerHairline, tokens::kDividerHairline.a * opacity), &lineBrush)) && lineBrush) {
        target->DrawLine(D2D1::Point2F(x0, y), D2D1::Point2F(x1, y), lineBrush.Get(), 0.75f);
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

    // Resolve factory from target if needed
    ComPtr<ID2D1Factory> d2dFactory = factory;
    if (!d2dFactory) {
        target->GetFactory(&d2dFactory);
    }
    if (!d2dFactory) return;

    ComPtr<ID2D1PathGeometry> bolt;
    if (FAILED(d2dFactory->CreatePathGeometry(&bolt)) || !bolt) return;

    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(bolt->Open(&sink)) || !sink) return;

    // Vertically centered coordinates (offset center by -0.41f * s)
    const float cyAdj = c.y - 0.41f * s;
    sink->BeginFigure(D2D1::Point2F(c.x + 0.07f * s, cyAdj + 0.16f * s), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(c.x - 0.11f * s, cyAdj + 0.44f * s));
    sink->AddLine(D2D1::Point2F(c.x + 0.00f * s, cyAdj + 0.44f * s));
    sink->AddLine(D2D1::Point2F(c.x - 0.06f * s, cyAdj + 0.66f * s));
    sink->AddLine(D2D1::Point2F(c.x + 0.15f * s, cyAdj + 0.36f * s));
    sink->AddLine(D2D1::Point2F(c.x + 0.03f * s, cyAdj + 0.36f * s));
    sink->AddLine(D2D1::Point2F(c.x + 0.12f * s, cyAdj + 0.16f * s));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    target->FillGeometry(bolt.Get(), brush);
}

// Draws a rounded level/progress bar with track and active fill
inline void DrawRoundedLevelBar(
    ID2D1RenderTarget* target,
    D2D1_RECT_F trackRect,
    float barHeight,
    float fraction,
    D2D1_COLOR_F barColor,
    float opacity = 1.0f)
{
    if (!target) return;
    const float radius = barHeight * 0.5f;

    // Track background
    ComPtr<ID2D1SolidColorBrush> trackBrush;
    if (SUCCEEDED(target->CreateSolidColorBrush(
            BentoWithAlpha(tokens::kTrackBackground, tokens::kTrackBackground.a * opacity), &trackBrush)) && trackBrush) {
        target->FillRoundedRectangle(D2D1::RoundedRect(trackRect, radius, radius), trackBrush.Get());
    }

    // Active fill
    const float clampedFrac = BentoClamp(fraction, 0.0f, 1.0f);
    if (clampedFrac > 0.01f) {
        const float fillW = std::max(barHeight, (trackRect.right - trackRect.left) * clampedFrac);
        const D2D1_RECT_F fillRect = D2D1::RectF(
            trackRect.left, trackRect.top,
            std::min(trackRect.right, trackRect.left + fillW), trackRect.bottom);

        ComPtr<ID2D1SolidColorBrush> fillBrush;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                BentoWithAlpha(barColor, barColor.a * opacity), &fillBrush)) && fillBrush) {
            target->FillRoundedRectangle(D2D1::RoundedRect(fillRect, radius, radius), fillBrush.Get());
        }
    }
}

// ============================================================================
// Bento Grid Tile Renderers
// ============================================================================

// ----------------------------------------------------------------------------
// TILE 1: Battery Hero (Top-Left, 163px × 64px)
// Centered/leading battery percentage + lightning bolt + Apple System Green bar + wattage
// ----------------------------------------------------------------------------
inline void DrawHeroTile(
    ID2D1RenderTarget* target,
    ID2D1Factory* factory,
    IDWriteTextFormat* boldTextFormat,
    IDWriteTextFormat* textFormat,
    IDWriteTextFormat* smallTextFormat,
    D2D1_RECT_F cardRect,
    float scale,
    const BentoBatteryData& data,
    D2D1_COLOR_F cardFill,
    float opacity = 1.0f)
{
    DrawBentoCard(target, cardRect, 10.0f * scale, cardFill, opacity);

    const float padInner = 10.0f * scale;
    const float innerX = cardRect.left + padInner;
    const float innerW = (cardRect.right - cardRect.left) - padInner * 2.0f;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> textWhite;
    ComPtr<ID2D1SolidColorBrush> textMuted;
    ComPtr<ID2D1SolidColorBrush> greenBrush;
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kTextPrimary, tokens::kTextPrimary.a * opacity), &textWhite);
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kTextSecondary, tokens::kTextSecondary.a * opacity), &textMuted);
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kAppleGreen, tokens::kAppleGreen.a * opacity), &greenBrush);

    // Row 1: Battery Percentage + Lightning Bolt + Wattage / Status
    const float topRowY = cardRect.top + 7.0f * scale;
    const float topRowH = 19.0f * scale;

    // Percentage text
    wchar_t pctText[16] = {};
    swprintf_s(pctText, L"%d%%", data.percent);

    if (textFormat && textWhite) {
        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        // Calculate text width approx or draw text in leading box
        const D2D1_RECT_F pctRect = D2D1::RectF(innerX, topRowY, innerX + 46.0f * scale, topRowY + topRowH);
        target->DrawTextW(pctText, static_cast<UINT32>(wcslen(pctText)), textFormat, pctRect,
                           textWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        // Approximate pct width: 2 digits + % ~ 34px, 3 digits ~ 42px
        const float pctAdvance = (data.percent >= 100) ? 42.0f * scale : 34.0f * scale;

        // Lightning bolt icon (when charging)
        if (data.charging && greenBrush) {
            const D2D1_POINT_2F boltCenter = D2D1::Point2F(innerX + pctAdvance + 8.0f * scale, topRowY + topRowH * 0.5f);
            DrawLightningBolt(target, factory, boltCenter, 11.5f * scale, greenBrush.Get());
        }
    }

    // Wattage / Power flow status (right-aligned in top row)
    wchar_t wattageText[48] = {};
    bool isFastCharge = false;
    if (std::abs(data.powerRateWatts) > 0.05f) {
        if (data.powerRateWatts > 0.0f) {
            if (data.powerRateWatts >= 35.0f) {
                swprintf_s(wattageText, L"+%.0fW Fast Chg", data.powerRateWatts);
                isFastCharge = true;
            } else {
                swprintf_s(wattageText, L"+%.1fW Flow", data.powerRateWatts);
            }
        } else {
            swprintf_s(wattageText, L"%.1fW Flow", data.powerRateWatts);
        }
    } else if (data.charging) {
        wcscpy_s(wattageText, L"AC Power");
    } else {
        wcscpy_s(wattageText, L"Normal Flow");
    }

    if (smallTextFormat) {
        smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        ID2D1SolidColorBrush* wattBrush = (isFastCharge && greenBrush) ? greenBrush.Get() : textMuted.Get();
        const D2D1_RECT_F wattRect = D2D1::RectF(innerX + 50.0f * scale, topRowY, innerX + innerW, topRowY + topRowH);
        target->DrawTextW(wattageText, static_cast<UINT32>(wcslen(wattageText)), smallTextFormat, wattRect,
                           wattBrush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Row 2: Apple Green Level Bar (6.0px rounded bar)
    const float barY = cardRect.top + 28.5f * scale;
    const float barH = 6.0f * scale;
    const D2D1_RECT_F barRect = D2D1::RectF(innerX, barY, innerX + innerW, barY + barH);

    // Color Resolution: STRICT Apple System Green (#34C759) when charging, NEVER cyan!
    D2D1_COLOR_F barColor = tokens::kAppleGreen;
    if (!data.charging) {
        if (data.percent <= 10) {
            barColor = tokens::kAppleRed;
        } else if (data.percent <= 20) {
            barColor = tokens::kAppleAmber;
        }
    }
    DrawRoundedLevelBar(target, barRect, barH, data.percent / 100.0f, barColor, opacity);

    // Row 3: Subtitle / State label (bottom row)
    const float botRowY = cardRect.top + 39.5f * scale;
    const float botRowH = 18.0f * scale;

    const wchar_t* subLabel = data.charging
        ? (data.percent >= 100 ? L"Fully Charged" : L"Charging")
        : (data.percent <= 20 ? L"Low Battery" : L"On Battery");

    if (smallTextFormat && textMuted) {
        smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const D2D1_RECT_F subRect = D2D1::RectF(innerX, botRowY, innerX + innerW * 0.55f, botRowY + botRowH);
        target->DrawTextW(subLabel, static_cast<UINT32>(wcslen(subLabel)), smallTextFormat, subRect,
                           (data.charging && greenBrush) ? greenBrush.Get() : textMuted.Get(),
                           D2D1_DRAW_TEXT_OPTIONS_CLIP);

        // Right side of bottom row: quick time summary if available
        wchar_t quickTime[32] = {};
        if (data.charging && data.secondsToFull > 0) {
            const int h = data.secondsToFull / 3600;
            const int m = (data.secondsToFull % 3600) / 60;
            if (h > 0) swprintf_s(quickTime, L"%dh %dm to full", h, m);
            else swprintf_s(quickTime, L"%dm to full", m);
        } else if (!data.charging && data.secondsRemaining > 0) {
            const int h = data.secondsRemaining / 3600;
            const int m = (data.secondsRemaining % 3600) / 60;
            if (h > 0) swprintf_s(quickTime, L"%dh %dm left", h, m);
            else swprintf_s(quickTime, L"%dm left", m);
        }

        if (quickTime[0] != L'\0') {
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            const D2D1_RECT_F timeRect = D2D1::RectF(innerX + innerW * 0.45f, botRowY, innerX + innerW, botRowY + botRowH);
            target->DrawTextW(quickTime, static_cast<UINT32>(wcslen(quickTime)), smallTextFormat, timeRect,
                               textMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }
}

// ----------------------------------------------------------------------------
// TILE 2: Connected Peripherals (Top-Right, 163px × 64px)
// Device Name + Category Icon + Level Bar + % OR Ambient "No Peripherals Connected"
// ----------------------------------------------------------------------------
inline void DrawPeripheralsTile(
    ID2D1RenderTarget* target,
    IDWriteTextFormat* boldTextFormat,
    IDWriteTextFormat* textFormat,
    IDWriteTextFormat* smallTextFormat,
    IDWriteTextFormat* iconFormat,
    D2D1_RECT_F cardRect,
    float scale,
    const std::vector<BentoAccessory>& accessories,
    D2D1_COLOR_F cardFill,
    float opacity = 1.0f)
{
    DrawBentoCard(target, cardRect, 10.0f * scale, cardFill, opacity);

    const float padInner = 10.0f * scale;
    const float innerX = cardRect.left + padInner;
    const float innerW = (cardRect.right - cardRect.left) - padInner * 2.0f;

    // Surface brushes
    ComPtr<ID2D1SolidColorBrush> textWhite;
    ComPtr<ID2D1SolidColorBrush> textMuted;
    ComPtr<ID2D1SolidColorBrush> textTertiary;
    ComPtr<ID2D1SolidColorBrush> greenBrush;
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kTextPrimary, tokens::kTextPrimary.a * opacity), &textWhite);
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kTextSecondary, tokens::kTextSecondary.a * opacity), &textMuted);
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kTextTertiary, tokens::kTextTertiary.a * opacity), &textTertiary);
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kAppleGreen, tokens::kAppleGreen.a * opacity), &greenBrush);

    // Filter connected accessories
    std::vector<const BentoAccessory*> activeList;
    for (const auto& acc : accessories) {
        if (acc.connected && !acc.name.empty()) {
            activeList.push_back(&acc);
        }
    }

    if (activeList.empty()) {
        // ====================================================================
        // Ambient "No Peripherals Connected" Card (Clean Apple Dark Restraint)
        // ====================================================================
        const float cx = (cardRect.left + cardRect.right) * 0.5f;

        // Centered Bluetooth glyph
        if (iconFormat && textTertiary) {
            iconFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            iconFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            const wchar_t* btGlyph = L"\uE702"; // Segoe Bluetooth
            const D2D1_RECT_F glyphRect = D2D1::RectF(
                cx - 16.0f * scale, cardRect.top + 10.0f * scale,
                cx + 16.0f * scale, cardRect.top + 30.0f * scale);

            target->DrawTextW(btGlyph, static_cast<UINT32>(wcslen(btGlyph)), iconFormat, glyphRect,
                               textTertiary.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
            iconFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        // Sentence-cased ambient placeholder
        if (smallTextFormat && textTertiary) {
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            const wchar_t* kEmptyMsg = L"No Peripherals Connected";
            const D2D1_RECT_F msgRect = D2D1::RectF(
                innerX, cardRect.top + 32.0f * scale,
                innerX + innerW, cardRect.top + 52.0f * scale);

            target->DrawTextW(kEmptyMsg, static_cast<UINT32>(wcslen(kEmptyMsg)), smallTextFormat, msgRect,
                               textTertiary.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    } else {
        // ====================================================================
        // Active Peripheral Tile (Hero accessory display)
        // ====================================================================
        const BentoAccessory* primary = activeList[0];

        // Row 1: Category Icon + Device Name + Battery %
        const float topRowY = cardRect.top + 7.0f * scale;
        const float topRowH = 19.0f * scale;
        const float iconW = 16.0f * scale;

        // Category Icon
        if (iconFormat && textWhite) {
            iconFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            iconFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            const wchar_t* glyph = GetCategoryGlyph(primary->category);
            const D2D1_RECT_F iconRect = D2D1::RectF(innerX, topRowY, innerX + iconW, topRowY + topRowH);
            target->DrawTextW(glyph, static_cast<UINT32>(wcslen(glyph)), iconFormat, iconRect,
                               textWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            iconFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        // Percentage text (right-aligned in top row)
        float pctRightW = 0.0f;
        if (primary->batteryPercent >= 0 && smallTextFormat && textWhite) {
            wchar_t batBuf[16] = {};
            swprintf_s(batBuf, L"%d%%", primary->batteryPercent);

            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            pctRightW = 34.0f * scale;
            const D2D1_RECT_F pctRect = D2D1::RectF(
                innerX + innerW - pctRightW, topRowY, innerX + innerW, topRowY + topRowH);

            D2D1_COLOR_F pctColor = (primary->batteryPercent <= 10) ? tokens::kAppleRed
                : ((primary->batteryPercent <= 20) ? tokens::kAppleAmber : tokens::kAppleGreen);

            ComPtr<ID2D1SolidColorBrush> pctBrush;
            if (SUCCEEDED(target->CreateSolidColorBrush(
                    BentoWithAlpha(pctColor, pctColor.a * opacity), &pctBrush)) && pctBrush) {
                target->DrawTextW(batBuf, static_cast<UINT32>(wcslen(batBuf)), smallTextFormat, pctRect,
                                   pctBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
        }

        // Device Name (clipped between icon and pct)
        if (smallTextFormat && textWhite) {
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            const float nameLeft = innerX + iconW + 6.0f * scale;
            const float nameRight = innerX + innerW - (pctRightW > 0 ? (pctRightW + 4.0f * scale) : 0.0f);
            const D2D1_RECT_F nameRect = D2D1::RectF(nameLeft, topRowY, nameRight, topRowY + topRowH);

            target->DrawTextW(primary->name.c_str(), static_cast<UINT32>(primary->name.size()),
                               smallTextFormat, nameRect, textWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // Row 2: 5.5px Rounded Level Bar
        const float barY = cardRect.top + 29.0f * scale;
        const float barH = 5.5f * scale;
        const D2D1_RECT_F barRect = D2D1::RectF(innerX, barY, innerX + innerW, barY + barH);

        if (primary->batteryPercent >= 0) {
            D2D1_COLOR_F barColor = (primary->batteryPercent <= 10) ? tokens::kAppleRed
                : ((primary->batteryPercent <= 20) ? tokens::kAppleAmber : tokens::kAppleGreen);

            DrawRoundedLevelBar(target, barRect, barH, primary->batteryPercent / 100.0f, barColor, opacity);
        } else {
            // Indeterminate / connected without battery track
            DrawRoundedLevelBar(target, barRect, barH, 1.0f, tokens::kCardHairline, opacity * 0.6f);
        }

        // Row 3: Status / Secondary Peripheral indicator
        const float botRowY = cardRect.top + 39.5f * scale;
        const float botRowH = 18.0f * scale;

        if (smallTextFormat && textMuted) {
            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            const wchar_t* statusStr = (primary->batteryPercent >= 0) ? L"Connected" : L"Bluetooth Active";
            const D2D1_RECT_F statRect = D2D1::RectF(innerX, botRowY, innerX + innerW * 0.60f, botRowY + botRowH);
            target->DrawTextW(statusStr, static_cast<UINT32>(wcslen(statusStr)), smallTextFormat, statRect,
                               textMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

            // If there's an additional connected device, mention it cleanly
            if (activeList.size() > 1) {
                wchar_t extraBuf[32] = {};
                swprintf_s(extraBuf, L"+%zu device", activeList.size() - 1);

                smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                const D2D1_RECT_F extraRect = D2D1::RectF(innerX + innerW * 0.50f, botRowY, innerX + innerW, botRowY + botRowH);
                target->DrawTextW(extraBuf, static_cast<UINT32>(wcslen(extraBuf)), smallTextFormat, extraRect,
                                   textMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }

            smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    }
}

// ----------------------------------------------------------------------------
// TILE 3: Time Remaining & Power Mode (Bottom-Left, 163px × 64px)
// Top: TIME REMAINING over "1h 24m until full" | Bottom: POWER MODE over "Balanced Mode"
// ----------------------------------------------------------------------------
inline void DrawTimePowerTile(
    ID2D1RenderTarget* target,
    IDWriteTextFormat* boldTextFormat,
    IDWriteTextFormat* textFormat,
    IDWriteTextFormat* smallTextFormat,
    D2D1_RECT_F cardRect,
    float scale,
    const BentoBatteryData& data,
    D2D1_COLOR_F cardFill,
    float opacity = 1.0f)
{
    DrawBentoCard(target, cardRect, 10.0f * scale, cardFill, opacity);

    const float padInner = 10.0f * scale;
    const float innerX = cardRect.left + padInner;
    const float innerW = (cardRect.right - cardRect.left) - padInner * 2.0f;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> textWhite;
    ComPtr<ID2D1SolidColorBrush> textMuted;
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kTextPrimary, tokens::kTextPrimary.a * opacity), &textWhite);
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kTextSecondary, tokens::kTextSecondary.a * opacity), &textMuted);

    // Upper Half: Time Remaining
    const float sec1Top = cardRect.top + 6.0f * scale;
    const wchar_t* timeLabel = data.charging ? L"TIME UNTIL FULL" : L"TIME REMAINING";

    if (smallTextFormat && textMuted) {
        smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const D2D1_RECT_F lblRect = D2D1::RectF(innerX, sec1Top, innerX + innerW, sec1Top + 11.5f * scale);
        target->DrawTextW(timeLabel, static_cast<UINT32>(wcslen(timeLabel)), smallTextFormat, lblRect,
                           textMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Time Value string
    wchar_t timeVal[48] = {};
    if (data.charging) {
        if (data.percent >= 100) {
            wcscpy_s(timeVal, L"Fully Charged");
        } else if (data.secondsToFull > 0) {
            const int h = data.secondsToFull / 3600;
            const int m = (data.secondsToFull % 3600) / 60;
            if (h > 0) swprintf_s(timeVal, L"%dh %dm until full", h, m);
            else swprintf_s(timeVal, L"%dm until full", m);
        } else {
            wcscpy_s(timeVal, L"Charging");
        }
    } else {
        if (data.secondsRemaining > 0) {
            const int h = data.secondsRemaining / 3600;
            const int m = (data.secondsRemaining % 3600) / 60;
            if (h > 0) swprintf_s(timeVal, L"%dh %dm remaining", h, m);
            else swprintf_s(timeVal, L"%dm remaining", m);
        } else if (data.healthPercent < 0 && data.cycleCount < 0 && data.percent == 100) {
            wcscpy_s(timeVal, L"AC Connected");
        } else {
            wcscpy_s(timeVal, L"Discharging");
        }
    }

    if (textFormat && textWhite) {
        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const D2D1_RECT_F valRect = D2D1::RectF(innerX, sec1Top + 11.5f * scale, innerX + innerW, sec1Top + 26.5f * scale);
        target->DrawTextW(timeVal, static_cast<UINT32>(wcslen(timeVal)), textFormat, valRect,
                           textWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Hairline separator between sub-sections
    DrawHairlineDivider(target, innerX, innerX + innerW, cardRect.top + 34.0f * scale, opacity);

    // Lower Half: Power Scheme / Mode
    const float sec2Top = cardRect.top + 36.5f * scale;

    if (smallTextFormat && textMuted) {
        smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const wchar_t* modeLabel = L"POWER MODE";
        const D2D1_RECT_F lblRect = D2D1::RectF(innerX, sec2Top, innerX + innerW, sec2Top + 11.5f * scale);
        target->DrawTextW(modeLabel, static_cast<UINT32>(wcslen(modeLabel)), smallTextFormat, lblRect,
                           textMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Power scheme value
    wchar_t modeVal[48] = {};
    const wchar_t* rawScheme = data.powerSchemeName.empty() ? L"Balanced" : data.powerSchemeName.c_str();
    if (wcsstr(rawScheme, L"Mode") != nullptr || wcsstr(rawScheme, L"mode") != nullptr) {
        swprintf_s(modeVal, L"%s", rawScheme);
    } else {
        swprintf_s(modeVal, L"%s Mode", rawScheme);
    }

    if (textFormat && textWhite) {
        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const D2D1_RECT_F valRect = D2D1::RectF(innerX, sec2Top + 11.5f * scale, innerX + innerW, sec2Top + 26.5f * scale);
        target->DrawTextW(modeVal, static_cast<UINT32>(wcslen(modeVal)), textFormat, valRect,
                           textWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

// ----------------------------------------------------------------------------
// TILE 4: Battery Health & Cycle Count (Bottom-Right, 163px × 64px)
// Top: BATTERY HEALTH over "92% Capacity" | Bottom: CYCLE COUNT over "142 Cycles"
// ----------------------------------------------------------------------------
inline void DrawHealthCyclesTile(
    ID2D1RenderTarget* target,
    IDWriteTextFormat* boldTextFormat,
    IDWriteTextFormat* textFormat,
    IDWriteTextFormat* smallTextFormat,
    D2D1_RECT_F cardRect,
    float scale,
    const BentoBatteryData& data,
    D2D1_COLOR_F cardFill,
    float opacity = 1.0f)
{
    DrawBentoCard(target, cardRect, 10.0f * scale, cardFill, opacity);

    const float padInner = 10.0f * scale;
    const float innerX = cardRect.left + padInner;
    const float innerW = (cardRect.right - cardRect.left) - padInner * 2.0f;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> textWhite;
    ComPtr<ID2D1SolidColorBrush> textMuted;
    ComPtr<ID2D1SolidColorBrush> greenBrush;
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kTextPrimary, tokens::kTextPrimary.a * opacity), &textWhite);
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kTextSecondary, tokens::kTextSecondary.a * opacity), &textMuted);
    target->CreateSolidColorBrush(BentoWithAlpha(tokens::kAppleGreen, tokens::kAppleGreen.a * opacity), &greenBrush);

    // Upper Half: Battery Health
    const float sec1Top = cardRect.top + 6.0f * scale;

    if (smallTextFormat && textMuted) {
        smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const wchar_t* healthLabel = L"BATTERY HEALTH";
        const D2D1_RECT_F lblRect = D2D1::RectF(innerX, sec1Top, innerX + innerW, sec1Top + 11.5f * scale);
        target->DrawTextW(healthLabel, static_cast<UINT32>(wcslen(healthLabel)), smallTextFormat, lblRect,
                           textMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Health capacity string
    wchar_t healthVal[48] = {};
    bool isHealthy = false;
    if (data.healthPercent > 0) {
        swprintf_s(healthVal, L"%d%% Capacity", data.healthPercent);
        isHealthy = (data.healthPercent >= 80);
    } else if (data.healthPercent < 0 && data.percent == 100) {
        wcscpy_s(healthVal, L"Desktop Power");
    } else {
        wcscpy_s(healthVal, L"Normal (AC)");
    }

    if (textFormat && textWhite) {
        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const D2D1_RECT_F valRect = D2D1::RectF(innerX, sec1Top + 11.5f * scale, innerX + innerW, sec1Top + 26.5f * scale);
        target->DrawTextW(healthVal, static_cast<UINT32>(wcslen(healthVal)), textFormat, valRect,
                           textWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        // Apple Green indicator dot for healthy battery condition
        if (isHealthy && greenBrush) {
            const float dotX = innerX + innerW - 6.0f * scale;
            const float dotY = sec1Top + 19.0f * scale;
            target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), 3.0f * scale, 3.0f * scale), greenBrush.Get());
        }
    }

    // Hairline separator between sub-sections
    DrawHairlineDivider(target, innerX, innerX + innerW, cardRect.top + 34.0f * scale, opacity);

    // Lower Half: Cycle Count
    const float sec2Top = cardRect.top + 36.5f * scale;

    if (smallTextFormat && textMuted) {
        smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const wchar_t* cycleLabel = L"CYCLE COUNT";
        const D2D1_RECT_F lblRect = D2D1::RectF(innerX, sec2Top, innerX + innerW, sec2Top + 11.5f * scale);
        target->DrawTextW(cycleLabel, static_cast<UINT32>(wcslen(cycleLabel)), smallTextFormat, lblRect,
                           textMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Cycle count string
    wchar_t cycleVal[48] = {};
    if (data.cycleCount > 0) {
        swprintf_s(cycleVal, L"%d Cycles", data.cycleCount);
    } else if (data.cycleCount < 0 && data.percent == 100) {
        wcscpy_s(cycleVal, L"AC Direct (0c)");
    } else {
        wcscpy_s(cycleVal, L"< 50 Cycles");
    }

    if (textFormat && textWhite) {
        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        const D2D1_RECT_F valRect = D2D1::RectF(innerX, sec2Top + 11.5f * scale, innerX + innerW, sec2Top + 26.5f * scale);
        target->DrawTextW(cycleVal, static_cast<UINT32>(wcslen(cycleVal)), textFormat, valRect,
                           textWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

// ============================================================================
// Primary Bento Grid Orchestration Function
// ============================================================================

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

    // Outer margins & layout geometry
    const float padX = 22.0f * scale;
    const float colW = 163.0f * scale;
    const float colGap = 10.0f * scale;
    const float cardH = 64.0f * scale;
    const float rowGap = 8.0f * scale;

    // Header Title: Left-aligned "Battery & Power"
    if (boldTextFormat) {
        ComPtr<ID2D1SolidColorBrush> titleBrush;
        if (SUCCEEDED(target->CreateSolidColorBrush(
                BentoWithAlpha(tokens::kTextPrimary, 0.96f * opacity), &titleBrush)) && titleBrush) {
            boldTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            boldTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            const wchar_t* kHeaderTitle = L"Battery & Power";
            const D2D1_RECT_F titleRect = D2D1::RectF(
                rect.left + padX, rect.top + 13.0f * scale,
                rect.right - padX, rect.top + 29.0f * scale);

            target->DrawTextW(kHeaderTitle, static_cast<UINT32>(wcslen(kHeaderTitle)), boldTextFormat,
                               titleRect, titleBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            boldTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        }
    }

    // Compute Exact 4-Tile Bento Grid Coordinates
    const float x0 = rect.left + padX;
    const float x1 = x0 + colW;
    const float x2 = x1 + colGap;
    const float x3 = x2 + colW;

    const float y0 = rect.top + 33.0f * scale;
    const float y1 = y0 + cardH;
    const float y2 = y1 + rowGap;
    const float y3 = y2 + cardH;

    const D2D1_RECT_F tile1Rect = D2D1::RectF(x0, y0, x1, y1); // Top-Left: Hero
    const D2D1_RECT_F tile2Rect = D2D1::RectF(x2, y0, x3, y1); // Top-Right: Peripherals
    const D2D1_RECT_F tile3Rect = D2D1::RectF(x0, y2, x1, y3); // Bottom-Left: Time & Power
    const D2D1_RECT_F tile4Rect = D2D1::RectF(x2, y2, x3, y3); // Bottom-Right: Health & Cycles

    // Tile 1: Battery Hero (Top-Left)
    DrawHeroTile(target, factory, boldTextFormat, textFormat, smallTextFormat,
                 tile1Rect, scale, data, cardFill, opacity);

    // Tile 2: Connected Peripherals (Top-Right)
    DrawPeripheralsTile(target, boldTextFormat, textFormat, smallTextFormat, iconFormat,
                        tile2Rect, scale, data.accessories, cardFill, opacity);

    // Tile 3: Time Remaining & Power Mode (Bottom-Left)
    DrawTimePowerTile(target, boldTextFormat, textFormat, smallTextFormat,
                      tile3Rect, scale, data, cardFill, opacity);

    // Tile 4: Battery Health & Cycle Count (Bottom-Right)
    DrawHealthCyclesTile(target, boldTextFormat, textFormat, smallTextFormat,
                         tile4Rect, scale, data, cardFill, opacity);
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

