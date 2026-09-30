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
#include <cmath>
#include <algorithm>

using Microsoft::WRL::ComPtr;

// ============================================================================
// W3C DTCG Direct2D Semantic Design Tokens (Personal Windhawk Mods AGY 2.0)
// ============================================================================
namespace tokens {
    const D2D1_COLOR_F kBgCanvas         = D2D1::ColorF(0.047f, 0.055f, 0.078f, 1.0f); // #0C0E14
    const D2D1_COLOR_F kCardBase         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.055f);
    const D2D1_COLOR_F kCardRaised       = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.090f);
    const D2D1_COLOR_F kCardHairline     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.085f);
    const D2D1_COLOR_F kCardHairlineHigh = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.160f);
    const D2D1_COLOR_F kTrackBg          = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.090f);
    
    // Text Inks
    const D2D1_COLOR_F kTextPrimary      = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.96f);
    const D2D1_COLOR_F kTextSecondary    = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.58f);
    const D2D1_COLOR_F kTextTertiary     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.38f);
    
    // Status Accents
    const D2D1_COLOR_F kAppleGreen       = D2D1::ColorF(0.204f, 0.780f, 0.349f, 1.0f); // #34C759
    const D2D1_COLOR_F kAppleGreenMuted  = D2D1::ColorF(0.204f, 0.780f, 0.349f, 0.18f);
    const D2D1_COLOR_F kAppleAmber       = D2D1::ColorF(1.000f, 0.690f, 0.129f, 1.0f); // #FFB021
    const D2D1_COLOR_F kAppleAmberMuted  = D2D1::ColorF(1.000f, 0.690f, 0.129f, 0.18f);
    const D2D1_COLOR_F kAppleRed         = D2D1::ColorF(1.000f, 0.349f, 0.322f, 1.0f); // #FF5952
    
    // Gemini Brand Colors
    const D2D1_COLOR_F kGeminiCyan       = D2D1::ColorF(0.298f, 0.788f, 0.941f, 1.0f); // #4CC9F0
    const D2D1_COLOR_F kGeminiPurple     = D2D1::ColorF(0.608f, 0.447f, 0.796f, 1.0f); // #9B72CB
    const D2D1_COLOR_F kAgentRoleBg      = D2D1::ColorF(0.204f, 0.780f, 0.349f, 0.15f);
    const D2D1_COLOR_F kAgentRoleBorder  = D2D1::ColorF(0.204f, 0.780f, 0.349f, 0.35f);
}

// ============================================================================
// Direct2D Drawing Helpers
// ============================================================================

void DrawSoftDropShadow(ID2D1RenderTarget* rt, ID2D1Factory* factory, D2D1_RECT_F rect, float radius, float blurOffset = 3.0f) {
    (void)factory;
    ComPtr<ID2D1SolidColorBrush> shadowBrush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.22f), &shadowBrush);
    D2D1_RECT_F shadowRect = D2D1::RectF(
        rect.left - 1.0f,
        rect.top + blurOffset * 0.5f,
        rect.right + 1.0f,
        rect.bottom + blurOffset
    );
    rt->FillRoundedRectangle(D2D1::RoundedRect(shadowRect, radius + 1.0f, radius + 1.0f), shadowBrush.Get());
}

