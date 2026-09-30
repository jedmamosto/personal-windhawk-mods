---
name: windhawk-mod-dev
description: Interactive workflow to design, scaffold, modify, refactor, modularize, and verify Windows desktop mods using the Windhawk Clang++ toolchain and Windows system APIs (Direct2D, WinRT, PDH, Power Management). Make sure to use this skill whenever creating a new Windhawk mod, editing or upgrading existing mods in 'Personal Windhawk Mods', resolving Windhawk compilation or macro errors, designing split-island Direct2D overlays, or adding system telemetry sensors, even if not explicitly named.
version: 1.1.0
---

# Windhawk Mod Developer: Antigravity Workspace Specialist

This skill guides the design, implementation, modularization, and verification of Windhawk desktop customization mods.

---

## 1. Epistemic Purpose & 3-Question Anchor Pattern

- **Why This Skill Exists**: Prevents compilation errors, token bloat, monolithic file corruption, and system API crashes when authoring Windhawk C++ mods.
- **Pareto 80/20 Guiding Questions**:
  1. *Did I isolate complex subsystems into companion `.hpp` headers instead of editing the monolithic `.wh.cpp` directly?*
  2. *Did I verify compilation with Windhawk Clang++ using `-DWH_MOD_ID=L\"<id>\"` and include paths before deploying?*
  3. *Did I verify that system sensor queries (PDH, WinRT, Location) include graceful fallbacks when OS services are stopped?*

---

## 2. Core Invariants & Boundaries

- **Wide String Macro Invariant**: DO NOT define `WH_MOD_ID` as a narrow string or unescaped literal. ALWAYS use `-DWH_MOD_ID=L\"<mod-id>\"` in compiler commands.
- **Companion Header Invariant**: DO NOT write new multi-thousand-line subsystems directly inside the master `.wh.cpp`. ALWAYS scaffold features in standalone `.hpp` files and link via `@compilerOptions -I`.
- **Graceful Sensor Degradation Invariant**: DO NOT assume Windows services (`lfsvc`, PDH, WinRT) are active. ALWAYS handle errors defensively (e.g. `0x80070422` disabled service) with tiered fallbacks.
- **Direct2D Layered Window Invariant**: DO NOT paint outer bounding shadows that clip against dark desktop windows. ALWAYS ensure alpha channels clear to `0.0f` and keep silhouettes borderless or sub-pixel hairline.
- **Micro-Layout DSL Invariant**: DO NOT compute manual Cartesian floating-point coordinate offsets line-by-line. ALWAYS use `SliceVertical3`, `CenterBox`, and `InsetRect` from `island_common.hpp`.
- **PCH-Accelerated Verification Invariant**: ALWAYS run `powershell -File dynamic-island-jedmamosto-fork\verify_mod.ps1` to achieve sub-second syntax verification with PCH auto-detection.
- **Compilation Gate Invariant**: DO NOT declare a mod change complete without running syntax verification. ALWAYS achieve exit code `0`.
- **UTF-8 Clipboard Invariant**: DO NOT copy multi-megabyte mod sources through raw shell buffers. ALWAYS use `node copy_to_clip.js` to preserve encoding without character truncation.
- **Direct2D Snapshot Invariant**: DO NOT mock or visually verify Direct2D UI changes using generic HTML mockups or AI image generation. ALWAYS use the project's native C++ WIC snapshot renderer (`.tools/render_snapshot.cpp` / `render_snapshot.bat`) to produce offscreen, pixel-perfect PNG snapshots.

---

## 3. Workflow Steps

### Step 1: Intake & Scaffolding
1. For a new mod, copy [resources/mod-template.wh.cpp](./resources/mod-template.wh.cpp) into a dedicated folder.
2. For an existing mod, identify target subsystems (e.g. UI layout, telemetry engine, notification hooks).

### Step 2: Implement via Subsystem Headers
1. Author complex logic in a dedicated `<feature>_engine.hpp` in the mod folder.
2. Add `@compilerOptions -I"<folder>"` to the `.wh.cpp` header so Windhawk finds the files during internal compilation.
3. Integrate hook points inside `Wh_ModInit` and the main render loop.
4. Consult [references/windows-apis.md](./references/windows-apis.md) for sensor recipes (PDH, WinRT, Location).

### Step 2.5: Visual UI Snapshot Verification (When modifying UI)
1. When altering Direct2D layout or designing UI mockups, author or update a standalone offscreen test harness in `.tools/` (e.g. `render_snapshot.cpp`).
2. Compile and run via `render_snapshot.bat` to emit a genuine WIC-rendered PNG snapshot.
3. Inspect the snapshot to verify padding, text wrapping, and alignment before merging into production headers.

### Step 3: Verify Compilation
1. Run syntax verification using Windhawk Clang++:
   `powershell -File .agents/skills/windhawk-mod-dev/scripts/verify_mod.ps1 -SourcePath <path-to-.wh.cpp>`
2. Consult [references/compiler-flags.md](./references/compiler-flags.md) if linker or macro errors occur.
3. Inspect compiler output. Ensure 0 errors and 0 warnings.

### Step 4: Deploy & Sync
1. Run `node copy_to_clip.js` to place the complete UTF-8 source into Windows clipboard.
2. Instruct user to paste into Windhawk Source Code tab and click **Compile and Save**.
