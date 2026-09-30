#pragma once

// ============================================================================
// Dynamic Island for Windows - Universal Notification Pipeline
// Architecture: Native WinRT UserNotificationListener & Direct2D Transient Banner
// Supported Universal Apps: Discord, Google Chrome, Messenger (+ Generic Fallback)
// Zero-polling event push via Windows.UI.Notifications.Management
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <optional>
#include <cwctype>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.UI.Notifications.Management.h>

namespace DynamicIsland::Notifications {

using Microsoft::WRL::ComPtr;

// ----------------------------------------------------------------------------
// Data Model
// ----------------------------------------------------------------------------
struct NotificationItem {
    std::wstring id;
    std::wstring appName; // "Discord", "Chrome", "Messenger"
    std::wstring title;
    std::wstring body;
    double receivedAt = 0.0;
    double expiresAt = 0.0;
    bool active = false;
};

enum class KnownAppType {
    Unknown = 0,
    Discord,
    Chrome,
    Messenger
};

// ----------------------------------------------------------------------------
// Utility Helpers
// ----------------------------------------------------------------------------
inline double GetCurrentTimestamp() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

inline std::wstring ToLower(std::wstring_view s) {
    std::wstring lower(s);
    for (wchar_t& c : lower) {
        c = static_cast<wchar_t>(towlower(c));
    }
    return lower;
}

inline KnownAppType ClassifyApp(std::wstring_view appName, std::wstring_view aumid) {
    const std::wstring lowerName = ToLower(appName);
    const std::wstring lowerAumid = ToLower(aumid);

    if (lowerName.find(L"discord") != std::wstring::npos ||
        lowerAumid.find(L"discord") != std::wstring::npos) {
        return KnownAppType::Discord;
    }
    if (lowerName.find(L"chrome") != std::wstring::npos ||
        lowerAumid.find(L"chrome") != std::wstring::npos) {
        return KnownAppType::Chrome;
    }
    if (lowerName.find(L"messenger") != std::wstring::npos ||
        lowerAumid.find(L"messenger") != std::wstring::npos ||
        lowerAumid.find(L"facebook") != std::wstring::npos) {
        return KnownAppType::Messenger;
    }
    return KnownAppType::Unknown;
}

inline std::wstring NormalizeAppName(std::wstring_view appName, std::wstring_view aumid) {
    const KnownAppType type = ClassifyApp(appName, aumid);
    switch (type) {
        case KnownAppType::Discord:
            return L"Discord";
        case KnownAppType::Chrome:
            return L"Chrome";
        case KnownAppType::Messenger:
            return L"Messenger";
        default:
            if (!appName.empty()) {
                return std::wstring(appName);
            }
            if (!aumid.empty()) {
                const size_t pos = aumid.rfind(L'.');
                if (pos != std::wstring_view::npos && pos + 1 < aumid.length()) {
                    return std::wstring(aumid.substr(pos + 1));
                }
                return std::wstring(aumid);
            }
            return L"Notification";
    }
}

// ----------------------------------------------------------------------------
// WinRT Notification Extractor
// ----------------------------------------------------------------------------
inline NotificationItem ExtractNotification(
    winrt::Windows::UI::Notifications::UserNotification const& userNotification,
    double ttlSeconds = 5.0)
{
    NotificationItem item;
    item.id = std::to_wstring(userNotification.Id());
    item.receivedAt = GetCurrentTimestamp();
    item.expiresAt = item.receivedAt + ttlSeconds;
    item.active = true;

    std::wstring aumid;
    std::wstring displayName;

    try {
        auto appInfo = userNotification.AppInfo();
        if (appInfo) {
            aumid = std::wstring(appInfo.AppUserModelId());
            auto displayInfo = appInfo.DisplayInfo();
            if (displayInfo) {
                displayName = std::wstring(displayInfo.DisplayName());
            }
        }
    } catch (...) {}

    item.appName = NormalizeAppName(displayName, aumid);

    try {
        auto notif = userNotification.Notification();
        if (notif) {
            auto visual = notif.Visual();
            if (visual) {
                auto binding = visual.GetBinding(
                    winrt::Windows::UI::Notifications::KnownNotificationBindings::ToastGeneric());
                if (binding) {
                    auto texts = binding.GetTextElements();
                    const uint32_t count = texts.Size();
                    if (count > 0) {
                        item.title = std::wstring(texts.GetAt(0).Text());
                    }
                    if (count > 1) {
                        item.body = std::wstring(texts.GetAt(1).Text());
                    }
                    for (uint32_t i = 2; i < count; ++i) {
                        std::wstring extra = std::wstring(texts.GetAt(i).Text());
                        if (!extra.empty()) {
                            if (!item.body.empty()) item.body += L"  ";
                            item.body += extra;
                        }
                    }
                }
            }
        }
    } catch (...) {}

    if (item.title.empty()) {
        item.title = item.appName.empty() ? L"Notification" : item.appName;
    }

    return item;
}

// ----------------------------------------------------------------------------
// Notification Listener Pipeline Core
// ----------------------------------------------------------------------------
using NotificationCallback = std::function<void(const NotificationItem&)>;

class NotificationListenerEngine {
public:
    NotificationListenerEngine() = default;
    ~NotificationListenerEngine() {
        Stop();
    }

