#pragma once

#ifndef WINDOW_HOOK_MANAGER_HPP
#define WINDOW_HOOK_MANAGER_HPP

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
#include <windowsx.h>
#include <dwmapi.h>
#include <dbt.h>
#include <shellapi.h>
#include <uiautomation.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <string>
#include <string_view>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <thread>

#include "island_common.hpp"
#include "palette_color_engine.hpp"
#include "icon_process_engine.hpp"
#include "weather_location_engine.hpp"
#include "telemetry_privacy_engine.hpp"
#include "media_smtc_engine.hpp"
#include "battery_dashboard.hpp"
#include "notification_engine.hpp"
#include "agy_telemetry_engine.hpp"
#include "bluetooth_dnd_engine.hpp"
#include "ui_painters_engine.hpp"

using namespace MediaEngine;

// Global thread handles and state instances
inline HANDLE g_renderThread = nullptr;
inline DWORD g_renderThreadId = 0;

inline HANDLE g_mediaThread = nullptr;
inline HANDLE g_audioThread = nullptr;
inline HANDLE g_weatherThread = nullptr;
inline HANDLE g_notificationThread = nullptr;
inline HANDLE g_bluetoothThread = nullptr;

inline HANDLE g_stopEvent = nullptr;
inline HANDLE g_settingsChangedEvent = nullptr;

inline agy::AgyTelemetryEngine g_agyTelemetry;

inline RECT g_satelliteClientRect = {};
inline std::atomic<bool> g_satelliteActive = false;
inline std::atomic<bool> g_satelliteExpanded = false;
inline std::atomic<bool> g_satelliteClickExpanded = false;
inline RECT g_primaryClientRect = {};
inline float g_primaryCornerRadius = 0.0f;

inline std::atomic<bool> g_running = false;
inline std::atomic<bool> g_clickExpanded = false;
inline std::atomic<int> g_hoveredFileTrayRow = -1;
inline std::atomic<bool> g_scrubbing = false;
inline std::atomic<float> g_scrubDragFraction = 0.0f;
inline std::atomic<double> g_lastLiveSeekTime = 0.0;
inline std::atomic<bool> g_audioCaptureNeeded = false;
inline std::atomic<bool> g_manuallyHidden = false;
inline UINT g_registeredHotkeyModifiers = 0;
inline UINT g_registeredHotkeyVk = 0;
inline bool g_hotkeyRegistered = false;

inline std::atomic<bool> g_fullscreenOverrideVisible = false;
inline std::atomic<bool> g_isFullscreen = false;
inline std::atomic<double> g_hotkeyUnhideUntil = 0.0;
inline UINT g_shellHookMessage = 0;
inline UINT g_taskbarCreatedMessage = 0;
inline bool g_volumeInitialized = false;

// Forward declarations
inline void ApplyBackdropRegion(HWND hwnd, int windowWidth, int windowHeight);
inline void PositionOverlayWindow(HWND hwnd, int width, int height);
inline void ApplyHideShowHotkey();
inline void ApplyBackdropMaterial(HWND hwnd);
inline void NotifyKeyboardThreadSettingChanged();

inline int MediaTransportHitTest(const MediaContentPoint& pt) {
    if (!pt.valid || pt.height <= MediaLayout::kExpandedMinHeight) {
        return -1;
    }

    const float centerX = pt.width * 0.5f;
    const float dy = pt.y - MediaLayout::kControlsY;

    struct Target {
        float offsetX;
        float radius;
        int command;
    };
    const Target targets[] = {
        {-MediaLayout::kControlSpacing, MediaLayout::kNavButtonRadius, 0},
        {0.0f, MediaLayout::kPlayButtonRadius, 1},
        {MediaLayout::kControlSpacing, MediaLayout::kNavButtonRadius, 2},
    };

    for (const Target& t : targets) {
        const float reach = t.radius + MediaLayout::kControlHitPad;
        if (std::fabs(dy) <= reach && std::fabs(pt.x - (centerX + t.offsetX)) <= reach) {
            return t.command;
        }
    }
    return -1;
}

// Fraction along the scrubber bar (0..1) for a content-space point, or -1 when
// the point is not on the bar.
inline float MediaScrubFractionFromContent(const MediaContentPoint& pt) {
    if (!pt.valid || pt.height <= MediaLayout::kExpandedMinHeight) {
        return -1.0f;
    }

    const float barLeft = MediaLayout::kScrubInsetLeft;
    const float barRight = pt.width - MediaLayout::kScrubInsetRight;
    if (barRight - barLeft <= 1.0f) {
        return -1.0f;
    }

    if (std::fabs(pt.y - MediaLayout::kScrubberY) > MediaLayout::kScrubHitHalfHeight ||
        pt.x < barLeft - MediaLayout::kScrubHitPadX ||
        pt.x > barRight + MediaLayout::kScrubHitPadX) {
        return -1.0f;
    }

    return Clamp((Clamp(pt.x, barLeft, barRight) - barLeft) / (barRight - barLeft), 0.0f, 1.0f);
}

// Clamped fraction for an ongoing drag, ignoring the on-bar test so the scrub
// keeps tracking once the press has been captured.
inline float MediaScrubFractionUnbounded(const MediaContentPoint& pt) {
    if (!pt.valid) {
        return -1.0f;
    }
    const float barLeft = MediaLayout::kScrubInsetLeft;
    const float barRight = pt.width - MediaLayout::kScrubInsetRight;
    if (barRight - barLeft <= 1.0f) {
        return -1.0f;
    }
    return Clamp((Clamp(pt.x, barLeft, barRight) - barLeft) / (barRight - barLeft), 0.0f, 1.0f);
}

// Index of the File Tray row under a content-space point, or -1. Rows are drawn
// newest-first, so index 0 is the most recently dropped file.
inline int FileTrayRowAtContentPoint(const MediaContentPoint& pt, int itemCount) {
    if (!pt.valid || itemCount <= 0 || pt.height <= MediaLayout::kExpandedMinHeight) {
        return -1;
    }
    if (pt.x < FileTrayLayout::kPadX || pt.x > pt.width - FileTrayLayout::kPadX) {
        return -1;
    }

    const float listBottom = pt.height - FileTrayLayout::kListBottomInset;
    if (pt.y < FileTrayLayout::kListTop || pt.y > listBottom) {
        return -1;
    }

    const float stride = FileTrayLayout::kRowHeight + FileTrayLayout::kRowGap;
    const int index = static_cast<int>((pt.y - FileTrayLayout::kListTop) / stride);
    // Reject the gap between rows so hovering dead space highlights nothing.
    const float rowTop = FileTrayLayout::kListTop + index * stride;
    if (pt.y > rowTop + FileTrayLayout::kRowHeight) {
        return -1;
    }

    const int capacity = FileTrayLayout::VisibleRowCapacity(pt.height);
    if (index < 0 || index >= capacity || index >= itemCount) {
        return -1;
    }
    return index;
}

// True when the point is over the expanded layout's album art.
inline bool MediaArtHitTest(const MediaContentPoint& pt) {
    if (!pt.valid || pt.height <= MediaLayout::kExpandedMinHeight) {
        return false;
    }
    return pt.x >= MediaLayout::kArtInsetX &&
           pt.x <= MediaLayout::kArtInsetX + MediaLayout::kArtSize &&
           pt.y >= MediaLayout::kArtInsetY &&
           pt.y <= MediaLayout::kArtInsetY + MediaLayout::kArtSize;
}


inline bool EqualsNoCase(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (size_t i = 0; i < a.size(); ++i) {
        if (towlower(a[i]) != towlower(b[i])) {
            return false;
        }
    }

    return true;
}

inline std::wstring GetStringSettingCopy(PCWSTR name) {
    PCWSTR value = Wh_GetStringSetting(name);
    std::wstring result = value ? value : L"";
    Wh_FreeStringSetting(value);
    return result;
}

inline std::wstring GetStringSettingWithFallback(PCWSTR primary, PCWSTR fallback, PCWSTR fallback2 = nullptr) {
    std::wstring result = GetStringSettingCopy(primary);
    if (!result.empty()) {
        return result;
    }
    result = GetStringSettingCopy(fallback);
    if (!result.empty()) {
        return result;
    }
    if (fallback2) {
        return GetStringSettingCopy(fallback2);
    }
    return L"";
}




inline bool IsForegroundFullscreen(HWND targetHwnd) {
    HWND fg = GetForegroundWindow();
    if (!fg || fg == targetHwnd || fg == GetDesktopWindow() || fg == GetShellWindow()) {
        return false;
    }

    if (!IsWindowVisible(fg)) return false;

    wchar_t className[256] = {};
    GetClassNameW(fg, className, 256);
    if (wcscmp(className, L"WorkerW") == 0 || wcscmp(className, L"Progman") == 0 ||
        wcscmp(className, L"Shell_TrayWnd") == 0) {
        return false;
    }

    RECT clientRect = {};
    if (!GetClientRect(fg, &clientRect)) return false;
    POINT pt = {0, 0};
    ClientToScreen(fg, &pt);
    clientRect.left += pt.x;
    clientRect.right += pt.x;
    clientRect.top += pt.y;
    clientRect.bottom += pt.y;

    HMONITOR hMon = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfoW(hMon, &mi)) {
        if (clientRect.left <= mi.rcMonitor.left &&
            clientRect.top <= mi.rcMonitor.top &&
            clientRect.right >= mi.rcMonitor.right &&
            clientRect.bottom >= mi.rcMonitor.bottom) {
            return true;
        }
    }

    return false;
}

// Returns true if the currently active foreground window is maximized on the island's monitor
inline bool IsForegroundMaximized(HWND targetHwnd) {
    HWND fg = GetForegroundWindow();
    if (!fg || fg == targetHwnd || fg == GetDesktopWindow() || fg == GetShellWindow()) {
        return false;
    }
    if (!IsWindowVisible(fg) || IsIconic(fg)) return false;

    wchar_t className[256] = {};
    GetClassNameW(fg, className, 256);
    if (wcscmp(className, L"WorkerW") == 0 || wcscmp(className, L"Progman") == 0 ||
        wcscmp(className, L"Shell_TrayWnd") == 0 || wcscmp(className, L"Shell_SecondaryTrayWnd") == 0) {
        return false;
    }

    if (IsZoomed(fg) || (GetWindowLongPtrW(fg, GWL_STYLE) & WS_MAXIMIZE) != 0) {
        HMONITOR hMonFg = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
        HMONITOR hMonTarget = targetHwnd ? MonitorFromWindow(targetHwnd, MONITOR_DEFAULTTONEAREST) : nullptr;
        if (!hMonTarget || hMonFg == hMonTarget) {
            return true;
        }
    }
    return false;
}

// Returns the DPI scale factor for the primary monitor (1.0 = 96 DPI = 100%)
inline float GetPrimaryMonitorDpiScale() {
    POINT pt = {0, 0};
    HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    UINT dpiX = 96, dpiY = 96;
    using GetDpiForMonitor_t = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static auto pGetDpiForMonitor = reinterpret_cast<GetDpiForMonitor_t>(
        GetProcAddress(GetModuleHandleW(L"shcore.dll"), "GetDpiForMonitor"));
    if (pGetDpiForMonitor) {
        pGetDpiForMonitor(monitor, 0 /* MDT_EFFECTIVE_DPI */, &dpiX, &dpiY);
    }
    return static_cast<float>(dpiX) / 96.0f;
}

inline int GetMonitorRefreshRate(HWND hwnd) {
    HMONITOR monitor = nullptr;
    if (hwnd) {
        monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    } else {
        POINT pt = {0, 0};
        monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }
    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(monitor, &mi)) {
        DEVMODEW dm = {};
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
            if (dm.dmDisplayFrequency > 1) {
                return static_cast<int>(dm.dmDisplayFrequency);
            }
        }
    }
    HDC hdc = GetDC(nullptr);
    int rate = GetDeviceCaps(hdc, VREFRESH);
    ReleaseDC(nullptr, hdc);
    return (rate > 1) ? rate : 60;
}


