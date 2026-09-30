#pragma once

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

#if __has_include(<winrt/Windows.UI.Notifications.Management.h>) && \
    __has_include(<winrt/Windows.UI.Notifications.h>)
#define DYNAMIC_ISLAND_HAS_USER_NOTIFICATION_LISTENER 1
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#else
#define DYNAMIC_ISLAND_HAS_USER_NOTIFICATION_LISTENER 0
#endif

#if __has_include(<winrt/Windows.Devices.Enumeration.h>) && \
    __has_include(<winrt/Windows.Devices.Bluetooth.h>) && \
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

typedef LONG NTSTATUS;
typedef NTSTATUS (NTAPI *PWNF_USER_CALLBACK)(
    ULONG64 StateName,
    ULONG ChangeStamp,
    void* TypeId,
    void* CallbackContext,
    const void* Buffer,
    ULONG BufferSize
);

typedef NTSTATUS(NTAPI* PFN_RtlSubscribeWnfStateChangeNotification)(
    void** Subscription,
    ULONG64 StateName,
    ULONG ChangeStamp,
    PWNF_USER_CALLBACK Callback,
    void* CallbackContext,
    const void* TypeId,
    ULONG SerializationGroup,
    ULONG Unknown
);

typedef NTSTATUS(NTAPI* PFN_RtlUnsubscribeWnfStateChangeNotification)(
    void* Subscription
);

typedef NTSTATUS(NTAPI* PFN_NtQueryWnfStateData)(
    const ULONG64* StateName,
    const void* TypeId,
    const void* ExplicitScope,
    ULONG* ChangeStamp,
    void* Buffer,
    ULONG* BufferSize
);

constexpr ULONG64 kWnfQuietHoursActiveProfileChanged = 0xD83063EA3BF1C75ULL;

NTSTATUS NTAPI WnfDndCallback(
    ULONG64 stateName,
    ULONG changeStamp,
    void* typeId,
    void* callbackContext,
    const void* buffer,
    ULONG bufferSize
) {
    if (stateName != kWnfQuietHoursActiveProfileChanged) return 0;
    int val = 0;
    if (buffer && bufferSize >= sizeof(int)) {
        val = *reinterpret_cast<const int*>(buffer);
    }
    const bool active = (val != 0);
    static std::atomic<bool> s_firstWnf = true;
    if (s_firstWnf.exchange(false)) {
        g_isDnDActive.store(active);
        return 0;
    }

    const bool prev = g_isDnDActive.exchange(active);
    if (prev != active && g_settings.doNotDisturbIndicator) {
        {
            std::lock_guard lock(g_stateMutex);
            g_state.doNotDisturb.active = true;
            g_state.doNotDisturb.enabled = active;
            g_state.doNotDisturb.expiresAt = NowSeconds() + 3.0;
        }
        TriggerNudge();
    }
    return 0;
}

inline void SubscribeDndNotification() {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return;

    auto pfnQuery = reinterpret_cast<PFN_NtQueryWnfStateData>(
        GetProcAddress(hNtdll, "NtQueryWnfStateData"));
    if (pfnQuery) {
        ULONG stamp = 0;
        ULONG size = sizeof(int);
        int val = 0;
        ULONG64 stateName = kWnfQuietHoursActiveProfileChanged;
        if (pfnQuery(&stateName, nullptr, nullptr, &stamp, &val, &size) == 0 && size >= sizeof(int)) {
            g_isDnDActive.store(val != 0);
        }
    }

    auto pfnSubscribe = reinterpret_cast<PFN_RtlSubscribeWnfStateChangeNotification>(
        GetProcAddress(hNtdll, "RtlSubscribeWnfStateChangeNotification"));
    if (!pfnSubscribe) return;

    pfnSubscribe(&g_wnfDndSubscription, kWnfQuietHoursActiveProfileChanged, 0,
                 WnfDndCallback, nullptr, nullptr, 0, 0);
}