    NotificationListenerEngine(const NotificationListenerEngine&) = delete;
    NotificationListenerEngine& operator=(const NotificationListenerEngine&) = delete;

    bool Initialize(NotificationCallback callback = nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isListening) {
            return true;
        }

        m_callback = std::move(callback);

        try {
            m_listener = winrt::Windows::UI::Notifications::Management::UserNotificationListener::Current();
            if (!m_listener) {
                return false;
            }

            auto status = m_listener.GetAccessStatus();
            if (status == winrt::Windows::UI::Notifications::Management::UserNotificationListenerAccessStatus::Unspecified) {
                auto op = m_listener.RequestAccessAsync();
                status = op.get();
            }

            if (status != winrt::Windows::UI::Notifications::Management::UserNotificationListenerAccessStatus::Allowed) {
                return false;
            }

            // Zero-polling push subscription
            m_token = m_listener.NotificationChanged(
                [this](auto const& sender, auto const& args) {
                    OnNotificationChanged(sender, args);
                });

            m_isListening = true;
            return true;
        } catch (...) {
            m_isListening = false;
            return false;
        }
    }

    void Stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isListening && m_listener) {
            try {
                m_listener.NotificationChanged(m_token);
            } catch (...) {}
            m_isListening = false;
        }
    }

    bool IsListening() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isListening;
    }

    std::optional<NotificationItem> GetLatestNotification() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_hasNotification && m_latestNotification.active) {
            return m_latestNotification;
        }
        return std::nullopt;
    }

    void SetNotification(const NotificationItem& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_latestNotification = item;
        m_hasNotification = true;
    }

    void DismissNotification(const std::wstring& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_hasNotification && m_latestNotification.id == id) {
            m_latestNotification.active = false;
        }
    }

    std::vector<NotificationItem> QueryCurrentToasts() {
        std::vector<NotificationItem> results;
        try {
            if (!m_listener) {
                m_listener = winrt::Windows::UI::Notifications::Management::UserNotificationListener::Current();
            }
            if (m_listener) {
                auto toasts = m_listener.GetNotificationsAsync(
                    winrt::Windows::UI::Notifications::NotificationKinds::Toast).get();
                for (auto const& un : toasts) {
                    results.push_back(ExtractNotification(un, m_defaultTtlSeconds));
                }
            }
        } catch (...) {}
        return results;
    }

