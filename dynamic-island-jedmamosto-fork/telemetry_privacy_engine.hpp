#pragma once

#ifndef TELEMETRY_PRIVACY_ENGINE_HPP
#define TELEMETRY_PRIVACY_ENGINE_HPP

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
#include <pdh.h>
#include <pdhmsg.h>
#include <powrprof.h>
#include <poclass.h>
#include <batclass.h>
#include <setupapi.h>
#include <devguid.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <cstdint>

#include "island_common.hpp"
#include "icon_process_engine.hpp"

// Forward declarations for external symbols defined in the main module
extern HWND g_hwnd;
extern FILETIME g_prevIdleTime;
extern FILETIME g_prevKernelTime;
extern FILETIME g_prevUserTime;
extern bool g_volumeInitialized;

void TriggerNudge();
bool IsIgnorableForegroundWindow(HWND hwnd, const std::wstring& title);

inline ULONGLONG FileTimeToUInt64(FILETIME ft) {
    ULARGE_INTEGER value = {};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return value.QuadPart;
}

// ============================================================================
// PDH Performance Queries (GPU, Network, Disk)
// ============================================================================

inline PDH_HQUERY g_gpuQuery = NULL;
inline PDH_HCOUNTER g_gpuCounter = NULL;

inline void InitGpuQuery() {
    if (g_gpuQuery == NULL) {
        if (PdhOpenQueryW(NULL, 0, &g_gpuQuery) == ERROR_SUCCESS) {
            PdhAddEnglishCounterW(g_gpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &g_gpuCounter);
            PdhCollectQueryData(g_gpuQuery);
        }
    }
}

inline int GetGpuUsage() {
    InitGpuQuery();
    if (!g_gpuQuery || !g_gpuCounter) return 0;

    PdhCollectQueryData(g_gpuQuery);

    DWORD bufferSize = 0;
    DWORD itemCount = 0;
    PdhGetFormattedCounterArrayW(g_gpuCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, NULL);

    if (bufferSize > 0) {
        std::vector<BYTE> buffer(bufferSize);
        PDH_FMT_COUNTERVALUE_ITEM_W* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());

        if (PdhGetFormattedCounterArrayW(g_gpuCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items) == ERROR_SUCCESS) {
            double total = 0;
            for (DWORD i = 0; i < itemCount; i++) {
                if (items[i].szName && wcsstr(items[i].szName, L"engtype_3D")) {
                    total += items[i].FmtValue.doubleValue;
                }
            }
            return ClampInt(static_cast<int>(total), 0, 100);
        }
    }
    return 0;
}

inline PDH_HQUERY g_netQuery = NULL;
inline PDH_HCOUNTER g_netUpCounter = NULL;
inline PDH_HCOUNTER g_netDownCounter = NULL;

inline void InitNetQuery() {
    if (g_netQuery == NULL) {
        if (PdhOpenQueryW(NULL, 0, &g_netQuery) == ERROR_SUCCESS) {
            PdhAddEnglishCounterW(g_netQuery, L"\\Network Interface(*)\\Bytes Sent/sec", 0, &g_netUpCounter);
            PdhAddEnglishCounterW(g_netQuery, L"\\Network Interface(*)\\Bytes Received/sec", 0, &g_netDownCounter);
            PdhCollectQueryData(g_netQuery);
        }
    }
}

inline void GetNetworkUsage(float& outUpMbps, float& outDownMbps) {
    outUpMbps = 0.0f;
    outDownMbps = 0.0f;
    InitNetQuery();
    if (!g_netQuery || !g_netUpCounter || !g_netDownCounter) return;

    PdhCollectQueryData(g_netQuery);

    auto getSum = [](PDH_HCOUNTER counter) -> double {
        DWORD bufferSize = 0;
        DWORD itemCount = 0;
        PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, NULL);
        if (bufferSize > 0) {
            std::vector<BYTE> buffer(bufferSize);
            PDH_FMT_COUNTERVALUE_ITEM_W* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
            if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items) == ERROR_SUCCESS) {
                double total = 0;
                for (DWORD i = 0; i < itemCount; i++) {
                    if (items[i].szName) {
                        if (wcsstr(items[i].szName, L"Loopback") == nullptr) {
                            total += items[i].FmtValue.doubleValue;
                        }
                    }
                }
                return total;
            }
        }
        return 0.0;
    };

    // Bytes to Mbps
    outUpMbps = static_cast<float>(getSum(g_netUpCounter) * 8.0 / 1000000.0);
    outDownMbps = static_cast<float>(getSum(g_netDownCounter) * 8.0 / 1000000.0);
}