inline void UnsubscribeDndNotification() {
    if (!g_wnfDndSubscription) return;
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        auto pfnUnsubscribe = reinterpret_cast<PFN_RtlUnsubscribeWnfStateChangeNotification>(
            GetProcAddress(hNtdll, "RtlUnsubscribeWnfStateChangeNotification"));
        if (pfnUnsubscribe) {
            pfnUnsubscribe(g_wnfDndSubscription);
        }
    }
    g_wnfDndSubscription = nullptr;
}

#if DYNAMIC_ISLAND_HAS_USER_NOTIFICATION_LISTENER
inline DWORD WINAPI NotificationThreadProc(void*) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    // Enforce 30-second delay from process creation before touching API to ensure UWP subsystem is loaded
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER ct;
        ct.LowPart = creationTime.dwLowDateTime;
        ct.HighPart = creationTime.dwHighDateTime;

        FILETIME systemTime;
        GetSystemTimeAsFileTime(&systemTime);
        ULARGE_INTEGER st;
        st.LowPart = systemTime.dwLowDateTime;
        st.HighPart = systemTime.dwHighDateTime;

        uint64_t msSinceProcessStart = (st.QuadPart - ct.QuadPart) / 10000;
        if (msSinceProcessStart < 30000) {
            DWORD waitTime = 30000 - (DWORD)msSinceProcessStart;
            Wh_Log(L"Process started recently. Delaying UserNotificationListener init by %d ms to let UWP subsystem load...", waitTime);
            WaitForSingleObject(g_stopEvent, waitTime);
        }
    }

    // Zero-polling push subscription via Universal Notification Engine
    g_notificationEngine.Initialize([](const DynamicIsland::Notifications::NotificationItem& item) {
        if (g_settings.notificationRespectDnD && g_isDnDActive.load()) {
            return;
        }
        NotificationSnapshot snapshot;
        snapshot.active = true;
        snapshot.expiresAt = NowSeconds() + 4.5;
        snapshot.app = item.appName;
        snapshot.title = item.title;
        snapshot.body = item.body;
        {
            std::lock_guard lock(g_stateMutex);
            g_state.notification = std::move(snapshot);
        }
        TriggerNudge();
    });

    WaitForSingleObject(g_stopEvent, INFINITE);
    g_notificationEngine.Stop();
    winrt::uninit_apartment();
    return 0;
}

