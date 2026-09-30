#pragma once

#ifndef ISLAND_COMMON_HPP
#define ISLAND_COMMON_HPP

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
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <optional>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <utility>

enum class UiLanguage {
    English,
    French,
    Spanish,
    German,
    Portuguese,
    Italian,
    Russian,
    Turkish,
    Hindi,
    ChineseSimplified,
    Japanese,
    Korean,
    Count,
};


constexpr wchar_t kWindowClass[] = L"Windhawk.DynamicIslandForWindows";
constexpr UINT WM_APP_LAYOUT_CHANGED = WM_APP + 0x442;
constexpr UINT WM_APP_NEW_EVENT = WM_APP + 0x443;
constexpr UINT WM_APP_MOUSE_WAKE = WM_APP + 0x446;
constexpr UINT WM_APP_CAPSLOCK = WM_APP + 0x444;
// RegisterHotKey cannot associate a hot key with a window created by another
// thread, and the same applies to tearing one down. LoadSettings runs on
// Windhawk's settings thread while the overlay window belongs to the render
// thread, so hotkey and backdrop changes are posted across and applied there.
constexpr UINT WM_APP_APPLY_HOTKEY = WM_APP + 0x447;
constexpr UINT WM_APP_APPLY_BACKDROP = WM_APP + 0x448;
constexpr int ID_HIDE_SHOW_HOTKEY = 1;
constexpr float kRenderPadX = 28.0f;
constexpr float kRenderPadY = 22.0f;
constexpr UINT kClipboardImageThumbMaxDim = 160;  // longer-side cap for the clipboard image thumbnail

// Layout for the expanded media dashboard. DrawMedia paints every element at an
// offset from the content rect it is handed, and OverlayWndProc hit-tests in
// that same content space (see MediaContentFromClient), so both sides read the
// constants below and cannot drift apart.
namespace MediaLayout {
    constexpr float kExpandedWidth = 380.0f;
    constexpr float kExpandedHeight = 184.0f;

    constexpr float kScrubberY = 114.0f;
    constexpr float kScrubMargin = 24.0f;
    constexpr float kScrubBarLeftInset = 48.0f;
    constexpr float kScrubBarRightInset = 48.0f;

    constexpr float kScrubHitHalfHeight = 14.0f;
    constexpr float kScrubHitPadX = 6.0f;

    // Distance from the content rect's left/right edge to each end of the
    // scrubber bar. Used by the content-space hit test so the bar's clickable
    // span follows the actual pill width instead of assuming 380px.
    constexpr float kScrubInsetLeft = kScrubMargin + kScrubBarLeftInset;
    constexpr float kScrubInsetRight = kScrubMargin + kScrubBarRightInset;

    // Transport controls exactly as DrawMedia paints them: y is measured down
    // from the content rect's top edge, x as an offset from the content rect's
    // horizontal center. The hit test derives its boxes from these same values.
    constexpr float kControlsY = 148.0f;
    constexpr float kControlSpacing = 64.0f;
    constexpr float kNavButtonRadius = 16.0f;
    constexpr float kPlayButtonRadius = 22.0f;
    // Extra slack around each button so the round targets are comfortable to
    // hit. Kept small enough that prev/next never overlap play/pause:
    // prev spans -86..-42, play spans -28..+28.
    constexpr float kControlHitPad = 6.0f;

    // Expanded-layout album art, as drawn by DrawMedia.
    constexpr float kArtInsetX = 24.0f;
    constexpr float kArtInsetY = 20.0f;
    constexpr float kArtSize = 64.0f;

    // The expanded dashboard is faded in by DrawMedia via
    // expandedAlpha = clamp((contentHeight - 60) / 60), so anything at or below
    // this height is still the collapsed pill and must not be hit-tested.
    constexpr float kExpandedMinHeight = 60.0f;
}

// Layout for the File Tray card (#33). Shared by DrawFileTrayDashboard and the
// row hit test in OverlayWndProc for the same reason MediaLayout is shared.
namespace FileTrayLayout {
    constexpr float kPadX = 26.0f;
    constexpr float kListTop = 46.0f;
    constexpr float kListBottomInset = 16.0f;
    constexpr float kRowHeight = 30.0f;
    constexpr float kRowGap = 6.0f;

    constexpr int VisibleRowCapacity(float contentHeight) {
        const float span = contentHeight - kListBottomInset - kListTop + kRowGap;
        const int capacity = static_cast<int>(span / (kRowHeight + kRowGap));
        return capacity < 1 ? 1 : capacity;
    }
}

// Layout for the collapsed idle strip (the clock, and optionally a weather
// reading beside it).
//
// Fixes windhawk-mods#5086. Note the tracker: plain "#33"-style references in
// this file are issues on devcode90/Dynamic-Island-for-Windows, whereas
// "windhawk-mods#NNNN" is the ramensoftware/windhawk-mods tracker that the
// published mod is submitted through. Both use bare #N in their own context, so
// the upstream ones are always qualified here.
//
// This width used to be a bare constant in ActivityForKind -- 96px, or 170px
// with weather -- which was wrong in both directions. Measured at the idle
// format's own face and size, "9:41" is 22.7px of ink in a 96px pill, so 24%
// occupancy and the rest dead air. The weather variant then split the pill
// 50/50 at its centre no matter how wide the two strings actually were.
//
// The fixed width also truncated long clocks, though only at larger type: the
// text box was a flat 84px while the idle font is 13.0f * textScale, so
// "10:41:32 PM" fits at textScale 1.0 (67.7px) but is clipped at 1.4 (94.8px)
// and 1.6 (108.4px).
//
// The strip is now measured and sized to its real content. As with
// GameOverlayLayout, the sizer (the render loop) and the painter
// (DrawIdleDashboard) both read these constants so they cannot drift apart.
namespace IdleStripLayout {
    constexpr float kPadX = 14.0f;         // inner padding at each end
    constexpr float kSlotGap = 9.0f;       // clock <-> divider <-> weather
    constexpr float kDividerWidth = 1.0f;
    constexpr float kDividerInsetY = 9.0f;

