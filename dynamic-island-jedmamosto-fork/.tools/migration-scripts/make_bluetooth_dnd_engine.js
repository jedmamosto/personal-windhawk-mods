const fs = require('fs');
const path = require('path');

const modDir = __dirname;
const srcFile = path.join(modDir, 'dynamic-island-jedmamosto-fork.wh.cpp');
const dstFile = path.join(modDir, 'bluetooth_dnd_engine.hpp');

const content = fs.readFileSync(srcFile, 'utf8');
const lines = content.split(/\r?\n/);

const s = lines.findIndex(l => l.includes('typedef LONG NTSTATUS;'));
const e = lines.findIndex((l, idx) => idx > s && l.includes('float SampleAudioAmplitude('));

let body = lines.slice(s, e).join('\n');

const defs = [
  'void SubscribeDndNotification() {',
  'void UnsubscribeDndNotification() {',
  'DWORD WINAPI NotificationThreadProc(void*) {',
  'BluetoothDeviceCategory ClassifyBluetoothDevice(',
  'int TryReadBluetoothBatteryPercentBLE(',
  'int TryReadClassicBluetoothBatteryPercent(',
  'int TryReadBluetoothBatteryPercent(',
  'int GetLastKnownBluetoothBatteryPercent(',
  'void HandleBluetoothConnected(',
  'void HandleBluetoothDisconnected(',
  'DWORD WINAPI BluetoothThreadProc(void*) {'
];

defs.forEach(d => {
  if (body.includes(d) && !body.includes('inline ' + d)) {
    body = body.replaceAll(d, 'inline ' + d);
  }
});

const header = `#pragma once

#ifndef BLUETOOTH_DND_ENGINE_HPP
#define BLUETOOTH_DND_ENGINE_HPP

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
#include <setupapi.h>
#include <devpropdef.h>
#include <devguid.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

#if __has_include(<winrt/Windows.UI.Notifications.Management.h>) && \\
    __has_include(<winrt/Windows.UI.Notifications.h>)
#define DYNAMIC_ISLAND_HAS_USER_NOTIFICATION_LISTENER 1
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#else
#define DYNAMIC_ISLAND_HAS_USER_NOTIFICATION_LISTENER 0
#endif

#if __has_include(<winrt/Windows.Devices.Enumeration.h>) && \\
    __has_include(<winrt/Windows.Devices.Bluetooth.h>) && \\
    __has_include(<winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>)
#define DYNAMIC_ISLAND_HAS_BLUETOOTH_WATCHER 1
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#else
#define DYNAMIC_ISLAND_HAS_BLUETOOTH_WATCHER 0
#endif

#include "island_common.hpp"
#include "notification_engine.hpp"

extern HANDLE g_stopEvent;

inline std::atomic<bool> g_isDnDActive = false;
inline void* g_wnfDndSubscription = nullptr;
inline std::mutex g_bluetoothBatteryCacheMutex;
inline std::unordered_map<std::wstring, int> g_bluetoothBatteryCache;
inline std::unordered_map<std::wstring, BluetoothAccessoryInfo> g_bluetoothConnectedAccessories;
inline std::atomic<uint64_t> g_bluetoothConnectGeneration = 0;
inline DynamicIsland::Notifications::NotificationListenerEngine g_notificationEngine;

${body}

#endif // BLUETOOTH_DND_ENGINE_HPP
`;

fs.writeFileSync(dstFile, header, 'utf8');
console.log('Successfully written bluetooth_dnd_engine.hpp! Lines:', header.split('\n').length);