#if 0 // Replaced legacy polling routine


    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        try {
            auto listener = UserNotificationListener::Current();
            auto access = listener.RequestAccessAsync().get();
            if (access != UserNotificationListenerAccessStatus::Allowed) {
                if (!accessLogged) {
                    if (access == UserNotificationListenerAccessStatus::Denied) {
                        // Actionable: this is a Windows privacy switch, not a
                        // mod setting, and it is the usual reason the module
                        // stays silent when everything else is configured.
                        Wh_Log(L"Notification listener permission DENIED by Windows. "
                               L"Enable Settings > Privacy & security > Notifications > "
                               L"\"Let apps access your notifications\", then restart the mod. "
                               L"Retrying meanwhile...");
                    } else {
                        Wh_Log(L"Notification listener permission not granted or UWP subsystem not ready on boot (status: %d); retrying connection loop...", (int)access);
                    }
                    accessLogged = true;
                }
                WaitForSingleObject(g_stopEvent, 3000);
                continue;
            }

            if (accessLogged) {
                Wh_Log(L"WinRT UserNotificationListener successfully connected.");
                accessLogged = false;
            }

            while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
                try {
                    auto notifications = listener.GetNotificationsAsync(NotificationKinds::Toast).get();
                    std::set<uint32_t> currentIds;

                    for (uint32_t i = 0; i < notifications.Size(); ++i) {
                        currentIds.insert(notifications.GetAt(i).Id());
                    }

                    if (firstPoll) {
                        seenIds = std::move(currentIds);
                        firstPoll = false;
                        WaitForSingleObject(g_stopEvent, 1000);
                        continue;
                    }

                    for (uint32_t i = 0; i < notifications.Size(); ++i) {
                        try {
                            auto userNotification = notifications.GetAt(i);
                            const uint32_t id = userNotification.Id();

                            if (seenIds.count(id)) {
                                continue;
                            }

                            // Immediately mark as seen so we don't process it again
                            seenIds.insert(id);

                            if (g_settings.notificationRespectDnD && g_isDnDActive.load()) {
                                continue;
                            }

                            NotificationSnapshot snapshot;
                            snapshot.active = true;
                            snapshot.expiresAt = NowSeconds() + 4.0;
                            auto appInfo = userNotification.AppInfo();
                            auto displayInfo = appInfo.DisplayInfo();
                            snapshot.app = displayInfo.DisplayName().c_str();
                            try {
                                auto logo = displayInfo.GetLogo({32.0f, 32.0f});
                                std::vector<uint8_t> logoBytes = ReadWinRtStreamBytes(logo);
                                if (!logoBytes.empty()) {
                                    DecodeImageBytesToPixels(logoBytes, &snapshot.icon);
                                }
                            } catch (...) {
                            }

                            if (snapshot.icon.bgra.empty() && !snapshot.app.empty()) {
                                snapshot.icon = FindAppIconByName(snapshot.app, 64);
                            }

                            auto notification = userNotification.Notification();
                            auto binding = notification.Visual().GetBinding(KnownNotificationBindings::ToastGeneric());
                            if (binding) {
                                auto textElements = binding.GetTextElements();
                                if (textElements.Size() > 0) {
                                    snapshot.title = textElements.GetAt(0).Text().c_str();
                                }
                                if (textElements.Size() > 1) {
                                    snapshot.body = textElements.GetAt(1).Text().c_str();
                                }
                            }

                            if (snapshot.title.empty()) {
                                snapshot.title = snapshot.app.empty() ? L"New notification" : snapshot.app;
                            }
                            if (!snapshot.body.empty()) {
                                snapshot.title += L" - " + snapshot.body;
                            }
                            if (snapshot.title.size() > 120) {
                                snapshot.title.resize(120);
                                snapshot.title += L"...";
                            }

                            {
                                std::lock_guard lock(g_stateMutex);
                                g_state.notification = std::move(snapshot);
                            }
                            TriggerNudge();
                        } catch (const winrt::hresult_error& nex) {
                            if (nex.to_abi() == 0x80004001 || nex.to_abi() == 0x80040154) { // E_NOTIMPL or REGDB_E_CLASSNOTREG
                                // UWP subsystem not ready, skip without spamming logs
                            } else {
                                Wh_Log(L"Failed to parse a notification (0x%08X); skipping.", nex.to_abi());
                            }
                        } catch (...) {
                            Wh_Log(L"Failed to parse a notification; skipping.");
                        }
                    }

                    seenIds = std::move(currentIds);
                } catch (const winrt::hresult_error& ex) {
                    const HRESULT hr = ex.to_abi();
                    if (hr == 0x80004001 || hr == 0x80040154) { // E_NOTIMPL or REGDB_E_CLASSNOTREG
                        if (!accessLogged) {
                            Wh_Log(L"Notification listener UWP subsystem not fully ready (0x%08X). Retrying in background...", hr);
                            accessLogged = true;
                        }
                    } else {
                        Wh_Log(L"NotificationThreadProc inner loop WinRT error: %s (0x%08X); reconnecting...", ex.message().c_str(), hr);
                    }
                    WaitForSingleObject(g_stopEvent, 3000);
                    break;
                } catch (...) {
                    Wh_Log(L"NotificationThreadProc inner loop unknown exception; reconnecting...");
                    WaitForSingleObject(g_stopEvent, 3000);
                    break;
                }

                WaitForSingleObject(g_stopEvent, 1000);
            }
        } catch (const winrt::hresult_error& ex) {
            if (!accessLogged) {
                Wh_Log(L"NotificationThreadProc connection error: %s (0x%08X). Retrying in 3s...", ex.message().c_str(), ex.to_abi());
                accessLogged = true;
            }
            WaitForSingleObject(g_stopEvent, 3000);
        } catch (...) {
            if (!accessLogged) {
                Wh_Log(L"NotificationThreadProc unknown connection exception. Retrying in 3s...");
                accessLogged = true;
            }
            WaitForSingleObject(g_stopEvent, 3000);
        }
    }

    winrt::uninit_apartment();
    return 0;
}
#endif
#endif