inline void LoadSettings() {
    Settings next;

    const std::wstring position = GetStringSettingCopy(L"Appearance.Position");
    if (EqualsNoCase(position, L"top-left")) {
        next.position = Position::TopLeft;
    } else if (EqualsNoCase(position, L"top-right")) {
        next.position = Position::TopRight;
    } else if (EqualsNoCase(position, L"bottom-center")) {
        next.position = Position::BottomCenter;
    } else if (EqualsNoCase(position, L"bottom-left")) {
        next.position = Position::BottomLeft;
    } else if (EqualsNoCase(position, L"bottom-right")) {
        next.position = Position::BottomRight;
    }

    const std::wstring scale = GetStringSettingCopy(L"Appearance.SizeScale");
    if (!scale.empty()) {
        wchar_t* end;
        float parsedScale = wcstof(scale.c_str(), &end);
        if (end != scale.c_str() && parsedScale > 0.1f && parsedScale < 10.0f) {
            next.sizeScale = parsedScale;
        }
    }

    // Auto DPI scaling: multiply sizeScale by monitor DPI factor.
    // On a 4K 200% display this doubles the island to the right physical size.
    if (Wh_GetIntSetting(L"Appearance.AutoDpiScale") != 0) {
        next.sizeScale *= GetPrimaryMonitorDpiScale();
    }

    std::wstring fontSetting = GetStringSettingWithFallback(L"Themes.FontFamily", L"Appearance.FontFamily");
    size_t firstChar = fontSetting.find_first_not_of(L" \t\r\n\"'");
    if (firstChar != std::wstring::npos) {
        size_t lastChar = fontSetting.find_last_not_of(L" \t\r\n\"'");
        next.fontFamily = fontSetting.substr(firstChar, lastChar - firstChar + 1);
    } else {
        next.fontFamily.clear();
    }

    const std::wstring accentMode = GetStringSettingCopy(L"Themes.AccentColorMode");
    if (EqualsNoCase(accentMode, L"system")) {
        next.accentMode = AccentMode::System;
    } else if (EqualsNoCase(accentMode, L"custom")) {
        next.accentMode = AccentMode::Custom;
    }

    next.customAccent = ColorFromHex(GetStringSettingCopy(L"Themes.CustomAccentHex"), next.customAccent);

    const std::wstring calAccent = GetStringSettingWithFallback(L"Themes.CalendarAccent", L"CalendarWeather.CalendarAccent", L"Modules.CalendarAccent");
    if (EqualsNoCase(calAccent, L"system")) {
        next.calendarAccent = CalendarAccentMode::System;
    } else {
        next.calendarAccent = CalendarAccentMode::Red;
    }

    const std::wstring fpsStr = GetStringSettingWithFallback(L"Animations.TargetFPS", L"Appearance.TargetFPS");
    if (EqualsNoCase(fpsStr, L"auto") || fpsStr.empty()) {
        next.targetFps = 0;
    } else {
        next.targetFps = _wtoi(fpsStr.c_str());
        if (next.targetFps < 0) next.targetFps = 0;
    }

    const std::wstring styleStr = GetStringSettingWithFallback(L"Animations.AnimationStyle", L"Appearance.AnimationStyle");
    if (EqualsNoCase(styleStr, L"smooth")) {
        next.animationStyle = AnimationStyle::Smooth;
    } else if (EqualsNoCase(styleStr, L"bouncy")) {
        next.animationStyle = AnimationStyle::Bouncy;
    } else if (EqualsNoCase(styleStr, L"snappy")) {
        next.animationStyle = AnimationStyle::Snappy;
    } else {
        next.animationStyle = AnimationStyle::Default;
    }

    const std::wstring speed = GetStringSettingWithFallback(L"Animations.AnimationSpeed", L"Appearance.AnimationSpeed", L"Behavior.AnimationSpeed");
    if (EqualsNoCase(speed, L"very-slow")) {
        next.animationSpeed = 0.5f;
    } else if (EqualsNoCase(speed, L"slow")) {
        next.animationSpeed = 0.75f;
    } else if (EqualsNoCase(speed, L"fast")) {
        next.animationSpeed = 1.35f;
    } else if (EqualsNoCase(speed, L"very-fast")) {
        next.animationSpeed = 1.65f;
    } else if (EqualsNoCase(speed, L"ultra-fast")) {
        next.animationSpeed = 2.0f;
    } else {
        next.animationSpeed = 1.0f;
    }

    next.media = Wh_GetIntSetting(L"Modules.Media") != 0;
    next.mediaAutoExpand = Wh_GetIntSetting(L"Modules.MediaAutoExpand") != 0;
    next.volume = Wh_GetIntSetting(L"Modules.Volume") != 0;
    if (!next.volume) {
        std::lock_guard lock(g_stateMutex);
        g_state.volume.active = false;
    }
    next.clipboard = Wh_GetIntSetting(L"Modules.Clipboard") != 0;
    next.statusCountdownProgress = Wh_GetIntSetting(L"Modules.StatusCountdownProgress") != 0;

    next.battery = Wh_GetIntSetting(L"Modules.Battery") != 0;
    next.batteryModule = next.battery;
    next.progress = Wh_GetIntSetting(L"Modules.Progress") != 0;
    next.capsLock = Wh_GetIntSetting(L"Modules.CapsLock") != 0;
    next.timerEnabled = Wh_GetIntSetting(L"Modules.TimerModule") != 0;

    next.hideShowHotkeyEnabled = Wh_GetIntSetting(L"Shortcuts.HideShowHotkeyEnabled") != 0;

    const std::wstring hotkeyModStr = GetStringSettingCopy(L"Shortcuts.HideShowModifiers");
    if (EqualsNoCase(hotkeyModStr, L"ctrl_shift")) {
        next.hideShowModifiers = MOD_CONTROL | MOD_SHIFT;
    } else if (EqualsNoCase(hotkeyModStr, L"alt_shift")) {
        next.hideShowModifiers = MOD_ALT | MOD_SHIFT;
    } else if (EqualsNoCase(hotkeyModStr, L"win_alt")) {
        next.hideShowModifiers = MOD_WIN | MOD_ALT;
    } else if (EqualsNoCase(hotkeyModStr, L"ctrl_alt_shift")) {
        next.hideShowModifiers = MOD_CONTROL | MOD_ALT | MOD_SHIFT;
    } else {
        next.hideShowModifiers = MOD_CONTROL | MOD_ALT;
    }
    next.hideShowModifiers |= MOD_NOREPEAT;

    // Single A-Z/0-9 key only; anything else (blank, multi-char, symbol) falls back to 'D'.
    const std::wstring hotkeyKeyStr = GetStringSettingCopy(L"Shortcuts.HideShowKey");
    next.hideShowVk = 'D';
    if (hotkeyKeyStr.size() == 1) {
        const wchar_t ch = towupper(hotkeyKeyStr[0]);
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
            next.hideShowVk = static_cast<UINT>(ch);
        }
    }

    next.bluetoothIndicator = Wh_GetIntSetting(L"Modules.BluetoothIndicator") != 0;
    next.bluetoothShowBattery = Wh_GetIntSetting(L"Modules.BluetoothShowBattery") != 0;
    next.doNotDisturbIndicator = Wh_GetIntSetting(L"Modules.DoNotDisturbIndicator") != 0;
    next.notificationRespectDnD = Wh_GetIntSetting(L"Modules.NotificationRespectDnD") != 0;

    // ── Premium material / redesign ──────────────────────────────────────────
    {
        const std::wstring backdrop = GetStringSettingCopy(L"Themes.BackdropMaterial");
        if (EqualsNoCase(backdrop, L"acrylic")) {
            next.backdropMaterial = BackdropMaterial::Acrylic;
        } else if (EqualsNoCase(backdrop, L"blur")) {
            next.backdropMaterial = BackdropMaterial::Blur;
        } else {
            next.backdropMaterial = BackdropMaterial::None;
        }
    }
    next.backdropTint = Clamp(Wh_GetIntSetting(L"Themes.BackdropTint") / 100.0f, 0.0f, 1.0f);
    next.backdropFillAlpha = Clamp(Wh_GetIntSetting(L"Themes.BackdropFillOpacity") / 100.0f, 0.0f, 1.0f);

    next.materialDepth = Wh_GetIntSetting(L"Themes.MaterialDepth") != 0;
    next.dropShadow = Wh_GetIntSetting(L"Themes.DropShadow") != 0;
    next.accentBloom = Clamp(Wh_GetIntSetting(L"Themes.AccentBloom") / 100.0f, 0.0f, 2.0f);
    next.textScale = Clamp(Wh_GetIntSetting(L"Themes.TextScale") / 100.0f, 0.7f, 1.6f);

    // ── Clock / date presentation (#61) ──────────────────────────────────────
    next.showSeconds = Wh_GetIntSetting(L"Modules.ShowSeconds") != 0;
    const std::wstring clockMode = GetStringSettingCopy(L"Modules.ClockFormat");
    next.clockFollowSystem = clockMode.empty() || EqualsNoCase(clockMode, L"system");
    next.use24HourClock = EqualsNoCase(clockMode, L"24h");
    next.dateFormat = GetStringSettingCopy(L"Modules.DateFormat");
    next.dateFirst = Wh_GetIntSetting(L"Modules.DateFirst") != 0;

    // ── Localization (#35) ───────────────────────────────────────────────────
    {
        std::wstring langTag = GetStringSettingCopy(L"Modules.Language");
        if (langTag.empty()) {
            langTag = L"auto";
        }
        next.language = langTag;
        const UiLanguage resolved = EqualsNoCase(langTag, L"auto") ? DetectSystemLanguage()
                                                                  : LanguageFromTag(langTag);
        g_uiLanguage.store(static_cast<int>(resolved), std::memory_order_relaxed);
    }

    // ── File tray (#33) ──────────────────────────────────────────────────────
    next.fileTrayModule = Wh_GetIntSetting(L"Modules.FileTrayModule") != 0;
    next.fileTrayMaxItems = ClampInt(Wh_GetIntSetting(L"Modules.FileTrayMaxItems"), 1, 25);

    // ── Media auto-expand exclusions (#62) ───────────────────────────────────
    next.mediaExpandBlocklist.clear();
    {
        const std::wstring raw = GetStringSettingCopy(L"Modules.MediaExpandBlocklist");
        size_t start = 0;
        while (start <= raw.size()) {
            const size_t comma = raw.find(L',', start);
            const size_t end = (comma == std::wstring::npos) ? raw.size() : comma;
            std::wstring token = raw.substr(start, end - start);
            const size_t a = token.find_first_not_of(L" \t\r\n");
            if (a != std::wstring::npos) {
                const size_t b = token.find_last_not_of(L" \t\r\n");
                std::wstring entry = token.substr(a, b - a + 1);
                // Stored lowercase so matching against media source/title is
                // case-insensitive without re-lowering on every frame.
                std::transform(entry.begin(), entry.end(), entry.begin(),
                               [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
                next.mediaExpandBlocklist.push_back(std::move(entry));
            }
            if (comma == std::wstring::npos) {
                break;
            }
            start = comma + 1;
        }
    }

    // ── Game overlay options (#25) ───────────────────────────────────────────
    next.gameOverlayShowFps = Wh_GetIntSetting(L"Modules.GameOverlayShowFps") != 0;
    next.gameOverlayShowCpu = Wh_GetIntSetting(L"Modules.GameOverlayShowCpu") != 0;
    next.gameOverlayShowGpu = Wh_GetIntSetting(L"Modules.GameOverlayShowGpu") != 0;
    next.gameOverlayShowRam = Wh_GetIntSetting(L"Modules.GameOverlayShowRam") != 0;
    next.gameOverlayShowDisk = Wh_GetIntSetting(L"Modules.GameOverlayShowDisk") != 0;
    next.gameOverlayCompact = Wh_GetIntSetting(L"Modules.GameOverlayCompact") != 0;
    next.tintOpacity = Clamp(Wh_GetIntSetting(L"Themes.TintIntensity") / 100.0f, 0.0f, 1.0f);
    const int settingOpacity = Wh_GetIntSetting(L"Themes.PillOpacity");
    const int localOpacity = Wh_GetIntValue(L"PillOpacityOverride", -1);
    next.pillOpacity = Clamp((localOpacity >= 0 ? localOpacity : settingOpacity) / 100.0f,
                             0.35f, 1.0f);
    next.gameOverlay = Wh_GetIntSetting(L"Modules.GameOverlay") != 0;
    next.showMetricText = Wh_GetIntSetting(L"Modules.ShowMetricText") != 0;
    next.weather = Wh_GetIntSetting(L"Modules.Weather") != 0;
    std::wstring customLoc = GetStringSettingWithFallback(L"Modules.CustomLocation", L"CustomLocation", L"Weather.CustomLocation");
    if (!customLoc.empty()) {
        next.weatherCity = customLoc;
    } else {
        next.weatherCity = GetStringSettingWithFallback(L"Modules.WeatherCity", L"Weather.WeatherCity", L"CalendarWeather.WeatherCity");
    }
    // Trim leading and trailing whitespace
    size_t locFirst = next.weatherCity.find_first_not_of(L" \t\r\n");
    if (locFirst == std::wstring::npos) {
        next.weatherCity.clear();
    } else {
        size_t locLast = next.weatherCity.find_last_not_of(L" \t\r\n");
        next.weatherCity = next.weatherCity.substr(locFirst, locLast - locFirst + 1);
    }
    next.customLocation = next.weatherCity;
    next.weatherFahrenheit = Wh_GetIntSetting(L"Modules.WeatherFahrenheit") != 0;
    const std::wstring hideSec = GetStringSettingWithFallback(L"Behavior.AutoHideIdleSeconds", L"Appearance.AutoHideIdleSeconds");
    next.autoHideIdleSeconds = hideSec.empty() ? 0 : _wtoi(hideSec.c_str());
    next.unhideOnHover = Wh_GetIntSetting(L"Behavior.UnhideOnHover") != 0;
    next.alwaysOnTop = Wh_GetIntSetting(L"Behavior.AlwaysOnTop") != 0;
    const int localExpandOnHover = Wh_GetIntValue(L"ExpandOnHoverOverride", -1);
    next.expandOnHover = localExpandOnHover >= 0 ? (localExpandOnHover != 0) : (Wh_GetIntSetting(L"Behavior.ExpandOnHover") != 0);
    next.autoDpiScale = Wh_GetIntSetting(L"Appearance.AutoDpiScale") != 0;
    next.offsetX = Wh_GetIntSetting(L"Appearance.OffsetX");
    next.offsetY = Wh_GetIntSetting(L"Appearance.OffsetY");
    next.separateExpandedOffsetY = Wh_GetIntSetting(L"Appearance.SeparateExpandedOffsetY") != 0;
    next.offsetYExpanded = Wh_GetIntSetting(L"Appearance.OffsetYExpanded");

    std::wstring mon = GetStringSettingCopy(L"Appearance.TargetMonitor");
    if (mon == L"primary") next.targetMonitor = 0;
    else if (mon == L"follow") next.targetMonitor = -1;
    else next.targetMonitor = _wtoi(mon.c_str());

    std::wstring shapeStr = GetStringSettingCopy(L"Appearance.ShapeStyle");
    static std::wstring s_lastConfiguredShape = L"";
    if (!shapeStr.empty() && shapeStr != s_lastConfiguredShape) {
        s_lastConfiguredShape = shapeStr;
        Wh_SetIntValue(L"W11StyleOverride", -1);
        Wh_SetIntValue(L"NotchStyleOverride", -1);
    }
    bool baseW11 = EqualsNoCase(shapeStr, L"w11");
    bool baseNotch = EqualsNoCase(shapeStr, L"notch");

    const int localW11Style = Wh_GetIntValue(L"W11StyleOverride", -1);
    next.w11Style = localW11Style >= 0 ? (localW11Style != 0) : baseW11;

    const int localNotchStyle = Wh_GetIntValue(L"NotchStyleOverride", -1);
    next.notchStyle = localNotchStyle >= 0 ? (localNotchStyle != 0) : baseNotch;

    // A bottom-anchored island cannot be a top-edge macOS notch.
    if (IsBottomPosition(next.position)) {
        next.notchStyle = false;
    }

    // Color settings — check local theme override first, then settings YAML.
    // Palettes live in kThemePalettes at file scope so the right-click menu
    // resolves the same colors and labels this does.
    //
    // A custom hex field counts as an intentional override once it differs from
    // the value shipped as its default. Presets keep working untouched for
    // anyone who never edits these fields, but an edited hex now takes effect
    // immediately instead of being silently discarded unless the Theme preset
    // also happened to be switched to Custom. Clearing the field back to its
    // default hands control back to the preset.
    //
    // `defaultHexes` deliberately lists the *historical* defaults as well as the
    // current one. Retiring the old palettes moved these defaults, and Windhawk
    // keeps whatever value is already stored in a user's config -- so someone who
    // never touched the hex fields would still be carrying "#0D0D0F" from the old
    // OLED Black default. Matching only the current default would read that as a
    // deliberate override and pin every new palette back to the old background.
    auto resolveColor = [](const wchar_t* settingKey,
                           std::initializer_list<const wchar_t*> defaultHexes,
                           D2D1_COLOR_F presetColor) -> D2D1_COLOR_F {
        const std::wstring value = GetStringSettingCopy(settingKey);
        if (value.empty()) {
            return presetColor;
        }

        // Compare the parsed colors rather than the strings, so equivalent
        // spellings of the default ("0D0D0F" without the '#', "#0D0D0FFF",
        // different case) are all still recognised as "untouched" and leave the
        // preset in charge.
        const D2D1_COLOR_F sentinel = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
        const D2D1_COLOR_F parsed = ColorFromHex(value, sentinel);
        if (parsed.r == sentinel.r && parsed.g == sentinel.g &&
            parsed.b == sentinel.b && parsed.a == sentinel.a) {
            // Unparseable, so treat it as not set.
            return presetColor;
        }

        for (const wchar_t* defaultHex : defaultHexes) {
            const D2D1_COLOR_F defaults = ColorFromHex(defaultHex, sentinel);
            if (parsed.r == defaults.r && parsed.g == defaults.g &&
                parsed.b == defaults.b && parsed.a == defaults.a) {
                return presetColor;
            }
        }
        return parsed;
    };

    auto applyPreset = [&](Settings& target, int idx) {
        const ThemePalette& p = kThemePalettes[idx];
        target.pillBgColor = resolveColor(
            L"Themes.PillBgColor", {kThemePalettes[0].bg, L"#0D0D0F"},
            ColorFromHex(p.bg, D2D1::ColorF(0.031f, 0.031f, 0.039f, 1.0f)));
        target.textPrimaryColor = resolveColor(
            L"Themes.TextPrimaryColor", {L"#FFFFFF", L"#F7F7F7"},
            ColorFromHex(p.fg, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f)));
        target.textSecondaryColor = resolveColor(
            L"Themes.TextSecondaryColor", {kThemePalettes[0].sec, L"#B0B0B8", L"#888888"},
            ColorFromHex(p.sec, D2D1::ColorF(0.608f, 0.608f, 0.647f, 1.0f)));
        target.contourBorderColor = resolveColor(
            L"Themes.ContourBorderHex", {kThemePalettes[0].border, L"#333338"},
            ColorFromHex(p.border, D2D1::ColorF(0.118f, 0.118f, 0.133f, 1.0f)));
    };

    std::wstring themePresetStr = GetStringSettingCopy(L"Themes.ThemePreset");
    // The active palette is persisted as an integer so the right-click menu can
    // change it without rewriting the settings file. Migrate the old key across
    // once, since the integers were renumbered when the palettes changed.
    if (Wh_GetIntValue(kThemeValueName, -1) < 0) {
        const int legacy = Wh_GetIntValue(kLegacyThemeValueName, -1);
        if (legacy >= 0) {
            int migrated;
            switch (legacy) {
                case 0:  migrated = 0; break;                  // OLED Black    -> Obsidian
                case 1:  migrated = 1; break;                  // Fluent        -> Graphite
                case 2:  migrated = 2; break;                  // Midnight Blue -> Slate
                case 3:  migrated = 6; break;                  // Deep Purple   -> Plum
                case 4:  migrated = 1; break;                  // Fluent Design -> Graphite
                default: migrated = kCustomThemeIndex; break;   // out of range meant Custom
            }
            Wh_SetIntValue(kThemeValueName, migrated);
        }
    }

    static std::wstring s_lastConfiguredPreset = L"";
    if (!themePresetStr.empty() && themePresetStr != s_lastConfiguredPreset) {
        s_lastConfiguredPreset = themePresetStr;
        const int fromSettings = ThemeIndexFromId(themePresetStr);
        if (fromSettings >= 0) {
            Wh_SetIntValue(kThemeValueName, fromSettings);
        }
    }

    const int theme = Wh_GetIntValue(kThemeValueName, -1);
    if (theme >= 0 && theme < kCustomThemeIndex) {
        next.themePreset = static_cast<ThemePreset>(theme);
        applyPreset(next, theme);
    } else if (theme >= kCustomThemeIndex || EqualsNoCase(themePresetStr, L"custom")) {
        // Explicit Custom: the hex fields are authoritative, defaults included.
        next.themePreset = ThemePreset::Custom;
        next.pillBgColor = ColorFromHex(GetStringSettingCopy(L"Themes.PillBgColor"),
                                        D2D1::ColorF(0.031f, 0.031f, 0.039f, 1.0f));
        next.textPrimaryColor = ColorFromHex(GetStringSettingCopy(L"Themes.TextPrimaryColor"),
                                             D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
        next.textSecondaryColor = ColorFromHex(GetStringSettingCopy(L"Themes.TextSecondaryColor"),
                                               D2D1::ColorF(0.608f, 0.608f, 0.647f, 1.0f));
        next.contourBorderColor = ColorFromHex(GetStringSettingCopy(L"Themes.ContourBorderHex"),
                                               D2D1::ColorF(0.118f, 0.118f, 0.133f, 1.0f));
    } else {
        const int fromSettings = ThemeIndexFromId(themePresetStr);
        const int presetIdx = (fromSettings >= 0 && fromSettings < kCustomThemeIndex) ? fromSettings : 0;
        next.themePreset = static_cast<ThemePreset>(presetIdx);
        applyPreset(next, presetIdx);
    }

    // Graphite inherits the old Fluent theme's auto-translucency: it is the
    // neutral Windows 11 grey, and it reads best when the desktop shows through
    // a little. The rest of the palettes are opaque unless the user says so.
    if (next.themePreset == ThemePreset::AppleDark) {
        if (localOpacity < 0) {
            next.pillOpacity = 1.0f;
        }
        next.materialDepth = false;
        next.dropShadow = false;
        next.accentBloom = 0.0f;
    } else if ((next.themePreset == ThemePreset::Graphite || next.themePreset == ThemePreset::LiquidGlass) && localOpacity < 0) {
        next.pillOpacity = 0.88f;
    }

    next.borderMergedMode = Wh_GetIntSetting(L"Appearance.BorderMergedMode") != 0;
    next.autoHideFullscreen = Wh_GetIntSetting(L"Behavior.AutoHideFullscreen") != 0;
    next.autoHideMaximized = Wh_GetIntSetting(L"Behavior.AutoHideMaximized") != 0;
    next.hardwareMonitorModule = Wh_GetIntSetting(L"Modules.HardwareMonitorModule") != 0;
    const std::wstring borderModeStr = GetStringSettingCopy(L"Themes.ContourBorderMode");
    if (EqualsNoCase(borderModeStr, L"borderless")) {
        next.contourBorderMode = ContourBorderMode::Borderless;
        next.contourBorderEnabled = false;
    } else if (EqualsNoCase(borderModeStr, L"auto")) {
        next.contourBorderMode = ContourBorderMode::Auto;
        next.contourBorderEnabled = true;
    } else if (EqualsNoCase(borderModeStr, L"default")) {
        next.contourBorderMode = ContourBorderMode::Default;
        next.contourBorderEnabled = true;
    } else {
        next.contourBorderEnabled = true;  // no such setting; the mode dropdown drives this
        next.contourBorderMode = next.contourBorderEnabled ? ContourBorderMode::Default : ContourBorderMode::Borderless;
    }
    next.clockAccentGlow = Wh_GetIntSetting(L"Themes.ClockAccentGlow") != 0;
    bool settingsChangedWhileHidden = (g_autoHiddenParked.load() || g_manuallyHidden.load());
    bool unhideRequested = (next.autoHideIdleSeconds == 0) ||
                           (next.unhideOnHover && !g_settings.unhideOnHover);
    if (unhideRequested || (settingsChangedWhileHidden && next.autoHideIdleSeconds == 0)) {
        g_autoHiddenParked = false;
        g_manuallyHidden = false;
        Wh_SetIntValue(L"ManuallyHidden", 0);
        g_hotkeyUnhideUntil.store(NowSeconds() + (next.autoHideIdleSeconds > 0 ? next.autoHideIdleSeconds : 6.0));
        if (g_hwnd) {
            ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
            PostMessageW(g_hwnd, WM_APP_NEW_EVENT, 0, 0);
        }
    } else if (settingsChangedWhileHidden && next.unhideOnHover) {
        g_autoHiddenParked = false;
        g_manuallyHidden = false;
        Wh_SetIntValue(L"ManuallyHidden", 0);
        g_hotkeyUnhideUntil.store(NowSeconds() + (next.autoHideIdleSeconds > 0 ? next.autoHideIdleSeconds : 6.0));
        if (g_hwnd) {
            ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
            PostMessageW(g_hwnd, WM_APP_NEW_EVENT, 0, 0);
        }
    }

    // Privacy Indicators: unified reading with complete fallback to legacy keys
    next.privacyDots = Wh_GetIntSetting(L"Indicators.PrivacyDots") != 0;
    next.privacyDotsMic = Wh_GetIntSetting(L"Indicators.PrivacyDotsMic") != 0;
    next.privacyDotsCam = Wh_GetIntSetting(L"Indicators.PrivacyDotsCam") != 0;
    next.privacyDotsPulse = true;  // no such setting; pulsing is always on

    std::wstring micHexStr = GetStringSettingWithFallback(L"Indicators.PrivacyDotsMicHex", L"Indicators.MicDotHex", L"Modules.PrivacyDotsMicHex");
    next.privacyDotsMicHex = ColorFromHex(micHexStr, D2D1::ColorF(1.0f, 0.584f, 0.0f, 1.0f));

    std::wstring camHexStr = GetStringSettingWithFallback(L"Indicators.PrivacyDotsCamHex", L"Indicators.CamDotHex", L"Modules.PrivacyDotsCamHex");
    next.privacyDotsCamHex = ColorFromHex(camHexStr, D2D1::ColorF(0.204f, 0.780f, 0.349f, 1.0f));

    // Compatibility aliases: always keep in sync
    next.privacyDotsEnabled = next.privacyDots;
    next.privacyDotPulsing = next.privacyDotsPulse;
    next.micDotColor = next.privacyDotsMicHex;
    next.camDotColor = next.privacyDotsCamHex;

    Wh_SetIntValue(L"PinnedExpanded", 0);

    // Compare-then-publish under one lock so the change flags describe exactly
    // the transition we are about to commit. Two LoadSettings() calls can run
    // concurrently (settings thread vs. tray menu on the render thread); without
    // this, both could read the same "old" values and each decide a hotkey
    // re-registration was needed, or neither would.
    bool cityChanged = false;
    bool hotkeySettingChanged = false;
    bool backdropChanged = false;
    bool capsLockChanged = false;
    {
        std::lock_guard lock(g_settingsMutex);
        cityChanged = next.weatherCity != g_settings.weatherCity;
        capsLockChanged = next.capsLock != g_settings.capsLock;
        hotkeySettingChanged =
            next.hideShowHotkeyEnabled != g_settings.hideShowHotkeyEnabled ||
            next.hideShowModifiers != g_settings.hideShowModifiers ||
            next.hideShowVk != g_settings.hideShowVk;
        backdropChanged =
            next.backdropMaterial != g_settings.backdropMaterial ||
            std::fabs(next.backdropTint - g_settings.backdropTint) > 0.001f;
        g_settings = std::move(next);
    }
    g_layoutDirty = true;
    if (cityChanged) {
        {
            std::lock_guard lock(g_stateMutex);
            if (!g_settings.weatherCity.empty()) {
                g_state.weather.city = g_settings.weatherCity;
            }
        }
        if (g_settingsChangedEvent) {
            SetEvent(g_settingsChangedEvent);
        }
    }
    // Installs or removes WH_KEYBOARD_LL to match. The keyboard thread would pick
    // this up on its next backstop tick anyway; this just makes it immediate.
    if (capsLockChanged) {
        NotifyKeyboardThreadSettingChanged();
    }
    // g_hwnd only exists once RenderThreadProc has created the overlay window;
    // the very first LoadSettings() call (at mod init, before StartThreads())
    // runs with g_hwnd still null, so ApplyHideShowHotkey() no-ops there and
    // RenderThreadProc does the initial registration itself right after
    // CreateWindowExW. Every later call (e.g. from WhTool_ModSettingsChanged)
    // re-registers live so hotkey edits apply without a mod restart.
    // Posted, not called: UnregisterHotKey / RegisterHotKey fail with
    // ERROR_WINDOW_OF_OTHER_THREAD from this thread, which left the old
    // combination registered, the new one never registered, and the bookkeeping
    // flag claiming otherwise -- so switching the hotkey off never released it.
    if (hotkeySettingChanged) {
        if (g_hwnd) {
            PostMessageW(g_hwnd, WM_APP_APPLY_HOTKEY, 0, 0);
        }
    }
    // Same story for the backdrop: no-ops before the window exists, and applies
    // live afterwards so switching blur/acrylic needs no mod restart.
    if (backdropChanged && g_hwnd) {
        PostMessageW(g_hwnd, WM_APP_APPLY_BACKDROP, 0, 0);
    }
}

inline void EnableBlurBehind(HWND hwnd) {
    DWM_BLURBEHIND blur = {};
    blur.dwFlags = DWM_BB_ENABLE;
    blur.fEnable = FALSE;
    DwmEnableBlurBehindWindow(hwnd, &blur);
}

// ── Backdrop material (#59) ──────────────────────────────────────────────────
// Real Windows blur/acrylic behind the island.
//
// This uses SetWindowCompositionAttribute, which is the same undocumented entry
// point Windows' own shell surfaces use for acrylic. It is resolved dynamically
// so the mod still loads cleanly on builds where it is absent, and every failure
// path simply leaves the island fully opaque rather than breaking rendering.
//
// It composes with UpdateLayeredWindow: DWM blurs whatever is behind the window,
// and the per-pixel alpha we paint decides how much of that blur shows through.
// That is why enabling a backdrop also caps the pill's own fill alpha further
// down -- an opaque fill would hide the very effect being switched on.

namespace Backdrop {

enum CompositionAttribute : int {
    WCA_ACCENT_POLICY = 19,
};

enum AccentState : int {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
};

struct AccentPolicy {
    AccentState state;
    DWORD flags;
    DWORD gradientColor;  // ABGR
    DWORD animationId;
};

struct CompositionAttributeData {
    CompositionAttribute attribute;
    void* data;
    SIZE_T dataSize;
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, CompositionAttributeData*);

inline SetWindowCompositionAttributeFn Resolve() {
    static SetWindowCompositionAttributeFn cached = [] {
        // user32 is already loaded in-process; GetModuleHandle avoids taking a
        // reference we would then have to release.
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (!user32) {
            return static_cast<SetWindowCompositionAttributeFn>(nullptr);
        }
        return reinterpret_cast<SetWindowCompositionAttributeFn>(
            GetProcAddress(user32, "SetWindowCompositionAttribute"));
    }();
    return cached;
}

}  // namespace Backdrop

inline void ApplyBackdropMaterial(HWND hwnd) {
    if (!hwnd) {
        return;
    }

    const auto setAttribute = Backdrop::Resolve();
    if (!setAttribute) {
        return;  // Unsupported build; island stays opaque.
    }

    Backdrop::AccentPolicy policy = {};
    switch (g_settings.backdropMaterial) {
        case BackdropMaterial::Blur:
            policy.state = Backdrop::ACCENT_ENABLE_BLURBEHIND;
            break;
        case BackdropMaterial::Acrylic:
            policy.state = Backdrop::ACCENT_ENABLE_ACRYLICBLURBEHIND;
            break;
        case BackdropMaterial::None:
        default:
            policy.state = Backdrop::ACCENT_DISABLED;
            break;
    }

    // Acrylic needs a tint supplied in the policy itself; the shell mixes this
    // over the blurred backdrop. Stored ABGR, and the alpha here is the tint
    // strength rather than the window's opacity.
    if (policy.state == Backdrop::ACCENT_ENABLE_ACRYLICBLURBEHIND) {
        const D2D1_COLOR_F tint = g_settings.pillBgColor;
        const auto channel = [](float v) {
            return static_cast<DWORD>(Clamp(v, 0.0f, 1.0f) * 255.0f);
        };
        const DWORD tintAlpha = channel(g_settings.backdropTint);
        policy.gradientColor = (tintAlpha << 24) | (channel(tint.b) << 16) |
                               (channel(tint.g) << 8) | channel(tint.r);
    }

    Backdrop::CompositionAttributeData data = {};
    data.attribute = Backdrop::WCA_ACCENT_POLICY;
    data.data = &policy;
    data.dataSize = sizeof(policy);
    setAttribute(hwnd, &data);
}

// Always unregisters before registering so a settings change never leaks the
// previous hotkey. Safe to call before the overlay window exists (no-ops).
inline void ApplyHideShowHotkey() {
    if (!g_hwnd) {
        return;
    }

    if (g_hotkeyRegistered) {
        UnregisterHotKey(g_hwnd, ID_HIDE_SHOW_HOTKEY);
        g_hotkeyRegistered = false;
    }

    if (g_settings.hideShowHotkeyEnabled) {
        if (RegisterHotKey(g_hwnd, ID_HIDE_SHOW_HOTKEY, g_settings.hideShowModifiers,
                           g_settings.hideShowVk)) {
            g_registeredHotkeyModifiers = g_settings.hideShowModifiers;
            g_registeredHotkeyVk = g_settings.hideShowVk;
            g_hotkeyRegistered = true;
        } else {
            Wh_Log(L"Failed to register hide/show hotkey (error %lu).", GetLastError());
        }
    }
}



struct MonitorEnumData {
    std::vector<HMONITOR> monitors;
};

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData) {
    auto* data = reinterpret_cast<MonitorEnumData*>(dwData);
    data->monitors.push_back(hMonitor);
    return TRUE;
}