    // Room reserved on the right for the mic/camera dot. DrawPrivacyDots anchors
    // it at rect.right - 20 with a 4px radius, so 18px clears it without letting
    // the text slide under it. Previously this was stolen from the text box while
    // the pill stayed 96px, so the clock just re-centred into a narrower slot.
    constexpr float kPrivacyReserve = 18.0f;

    constexpr float kHeight = 36.0f;

    // The stadium cap is kHeight/2 at each end, so anything below ~2x the height
    // stops reading as a pill. The ceiling keeps a long localized string from
    // turning the island into a bar.
    constexpr float kMinWidth = 72.0f;
    constexpr float kMaxWidth = 280.0f;

    // Measured widths are rounded up to this so sub-pixel text metrics can't
    // resize the layered window every frame.
    constexpr float kWidthQuantum = 2.0f;
}

// Layout for the game overlay strip. The size the island animates to is decided
// in the render loop, while the contents are painted by DrawGameOverlay. Those
// two kept their own copies of the card width, padding and height, so widening a
// card in one place left the other sizing the island for the old value -- the
// strip would reserve room for four cards and then drop the last one on the
// "ran out of room" check. Both read this now.
//
// Deliberately free of g_settings so it can sit up here with the other layout
// namespaces; callers pass what they know.
namespace GameOverlayLayout {
    struct Metrics {
        float padX;
        float padY;
        float cardW;
        float gap;
        float fpsW;
        float fpsGap;
        float radius;
        float height;
    };

    // Cards are wider than the original 52/62 so a label and a three-digit value
    // sit in separate bands without touching, and the corner radius matches the
    // 9-10px used by every other card instead of the old 16, which on a 44px-tall
    // card was very nearly a stadium.
    constexpr Metrics For(bool compact) {
        return compact ? Metrics{10.0f,  9.0f, 62.0f, 6.0f, 74.0f, 7.0f,  9.0f, 56.0f}
                       : Metrics{12.0f, 11.0f, 74.0f, 7.0f, 88.0f, 8.0f, 10.0f, 68.0f};
    }

    constexpr float kMinWidth = 140.0f;

    constexpr float Width(bool compact, bool showFps, int metricCount) {
        const Metrics m = For(compact);
        float w = m.padX * 2.0f;
        if (showFps) {
            w += m.fpsW + m.fpsGap;
        }
        if (metricCount > 0) {
            w += static_cast<float>(metricCount) * m.cardW +
                 static_cast<float>(metricCount - 1) * m.gap;
        }
        return w < kMinWidth ? kMinWidth : w;
    }
}

// ── Media dashboard hit-test geometry ────────────────────────────────────────
// DrawMedia publishes the exact content-space rect it painted into, together
// with the scale that maps content px to client px. OverlayWndProc converts
// mouse positions through this instead of assuming the pill is centered in the
// client area, which it is not when the notch / border-merged offset, the hover
// scale, or a split (two-pill) layout is in play. Keeping one published source
// of truth is what stops the drawn buttons and their hit boxes from drifting
// apart -- the drift that made prev/next clicks fall through to "open the app".
// The fields are guarded by a seqlock rather than read independently: the render
// thread rewrites them every frame, and during the expand animation the content
// height sweeps ~36 -> 184px, so a reader that caught half of one frame and half
// of the next could misplace a hit box for a click. g_mediaHitSeq is odd while a
// write is in progress; readers retry until they see the same even value twice.
// The payload stays in relaxed atomics so there is no formal data race; the
// seqlock counter is what provides consistency *across* the fields.
inline std::atomic<unsigned> g_mediaHitSeq{0};
inline std::atomic<float> g_mediaHitLeft{0.0f};
inline std::atomic<float> g_mediaHitTop{0.0f};
inline std::atomic<float> g_mediaHitRight{0.0f};
inline std::atomic<float> g_mediaHitBottom{0.0f};
inline std::atomic<float> g_mediaHitScale{1.0f};
inline std::atomic<unsigned long long> g_mediaHitStamp{0};

struct MediaContentPoint {
    bool valid = false;
    float x = 0.0f;        // relative to the content rect's left edge
    float y = 0.0f;        // relative to the content rect's top edge
    float width = 0.0f;    // content rect width
    float height = 0.0f;   // content rect height
};

// The conversion and hit-test helpers live further down, just after Clamp().

enum class IslandKind {
    Idle,
    Media,
    Progress,
    Clipboard,
    Notification,
    Volume,
    BatteryLow,
    CapsLock,
    Device,
    Bluetooth,
    Timer,
    DoNotDisturb,
    Split,
};

enum class BluetoothDeviceCategory {
    Headphones,
    Speaker,
    Mouse,
    Keyboard,
    Phone,
    Generic,
};

#ifndef BLUETOOTH_ACCESSORY_INFO_DEFINED
#define BLUETOOTH_ACCESSORY_INFO_DEFINED
struct BluetoothAccessoryInfo {
    std::wstring name;
    BluetoothDeviceCategory category = BluetoothDeviceCategory::Generic;
    int batteryPercent = -1;
    bool connected = false;
};
#endif

enum class Position {
    TopCenter,
    TopLeft,
    TopRight,
    BottomCenter,
    BottomLeft,
    BottomRight,
};

// Anything anchored to the bottom edge shares the same vertical placement and
// cannot use the top-edge macOS notch shape.
constexpr bool IsBottomPosition(Position position) {
    return position == Position::BottomCenter ||
           position == Position::BottomLeft ||
           position == Position::BottomRight;
}

enum class AccentMode {
    Auto,
    System,
    Custom,
};

enum class AnimationStyle {
    Smooth,
    Default,
    Bouncy,
    Snappy,
};

enum class CalendarAccentMode {
    Red,
    System,
};

// Curated palettes. These replace the original four (OLED Black, Fluent,
// Midnight Blue, Deep Purple), which were built for a material that painted a
// glass highlight along every edge. With the edge lighting gone, a palette has
// to carry the whole look on flat fills, so each of these is tuned for
// separation by value alone -- and one is light, which the old set had none of.
//
// Order is load-bearing: it is persisted as an integer index, so append new
// entries at the end rather than inserting. Custom must stay last.
enum class ThemePreset {
    Obsidian,
    Graphite,
    Slate,
    Nord,
    Evergreen,
    Espresso,
    Plum,
    Porcelain,
    LiquidGlass,
    AppleDark,
    Custom,
};