void DrawCircularProgressRing(
    ID2D1RenderTarget* rt,
    ID2D1Factory* factory,
    D2D1_POINT_2F center,
    float ringRadius,
    float strokeWidth,
    float percentage, // 0.0f to 1.0f
    D2D1_COLOR_F arcColor,
    D2D1_COLOR_F trackColor
) {
    // 1. Background Track
    ComPtr<ID2D1SolidColorBrush> trackBrush;
    rt->CreateSolidColorBrush(trackColor, &trackBrush);
    rt->DrawEllipse(D2D1::Ellipse(center, ringRadius, ringRadius), trackBrush.Get(), strokeWidth);

    // 2. Clamped Percentage Arc
    float clampedPct = std::clamp(percentage, 0.01f, 1.0f);
    float startAngle = -3.14159265f * 0.5f; // Top (-90 degrees)
    float sweepAngle = 2.0f * 3.14159265f * clampedPct;

    ComPtr<ID2D1PathGeometry> arcGeom;
    if (SUCCEEDED(factory->CreatePathGeometry(&arcGeom))) {
        ComPtr<ID2D1GeometrySink> sink;
        if (SUCCEEDED(arcGeom->Open(&sink))) {
            const int segments = 36;
            sink->BeginFigure(
                D2D1::Point2F(center.x + std::cos(startAngle) * ringRadius,
                              center.y + std::sin(startAngle) * ringRadius),
                D2D1_FIGURE_BEGIN_HOLLOW);
            for (int i = 1; i <= segments; ++i) {
                float a = startAngle + sweepAngle * (static_cast<float>(i) / segments);
                sink->AddLine(D2D1::Point2F(center.x + std::cos(a) * ringRadius,
                                            center.y + std::sin(a) * ringRadius));
            }
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();

            // Stroke style for rounded caps
            ComPtr<ID2D1StrokeStyle> strokeStyle;
            D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f
            );
            factory->CreateStrokeStyle(&strokeProps, nullptr, 0, &strokeStyle);

            ComPtr<ID2D1SolidColorBrush> arcBrush;
            rt->CreateSolidColorBrush(arcColor, &arcBrush);
            rt->DrawGeometry(arcGeom.Get(), arcBrush.Get(), strokeWidth, strokeStyle.Get());
        }
    }
}

