const fs = require('fs');
const path = require('path');

const modDir = __dirname;
const srcFile = path.join(modDir, 'dynamic-island-jedmamosto-fork.wh.cpp');
const dstFile = path.join(modDir, 'window_hook_manager.hpp');

const content = fs.readFileSync(srcFile, 'utf8');
const lines = content.split(/\r?\n/);

function getRange(sStr, eStr) {
  const s = lines.findIndex(l => l.includes(sStr));
  const e = lines.findIndex((l, idx) => idx > s && l.includes(eStr));
  if (s === -1 || e === -1) throw new Error('Range not found: ' + sStr + ' -> ' + eStr);
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
const r4 = getRange('void LoadSettings() {', 'namespace Backdrop {');
let b4 = lines.slice(r4.s, r4.e).join('\n');

// 5. Backdrop & window positioning (2667 to 3175)
const r5 = getRange('namespace Backdrop {', 'bool DecodeImageBytesToPixels(');
let b5 = lines.slice(r5.s, r5.e).join('\n');

// 6A. Clipboard, toast, click-through, timer, mute (6227 to 6649)
// Note: ends at the definition of SeekMediaToTicks(int64_t targetTicks) {
const r6a = getRange('std::wstring ReadClipboardText(HWND hwnd)', 'void SeekMediaToTicks(int64_t targetTicks) {');
let b6a = lines.slice(r6a.s, r6a.e).join('\n');

// 6B. Dismiss transient state & context menu (6905 to 7159)
const r6b = getRange('void DismissTransientState()', 'enum class WeatherVisual {');
let b6b = lines.slice(r6b.s, r6b.e).join('\n');

// 7. Activities, Hooks, WndProc, RenderThread, Start/Stop (11611 to 13663)
const r7 = getRange('Activity ActivityForKind(IslandKind kind,', 'BOOL WhTool_ModInit()');
let b7 = lines.slice(r7.s, r7.e).join('\n');

// In b7, mark globals inline
const b7Globals = [
  'HHOOK g_keyboardHook = nullptr;',
  'HANDLE g_keyboardThread = nullptr;',
  'DWORD g_keyboardThreadId = 0;',
  'HHOOK g_mouseHook = nullptr;',
  'HANDLE g_mouseThread = nullptr;',
  'DWORD g_mouseThreadId = 0;',
  'std::atomic<int64_t> g_lastMouseWakeCheckMs = 0;'
];
b7Globals.forEach(g => {
  b7 = b7.replace(g, 'inline ' + g);
});

// Remove redundant forward declarations in b6a that conflict with MediaEngine
b6a = b6a.replace('void OpenRelevantApp();', '// void OpenRelevantApp();');
b6a = b6a.replace('void SeekMediaToTicks(int64_t targetTicks);', '// void SeekMediaToTicks(int64_t targetTicks);');

// Remove duplicate WM_APP_CAPSLOCK in b7
b7 = b7.replace('constexpr UINT WM_APP_CAPSLOCK = WM_APP + 0x444;', '// constexpr UINT WM_APP_CAPSLOCK defined in island_common.hpp');

// Remove closing anonymous namespace in b7
b7 = b7.replace(/\}\s*\/\/\s*namespace/g, '// } namespace');

// Exact replacements: match the exact signature at definition and prepend 'inline '
const exactDefs = [
  'Activity ActivityForKind(',
  'std::vector<IslandKind> ChooseActivities(',
  'int MediaTransportHitTest(',
  'float MediaScrubFractionFromContent(',
  'float MediaScrubFractionUnbounded(',
  'int FileTrayRowAtContentPoint(',
  'bool MediaArtHitTest(',
  'bool EqualsNoCase(',
  'std::wstring GetStringSettingCopy(',
  'std::wstring GetStringSettingWithFallback(',
  'bool IsForegroundFullscreen(',
  'bool IsForegroundMaximized(',
  'float GetPrimaryMonitorDpiScale(',
  'int GetMonitorRefreshRate(',
  'void LoadSettings() {',
  'void EnableBlurBehind(',
  'SetWindowCompositionAttributeFn Resolve() {',
  'void ApplyBackdropMaterial(HWND hwnd) {',
  'void ApplyHideShowHotkey() {',
  'RECT GetAnchorWorkRect() {',
  'int ResolveOffsetY(float windowHeight) {',
  'void PositionOverlayWindow(HWND hwnd, int width, int height) {',
  'void ApplyBackdropRegion(HWND hwnd, int windowWidth, int windowHeight) {',
  'bool IsShellOwnedWindow(HWND hwnd) {',
  'void EnsureTopmost(HWND hwnd) {',
  'RECT GetIslandDockRect() {',
  'bool FocusAntigravityWindow() {',
  'std::wstring ReadClipboardText(HWND hwnd) {',
  'bool ReadClipboardImagePixels(HWND hwnd, BitmapPixels* outPixels, UINT maxDim) {',
  'bool IsLikelyToastWindow(HWND hwnd, const wchar_t* className, const wchar_t* title) {',
  'void CaptureShellNotification(HWND hwnd) {',
  'void CaptureClipboard(HWND hwnd) {',
  'void SetClickThrough(HWND hwnd, bool clickThrough) {',
  'void HandleStatusClickAtPoint(HWND hwnd, LPARAM lParam) {',
  'void StartFocusTimer(int minutes, bool isBreak) {',
  'void ToggleTimerPause() {',
  'void StopFocusTimer() {',
  'void ToggleEndpointMute() {',
  'void DismissTransientState() {',
  'void ShowContextMenu(HWND hwnd, POINT screenPoint) {',
  'LRESULT CALLBACK LowLevelKeyboardProc(',
  'void NotifyKeyboardThreadSettingChanged() {',
  'DWORD WINAPI KeyboardThreadProc(',
  'LRESULT CALLBACK LowLevelMouseProc(',
  'DWORD WINAPI MouseThreadProc(',
  'LRESULT CALLBACK OverlayWndProc(',
  'DWORD WINAPI RenderThreadProc(',
  'bool StartThreads() {',
  'void StopThreads() {'
];

function inlineBlock(block) {
  let res = block;
  exactDefs.forEach(def => {
    if (res.includes(def) && !res.includes('inline ' + def)) {
      res = res.replace(def, 'inline ' + def);
    }
  });
  return res;
}

b1 = inlineBlock(b1);
b2 = inlineBlock(b2);
b3 = inlineBlock(b3);
b4 = inlineBlock(b4);
b5 = inlineBlock(b5);
b6a = inlineBlock(b6a);
b6b = inlineBlock(b6b);
b7 = inlineBlock(b7);

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

${b1}

${b2}

${b3}

${b4}

${b5}

${b6a}

${b6b}

${b7}

#endif // WINDOW_HOOK_MANAGER_HPP
`;

fs.writeFileSync(dstFile, header, 'utf8');
console.log('Successfully written clean window_hook_manager.hpp! Lines:', header.split('\n').length);
