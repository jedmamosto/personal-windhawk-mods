const fs = require('fs');
const path = require('path');

const modDir = path.resolve('dynamic-island-jedmamosto-fork');
const srcFile = path.join(modDir, 'dynamic-island-jedmamosto-fork.wh.cpp');
const dstFile = path.join(modDir, 'window_hook_manager.hpp');

const content = fs.readFileSync(srcFile, 'utf8');
const lines = content.split(/\r?\n/);

// Find blocks:
// 1. Backdrop and window positioning: from "namespace Backdrop {" (line ~2667) to "bool DecodeImageBytesToPixels" (line ~3175)
const startBackdrop = lines.findIndex(l => l.includes('namespace Backdrop {'));
const endPositioning = lines.findIndex(l => l.includes('bool DecodeImageBytesToPixels'));

console.log('Backdrop to Positioning:', startBackdrop, 'to', endPositioning);

// 2. Clipboard, Toast, Timer, Activities, Clickthrough: from "std::wstring ReadClipboardText(HWND hwnd)" (line ~6227) to before "class Renderer {" (line ~7241)
const startClip = lines.findIndex(l => l.includes('std::wstring ReadClipboardText(HWND hwnd)'));
const endClip = lines.findIndex(l => l.includes('class Renderer {'));

console.log('Clip, Toast, Activities, Clickthrough:', startClip, 'to', endClip);

// 3. Hooks, WndProc, RenderThreadProc, Start/StopThreads: from "LRESULT CALLBACK LowLevelKeyboardProc" (line ~11751) to "void WhTool_ModSettingsChanged()" (line ~13683)
const startHooks = lines.findIndex(l => l.includes('LRESULT CALLBACK LowLevelKeyboardProc'));
const endThreads = lines.findIndex(l => l.includes('void WhTool_ModSettingsChanged()'));

console.log('Hooks and Threads:', startHooks, 'to', endThreads);

let blockBackdrop = lines.slice(startBackdrop, endPositioning).join('\n');
let blockClip = lines.slice(startClip, endClip).join('\n');
let blockHooks = lines.slice(startHooks, endThreads).join('\n');

// Replace void FocusAntigravityWindow forward decl if present, or mark inline
blockBackdrop = blockBackdrop.replace(/void ApplyBackdropMaterial\(/g, 'inline void ApplyBackdropMaterial(');
blockBackdrop = blockBackdrop.replace(/void ApplyHideShowHotkey\(/g, 'inline void ApplyHideShowHotkey(');
blockBackdrop = blockBackdrop.replace(/RECT GetAnchorWorkRect\(/g, 'inline RECT GetAnchorWorkRect(');
blockBackdrop = blockBackdrop.replace(/int ResolveOffsetY\(/g, 'inline int ResolveOffsetY(');
blockBackdrop = blockBackdrop.replace(/void PositionOverlayWindow\(/g, 'inline void PositionOverlayWindow(');
blockBackdrop = blockBackdrop.replace(/void EnsureTopmost\(/g, 'inline void EnsureTopmost(');
blockBackdrop = blockBackdrop.replace(/RECT GetIslandDockRect\(/g, 'inline RECT GetIslandDockRect(');
blockBackdrop = blockBackdrop.replace(/bool FocusAntigravityWindow\(/g, 'inline bool FocusAntigravityWindow(');

blockClip = blockClip.replace(/std::wstring ReadClipboardText\(/g, 'inline std::wstring ReadClipboardText(');
blockClip = blockClip.replace(/bool ReadClipboardImagePixels\(/g, 'inline bool ReadClipboardImagePixels(');
blockClip = blockClip.replace(/bool IsLikelyToastWindow\(/g, 'inline bool IsLikelyToastWindow(');
blockClip = blockClip.replace(/void CaptureShellNotification\(/g, 'inline void CaptureShellNotification(');

blockHooks = blockHooks.replace(/bool StartThreads\(/g, 'inline bool StartThreads(');
blockHooks = blockHooks.replace(/void StopThreads\(/g, 'inline void StopThreads(');

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

${blockBackdrop}

${blockClip}

${blockHooks}

#endif // WINDOW_HOOK_MANAGER_HPP
`;

fs.writeFileSync(dstFile, header, 'utf8');
console.log('Successfully written window_hook_manager.hpp! Lines:', header.split('\n').length);