private:
    void OnNotificationChanged(
        winrt::Windows::UI::Notifications::Management::UserNotificationListener const& sender,
        winrt::Windows::UI::Notifications::UserNotificationChangedEventArgs const& args)
    {
        try {
            const auto kind = args.ChangeKind();
            if (kind == winrt::Windows::UI::Notifications::UserNotificationChangedKind::Added) {
                const uint32_t notifId = args.UserNotificationId();
                winrt::Windows::UI::Notifications::UserNotification un{ nullptr };

                try {
                    un = sender.GetNotification(notifId);
                } catch (...) {}

                if (!un) {
                    auto list = sender.GetNotificationsAsync(
                        winrt::Windows::UI::Notifications::NotificationKinds::Toast).get();
                    for (auto const& item : list) {
                        if (item.Id() == notifId) {
                            un = item;
                            break;
                        }
                    }
                    if (!un && list.Size() > 0) {
                        un = list.GetAt(list.Size() - 1);
                    }
                }

                if (un) {
                    NotificationItem item = ExtractNotification(un, m_defaultTtlSeconds);

                    NotificationCallback cb;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_latestNotification = item;
                        m_hasNotification = true;
                        cb = m_callback;
                    }

                    if (cb) {
                        cb(item);
                    }
                }
            }
        } catch (...) {}
    }

    mutable std::mutex m_mutex;
    winrt::Windows::UI::Notifications::Management::UserNotificationListener m_listener{ nullptr };
    winrt::event_token m_token{};
    bool m_isListening = false;
    NotificationItem m_latestNotification{};
    bool m_hasNotification = false;
    double m_defaultTtlSeconds = 5.0;
    NotificationCallback m_callback;
};

// ----------------------------------------------------------------------------
// Direct2D Transient Banner Renderer
// Layout: 320px x 48px rounded capsule
// Left: App vector badge (Discord blurple, Chrome tri-color, Messenger gradient)
// Center/Right: Title (Bold, primary) + Body preview (Muted secondary)
// ----------------------------------------------------------------------------
class NotificationBannerRenderer {
public:
    NotificationBannerRenderer() = default;

    explicit NotificationBannerRenderer(ID2D1RenderTarget* target, IDWriteFactory* dwriteFactory)
        : m_target(target), m_dwriteFactory(dwriteFactory) {}

    void SetRenderTarget(ID2D1RenderTarget* target, IDWriteFactory* dwriteFactory) {
        m_target = target;
        m_dwriteFactory = dwriteFactory;
    }