inline PDH_HQUERY g_diskQuery = NULL;
inline PDH_HCOUNTER g_diskCounter = NULL;
inline bool g_diskQueryFailed = false;
inline double g_smoothedDiskPercent = -1.0;

inline void InitDiskQuery() {
    if (g_diskQuery == NULL && !g_diskQueryFailed) {
        if (PdhOpenQueryW(NULL, 0, &g_diskQuery) == ERROR_SUCCESS) {
            PDH_STATUS status = PdhAddEnglishCounterW(g_diskQuery, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &g_diskCounter);
            if (status != ERROR_SUCCESS) {
                status = PdhAddCounterW(g_diskQuery, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &g_diskCounter);
            }
            if (status == ERROR_SUCCESS) {
                PdhCollectQueryData(g_diskQuery);
            } else {
                PdhCloseQuery(g_diskQuery);
                g_diskQuery = NULL;
                g_diskCounter = NULL;
                g_diskQueryFailed = true;
            }
        } else {
            g_diskQueryFailed = true;
        }
    }
}

inline int GetDiskUsage(int fallbackStoragePercent) {
    InitDiskQuery();
    if (!g_diskQuery || !g_diskCounter) {
        return fallbackStoragePercent;
    }

    if (PdhCollectQueryData(g_diskQuery) == ERROR_SUCCESS) {
        PDH_FMT_COUNTERVALUE counterVal = {};
        if (PdhGetFormattedCounterValue(g_diskCounter, PDH_FMT_DOUBLE, NULL, &counterVal) == ERROR_SUCCESS) {
            if (counterVal.CStatus == PDH_CSTATUS_VALID_DATA || counterVal.CStatus == PDH_CSTATUS_NEW_DATA) {
                const double raw = Clamp(static_cast<float>(counterVal.doubleValue), 0.0f, 100.0f);
                if (g_smoothedDiskPercent < 0.0) {
                    g_smoothedDiskPercent = raw;
                } else {
                    // Smooth sample transitions: 70% new sample, 30% previous sample
                    g_smoothedDiskPercent = 0.70 * raw + 0.30 * g_smoothedDiskPercent;
                }
                return ClampInt(static_cast<int>(std::round(g_smoothedDiskPercent)), 0, 100);
            }
        }
    }
    return fallbackStoragePercent;
}

// ============================================================================
// Battery Telemetry & IOCTL Queries
// ============================================================================

