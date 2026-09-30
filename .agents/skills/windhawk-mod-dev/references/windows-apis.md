# Windows System APIs & Sensor Recipes

## 1. Real-Time Disk Activity (PDH)

### Problem
`GetDiskFreeSpaceExW` measures static disk capacity (e.g. SSD 78% full). It never changes with disk read/write throughput.

### Solution: Performance Data Helper (PDH)
Link `-lpdh` and query the English counter `\PhysicalDisk(_Total)\% Disk Time`:

```cpp
#include <pdh.h>
#include <pdhmsg.h>

static PDH_HQUERY g_pdhQuery = nullptr;
static PDH_HCOUNTER g_diskCounter = nullptr;

bool InitDiskCounter() {
    if (PdhOpenQueryW(nullptr, 0, &g_pdhQuery) != ERROR_SUCCESS) return false;
    PDH_STATUS status = PdhAddEnglishCounterW(
        g_pdhQuery, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &g_diskCounter);
    if (status != ERROR_SUCCESS) return false;
    PdhCollectQueryData(g_pdhQuery); // Prime first sample
    return true;
}

int SampleDiskActivePercent() {
    if (!g_pdhQuery || !g_diskCounter) return 0;
    if (PdhCollectQueryData(g_pdhQuery) != ERROR_SUCCESS) return 0;
    PDH_FMT_COUNTERVALUE val;
    if (PdhGetFormattedCounterValue(g_diskCounter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
        return static_cast<int>(std::clamp(val.doubleValue, 0.0, 100.0));
    }
    return 0;
}
```

---

## 2. Universal WinRT Notifications (Discord, Chrome, Messenger)

### Problem
Legacy balloon notification hooks miss modern Windows 10/11 desktop toast notifications.

### Solution: `UserNotificationListener`
Use WinRT `Windows.UI.Notifications.Management.UserNotificationListener`:

```cpp
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.UI.Notifications.Management.h>

using namespace winrt::Windows::UI::Notifications::Management;

void SubscribePushNotifications() {
    auto listener = UserNotificationListener::Current();
    auto status = listener.RequestAccessAsync().get();
    if (status != UserNotificationListenerAccessStatus::Allowed) return;

    listener.NotificationChanged([](auto&& sender, auto&& args) {
        // Zero-polling push callback when toast arrives
        // Query args.NotificationId() and extract app metadata
    });
}
```

---

## 3. 4-Tier Adaptive Geolocation

### Problem
Windows Location Service (`lfsvc`) is often disabled in Services (`0x80070422`), causing WinRT location APIs to fail. IP-based fallbacks frequently resolve to distant ISP routing gateways (e.g. Talisay/Cebu).

### Solution: 4-Tier Resolution Hierarchy
1. **Tier 1 (User Setting)**: Mod settings `CustomLocation` field.
2. **Tier 2 (Windows Geolocation API)**: Query `ILocation` / WinRT Geolocation with defensive timeout.
3. **Tier 3 (Wi-Fi BSSID Triangulation)**: Query active router MAC (`WlanGetNetworkBssList`) via low-overhead geolocation endpoint.
4. **Tier 4 (IP Fallback)**: Fall back to `wttr.in/?format=j1` or IP-API with clear UI indication.

---

## 4. Power & Battery Telemetry

### Required Headers & Flags
- Library: `-lpowrprof`
- Headers: `<powrprof.h>`, `<batclass.h>`, `<devguid.h>`

### Metrics to Extract
1. **Wattage (Charge / Discharge Rate)**:
   Query `SYSTEM_POWER_STATUS` or battery IOCTL `IOCTL_BATTERY_QUERY_STATUS`.
   Rate in milliwatts: `Rate (mW) / 1000.0f = Watts`.
2. **Battery Health %**:
   `DesignedCapacity` vs `FullChargeCapacity` via `IOCTL_BATTERY_QUERY_INFORMATION`.
   `health% = (FullChargeCapacity * 100) / DesignedCapacity`.
3. **Active Power Scheme**:
   `PowerGetActiveScheme(nullptr, &activeGuid)` and `PowerReadFriendlyName`.