    // Direct2D Transient Banner Layout
    // Dimensions: 320px x 48px capsule
    void DrawNotificationBanner(const NotificationItem& notif, D2D1_RECT_F rect) {
        if (!m_target) return;

        // Ensure capsule minimum dimensions
        const float w = rect.right - rect.left;
        const float h = rect.bottom - rect.top;
        if (w < 100.0f || h < 24.0f) return;

        const float capsuleRadius = h * 0.5f; // Perfect capsule endcaps (24px for 48px height)
        const float cy = (rect.top + rect.bottom) * 0.5f;

        // 1. OLED Black Capsule Surface
        ComPtr<ID2D1SolidColorBrush> bgBrush;
        m_target->CreateSolidColorBrush(D2D1::ColorF(0.02f, 0.02f, 0.025f, 0.98f), &bgBrush);
        m_target->FillRoundedRectangle(D2D1::RoundedRect(rect, capsuleRadius, capsuleRadius), bgBrush.Get());

        // 2. Subtle Rim Border
        ComPtr<ID2D1SolidColorBrush> rimBrush;
        m_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), &rimBrush);
        m_target->DrawRoundedRectangle(D2D1::RoundedRect(rect, capsuleRadius, capsuleRadius), rimBrush.Get(), 1.0f);

        // 3. App Vector Badge on Left
        const float badgeSize = 30.0f;
        const float badgeLeft = rect.left + 12.0f;
        const D2D1_RECT_F badgeRect = D2D1::RectF(
            badgeLeft,
            cy - badgeSize * 0.5f,
            badgeLeft + badgeSize,
            cy + badgeSize * 0.5f);

        DrawAppBadge(notif.appName, badgeRect);

        // 4. Center / Right Text Layout
        const float tx = badgeRect.right + 12.0f;
        const float rightBound = rect.right - 14.0f;

        EnsureTextFormats();

        // Title: Bold primary text
        const D2D1_RECT_F titleRect = D2D1::RectF(tx, cy - 16.0f, rightBound, cy);
        ComPtr<ID2D1SolidColorBrush> primaryTextBrush;
        m_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.96f), &primaryTextBrush);

        const std::wstring& titleText = notif.title.empty() ? notif.appName : notif.title;
        if (m_titleFormat) {
            m_target->DrawTextW(
                titleText.c_str(),
                static_cast<UINT32>(titleText.size()),
                m_titleFormat.Get(),
                titleRect,
                primaryTextBrush.Get(),
                D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // Body Preview: Muted secondary text
        const D2D1_RECT_F bodyRect = D2D1::RectF(tx, cy + 1.0f, rightBound, cy + 16.5f);
        ComPtr<ID2D1SolidColorBrush> secondaryTextBrush;
        m_target->CreateSolidColorBrush(D2D1::ColorF(0.70f, 0.70f, 0.74f, 0.82f), &secondaryTextBrush);

        const std::wstring& bodyText = notif.body.empty() ? notif.appName : notif.body;
        if (m_bodyFormat) {
            m_target->DrawTextW(
                bodyText.c_str(),
                static_cast<UINT32>(bodyText.size()),
                m_bodyFormat.Get(),
                bodyRect,
                secondaryTextBrush.Get(),
                D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        // 5. Transient Countdown Progress Bar (if active and has duration)
        const double now = GetCurrentTimestamp();
        if (notif.expiresAt > notif.receivedAt && notif.expiresAt > now) {
            const float duration = static_cast<float>(notif.expiresAt - notif.receivedAt);
            const float remaining = static_cast<float>(notif.expiresAt - now);
            const float progress = std::clamp(remaining / duration, 0.0f, 1.0f);

            const float barLeft = tx;
            const float barRight = rightBound;
            const float barY = rect.bottom - 4.0f;
            const float barWidth = (barRight - barLeft) * progress;

            ComPtr<ID2D1SolidColorBrush> progressTrack;
            m_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f), &progressTrack);
            m_target->DrawLine(
                D2D1::Point2F(barLeft, barY),
                D2D1::Point2F(barRight, barY),
                progressTrack.Get(), 1.5f);

            ComPtr<ID2D1SolidColorBrush> progressBar;
            m_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.40f), &progressBar);
            m_target->DrawLine(
                D2D1::Point2F(barLeft, barY),
                D2D1::Point2F(barLeft + barWidth, barY),
                progressBar.Get(), 1.5f);
        }
    }