#if DYNAMIC_ISLAND_HAS_BLUETOOTH_WATCHER

inline BluetoothDeviceCategory ClassifyBluetoothDevice(
    const winrt::Windows::Devices::Enumeration::DeviceInformation& info) {
    // 1) Name-based heuristic first — cheap, synchronous, and correct for
    //    the overwhelming majority of consumer devices, which advertise a
    //    descriptive friendly name.
    std::wstring name = ToLowerCopy(std::wstring(info.Name().c_str()));

    if (name.find(L"headphone") != std::wstring::npos ||
        name.find(L"headset") != std::wstring::npos ||
        name.find(L"earbud") != std::wstring::npos ||
        name.find(L"buds") != std::wstring::npos ||
        name.find(L"airpods") != std::wstring::npos) {
        return BluetoothDeviceCategory::Headphones;
    }
    if (name.find(L"speaker") != std::wstring::npos ||
        name.find(L"soundbar") != std::wstring::npos ||
        name.find(L"boombox") != std::wstring::npos) {
        return BluetoothDeviceCategory::Speaker;
    }
    if (name.find(L"mouse") != std::wstring::npos) {
        return BluetoothDeviceCategory::Mouse;
    }
    if (name.find(L"keyboard") != std::wstring::npos) {
        return BluetoothDeviceCategory::Keyboard;
    }
    if (name.find(L"iphone") != std::wstring::npos ||
        name.find(L"phone") != std::wstring::npos ||
        name.find(L"galaxy") != std::wstring::npos ||
        name.find(L"pixel") != std::wstring::npos) {
        return BluetoothDeviceCategory::Phone;
    }

    // 2) Fall back to the classic Bluetooth Class-of-Device major-class bits.
    try {
        using winrt::Windows::Devices::Bluetooth::BluetoothDevice;
        using winrt::Windows::Devices::Bluetooth::BluetoothMajorClass;
        auto device = BluetoothDevice::FromIdAsync(info.Id()).get();
        if (device) {
            switch (device.ClassOfDevice().MajorClass()) {
                case BluetoothMajorClass::Phone:
                    return BluetoothDeviceCategory::Phone;
                case BluetoothMajorClass::AudioVideo:
                    return BluetoothDeviceCategory::Headphones;
                case BluetoothMajorClass::Peripheral:
                    return BluetoothDeviceCategory::Mouse;
                default:
                    break;
            }
        }
    } catch (...) {
        // Not every device id resolves to a classic BluetoothDevice — BLE-only
        // peripherals in particular will throw here. Fall through.
    }

    return BluetoothDeviceCategory::Generic;
}

