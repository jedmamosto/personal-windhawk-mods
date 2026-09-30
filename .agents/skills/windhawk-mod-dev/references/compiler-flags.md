# Windhawk Clang++ Compiler Flags & Build Reference

## 1. Compiler Toolchain Paths
- **Compiler**: `C:\Program Files\Windhawk\Compiler\bin\clang++.exe`
- **Windhawk API Include**: `C:\Program Files\Windhawk\Compiler\include`
- **Forced Header**: `windhawk_api.h`

---

## 2. Essential Preprocessor Macros
When invoking Clang++ directly or via tasks, the following defines are mandatory:

```text
-DUNICODE
-D_UNICODE
-DWH_MOD
-DWH_MOD_ID=L\"<mod-id>\"
```

### Critical Pitfall: Wide String Macro Escaping
Windhawk internally passes `-DWH_MOD_ID=L"mod-id"`. Inside mod source files, `WH_MOD_ID` is used in:
- `wcscmp(argv[i + 1], WH_MOD_ID)`
- `CreateMutex(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID)`
- `sizeof(L" -tool-mod \"" WH_MOD_ID "\"")`

If `WH_MOD_ID` is defined without `L` (as a narrow string), compilation fails with:
`error: no matching function for call to 'wcscmp'` (cannot convert `const char[]` to `const wchar_t*`).

#### Command Shell Escaping Rules:
- **PowerShell**: `"-DWH_MOD_ID=L\`"<mod-id>\`""`
- **Windows cmd.exe / batch**: `-DWH_MOD_ID=L\"<mod-id>\"`
- **VS Code `tasks.json`**: `"-DWH_MOD_ID=L\"${fileBasenameNoExtension}\""`

---

## 3. Recommended Library Linkage Matrix
Add these flags to `// @compilerOptions` in the mod metadata block:

| Library | Purpose |
| :--- | :--- |
| `-lole32 -loleaut32` | COM interfaces and BSTR string operations |
| `-lshcore` | Per-monitor DPI awareness and scaling APIs |
| `-ld2d1 -ldwrite` | Direct2D hardware rendering and DirectWrite typography |
| `-ldwmapi` | DWM window attributes, dark mode frame styling, blur/acrylic |
| `-lgdi32 -luser32 -lshell32` | Standard Win32 GDI, window management, shell APIs |
| `-lruntimeobject` | WinRT activation and WinRT C++/WinRT runtime |
| `-lwindowscodecs` | WIC image decoding (album art, PNG/JPEG extraction) |
| `-lavrt` | Multimedia Class Scheduler Service (smooth 60/120fps animations) |
| `-lsetupapi` | Hardware device interface enumeration |
| `-lwinhttp` | Native asynchronous HTTP network fetching |
| `-lpdh` | Performance Data Helper counters (real-time CPU, GPU, Disk activity) |
| `-lwinmm` | Multimedia timer precision (`timeBeginPeriod`) |
| `-llocationapi` | Windows Location COM sensor interface |
| `-lpowrprof` | Power Schemes and Battery IOCTL queries |
| `-I"<mod-folder>"` | Resolves modular subsystem `.hpp` companion headers |