// One table drives the settings dropdown, the right-click menu labels and the
// resolved colors. It used to be three separate lists, which is how the menu
// ended up carrying special cases for palette indexes that no longer matched.
//
// Secondary text is held at roughly 4.5:1 against its own background in every
// row, because no edge highlight is left to help muted labels separate from the
// surface behind them.
struct ThemePalette {
    const wchar_t* id;      // value stored in Themes.ThemePreset
    const wchar_t* label;   // right-click menu label
    const wchar_t* bg;
    const wchar_t* fg;
    const wchar_t* sec;
    const wchar_t* border;
};

static constexpr ThemePalette kThemePalettes[] = {
    {L"obsidian",    L"Obsidian",                     L"#08080A", L"#FFFFFF", L"#9B9BA5", L"#1E1E22"},
    {L"graphite",    L"Graphite",                     L"#1C1C1E", L"#FFFFFF", L"#A8A8AE", L"#323236"},
    {L"slate",       L"Slate",                        L"#111721", L"#E9EEF6", L"#92A2B8", L"#232C3A"},
    {L"nord",        L"Nord",                         L"#2E3440", L"#ECEFF4", L"#A7B0C0", L"#3B4252"},
    {L"evergreen",   L"Evergreen",                    L"#0C1512", L"#E4F1EA", L"#8CAE9D", L"#1A2A23"},
    {L"espresso",    L"Espresso",                     L"#1A1512", L"#F6EEE7", L"#B7A395", L"#2E2420"},
    {L"plum",        L"Plum",                         L"#16111C", L"#F1EAF6", L"#AC9BB9", L"#281F33"},
    {L"porcelain",   L"Porcelain (Light)",            L"#F5F6F8", L"#14161A", L"#5B6270", L"#D8DBE1"},
    {L"liquidglass", L"OS Liquid Glass 26",           L"#141418", L"#FFFFFF", L"#A0A0AA", L"#383844"},
    {L"appledark",   L"Apple Dark (iOS Native)",      L"#000000", L"#FFFFFF", L"#86868B", L"#0D0D0E"},
};

// Custom sits one past the last real palette. Derived, so adding a palette
// cannot leave a stale literal behind.
static constexpr int kCustomThemeIndex = static_cast<int>(ARRAYSIZE(kThemePalettes));

// Persisted under a new key. Retiring the original palettes renumbered these
// integers, and reusing "ColorTheme" would have silently reinterpreted a saved
// 4 (previously Custom) as the palette that now sits at index 4.
static constexpr const wchar_t* kThemeValueName = L"ColorThemeV2";
static constexpr const wchar_t* kLegacyThemeValueName = L"ColorTheme";

// Base menu command id for the theme submenu; entries occupy
// [kThemeMenuIdBase, kThemeMenuIdBase + kCustomThemeIndex].
static constexpr UINT kThemeMenuIdBase = 20;

// Maps a preset id to its palette index, accepting the retired ids too: a
// user's stored setting keeps its old string after the option disappears from
// the dropdown, so each legacy id is pointed at the new palette closest to it.
inline int ThemeIndexFromId(std::wstring_view id) {
    auto iequals = [](std::wstring_view a, std::wstring_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (towlower(a[i]) != towlower(b[i])) return false;
        }
        return true;
    };

    for (int i = 0; i < kCustomThemeIndex; ++i) {
        if (iequals(id, kThemePalettes[i].id)) return i;
    }
    if (iequals(id, L"custom")) return kCustomThemeIndex;

    if (iequals(id, L"apple") || iequals(id, L"apple-dark") || iequals(id, L"appledark") || iequals(id, L"ios")) return 9; // -> AppleDark
    if (iequals(id, L"liquidglass") || iequals(id, L"liquid-glass")) return 8; // -> LiquidGlass
    if (iequals(id, L"oled-black")) return 0;     // -> Obsidian
    if (iequals(id, L"fluent") ||
        iequals(id, L"mica") ||
        iequals(id, L"dark-gray")) return 1;      // -> Graphite
    if (iequals(id, L"midnight-blue")) return 2;  // -> Slate
    if (iequals(id, L"deep-purple")) return 6;    // -> Plum
    return -1;
}

// Real Windows blur / acrylic painted behind the island (#59).
enum class BackdropMaterial {
    None,
    Blur,
    Acrylic,
};

enum class ContourBorderMode {
    Default,
    Auto,
    Borderless,
};