// Reads the standard GATT Battery Service (0x180F) / Battery Level
// characteristic (0x2A19). Only works for devices that expose battery this
// way — mostly BLE and BLE-dual-mode devices. Classic-only devices will
// fail the GetGattServicesForUuidAsync call and we just report -1 (unknown).
inline int TryReadBluetoothBatteryPercentBLE(winrt::hstring const& deviceId) {
    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

    try {
        auto bleDevice = BluetoothLEDevice::FromIdAsync(deviceId).get();
        if (!bleDevice) {
            return -1;
        }

        auto servicesResult = bleDevice.GetGattServicesForUuidAsync(
            GattServiceUuids::Battery(), BluetoothCacheMode::Uncached).get();
        if (servicesResult.Status() != GattCommunicationStatus::Success ||
            servicesResult.Services().Size() == 0) {
            return -1;
        }

        auto service = servicesResult.Services().GetAt(0);
        auto charsResult = service.GetCharacteristicsForUuidAsync(
            GattCharacteristicUuids::BatteryLevel(), BluetoothCacheMode::Uncached).get();
        if (charsResult.Status() != GattCommunicationStatus::Success ||
            charsResult.Characteristics().Size() == 0) {
            return -1;
        }

        auto characteristic = charsResult.Characteristics().GetAt(0);
        auto readResult = characteristic.ReadValueAsync(BluetoothCacheMode::Uncached).get();
        if (readResult.Status() != GattCommunicationStatus::Success) {
            return -1;
        }

        auto buffer = readResult.Value();
        if (buffer.Length() < 1) {
            return -1;
        }

        auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
        uint8_t raw = reader.ReadByte();
        return ClampInt(static_cast<int>(raw), 0, 100);
    } catch (...) {
        return -1;
    }
}

// Windows' own Settings > Bluetooth & devices page shows a battery percentage
// for most classic (non-BLE) headphones/earbuds/speakers using an
// undocumented per-devnode property exposed by the Microsoft Bluetooth
// classic driver stack — not GATT. This is the same property those battery
// tray-icon utilities read. Query it via SetupAPI on the Bluetooth-class
// devnode whose instance ID embeds the device's Bluetooth address.
static const GUID kGuidDevClassBluetooth = {
    0xe0cbf06c, 0xcd8b, 0x4647, {0xbb, 0x8a, 0x26, 0x3b, 0x43, 0xf0, 0xf9, 0x74}};

static const DEVPROPKEY PKEY_Bluetooth_Battery = {
    {0x104ea319, 0x6ee2, 0x4701, {0xbd, 0x47, 0x8d, 0xdb, 0xf4, 0x25, 0xbb, 0xe5}}, 2};

inline int TryReadClassicBluetoothBatteryPercent(uint64_t address) {
    if (!address) {
        Wh_Log(L"BT battery: no address to search for.");
        return -1;
    }

    wchar_t addrHex[16] = {};
    swprintf_s(addrHex, L"%012llX", static_cast<unsigned long long>(address));
    const std::wstring addrLower = ToLowerCopy(addrHex);
    Wh_Log(L"BT battery: searching devnodes for address %s", addrLower.c_str());

    // Enumerate everything the Bluetooth bus driver (BTHENUM) exposes,
    // regardless of which device setup class it landed in. This is broader
    // than filtering by GUID_DEVCLASS_BLUETOOTH and matches what battery
    // tray utilities do.
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsExW(
        nullptr, L"BTHENUM", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT,
        nullptr, nullptr, nullptr);

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        Wh_Log(L"BT battery: SetupDiGetClassDevsExW(BTHENUM) failed (0x%lx), falling back to class GUID.", GetLastError());
        deviceInfoSet = SetupDiGetClassDevsW(&kGuidDevClassBluetooth, nullptr, nullptr, DIGCF_PRESENT);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) {
            Wh_Log(L"BT battery: fallback enumeration also failed.");
            return -1;
        }
    }

    int result = -1;
    int matchedCount = 0;
    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(devInfoData);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &devInfoData); ++i) {
        wchar_t instanceId[512] = {};
        if (!SetupDiGetDeviceInstanceIdW(deviceInfoSet, &devInfoData, instanceId,
                                         ARRAYSIZE(instanceId), nullptr)) {
            continue;
        }

        if (ToLowerCopy(instanceId).find(addrLower) == std::wstring::npos) {
            continue;
        }

        ++matchedCount;
        Wh_Log(L"BT battery: matched devnode %s", instanceId);

        DEVPROPTYPE propType = 0;
        BYTE battery = 0;
        DWORD required = 0;
        if (SetupDiGetDevicePropertyW(deviceInfoSet, &devInfoData, &PKEY_Bluetooth_Battery,
                                      &propType, &battery, sizeof(battery), &required, 0)) {
            Wh_Log(L"BT battery: property present on this devnode, type=%lu value=%u", propType, battery);
            if (propType == DEVPROP_TYPE_BYTE && battery != 0xFF) {
                result = ClampInt(static_cast<int>(battery), 0, 100);
                break;
            }
        } else {
            Wh_Log(L"BT battery: PKEY_Bluetooth_Battery not set on this devnode yet (error 0x%lx).", GetLastError());
        }
    }

    if (matchedCount == 0) {
        Wh_Log(L"BT battery: no devnode instance ID contained address %s.", addrLower.c_str());
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return result;
}