inline void UpdateBatterySnapshot() {
    SYSTEM_POWER_STATUS status = {};
    if (!GetSystemPowerStatus(&status)) {
        return;
    }

    const bool newCharging = (status.ACLineStatus == 1);
    const int newPercent = (status.BatteryLifePercent == 255) ? 100 : status.BatteryLifePercent;
    const int newSecondsRemaining = (status.BatteryLifeTime != static_cast<DWORD>(-1) && status.BatteryLifeTime != 0xFFFFFFFF)
        ? static_cast<int>(status.BatteryLifeTime) : -1;
    int newSecondsToFull = (status.BatteryFullLifeTime != static_cast<DWORD>(-1) && status.BatteryFullLifeTime != 0xFFFFFFFF && status.BatteryFullLifeTime > 0)
        ? static_cast<int>(status.BatteryFullLifeTime) : -1;

    // Active power scheme name
    std::wstring schemeName = L"Balanced";
    GUID* pActiveScheme = nullptr;
    if (PowerGetActiveScheme(nullptr, &pActiveScheme) == ERROR_SUCCESS && pActiveScheme) {
        UCHAR schemeBuf[512] = {};
        DWORD schemeBufSize = sizeof(schemeBuf);
        if (PowerReadFriendlyName(nullptr, pActiveScheme, nullptr, nullptr, schemeBuf, &schemeBufSize) == ERROR_SUCCESS && schemeBufSize > 0) {
            const wchar_t* readName = reinterpret_cast<const wchar_t*>(schemeBuf);
            if (readName && wcslen(readName) > 0) {
                schemeName = readName;
            }
        }
        LocalFree(pActiveScheme);
    }

    // Hardware query via battery IOCTLs
    int newHealthPercent = -1;
    int newCycleCount = -1;
    float newPowerRateWatts = 0.0f;

    // Battery device interface GUID: {72631e54-78a4-11d0-bcf7-00aa00b7b32a}
    static const GUID kBatteryGuid = { 0x72631e54, 0x78a4, 0x11d0, { 0xbc, 0xf7, 0x00, 0xaa, 0x00, 0xb7, 0xb3, 0x2a } };

    if (!(status.BatteryFlag & 128)) {
        HDEVINFO hdev = SetupDiGetClassDevs(&kBatteryGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (hdev != INVALID_HANDLE_VALUE) {
            SP_DEVICE_INTERFACE_DATA did = {};
            did.cbSize = sizeof(did);
            if (SetupDiEnumDeviceInterfaces(hdev, nullptr, &kBatteryGuid, 0, &did)) {
                DWORD cbRequired = 0;
                SetupDiGetDeviceInterfaceDetail(hdev, &did, nullptr, 0, &cbRequired, nullptr);
                if (cbRequired > 0) {
                    std::vector<BYTE> detailBuf(cbRequired);
                    PSP_DEVICE_INTERFACE_DETAIL_DATA pdid = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(detailBuf.data());
                    pdid->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
                    if (SetupDiGetDeviceInterfaceDetail(hdev, &did, pdid, cbRequired, &cbRequired, nullptr)) {
                        HANDLE hBattery = CreateFile(pdid->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                        if (hBattery != INVALID_HANDLE_VALUE) {
                            BATTERY_QUERY_INFORMATION bqi = {};
                            DWORD dwWait = 0;
                            DWORD dwBytes = 0;
                            if (DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_TAG, &dwWait, sizeof(dwWait),
                                                &bqi.BatteryTag, sizeof(bqi.BatteryTag), &dwBytes, nullptr) &&
                                bqi.BatteryTag != BATTERY_TAG_INVALID) {
                                
                                bqi.InformationLevel = BatteryInformation;
                                BATTERY_INFORMATION bi = {};
                                if (DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi),
                                                    &bi, sizeof(bi), &dwBytes, nullptr)) {
                                    if (bi.DesignedCapacity > 0 && bi.FullChargedCapacity > 0) {
                                        newHealthPercent = ClampInt(
                                            static_cast<int>((static_cast<uint64_t>(bi.FullChargedCapacity) * 100ULL) / bi.DesignedCapacity),
                                            0, 100);
                                    }
                                    if (bi.CycleCount != 0 && bi.CycleCount != 0xFFFFFFFF) {
                                        newCycleCount = static_cast<int>(bi.CycleCount);
                                    }
                                }

                                BATTERY_WAIT_STATUS bws = {};
                                bws.BatteryTag = bqi.BatteryTag;
                                BATTERY_STATUS bs = {};
                                if (DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_STATUS, &bws, sizeof(bws),
                                                    &bs, sizeof(bs), &dwBytes, nullptr)) {
                                    if (bs.Rate != static_cast<LONG>(BATTERY_UNKNOWN_RATE) && bs.Rate != 0) {
                                        float rateMw = static_cast<float>(bs.Rate);
                                        if ((bi.Capabilities & BATTERY_CAPACITY_RELATIVE) && bs.Voltage != BATTERY_UNKNOWN_VOLTAGE && bs.Voltage > 0) {
                                            rateMw = (static_cast<float>(bs.Rate) * static_cast<float>(bs.Voltage)) / 1000.0f;
                                        }
                                        newPowerRateWatts = rateMw / 1000.0f;
                                        if (status.ACLineStatus == 0 && newPowerRateWatts > 0.0f) {
                                            newPowerRateWatts = -newPowerRateWatts;
                                        } else if (status.ACLineStatus == 1 && newPowerRateWatts < 0.0f) {
                                            newPowerRateWatts = -newPowerRateWatts;
                                        }
                                    }

                                    if (newSecondsToFull < 0 && status.ACLineStatus == 1 && bi.FullChargedCapacity > bs.Capacity && bs.Rate > 0) {
                                        const uint64_t needed = bi.FullChargedCapacity - bs.Capacity;
                                        newSecondsToFull = static_cast<int>((needed * 3600ULL) / static_cast<uint64_t>(bs.Rate));
                                    }
                                }
                            }
                            CloseHandle(hBattery);
                        }
                    }
                }
            }
            SetupDiDestroyDeviceInfoList(hdev);
        }
    }

    if (newCharging && newPercent >= 100) {
        newSecondsToFull = 0;
    }

    bool triggerAlert = false;
    {
        std::lock_guard lock(g_stateMutex);
        static bool s_batteryInit = false;
        if (!s_batteryInit) {
            g_state.battery.charging = newCharging;
            g_state.battery.percent = newPercent;
            s_batteryInit = true;
        }

        if (g_state.battery.charging != newCharging) {
            triggerAlert = true;
        }

        if (!newCharging && newPercent < g_state.battery.percent && (newPercent == 20 || newPercent == 10)) {
            triggerAlert = true;
        }

        g_state.battery.charging = newCharging;
        g_state.battery.percent = newPercent;
        g_state.battery.secondsRemaining = newSecondsRemaining;
        g_state.battery.secondsToFull = newSecondsToFull;
        g_state.battery.powerRateWatts = newPowerRateWatts;
        g_state.battery.healthPercent = newHealthPercent;
        g_state.battery.cycleCount = newCycleCount;
        g_state.battery.powerSchemeName = schemeName;
        g_state.battery.low = (!newCharging && newPercent <= 20);

        if (triggerAlert) {
            g_state.battery.active = true;
            g_state.battery.expiresAt = NowSeconds() + 4.0;
        }
    }

    if (triggerAlert) {
        TriggerNudge();
    }
}

