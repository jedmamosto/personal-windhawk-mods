# Agent Guidelines: Personal Windhawk Mods

Repository guidelines, C++ Windhawk toolchain standards, and architecture invariants.

## 1. Project Overview

High-performance desktop customization mods for Windows 11 using the Windhawk injection platform.
The primary codebase is `dynamic-island-jedmamosto-fork`, utilizing hardware-accelerated Direct2D and DirectWrite.

## 2. Tech Stack & Build Commands

- **Language**: Modern C++ (C++17 / C++20).
- **Toolchain**: Windhawk Clang++ compiler and Windows 10/11 SDK.
- **Graphics & Typography**: Direct2D hardware-accelerated rendering and DirectWrite font typography.
- **System APIs**: WinRT (SMTC, Push Notifications, Bluetooth), Windows PDH, Power Management, and G-Helper IPC.
- **Verify Build (Fast PCH)**: Run `powershell -File dynamic-island-jedmamosto-fork\verify_mod.ps1`.
- **Rebuild PCH**: Run `dynamic-island-jedmamosto-fork\build_pch.bat` when Windows SDK headers change.
- **Clipboard Sync**: Run `node dynamic-island-jedmamosto-fork\copy_to_clip.js` to bundle source for Windhawk.
- **Render Direct2D Snapshot**: Run `dynamic-island-jedmamosto-fork\render_snapshot.bat` (or `.tools\render_*.bat`) to compile the offscreen WIC Direct2D test harness and produce pixel-perfect PNG snapshots.

## 3. Directory Taxonomy

- `dynamic-island-jedmamosto-fork/`: Modular source headers and build scripts.
  - `dynamic-island-jedmamosto-fork.wh.cpp`: Root mod entry point and exported Windhawk hooks.
  - `dynamic_island_pch.hpp`: Static Windows SDK and DirectX pre-compiled header definition.
  - `battery_dashboard.hpp`: Battery Bento dashboard and 2x3 peripheral card grid.
  - `ui_painters_engine.hpp`: Direct2D render orchestration and Hardware Monitor painters.
  - `telemetry_privacy_engine.hpp`: PDH counters, G-Helper IPC, and battery IOCTL hardware metrics.
  - `island_common.hpp`: Declarative micro-layout DSL (`SliceVertical3`, `CenterBox`, `InsetRect`).
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
5. **No Message Queue Blocking**: Perform background network, G-Helper file IO, and IOCTL requests on worker threads.
6. **Declarative Micro-Layout**: Use `SliceVertical3`, `CenterBox`, and `InsetRect` in `island_common.hpp` instead of raw Cartesian math.
7. **PCH-Accelerated Verification**: Run `verify_mod.ps1` with PCH detection to confirm clean compilation (exit code 0).
8. **Native Direct2D Snapshot Verification**: For all Direct2D UI changes, layout redesigns, and visual mockups, ALWAYS utilize the project's native C++ offscreen WIC snapshot pipeline (`.tools/render_snapshot.cpp`) to compile and emit real PNG renders with genuine DirectWrite typography, rather than approximating with external HTML mockups or AI-generated images.

## 5. Documentation Standards

- Follow [PRODUCT.md](PRODUCT.md) for product scope and boundary definitions.
- Follow [DESIGN.md](DESIGN.md) for Direct2D tokens, 2x3 grid geometry, and typography hierarchies.
- Write in ASD-STE100 English with concise sentences under 20 words.