inline RECT GetAnchorWorkRect() {
    HMONITOR selectedMonitor = nullptr;

    if (g_settings.targetMonitor == -1) {
        POINT pt = {0, 0};
        GetCursorPos(&pt);
        selectedMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    } else if (g_settings.targetMonitor > 0) {
        MonitorEnumData data;
        EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&data));

        int index = g_settings.targetMonitor - 1;
        if (index >= 0 && index < static_cast<int>(data.monitors.size())) {
            selectedMonitor = data.monitors[index];
        }
    }

    if (!selectedMonitor) {
        POINT pt = {0, 0};
        selectedMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }

    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(selectedMonitor, &mi);
    return mi.rcWork;
}

// Vertical offset for the island's current size (#83).
//
// One Offset Y forced a compromise: a value that tucks the idle pill up near the
// screen edge leaves the expanded dashboard awkward to reach, and vice versa.
// With the separate offset enabled, Offset Y becomes the collapsed value and
// OffsetYExpanded the expanded one, and the island *eases* between them in step
// with the expansion itself rather than snapping at some threshold.
//
// windowHeight is the layered window's height, which carries kRenderPadY on both
// edges, so the pill's own height is recovered before measuring how expanded it
// is.
inline int ResolveOffsetY(float windowHeight) {
    if (!g_settings.separateExpandedOffsetY) {
        return g_settings.offsetY;
    }

    const float scale = std::max(0.05f, g_settings.sizeScale);
    const float pillHeight = std::max(0.0f, windowHeight - kRenderPadY * 2.0f) / scale;

    // Collapsed alert pills are ~44px tall in content space; the expanded
    // dashboard is MediaLayout::kExpandedHeight.
    constexpr float kCollapsedHeight = 44.0f;
    const float span = MediaLayout::kExpandedHeight - kCollapsedHeight;
    const float t = (span <= 1.0f) ? 0.0f
                                   : Clamp((pillHeight - kCollapsedHeight) / span, 0.0f, 1.0f);

    const float collapsed = static_cast<float>(g_settings.offsetY);
    const float expanded = static_cast<float>(g_settings.offsetYExpanded);
    return static_cast<int>(std::lround(collapsed + (expanded - collapsed) * t));
}

inline void PositionOverlayWindow(HWND hwnd, int width, int height) {
    RECT work = GetAnchorWorkRect();
    const bool flushTop = g_settings.notchStyle || g_settings.borderMergedMode;
    int x = work.left + (work.right - work.left - width) / 2;
    int y = flushTop ? work.top : (work.top + 8);

    switch (g_settings.position) {
        case Position::TopLeft:
            x = work.left + 16;
            y = flushTop ? work.top : (work.top + 8);
            break;
        case Position::TopRight:
            x = work.right - width - 16;
            y = flushTop ? work.top : (work.top + 8);
            break;
        case Position::BottomCenter:
            x = work.left + (work.right - work.left - width) / 2;
            y = work.bottom - height - 40;
            break;
        case Position::BottomLeft:
            x = work.left + 16;
            y = work.bottom - height - 40;
            break;
        case Position::BottomRight:
            x = work.right - width - 16;
            y = work.bottom - height - 40;
            break;
        case Position::TopCenter:
        default:
            break;
    }

    HWND zOrder = g_settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;

    // Manage owner window to firmly anchor to desktop when alwaysOnTop is false
    if (g_settings.alwaysOnTop) {
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, 0);
    } else {
        HWND hProgman = FindWindowW(L"Progman", nullptr);
        if (hProgman) {
            SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(hProgman));
        }
    }

    x += g_settings.offsetX;
    y += ResolveOffsetY(static_cast<float>(height));
    SetWindowPos(hwnd, zOrder, x, y, width, height,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);

    ApplyBackdropRegion(hwnd, width, height);
}

// A composition backdrop (#59) is blurred across the window's whole rectangle
// and is *not* masked by the per-pixel alpha we paint. Left alone that would
// show a blurred rectangle filling the transparent render padding around the
// island. Constraining the window to a rounded region confines the blur to the
// island's own silhouette.
//
// The region is cleared again whenever no backdrop is active, because it would
// otherwise clip the soft drop shadow, which is drawn out in that same padding.
inline void ApplyBackdropRegion(HWND hwnd, int windowWidth, int windowHeight) {
    if (!hwnd) {
        return;
    }

    if (g_settings.backdropMaterial == BackdropMaterial::None) {
        SetWindowRgn(hwnd, nullptr, TRUE);
        return;
    }

    const int pillWidth = windowWidth - static_cast<int>(std::round(kRenderPadX * 2.0f));
    const int pillHeight = windowHeight - static_cast<int>(std::round(kRenderPadY * 2.0f));
    if (pillWidth <= 1 || pillHeight <= 1) {
        SetWindowRgn(hwnd, nullptr, TRUE);
        return;
    }

    const int left = static_cast<int>(std::round(kRenderPadX));
    const int top = (g_settings.notchStyle || g_settings.borderMergedMode)
                        ? 0
                        : static_cast<int>(std::round(kRenderPadY));

    // The pill bounces a few pixels vertically via the nudge spring, which this
    // function does not see, so the region is padded to avoid clipping content
    // mid-animation.
    constexpr int kNudgeSlack = 10;
    const int regionTop = std::max(0, top - kNudgeSlack);
    const int regionBottom = std::min(windowHeight, top + pillHeight + kNudgeSlack);

    int radius;
    if (g_settings.notchStyle) {
        radius = static_cast<int>(std::round(16.0f * g_settings.sizeScale));
    } else if (g_settings.w11Style) {
        radius = static_cast<int>(std::round(8.0f * g_settings.sizeScale));
    } else {
        radius = static_cast<int>(std::round(
            std::min(pillHeight * 0.5f, 44.0f * g_settings.sizeScale)));
    }
    radius = std::max(0, radius);

    // CreateRoundRectRgn takes the full ellipse size, not the corner radius.
    HRGN region = CreateRoundRectRgn(left, regionTop, left + pillWidth + 1, regionBottom + 1,
                                     radius * 2, radius * 2);
    if (region) {
        // Ownership transfers to the system on success.
        if (SetWindowRgn(hwnd, region, TRUE) == 0) {
            DeleteObject(region);
        }
    }
}

// Windows' own shell surfaces are all WS_EX_TOPMOST, and they are *supposed* to
// sit above the island. Raising ourselves over the Start menu, the notification
// centre, the volume OSD, Task View, a popup menu or a tooltip would be a bug,
// not a fix, so these classes are never treated as something to reclaim from.
inline bool IsShellOwnedWindow(HWND hwnd) {
    wchar_t className[128] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0) {
        // Unknown, so treat it as shell-owned and leave the z-order alone.
        return true;
    }

    static const wchar_t* const kShellClasses[] = {
        L"Windows.UI.Core.CoreWindow",              // Start, Action Center, Search
        L"Xaml_WindowedPopupClass",                 // WinUI flyouts and popups
        L"Shell_TrayWnd",                           // taskbar
        L"Shell_SecondaryTrayWnd",                  // taskbar on other monitors
        L"TopLevelWindowForOverflowXamlIsland",     // taskbar overflow
        L"MultitaskingViewFrame",                   // Task View
        L"ForegroundStaging",                       // Task View staging
        L"#32768",                                  // popup menus
        L"tooltips_class32",                        // tooltips
        L"NarratorHelperWindow",
    };

    for (const wchar_t* candidate : kShellClasses) {
        if (_wcsicmp(className, candidate) == 0) {
            return true;
        }
    }
    return false;
}

// Re-asserts the island's place in the topmost band when another always-on-top
// window has been raised over it (PowerToys' bar, taskbar mods, other
// overlays). PositionOverlayWindow only runs on a resize or an explicit layout
// change, so without this the island stayed buried until the next one.
//
// Deliberately conservative: shell surfaces are ignored, a slight clip does not
// count, and we only act when we are still in the topmost band ourselves. That
// keeps this from turning into a z-order tug-of-war or from stealing focus
// surfaces away from Windows.
inline void EnsureTopmost(HWND hwnd) {
    if (!g_settings.alwaysOnTop || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
        return;
    }

    // If we are not topmost at all, PositionOverlayWindow owns fixing that;
    // walking the whole z-order from a demoted position would be expensive.
    if ((GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0) {
        return;
    }

    RECT islandRect = {};
    if (!GetWindowRect(hwnd, &islandRect)) {
        return;
    }

    const long islandArea = std::max(1L, (islandRect.right - islandRect.left) *
                                             (islandRect.bottom - islandRect.top));
    // Require a real overlap, not a one-pixel graze.
    const long minOverlapArea = std::max(256L, islandArea / 8);

    bool covered = false;
    for (HWND above = GetWindow(hwnd, GW_HWNDPREV); above != nullptr;
         above = GetWindow(above, GW_HWNDPREV)) {
        if (above == hwnd) {
            continue;
        }
        // Everything above a topmost window is itself topmost, so reaching a
        // non-topmost window means we are already at the top of that band.
        if ((GetWindowLongW(above, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0) {
            break;
        }
        if (!IsWindowVisible(above) || IsIconic(above) || IsShellOwnedWindow(above)) {
            continue;
        }

        RECT otherRect = {};
        RECT intersection = {};
        if (GetWindowRect(above, &otherRect) &&
            IntersectRect(&intersection, &islandRect, &otherRect)) {
            const long overlap = (intersection.right - intersection.left) *
                                 (intersection.bottom - intersection.top);
            if (overlap >= minOverlapArea) {
                covered = true;
                break;
            }
        }
    }

    if (covered) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
}

inline RECT GetIslandDockRect() {
    RECT work = GetAnchorWorkRect();
    const float scale = g_settings.sizeScale;
    const int dockWidth = static_cast<int>(std::max(280.0f, 340.0f * scale));
    const int dockHeight = static_cast<int>(std::max(48.0f, 56.0f * scale));
    const bool flushTop = g_settings.notchStyle || g_settings.borderMergedMode;

    int x = work.left + (work.right - work.left - dockWidth) / 2;
    int y = flushTop ? work.top : (work.top + 8);

    switch (g_settings.position) {
        case Position::TopLeft:
            x = work.left + 16;
            y = flushTop ? work.top : (work.top + 8);
            break;
        case Position::TopRight:
            x = work.right - dockWidth - 16;
            y = flushTop ? work.top : (work.top + 8);
            break;
        case Position::BottomCenter:
            x = work.left + (work.right - work.left - dockWidth) / 2;
            y = work.bottom - dockHeight - 40;
            break;
        case Position::BottomLeft:
            x = work.left + 16;
            y = work.bottom - dockHeight - 40;
            break;
        case Position::BottomRight:
            x = work.right - dockWidth - 16;
            y = work.bottom - dockHeight - 40;
            break;
        case Position::TopCenter:
        default:
            break;
    }

    x += g_settings.offsetX;
    // The dock rect is the hover target for a hidden island, which is always in
    // its collapsed state, so it follows the collapsed offset.
    y += g_settings.offsetY;

    RECT r;
    r.left = x;
    r.right = x + dockWidth;
    if (IsBottomPosition(g_settings.position)) {
        r.top = y;
        r.bottom = std::max(static_cast<int>(work.bottom), y + dockHeight + 20);
    } else {
        r.top = std::min(y, static_cast<int>(work.top));
        r.bottom = y + dockHeight;
    }
    return r;
}

static BOOL CALLBACK EnumWindowsTitleProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, ARRAYSIZE(title));
    if (title[0] != L'\0') {
        if (wcsstr(title, L"Antigravity") != nullptr) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL CALLBACK EnumWindowsProcProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        wchar_t procPath[MAX_PATH] = {};
        DWORD size = ARRAYSIZE(procPath);
        if (QueryFullProcessImageNameW(hProc, 0, procPath, &size)) {
            for (DWORD i = 0; i < size; ++i) {
                procPath[i] = towlower(procPath[i]);
            }
            if (wcsstr(procPath, L"antigravity") != nullptr) {
                *reinterpret_cast<HWND*>(lParam) = hwnd;
                CloseHandle(hProc);
                return FALSE;
            }
        }
        CloseHandle(hProc);
    }
    return TRUE;
}

inline bool FocusAntigravityWindow() {
    HWND targetHwnd = nullptr;

    // 1. Search top-level windows for Antigravity in the title
    EnumWindows(EnumWindowsTitleProc, reinterpret_cast<LPARAM>(&targetHwnd));

    // 2. Fallback: Search processes for antigravity executable
    if (!targetHwnd) {
        EnumWindows(EnumWindowsProcProc, reinterpret_cast<LPARAM>(&targetHwnd));
    }

    if (targetHwnd) {
        HWND hForeground = GetForegroundWindow();
        DWORD foregroundThread = GetWindowThreadProcessId(hForeground, nullptr);
        DWORD currentThread = GetCurrentThreadId();
        if (foregroundThread != currentThread) {
            AttachThreadInput(currentThread, foregroundThread, TRUE);
            BringWindowToTop(targetHwnd);
            if (IsIconic(targetHwnd)) ShowWindow(targetHwnd, SW_RESTORE);
            SetForegroundWindow(targetHwnd);
            AttachThreadInput(currentThread, foregroundThread, FALSE);
        } else {
            BringWindowToTop(targetHwnd);
            if (IsIconic(targetHwnd)) ShowWindow(targetHwnd, SW_RESTORE);
            SetForegroundWindow(targetHwnd);
        }
        return true;
    }
    return false;
}


inline std::wstring ReadClipboardText(HWND hwnd) {
    std::wstring text;
    if (!OpenClipboard(hwnd)) {
        return text;
    }

    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data) {
        auto* locked = static_cast<const wchar_t*>(GlobalLock(data));
        if (locked) {
            text = locked;
            GlobalUnlock(data);
        }
    }

    CloseClipboard();
    return text;
}