inline int TryReadBluetoothBatteryPercent(winrt::hstring const& deviceId) {
    int result = -1;

    // Try the classic per-devnode battery property first — this is what
    // Settings > Bluetooth & devices reads, and it's what covers most
    // headsets/earbuds/speakers that pair over classic Bluetooth (BR/EDR)
    // rather than BLE.
    try {
        using winrt::Windows::Devices::Bluetooth::BluetoothDevice;
        auto classicDevice = BluetoothDevice::FromIdAsync(deviceId).get();
        if (classicDevice) {
            result = TryReadClassicBluetoothBatteryPercent(classicDevice.BluetoothAddress());
        }
    } catch (...) {
        // Not resolvable as a classic BluetoothDevice (BLE-only peripheral) — fall through.
    }

    if (result < 0) {
        // Fall back to BLE GATT Battery Service for BLE / dual-mode devices.
        result = TryReadBluetoothBatteryPercentBLE(deviceId);
    }

    if (result >= 0) {
        // Remember this reading so a later disconnect (when the device can
        // no longer be queried) can still show the last known level.
        std::lock_guard lock(g_bluetoothBatteryCacheMutex);
        const std::wstring devKey(deviceId.c_str());
        g_bluetoothBatteryCache[devKey] = result;
        auto itAcc = g_bluetoothConnectedAccessories.find(devKey);
        if (itAcc != g_bluetoothConnectedAccessories.end()) {
            itAcc->second.batteryPercent = result;
        }
    }

    return result;
}

// Returns the last battery percent we successfully read for this device
// while it was connected, or -1 if we never learned one.
inline int GetLastKnownBluetoothBatteryPercent(const std::wstring& deviceId) {
    std::lock_guard lock(g_bluetoothBatteryCacheMutex);
    auto it = g_bluetoothBatteryCache.find(deviceId);
    return it != g_bluetoothBatteryCache.end() ? it->second : -1;
}

// Small per-device cache so a Removed event (which only carries an Id, not
// a full DeviceInformation) can still show a name/icon on disconnect.
struct BluetoothTrackedDevice {
    std::wstring name;
    BluetoothDeviceCategory category;
};