// ============================================================================
// Windows Privacy Indicators & Registry Queries
// ============================================================================

// CleanActiveAppName is defined in icon_process_engine.hpp

inline bool ReadRegQword(HKEY key, const wchar_t* name, uint64_t* out) {
    DWORD type = 0;
    uint64_t value = 0;
    DWORD dataSize = sizeof(value);
    if (RegQueryValueExW(key, name, nullptr, &type,
                         reinterpret_cast<LPBYTE>(&value), &dataSize) != ERROR_SUCCESS) {
        return false;
    }
    if (type != REG_QWORD || dataSize != sizeof(value)) {
        return false;
    }
    *out = value;
    return true;
}

inline bool ConsentStoreEntryInUse(HKEY key) {
    uint64_t stopTime = 0;
    if (!ReadRegQword(key, L"LastUsedTimeStop", &stopTime) || stopTime != 0) {
        return false;
    }

    uint64_t startTime = 0;
    if (ReadRegQword(key, L"LastUsedTimeStart", &startTime)) {
        return startTime != 0;
    }

    static std::atomic<bool> loggedMissingStart{false};
    if (!loggedMissingStart.exchange(true)) {
        Wh_Log(L"Privacy indicator: a ConsentStore entry has LastUsedTimeStop=0 but no "
               L"readable REG_QWORD LastUsedTimeStart; treating it as not in use.");
    }
    return false;
}