// Decodes whatever bitmap is currently on the clipboard into a small BGRA
// thumbnail, aspect-preserving and capped at maxDim on the longer side.
// Requesting CF_BITMAP works regardless of which bitmap format the source
// app actually placed on the clipboard (CF_DIB/CF_DIBV5) — Windows
// synthesizes CF_BITMAP from those automatically.
inline bool ReadClipboardImagePixels(HWND hwnd, BitmapPixels* outPixels, UINT maxDim) {
    if (!outPixels || !OpenClipboard(hwnd)) {
        return false;
    }

    HBITMAP sourceBitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    if (!sourceBitmap) {
        CloseClipboard();
        return false;
    }

    BITMAP bm = {};
    if (!GetObject(sourceBitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        CloseClipboard();
        return false;
    }

    UINT srcW = static_cast<UINT>(bm.bmWidth);
    UINT srcH = static_cast<UINT>(bm.bmHeight);
    UINT dstW = srcW;
    UINT dstH = srcH;
    if (srcW > maxDim || srcH > maxDim) {
        const float scale = static_cast<float>(maxDim) / static_cast<float>(std::max(srcW, srcH));
        dstW = std::max<UINT>(1, static_cast<UINT>(srcW * scale));
        dstH = std::max<UINT>(1, static_cast<UINT>(srcH * scale));
    }

    HDC screen = GetDC(nullptr);
    HDC srcDc = CreateCompatibleDC(screen);
    HDC dstDc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (!srcDc || !dstDc) {
        if (srcDc) DeleteDC(srcDc);
        if (dstDc) DeleteDC(dstDc);
        CloseClipboard();
        return false;
    }

    // Selecting the clipboard's own HBITMAP into a scratch DC to read from it
    // is the MSDN-documented way to do this; we must not DeleteObject it.
    HGDIOBJ oldSrc = SelectObject(srcDc, sourceBitmap);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = static_cast<LONG>(dstW);
    bi.bmiHeader.biHeight = -static_cast<LONG>(dstH);  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(dstDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib) {
        SelectObject(srcDc, oldSrc);
        DeleteDC(srcDc);
        DeleteDC(dstDc);
        CloseClipboard();
        return false;
    }

    HGDIOBJ oldDst = SelectObject(dstDc, dib);
    SetStretchBltMode(dstDc, HALFTONE);
    SetBrushOrgEx(dstDc, 0, 0, nullptr);
    StretchBlt(dstDc, 0, 0, static_cast<int>(dstW), static_cast<int>(dstH),
               srcDc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

    BitmapPixels pixels;
    pixels.width = dstW;
    pixels.height = dstH;
    pixels.bgra.resize(static_cast<size_t>(dstW) * dstH * 4);
    memcpy(pixels.bgra.data(), bits, pixels.bgra.size());

    // GDI blits never populate an alpha channel; without this the D2D bitmap
    // (which expects premultiplied alpha) would render fully invisible.
    for (size_t i = 3; i < pixels.bgra.size(); i += 4) {
        pixels.bgra[i] = 255;
    }

    pixels.generation = ++g_artGenerationCounter;
    *outPixels = std::move(pixels);

    SelectObject(dstDc, oldDst);
    DeleteObject(dib);
    SelectObject(srcDc, oldSrc);
    DeleteDC(srcDc);
    DeleteDC(dstDc);
    CloseClipboard();
    return true;
}

inline bool IsLikelyToastWindow(HWND hwnd, const wchar_t* className, const wchar_t* title) {
    if (hwnd == g_hwnd || !hwnd) {
        return false;
    }

    if (GetWindow(hwnd, GW_OWNER)) {
        return false;
    }

    const std::wstring cls = ToLowerCopy(className ? className : L"");
    const std::wstring text = ToLowerCopy(title ? title : L"");

    // Classic Windows 10 toasts have clear class names
    if (cls.find(L"notification") != std::wstring::npos ||
        cls.find(L"toast") != std::wstring::npos ||
        cls.find(L"windows.ui.notifications") != std::wstring::npos) {
        return true;
    }

    // Windows 11 toasts use generic XAML or CoreWindow classes, usually hosted by
    // explorer.exe, sihost.exe, or ShellExperienceHost.exe.
    // Importantly, their title is often empty at the exact moment of creation!
    if (cls.find(L"xaml_windowedpopupclass") != std::wstring::npos ||
        cls.find(L"windows.ui.core.corewindow") != std::wstring::npos) {

        std::wstring image;
        if (ProcessImageNameForWindow(hwnd, &image)) {
            const std::wstring base = ToLowerCopy(BaseNameFromPath(image));
            if (base == L"explorer.exe" || base == L"sihost.exe" || base == L"shellexperiencehost.exe") {
                // Ensure it's not the start menu, search, or action center main panel
                if (text != L"start" && text != L"action center" && text != L"search" && text != L"task view") {
                    return true;
                }
            }
        }
    }

    return false;
}

inline void CaptureShellNotification(HWND hwnd) {
    // Grace period: ignore notifications that fire in the first 3 seconds after
    // the mod starts. sihost.exe gets injected while system windows are still
    // settling, which causes false positives (e.g. Snipping Tool windows).
    if (NowSeconds() < 3.0) {
        return;
    }

    wchar_t className[128] = {};
    wchar_t title[192] = {};
    GetClassNameW(hwnd, className, ARRAYSIZE(className));
    GetWindowTextW(hwnd, title, ARRAYSIZE(title));

    if (!IsLikelyToastWindow(hwnd, className, title)) {
        return;
    }

    NotificationSnapshot notification;
    notification.active = true;
    notification.app = L"Notification";
    notification.title = title;
    notification.expiresAt = NowSeconds() + 4.0;
    // Fetch a 64px icon to ensure crisp rendering inside the pill
    notification.icon = GetWindowIconPixels(hwnd, 64);

    if (notification.title.size() > 96) {
        notification.body = notification.title.substr(64);
        notification.title.resize(64);
        notification.title += L"...";
    }

    {
        std::lock_guard lock(g_stateMutex);
        g_state.notification = std::move(notification);
    }
    TriggerNudge();

    // Spawn a background thread to extract the full rich text body of the toast using UI Automation.
    // Modern Windows Toasts often only provide the App Name via GetWindowTextW, leaving the body hidden in the XAML tree.
    std::thread([hwnd]() {
        Sleep(400); // Give the heavy UWP XAML tree enough time to fully construct the text nodes
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(hr)) {
            IUIAutomation* uia = nullptr;
            hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void**)&uia);
            if (SUCCEEDED(hr) && uia) {
                IUIAutomation2* uia2 = nullptr;
                if (SUCCEEDED(uia->QueryInterface(__uuidof(IUIAutomation2), (void**)&uia2)) && uia2) {
                    uia2->put_TransactionTimeout(500);
                    uia2->put_ConnectionTimeout(500);
                    uia2->Release();
                }
                IUIAutomationElement* windowEl = nullptr;
                if (SUCCEEDED(uia->ElementFromHandle(hwnd, &windowEl)) && windowEl) {
                    IUIAutomationCondition* cond = nullptr;
                    uia->CreateTrueCondition(&cond);
                    IUIAutomationElementArray* elements = nullptr;
                    if (SUCCEEDED(windowEl->FindAll(TreeScope_Descendants, cond, &elements)) && elements) {
                        int count = 0;
                        elements->get_Length(&count);
                        std::wstring appName;
                        std::wstring fullText;
                        for (int i = 0; i < count; ++i) {
                            IUIAutomationElement* el = nullptr;
                            if (SUCCEEDED(elements->GetElement(i, &el)) && el) {
                                BSTR name = nullptr;
                                el->get_CurrentName(&name);
                                if (name && wcslen(name) > 0) {
                                    std::wstring chunk = name;
                                    // Skip generic screen-reader labels often found in toasts
                                    if (chunk != L"Notification" && chunk != L"New notification") {
                                        if (appName.empty()) {
                                            appName = chunk;
                                        } else {
                                            if (!fullText.empty()) fullText += L"  -  ";
                                            fullText += chunk;
                                        }
                                    }
                                }
                                if (name) SysFreeString(name);
                                el->Release();
                            }
                        }
                        elements->Release();

                        if (fullText.empty() && !appName.empty()) {
                            fullText = appName;
                            appName = L"Notification";
                        }

                        if (!fullText.empty()) {
                            std::lock_guard lock(g_stateMutex);
                            if (g_state.notification.active) {
                                if (!appName.empty()) {
                                    g_state.notification.app = appName;
                                    if (appName != L"Notification") {
                                        BitmapPixels resolvedIcon = FindAppIconByName(appName, 64);
                                        if (!resolvedIcon.bgra.empty()) {
                                            g_state.notification.icon = std::move(resolvedIcon);
                                        }
                                    }
                                }
                                g_state.notification.title = fullText;
                            }
                        }
                    }
                    if (cond) cond->Release();
                    windowEl->Release();
                }
                uia->Release();
            }
            CoUninitialize();
        }
    }).detach();
}

inline void CaptureClipboard(HWND hwnd) {
    ClipboardSnapshot clip;
    clip.expiresAt = NowSeconds() + 2.5;
    HWND owner = GetClipboardOwner();
    if (!owner) {
        owner = GetForegroundWindow();
    }
    wchar_t ownerTitle[80] = {};
    if (owner) {
        GetWindowTextW(owner, ownerTitle, ARRAYSIZE(ownerTitle));
    }
    if (owner && !IsIgnorableForegroundWindow(owner, ownerTitle)) {
        DWORD pid = 0;
        GetWindowThreadProcessId(owner, &pid);
        // Fetch at 64px for crisp rendering — 18px/32px is often too small for icon APIs and returns empty.
        clip.appIcon = GetWindowIconPixels(owner, 64);

        clip.appName = ownerTitle;
        if (clip.appName.empty()) {
            std::wstring path;
            if (ProcessImageNameForPid(pid, &path)) {
                clip.appName = StripExtension(BaseNameFromPath(path));
            }
        }
        if (clip.appName.size() > 24) {
            clip.appName.resize(24);
            clip.appName += L"...";
        }
    }

    std::wstring text = ReadClipboardText(hwnd);
    if (!text.empty()) {
        constexpr size_t maxChars = 96;
        std::replace(text.begin(), text.end(), L'\r', L' ');
        std::replace(text.begin(), text.end(), L'\n', L' ');
        if (text.size() > maxChars) {
            text.resize(maxChars);
            text += L"...";
        }
        clip.text = text;
        clip.image = false;
        clip.active = true;
    } else {
        BitmapPixels imagePixels;
        if (ReadClipboardImagePixels(hwnd, &imagePixels, kClipboardImageThumbMaxDim)) {
            clip.text = L"Image copied";
            clip.image = true;
            clip.active = true;
            clip.imagePreview = std::move(imagePixels);
        }
    }

    if (clip.active) {
        {
            std::lock_guard lock(g_stateMutex);
            g_state.clipboard = std::move(clip);
        }
    }
}

inline void SetClickThrough(HWND hwnd, bool clickThrough) {
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const bool has = (exStyle & WS_EX_TRANSPARENT) != 0;
    if (clickThrough == has) {
        return;
    }

    if (clickThrough) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }

    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// void OpenRelevantApp();


void ToggleEndpointMute();
// void SeekMediaToTicks(int64_t targetTicks);

inline void HandleStatusClickAtPoint(HWND hwnd, LPARAM lParam) {
    return;
}

inline void StartFocusTimer(int minutes, bool isBreak) {
    {
        std::lock_guard lock(g_stateMutex);
        g_state.timer.active = true;
        g_state.timer.running = true;
        g_state.timer.isBreak = isBreak;
        g_state.timer.totalSeconds = minutes * 60;
        g_state.timer.endsAt = NowSeconds() + minutes * 60;
        g_state.timer.remainingAtPause = 0.0;
        g_state.timer.justFinished = false;
    }
    TriggerNudge();
}

inline void ToggleTimerPause() {
    {
        std::lock_guard lock(g_stateMutex);
        if (!g_state.timer.active) {
            return;
        }
        const double now = NowSeconds();
        if (g_state.timer.running) {
            g_state.timer.remainingAtPause = std::max(0.0, g_state.timer.endsAt - now);
            g_state.timer.running = false;
        } else {
            g_state.timer.endsAt = now + g_state.timer.remainingAtPause;
            g_state.timer.running = true;
        }
    }
    TriggerNudge();
}

inline void StopFocusTimer() {
    {
        std::lock_guard lock(g_stateMutex);
        g_state.timer.active = false;
        g_state.timer.running = false;
        g_state.timer.justFinished = false;
    }
    TriggerNudge();
}

inline void ToggleEndpointMute() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioEndpointVolume> volume;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(hr)) {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    }
    if (SUCCEEDED(hr)) {
        hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                              reinterpret_cast<void**>(volume.GetAddressOf()));
    }
    if (SUCCEEDED(hr)) {
        BOOL muted = FALSE;
        volume->GetMute(&muted);
        volume->SetMute(!muted, nullptr);
        std::lock_guard lock(g_stateMutex);
        g_state.muted = !muted;
    }
}

// Shared by the scrubber's live drag updates and its on-release commit.

inline void DismissTransientState() {
    std::lock_guard lock(g_stateMutex);
    g_state.clipboard.active = false;
    g_state.notification.active = false;
    g_state.volume.active = false;
    g_state.progress.active = false;
    g_state.capsLock.active = false;
    g_state.device.active = false;
    g_state.bluetoothDevice.active = false;
    g_state.battery.active = false;
    Wh_SetIntValue(L"ProgressPercent", -1);
}

inline void ShowContextMenu(HWND hwnd, POINT screenPoint) {
    bool timerActive = false;
    bool timerRunning = false;
    {
        std::lock_guard lock(g_stateMutex);
        timerActive = g_state.timer.active;
        timerRunning = g_state.timer.running;
    }

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 40, g_manuallyHidden.load() ? L"Show Island" : L"Hide Island");
    AppendMenuW(menu, MF_STRING, 1, L"Dismiss");

    HMENU timerMenu = CreatePopupMenu();
    AppendMenuW(timerMenu, MF_STRING, 30, L"Start 25 min Focus");
    AppendMenuW(timerMenu, MF_STRING, 31, L"Start 50 min Focus");
    AppendMenuW(timerMenu, MF_STRING, 32, L"Start 5 min Break");
    if (timerActive) {
        AppendMenuW(timerMenu, MF_STRING, 33, timerRunning ? L"Pause Timer" : L"Resume Timer");
        AppendMenuW(timerMenu, MF_STRING, 34, L"Stop Timer");
    }
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(timerMenu), L"Focus Timer");
    if (g_settings.fileTrayModule) {
        size_t trayCount = 0;
        {
            std::lock_guard lock(g_stateMutex);
            trayCount = g_state.fileTrayItems.size();
        }
        std::wstring label = std::wstring(Loc(L"File Tray")) + L": ";
        if (trayCount == 0) {
            label += L"—";
            AppendMenuW(menu, MF_STRING | MF_GRAYED | MF_DISABLED, 41, label.c_str());
        } else {
            label += L"Clear " + std::to_wstring(trayCount);
            AppendMenuW(menu, MF_STRING, 41, label.c_str());
        }
    }
    AppendMenuW(menu, MF_STRING, 2, L"Pin expanded");
    AppendMenuW(menu, MF_STRING, 3, Wh_GetIntValue(L"GameOverlayPinned", 0) ? L"Hide game overlay" : L"Show game overlay");
    std::wstring shapeStr = GetStringSettingCopy(L"Appearance.ShapeStyle");
    const int activeW11 = Wh_GetIntValue(L"W11StyleOverride", -1) >= 0
                          ? Wh_GetIntValue(L"W11StyleOverride", 0)
                          : EqualsNoCase(shapeStr, L"w11");
    AppendMenuW(menu, MF_STRING, 10, activeW11 ? L"Use iPhone Pill Style" : L"Use Windows 11 Flyout Style");
    const int activeNotch = Wh_GetIntValue(L"NotchStyleOverride", -1) >= 0
                          ? Wh_GetIntValue(L"NotchStyleOverride", 0)
                          : EqualsNoCase(shapeStr, L"notch");
    if (IsBottomPosition(g_settings.position)) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED | MF_DISABLED, 12, L"macOS Notch (Unavailable at Bottom)");
    } else {
        AppendMenuW(menu, MF_STRING, 12, activeNotch ? L"Disable macOS Notch Style" : L"Use macOS Notch Style");
    }
    const int activeExpandOnHover = Wh_GetIntValue(L"ExpandOnHoverOverride", -1) >= 0
                          ? Wh_GetIntValue(L"ExpandOnHoverOverride", 0)
                          : (Wh_GetIntSetting(L"Behavior.ExpandOnHover") != 0);
    AppendMenuW(menu, MF_STRING, 11, activeExpandOnHover ? L"Expand on Click" : L"Expand on Hover");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 4, L"Transparency 100%");
    AppendMenuW(menu, MF_STRING, 5, L"Transparency 85%");
    AppendMenuW(menu, MF_STRING, 6, L"Transparency 70%");
    AppendMenuW(menu, MF_STRING, 7, L"Transparency 55%");
    AppendMenuW(menu, MF_STRING, 8, L"Reset transparency");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    // Color theme presets. Built from kThemePalettes and nested in a submenu:
    // nine palettes listed flat would dominate the root menu.
    const int activeTheme = Wh_GetIntValue(kThemeValueName, static_cast<int>(g_settings.themePreset));
    HMENU themeMenu = CreatePopupMenu();
    for (int i = 0; i < kCustomThemeIndex; ++i) {
        AppendMenuW(themeMenu, MF_STRING | (activeTheme == i ? MF_CHECKED : 0),
                    kThemeMenuIdBase + static_cast<UINT>(i), kThemePalettes[i].label);
    }
    AppendMenuW(themeMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(themeMenu, MF_STRING | (activeTheme >= kCustomThemeIndex ? MF_CHECKED : 0),
                kThemeMenuIdBase + static_cast<UINT>(kCustomThemeIndex), L"Custom Colors");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(themeMenu), L"Theme");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 9, L"Open Windhawk settings");

    SetForegroundWindow(hwnd);
    const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                   screenPoint.x, screenPoint.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    // Theme ids are a contiguous range rather than individual cases, so adding a
    // palette to kThemePalettes needs no change here.
    if (cmd >= kThemeMenuIdBase &&
        cmd <= kThemeMenuIdBase + static_cast<UINT>(kCustomThemeIndex)) {
        Wh_SetIntValue(kThemeValueName, static_cast<int>(cmd - kThemeMenuIdBase));
        LoadSettings();
        return;
    }

    switch (cmd) {
        case 1:
            DismissTransientState();
            g_clickExpanded = false;
            g_layoutDirty = true;
            TriggerNudge();
            break;
        case 2:
            Wh_SetIntValue(L"PinnedExpanded", Wh_GetIntValue(L"PinnedExpanded", 0) ? 0 : 1);
            break;
        case 3:
            Wh_SetIntValue(L"GameOverlayPinned", Wh_GetIntValue(L"GameOverlayPinned", 0) ? 0 : 1);
            break;
        case 4:
            Wh_SetIntValue(L"PillOpacityOverride", 100);
            LoadSettings();
            break;
        case 5:
            Wh_SetIntValue(L"PillOpacityOverride", 85);
            LoadSettings();
            break;
        case 6:
            Wh_SetIntValue(L"PillOpacityOverride", 70);
            LoadSettings();
            break;
        case 7:
            Wh_SetIntValue(L"PillOpacityOverride", 55);
            LoadSettings();
            break;
        case 8:
            Wh_SetIntValue(L"PillOpacityOverride", -1);
            LoadSettings();
            break;
        case 9: {
            // Launch the Windhawk UI, resolved relative to our own module rather
            // than by launching our own executable.
            //
            // Running the current process directly only worked because under
            // Windhawk 1.x that *is* windhawk.exe. Windhawk 2.0 runs tool mods in
            // a separate windhawk-mod.exe, so re-launching the current process
            // would have spawned another mod host instead of opening settings.
            // Taking the directory and appending windhawk.exe is correct on both,
            // since the two executables live side by side.
            wchar_t windhawkPath[MAX_PATH] = {};
            const DWORD pathLen =
                GetModuleFileNameW(nullptr, windhawkPath, ARRAYSIZE(windhawkPath));
            if (pathLen == 0 || pathLen >= ARRAYSIZE(windhawkPath)) {
                Wh_Log(L"Could not resolve the Windhawk directory (GetModuleFileNameW: %lu).",
                       pathLen);
                break;
            }

            wchar_t* lastSlash = wcsrchr(windhawkPath, L'\\');
            if (!lastSlash) {
                Wh_Log(L"Unexpected module path with no directory separator.");
                break;
            }
            *(lastSlash + 1) = L'\0';  // keep the trailing slash

            if (wcscat_s(windhawkPath, ARRAYSIZE(windhawkPath), L"windhawk.exe") != 0) {
                Wh_Log(L"Windhawk directory path is too long to append the executable name.");
                break;
            }

            HINSTANCE result = ShellExecuteW(nullptr, L"open",
                                             windhawkPath,
                                             nullptr,
                                             nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) <= 32) {
                Wh_Log(L"Failed to open Windhawk settings (%s).", windhawkPath);
            }
            break;
        }
        case 10: {
            std::wstring shapeStr = GetStringSettingCopy(L"Appearance.ShapeStyle");
            const int activeW11Val = Wh_GetIntValue(L"W11StyleOverride", -1) >= 0
                                  ? Wh_GetIntValue(L"W11StyleOverride", 0)
                                  : EqualsNoCase(shapeStr, L"w11");
            Wh_SetIntValue(L"W11StyleOverride", activeW11Val ? 0 : 1);
            if (!activeW11Val) Wh_SetIntValue(L"NotchStyleOverride", 0);
            LoadSettings();
            g_layoutDirty = true;
            break;
        }
        case 12: {
            if (IsBottomPosition(g_settings.position)) {
                break;
            }
            std::wstring shapeStr = GetStringSettingCopy(L"Appearance.ShapeStyle");
            const int activeNotchVal = Wh_GetIntValue(L"NotchStyleOverride", -1) >= 0
                                    ? Wh_GetIntValue(L"NotchStyleOverride", 0)
                                    : EqualsNoCase(shapeStr, L"notch");
            Wh_SetIntValue(L"NotchStyleOverride", activeNotchVal ? 0 : 1);
            if (!activeNotchVal) Wh_SetIntValue(L"W11StyleOverride", 0);
            LoadSettings();
            g_layoutDirty = true;
            TriggerNudge();
            break;
        }
        case 11: {
            const int activeExpandOnHover = Wh_GetIntValue(L"ExpandOnHoverOverride", -1) >= 0
                                  ? Wh_GetIntValue(L"ExpandOnHoverOverride", 0)
                                  : (Wh_GetIntSetting(L"Behavior.ExpandOnHover") != 0);
            Wh_SetIntValue(L"ExpandOnHoverOverride", activeExpandOnHover ? 0 : 1);
            LoadSettings();
            g_layoutDirty = true;
            break;
        }
        case 30:
            StartFocusTimer(25, false);
            break;
        case 31:
            StartFocusTimer(50, false);
            break;
        case 32:
            StartFocusTimer(5, true);
            break;
        case 33:
            ToggleTimerPause();
            break;
        case 34:
            StopFocusTimer();
            break;
        case 40: {
            // Only reachable while visible (a hidden window can't be
            // right-clicked), so in practice this toggles hidden -> the
            // hotkey is the only way back. That's intentional; see readme.
            const bool nowHidden = !g_manuallyHidden.load();
            g_manuallyHidden = nowHidden;
            Wh_SetIntValue(L"ManuallyHidden", nowHidden ? 1 : 0);
            if (nowHidden) {
                g_hotkeyUnhideUntil.store(0.0);
            }
            g_layoutDirty = true;
            break;
        }
        case 41: {
            // Clear the File Tray. Only the island's references are dropped --
            // nothing on disk is touched.
            {
                std::lock_guard lock(g_stateMutex);
                g_state.fileTrayItems.clear();
            }
            g_hoveredFileTrayRow = -1;
            g_layoutDirty = true;
            break;
        }
    }
}

