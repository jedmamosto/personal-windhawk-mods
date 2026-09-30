# Personal Windhawk Mods

Personal workspace for custom Windhawk mods developed by Jed Mamosto.

## Workspace Architecture

This workspace is structured as a multi-mod repository to house all your custom Windhawk mods in one place. Each mod is isolated in its own folder with dedicated source files, headers, build verification scripts, and documentation.

```text
Personal Windhawk Mods/
├── .vscode/
│   ├── c_cpp_properties.json      # Pre-configured Windhawk Clang++ IntelliSense & headers
│   └── tasks.json                 # Ctrl+Shift+B one-key syntax verification for any mod
├── .gitignore                     # Ignores build artifacts, dumps, and temp logs
├── README.md                      # Workspace index and mod registry
└── dynamic-island-jedmamosto-fork/ # Dynamic Island mod (Apple Dark, Battery, Notifications, AGY Satellite)
```

## Active Mods

- **[dynamic-island-jedmamosto-fork](./dynamic-island-jedmamosto-fork/README.md)**: Customized fork of Dynamic Island for Windows featuring:
  - **Modular Architecture**: Decoupled 13.8k-line monolith into a 690-line root mod with 12 clean companion headers (`.hpp`).
  - **Apple iOS OLED Fidelity**: Pure black (`#000000`, 100% opacity), crisp borderless silhouette, and zero muddy halo artifacts.
  - **Live PDH Disk Metric**: Real-time disk active time percentage instead of static drive capacity.
  - **Persistent Clock**: Preserves digital clock during media playback in both collapsed pill and expanded media views.
  - **Adaptive Geolocation**: 4-tier weather location resolution (Settings override -> Windows Location Service -> IP Geo fallback).
  - **Battery Section & Dashboard (Tab #5)**: Live discharge/charge wattage, health %, cycle count, and connected Bluetooth accessory battery levels.
  - **Universal Push Notifications**: Native WinRT `UserNotificationListener` toasts for Discord, Google Chrome, and Messenger.
  - **AGY Split-Island Satellite**: Right-side detached pill displaying circular context token usage, subagent activity chips, running tasks, and Option A+C MRU session carousel cycling.

## Adding Future Mods

1. Create a new directory under this workspace (e.g., `taskbar-stylist/`).
2. Author your `my-mod.wh.cpp` mod source.
3. Open the file in VS Code and press `Ctrl+Shift+B` — the workspace automatically compiles and verifies the active file against the native Windhawk Clang++ compiler and API headers.

## License

This project is licensed under the [MIT License](./LICENSE). Individual mods retain their respective upstream licenses and contributor attributions.