inline void HandleBluetoothConnected(
    const winrt::Windows::Devices::Enumeration::DeviceInformation& info,
    std::unordered_map<std::wstring, BluetoothTrackedDevice>& cache,
    std::mutex& cacheMutex) {
    std::wstring name = info.Name().c_str();
    if (name.empty()) {
        return;
    }

    BluetoothDeviceCategory category = ClassifyBluetoothDevice(info);
    {
        std::lock_guard lock(cacheMutex);
        cache[std::wstring(info.Id().c_str())] = BluetoothTrackedDevice{name, category};
    }
    {
        std::lock_guard lock(g_bluetoothBatteryCacheMutex);
        auto& acc = g_bluetoothConnectedAccessories[std::wstring(info.Id().c_str())];
        acc.name = name;
        acc.category = category;
        acc.connected = true;
        auto itB = g_bluetoothBatteryCache.find(std::wstring(info.Id().c_str()));
        if (itB != g_bluetoothBatteryCache.end()) {
            acc.batteryPercent = itB->second;
        }
    }

    if (!g_settings.bluetoothIndicator) {
        return;
    }

    Wh_Log(L"Bluetooth: connected - %s", name.c_str());

    const uint64_t myGeneration = ++g_bluetoothConnectGeneration;
    const std::wstring deviceId = info.Id().c_str();

    BluetoothDeviceSnapshot snapshot;
    snapshot.active = true;
    snapshot.connected = true;
    snapshot.deviceName = name;
    snapshot.category = category;
    snapshot.expiresAt = NowSeconds() + 4.0;
    snapshot.batteryPercent = g_settings.bluetoothShowBattery
        ? TryReadBluetoothBatteryPercent(info.Id())
        : -1;

    {
        std::lock_guard lock(g_stateMutex);
        g_state.bluetoothDevice = std::move(snapshot);
    }
    TriggerNudge();

    // Windows often hasn't populated the battery property at the exact
    // instant the connection event fires — it needs a moment to actually
    // query the device. Keep retrying in the background for a while; if a
    // value shows up and this connection is still the one being displayed,
    // patch it into the live state and re-render.
    if (g_settings.bluetoothShowBattery && snapshot.batteryPercent < 0) {
        std::thread([deviceId, myGeneration]() {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            for (int attempt = 0; attempt < 6; ++attempt) {
                Sleep(1500);
                if (g_bluetoothConnectGeneration.load() != myGeneration) {
                    break;  // a newer connect/disconnect event superseded this one
                }

                int battery = TryReadBluetoothBatteryPercent(winrt::hstring(deviceId));
                if (battery >= 0) {
                    std::lock_guard lock(g_stateMutex);
                    if (g_bluetoothConnectGeneration.load() == myGeneration &&
                        g_state.bluetoothDevice.connected) {
                        g_state.bluetoothDevice.batteryPercent = battery;
                        Wh_Log(L"Bluetooth: battery arrived late (%d%%) on retry %d.", battery, attempt + 1);
                    }
                    TriggerNudge();
                    break;
                }
            }
            winrt::uninit_apartment();
        }).detach();
    }
}

inline void HandleBluetoothDisconnected(
    winrt::hstring const& id,
    std::unordered_map<std::wstring, BluetoothTrackedDevice>& cache,
    std::mutex& cacheMutex) {
    ++g_bluetoothConnectGeneration;  // cancel any pending battery retry for the old connection

    if (!g_settings.bluetoothIndicator) {
        return;
    }

    std::wstring name;
    BluetoothDeviceCategory category = BluetoothDeviceCategory::Generic;
    {
        std::lock_guard lock(cacheMutex);
        auto it = cache.find(std::wstring(id.c_str()));
        if (it != cache.end()) {
            name = it->second.name;
            category = it->second.category;
        }
    }
    if (name.empty()) {
        name = L"Bluetooth Device";
    }

    Wh_Log(L"Bluetooth: disconnected - %s", name.c_str());

    {
        std::lock_guard lock(g_bluetoothBatteryCacheMutex);
        auto itAcc = g_bluetoothConnectedAccessories.find(std::wstring(id.c_str()));
        if (itAcc != g_bluetoothConnectedAccessories.end()) {
            itAcc->second.connected = false;
        }
    }

    BluetoothDeviceSnapshot snapshot;
    snapshot.active = true;
    snapshot.connected = false;
    snapshot.deviceName = name;
    snapshot.category = category;
    snapshot.batteryPercent = GetLastKnownBluetoothBatteryPercent(std::wstring(id.c_str()));
    snapshot.expiresAt = NowSeconds() + 4.0;

    {
        std::lock_guard lock(g_stateMutex);
        g_state.bluetoothDevice = std::move(snapshot);
    }
    TriggerNudge();
}