// ── Weather iconography ──────────────────────────────────────────────────────
// The dashboard used colour emoji (☀️ ⛅ 🌡️) rendered with ENABLE_COLOR_FONT,
// which clashed badly with the island's monochrome, accent-tinted UI and varied
// in size and baseline between glyphs. These are drawn as vectors instead: they
// inherit the accent colour, scale cleanly, sit on a predictable baseline, and
// can never fall back to a missing-glyph box the way a font-dependent icon can.

inline Activity ActivityForKind(IslandKind kind, const Settings& settings, const SharedState& state) {
    Activity activity;
    activity.kind = kind;

    switch (kind) {
        case IslandKind::Media:
            activity.width = 180.0f;
            activity.height = 44.0f;
            break;
        case IslandKind::Progress:
            activity.width = 230.0f;
            activity.height = 48.0f;
            break;
        case IslandKind::Clipboard:
            activity.width = 340.0f;
            activity.height = 56.0f;
            break;
        case IslandKind::Notification:
            activity.width = 360.0f;
            activity.height = 58.0f;
            break;
        case IslandKind::Volume:
            activity.width = 300.0f;
            activity.height = 54.0f;
            break;
        case IslandKind::BatteryLow:
            activity.width = 290.0f;
            activity.height = 52.0f;
            break;
        case IslandKind::CapsLock:
            activity.width = 180.0f;
            activity.height = 42.0f;
            break;
        case IslandKind::Device:
            activity.width = 240.0f;
            activity.height = 50.0f;
            break;
        case IslandKind::Bluetooth:
            activity.width = 280.0f;
            activity.height = 54.0f;
            break;
        case IslandKind::Timer:
            activity.width = 260.0f;
            activity.height = 54.0f;
            break;
        case IslandKind::DoNotDisturb:
            activity.width = 220.0f;
            activity.height = 42.0f;
            break;
        case IslandKind::Idle:
        default:
            if (settings.autoHideIdleSeconds == -1 && !state.system.micActive && !state.system.cameraActive) {
                activity.width = 0.0f;
                activity.height = 0.0f;
            } else {
                // Seed only. The real collapsed width is measured from the clock
                // and weather strings by Renderer::MeasureIdleStrip and applied in
                // the render loop -- this function has no DWrite access. A fixed
                // width here was the whole bug behind windhawk-mods#5086: dead
                // air around a short "9:41", and clipping on "10:41:32 PM" once
                // Text size reached 140.
                activity.width = settings.weather ? 170.0f : 96.0f;
                activity.height = IdleStripLayout::kHeight;
            }
            break;
    }

    activity.width *= settings.sizeScale;
    activity.height *= settings.sizeScale;
    return activity;
}

inline std::vector<IslandKind> ChooseActivities(const SharedState& state, const Settings& settings, double now) {
    std::vector<IslandKind> activities;

    if (state.clipboard.active && now < state.clipboard.expiresAt) {
        activities.push_back(IslandKind::Clipboard);
    }
    // settings.capsLock is checked here, like every other module above and below.
    // Leaving it out was what made the Caps Lock toggle not actually work: the
    // handler gate stopped the nudge but the pill was still selected and drawn.
    if (settings.capsLock && state.capsLock.active && now < state.capsLock.expiresAt) {
        activities.push_back(IslandKind::CapsLock);
    }
    if (state.device.active && now < state.device.expiresAt) {
        activities.push_back(IslandKind::Device);
    }
    if (settings.bluetoothIndicator && state.bluetoothDevice.active &&
        now < state.bluetoothDevice.expiresAt) {
        activities.push_back(IslandKind::Bluetooth);
    }
    if (settings.doNotDisturbIndicator && state.doNotDisturb.active &&
        now < state.doNotDisturb.expiresAt) {
        activities.push_back(IslandKind::DoNotDisturb);
    }
    if (settings.volume && state.volume.active && now < state.volume.expiresAt) {
        activities.push_back(IslandKind::Volume);
    }
    if (state.notification.active && now < state.notification.expiresAt) {
        activities.push_back(IslandKind::Notification);
    }
    if (settings.battery && state.battery.active && now < state.battery.expiresAt) {
        activities.push_back(IslandKind::BatteryLow);
    }
    if (settings.progress && state.progress.active) {
        activities.push_back(IslandKind::Progress);
    }
    if (settings.timerEnabled &&
        ((state.timer.justFinished && now < state.timer.finishedExpiresAt) ||
         state.timer.active)) {
        activities.push_back(IslandKind::Timer);
    }
    if (settings.media && state.media.available) {
        activities.push_back(IslandKind::Media);
    }

    if (activities.empty()) {
        activities.push_back(IslandKind::Idle);
    }

    return activities;
}

// constexpr UINT WM_APP_CAPSLOCK defined in island_common.hpp
inline HHOOK g_keyboardHook = nullptr;
inline HANDLE g_keyboardThread = nullptr;
inline DWORD g_keyboardThreadId = 0;

// Deliberately does nothing but forward the event.
//
// This used to write g_state.capsLock here, under g_stateMutex, before posting.
// Two problems with that. It recorded the pill even when the Caps Lock module was
// switched off, because the setting is only checked once the message reaches the
// window thread -- so the pill still appeared for its full 2.5s. And a
// WH_KEYBOARD_LL callback runs inline on the input path, blocking every keystroke
// system-wide until it returns, so taking a lock that the render, weather or
// media threads also hold risked stalling typing on the whole desktop.
//
// The WM_APP_CAPSLOCK handler writes exactly the same fields on the window
// thread, after checking the setting. Nothing is lost by only posting.
inline LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            auto* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            if (kbd->vkCode == VK_CAPITAL || kbd->vkCode == VK_NUMLOCK) {
                const bool capsOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                const bool numOn = (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
                if (HWND hwnd = g_hwnd) {
                    const LPARAM state = (capsOn ? 1 : 0) | (numOn ? 2 : 0);
                    PostMessageW(hwnd, WM_APP_CAPSLOCK, kbd->vkCode, state);
                }
            }
        }
    }
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

// A system-wide WH_KEYBOARD_LL puts this process on the path of every keystroke
// on the desktop, so it is only installed while it has something to report. The
// hook exists solely to notice Caps Lock and Num Lock for the indicator pill;
// with that module off it was still running for nothing.
static bool CapsLockHookWanted() {
    return g_settings.capsLock;
}

// Wakes the keyboard thread so it re-evaluates whether the hook is needed.
inline void NotifyKeyboardThreadSettingChanged() {
    if (g_keyboardThreadId != 0) {
        PostThreadMessageW(g_keyboardThreadId, WM_NULL, 0, 0);
    }
}

inline DWORD WINAPI KeyboardThreadProc(void*) {
    // Bounded by the stop event rather than an open-ended Sleep loop, so an early
    // unload cannot leave this spinning while waiting for a window that is never
    // going to appear.
    while (!g_hwnd && WaitForSingleObject(g_stopEvent, 10) == WAIT_TIMEOUT) {
    }

    bool quit = false;
    while (!quit && WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        const bool wanted = CapsLockHookWanted();
        if (wanted && !g_keyboardHook) {
            g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
        } else if (!wanted && g_keyboardHook) {
            UnhookWindowsHookEx(g_keyboardHook);
            g_keyboardHook = nullptr;
        }

        // A low-level hook is delivered through the installing thread's message
        // queue, so this has to keep pumping while the hook is up.
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                quit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (quit) {
            break;
        }

        // Blocks until the stop event fires, a message arrives, or the timeout.
        // LoadSettings posts a WM_NULL when the module is toggled so the hook is
        // reconciled at once; the timeout is only a backstop. A timed wait, not a
        // spin, so an idle keyboard thread costs nothing.
        MsgWaitForMultipleObjects(1, &g_stopEvent, FALSE, 1000, QS_ALLINPUT);
    }

    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
    return 0;
}

// --- Low-level mouse hook: wakes the parked render thread when the cursor
// approaches where the (currently OS-hidden) island sits, so hover-to-unhide
// keeps working without polling GetCursorPos every frame.
inline HHOOK g_mouseHook = nullptr;
inline HANDLE g_mouseThread = nullptr;
inline DWORD g_mouseThreadId = 0;

// Wakes the mouse thread so it re-evaluates whether the wake hook is needed.
// Posted whenever the island parks or unparks, so the hook is installed and
// removed in step with that rather than on the thread's backstop timeout.
inline void NotifyMouseThreadParkedChanged() {
    if (g_mouseThreadId != 0) {
        PostThreadMessageW(g_mouseThreadId, WM_NULL, 0, 0);
    }
}
inline std::atomic<int64_t> g_lastMouseWakeCheckMs = 0;

