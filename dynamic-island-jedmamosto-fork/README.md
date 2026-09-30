# Dynamic Island (jedmamosto-fork)

A modular, high-fidelity fork of [Dynamic Island for Windows](https://github.com/devcode90/Dynamic-Island-for-Windows) customized by Jed Mamosto. Built natively in modern C++ with hardware-accelerated Direct2D and DirectWrite rendering.

---

## Key Features & Customizations

1. **Apple iOS Native OLED Black Fidelity (`appledark`)**: True pitch black (`#000000`, 100% opacity), 0.5px hairline border, Apple secondary gray (`#86868B`), authentic privacy dots (microphone `#FF9F0A`, webcam `#30D158`), and zero muddy shadow halos.
2. **Live Hardware Monitor**: Real-time Windows Performance Data Helper (PDH) live % Disk Active Time instead of static storage capacity.
3. **Persistent Digital Clock**: Digital clock stays anchored in both collapsed pill (`[Album Art] [Clock] [Waveform]`) and expanded card header with paced invalidation when paused.
4. **4-Tier Adaptive Weather Geolocation**: Adaptive resolution hierarchy supporting custom locations (e.g. `Sibalom, Antique`), Windows Location Services, and IP Geo fallbacks.
5. **Dedicated Battery & Power Dashboard (Tab #5)**: Real-time battery percentage, discharge/charge wattage flow, battery wear/health, cycle count, Windows power plan switcher, and connected Bluetooth accessory battery levels.
6. **Universal Push Notification Pipeline**: Zero-polling WinRT `UserNotificationListener` push notifications for Discord, Chrome, Messenger, and system apps with 4.5s transient spring banners.
7. **AGY Split-Island Satellite**: Right-side detached satellite pill (`[Media / Clock]  (AGY Satellite)`) that manifests dynamically only when Google Antigravity is active, displaying circular context token usage, subagent activity chips, running tasks, and MRU session carousel cycling.
8. **Ultra-Smooth 360Hz+ Animations & Zero-CPU Auto-Parking**: High refresh rate monitor support with microsecond-accurate frame pacing and zero CPU draw when idle or hidden.

---

## Modular Architecture

The mod is organized into a clean 690-line root file and 12 specialized companion headers:

| Header | Description |
| :--- | :--- |
| [`island_common.hpp`](./island_common.hpp) | Core data models, settings, spring physics, layout bounds, and shared inline globals. |
| [`palette_color_engine.hpp`](./palette_color_engine.hpp) | Curated themes, dominant album art color extractor, and WCAG contrast calculations. |
| [`icon_process_engine.hpp`](./icon_process_engine.hpp) | 64px executable icon extractor, DIB pixel conversion, and PID-keyed LRU cache. |
| [`weather_location_engine.hpp`](./weather_location_engine.hpp) | WinRT/COM geolocation resolver and asynchronous wttr.in WinHTTP client. |
| [`telemetry_privacy_engine.hpp`](./telemetry_privacy_engine.hpp) | PDH performance counters (CPU, RAM, GPU, Disk, Net), battery IOCTLs, and ConsentStore privacy polling. |
| [`media_smtc_engine.hpp`](./media_smtc_engine.hpp) | WinRT SMTC media transport controls, VLC window watcher, and WASAPI loopback audio analyzer. |
| [`battery_dashboard.hpp`](./battery_dashboard.hpp) | Battery dashboard (Tab #5) rendering wattage, health %, cycle count, and accessory status. |
| [`notification_engine.hpp`](./notification_engine.hpp) | WinRT `UserNotificationListener` toast capture pipeline. |
| [`agy_telemetry_engine.hpp`](./agy_telemetry_engine.hpp) | Google Antigravity IPC pipe listener and satellite session tracker. |
| [`bluetooth_dnd_engine.hpp`](./bluetooth_dnd_engine.hpp) | WinRT Bluetooth device classification and WNF Do Not Disturb watcher. |
| [`ui_painters_engine.hpp`](./ui_painters_engine.hpp) | Direct2D `Renderer` class and 8 modular dashboard painters. |
| [`window_hook_manager.hpp`](./window_hook_manager.hpp) | Window procedure, low-level mouse/keyboard hooks, 360Hz render loop, and thread lifecycle. |

---

## Installation in Windhawk

1. Open **Windhawk** -> **Mods** -> **New Mod / Edit Fork**.
2. Run `node copy_to_clip.js` (or double-click `copy_source_to_clipboard.bat`) to place the root source on your clipboard.
3. Paste into the Windhawk Mod Editor and click **Compile and Save**.

---

## Verification & Build

Run the automated syntax and build verification script with Windhawk's Clang++ compiler:

```cmd
verify_build.bat
```

Or in VS Code, press `Ctrl + Shift + B` to trigger the pre-configured build task.

---

## Credits & Upstream

- Original project: [Dynamic Island for Windows](https://github.com/devcode90/Dynamic-Island-for-Windows) by **Himanshu (devcode90)**.
- Upstream contributors: **Sarthak Singh (sarthakaksh)**, **ciizerr**, **ChrisSch-dev**, **thevioletto**.
- Fork customizations and modular architecture by **Jed Mamosto**.