struct Settings {
    Position position = Position::TopCenter;
    int targetMonitor = 0;
    int offsetX = 0;
    int offsetY = 0;
    bool separateExpandedOffsetY = false;  // #83
    int offsetYExpanded = 0;               // #83
    float sizeScale = 1.0f;
    std::wstring fontFamily;
    AccentMode accentMode = AccentMode::Auto;
    D2D1_COLOR_F customAccent = D2D1::ColorF(0x4cc9f0);
    int targetFps = 0; // 0 = Auto
    AnimationStyle animationStyle = AnimationStyle::Default;
    float animationSpeed = 1.0f;
    bool media = true;
    bool mediaAutoExpand = false;
    bool clipboard = true;
    bool statusCountdownProgress = false;
    bool battery = true;
    bool batteryModule = true;
    bool progress = true;
    bool volume = true;
    CalendarAccentMode calendarAccent = CalendarAccentMode::Red;
    bool privacyDots = true;
    bool privacyDotsMic = true;
    bool privacyDotsCam = true;
    bool privacyDotsPulse = true;
    // Modules.CapsLock -- fixes windhawk-mods#4352, which asked for a way to turn
    // the Caps Lock / Num Lock indicator off.
    //
    // Enforced in three places, because any one of them alone leaks:
    //   ChooseActivities        - the pill is never selected for display
    //   WM_APP_CAPSLOCK handler - no state is recorded, no nudge is triggered
    //   CapsLockHookWanted      - WH_KEYBOARD_LL is not installed at all
    // The activity gate is the one that actually hides the pill; the others stop
    // the work leading up to it.
    bool capsLock = true;
    bool timerEnabled = true;
    bool hideShowHotkeyEnabled = true;
    UINT hideShowModifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
    UINT hideShowVk = 'D';
    bool bluetoothIndicator = true;
    bool bluetoothShowBattery = true;
    D2D1_COLOR_F privacyDotsMicHex = D2D1::ColorF(1.0f, 0.584f, 0.0f, 1.0f); // #FF9500
    D2D1_COLOR_F privacyDotsCamHex = D2D1::ColorF(0.133f, 0.776f, 0.239f, 1.0f); // #10B981
    float tintOpacity = 0.72f;
    float pillOpacity = 0.96f;
    bool gameOverlay = false;
    bool showMetricText = true;
    bool weather = true;
    std::wstring weatherCity;
    std::wstring customLocation;
    bool weatherFahrenheit = false;
    int autoHideIdleSeconds = 0;
    bool autoHideFullscreen = true;
    bool autoHideMaximized = true;
    bool borderMergedMode = false;
    bool unhideOnHover = true;
    bool alwaysOnTop = true;
    bool expandOnHover = true;
    bool autoDpiScale = true;
    bool w11Style = false;
    bool notchStyle = false;
    // Color customization
    ThemePreset themePreset = ThemePreset::AppleDark;
    D2D1_COLOR_F pillBgColor = D2D1::ColorF(0.031f, 0.031f, 0.039f, 1.0f); // #08080A
    D2D1_COLOR_F textPrimaryColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f); // #FFFFFF
    D2D1_COLOR_F textSecondaryColor = D2D1::ColorF(0.608f, 0.608f, 0.647f, 1.0f); // #9B9BA5
    ContourBorderMode contourBorderMode = ContourBorderMode::Default;
    bool contourBorderEnabled = true;
    D2D1_COLOR_F contourBorderColor = D2D1::ColorF(0.200f, 0.200f, 0.220f, 1.0f); // #333338
    bool clockAccentGlow = true;
    bool privacyDotsEnabled = true;
    bool privacyDotPulsing = true;
    D2D1_COLOR_F micDotColor = D2D1::ColorF(1.0f, 0.624f, 0.039f, 1.0f); // Apple systemOrange #FF9F0A
    D2D1_COLOR_F camDotColor = D2D1::ColorF(0.188f, 0.820f, 0.345f, 1.0f); // Apple systemGreen #30D158
    bool hardwareMonitorModule = true;
    bool doNotDisturbIndicator = true;
    bool notificationRespectDnD = true;

    // ── Premium material / redesign ──────────────────────────────────────────
    bool materialDepth = true;      // downward depth shading + accent bloom
    bool dropShadow = false;        // soft shadow under the island (disabled for razor-sharp contour)
    float accentBloom = 1.0f;       // 0..2 multiplier on the accent wash
    float textScale = 1.0f;         // independent typography scale (#41)
    BackdropMaterial backdropMaterial = BackdropMaterial::None;  // #59
    float backdropTint = 0.55f;     // acrylic tint strength, 0..1
    float backdropFillAlpha = 0.45f;  // how opaque the pill's own fill stays

    // ── Clock / date presentation (#61) ──────────────────────────────────────
    bool showSeconds = false;
    bool use24HourClock = false;    // false = follow system locale
    bool clockFollowSystem = true;
    std::wstring dateFormat;        // empty = locale default
    bool dateFirst = false;         // show date before time in the idle strip

    // ── Localization (#35) ───────────────────────────────────────────────────
    std::wstring language = L"auto";

    // ── File tray (#33) ──────────────────────────────────────────────────────
    bool fileTrayModule = false;
    int fileTrayMaxItems = 10;

    // ── Media auto-expand exclusions (#62) ───────────────────────────────────
    std::vector<std::wstring> mediaExpandBlocklist;

    // ── Game overlay options (#25) ───────────────────────────────────────────
    bool gameOverlayShowFps = true;
    bool gameOverlayShowCpu = true;
    bool gameOverlayShowGpu = true;
    bool gameOverlayShowRam = true;
    bool gameOverlayShowDisk = true;
    bool gameOverlayCompact = false;
};

struct BitmapPixels {
    std::vector<uint8_t> bgra;
    UINT width = 0;
    UINT height = 0;
    uint64_t generation = 0;
    D2D1_COLOR_F sampledAccent = D2D1::ColorF(0x4cc9f0);
};

struct MediaSnapshot {
    bool available = false;
    bool playing = false;
    std::wstring title;
    std::wstring artist;
    std::wstring albumTitle;
    std::wstring sourceAppUserModelId;
    std::wstring sourceName;
    std::wstring sourceBadge;
    BitmapPixels art;
    BitmapPixels sourceIcon;
    uint64_t artGeneration = 0;
    uint64_t sourceIconGeneration = 0;
    double artChangedAt = 0.0;
    double titleChangedAt = 0.0;
    int64_t positionTicks = 0;
    int64_t endTicks = 0;
    int64_t lastUpdatedTicks = 0;
};

struct ClipboardSnapshot {
    bool active = false;
    bool image = false;
    std::wstring text;
    std::wstring appName;
    BitmapPixels appIcon;
    BitmapPixels imagePreview;  // decoded thumbnail when the clipboard holds an image
    double expiresAt = 0.0;
};

struct BatterySnapshot {
    bool active = false;
    bool low = false;
    bool charging = false;
    int percent = 100;
    int secondsRemaining = -1;
    int secondsToFull = -1;
    float powerRateWatts = 0.0f;
    int healthPercent = -1;  // DesignedCapacity vs FullChargeCapacity
    int cycleCount = -1;
    std::wstring powerSchemeName = L"Balanced";
    double expiresAt = 0.0;
};

struct ProgressSnapshot {
    bool active = false;
    int percent = 0;
};

struct NotificationSnapshot {
    bool active = false;
    std::wstring app;
    std::wstring title;
    std::wstring body;
    BitmapPixels icon;
    double expiresAt = 0.0;
};