inline bool IsDeviceActiveViaRegistry(const wchar_t* capability, std::wstring* outAppName = nullptr) {
    bool isActive = false;
    std::wstring basePath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\";
    basePath += capability;

    auto CheckSubkeys = [&](HKEY hKeyParent) -> bool {
        DWORD index = 0;
        wchar_t subKeyName[256];
        DWORD nameLen = ARRAYSIZE(subKeyName);
        while (RegEnumKeyExW(hKeyParent, index, subKeyName, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            HKEY hSub;
            if (RegOpenKeyExW(hKeyParent, subKeyName, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                if (_wcsicmp(subKeyName, L"NonPackaged") == 0) {
                    DWORD npIndex = 0;
                    wchar_t npSubKeyName[256];
                    DWORD npNameLen = ARRAYSIZE(npSubKeyName);
                    while (RegEnumKeyExW(hSub, npIndex, npSubKeyName, &npNameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                        HKEY hNpSub;
                        if (RegOpenKeyExW(hSub, npSubKeyName, 0, KEY_READ, &hNpSub) == ERROR_SUCCESS) {
                            if (ConsentStoreEntryInUse(hNpSub)) {
                                if (outAppName && outAppName->empty()) {
                                    *outAppName = CleanActiveAppName(npSubKeyName);
                                }
                                RegCloseKey(hNpSub);
                                RegCloseKey(hSub);
                                return true;
                            }
                            RegCloseKey(hNpSub);
                        }
                        npIndex++;
                        npNameLen = ARRAYSIZE(npSubKeyName);
                    }
                } else {
                    if (ConsentStoreEntryInUse(hSub)) {
                        if (outAppName && outAppName->empty()) {
                            *outAppName = CleanActiveAppName(subKeyName);
                        }
                        RegCloseKey(hSub);
                        return true;
                    }
                }
                RegCloseKey(hSub);
            }
            index++;
            nameLen = ARRAYSIZE(subKeyName);
        }
        return false;
    };

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, basePath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        isActive = CheckSubkeys(hKey);
        RegCloseKey(hKey);
    }

    if (!isActive) {
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, basePath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            isActive = CheckSubkeys(hKey);
            RegCloseKey(hKey);
        }
    }

    return isActive;
}

inline bool IsMicrophoneActive(std::wstring* outAppName = nullptr) {
    return IsDeviceActiveViaRegistry(L"microphone", outAppName);
}

inline bool IsCameraActive(std::wstring* outAppName = nullptr) {
    return IsDeviceActiveViaRegistry(L"webcam", outAppName);
}

inline void UpdatePrivacyIndicators() {
    std::wstring micApp;
    std::wstring camApp;
    const bool mic = (g_settings.privacyDots && g_settings.privacyDotsMic) ? IsMicrophoneActive(&micApp) : false;
    const bool cam = (g_settings.privacyDots && g_settings.privacyDotsCam) ? IsCameraActive(&camApp) : false;
    std::lock_guard lock(g_stateMutex);
    g_state.system.micActive = mic;
    g_state.system.cameraActive = cam;
    g_state.system.micApp = micApp;
    g_state.system.cameraApp = camApp;
}

inline void UpdateProgressSnapshot() {
    const int progress = Wh_GetIntValue(L"ProgressPercent", -1);
    std::lock_guard lock(g_stateMutex);
    g_state.progress.active = progress >= 0 && progress <= 100;
    g_state.progress.percent = ClampInt(progress, 0, 100);
}

// ============================================================================
// Core System Snapshot Aggregator (CPU, RAM, Disk, Audio, Foreground)
// ============================================================================

inline void UpdateSystemSnapshot(bool includeGpuStats, bool includeNetStats) {
    SystemSnapshot next;
    {
        std::lock_guard lock(g_stateMutex);
        next = g_state.system;
        next.charging = g_state.system.charging;
    }

    // GPU/network sampling is comparatively expensive, so only sample when displayed
    if (includeGpuStats) {
        next.gpuPercent = GetGpuUsage();
    }
    if (includeNetStats) {
        GetNetworkUsage(next.netUpMbps, next.netDownMbps);
    }

    MEMORYSTATUSEX memory = {};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        next.memoryPercent = static_cast<int>(memory.dwMemoryLoad);
        next.memoryTotalGB = static_cast<float>(memory.ullTotalPhys) / (1024.0f * 1024.0f * 1024.0f);
        next.memoryUsedGB = next.memoryTotalGB - static_cast<float>(memory.ullAvailPhys) / (1024.0f * 1024.0f * 1024.0f);
    }

    ULARGE_INTEGER freeBytesAvailable = {};
    ULARGE_INTEGER totalBytes = {};
    ULARGE_INTEGER totalFreeBytes = {};
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes) &&
        totalBytes.QuadPart > 0) {
        next.diskFreePercent = ClampInt(
            static_cast<int>(totalFreeBytes.QuadPart * 100 / totalBytes.QuadPart), 0, 100);
    }
    const int storageUsedPercent = 100 - next.diskFreePercent;
    next.diskPercent = GetDiskUsage(storageUsedPercent);

    HWND foreground = GetForegroundWindow();
    if (foreground && foreground != g_hwnd) {
        wchar_t title[96] = {};
        GetWindowTextW(foreground, title, ARRAYSIZE(title));
        if (!IsIgnorableForegroundWindow(foreground, title)) {
            next.foregroundTitle = title;
            if (next.foregroundTitle.size() > 42) {
                next.foregroundTitle.resize(42);
                next.foregroundTitle += L"...";
            }
        } else {
            next.foregroundTitle.clear();
        }
    }

    FILETIME idle = {};
    FILETIME kernel = {};
    FILETIME user = {};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        const ULONGLONG idleNow = FileTimeToUInt64(idle);
        const ULONGLONG kernelNow = FileTimeToUInt64(kernel);
        const ULONGLONG userNow = FileTimeToUInt64(user);
        const ULONGLONG idlePrev = FileTimeToUInt64(g_prevIdleTime);
        const ULONGLONG kernelPrev = FileTimeToUInt64(g_prevKernelTime);
        const ULONGLONG userPrev = FileTimeToUInt64(g_prevUserTime);

        const ULONGLONG total = (kernelNow - kernelPrev) + (userNow - userPrev);
        const ULONGLONG idleDelta = idleNow - idlePrev;
        if (total > 0 && kernelPrev != 0) {
            next.cpuPercent = ClampInt(static_cast<int>((total - idleDelta) * 100 / total), 0, 100);
        }

        g_prevIdleTime = idle;
        g_prevKernelTime = kernel;
        g_prevUserTime = user;
    }

    static Microsoft::WRL::ComPtr<IAudioEndpointVolume> s_volume;
    if (!s_volume) {
        Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
        Microsoft::WRL::ComPtr<IMMDevice> device;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
        if (SUCCEEDED(hr)) {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        }
        if (SUCCEEDED(hr)) {
            hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(s_volume.GetAddressOf()));
        }
    }

    if (s_volume) {
        float level = 0.0f;
        BOOL muted = FALSE;
        if (SUCCEEDED(s_volume->GetMasterVolumeLevelScalar(&level)) && SUCCEEDED(s_volume->GetMute(&muted))) {
            next.volumePercent = ClampInt(static_cast<int>(level * 100.0f + 0.5f), 0, 100);
            next.volumeMuted = muted != FALSE;
        } else {
            s_volume.Reset();
        }
    }

    std::lock_guard lock(g_stateMutex);
    const bool volumeChanged =
        g_volumeInitialized &&
        (std::abs(next.volumePercent - g_state.system.volumePercent) >= 2 ||
         next.volumeMuted != g_state.system.volumeMuted);
    g_state.system = next;
    g_state.muted = next.volumeMuted;
    if (volumeChanged && g_settings.volume) {
        g_state.volume.active = true;
        g_state.volume.percent = next.volumePercent;
        g_state.volume.muted = next.volumeMuted;
        g_state.volume.deviceName = L"System audio";
        g_state.volume.expiresAt = NowSeconds() + 1.8;
        TriggerNudge();
    }
    g_volumeInitialized = true;
}

#endif // TELEMETRY_PRIVACY_ENGINE_HPP