inline DWORD WINAPI BluetoothThreadProc(void*) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    using winrt::Windows::Devices::Enumeration::DeviceInformation;
    using winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;
    using winrt::Windows::Devices::Enumeration::DeviceWatcher;
    using winrt::Windows::Devices::Bluetooth::BluetoothDevice;
    using winrt::Windows::Devices::Bluetooth::BluetoothLEDevice;
    using winrt::Windows::Devices::Bluetooth::BluetoothConnectionStatus;

    std::unordered_map<std::wstring, BluetoothTrackedDevice> deviceCache;
    std::mutex cacheMutex;
    std::vector<DeviceWatcher> watchers;

    // Watching the "Connected" selector directly means a device APPEARING
    // in the watcher (Added) is a connect, and DISAPPEARING (Removed) is a
    // disconnect — no property polling or IsConnected lookups required.
    auto startWatcher = [&](winrt::hstring const& selector, const wchar_t* label) {
        try {
            DeviceWatcher watcher = DeviceInformation::CreateWatcher(selector);
            auto enumDone = std::make_shared<std::atomic<bool>>(false);

            watcher.EnumerationCompleted(
                [enumDone](DeviceWatcher const&, winrt::Windows::Foundation::IInspectable const&) {
                    *enumDone = true;
                });

            watcher.Added([&deviceCache, &cacheMutex, enumDone](
                              DeviceWatcher const&, DeviceInformation const& info) {
                // Devices reported before EnumerationCompleted are the
                // watcher's initial snapshot (already connected when the mod
                // started) — cache them silently so a later disconnect still
                // resolves a name, but don't pop a card for a connection the
                // user didn't just cause.
                if (!*enumDone) {
                    std::wstring name = info.Name().c_str();
                    if (!name.empty()) {
                        BluetoothDeviceCategory cat = ClassifyBluetoothDevice(info);
                        {
                            std::lock_guard lock(cacheMutex);
                            deviceCache[std::wstring(info.Id().c_str())] =
                                BluetoothTrackedDevice{name, cat};
                        }
                        {
                            std::lock_guard lock(g_bluetoothBatteryCacheMutex);
                            auto& acc = g_bluetoothConnectedAccessories[std::wstring(info.Id().c_str())];
                            acc.name = name;
                            acc.category = cat;
                            acc.connected = true;
                            auto itB = g_bluetoothBatteryCache.find(std::wstring(info.Id().c_str()));
                            if (itB != g_bluetoothBatteryCache.end()) {
                                acc.batteryPercent = itB->second;
                            }
                        }
                    }
                    return;
                }
                HandleBluetoothConnected(info, deviceCache, cacheMutex);
            });

            watcher.Removed([&deviceCache, &cacheMutex](
                                DeviceWatcher const&, DeviceInformationUpdate const& update) {
                HandleBluetoothDisconnected(update.Id(), deviceCache, cacheMutex);
            });

            watcher.Start();
            watchers.push_back(watcher);
            Wh_Log(L"Bluetooth: %s watcher started.", label);
        } catch (...) {
            Wh_Log(L"Bluetooth: failed to start %s watcher.", label);
        }
    };

    startWatcher(
        BluetoothDevice::GetDeviceSelectorFromConnectionStatus(BluetoothConnectionStatus::Connected),
        L"classic");
    startWatcher(
        BluetoothLEDevice::GetDeviceSelectorFromConnectionStatus(BluetoothConnectionStatus::Connected),
        L"BLE");

    // DeviceWatcher does its work via WinRT callbacks on background threads;
    // this thread just needs to stay alive to keep the watchers rooted
    // until shutdown is signaled.
    while (WaitForSingleObject(g_stopEvent, 1000) == WAIT_TIMEOUT) {
    }

    for (auto& watcher : watchers) {
        try {
            watcher.Stop();
        } catch (...) {
        }
    }

    winrt::uninit_apartment();
    return 0;
}

#else  // !DYNAMIC_ISLAND_HAS_BLUETOOTH_WATCHER

inline DWORD WINAPI BluetoothThreadProc(void*) {
    // SDK used to build this mod doesn't expose the WinRT Bluetooth headers;
    // the indicator silently stays inactive instead of failing the mod.
    return 0;
}

#endif  // DYNAMIC_ISLAND_HAS_BLUETOOTH_WATCHER


#endif // BLUETOOTH_DND_ENGINE_HPP
