# Agent Guidelines: Personal Windhawk Mods

Repository guidelines, C++ Windhawk toolchain standards, and architecture invariants.

## 1. Project Overview

High-performance desktop customization mods for Windows 11 using the Windhawk injection platform.
The primary codebase is `dynamic-island-jedmamosto-fork`, utilizing hardware-accelerated Direct2D and DirectWrite.

## 2. Tech Stack & Build Commands

- **Language**: Modern C++ (C++17 / C++20).
- **Toolchain**: Windhawk Clang++ compiler and Windows 10/11 SDK.
- **Graphics & Typography**: Direct2D hardware-accelerated rendering and DirectWrite font typography.
- **System APIs**: WinRT (SMTC, Push Notifications, Bluetooth), Windows PDH, and Power Management.
- **Verify Build**: Run `dynamic-island-jedmamosto-fork\verify_build.bat` to verify Clang++ compilation.
- **Clipboard Sync**: Run `node dynamic-island-jedmamosto-fork\copy_to_clip.js` to bundle source for Windhawk.

## 3. Directory Taxonomy

- `dynamic-island-jedmamosto-fork/`: Modular source headers and build scripts.
  - `dynamic-island-jedmamosto-fork.wh.cpp`: Root mod entry point and exported Windhawk hooks.
  - `battery_dashboard.hpp`: Battery Bento dashboard and 2x3 peripheral card grid.
  - `ui_painters_engine.hpp`: Direct2D render orchestration and Hardware Monitor painters.
  - `telemetry_privacy_engine.hpp`: PDH counters and battery IOCTL hardware metrics.
- `.agents/specs/`: Active feature specifications and task artifacts.
- `.agents/docs/`: Subsystem architecture references and ADRs.
- `.agents/templates/`: Canonical documentation templates.
- `.agents/rules/`: Workspace agent governance rules.
- `.agents/skills/`: Custom Windhawk mod development skills.

## 4. Engineering Invariants

1. **Zero-CPU Auto-Parking**: Suspend render loops and timers when collapsed, hidden, or occluded.
2. **Modular Architecture**: Keep engine headers isolated. Avoid monolithic code dumps into the root file.
3. **Safe Memory & ComPtr**: Use `Microsoft::WRL::ComPtr` for all DirectX and COM interfaces.
4. **No Heavy Runtimes**: Never introduce Electron, Node runtimes, or web views into injection targets.
5. **No Message Queue Blocking**: Perform background network and IOCTL requests on worker threads.
6. **Build Verification**: Run `verify_build.bat` to confirm clean compilation before completing tasks.

## 5. Documentation Standards

- Follow [PRODUCT.md](PRODUCT.md) for product scope and boundary definitions.
- Follow [DESIGN.md](DESIGN.md) for Direct2D tokens, 2x3 grid geometry, and typography hierarchies.
- Write in ASD-STE100 English with concise sentences under 20 words.