inline LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_MOUSEMOVE &&
        g_settings.unhideOnHover &&
        g_autoHiddenParked.load(std::memory_order_relaxed)) {
        const int64_t nowMs = static_cast<int64_t>(GetTickCount64());
        const int64_t last = g_lastMouseWakeCheckMs.load(std::memory_order_relaxed);
        if (nowMs - last >= 60) {  // ~16Hz check rate for responsive unhide
            g_lastMouseWakeCheckMs.store(nowMs, std::memory_order_relaxed);
            auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            HWND hwnd = g_hwnd;
            RECT dockRect = GetIslandDockRect();
            if (hwnd && PtInRect(&dockRect, info->pt)) {
                g_autoHiddenParked = false;
                PostMessageW(hwnd, WM_APP_MOUSE_WAKE, 0, 0);
            }
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

// A system-wide WH_MOUSE_LL routes every mouse event on the desktop through this
// thread, so it is only installed while it can actually do something: the hook's
// whole job is to notice a hover over a *parked* island.
//
// Gating on the settings instead was not enough. UnhideOnHover and
// AutoHideFullscreen both default to true, so every user on defaults still got
// the hook from startup -- the exact case the gate was added to avoid. Tying it
// to the parked state means it exists only while the island is actually hidden.
static bool MouseWakeHookWanted() {
    return g_settings.unhideOnHover &&
           g_autoHiddenParked.load(std::memory_order_relaxed);
}

inline DWORD WINAPI MouseThreadProc(void*) {
    while (!g_hwnd && WaitForSingleObject(g_stopEvent, 10) == WAIT_TIMEOUT) {
    }

    bool quit = false;
    while (!quit && WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        const bool wanted = MouseWakeHookWanted();
        if (wanted && !g_mouseHook) {
            g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
        } else if (!wanted && g_mouseHook) {
            UnhookWindowsHookEx(g_mouseHook);
            g_mouseHook = nullptr;
        }

        // A low-level hook is delivered through the installing thread's message
        // queue, so this has to keep pumping while the hook is up.
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                quit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (quit) {
            break;
        }

        // Blocks until the stop event fires, a message arrives, or the timeout.
        // The render thread posts a WM_NULL when it parks or unparks, so the hook
        // is reconciled immediately rather than up to a tick later; the timeout
        // is only a backstop in case a transition is ever missed. This is a timed
        // wait, not a spin, so an idle mouse thread costs nothing.
        MsgWaitForMultipleObjects(1, &g_stopEvent, FALSE, 1000, QS_ALLINPUT);
    }

    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = nullptr;
    }
    return 0;
}

inline LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static POINT s_touchStart = {0, 0};
    static ULONGLONG s_touchStartTime = 0;
    switch (msg) {
        case WM_CREATE:
            AddClipboardFormatListener(hwnd);
            if (g_shellHookMessage == 0) g_shellHookMessage = RegisterWindowMessageW(L"SHELLHOOK");
            if (g_taskbarCreatedMessage == 0) g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
            RegisterShellHookWindow(hwnd);
            // File Tray (#33). Accepting drops costs nothing when the module is
            // off; WM_DROPFILES just ignores them in that case.
            DragAcceptFiles(hwnd, TRUE);
            return 0;

        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            if (!g_settings.fileTrayModule) {
                DragFinish(drop);
                return 0;
            }

            const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            std::vector<FileTrayItem> added;
            added.reserve(count);

            for (UINT i = 0; i < count; ++i) {
                wchar_t path[MAX_PATH] = {};
                if (DragQueryFileW(drop, i, path, ARRAYSIZE(path)) == 0) {
                    continue;
                }

                FileTrayItem item;
                item.path = path;

                // BaseNameFromPath instead of PathFindFileNameW so we do not
                // pull in shlwapi just for this.
                item.name = BaseNameFromPath(path);
                if (item.name.empty()) {
                    item.name = path;
                }

                WIN32_FILE_ATTRIBUTE_DATA attr = {};
                if (GetFileAttributesExW(path, GetFileExInfoStandard, &attr)) {
                    item.isDirectory = (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    item.sizeBytes = (static_cast<uint64_t>(attr.nFileSizeHigh) << 32) | attr.nFileSizeLow;
                }

                // Grab the real Explorer icon so the shelf looks native.
                SHFILEINFOW info = {};
                if (SHGetFileInfoW(path, 0, &info, sizeof(info), SHGFI_ICON | SHGFI_SMALLICON) && info.hIcon) {
                    IconToPixels(info.hIcon, 32, &item.icon);
                    DestroyIcon(info.hIcon);
                }

                added.push_back(std::move(item));
            }
            DragFinish(drop);

            if (!added.empty()) {
                std::lock_guard lock(g_stateMutex);
                for (auto& item : added) {
                    // Re-dropping a file promotes the existing entry instead of
                    // creating a duplicate.
                    auto existing = std::find_if(g_state.fileTrayItems.begin(), g_state.fileTrayItems.end(),
                                                 [&](const FileTrayItem& other) {
                                                     return _wcsicmp(other.path.c_str(), item.path.c_str()) == 0;
                                                 });
                    if (existing != g_state.fileTrayItems.end()) {
                        g_state.fileTrayItems.erase(existing);
                    }
                    g_state.fileTrayItems.push_back(std::move(item));
                }

                const size_t cap = static_cast<size_t>(std::max(1, g_settings.fileTrayMaxItems));
                while (g_state.fileTrayItems.size() > cap) {
                    g_state.fileTrayItems.erase(g_state.fileTrayItems.begin());
                }
            }

            // Jump to the shelf so the drop is visibly acknowledged.
            const int trayTab = FileTrayTabIndex(g_settings);
            if (trayTab >= 0) {
                g_idleTab = trayTab;
            }
            g_layoutDirty = true;
            return 0;
        }

        case WM_DESTROY:
            RemoveClipboardFormatListener(hwnd);
            DeregisterShellHookWindow(hwnd);
            return 0;

        // Both of these touch the window from the thread that owns it, the only
        // thread allowed to bind a hot key to it or reshape it.
        case WM_APP_APPLY_HOTKEY:
            ApplyHideShowHotkey();
            return 0;

        case WM_APP_APPLY_BACKDROP: {
            ApplyBackdropMaterial(hwnd);
            RECT rc{};
            if (GetWindowRect(hwnd, &rc)) {
                ApplyBackdropRegion(hwnd, rc.right - rc.left, rc.bottom - rc.top);
            }
            return 0;
        }

        case WM_APP_MOUSE_WAKE:
            // No-op payload — its only job is to wake MsgWaitForMultipleObjects
            // and get drained by the PeekMessage pump.
            return 0;

        case WM_APP_CAPSLOCK: {
            if (!g_settings.capsLock) return 0;
            bool isNum = (wParam == VK_NUMLOCK);
            bool capsOn = (lParam & 1) != 0;
            bool numOn = (lParam & 2) != 0;
            {
                std::lock_guard lock(g_stateMutex);
                g_state.capsLock.active = true;
                g_state.capsLock.capsOn = capsOn;
                g_state.capsLock.numOn = numOn;
                g_state.capsLock.isNumEvent = isNum;
                g_state.capsLock.expiresAt = NowSeconds() + 2.5;
            }
            TriggerNudge();
            return 0;
        }

        case WM_DEVICECHANGE: {
            // DBT_DEVICEARRIVAL = 0x8000, DBT_DEVICEREMOVECOMPLETE = 0x8004
            if (wParam == 0x8000 || wParam == 0x8004) {
                bool arrived = (wParam == 0x8000);
                std::wstring devName;
                bool isBt = false;

                if (lParam) {
                    auto* hdr = reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);
                    if (hdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                        devName = L"USB Drive";
                    } else if (hdr->dbch_devicetype == DBT_DEVTYP_PORT) {
                        devName = L"COM Device";
                    } else {
                        // Generic/Bluetooth OEM
                        isBt = true;
                        devName = L"Bluetooth Device";
                    }
                }

                {
                    std::lock_guard lock(g_stateMutex);
                    g_state.device.active = true;
                    g_state.device.eventType = arrived ? DeviceEventType::Connected
                                                       : DeviceEventType::Disconnected;
                    g_state.device.deviceName = devName;
                    g_state.device.isBluetoothLike = isBt;
                    g_state.device.expiresAt = NowSeconds() + 3.0;
                }
                TriggerNudge();
            }
            return 0;
        }

        case WM_POWERBROADCAST: {
            if (wParam == PBT_APMRESUMESUSPEND || wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMECRITICAL) {
                if (g_settingsChangedEvent) {
                    SetEvent(g_settingsChangedEvent);
                }
                {
                    std::lock_guard lock(g_stateMutex);
                    g_state.weather.lastUpdated = 0.0;
                }
                TriggerNudge();
            }
            return TRUE;
        }

        case WM_CLIPBOARDUPDATE:
            if (g_settings.clipboard) {
                CaptureClipboard(hwnd);
            }
            return 0;



        case WM_NCHITTEST: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);

            bool hit = false;
            if (PtInCapsule(g_primaryClientRect, g_primaryCornerRadius, pt)) {
                hit = true;
            } else if (g_satelliteActive.load() && PtInRect(&g_satelliteClientRect, pt)) {
                hit = true;
            }

            return hit ? HTCLIENT : HTTRANSPARENT;
        }

        case WM_APP_LAYOUT_CHANGED:
            g_layoutDirty = true;
            g_autoHiddenParked = false;
            return 0;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                if (g_scrubbing.load()) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }

                POINT ptCursor;
                GetCursorPos(&ptCursor);
                ScreenToClient(hwnd, &ptCursor);
                if (g_satelliteActive.load() && PtInRect(&g_satelliteClientRect, ptCursor)) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }

                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);

                bool mediaActive = false;
                {
                    std::lock_guard lock(g_stateMutex);
                    mediaActive = g_settings.media && g_state.media.available;
                }

                const int currentTab = NormalizedTabIndex(g_settings);
                if (mediaActive && currentTab == 0) {
                    const MediaContentPoint cp = MediaContentFromClient(pt.x, pt.y);
                    const bool hoverClickable = MediaArtHitTest(cp) ||
                                               MediaTransportHitTest(cp) != -1 ||
                                               MediaScrubFractionFromContent(cp) >= 0.0f;
                    if (hoverClickable) {
                        SetCursor(LoadCursorW(nullptr, IDC_HAND));
                        return TRUE;
                    }
                }

                // A File Tray row under the cursor is clickable (opens the file).
                if (g_settings.fileTrayModule && currentTab == FileTrayTabIndex(g_settings) &&
                    g_hoveredFileTrayRow.load(std::memory_order_relaxed) >= 0) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;

        case WM_LBUTTONDOWN:
            {
                int xPos = GET_X_LPARAM(lParam);
                int yPos = GET_Y_LPARAM(lParam);

                s_touchStart.x = xPos;
                s_touchStart.y = yPos;
                s_touchStartTime = GetTickCount64();

                bool mediaActive = false;
                {
                    std::lock_guard lock(g_stateMutex);
                    mediaActive = g_settings.media && g_state.media.available;
                }

                const int currentTab = NormalizedTabIndex(g_settings);
                if (mediaActive && currentTab == 0) {
                    const MediaContentPoint cp = MediaContentFromClient(xPos, yPos);

                    const int cmd = MediaTransportHitTest(cp);
                    if (cmd != -1) {
                        g_pressedMediaButton = cmd;
                        SetCapture(hwnd);
                        g_layoutDirty = true;
                        return 0;
                    }

                    const float fraction = MediaScrubFractionFromContent(cp);
                    if (fraction >= 0.0f) {
                        g_scrubDragFraction.store(fraction, std::memory_order_relaxed);
                        g_scrubbing = true;
                        g_lastLiveSeekTime = 0.0;
                        SetCapture(hwnd);
                        g_layoutDirty = true;
                        return 0;
                    }
                }
            }
            break;

        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme = {};
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);

            const int xPos = GET_X_LPARAM(lParam);
            const int yPos = GET_Y_LPARAM(lParam);

            if (g_scrubbing.load()) {
                const MediaContentPoint cp = MediaContentFromClient(xPos, yPos);
                const float fraction = MediaScrubFractionUnbounded(cp);
                if (fraction < 0.0f) {
                    return 0;
                }
                g_scrubDragFraction.store(fraction, std::memory_order_relaxed);
                g_layoutDirty = true;

                const double now = NowSeconds();
                if (now - g_lastLiveSeekTime.load(std::memory_order_relaxed) >= 0.15) {
                    g_lastLiveSeekTime.store(now, std::memory_order_relaxed);
                    int64_t endTicks = 0;
                    {
                        std::lock_guard lock(g_stateMutex);
                        endTicks = g_state.media.endTicks;
                    }
                    if (endTicks > 0) {
                        SeekMediaToTicks(static_cast<int64_t>(fraction * endTicks));
                    }
                }
                return 0;
            }

            int hovered = -1;
            bool mediaActive = false;
            {
                std::lock_guard lock(g_stateMutex);
                mediaActive = g_settings.media && g_state.media.available;
            }
            const int currentTab = NormalizedTabIndex(g_settings);

            if (mediaActive && currentTab == 0) {
                hovered = MediaTransportHitTest(MediaContentFromClient(xPos, yPos));
            }

            if (g_hoveredMediaButton.exchange(hovered) != hovered) {
                g_layoutDirty = true;
            }

            // File Tray row highlight.
            int hoveredRow = -1;
            if (g_settings.fileTrayModule && currentTab == FileTrayTabIndex(g_settings)) {
                size_t trayCount = 0;
                {
                    std::lock_guard lock(g_stateMutex);
                    trayCount = g_state.fileTrayItems.size();
                }
                hoveredRow = FileTrayRowAtContentPoint(MediaContentFromClient(xPos, yPos),
                                                       static_cast<int>(trayCount));
            }
            if (g_hoveredFileTrayRow.exchange(hoveredRow) != hoveredRow) {
                g_layoutDirty = true;
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (g_hoveredMediaButton.exchange(-1) != -1) {
                g_layoutDirty = true;
            }
            return 0;

        case WM_CAPTURECHANGED:
            if (reinterpret_cast<HWND>(lParam) != hwnd) {
                if (g_scrubbing.exchange(false)) {
                    g_layoutDirty = true;
                }
                if (g_pressedMediaButton.exchange(-1) != -1) {
                    g_layoutDirty = true;
                }
                if (g_hoveredMediaButton.exchange(-1) != -1) {
                    g_layoutDirty = true;
                }
            }
            return 0;

        case WM_LBUTTONUP:
            {
                if (g_scrubbing.load()) {
                    g_scrubbing = false;
                    ReleaseCapture();

                    const float finalFraction = Clamp(g_scrubDragFraction.load(std::memory_order_relaxed), 0.0f, 1.0f);
                    int64_t endTicks = 0;
                    {
                        std::lock_guard lock(g_stateMutex);
                        endTicks = g_state.media.endTicks;
                    }
                    if (endTicks > 0) {
                        const int64_t targetTicks = static_cast<int64_t>(finalFraction * endTicks);
                        SeekMediaToTicks(targetTicks);
                        std::lock_guard lock(g_stateMutex);
                        g_state.media.positionTicks = targetTicks;
                        g_state.media.lastUpdatedTicks = GetTickCount64();
                    }
                    g_layoutDirty = true;
                    return 0;
                }

                if (g_pressedMediaButton.load() != -1) {
                    g_pressedMediaButton = -1;
                    ReleaseCapture();
                    g_layoutDirty = true;
                }

                int xPos = GET_X_LPARAM(lParam);
                int yPos = GET_Y_LPARAM(lParam);
                POINT ptClient = { xPos, yPos };

                // Detached Split-Island Satellite click routing:
                if (g_satelliteActive.load() && PtInRect(&g_satelliteClientRect, ptClient)) {
                    if (g_satelliteExpanded.load()) {
                        D2D1_POINT_2F d2dPt = D2D1::Point2F(static_cast<float>(xPos), static_cast<float>(yPos));
                        if (g_agyTelemetry.HandleMouseClick(d2dPt)) {
                            g_layoutDirty = true;
                            return 0;
                        }
                        FocusAntigravityWindow();
                        return 0;
                    }
                    if (!g_settings.expandOnHover) {
                        g_satelliteClickExpanded.store(true);
                        g_layoutDirty = true;
                    } else {
                        FocusAntigravityWindow();
                    }
                    return 0;
                }

                ULONGLONG now = GetTickCount64();
                if (s_touchStartTime > 0 && (now - s_touchStartTime) < 500) {
                    int dx = xPos - s_touchStart.x;
                    if (abs(dx) > 40) { // Horizontal swipe threshold
                        const int maxTabs = ActiveTabCount(g_settings);
                        if (maxTabs > 1) {
                            if (dx > 0) { // Swipe right -> previous tab
                                g_idleTab = (g_idleTab - 1 + maxTabs) % maxTabs;
                            } else { // Swipe left -> next tab
                                g_idleTab = (g_idleTab + 1) % maxTabs;
                            }
                            g_layoutDirty = true;
                        }
                        s_touchStartTime = 0;
                        return 0; // Consume swipe gesture
                    }
                }
                s_touchStartTime = 0;

                bool mediaActive = false;
                std::vector<IslandKind> kinds;
                {
                    std::lock_guard lock(g_stateMutex);
                    mediaActive = g_settings.media && g_state.media.available;
                    kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                }
                const bool gameMetricsPresent =
                    !kinds.empty() && kinds[0] == IslandKind::Idle &&
                    (g_settings.gameOverlay || Wh_GetIntValue(L"GameOverlayPinned", 0) != 0);

                bool expanded = Wh_GetIntValue(L"PinnedExpanded", 0) != 0 || g_clickExpanded.load();
                if (!gameMetricsPresent && !g_settings.expandOnHover && !expanded) {
                    g_clickExpanded = true;
                    g_layoutDirty = true;
                    return 0; // consumed click to expand
                }

                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                const float height = static_cast<float>(clientRect.bottom - clientRect.top);

                const int currentTab = NormalizedTabIndex(g_settings);

                const MediaContentPoint cp = MediaContentFromClient(xPos, yPos);

                // File Tray: clicking a row opens that file with its default app.
                if (g_settings.fileTrayModule && currentTab == FileTrayTabIndex(g_settings)) {
                    std::wstring toOpen;
                    {
                        std::lock_guard lock(g_stateMutex);
                        const int row = FileTrayRowAtContentPoint(
                            cp, static_cast<int>(g_state.fileTrayItems.size()));
                        if (row >= 0) {
                            // Rows render newest-first, so map back from the end.
                            const size_t index = g_state.fileTrayItems.size() - 1 - static_cast<size_t>(row);
                            toOpen = g_state.fileTrayItems[index].path;
                        }
                    }
                    if (!toOpen.empty()) {
                        SHELLEXECUTEINFOW sei = {sizeof(sei)};
                        sei.fMask = SEE_MASK_FLAG_NO_UI;
                        sei.lpFile = toOpen.c_str();
                        sei.nShow = SW_SHOWNORMAL;
                        ShellExecuteExW(&sei);
                        return 0;
                    }
                    // A click on the empty shelf should not fall through to
                    // "focus the media app".
                    return 0;
                }

                if (mediaActive && currentTab == 0) {
                    const int cmd = MediaTransportHitTest(cp);
                    if (cmd != -1) {
                        SendMediaTransportCommand(cmd);
                        return 0;
                    }

                    // A release over the scrubber that never went through the
                    // drag path (press started elsewhere) is swallowed rather
                    // than falling through to "open the app". The actual seek
                    // is handled by the g_scrubbing branch above.
                    if (MediaScrubFractionFromContent(cp) >= 0.0f) {
                        return 0;
                    }
                }

                if (height > 50.0f) {
                    if (mediaActive) {
                        if (currentTab == 0) {
                            OpenRelevantApp();
                        }
                    } else if (kinds.empty() || kinds[0] != IslandKind::Idle) {
                        HandleStatusClickAtPoint(hwnd, lParam);
                    }
                } else {
                    if (mediaActive) {
                        OpenRelevantApp();
                    } else {
                        HandleStatusClickAtPoint(hwnd, lParam);
                    }
                }
            }
            return 0;

        case WM_MBUTTONUP:
            ToggleEndpointMute();
            return 0;

        case WM_LBUTTONDBLCLK:
            Wh_SetIntValue(L"PinnedExpanded", Wh_GetIntValue(L"PinnedExpanded", 0) ? 0 : 1);
            return 0;

        case WM_MOUSEWHEEL: {
            static ULONGLONG lastScrollTime = 0;
            ULONGLONG now = GetTickCount64();
            if (now - lastScrollTime < 150) return 0; // 150ms debounce
            lastScrollTime = now;

            POINT ptWheel = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &ptWheel);

            // Satellite mouse wheel session cycle
            if (g_satelliteActive.load() && PtInRect(&g_satelliteClientRect, ptWheel)) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta > 0) {
                    g_agyTelemetry.CyclePrevSession();
                } else if (delta < 0) {
                    g_agyTelemetry.CycleNextSession();
                }
                g_layoutDirty = true;
                return 0;
            }

            const int maxTabs = ActiveTabCount(g_settings);
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (delta > 0) {
                if (g_idleTab > 0) g_idleTab--;
            } else if (delta < 0) {
                if (g_idleTab < maxTabs - 1) g_idleTab++;
            }

            g_layoutDirty = true;
            return 0;
        }

        case WM_RBUTTONUP: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(hwnd, &pt);
            ShowContextMenu(hwnd, pt);
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == ID_HIDE_SHOW_HOTKEY) {
                if (g_isFullscreen.load(std::memory_order_relaxed)) {
                    g_fullscreenOverrideVisible = !g_fullscreenOverrideVisible.load();
                    g_autoHiddenParked = false;
                    g_layoutDirty = true;
                    if (g_fullscreenOverrideVisible.load()) {
                        g_manuallyHidden = false;
                        Wh_SetIntValue(L"ManuallyHidden", 0);
                        g_hotkeyUnhideUntil.store(NowSeconds() + (g_settings.autoHideIdleSeconds > 0 ? g_settings.autoHideIdleSeconds : 6.0));
                        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                        PostMessageW(hwnd, WM_APP_NEW_EVENT, 0, 0);
                    }
                } else {
                    bool isCurrentlyHidden = g_manuallyHidden.load() || g_autoHiddenParked.load();
                    if (hwnd && !IsWindowVisible(hwnd)) {
                        isCurrentlyHidden = true;
                    }
                    if (isCurrentlyHidden) {
                        // User wants to reveal / unhide
                        g_manuallyHidden = false;
                        Wh_SetIntValue(L"ManuallyHidden", 0);
                        g_autoHiddenParked = false;
                        g_fullscreenOverrideVisible = true;
                        g_hotkeyUnhideUntil.store(NowSeconds() + (g_settings.autoHideIdleSeconds > 0 ? g_settings.autoHideIdleSeconds : 6.0));
                        g_layoutDirty = true;
                        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                        PostMessageW(hwnd, WM_APP_NEW_EVENT, 0, 0);
                    } else {
                        // User wants to manually hide
                        g_manuallyHidden = true;
                        Wh_SetIntValue(L"ManuallyHidden", 1);
                        g_hotkeyUnhideUntil.store(0.0);
                        g_fullscreenOverrideVisible = false;
                        g_layoutDirty = true;
                        PostMessageW(hwnd, WM_APP_NEW_EVENT, 0, 0);
                    }
                }
            }
            return 0;
        }
    }

    if (msg == g_shellHookMessage && g_shellHookMessage != 0) {
        if (wParam == HSHELL_WINDOWCREATED) {
            CaptureShellNotification(reinterpret_cast<HWND>(lParam));
        }
        return 0;
    }

    if (msg == g_taskbarCreatedMessage && g_taskbarCreatedMessage != 0) {
        Wh_Log(L"TaskbarCreated received; re-registering shell hook window.");
        DeregisterShellHookWindow(hwnd);
        RegisterShellHookWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline DWORD WINAPI RenderThreadProc(void*) {
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        kWindowClass, L"Dynamic Island for Windows", WS_POPUP, 0, 0, 520, 140,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!hwnd) {
        Wh_Log(L"Failed to create Dynamic Island overlay window.");
        if (SUCCEEDED(hrCo)) {
            CoUninitialize();
        }
        return 0;
    }

    g_hwnd = hwnd;
    g_agyTelemetry.SetNotificationHwnd(hwnd);
    if (g_shellHookMessage == 0) g_shellHookMessage = RegisterWindowMessageW(L"SHELLHOOK");
    if (g_taskbarCreatedMessage == 0) g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    using ChangeWindowMessageFilterEx_t = BOOL(WINAPI*)(HWND, UINT, DWORD, PVOID);
    static auto pChangeWindowMessageFilterEx = reinterpret_cast<ChangeWindowMessageFilterEx_t>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "ChangeWindowMessageFilterEx"));
    if (pChangeWindowMessageFilterEx) {
        if (g_shellHookMessage) pChangeWindowMessageFilterEx(hwnd, g_shellHookMessage, 1 /*MSGFLT_ALLOW*/, nullptr);
        if (g_taskbarCreatedMessage) pChangeWindowMessageFilterEx(hwnd, g_taskbarCreatedMessage, 1 /*MSGFLT_ALLOW*/, nullptr);
        pChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, 1 /*MSGFLT_ALLOW*/, nullptr);
        pChangeWindowMessageFilterEx(hwnd, 0x0049 /*WM_COPYGLOBALDATA*/, 1 /*MSGFLT_ALLOW*/, nullptr);
    } else {
        using ChangeWindowMessageFilter_t = BOOL(WINAPI*)(UINT, DWORD);
        static auto pChangeWindowMessageFilter = reinterpret_cast<ChangeWindowMessageFilter_t>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "ChangeWindowMessageFilter"));
        if (pChangeWindowMessageFilter) {
            if (g_shellHookMessage) pChangeWindowMessageFilter(g_shellHookMessage, 1 /*MSGFLT_ADD*/);
            if (g_taskbarCreatedMessage) pChangeWindowMessageFilter(g_taskbarCreatedMessage, 1 /*MSGFLT_ADD*/);
            pChangeWindowMessageFilter(WM_COPYDATA, 1 /*MSGFLT_ADD*/);
            pChangeWindowMessageFilter(0x0049 /*WM_COPYGLOBALDATA*/, 1 /*MSGFLT_ADD*/);
        }
    }
    EnableBlurBehind(hwnd);
    ApplyBackdropMaterial(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    ApplyHideShowHotkey();

    if (g_settings.autoHideIdleSeconds == 0) {
        g_manuallyHidden = false;
        Wh_SetIntValue(L"ManuallyHidden", 0);
    } else {
        g_manuallyHidden = Wh_GetIntValue(L"ManuallyHidden", 0) != 0;
    }
    if (g_manuallyHidden.load()) {
        ShowWindow(hwnd, SW_HIDE);
    }

    Renderer renderer;
    if (!renderer.Initialize(hwnd)) {
        Wh_Log(L"Renderer::Initialize failed; destroying overlay window.");
        DestroyWindow(hwnd);
        g_hwnd = nullptr;
        if (SUCCEEDED(hrCo)) {
            CoUninitialize();
        }
        return 0;
    }

    using TimeBeginPeriod_t = MMRESULT(WINAPI*)(UINT);
    using TimeEndPeriod_t = MMRESULT(WINAPI*)(UINT);
    static auto pTimeBeginPeriod = reinterpret_cast<TimeBeginPeriod_t>(
        GetProcAddress(LoadLibraryW(L"winmm.dll"), "timeBeginPeriod"));
    static auto pTimeEndPeriod = reinterpret_cast<TimeEndPeriod_t>(
        GetProcAddress(GetModuleHandleW(L"winmm.dll"), "timeEndPeriod"));

    // A 1ms timer resolution costs power, so it is requested only while the island
    // is actually animating. It used to be requested once here and released at
    // shutdown, so a parked or idle island held it for the mod's whole lifetime.
    //
    // Since Windows 10 2004 this affects only the calling process's timers rather
    // than the system clock globally, but the power cost is the reason to scope it
    // either way.
    //
    // The flag is only set when timeBeginPeriod actually succeeded, so the
    // begin/end pairs stay balanced -- these calls are reference counted per
    // process, and an unmatched timeEndPeriod would decrement someone else's
    // request.
    bool highResTimer = false;

    // How long the resolution is held after animation stops. Long enough to ride
    // out the gaps between 60Hz content frames and brief pauses between spring
    // animations without flapping, short enough that a settled island gives it back
    // promptly. A plain local rather than a static, so an unload/reload cycle cannot
    // carry a stale timestamp across.
    constexpr double kHighResTimerHoldSec = 0.5;
    double lastAnimatingAt = -1.0;

    auto setHighResTimer = [&](bool want) {
        if (want == highResTimer) {
            return;
        }
        if (want) {
            if (pTimeBeginPeriod && pTimeBeginPeriod(1) == TIMERR_NOERROR) {
                highResTimer = true;
            }
        } else {
            if (pTimeEndPeriod) {
                pTimeEndPeriod(1);
            }
            highResTimer = false;
        }
    };

    SpringValue widthSpring;
    SpringValue heightSpring;
    SpringValue nudgeSpring;
    SpringValue satWidthSpring;
    SpringValue satHeightSpring;
    widthSpring.Reset((g_settings.autoHideIdleSeconds == -1 ? 0.0f : 120.0f) * g_settings.sizeScale);
    heightSpring.Reset((g_settings.autoHideIdleSeconds == -1 ? 0.0f : 36.0f) * g_settings.sizeScale);
    nudgeSpring.Reset(0.0f);

    IslandKind previousPrimary = IslandKind::Idle;
    auto previousFrame = std::chrono::steady_clock::now();
    auto nextFrameTarget = previousFrame;
    double nextBatteryPoll = 0.0;
    double nextProgressPoll = 0.0;
    double nextSystemPoll = 0.0;
    double nextPrivacyPoll = 0.0;
    bool wasManuallyHidden = false;
    bool wasAutoHiddenParked = false;
    bool parkedForFullscreen = false;

    static double lastInteractionTime = NowSeconds();
    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_APP_NEW_EVENT) {
                nudgeSpring.value = -6.0f;
                nudgeSpring.velocity = 0.0f;
                nudgeSpring.target = 0.0f;
                lastInteractionTime = NowSeconds();
                continue;
            }
            if (message.message == WM_APP_LAYOUT_CHANGED) {
                lastInteractionTime = NowSeconds();
                g_layoutDirty = true;
                g_autoHiddenParked = false;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        bool justUnhidden = false;

        if (g_manuallyHidden.load()) {
            // Manual hide always takes precedence over — and invalidates —
            // any auto-park bookkeeping, since the window's shown/hidden
            // state is now fully owned by the manual toggle. Without this,
            // toggling manual-hide off after having been auto-parked would
            // leave g_autoHiddenParked stale-true and the window stuck
            // hidden.
            g_autoHiddenParked = false;
            wasAutoHiddenParked = false;

            if (!wasManuallyHidden) {
                // Just hid: drop to zero-CPU parking immediately, no
                // lingering render/animation work this frame.
                ShowWindow(hwnd, SW_HIDE);
                g_audioCaptureNeeded.store(false, std::memory_order_relaxed);
                wasManuallyHidden = true;
            }
            previousFrame = std::chrono::steady_clock::now();
            // Fully parked: no polling, no timer wakeups — only the stop
            // event or a posted/queued message (hotkey, settings change,
            // clipboard update, etc.) wakes this thread while hidden. Nothing
            // is being paced, so give the system timer resolution back.
            setHighResTimer(false);
            MsgWaitForMultipleObjects(1, &g_stopEvent, FALSE, INFINITE, QS_ALLINPUT);
            nextFrameTarget = std::chrono::steady_clock::now();
            continue;
        }

        // Sits ahead of the parked branch below on purpose: that branch can block
        // for up to 1.5s, and the mouse thread needs to hear about a transition
        // before that, not after.
        {
            static bool prevParkedForHook = false;
            const bool parkedNow = g_autoHiddenParked.load(std::memory_order_relaxed);
            if (parkedNow != prevParkedForHook) {
                prevParkedForHook = parkedNow;
                NotifyMouseThreadParkedChanged();
            }
        }

        if (g_autoHiddenParked.load()) {
            if (g_settings.autoHideIdleSeconds == 0 && !parkedForFullscreen) {
                g_autoHiddenParked = false;
            } else {
                previousFrame = std::chrono::steady_clock::now();
                // Auto-parked, so nothing is animating; same reasoning as above.
                setHighResTimer(false);
                if (parkedForFullscreen) {
                    MsgWaitForMultipleObjects(1, &g_stopEvent, FALSE, 1500, QS_ALLINPUT);
                    const bool stillFullscreen =
                        g_settings.autoHideFullscreen && IsForegroundFullscreen(hwnd);
                    g_isFullscreen.store(stillFullscreen, std::memory_order_relaxed);
                    if (!stillFullscreen) {
                        g_fullscreenOverrideVisible = false;
                        g_autoHiddenParked = false;
                    } else if (g_fullscreenOverrideVisible.load()) {
                        g_autoHiddenParked = false;
                    }
                } else {
                    MsgWaitForMultipleObjects(1, &g_stopEvent, FALSE, INFINITE, QS_ALLINPUT);
                }
                nextFrameTarget = std::chrono::steady_clock::now();
                continue;
            }
        }

        if (wasManuallyHidden) {
            // Just un-hid. Show now; springs get snapped straight to their
            // freshly-computed targets further down this same iteration so
            // there's no stale pop-in animation from wherever they were
            // left off before hiding.
            wasManuallyHidden = false;
            justUnhidden = true;
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            lastInteractionTime = NowSeconds();
            g_layoutDirty = true;
            previousFrame = std::chrono::steady_clock::now();
            nextFrameTarget = previousFrame;
        }

        if (wasAutoHiddenParked && !g_autoHiddenParked.load()) {
            // Something woke us (mouse near the island, a transient alert,
            // the hotkey, fullscreen ending, etc.) — reveal and let this
            // frame's normal logic decide whether to actually stay visible
            // or immediately re-collapse and re-park.
            wasAutoHiddenParked = false;
            justUnhidden = true;
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            lastInteractionTime = NowSeconds();
            g_layoutDirty = true;
            previousFrame = std::chrono::steady_clock::now();
            nextFrameTarget = previousFrame;
        }

        const double now = NowSeconds();
        if (now >= nextBatteryPoll) {
            UpdateBatterySnapshot();
            nextBatteryPoll = now + 15.0;
        }
        if (now >= nextProgressPoll) {
            UpdateProgressSnapshot();
            nextProgressPoll = now + 0.25;
        }
        if (now >= nextPrivacyPoll) {
            UpdatePrivacyIndicators();
            nextPrivacyPoll = now + 2.0;  // poll every 2 s
        }

        bool timerJustCompleted = false;
        SharedState snapshot;
        {
            std::lock_guard lock(g_stateMutex);
            snapshot = g_state;
            if (g_state.timer.active && g_state.timer.running && now >= g_state.timer.endsAt) {
                g_state.timer.active = false;
                g_state.timer.running = false;
                g_state.timer.justFinished = true;
                g_state.timer.finishedExpiresAt = now + 6.0;
                timerJustCompleted = true;
            }
            if (g_state.timer.justFinished && now >= g_state.timer.finishedExpiresAt) {
                g_state.timer.justFinished = false;
            }
            snapshot.timer = g_state.timer;
            if (g_state.clipboard.active && now >= g_state.clipboard.expiresAt) {
                g_state.clipboard.active = false;
                snapshot.clipboard.active = false;
            }
            if (g_state.notification.active && now >= g_state.notification.expiresAt) {
                g_state.notification.active = false;
                snapshot.notification.active = false;
            }
            if (g_state.volume.active && now >= g_state.volume.expiresAt) {
                g_state.volume.active = false;
                snapshot.volume.active = false;
            }
            if (g_state.capsLock.active && now >= g_state.capsLock.expiresAt) {
                g_state.capsLock.active = false;
                snapshot.capsLock.active = false;
            }
            if (g_state.battery.active && now >= g_state.battery.expiresAt) {
                g_state.battery.active = false;
                snapshot.battery.active = false;
            }
            if (g_state.device.active && now >= g_state.device.expiresAt) {
                g_state.device.active = false;
                snapshot.device.active = false;
            }
        }
            if (timerJustCompleted) {
                TriggerNudge();
        }

        const std::vector<IslandKind> kinds = ChooseActivities(snapshot, g_settings, now);
        Activity primary = ActivityForKind(kinds[0], g_settings, snapshot);
        std::optional<Activity> secondary;
        if (kinds.size() >= 2) {
            secondary = ActivityForKind(kinds[1], g_settings, snapshot);
        }

        const bool pinned = Wh_GetIntValue(L"PinnedExpanded", 0) != 0;

        if (primary.kind != previousPrimary) {
            if (primary.kind != IslandKind::Idle) {
                nudgeSpring.value = -6.0f;
                nudgeSpring.velocity = 0.0f;
                nudgeSpring.target = 0.0f;
            }
            lastInteractionTime = now;
        }
        previousPrimary = primary.kind;

        static bool isFullscreen = false;
        static double lastFullscreenCheck = -1.0;  // -1.0 guarantees the very first iteration checks
        if (now - lastFullscreenCheck > 0.5) {
            const bool newFullscreen = g_settings.autoHideFullscreen && IsForegroundFullscreen(hwnd);
            if (isFullscreen && !newFullscreen) {
                // Fullscreen ended — re-arm so the next fullscreen session
                // hides again even if the hotkey was used to reveal the
                // island this time.
                g_fullscreenOverrideVisible = false;
            }
            isFullscreen = newFullscreen;
            g_isFullscreen.store(isFullscreen, std::memory_order_relaxed);
            lastFullscreenCheck = now;
        }

        static bool isMaximized = false;
        static double lastMaximizedCheck = -1.0;
        if (now - lastMaximizedCheck > 0.5) {
            isMaximized = g_settings.autoHideMaximized && IsForegroundMaximized(hwnd);
            lastMaximizedCheck = now;
        }

        RECT windowRect = {};
        GetWindowRect(hwnd, &windowRect);
        POINT cursor = {};
        GetCursorPos(&cursor);

        bool hover = false;
        if (widthSpring.value > 1.0f && heightSpring.value > 1.0f) {
            const float topPad = (g_settings.notchStyle || g_settings.borderMergedMode) ? 0.0f : kRenderPadY;
            RECT pillRect = {
                windowRect.left + static_cast<int>(std::round(kRenderPadX)),
                windowRect.top + static_cast<int>(std::round(topPad + nudgeSpring.value)),
                windowRect.left + static_cast<int>(std::round(kRenderPadX + widthSpring.value)),
                windowRect.top + static_cast<int>(std::round(topPad + nudgeSpring.value + heightSpring.value))
            };
            if (isMaximized) {
                pillRect.bottom = std::max(pillRect.bottom, windowRect.top + static_cast<int>(std::round(topPad + 14.0f * g_settings.sizeScale)));
            }
            hover = PtInRect(&pillRect, cursor) != FALSE;
        } else if (g_settings.unhideOnHover) {
            RECT dockRect = GetIslandDockRect();
            hover = PtInRect(&dockRect, cursor) != FALSE;
        }

        // 250ms Hover Dwell Gate: filters accidental cursor sweeps across top bezel
        static double hoverStartTime = 0.0;
        static bool isHoverActive = false;
        if (hover) {
            if (hoverStartTime == 0.0) {
                hoverStartTime = now;
            } else if (now - hoverStartTime >= 0.250) { // 250ms dwell gate
                isHoverActive = true;
            }
        } else {
            hoverStartTime = 0.0;
            isHoverActive = false;
        }

        bool needsRender = false;

        if (!hover && g_clickExpanded.load()) {
            g_clickExpanded = false;
            needsRender = true;
        }
        if (!hover && g_hoveredMediaButton.load() != -1) {
            g_hoveredMediaButton = -1;
            needsRender = true;
        }
        // Fixes windhawk-mods#4738: active playback used to count as a continuous
        // event, so the island stayed visible for as long as anything was playing
        // and kept resetting the auto-hide timer. Only the 5s window after a
        // *title* change is transient now, so the pill behaves like the clipboard
        // and battery alerts -- it surfaces, then hides again while playback
        // continues in the background.
        const bool recentTrackChange = g_settings.mediaAutoExpand &&
                                       !MediaExpandBlocked(snapshot.media) &&
                                       primary.kind == IslandKind::Media &&
                                       snapshot.media.playing &&
                                       !snapshot.media.title.empty() &&
                                       (now - snapshot.media.titleChangedAt < 5.0);

        bool isTransientAlert = (primary.kind == IslandKind::Clipboard ||
                                 primary.kind == IslandKind::Notification ||
                                 primary.kind == IslandKind::Volume ||
                                 primary.kind == IslandKind::BatteryLow ||
                                 primary.kind == IslandKind::CapsLock ||
                                 primary.kind == IslandKind::Device ||
                                 primary.kind == IslandKind::Bluetooth ||
                                 primary.kind == IslandKind::DoNotDisturb ||
                                 recentTrackChange);

        const bool unhideGraceActive = (now < g_hotkeyUnhideUntil.load());
        const bool hoverUnhides = g_settings.unhideOnHover && (isHoverActive || hover);

        bool currentlyHidden = false;
        if (!unhideGraceActive) {
            if (g_settings.autoHideIdleSeconds == -1 && !isTransientAlert && !pinned) {
                currentlyHidden = true;
            } else if (g_settings.autoHideIdleSeconds > 0) {
                currentlyHidden = (now - lastInteractionTime > g_settings.autoHideIdleSeconds);
            }
        }

        bool isHoverExpanded = g_settings.expandOnHover ? isHoverActive : (hover && g_clickExpanded.load());
        const bool microNotchActive = isMaximized && !isFullscreen && !pinned && !isHoverActive && !isTransientAlert && !unhideGraceActive;
        const bool gameMetricsPresent = primary.kind == IslandKind::Idle &&
            (g_settings.gameOverlay || Wh_GetIntValue(L"GameOverlayPinned", 0) != 0);
        if (gameMetricsPresent) {
            isHoverExpanded = false;
        }

        if (currentlyHidden && !g_settings.unhideOnHover) {
            isHoverExpanded = false;
        } else if (isHoverExpanded || hoverUnhides || pinned || isTransientAlert || unhideGraceActive) {
            lastInteractionTime = now;
        }

        bool isHidden = false;
        if (!unhideGraceActive) {
            if (g_settings.autoHideIdleSeconds == -1 && !isTransientAlert && !isHoverExpanded && !hoverUnhides && !pinned) {
                isHidden = true;
            } else if (g_settings.autoHideIdleSeconds > 0) {
                if (hoverUnhides) {
                    isHidden = false;
                } else {
                    isHidden = (now - lastInteractionTime > g_settings.autoHideIdleSeconds);
                }
            }
        }



        // Reclaim the top of the z-order if another always-on-top window has
        // been raised over the island. Cheap and rate-limited, and skipped
        // while the island is hidden or suppressed so it cannot un-hide itself.
        static double lastTopmostCheck = 0.0;
        if (!isFullscreen && !g_manuallyHidden.load() && !g_autoHiddenParked.load() &&
            now - lastTopmostCheck > 1.0) {
            EnsureTopmost(hwnd);
            lastTopmostCheck = now;
        }

        const bool micIndicatorActive = snapshot.system.micActive && g_settings.privacyDots && g_settings.privacyDotsMic;
        const bool camIndicatorActive = snapshot.system.cameraActive && g_settings.privacyDots && g_settings.privacyDotsCam;
        const bool privacyActive = micIndicatorActive || camIndicatorActive;

        // Whether any surface that actually draws CPU / RAM / disk / GPU / network
        // figures is on screen: the in-game overlay, or the idle dashboard's
        // Hardware Monitor tab while expanded (hovered or pinned) and scrolled into
        // view.
        //
        // Hoisted out of the poll block below because two decisions need it: whether
        // to sample the expensive GPU and network counters at all, and whether a
        // change in those numbers is worth repainting for.
        const int metricTabIdx = NormalizedTabIndex(g_settings);
        const bool onHardwareMonitorTab = (metricTabIdx == HardwareMonitorTabIndex(g_settings));
        const bool hwMonitorVisible = (primary.kind == IslandKind::Idle || primary.kind == IslandKind::Media) &&
            !isFullscreen && !gameMetricsPresent && (pinned || isHoverExpanded) && onHardwareMonitorTab;
        const bool gameOverlayVisible = gameMetricsPresent && !isFullscreen;
        const bool systemMetricsVisible = hwMonitorVisible || gameOverlayVisible;

        if (now >= nextSystemPoll) {
            const bool needGpuStats = gameOverlayVisible || hwMonitorVisible;
            const bool needNetStats = hwMonitorVisible;  // net is only ever drawn in the HW dashboard

            UpdateSystemSnapshot(needGpuStats, needNetStats);
            nextSystemPoll = now + 1.0;
        }

        if (primary.kind == IslandKind::Idle) {
            if (!isFullscreen && (pinned || isHoverExpanded)) {
                primary.width = MediaLayout::kExpandedWidth * g_settings.sizeScale;
                primary.height = MediaLayout::kExpandedHeight * g_settings.sizeScale;
            } else if (microNotchActive) {
                primary.width = 120.0f * g_settings.sizeScale;
                primary.height = 6.0f * g_settings.sizeScale;
            } else if (primary.width > 0.0f) {
                // Collapsed: size the strip to the text it will actually render
                // (windhawk-mods#5086). ActivityForKind's 96/170px was wrong both
                // ways -- dead air around a short "9:41", and clipping on
                // "10:41:32 PM" at larger Text size.
                // The > 0 guard preserves ActivityForKind's fully-hidden case.
                primary.width =
                    renderer.MeasureIdleStrip(snapshot, g_settings, now).totalWidth *
                    g_settings.sizeScale;
            }
        } else if (microNotchActive) {
            primary.width = 120.0f * g_settings.sizeScale;
            primary.height = 6.0f * g_settings.sizeScale;
            secondary.reset();
        }
        if (!isFullscreen && primary.kind == IslandKind::Idle &&
            (g_settings.gameOverlay || Wh_GetIntValue(L"GameOverlayPinned", 0) != 0)) {
            // Width follows the metrics actually enabled (#25) so switching some
            // off shrinks the strip instead of leaving empty space, and compact
            // mode narrows it enough to sit neatly in the taskbar area. Both the
            // size and the painting come from GameOverlayLayout, so the strip
            // cannot end up sized for a card width DrawGameOverlay no longer uses.
            const int metricCount = (g_settings.gameOverlayShowCpu ? 1 : 0) +
                                    (g_settings.gameOverlayShowRam ? 1 : 0) +
                                    (g_settings.gameOverlayShowGpu ? 1 : 0) +
                                    (g_settings.gameOverlayShowDisk ? 1 : 0);

            const bool compact = g_settings.gameOverlayCompact;
            primary.width = GameOverlayLayout::Width(compact, g_settings.gameOverlayShowFps,
                                                     metricCount) * g_settings.sizeScale;
            primary.height = GameOverlayLayout::For(compact).height * g_settings.sizeScale;
        }
        if (primary.kind == IslandKind::Media) {
            if (!isFullscreen && (isHoverExpanded || pinned || recentTrackChange)) {
                primary.width = MediaLayout::kExpandedWidth * g_settings.sizeScale;
                primary.height = MediaLayout::kExpandedHeight * g_settings.sizeScale;
            }
        }

        // --- Detached Satellite Pill Telemetry & Spring Targets ---
        if (g_agyTelemetry.CheckForUpdates(now)) {
            needsRender = true;
        }
        const bool agyActive = g_agyTelemetry.IsActive();

        POINT cursorClient = cursor;
        ScreenToClient(hwnd, &cursorClient);
        bool satHover = false;
        if (g_satelliteActive.load() && satWidthSpring.value > 10.0f) {
            satHover = PtInRect(&g_satelliteClientRect, cursorClient) != FALSE;
        }
        bool satExpanded = false;
        if (g_settings.expandOnHover) {
            satExpanded = satHover;
        } else {
            if (g_satelliteClickExpanded.load()) {
                if (satHover) {
                    satExpanded = true;
                } else {
                    g_satelliteClickExpanded.store(false);
                }
            }
        }
        if (pinned) {
            satExpanded = true;
        }
        g_satelliteExpanded = satExpanded;

        float satTargetWidth = 0.0f;
        float satTargetHeight = 0.0f;
        if (agyActive && !isFullscreen && !g_manuallyHidden.load()) {
            if (satExpanded) {
                // Expanded Dashboard Mode: full multi-session bento card
                satTargetWidth = 380.0f * g_settings.sizeScale;
                satTargetHeight = g_agyTelemetry.GetExpandedHeight(g_settings.sizeScale);
            } else {
                // Collapsed Notch Modes (Minimized when idle, Normal when working):
                // Clean compact notch silhouette with zero crowded text.
                satTargetWidth = 46.0f * g_settings.sizeScale;
                satTargetHeight = primary.height > 1.0f ? primary.height : (36.0f * g_settings.sizeScale);
            }
        }
        satWidthSpring.target = satTargetWidth;
        satHeightSpring.target = satTargetHeight;

        const bool fullscreenSuppressed =
            isFullscreen && !g_fullscreenOverrideVisible.load(std::memory_order_relaxed);
        if ((isHidden || fullscreenSuppressed) && !privacyActive && !pinned && !isHoverExpanded && !isTransientAlert) {
            primary.width = 0.0f;
            primary.height = 0.0f;
            secondary.reset();
        }

        const bool mediaWaveformVisible =
            g_settings.media && snapshot.media.playing &&
            ((primary.kind == IslandKind::Media && primary.width > 1.0f && primary.height > 1.0f) ||
             (secondary && secondary->kind == IslandKind::Media && secondary->width > 1.0f && secondary->height > 1.0f));
        g_audioCaptureNeeded.store(mediaWaveformVisible, std::memory_order_relaxed);

        float targetWidth = primary.width;
        float targetHeight = primary.height;
        if (secondary) {
            targetWidth = primary.width + secondary->width + 12.0f * g_settings.sizeScale;
            targetHeight = std::max(primary.height, secondary->height);
        }

        widthSpring.target = targetWidth;
        heightSpring.target = targetHeight;

        if (justUnhidden || (unhideGraceActive && widthSpring.value < 0.5f && targetWidth > 1.0f)) {
            widthSpring.Reset(targetWidth);
            heightSpring.Reset(targetHeight);
            nudgeSpring.Reset(0.0f);
        }

        const auto currentFrame = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(currentFrame - previousFrame).count();
        previousFrame = currentFrame;
        dt = Clamp(dt, 0.001f, 0.050f);

        float styleStiffnessMult = 1.0f;
        float styleDampingMult = 1.0f;
        if (g_settings.animationStyle == AnimationStyle::Smooth) {
            styleStiffnessMult = 1.0f;
            styleDampingMult = 1.35f; // Critically damped, no bounciness
        } else if (g_settings.animationStyle == AnimationStyle::Bouncy) {
            styleStiffnessMult = 1.1f;
            styleDampingMult = 0.70f; // Underdamped, lively elasticity
        } else if (g_settings.animationStyle == AnimationStyle::Snappy) {
            styleStiffnessMult = 1.5f;
            styleDampingMult = 1.25f; // High stiffness and quick settle
        }

        const float speed = g_settings.animationSpeed;
        float widthStiffness = 280.0f * styleStiffnessMult;
        float widthDamping = 24.0f * styleDampingMult;
        if (targetWidth > widthSpring.value) {
            widthStiffness = 380.0f * styleStiffnessMult;
            widthDamping = 26.0f * styleDampingMult;
        } else if (targetWidth < widthSpring.value) {
            widthStiffness = 200.0f * styleStiffnessMult;
            widthDamping = 28.0f * styleDampingMult;
        }

        float heightStiffness = 280.0f * styleStiffnessMult;
        float heightDamping = 24.0f * styleDampingMult;
        if (targetHeight > heightSpring.value) {
            heightStiffness = 380.0f * styleStiffnessMult;
            heightDamping = 26.0f * styleDampingMult;
        } else if (targetHeight < heightSpring.value) {
            heightStiffness = 200.0f * styleStiffnessMult;
            heightDamping = 28.0f * styleDampingMult;
        }

        widthSpring.Step(dt * speed, widthStiffness, widthDamping);
        if (widthSpring.value < 0.0f) {
            widthSpring.value = 0.0f;
            widthSpring.velocity = 0.0f;
        }

        heightSpring.Step(dt * speed, heightStiffness, heightDamping);
        if (heightSpring.value < 0.0f) {
            heightSpring.value = 0.0f;
            heightSpring.velocity = 0.0f;
        }

        nudgeSpring.Step(dt * speed, 280.0f * styleStiffnessMult, 24.0f * styleDampingMult);

        satWidthSpring.Step(dt * speed, widthStiffness, widthDamping);
        if (satWidthSpring.value < 0.0f) {
            satWidthSpring.value = 0.0f;
            satWidthSpring.velocity = 0.0f;
        }

        satHeightSpring.Step(dt * speed, heightStiffness, heightDamping);
        if (satHeightSpring.value < 0.0f) {
            satHeightSpring.value = 0.0f;
            satHeightSpring.velocity = 0.0f;
        }

        const bool isSatVisible = satWidthSpring.value > 1.0f;
        g_satelliteActive.store(isSatVisible, std::memory_order_relaxed);

        if (isSatVisible || satWidthSpring.target > 1.0f || std::fabs(satWidthSpring.velocity) > 0.05f) {
            needsRender = true;
        }

        {
            std::lock_guard lock(g_stateMutex);
            g_state.system.renderFps = ClampInt(static_cast<int>(1.0f / std::max(dt, 0.001f) + 0.5f), 0, 1000);
        }

        const bool deliberateHover = isHoverActive; // dwelled >= 250ms
        const bool draggingOrHover = deliberateHover || ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && PtInRect(&windowRect, cursor));

        // Ctrl+hover see-through: holding Ctrl while hovering makes the island
        // transparent and click-through in ANY state, so clicks pass through
        // to windows underneath. Releasing Ctrl or moving away restores it.
        const bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool ctrlHoverCT = hover && ctrlHeld;

        bool clickThrough = ((primary.kind == IslandKind::Idle && !draggingOrHover && !pinned) || ctrlHoverCT);
        if (microNotchActive) {
            clickThrough = !deliberateHover && !pinned;
        }
        SetClickThrough(hwnd, clickThrough);

        // Check if animating structurally
        if (std::abs(widthSpring.velocity) > 0.01f || std::abs(widthSpring.target - widthSpring.value) > 0.01f ||
            std::abs(heightSpring.velocity) > 0.01f || std::abs(heightSpring.target - heightSpring.value) > 0.01f ||
            std::abs(nudgeSpring.velocity) > 0.01f || std::abs(nudgeSpring.target - nudgeSpring.value) > 0.01f) {
            needsRender = true;
        }

        // Active Monitor Tracking (Follow Mouse)
        if (g_settings.targetMonitor == -1) {
            static HMONITOR s_lastMonitor = nullptr;
            POINT pt;
            GetCursorPos(&pt);
            HMONITOR currentMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            if (currentMonitor != s_lastMonitor) {
                s_lastMonitor = currentMonitor;
                g_layoutDirty = true;
            }
        }

        // Check if layout was explicitly invalidated
        if (g_layoutDirty.load()) {
            needsRender = true;
        }

        // Hover or pinned state changes visual elements slightly
        static bool prevHover = false;
        static bool prevPinned = false;
        if (hover != prevHover || pinned != prevPinned) {
            needsRender = true;
            prevHover = hover;
            prevPinned = pinned;
        }

        // Activities that animate continuously: the waveform bars, the marquee
        // scroll and the battery pulse. These still only want ~60Hz -- they look no
        // different above it, and they should not piggyback on whatever high Target
        // FPS the user picked for structural resize animation.
        //
        // The rate limit lives in the pacer at the bottom of the loop, not here.
        // This used to gate needsRender behind a 1/60s (16.667ms) timer and then
        // fall through to the flat 16ms idle wait, which cannot satisfy it: 16ms is
        // shorter than the gate, so the next pass failed the check and waited a
        // second time. The result was a paint roughly every 32ms -- about 31fps
        // instead of 60, which is what made playing media look choppy.
        // Media only counts as continuous while something on it is actually moving.
        //
        // ChooseActivities selects the Media pill for any *available* SMTC session,
        // playing or not, so keying off the kind alone meant a paused Spotify or a
        // browser tab with a paused video -- which can sit there for hours, since
        // browsers keep the session alive -- held the island at a 60fps render loop
        // and 1ms timer resolution indefinitely. Nothing on it moves in that state:
        // the collapsed pill draws the album art plus a row of flat bars, taking the
        // !playing branch that skips DrawWaveform entirely.
        //
        // Paused media now falls back to the ordinary change detection above (title
        // and art generation, springs, hover), so the timer is released 0.5s after
        // the last paint.
        //
        // The marquees are expanded-only, hence the hover/pinned terms. No need to
        // include recentTrackChange: it already requires snapshot.media.playing, so
        // it cannot be true while paused.
        // Checks the secondary pill too. The island can show two pills side by side,
        // so a playing media pill sitting next to a running timer or a progress ring
        // is the secondary one -- and looking only at primary left its waveform
        // frozen.
        const bool mediaShown = primary.kind == IslandKind::Media ||
                                (secondary && secondary->kind == IslandKind::Media);
        const bool mediaAnimating =
            mediaShown && (snapshot.media.playing || isHoverExpanded || pinned);

        // Transient alert pills. All expire within a few seconds, so treating them as
        // continuous cannot run away.
        //
        // Device, Bluetooth and Do Not Disturb are here only when the countdown bar
        // is switched on, since that bar is the one thing on them that moves.
        const auto isTransientPill = [](IslandKind kind) {
            return kind == IslandKind::BatteryLow || kind == IslandKind::Clipboard ||
                   kind == IslandKind::Notification;
        };
        const auto hasCountdownBar = [](IslandKind kind) {
            return kind == IslandKind::Device || kind == IslandKind::Bluetooth ||
                   kind == IslandKind::DoNotDisturb;
        };
        const bool countdownBarsOn = g_settings.statusCountdownProgress;

        const bool continuousAnimation =
            mediaAnimating || isTransientPill(primary.kind) ||
            (secondary && isTransientPill(secondary->kind)) ||
            (countdownBarsOn && (hasCountdownBar(primary.kind) ||
                                 (secondary && hasCountdownBar(secondary->kind))));
        if (continuousAnimation) {
            needsRender = true;
        }

        // Privacy dots
        if (snapshot.system.micActive || snapshot.system.cameraActive) {
            needsRender = true;
        }

        // Idle dashboard and media pill clock changes once a minute
        static SYSTEMTIME prevTime = {};
        if ((primary.kind == IslandKind::Idle || primary.kind == IslandKind::Media) && !isHidden) {
            SYSTEMTIME local = {};
            GetLocalTime(&local);
            if (local.wMinute != prevTime.wMinute) {
                needsRender = true;
                prevTime = local;
            }
        }

        // Text that ticks once a second: the focus timer countdown and, when Show
        // seconds is on, the clock.
        //
        // Both used to ride on a side effect. The system poll refreshed CPU load
        // every second and the change detection compared it unconditionally, so the
        // whole island repainted about once a second whether anything visible had
        // changed or not. Gating those metric comparisons on visibility removed that,
        // which left DrawTimer recomputing its m:ss from NowSeconds() on a surface
        // nothing marked dirty -- a 25 minute session sat at 25:00 until some
        // unrelated event forced a paint -- and left the seconds clock updating once
        // a minute despite its own setting promising every second.
        //
        // Keyed to the displayed value rather than to elapsed time, so each visible
        // change paints exactly once. Deliberately not folded into
        // continuousAnimation: these need one frame per second, not the 1ms timer
        // resolution that continuous animation asks for.
        int shownSecond = -1;
        const bool timerShown = primary.kind == IslandKind::Timer ||
                               (secondary && secondary->kind == IslandKind::Timer);
        if (timerShown && snapshot.timer.running) {
            shownSecond = static_cast<int>(std::ceil(snapshot.timer.endsAt - now));
        } else if ((primary.kind == IslandKind::Idle || primary.kind == IslandKind::Media) && g_settings.showSeconds && !isHidden) {
            SYSTEMTIME st = {};
            GetLocalTime(&st);
            shownSecond = st.wSecond;
        }
        static int s_prevShownSecond = -1;
        if (shownSecond != s_prevShownSecond) {
            s_prevShownSecond = shownSecond;
            needsRender = true;
        }

        // Compare data snapshot to detect changes
        static uint64_t prevArtGen = 0;
        static uint64_t prevSrcIconGen = 0;
        static uint64_t prevNotifIconGen = 0;
        static uint64_t prevClipIconGen = 0;
        static int prevCpu = -1;
        static int prevRam = -1;
        static int prevDisk = -1;
        static int prevVol = -1;
        static bool prevMuted = false;
        static int prevBat = -1;
        static bool prevCharging = false;
        static int prevProg = -1;
        static std::wstring prevMediaTitle;
        static bool prevPlaying = false;

        // CPU / RAM / disk are compared only while a surface that draws them is on
        // screen. UpdateSystemSnapshot refreshes them every second and CPU load
        // essentially always differs between samples, so comparing them
        // unconditionally repainted the whole island once a second for numbers that
        // appear nowhere on the collapsed pill.
        const bool systemMetricsChanged =
            systemMetricsVisible && (snapshot.system.cpuPercent != prevCpu ||
                                     snapshot.system.memoryPercent != prevRam ||
                                     snapshot.system.diskPercent != prevDisk);

        if (snapshot.media.artGeneration != prevArtGen ||
            snapshot.media.sourceIconGeneration != prevSrcIconGen ||
            snapshot.media.title != prevMediaTitle ||
            // Play/pause has to be in here now that the metric tick no longer
            // repaints every second as a side effect. Pausing while collapsed swaps
            // the live waveform for the static bars, and without this the frozen
            // last playing frame stayed up until some unrelated repaint came along.
            snapshot.media.playing != prevPlaying ||
            snapshot.notification.icon.generation != prevNotifIconGen ||
            snapshot.clipboard.appIcon.generation != prevClipIconGen ||
            systemMetricsChanged ||
            snapshot.system.volumePercent != prevVol ||
            snapshot.system.volumeMuted != prevMuted ||
            snapshot.battery.percent != prevBat ||
            snapshot.battery.charging != prevCharging ||
            snapshot.progress.percent != prevProg) {
            needsRender = true;
            prevArtGen = snapshot.media.artGeneration;
            prevSrcIconGen = snapshot.media.sourceIconGeneration;
            prevMediaTitle = snapshot.media.title;
            prevPlaying = snapshot.media.playing;
            prevNotifIconGen = snapshot.notification.icon.generation;
            prevClipIconGen = snapshot.clipboard.appIcon.generation;
            prevCpu = snapshot.system.cpuPercent;
            prevRam = snapshot.system.memoryPercent;
            prevDisk = snapshot.system.diskPercent;
            prevVol = snapshot.system.volumePercent;
            prevMuted = snapshot.system.volumeMuted;
            prevBat = snapshot.battery.percent;
            prevCharging = snapshot.battery.charging;
            prevProg = snapshot.progress.percent;
        }

        // Track whether Ctrl+hover click-through state changed so we re-render
        static bool prevCtrlHoverCT = false;
        if (ctrlHoverCT != prevCtrlHoverCT) {
            needsRender = true;
            prevCtrlHoverCT = ctrlHoverCT;
        }

        if (needsRender) {
            // When Ctrl+hover click-through is active, reduce pill opacity so the
            // island becomes visually see-through to match the pass-through behavior.
            Settings renderSettings = GetSettingsCopy();
            if (ctrlHoverCT) {
                renderSettings.pillOpacity = Clamp(renderSettings.pillOpacity * 0.35f, 0.15f, 0.45f);
            } else if (renderSettings.themePreset == ThemePreset::Graphite && Wh_GetIntValue(L"PillOpacityOverride", -1) < 0) {
                const bool isExpanded = isHoverExpanded || pinned || isTransientAlert || (widthSpring.value > 260.0f);
                renderSettings.pillOpacity = isExpanded ? 0.98f : 0.88f;
            }
            renderer.Render(snapshot, renderSettings, primary, secondary,
                            widthSpring.value, heightSpring.value, nudgeSpring.value,
                            hover, pinned, now,
                            satWidthSpring.value, satHeightSpring.value,
                            isSatVisible, satExpanded);
        }

        // --- Zero-CPU parking for auto-hidden states (idle timeout / fullscreen) ---
        // Only parks once the collapse animation has actually settled at 0,
        // so the shrink still animates before we cut over to OS-hidden.
        const bool wantsAutoHiddenPark =
            !g_manuallyHidden.load() && !pinned && !isHoverExpanded && !isTransientAlert &&
            !privacyActive && !agyActive && !isSatVisible &&
            satWidthSpring.value < 0.5f && satWidthSpring.target < 0.5f &&
            (isHidden || fullscreenSuppressed) &&
            widthSpring.value < 0.5f && heightSpring.value < 0.5f &&
            std::fabs(widthSpring.velocity) < 0.5f && std::fabs(heightSpring.velocity) < 0.5f;

        if (wantsAutoHiddenPark) {
            parkedForFullscreen = fullscreenSuppressed;
            wasAutoHiddenParked = true;
            g_autoHiddenParked = true;
            ShowWindow(hwnd, SW_HIDE);
            g_audioCaptureNeeded.store(false, std::memory_order_relaxed);
            setHighResTimer(false);
            continue;
        }

        int targetFps = g_settings.targetFps;
        if (targetFps <= 0) {
            targetFps = GetMonitorRefreshRate(hwnd);
        }
        targetFps = ClampInt(targetFps, 30, 1000);
        double targetFrameMs = 1000.0 / static_cast<double>(targetFps);

        // Structural animation -- the springs resizing or nudging the island -- is
        // what benefits from a high Target FPS. Continuous content does not, so once
        // the springs have settled the interval is relaxed to 60Hz and the precise
        // pacer below hits it accurately.
        //
        // std::max, not a plain assignment: a user who deliberately set Target FPS
        // to 40 should keep 40 rather than being pushed up to 60.
        const bool springsAnimating =
            std::fabs(widthSpring.value - widthSpring.target) > 0.5f ||
            std::fabs(heightSpring.value - heightSpring.target) > 0.5f ||
            std::fabs(widthSpring.velocity) > 0.5f ||
            std::fabs(heightSpring.velocity) > 0.5f ||
            std::fabs(nudgeSpring.value) > 0.5f ||
            std::fabs(nudgeSpring.velocity) > 0.5f;

        if (continuousAnimation && !springsAnimating) {
            constexpr double kContinuousFrameMs = 1000.0 / 60.0;
            targetFrameMs = std::max(targetFrameMs, kContinuousFrameMs);
        }

        // Keyed to actual animation, not to repainting.
        //
        // A single repaint does not need 1ms pacing -- it needs one frame, which the
        // pacer delivers fine at the default resolution. Only things that draw a
        // *sequence* of frames care: continuous content and spring motion.
        //
        // Keying it to needsRender was wrong twice over. Per frame it thrashed,
        // because needsRender alternates when 60Hz content runs under a higher
        // Target FPS. And with the hold added, any one-off repaint still took the
        // resolution for the full hold -- including the metric tick, which fired
        // every second, so a plain idle island requested and released it once a
        // second and held it roughly half the time with nothing moving at all.
        //
        // continuousAnimation and springsAnimating are exactly the two cases that
        // want precise frame spacing, so they drive it directly.
        if (continuousAnimation || springsAnimating) {
            lastAnimatingAt = now;
        }
        setHighResTimer(lastAnimatingAt >= 0.0 && now - lastAnimatingAt < kHighResTimerHoldSec);

        if (!needsRender) {
            // Nothing changed on screen, so poll at ~60Hz rather than the target
            // frame rate. Accuracy does not matter here -- this is a poll interval,
            // not frame pacing -- so it runs at whatever resolution is in effect.
            WaitForSingleObject(g_stopEvent, 16);
            nextFrameTarget = std::chrono::steady_clock::now();
        } else {
            // When animating, achieve ultra-smooth target refresh rate (e.g. 144Hz, 240Hz, 360Hz+).
            nextFrameTarget += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double, std::milli>(targetFrameMs));

            auto nowTime = std::chrono::steady_clock::now();
            if (nowTime < nextFrameTarget) {
                double remainingMs = std::chrono::duration<double, std::milli>(nextFrameTarget - nowTime).count();
                if (remainingMs >= 1.5) {
                    // Sleep for the bulk of the remaining time using OS event wait (zero CPU usage)
                    WaitForSingleObject(g_stopEvent, static_cast<DWORD>(remainingMs - 0.5));
                }
                // Yield for the final fraction of a millisecond to ensure jitter-free presentation on 360Hz displays without CPU waste
                while (std::chrono::steady_clock::now() < nextFrameTarget &&
                       WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
                    std::this_thread::yield();
                }
            } else {
                // If we fell behind, reset target to avoid speed-up catch-up loop
                nextFrameTarget = nowTime;
            }
        }
    }

    // Balances whichever state the loop exited in; a no-op if already released.
    setHighResTimer(false);

    renderer.Shutdown();
    DestroyWindow(hwnd);
    g_hwnd = nullptr;
    UnregisterClassW(kWindowClass, wc.hInstance);

    if (SUCCEEDED(hrCo)) {
        CoUninitialize();
    }

    return 0;
}