struct VolumeSnapshot {
    bool active = false;
    int percent = 0;
    bool muted = false;
    std::wstring deviceName;
    double expiresAt = 0.0;
};

struct CapsLockSnapshot {
    bool active = false;
    bool capsOn = false;
    bool numOn = false;
    bool isNumEvent = false;
    double expiresAt = 0.0;
};

struct TimerSnapshot {
    bool active = false;          // a session exists (running or paused)
    bool running = false;         // currently counting down
    bool isBreak = false;         // work vs break session
    int totalSeconds = 0;
    double endsAt = 0.0;          // NowSeconds() at completion, while running
    double remainingAtPause = 0.0;
    bool justFinished = false;
    double finishedExpiresAt = 0.0;
};

enum class DeviceEventType {
    Connected,
    Disconnected,
};

struct DeviceSnapshot {
    bool active = false;
    DeviceEventType eventType = DeviceEventType::Connected;
    std::wstring deviceName;  // e.g. "USB Drive" or "Bluetooth Device"
    bool isBluetoothLike = false;
    double expiresAt = 0.0;
};

struct BluetoothDeviceSnapshot {
    bool active = false;
    bool connected = true;   // true = just connected, false = just disconnected
    std::wstring deviceName;
    int batteryPercent = -1; // -1 = unknown/unavailable
    BluetoothDeviceCategory category = BluetoothDeviceCategory::Generic;
    double expiresAt = 0.0;
};

struct SystemSnapshot {
    int volumePercent = 0;
    bool volumeMuted = false;
    int cpuPercent = 0;
    int memoryPercent = 0;
    float memoryUsedGB = 0.0f;
    float memoryTotalGB = 0.0f;
    int diskFreePercent = 0;
    int diskPercent = 0;
    int renderFps = 0;
    int gpuPercent = -1;
    float netUpMbps = 0.0f;
    float netDownMbps = 0.0f;
    bool charging = false;
    bool micActive = false;      // orange dot: microphone in use
    bool cameraActive = false;   // green dot: camera in use
    std::wstring foregroundTitle;
    std::wstring micApp;
    std::wstring cameraApp;
};

struct Activity {
    IslandKind kind = IslandKind::Idle;
    float width = 120.0f;
    float height = 36.0f;
};

struct WeatherSnapshot {
    bool hasData = false;
    float temperature = 0.0f;
    int weatherCode = 0;
    std::wstring city;
    std::wstring weatherDesc;
    std::wstring windSpeed;
    std::wstring windDir;
    std::wstring humidity;
    std::wstring feelsLike;
    double lastUpdated = 0.0;
};

struct DoNotDisturbSnapshot {
    bool active = false;
    bool enabled = false;
    double expiresAt = 0.0;
};

// One file parked on the island's shelf (#33). Kept deliberately small: the
// tray holds references, never copies of the files themselves.
struct FileTrayItem {
    std::wstring path;
    std::wstring name;
    uint64_t sizeBytes = 0;
    bool isDirectory = false;
    BitmapPixels icon;
};

struct SharedState {
    MediaSnapshot media;
    ClipboardSnapshot clipboard;
    NotificationSnapshot notification;
    VolumeSnapshot volume;
    CapsLockSnapshot capsLock;
    TimerSnapshot timer;
    DeviceSnapshot device;
    BluetoothDeviceSnapshot bluetoothDevice;
    DoNotDisturbSnapshot doNotDisturb;
    BatterySnapshot battery;
    ProgressSnapshot progress;
    SystemSnapshot system;
    WeatherSnapshot weather;
    std::array<float, 48> waveform{};
    size_t waveformWrite = 0;
    bool muted = false;
    std::vector<FileTrayItem> fileTrayItems;
};

struct SpringValue {
    float value = 0.0f;
    float velocity = 0.0f;
    float target = 0.0f;

    void Reset(float v) {
        value = target = v;
        velocity = 0.0f;
    }

    void Step(float totalDt, float stiffness, float damping) {
        const float kFixedDt = 0.0005f;
        while (totalDt > 0.0f) {
            float dt = std::min(totalDt, kFixedDt);
            const float displacement = value - target;
            const float acceleration = -stiffness * displacement - damping * velocity;
            velocity += acceleration * dt;
            value += velocity * dt;
            totalDt -= dt;
        }

        if (std::fabs(value - target) < 0.01f && std::fabs(velocity) < 0.01f) {
            value = target;
            velocity = 0.0f;
        }
    }
};


inline Settings g_settings;
inline std::mutex g_settingsMutex;

inline Settings GetSettingsCopy() {
    std::lock_guard lock(g_settingsMutex);
    return g_settings;
}

inline std::mutex g_stateMutex;
inline SharedState g_state;
inline std::atomic<uint64_t> g_artGenerationCounter = 0;
inline std::atomic<int> g_idleTab = 0;
inline std::atomic<int> g_uiLanguage{static_cast<int>(UiLanguage::English)};

inline double NowSeconds() {
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    return std::chrono::duration<double>(clock::now() - start).count();
}

inline float Clamp(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

inline int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}


// ── Dashboard tab loop ───────────────────────────────────────────────────────
// Single source of truth for how many tabs are in the mouse-wheel scroll loop.
// The first tab's meaning depends on context -- the media view when something is
// playing, the clock when idle -- but the *count* is identical, which is why the
// renderer, the scroll handler and the hit tests can all share this. That
// arithmetic used to be copy-pasted in seven places, which is exactly how they
// drifted out of sync.
inline int ActiveTabCount(const Settings& settings) {
    int count = 2;  // primary (media or clock) + calendar
    if (settings.weather) ++count;
    if (settings.hardwareMonitorModule) ++count;
    if (settings.fileTrayModule) ++count;
    if (settings.batteryModule) ++count;
    return count;
}

inline int BatteryTabIndex(const Settings& settings) {
    if (!settings.batteryModule) {
        return -1;
    }
    return 2 + (settings.weather ? 1 : 0) + (settings.hardwareMonitorModule ? 1 : 0) + (settings.fileTrayModule ? 1 : 0);
}

