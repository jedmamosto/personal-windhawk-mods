const fs = require('fs');
const path = require('path');

const modDir = path.resolve('dynamic-island-jedmamosto-fork');
const srcFile = path.join(modDir, 'dynamic-island-jedmamosto-fork.wh.cpp');
const dstFile = path.join(modDir, 'window_hook_manager.hpp');

const content = fs.readFileSync(srcFile, 'utf8');
const lines = content.split(/\r?\n/);

// Helper to get lines by search
function getRange(startStr, endStr) {
    const s = lines.findIndex(l => l.includes(startStr));
    const e = lines.findIndex((l, idx) => idx > s && l.includes(endStr));
    if (s === -1 || e === -1) throw new Error(`Range not found: ${startStr} -> ${endStr} (${s}, ${e})`);
    return { s, e };
}

// 1. Hit test helpers (1690 to 1795)
const r1 = getRange('int MediaTransportHitTest(', 'bool EqualsNoCase(');
let b1 = lines.slice(r1.s, r1.e).join('\n');

// 2. Settings string helpers (1797 to 1833)
const r2 = getRange('bool EqualsNoCase(', 'D2D1_COLOR_F ColorFromHex(');
let b2 = lines.slice(r2.s, r2.e).join('\n');

// 3. Foreground & monitor refresh helpers (1996 to 2098)
const r3 = getRange('bool IsForegroundFullscreen(', 'D2D1_COLOR_F GetSystemAccentColor(');
let b3 = lines.slice(r3.s, r3.e).join('\n');

// 4. LoadSettings (2117 to 2665)
const r4 = getRange('void ApplyHideShowHotkey();', 'namespace Backdrop {');
let b4 = lines.slice(r4.s, r4.e).join('\n');

// 5. Backdrop & window positioning (2667 to 3175)
const r5 = getRange('namespace Backdrop {', 'bool DecodeImageBytesToPixels(');
let b5 = lines.slice(r5.s, r5.e).join('\n');

// 6. Clipboard, toast, timer, activities (6227 to 7240)
const r6 = getRange('std::wstring ReadClipboardText(HWND hwnd)', 'class Renderer {');
let b6 = lines.slice(r6.s, r6.e).join('\n');

// 7. Hooks, WndProc, RenderThread, Start/Stop (11751 to 13682)
const r7 = getRange('LRESULT CALLBACK LowLevelKeyboardProc', 'void WhTool_ModSettingsChanged()');
let b7 = lines.slice(r7.s, r7.e).join('\n');

// Mark functions inline to avoid ODR/duplicate symbol violations in companion headers
function makeInline(txt, funcName) {
    const regex = new RegExp(`\\b(${funcName}\\s*\\()`, 'g');
    return txt.replace(regex, 'inline $1');
}

[
    'MediaTransportHitTest', 'MediaScrubFractionFromContent', 'MediaScrubFractionUnbounded',
    'FileTrayRowAtContentPoint', 'MediaArtHitTest', 'EqualsNoCase', 'GetStringSettingCopy',
    'GetStringSettingWithFallback', 'IsForegroundFullscreen', 'IsForegroundMaximized',
    'GetPrimaryMonitorDpiScale', 'GetMonitorRefreshRate', 'LoadSettings', 'ApplyBackdropMaterial',
    'ApplyHideShowHotkey', 'GetAnchorWorkRect', 'ResolveOffsetY', 'PositionOverlayWindow',
    'ApplyBackdropRegion', 'EnsureTopmost', 'GetIslandDockRect', 'FocusAntigravityWindow',
    'ReadClipboardText', 'ReadClipboardImagePixels', 'IsLikelyToastWindow', 'CaptureShellNotification',
    'CaptureClipboard', 'SetClickThrough', 'UpdateTimerSnapshot', 'StartFocusTimer', 'CancelTimer',
    'ChooseActivities', 'DecideClickThrough', 'FindIslandWindow', 'StartThreads', 'StopThreads'
].forEach(fn => {
    b1 = makeInline(b1, fn);
    b2 = makeInline(b2, fn);
    b3 = makeInline(b3, fn);
    b4 = makeInline(b4, fn);
    b5 = makeInline(b5, fn);
    b6 = makeInline(b6, fn);
    b7 = makeInline(b7, fn);
});

const header = `#pragma once

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
#include <shellapi.h>
#include <uiautomation.h>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <algorithm>

#include "island_common.hpp"
#include "palette_color_engine.hpp"
#include "icon_process_engine.hpp"
#include "weather_location_engine.hpp"
#include "telemetry_privacy_engine.hpp"
#include "media_smtc_engine.hpp"
#include "battery_dashboard.hpp"
#include "notification_engine.hpp"
#include "agy_telemetry_engine.hpp"
#include "ui_painters_engine.hpp"

// Global thread handles and state instances
inline HANDLE g_mouseThread = nullptr;
inline DWORD g_mouseThreadId = 0;
inline HHOOK g_mouseHook = nullptr;

inline HANDLE g_keyboardThread = nullptr;
inline DWORD g_keyboardThreadId = 0;
inline HHOOK g_keyboardHook = nullptr;

inline HANDLE g_renderThread = nullptr;
inline DWORD g_renderThreadId = 0;

inline HANDLE g_mediaThread = nullptr;
inline HANDLE g_audioThread = nullptr;
inline HANDLE g_weatherThread = nullptr;
inline HANDLE g_notificationThread = nullptr;
inline HANDLE g_bluetoothThread = nullptr;

inline HANDLE g_stopEvent = nullptr;
inline HANDLE g_settingsChangedEvent = nullptr;

inline DynamicIsland::Notifications::NotificationListenerEngine g_notificationEngine;
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

${b1}

${b2}

${b3}

${b4}

${b5}

${b6}

${b7}

#endif // WINDOW_HOOK_MANAGER_HPP
`;

fs.writeFileSync(dstFile, header, 'utf8');
console.log('Successfully written full window_hook_manager.hpp! Lines:', header.split('\n').length);