private:
    void EnsureTextFormats() {
        if (!m_dwriteFactory) return;

        if (!m_titleFormat) {
            m_dwriteFactory->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_SEMI_BOLD,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                12.5f,
                L"en-US",
                &m_titleFormat);
            if (m_titleFormat) {
                m_titleFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                m_titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                DWRITE_TRIMMING trimming = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
                ComPtr<IDWriteInlineObject> ellipsis;
                m_dwriteFactory->CreateEllipsisTrimmingSign(m_titleFormat.Get(), &ellipsis);
                m_titleFormat->SetTrimming(&trimming, ellipsis.Get());
            }
        }

        if (!m_bodyFormat) {
            m_dwriteFactory->CreateTextFormat(
                L"Segoe UI",
                nullptr,
                DWRITE_FONT_WEIGHT_REGULAR,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                11.0f,
                L"en-US",
                &m_bodyFormat);
            if (m_bodyFormat) {
                m_bodyFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                m_bodyFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                DWRITE_TRIMMING trimming = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
                ComPtr<IDWriteInlineObject> ellipsis;
                m_dwriteFactory->CreateEllipsisTrimmingSign(m_bodyFormat.Get(), &ellipsis);
                m_bodyFormat->SetTrimming(&trimming, ellipsis.Get());
            }
        }
    }

    void DrawAppBadge(std::wstring_view appName, D2D1_RECT_F badge) {
        const KnownAppType type = ClassifyApp(appName, L"");
        switch (type) {
            case KnownAppType::Discord:
                DrawDiscordBadge(badge);
                break;
            case KnownAppType::Chrome:
                DrawChromeBadge(badge);
                break;
            case KnownAppType::Messenger:
                DrawMessengerBadge(badge);
                break;
            default:
                DrawGenericBadge(badge);
                break;
        }
    }

    // Discord Vector Badge: Blurple #5865F2 + Crisp Clyde controller glyph
    void DrawDiscordBadge(D2D1_RECT_F badge) {
        const float br = 7.5f; // iOS rounded squircle
        ComPtr<ID2D1SolidColorBrush> blurpleBrush;
        m_target->CreateSolidColorBrush(
            D2D1::ColorF(88.0f / 255.0f, 101.0f / 255.0f, 242.0f / 255.0f, 1.0f),
            &blurpleBrush);
        m_target->FillRoundedRectangle(D2D1::RoundedRect(badge, br, br), blurpleBrush.Get());

        // Center Clyde Glyph
        const D2D1_POINT_2F bc = D2D1::Point2F(
            (badge.left + badge.right) * 0.5f,
            (badge.top + badge.bottom) * 0.5f);

        ComPtr<ID2D1Factory> factory;
        m_target->GetFactory(&factory);
        if (!factory) return;

        ComPtr<ID2D1PathGeometry> clydeGeometry;
        if (SUCCEEDED(factory->CreatePathGeometry(&clydeGeometry))) {
            ComPtr<ID2D1GeometrySink> sink;
            if (SUCCEEDED(clydeGeometry->Open(&sink))) {
                // Outer controller head
                sink->BeginFigure(D2D1::Point2F(bc.x - 7.5f, bc.y - 4.5f), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(bc.x - 3.5f, bc.y - 3.5f),
                    D2D1::Point2F(bc.x + 3.5f, bc.y - 3.5f),
                    D2D1::Point2F(bc.x + 7.5f, bc.y - 4.5f)));
                sink->AddLine(D2D1::Point2F(bc.x + 8.5f, bc.y - 1.5f));
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(bc.x + 8.8f, bc.y + 2.5f),
                    D2D1::Point2F(bc.x + 7.5f, bc.y + 5.0f),
                    D2D1::Point2F(bc.x + 5.5f, bc.y + 5.5f)));
                sink->AddLine(D2D1::Point2F(bc.x + 4.2f, bc.y + 4.0f));
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(bc.x + 2.5f, bc.y + 4.8f),
                    D2D1::Point2F(bc.x - 2.5f, bc.y + 4.8f),
                    D2D1::Point2F(bc.x - 4.2f, bc.y + 4.0f)));
                sink->AddLine(D2D1::Point2F(bc.x - 5.5f, bc.y + 5.5f));
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(bc.x - 7.5f, bc.y + 5.0f),
                    D2D1::Point2F(bc.x - 8.8f, bc.y + 2.5f),
                    D2D1::Point2F(bc.x - 8.5f, bc.y - 1.5f)));
                sink->AddLine(D2D1::Point2F(bc.x - 7.5f, bc.y - 4.5f));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();

                ComPtr<ID2D1SolidColorBrush> whiteBrush;
                m_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &whiteBrush);
                m_target->FillGeometry(clydeGeometry.Get(), whiteBrush.Get());

                // Two Clyde Eyes
                m_target->FillEllipse(
                    D2D1::Ellipse(D2D1::Point2F(bc.x - 3.2f, bc.y + 0.6f), 1.4f, 1.7f),
                    blurpleBrush.Get());
                m_target->FillEllipse(
                    D2D1::Ellipse(D2D1::Point2F(bc.x + 3.2f, bc.y + 0.6f), 1.4f, 1.7f),
                    blurpleBrush.Get());
            }
        }
    }

    // Chrome Vector Badge: Crisp Tri-Color (Red, Green, Yellow) + White border & Blue Core
    void DrawChromeBadge(D2D1_RECT_F badge) {
        const float br = 7.5f;
        ComPtr<ID2D1SolidColorBrush> darkPlate;
        m_target->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.13f, 0.15f, 1.0f), &darkPlate);
        m_target->FillRoundedRectangle(D2D1::RoundedRect(badge, br, br), darkPlate.Get());

        const D2D1_POINT_2F bc = D2D1::Point2F(
            (badge.left + badge.right) * 0.5f,
            (badge.top + badge.bottom) * 0.5f);
        const float r = 12.5f;

        ComPtr<ID2D1Factory> factory;
        m_target->GetFactory(&factory);
        if (!factory) return;

        constexpr float kPi = 3.14159265f;

        // Tri-color angles:
        // Sector 1: Red (top-left to top-right)
        // Sector 2: Green (top-right to bottom)
        // Sector 3: Yellow (bottom to top-left)
        auto drawSector = [&](float a1, float a2, D2D1_COLOR_F color) {
            ComPtr<ID2D1PathGeometry> geom;
            if (SUCCEEDED(factory->CreatePathGeometry(&geom))) {
                ComPtr<ID2D1GeometrySink> sink;
                if (SUCCEEDED(geom->Open(&sink))) {
                    sink->BeginFigure(bc, D2D1_FIGURE_BEGIN_FILLED);
                    sink->AddLine(D2D1::Point2F(bc.x + r * std::cos(a1), bc.y + r * std::sin(a1)));
                    sink->AddArc(D2D1::ArcSegment(
                        D2D1::Point2F(bc.x + r * std::cos(a2), bc.y + r * std::sin(a2)),
                        D2D1::SizeF(r, r),
                        0.0f,
                        D2D1_SWEEP_DIRECTION_CLOCKWISE,
                        D2D1_ARC_SIZE_SMALL));
                    sink->AddLine(bc);
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    sink->Close();

                    ComPtr<ID2D1SolidColorBrush> brush;
                    m_target->CreateSolidColorBrush(color, &brush);
                    m_target->FillGeometry(geom.Get(), brush.Get());
                }
            }
        };

        // Red: #EA4335
        drawSector(-5.0f * kPi / 6.0f, -kPi / 6.0f, D2D1::ColorF(234.0f / 255.0f, 67.0f / 255.0f, 53.0f / 255.0f));
        // Green: #34A853
        drawSector(-kPi / 6.0f, kPi / 2.0f, D2D1::ColorF(52.0f / 255.0f, 168.0f / 255.0f, 83.0f / 255.0f));
        // Yellow: #FBBC05
        drawSector(kPi / 2.0f, 7.0f * kPi / 6.0f, D2D1::ColorF(251.0f / 255.0f, 188.0f / 255.0f, 5.0f / 255.0f));

        // Center White Ring Separator
        ComPtr<ID2D1SolidColorBrush> whiteBrush;
        m_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &whiteBrush);
        m_target->FillEllipse(D2D1::Ellipse(bc, 6.4f, 6.4f), whiteBrush.Get());

        // Center Blue Core: #4285F4
        ComPtr<ID2D1SolidColorBrush> blueBrush;
        m_target->CreateSolidColorBrush(
            D2D1::ColorF(66.0f / 255.0f, 133.0f / 255.0f, 244.0f / 255.0f, 1.0f),
            &blueBrush);
        m_target->FillEllipse(D2D1::Ellipse(bc, 4.8f, 4.8f), blueBrush.Get());
    }

    // Messenger Vector Badge: Blue/Purple gradient + Speech bubble with lightning bolt
    void DrawMessengerBadge(D2D1_RECT_F badge) {
        // Gradient from Top-Right (#00B2FE) to Bottom-Left (#A824DE / #E80074)
        D2D1_GRADIENT_STOP stops[2];
        stops[0].position = 0.0f;
        stops[0].color = D2D1::ColorF(0.0f, 0.68f, 1.0f, 1.0f);     // Electric Blue
        stops[1].position = 1.0f;
        stops[1].color = D2D1::ColorF(0.72f, 0.12f, 0.88f, 1.0f);    // Vibrant Purple

        ComPtr<ID2D1GradientStopCollection> stopCollection;
        m_target->CreateGradientStopCollection(stops, 2, &stopCollection);

        if (stopCollection) {
            ComPtr<ID2D1LinearGradientBrush> gradBrush;
            m_target->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(badge.right, badge.top),
                    D2D1::Point2F(badge.left, badge.bottom)),
                stopCollection.Get(),
                &gradBrush);

            const D2D1_POINT_2F bc = D2D1::Point2F(
                (badge.left + badge.right) * 0.5f,
                (badge.top + badge.bottom) * 0.5f);

            ComPtr<ID2D1Factory> factory;
            m_target->GetFactory(&factory);
            if (factory) {
                // Speech bubble with lower-left tail
                ComPtr<ID2D1PathGeometry> bubbleGeometry;
                if (SUCCEEDED(factory->CreatePathGeometry(&bubbleGeometry))) {
                    ComPtr<ID2D1GeometrySink> sink;
                    if (SUCCEEDED(bubbleGeometry->Open(&sink))) {
                        sink->BeginFigure(D2D1::Point2F(bc.x - 7.0f, bc.y + 9.5f), D2D1_FIGURE_BEGIN_FILLED);
                        sink->AddLine(D2D1::Point2F(bc.x - 11.5f, bc.y + 12.0f));
                        sink->AddLine(D2D1::Point2F(bc.x - 9.5f, bc.y + 7.5f));
                        sink->AddBezier(D2D1::BezierSegment(
                            D2D1::Point2F(bc.x - 13.0f, bc.y + 4.0f),
                            D2D1::Point2F(bc.x - 13.0f, bc.y - 7.0f),
                            D2D1::Point2F(bc.x, bc.y - 12.0f)));
                        sink->AddBezier(D2D1::BezierSegment(
                            D2D1::Point2F(bc.x + 13.0f, bc.y - 12.0f),
                            D2D1::Point2F(bc.x + 13.0f, bc.y + 7.0f),
                            D2D1::Point2F(bc.x, bc.y + 11.5f)));
                        sink->AddLine(D2D1::Point2F(bc.x - 7.0f, bc.y + 9.5f));
                        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                        sink->Close();

                        m_target->FillGeometry(bubbleGeometry.Get(), gradBrush.Get());
                    }
                }

                // White Lightning Bolt Zig-Zag
                ComPtr<ID2D1PathGeometry> boltGeometry;
                if (SUCCEEDED(factory->CreatePathGeometry(&boltGeometry))) {
                    ComPtr<ID2D1GeometrySink> sink;
                    if (SUCCEEDED(boltGeometry->Open(&sink))) {
                        sink->BeginFigure(D2D1::Point2F(bc.x + 4.5f, bc.y - 5.5f), D2D1_FIGURE_BEGIN_FILLED);
                        sink->AddLine(D2D1::Point2F(bc.x - 1.2f, bc.y - 0.2f));
                        sink->AddLine(D2D1::Point2F(bc.x + 2.0f, bc.y - 0.2f));
                        sink->AddLine(D2D1::Point2F(bc.x - 4.5f, bc.y + 5.5f));
                        sink->AddLine(D2D1::Point2F(bc.x + 1.2f, bc.y + 0.2f));
                        sink->AddLine(D2D1::Point2F(bc.x - 2.0f, bc.y + 0.2f));
                        sink->AddLine(D2D1::Point2F(bc.x + 4.5f, bc.y - 5.5f));
                        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                        sink->Close();

                        ComPtr<ID2D1SolidColorBrush> whiteBrush;
                        m_target->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &whiteBrush);
                        m_target->FillGeometry(boltGeometry.Get(), whiteBrush.Get());
                    }
                }
            }
        }
    }

    // Generic App Fallback Badge: Frosted Plate + Modern Vector Bell Icon
    void DrawGenericBadge(D2D1_RECT_F badge) {
        const float br = 7.5f;
        ComPtr<ID2D1SolidColorBrush> plateBrush;
        m_target->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.20f, 0.24f, 1.0f), &plateBrush);
        m_target->FillRoundedRectangle(D2D1::RoundedRect(badge, br, br), plateBrush.Get());

        const D2D1_POINT_2F bc = D2D1::Point2F(
            (badge.left + badge.right) * 0.5f,
            (badge.top + badge.bottom) * 0.5f);

        ComPtr<ID2D1SolidColorBrush> glyphBrush;
        m_target->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.95f, 0.98f, 0.95f), &glyphBrush);

        // Bell Dome + Clapper
        ComPtr<ID2D1Factory> factory;
        m_target->GetFactory(&factory);
        if (factory) {
            ComPtr<ID2D1PathGeometry> bellGeometry;
            if (SUCCEEDED(factory->CreatePathGeometry(&bellGeometry))) {
                ComPtr<ID2D1GeometrySink> sink;
                if (SUCCEEDED(bellGeometry->Open(&sink))) {
                    sink->BeginFigure(D2D1::Point2F(bc.x, bc.y - 7.0f), D2D1_FIGURE_BEGIN_FILLED);
                    sink->AddBezier(D2D1::BezierSegment(
                        D2D1::Point2F(bc.x + 5.5f, bc.y - 6.5f),
                        D2D1::Point2F(bc.x + 6.0f, bc.y + 1.0f),
                        D2D1::Point2F(bc.x + 7.5f, bc.y + 4.5f)));
                    sink->AddLine(D2D1::Point2F(bc.x - 7.5f, bc.y + 4.5f));
                    sink->AddBezier(D2D1::BezierSegment(
                        D2D1::Point2F(bc.x - 6.0f, bc.y + 1.0f),
                        D2D1::Point2F(bc.x - 5.5f, bc.y - 6.5f),
                        D2D1::Point2F(bc.x, bc.y - 7.0f)));
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    sink->Close();

                    m_target->FillGeometry(bellGeometry.Get(), glyphBrush.Get());
                    // Bell clapper dot
                    m_target->FillEllipse(
                        D2D1::Ellipse(D2D1::Point2F(bc.x, bc.y + 6.2f), 1.8f, 1.8f),
                        glyphBrush.Get());
                }
            }
        }
    }

    ID2D1RenderTarget* m_target = nullptr;
    IDWriteFactory* m_dwriteFactory = nullptr;
    ComPtr<IDWriteTextFormat> m_titleFormat;
    ComPtr<IDWriteTextFormat> m_bodyFormat;
};

// ----------------------------------------------------------------------------
// Standalone Functional Entry Point matching requirement signature
// ----------------------------------------------------------------------------
inline void DrawNotificationBanner(
    ID2D1RenderTarget* target,
    IDWriteFactory* dwriteFactory,
    const NotificationItem& notif,
    D2D1_RECT_F rect)
{
    NotificationBannerRenderer renderer(target, dwriteFactory);
    renderer.DrawNotificationBanner(notif, rect);
}

} // namespace DynamicIsland::Notifications