inline int NormalizedTabIndex(const Settings& settings) {
    const int total = std::max(1, ActiveTabCount(settings));
    const int raw = g_idleTab.load(std::memory_order_relaxed);
    return ((raw % total) + total) % total;
}

// Tab order is always: primary, calendar, weather?, hardware?, file tray?
// The expensive GPU / network counters are only sampled while the hardware card
// is actually on screen, so that check needs the card's real index rather than
// assuming it is the last tab -- which stopped being true once the File Tray
// was added after it.
inline int HardwareMonitorTabIndex(const Settings& settings) {
    if (!settings.hardwareMonitorModule) {
        return -1;
    }
    return 2 + (settings.weather ? 1 : 0);
}

inline int FileTrayTabIndex(const Settings& settings) {
    if (!settings.fileTrayModule) {
        return -1;
    }
    return 2 + (settings.weather ? 1 : 0) + (settings.hardwareMonitorModule ? 1 : 0);
}


inline bool PtInCapsule(const RECT& rc, float radius, POINT pt) {
    if (rc.right <= rc.left || rc.bottom <= rc.top) {
        return false;
    }
    if (pt.x < rc.left || pt.x > rc.right || pt.y < rc.top || pt.y > rc.bottom) {
        return false;
    }
    float r = std::min(radius, std::min((rc.right - rc.left) * 0.5f, (rc.bottom - rc.top) * 0.5f));
    if (r <= 1.0f) return true;

    if (pt.x < rc.left + r && pt.y < rc.top + r) {
        float dx = static_cast<float>(pt.x) - (static_cast<float>(rc.left) + r);
        float dy = static_cast<float>(pt.y) - (static_cast<float>(rc.top) + r);
        return (dx * dx + dy * dy) <= (r * r);
    }
    if (pt.x > rc.right - r && pt.y < rc.top + r) {
        float dx = static_cast<float>(pt.x) - (static_cast<float>(rc.right) - r);
        float dy = static_cast<float>(pt.y) - (static_cast<float>(rc.top) + r);
        return (dx * dx + dy * dy) <= (r * r);
    }
    if (pt.x < rc.left + r && pt.y > rc.bottom - r) {
        float dx = static_cast<float>(pt.x) - (static_cast<float>(rc.left) + r);
        float dy = static_cast<float>(pt.y) - (static_cast<float>(rc.bottom) - r);
        return (dx * dx + dy * dy) <= (r * r);
    }
    if (pt.x > rc.right - r && pt.y > rc.bottom - r) {
        float dx = static_cast<float>(pt.x) - (static_cast<float>(rc.right) - r);
        float dy = static_cast<float>(pt.y) - (static_cast<float>(rc.bottom) - r);
        return (dx * dx + dy * dy) <= (r * r);
    }
    return true;
}

inline UiLanguage LanguageFromTag(std::wstring_view tag) {
    struct Entry { const wchar_t* tag; UiLanguage lang; };
    static constexpr Entry kTags[] = {
        {L"en", UiLanguage::English},   {L"fr", UiLanguage::French},
        {L"es", UiLanguage::Spanish},   {L"de", UiLanguage::German},
        {L"pt", UiLanguage::Portuguese},{L"it", UiLanguage::Italian},
        {L"ru", UiLanguage::Russian},   {L"tr", UiLanguage::Turkish},
        {L"hi", UiLanguage::Hindi},     {L"zh", UiLanguage::ChineseSimplified},
        {L"ja", UiLanguage::Japanese},  {L"ko", UiLanguage::Korean},
    };
    if (tag.size() < 2) {
        return UiLanguage::English;
    }
    for (const Entry& e : kTags) {
        // Prefix match so "pt-BR" / "zh-Hans-CN" resolve to their base language.
        if (_wcsnicmp(tag.data(), e.tag, 2) == 0) {
            return e.lang;
        }
    }
    return UiLanguage::English;
}

// Resolves the "auto" language setting from the user's Windows UI language.
UiLanguage DetectSystemLanguage() {
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(name, ARRAYSIZE(name)) > 0) {
        return LanguageFromTag(name);
    }
    return UiLanguage::English;
}

// ── Clock / date presentation (#61) ──────────────────────────────────────────

// Formats the time honouring the 12h/24h override and the seconds toggle.
// Windows' own locale formatting is used unless the user forced a mode, so the
// default keeps regional conventions (separators, AM/PM placement) intact.
std::wstring FormatIslandTime(const SYSTEMTIME& local, bool followSystem, bool use24Hour,
                              bool showSeconds) {
    wchar_t buffer[64] = {};

    if (followSystem) {
        const DWORD flags = showSeconds ? 0 : TIME_NOSECONDS;
        if (GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, flags, &local, nullptr, buffer,
                            ARRAYSIZE(buffer)) > 0) {
            return buffer;
        }
        return L"--:--";
    }

    // Explicit override: build the pattern rather than fighting locale flags.
    const wchar_t* pattern = use24Hour ? (showSeconds ? L"HH:mm:ss" : L"HH:mm")
                                       : (showSeconds ? L"h:mm:ss tt" : L"h:mm tt");
    if (GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local, pattern, buffer,
                        ARRAYSIZE(buffer)) > 0) {
        return buffer;
    }
    return L"--:--";
}

// Formats the date. A custom pattern is passed straight to Windows, which
// already supports yyyy / MM / dd / MMM / MMMM / ddd / dddd and prints anything
// else literally -- so CJK patterns such as yyyy年MM月dd日 work as typed.
std::wstring FormatIslandDate(const SYSTEMTIME& local, const std::wstring& customFormat,
                              const wchar_t* fallbackPattern) {
    wchar_t buffer[128] = {};

    if (!customFormat.empty()) {
        if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local, customFormat.c_str(), buffer,
                            ARRAYSIZE(buffer), nullptr) > 0) {
            return buffer;
        }
        // An invalid pattern falls through to the default rather than showing
        // nothing at all.
    }

    if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local, fallbackPattern, buffer,
                        ARRAYSIZE(buffer), nullptr) > 0) {
        return buffer;
    }
    return std::wstring();
}