inline bool StartThreads() {
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_settingsChangedEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_stopEvent || !g_settingsChangedEvent) {
        return false;
    }

    g_running = true;
    g_renderThread = CreateThread(nullptr, 0, RenderThreadProc, nullptr, 0, nullptr);
    if (!g_renderThread) {
        return false;
    }

    g_mediaThread = CreateThread(nullptr, 0, MediaThreadProc, nullptr, 0, nullptr);
    g_audioThread = CreateThread(nullptr, 0, AudioThreadProc, nullptr, 0, nullptr);
    g_weatherThread = CreateThread(nullptr, 0, WeatherThreadProc, nullptr, 0, nullptr);
    g_keyboardThread = CreateThread(nullptr, 0, KeyboardThreadProc, nullptr, 0, &g_keyboardThreadId);
    g_mouseThread = CreateThread(nullptr, 0, MouseThreadProc, nullptr, 0, &g_mouseThreadId);
#if DYNAMIC_ISLAND_HAS_USER_NOTIFICATION_LISTENER
    g_notificationThread = CreateThread(nullptr, 0, NotificationThreadProc, nullptr, 0, nullptr);
#endif
    g_bluetoothThread = CreateThread(nullptr, 0, BluetoothThreadProc, nullptr, 0, nullptr);
    SubscribeDndNotification();

    return true;
}