// ============================================================================
// Collapsed Notch Renderer (Fixed Inset & Optical Centering)
// ============================================================================
void DrawCollapsedNotchPill(
    ID2D1RenderTarget* rt,
    ID2D1Factory* factory,
    IDWriteTextFormat* textFmt,
    D2D1_RECT_F rect,
    bool showTextInfo,
    const wchar_t* infoText,
    bool isWarning = false
) {
    float pillRadius = (rect.bottom - rect.top) * 0.5f;

    // Drop shadow
    DrawSoftDropShadow(rt, factory, rect, pillRadius, 4.0f);

    // Pill Base (Obsidian Glass)
    ComPtr<ID2D1SolidColorBrush> fillBrush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.06f, 0.06f, 0.08f, 0.95f), &fillBrush);
    rt->FillRoundedRectangle(D2D1::RoundedRect(rect, pillRadius, pillRadius), fillBrush.Get());

    // Hairline Border
    ComPtr<ID2D1SolidColorBrush> borderBrush;
    rt->CreateSolidColorBrush(tokens::kCardHairlineHigh, &borderBrush);
    rt->DrawRoundedRectangle(D2D1::RoundedRect(rect, pillRadius, pillRadius), borderBrush.Get(), 1.0f);

    // Geometric Math to Guarantee NO Border Clipping:
    // When capsule height is H (e.g. 34px), pill radius R = 17px.
    // In compact pill mode (no text), center is precisely in the middle.
    // In extended pill mode, center is anchored with exact left inset = pillRadius.
    D2D1_POINT_2F orbCenter;
    if (showTextInfo) {
        orbCenter = D2D1::Point2F(rect.left + pillRadius, (rect.top + rect.bottom) * 0.5f);
    } else {
        orbCenter = D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f);
    }

    // Outer Ambient Glow Ring (Radius 9.5px, max clearance to 17px border is >7.5px!)
    const float ringRadius = 9.5f;
    const float ringStroke = 1.8f;
    D2D1_COLOR_F statusColor = isWarning ? tokens::kAppleAmber : tokens::kAppleGreen;

    ComPtr<ID2D1SolidColorBrush> ringBgBrush;
    rt->CreateSolidColorBrush(D2D1::ColorF(statusColor.r, statusColor.g, statusColor.b, 0.16f), &ringBgBrush);
    rt->DrawEllipse(D2D1::Ellipse(orbCenter, ringRadius, ringRadius), ringBgBrush.Get(), ringStroke);

    // Active Segment Arc (75% circular arc indicating active session)
    DrawCircularProgressRing(rt, factory, orbCenter, ringRadius, ringStroke, 0.75f, statusColor, D2D1::ColorF(0,0,0,0));

    // Solid Status Orb Core (Radius 4.0px, perfectly contained inside ring)
    ComPtr<ID2D1SolidColorBrush> orbBrush;
    rt->CreateSolidColorBrush(statusColor, &orbBrush);
    rt->FillEllipse(D2D1::Ellipse(orbCenter, 4.0f, 4.0f), orbBrush.Get());

    // Specular Highlight on Orb
    ComPtr<ID2D1SolidColorBrush> specBrush;
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.65f), &specBrush);
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(orbCenter.x - 1.2f, orbCenter.y - 1.2f), 1.2f, 1.2f), specBrush.Get());

    // Optional Turn Text Label
    if (showTextInfo && infoText && textFmt) {
        ComPtr<ID2D1SolidColorBrush> textBrush;
        rt->CreateSolidColorBrush(tokens::kTextPrimary, &textBrush);
        D2D1_RECT_F textRect = D2D1::RectF(
            orbCenter.x + ringRadius + 8.0f,
            rect.top,
            rect.right - 14.0f,
            rect.bottom
        );
        textFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt->DrawTextW(infoText, static_cast<UINT32>(wcslen(infoText)), textFmt, textRect, textBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

// ============================================================================
// Expanded Island Card Renderer (AGY 2.0 Full Dashboard)
// ============================================================================
void DrawExpandedAgyDashboard(
    ID2D1RenderTarget* rt,
    ID2D1Factory* factory,
    IDWriteTextFormat* titleFormat,
    IDWriteTextFormat* boldFormat,
    IDWriteTextFormat* bodyFormat,
    IDWriteTextFormat* smallFormat,
    IDWriteTextFormat* microFormat,
    D2D1_RECT_F rect
) {
    const float cardRadius = 20.0f;

    // 1. Soft Ambient Drop Shadow
    DrawSoftDropShadow(rt, factory, rect, cardRadius, 10.0f);

    // 2. Main Card Surface (Deep Obsidian Glass)
    ComPtr<ID2D1SolidColorBrush> cardBg;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.06f, 0.065f, 0.082f, 0.96f), &cardBg);
    rt->FillRoundedRectangle(D2D1::RoundedRect(rect, cardRadius, cardRadius), cardBg.Get());

    // 3. Crisp Hairline Border
    ComPtr<ID2D1SolidColorBrush> cardBorder;
    rt->CreateSolidColorBrush(tokens::kCardHairlineHigh, &cardBorder);
    rt->DrawRoundedRectangle(D2D1::RoundedRect(rect, cardRadius, cardRadius), cardBorder.Get(), 1.0f);

    // Reusable brushes
    ComPtr<ID2D1SolidColorBrush> brushWhite;
    ComPtr<ID2D1SolidColorBrush> brushMuted;
    ComPtr<ID2D1SolidColorBrush> brushTertiary;
    ComPtr<ID2D1SolidColorBrush> brushGreen;
    ComPtr<ID2D1SolidColorBrush> brushAmber;
    ComPtr<ID2D1SolidColorBrush> brushCyan;
    ComPtr<ID2D1SolidColorBrush> brushPurple;
    ComPtr<ID2D1SolidColorBrush> brushBentoBg;
    ComPtr<ID2D1SolidColorBrush> brushBentoBorder;

    rt->CreateSolidColorBrush(tokens::kTextPrimary, &brushWhite);
    rt->CreateSolidColorBrush(tokens::kTextSecondary, &brushMuted);
    rt->CreateSolidColorBrush(tokens::kTextTertiary, &brushTertiary);
    rt->CreateSolidColorBrush(tokens::kAppleGreen, &brushGreen);
    rt->CreateSolidColorBrush(tokens::kAppleAmber, &brushAmber);
    rt->CreateSolidColorBrush(tokens::kGeminiCyan, &brushCyan);
    rt->CreateSolidColorBrush(tokens::kGeminiPurple, &brushPurple);
    rt->CreateSolidColorBrush(tokens::kCardBase, &brushBentoBg);
    rt->CreateSolidColorBrush(tokens::kCardHairline, &brushBentoBorder);

    const float padX = 18.0f;

    // ========================================================================
    // ZONE A: Header Row (Title, Turn/Step Capsule, Status Badge)
    // ========================================================================
    const float headerTop = rect.top + 16.0f;
    const float headerBottom = headerTop + 24.0f;

    // A1. Antigravity Icon Glyph ("✦")
    if (boldFormat) {
        D2D1_RECT_F iconRect = D2D1::RectF(rect.left + padX, headerTop, rect.left + padX + 18.0f, headerBottom);
        titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kSparkle[] = L"✦";
        rt->DrawTextW(kSparkle, 1, titleFormat, iconRect, brushCyan.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // A2. Clean Conversation Title
    if (titleFormat) {
        D2D1_RECT_F titleRect = D2D1::RectF(rect.left + padX + 20.0f, headerTop, rect.right - 160.0f, headerBottom);
        titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kTitle[] = L"Personal Windhawk Mods";
        rt->DrawTextW(kTitle, static_cast<UINT32>(wcslen(kTitle)), titleFormat, titleRect, brushWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // A3. Turn / Step Capsule Pill
    const float pillW = 136.0f;
    const float pillH = 22.0f;
    D2D1_RECT_F turnPillRect = D2D1::RectF(
        rect.right - padX - pillW,
        headerTop + 1.0f,
        rect.right - padX,
        headerTop + 1.0f + pillH
    );
    rt->FillRoundedRectangle(D2D1::RoundedRect(turnPillRect, 11.0f, 11.0f), brushBentoBg.Get());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(turnPillRect, 11.0f, 11.0f), brushBentoBorder.Get(), 1.0f);

    // Active Green Pulsing Dot inside Turn Pill
    D2D1_POINT_2F dotCenter = D2D1::Point2F(turnPillRect.left + 11.0f, (turnPillRect.top + turnPillRect.bottom) * 0.5f);
    rt->FillEllipse(D2D1::Ellipse(dotCenter, 3.5f, 3.5f), brushGreen.Get());

    // Turn & Step Text
    if (smallFormat) {
        D2D1_RECT_F turnTextRect = D2D1::RectF(turnPillRect.left + 20.0f, turnPillRect.top, turnPillRect.right - 8.0f, turnPillRect.bottom);
        smallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kTurnStep[] = L"Turn 5 • Step 140";
        rt->DrawTextW(kTurnStep, static_cast<UINT32>(wcslen(kTurnStep)), smallFormat, turnTextRect, brushWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // ========================================================================
    // ZONE B: Latest Message Snippet Box with Role Attribution Badge
    // ========================================================================
    const float msgBoxTop = headerBottom + 10.0f;
    const float msgBoxHeight = 50.0f;
    D2D1_RECT_F msgBoxRect = D2D1::RectF(rect.left + padX, msgBoxTop, rect.right - padX, msgBoxTop + msgBoxHeight);

    rt->FillRoundedRectangle(D2D1::RoundedRect(msgBoxRect, 9.0f, 9.0f), brushBentoBg.Get());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(msgBoxRect, 9.0f, 9.0f), brushBentoBorder.Get(), 1.0f);

    // B1. Role Badge ("AGENT" in emerald badge)
    const float badgeLeft = msgBoxRect.left + 10.0f;
    const float badgeTop = msgBoxRect.top + 8.0f;
    const float badgeW = 46.0f;
    const float badgeH = 15.0f;
    D2D1_RECT_F badgeRect = D2D1::RectF(badgeLeft, badgeTop, badgeLeft + badgeW, badgeTop + badgeH);

    ComPtr<ID2D1SolidColorBrush> agentBgBrush;
    ComPtr<ID2D1SolidColorBrush> agentBorderBrush;
    rt->CreateSolidColorBrush(tokens::kAgentRoleBg, &agentBgBrush);
    rt->CreateSolidColorBrush(tokens::kAgentRoleBorder, &agentBorderBrush);

    rt->FillRoundedRectangle(D2D1::RoundedRect(badgeRect, 4.0f, 4.0f), agentBgBrush.Get());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(badgeRect, 4.0f, 4.0f), agentBorderBrush.Get(), 0.8f);

    if (microFormat) {
        microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kAgentTag[] = L"AGENT";
        rt->DrawTextW(kAgentTag, static_cast<UINT32>(wcslen(kAgentTag)), microFormat, badgeRect, brushGreen.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // B2. Relative Timestamp
    if (microFormat) {
        D2D1_RECT_F timeRect = D2D1::RectF(badgeRect.right + 8.0f, badgeTop, msgBoxRect.right - 10.0f, badgeTop + badgeH);
        microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kTime[] = L"Just now";
        rt->DrawTextW(kTime, static_cast<UINT32>(wcslen(kTime)), microFormat, timeRect, brushTertiary.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // B3. Message Snippet Content
    if (bodyFormat) {
        D2D1_RECT_F snippetRect = D2D1::RectF(
            msgBoxRect.left + 10.0f,
            msgBoxRect.top + 26.0f,
            msgBoxRect.right - 10.0f,
            msgBoxRect.bottom - 6.0f
        );
        bodyFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        bodyFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        const wchar_t kSnippet[] = L"Rendered Direct2D offscreen snapshot with zero notch border clipping.";
        rt->DrawTextW(kSnippet, static_cast<UINT32>(wcslen(kSnippet)), bodyFormat, snippetRect, brushWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // ========================================================================
    // ZONE C: Gemini Models Usage Limits (Dual Circular Progress Rings)
    // ========================================================================
    const float quotaTop = msgBoxRect.bottom + 8.0f;
    const float quotaHeight = 64.0f;
    const float quotaTotalWidth = (rect.right - rect.left) - padX * 2.0f;
    const float colGap = 8.0f;
    const float quotaCardWidth = (quotaTotalWidth - colGap) * 0.5f;

    // C1. Left Card: 5-Hour Limit
    D2D1_RECT_F fiveHrRect = D2D1::RectF(rect.left + padX, quotaTop, rect.left + padX + quotaCardWidth, quotaTop + quotaHeight);
    rt->FillRoundedRectangle(D2D1::RoundedRect(fiveHrRect, 9.0f, 9.0f), brushBentoBg.Get());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(fiveHrRect, 9.0f, 9.0f), brushBentoBorder.Get(), 1.0f);

    D2D1_POINT_2F ring5hCenter = D2D1::Point2F(fiveHrRect.left + 28.0f, (fiveHrRect.top + fiveHrRect.bottom) * 0.5f);
    DrawCircularProgressRing(rt, factory, ring5hCenter, 18.0f, 3.2f, 0.80f, tokens::kGeminiCyan, tokens::kTrackBg);

    // Percentage inside ring
    if (boldFormat) {
        D2D1_RECT_F pct5hRect = D2D1::RectF(ring5hCenter.x - 18.0f, ring5hCenter.y - 18.0f, ring5hCenter.x + 18.0f, ring5hCenter.y + 18.0f);
        smallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kPct5h[] = L"80%";
        rt->DrawTextW(kPct5h, static_cast<UINT32>(wcslen(kPct5h)), smallFormat, pct5hRect, brushWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Texts to the right of ring
    float text5hLeft = ring5hCenter.x + 26.0f;
    if (microFormat && smallFormat) {
        D2D1_RECT_F lbl5hRect = D2D1::RectF(text5hLeft, fiveHrRect.top + 9.0f, fiveHrRect.right - 8.0f, fiveHrRect.top + 22.0f);
        microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kLbl5h[] = L"5-HOUR LIMIT";
        rt->DrawTextW(kLbl5h, static_cast<UINT32>(wcslen(kLbl5h)), microFormat, lbl5hRect, brushMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        D2D1_RECT_F val5hRect = D2D1::RectF(text5hLeft, fiveHrRect.top + 23.0f, fiveHrRect.right - 8.0f, fiveHrRect.top + 39.0f);
        boldFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        boldFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kVal5h[] = L"80% remaining";
        rt->DrawTextW(kVal5h, static_cast<UINT32>(wcslen(kVal5h)), boldFormat, val5hRect, brushWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        D2D1_RECT_F rst5hRect = D2D1::RectF(text5hLeft, fiveHrRect.top + 40.0f, fiveHrRect.right - 8.0f, fiveHrRect.top + 54.0f);
        microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kRst5h[] = L"Resets in 4h 27m";
        rt->DrawTextW(kRst5h, static_cast<UINT32>(wcslen(kRst5h)), microFormat, rst5hRect, brushTertiary.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // C2. Right Card: Weekly Limit
    D2D1_RECT_F weeklyRect = D2D1::RectF(fiveHrRect.right + colGap, quotaTop, rect.right - padX, quotaTop + quotaHeight);
    rt->FillRoundedRectangle(D2D1::RoundedRect(weeklyRect, 9.0f, 9.0f), brushBentoBg.Get());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(weeklyRect, 9.0f, 9.0f), brushBentoBorder.Get(), 1.0f);

    D2D1_POINT_2F ringWkCenter = D2D1::Point2F(weeklyRect.left + 28.0f, (weeklyRect.top + weeklyRect.bottom) * 0.5f);
    DrawCircularProgressRing(rt, factory, ringWkCenter, 18.0f, 3.2f, 0.48f, tokens::kGeminiPurple, tokens::kTrackBg);

    // Percentage inside weekly ring
    if (smallFormat) {
        D2D1_RECT_F pctWkRect = D2D1::RectF(ringWkCenter.x - 18.0f, ringWkCenter.y - 18.0f, ringWkCenter.x + 18.0f, ringWkCenter.y + 18.0f);
        smallFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kPctWk[] = L"48%";
        rt->DrawTextW(kPctWk, static_cast<UINT32>(wcslen(kPctWk)), smallFormat, pctWkRect, brushWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Texts to the right of weekly ring
    float textWkLeft = ringWkCenter.x + 26.0f;
    if (microFormat && smallFormat) {
        D2D1_RECT_F lblWkRect = D2D1::RectF(textWkLeft, weeklyRect.top + 9.0f, weeklyRect.right - 8.0f, weeklyRect.top + 22.0f);
        microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kLblWk[] = L"WEEKLY LIMIT";
        rt->DrawTextW(kLblWk, static_cast<UINT32>(wcslen(kLblWk)), microFormat, lblWkRect, brushMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        D2D1_RECT_F valWkRect = D2D1::RectF(textWkLeft, weeklyRect.top + 23.0f, weeklyRect.right - 8.0f, weeklyRect.top + 39.0f);
        boldFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        boldFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kValWk[] = L"48% remaining";
        rt->DrawTextW(kValWk, static_cast<UINT32>(wcslen(kValWk)), boldFormat, valWkRect, brushWhite.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        D2D1_RECT_F rstWkRect = D2D1::RectF(textWkLeft, weeklyRect.top + 40.0f, weeklyRect.right - 8.0f, weeklyRect.top + 54.0f);
        microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kRstWk[] = L"Resets in 5d 23h";
        rt->DrawTextW(kRstWk, static_cast<UINT32>(wcslen(kRstWk)), microFormat, rstWkRect, brushTertiary.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // ========================================================================
    // ZONE D: Context Compaction Warning & Token Budget Track
    // ========================================================================
    const float contextTop = quotaTop + quotaHeight + 8.0f;
    const float contextHeight = 36.0f;
    D2D1_RECT_F contextRect = D2D1::RectF(rect.left + padX, contextTop, rect.right - padX, contextTop + contextHeight);

    rt->FillRoundedRectangle(D2D1::RoundedRect(contextRect, 9.0f, 9.0f), brushBentoBg.Get());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(contextRect, 9.0f, 9.0f), brushBentoBorder.Get(), 1.0f);

    // D1. Context Header Label & Numbers
    if (microFormat && smallFormat) {
        D2D1_RECT_F ctxLblRect = D2D1::RectF(contextRect.left + 10.0f, contextRect.top + 5.0f, contextRect.left + 150.0f, contextRect.top + 17.0f);
        microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kCtxLbl[] = L"CONTEXT TOKENS";
        rt->DrawTextW(kCtxLbl, static_cast<UINT32>(wcslen(kCtxLbl)), microFormat, ctxLblRect, brushMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        D2D1_RECT_F ctxValRect = D2D1::RectF(contextRect.right - 230.0f, contextRect.top + 5.0f, contextRect.right - 10.0f, contextRect.top + 17.0f);
        microFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        microFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kCtxVal[] = L"156k / 200k (78%) • Compaction at 80%";
        rt->DrawTextW(kCtxVal, static_cast<UINT32>(wcslen(kCtxVal)), microFormat, ctxValRect, brushAmber.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // D2. Progress Bar Track
    const float barLeft = contextRect.left + 10.0f;
    const float barRight = contextRect.right - 10.0f;
    const float barWidth = barRight - barLeft;
    const float barTop = contextRect.top + 21.0f;
    const float barBottom = barTop + 6.0f;
    const float barRadius = 3.0f;

    D2D1_RECT_F trackRect = D2D1::RectF(barLeft, barTop, barRight, barBottom);
    ComPtr<ID2D1SolidColorBrush> trackBrush;
    rt->CreateSolidColorBrush(tokens::kTrackBg, &trackBrush);
    rt->FillRoundedRectangle(D2D1::RoundedRect(trackRect, barRadius, barRadius), trackBrush.Get());

    // 78% Fill (Amber Warning tint as it approaches 80%)
    const float fillWidth = barWidth * 0.78f;
    D2D1_RECT_F fillRect = D2D1::RectF(barLeft, barTop, barLeft + fillWidth, barBottom);
    rt->FillRoundedRectangle(D2D1::RoundedRect(fillRect, barRadius, barRadius), brushAmber.Get());

    // 80% Threshold Hairline Indicator
    const float thresholdX = barLeft + barWidth * 0.80f;
    ComPtr<ID2D1SolidColorBrush> threshBrush;
    rt->CreateSolidColorBrush(tokens::kAppleRed, &threshBrush);
    rt->DrawLine(
        D2D1::Point2F(thresholdX, barTop - 2.0f),
        D2D1::Point2F(thresholdX, barBottom + 2.0f),
        threshBrush.Get(),
        1.5f
    );
}

// ============================================================================
// Main Snapshot Orchestrator
// ============================================================================
int wmain(int argc, wchar_t* argv[]) {
    (void)argc;
    (void)argv;

    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::wcerr << L"[ERROR] Failed to initialize COM: 0x" << std::hex << hr << std::endl;
        return 1;
    }

    const UINT canvasWidth = 620;
    const UINT canvasHeight = 440;
    const wchar_t* outFilename = L"snapshot_agy.png";

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
    hr = wicFactory->CreateBitmap(canvasWidth, canvasHeight, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &wicBitmap);
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

    ComPtr<ID2D1RenderTarget> rt;
    hr = d2dFactory->CreateWicBitmapRenderTarget(wicBitmap.Get(), props, &rt);
    if (FAILED(hr) || !rt) {
        std::wcerr << L"[ERROR] Failed to create WicBitmapRenderTarget: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 5. Create DirectWrite Factory & Typography Formats
    ComPtr<IDWriteFactory> dwriteFactory;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &dwriteFactory);
    if (FAILED(hr) || !dwriteFactory) {
        std::wcerr << L"[ERROR] Failed to create DWriteFactory: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    ComPtr<IDWriteTextFormat> sectionTitleFmt;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us", &sectionTitleFmt);

    ComPtr<IDWriteTextFormat> titleFmt;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &titleFmt);

    ComPtr<IDWriteTextFormat> boldFmt;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &boldFmt);

    ComPtr<IDWriteTextFormat> bodyFmt;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us", &bodyFmt);

    ComPtr<IDWriteTextFormat> smallFmt;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &smallFmt);

    ComPtr<IDWriteTextFormat> microFmt;
    dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 8.5f, L"en-us", &microFmt);

    // 6. Draw Content to Target
    rt->BeginDraw();
    rt->Clear(tokens::kBgCanvas);

    ComPtr<ID2D1SolidColorBrush> labelBrush;
    rt->CreateSolidColorBrush(tokens::kTextSecondary, &labelBrush);

    // --- Section 1: Collapsed Notch Inset Fix Showcase ---
    if (sectionTitleFmt) {
        sectionTitleFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        sectionTitleFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const wchar_t kSec1[] = L"1. COLLAPSED NOTCH / SATELLITE PILL (OPTICAL INSET FIX - NO BORDER CLIPPING)";
        rt->DrawTextW(kSec1, static_cast<UINT32>(wcslen(kSec1)), sectionTitleFmt.Get(),
            D2D1::RectF(24.0f, 16.0f, 596.0f, 32.0f), labelBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Pill A: Ultra-Compact Status Pill (54x34) - Normal Active State
    D2D1_RECT_F pillARect = D2D1::RectF(30.0f, 40.0f, 84.0f, 74.0f);
    DrawCollapsedNotchPill(rt.Get(), d2dFactory.Get(), smallFmt.Get(), pillARect, false, nullptr, false);

    // Pill B: Extended Status Pill with Turn Info (148x34)
    D2D1_RECT_F pillBRect = D2D1::RectF(108.0f, 40.0f, 256.0f, 74.0f);
    DrawCollapsedNotchPill(rt.Get(), d2dFactory.Get(), smallFmt.Get(), pillBRect, true, L"Turn 5 • Step 140", false);

    // Pill C: Compaction Warning State Pill (54x34)
    D2D1_RECT_F pillCRect = D2D1::RectF(280.0f, 40.0f, 334.0f, 74.0f);
    DrawCollapsedNotchPill(rt.Get(), d2dFactory.Get(), smallFmt.Get(), pillCRect, false, nullptr, true);

    // Pill D: Extended Warning State Pill (168x34)
    D2D1_RECT_F pillDRect = D2D1::RectF(358.0f, 40.0f, 526.0f, 74.0f);
    DrawCollapsedNotchPill(rt.Get(), d2dFactory.Get(), smallFmt.Get(), pillDRect, true, L"78% Tokens (Compacting)", true);

    // Sub-labels for Pill Showcase
    if (microFmt) {
        microFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        const wchar_t kDescA[] = L"A: 54x34 Active";
        rt->DrawTextW(kDescA, static_cast<UINT32>(wcslen(kDescA)), microFmt.Get(),
            D2D1::RectF(30.0f, 78.0f, 100.0f, 92.0f), labelBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        const wchar_t kDescB[] = L"B: 148x34 Extended Telemetry";
        rt->DrawTextW(kDescB, static_cast<UINT32>(wcslen(kDescB)), microFmt.Get(),
            D2D1::RectF(108.0f, 78.0f, 260.0f, 92.0f), labelBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        const wchar_t kDescC[] = L"C: Warning Orb";
        rt->DrawTextW(kDescC, static_cast<UINT32>(wcslen(kDescC)), microFmt.Get(),
            D2D1::RectF(280.0f, 78.0f, 350.0f, 92.0f), labelBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        const wchar_t kDescD[] = L"D: Compaction Alert";
        rt->DrawTextW(kDescD, static_cast<UINT32>(wcslen(kDescD)), microFmt.Get(),
            D2D1::RectF(358.0f, 78.0f, 520.0f, 92.0f), labelBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // --- Section 2: Expanded Island Dashboard (AGY 2.0) ---
    if (sectionTitleFmt) {
        const wchar_t kSec2[] = L"2. EXPANDED ISLAND CARD (DIRECT2D AGY 2.0 DASHBOARD)";
        rt->DrawTextW(kSec2, static_cast<UINT32>(wcslen(kSec2)), sectionTitleFmt.Get(),
            D2D1::RectF(24.0f, 108.0f, 596.0f, 124.0f), labelBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Expanded Card Dimensions: 420px wide x 230px high, horizontally centered
    const float cardW = 420.0f;
    const float cardH = 230.0f;
    const float cardLeft = (canvasWidth - cardW) * 0.5f; // 100.0f
    const float cardTop = 136.0f;
    D2D1_RECT_F expandedCardRect = D2D1::RectF(cardLeft, cardTop, cardLeft + cardW, cardTop + cardH);

    DrawExpandedAgyDashboard(
        rt.Get(),
        d2dFactory.Get(),
        titleFmt.Get(),
        boldFmt.Get(),
        bodyFmt.Get(),
        smallFmt.Get(),
        microFmt.Get(),
        expandedCardRect
    );

    hr = rt->EndDraw();
    if (FAILED(hr)) {
        std::wcerr << L"[ERROR] Direct2D EndDraw failed: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    // 7. Save output to PNG via WIC
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
    hr = frame->SetSize(canvasWidth, canvasHeight);
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppPBGRA;
    hr = frame->SetPixelFormat(&pixelFormat);
    hr = frame->WriteSource(wicBitmap.Get(), nullptr);
    hr = frame->Commit();
    hr = encoder->Commit();

    if (SUCCEEDED(hr)) {
        std::wcout << L"[SUCCESS] Rendered AGY 2.0 Direct2D snapshot to: " << outFilename << std::endl;
    } else {
        std::wcerr << L"[ERROR] Failed to commit PNG frame: 0x" << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    CoUninitialize();
    return 0;
}