inline const wchar_t* Loc(const wchar_t* english) {
    struct Row {
        const wchar_t* key;
        // Indexed by UiLanguage; nullptr falls back to the English key.
        const wchar_t* text[static_cast<size_t>(UiLanguage::Count)];
    };

    // Order: en, fr, es, de, pt, it, ru, tr, hi, zh, ja, ko
    static const Row kRows[] = {
        {L"Media", {nullptr, L"Média", L"Multimedia", L"Medien", L"Mídia", L"Media", L"Медиа", L"Medya", L"मीडिया", L"媒体", L"メディア", L"미디어"}},
        {L"Calendar", {nullptr, L"Calendrier", L"Calendario", L"Kalender", L"Calendário", L"Calendario", L"Календарь", L"Takvim", L"कैलेंडर", L"日历", L"カレンダー", L"캘린더"}},
        {L"Weather", {nullptr, L"Météo", L"Tiempo", L"Wetter", L"Tempo", L"Meteo", L"Погода", L"Hava", L"मौसम", L"天气", L"天気", L"날씨"}},
        {L"Hardware Monitor", {nullptr, L"Moniteur matériel", L"Monitor de hardware", L"Hardware-Monitor", L"Monitor de hardware", L"Monitor hardware", L"Монитор системы", L"Donanım İzleme", L"हार्डवेयर मॉनिटर", L"硬件监视器", L"ハードウェア モニター", L"하드웨어 모니터"}},
        {L"File Tray", {nullptr, L"Bac à fichiers", L"Bandeja de archivos", L"Dateiablage", L"Bandeja de arquivos", L"Vassoio file", L"Файлы", L"Dosya Tepsisi", L"फ़ाइल ट्रे", L"文件托盘", L"ファイル トレイ", L"파일 트레이"}},
        {L"Bluetooth", {nullptr, L"Bluetooth", L"Bluetooth", L"Bluetooth", L"Bluetooth", L"Bluetooth", L"Bluetooth", L"Bluetooth", L"ब्लूटूथ", L"蓝牙", L"Bluetooth", L"블루투스"}},
        {L"Copied", {nullptr, L"Copié", L"Copiado", L"Kopiert", L"Copiado", L"Copiato", L"Скопировано", L"Kopyalandı", L"कॉपी किया गया", L"已复制", L"コピーしました", L"복사됨"}},
        {L"Volume", {nullptr, L"Volume", L"Volumen", L"Lautstärke", L"Volume", L"Volume", L"Громкость", L"Ses", L"वॉल्यूम", L"音量", L"音量", L"볼륨"}},
        {L"Muted", {nullptr, L"Muet", L"Silenciado", L"Stumm", L"Sem som", L"Muto", L"Без звука", L"Sessiz", L"म्यूट", L"已静音", L"ミュート", L"음소거"}},
        {L"Low Battery", {nullptr, L"Batterie faible", L"Batería baja", L"Akku schwach", L"Bateria fraca", L"Batteria scarica", L"Батарея разряжена", L"Pil Az", L"बैटरी कम", L"电量低", L"バッテリー残量低下", L"배터리 부족"}},
        {L"Charging", {nullptr, L"En charge", L"Cargando", L"Wird geladen", L"Carregando", L"In carica", L"Зарядка", L"Şarj oluyor", L"चार्ज हो रहा है", L"正在充电", L"充電中", L"충전 중"}},
        {L"Connected", {nullptr, L"Connecté", L"Conectado", L"Verbunden", L"Conectado", L"Connesso", L"Подключено", L"Bağlandı", L"कनेक्ट किया गया", L"已连接", L"接続済み", L"연결됨"}},
        {L"Disconnected", {nullptr, L"Déconnecté", L"Desconectado", L"Getrennt", L"Desconectado", L"Disconnesso", L"Отключено", L"Bağlantı kesildi", L"डिस्कनेक्ट किया गया", L"已断开", L"切断されました", L"연결 끊김"}},
        {L"Caps Lock", {nullptr, L"Verr. Maj", L"Bloq Mayús", L"Feststelltaste", L"Caps Lock", L"Blocco maiuscole", L"Caps Lock", L"Caps Lock", L"कैप्स लॉक", L"大写锁定", L"Caps Lock", L"Caps Lock"}},
        {L"Num Lock", {nullptr, L"Verr. Num", L"Bloq Num", L"Num-Taste", L"Num Lock", L"Blocco num", L"Num Lock", L"Num Lock", L"नम लॉक", L"数字锁定", L"Num Lock", L"Num Lock"}},
        {L"On", {nullptr, L"Activé", L"Activado", L"Ein", L"Ligado", L"Attivo", L"Вкл", L"Açık", L"चालू", L"开", L"オン", L"켜짐"}},
        {L"Off", {nullptr, L"Désactivé", L"Desactivado", L"Aus", L"Desligado", L"Disattivo", L"Выкл", L"Kapalı", L"बंद", L"关", L"オフ", L"꺼짐"}},
        {L"Do Not Disturb", {nullptr, L"Ne pas déranger", L"No molestar", L"Nicht stören", L"Não perturbe", L"Non disturbare", L"Не беспокоить", L"Rahatsız Etme", L"परेशान न करें", L"专注助手", L"応答不可", L"방해 금지"}},
        {L"Focus", {nullptr, L"Concentration", L"Concentración", L"Fokus", L"Foco", L"Concentrazione", L"Фокус", L"Odak", L"फ़ोकस", L"专注", L"集中", L"집중"}},
        {L"Break", {nullptr, L"Pause", L"Descanso", L"Pause", L"Pausa", L"Pausa", L"Перерыв", L"Mola", L"विराम", L"休息", L"休憩", L"휴식"}},
        {L"Unknown", {nullptr, L"Inconnu", L"Desconocido", L"Unbekannt", L"Desconhecido", L"Sconosciuto", L"Неизвестно", L"Bilinmiyor", L"अज्ञात", L"未知", L"不明", L"알 수 없음"}},
        {L"Loading...", {nullptr, L"Chargement...", L"Cargando...", L"Wird geladen...", L"Carregando...", L"Caricamento...", L"Загрузка...", L"Yükleniyor...", L"लोड हो रहा है...", L"加载中...", L"読み込み中...", L"불러오는 중..."}},
        {L"Locating...", {nullptr, L"Localisation...", L"Ubicando...", L"Standort...", L"Localizando...", L"Localizzazione...", L"Определение...", L"Konum...", L"स्थान...", L"定位中...", L"位置情報...", L"위치 확인 중..."}},
        {L"Wind", {nullptr, L"Vent", L"Viento", L"Wind", L"Vento", L"Vento", L"Ветер", L"Rüzgar", L"हवा", L"风速", L"風", L"바람"}},
        {L"Feels Like", {nullptr, L"Ressenti", L"Sensación", L"Gefühlt", L"Sensação", L"Percepita", L"Ощущается", L"Hissedilen", L"महसूस", L"体感", L"体感", L"체감"}},
        {L"Humidity", {nullptr, L"Humidité", L"Humedad", L"Luftfeuchte", L"Umidade", L"Umidità", L"Влажность", L"Nem", L"नमी", L"湿度", L"湿度", L"습도"}},
        {L"Drag files here", {nullptr, L"Déposez des fichiers ici", L"Arrastra archivos aquí", L"Dateien hierher ziehen", L"Arraste arquivos aqui", L"Trascina i file qui", L"Перетащите файлы сюда", L"Dosyaları buraya sürükleyin", L"फ़ाइलें यहाँ खींचें", L"将文件拖到此处", L"ここにファイルをドラッグ", L"여기에 파일을 끌어다 놓으세요"}},
        {L"No devices", {nullptr, L"Aucun appareil", L"Sin dispositivos", L"Keine Geräte", L"Nenhum dispositivo", L"Nessun dispositivo", L"Нет устройств", L"Cihaz yok", L"कोई डिवाइस नहीं", L"无设备", L"デバイスなし", L"장치 없음"}},
        {L"item", {nullptr, L"élément", L"elemento", L"Element", L"item", L"elemento", L"элемент", L"öğe", L"आइटम", L"项", L"項目", L"항목"}},
        {L"items", {nullptr, L"éléments", L"elementos", L"Elemente", L"itens", L"elementi", L"элементов", L"öğe", L"आइटम", L"项", L"項目", L"항목"}},
    };

    const int langIndex = g_uiLanguage.load(std::memory_order_relaxed);
    if (langIndex <= static_cast<int>(UiLanguage::English) ||
        langIndex >= static_cast<int>(UiLanguage::Count)) {
        return english;
    }

    for (const Row& row : kRows) {
        if (wcscmp(row.key, english) == 0) {
            const wchar_t* translated = row.text[static_cast<size_t>(langIndex)];
            return translated ? translated : english;
        }
    }
    return english;
}