inline void StopThreads() {
    UnsubscribeDndNotification();
    if (g_keyboardThreadId != 0) {
        PostThreadMessageW(g_keyboardThreadId, WM_QUIT, 0, 0);
    }
    if (g_mouseThreadId != 0) {
        PostThreadMessageW(g_mouseThreadId, WM_QUIT, 0, 0);
    }
    if (g_stopEvent) {
        SetEvent(g_stopEvent);
    }

    HANDLE handles[] = {g_renderThread, g_mediaThread, g_audioThread, g_weatherThread, g_notificationThread, g_keyboardThread, g_mouseThread, g_bluetoothThread};
    for (HANDLE handle : handles) {
        if (handle) {
            WaitForSingleObject(handle, 3000);
            CloseHandle(handle);
        }
    }

    g_renderThread = nullptr;
    g_mediaThread = nullptr;
    g_audioThread = nullptr;
    g_weatherThread = nullptr;
    g_notificationThread = nullptr;
    g_keyboardThread = nullptr;
    g_keyboardThreadId = 0;
    g_mouseThread = nullptr;
    g_mouseThreadId = 0;
    g_bluetoothThread = nullptr;

    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
    if (g_settingsChangedEvent) {
        CloseHandle(g_settingsChangedEvent);
        g_settingsChangedEvent = nullptr;
    }

    g_running = false;
}



// } namespace


#endif // WINDOW_HOOK_MANAGER_HPP