inline MediaContentPoint MediaContentFromClient(int clientX, int clientY) {
    MediaContentPoint out;

    // Seqlock read: retry until a write is not in progress and the counter has
    // not moved, so all six fields come from the same frame.
    float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f, scale = 1.0f;
    unsigned long long stamp = 0;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const unsigned before = g_mediaHitSeq.load(std::memory_order_relaxed);
        if (before & 1u) {
            continue;  // write in progress
        }
        std::atomic_thread_fence(std::memory_order_acquire);

        left = g_mediaHitLeft.load(std::memory_order_relaxed);
        top = g_mediaHitTop.load(std::memory_order_relaxed);
        right = g_mediaHitRight.load(std::memory_order_relaxed);
        bottom = g_mediaHitBottom.load(std::memory_order_relaxed);
        scale = g_mediaHitScale.load(std::memory_order_relaxed);
        stamp = g_mediaHitStamp.load(std::memory_order_relaxed);

        std::atomic_thread_fence(std::memory_order_acquire);
        if (g_mediaHitSeq.load(std::memory_order_relaxed) == before) {
            break;
        }
        stamp = 0;  // torn; force another attempt (or bail out below)
    }

    if (stamp == 0 || GetTickCount64() - stamp > 500) {
        return out;
    }

    if (scale < 0.05f) {
        scale = 1.0f;
    }

    const float width = right - left;
    const float height = bottom - top;
    if (width <= 1.0f || height <= 1.0f) {
        return out;
    }

    // DrawPill scales content about the pill centre, so undo that about the
    // same point to get back into content space.
    const float centerX = (left + right) * 0.5f;
    const float centerY = (top + bottom) * 0.5f;
    const float contentX = (static_cast<float>(clientX) - centerX) / scale + centerX;
    const float contentY = (static_cast<float>(clientY) - centerY) / scale + centerY;

    out.valid = true;
    out.x = contentX - left;
    out.y = contentY - top;
    out.width = width;
    out.height = height;
    return out;
}

inline std::wstring ToLowerCopy(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    return value;
}

inline std::wstring BaseNameFromPath(std::wstring path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.erase(0, slash + 1);
    }
    return path;
}

inline std::wstring StripExtension(std::wstring value) {
    const size_t dot = value.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        value.resize(dot);
    }
    return value;
}

inline HWND g_hwnd = nullptr;
inline std::atomic<bool> g_autoHiddenParked = false;
inline std::atomic<double> g_lastNudgeTime = 0.0;
inline std::atomic<bool> g_layoutDirty = true;
inline std::atomic<int> g_hoveredMediaButton = -1;
inline std::atomic<int> g_pressedMediaButton = -1;
inline FILETIME g_prevIdleTime = {};
inline FILETIME g_prevKernelTime = {};
inline FILETIME g_prevUserTime = {};

inline void TriggerNudge() {
    const bool wasParked = g_autoHiddenParked.exchange(false, std::memory_order_relaxed);
    const double now = NowSeconds();
    const double previous = g_lastNudgeTime.load();
    if (!wasParked && now - previous < 0.45) {
        return;
    }
    g_lastNudgeTime = now;
    HWND hwnd = g_hwnd;
    if (hwnd) {
        PostMessageW(hwnd, WM_APP_NEW_EVENT, 0, 0);
    }
}

#endif // ISLAND_COMMON_HPP
