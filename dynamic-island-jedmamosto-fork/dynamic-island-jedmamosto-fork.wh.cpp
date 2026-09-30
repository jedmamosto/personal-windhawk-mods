// ==WindhawkMod==
// @id              dynamic-island-jedmamosto-fork
// @name            Dynamic Island (jedmamosto-fork)
// @description     Customized fork of Dynamic Island for Windows with Apple iOS Native OLED Black fidelity and clean zero-halo rendering.
// @version         1.3.1
// @author          Himanshu & Jed Mamosto
// @github          https://github.com/devcode90
// @include         windhawk.exe
// @compilerOptions -lole32 -loleaut32 -lshcore -ld2d1 -ldwrite -ldwmapi -lgdi32 -luser32 -lshell32 -lruntimeobject -lwindowscodecs -lavrt -lsetupapi -lwinhttp -lpdh -lwinmm -llocationapi -lpowrprof -I"C:\Users\ASUS\Personal Windhawk Mods\dynamic-island-jedmamosto-fork"
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Dynamic Island for Windows

A fluid, living overlay inspired by Apple's Dynamic Island, bringing a beautiful, highly-responsive UI to your Windows desktop. Built natively with hardware-accelerated Direct2D rendering for a buttery-smooth 60 FPS experience.

![Dynamic Island running on the desktop](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/desktop.png)

![Dynamic Island surfaces](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/Full-preview-v2.png)

---

## 🚀 Modules & Dashboards

The Dynamic Island intelligently expands to display context-aware dashboards. You can easily navigate between different views using your mouse scroll wheel.

| Module | Description | Preview |
| :--- | :--- | :--- |
| **Media Player** | Shows live album art, track details, audio waveforms, and full playback controls. | ![Media](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/media-v2.png) |
| **Calendar** | A monthly grid that always fits its rows, with today marked in the accent colour. | ![Calendar](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/calendar.png) |
| **Weather** | Real-time weather stats powered by wttr.in, including wind speed, humidity, and "feels like" temperature. | ![Weather](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/weather-v2.png) |
| **Hardware Monitor** | CPU, RAM, GPU, disk and live network throughput, with load bars that turn amber past 75% and red past 90%. | ![Hardware Monitor](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/hardware-monitor.png) |
| **Game Overlay** | Real-time FPS, CPU, GPU, RAM and disk, sized to whichever metrics you enable. | ![Gamebar](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/gamebar-v2.png) |
| **Idle View** | A minimal dashboard with your battery status, digital clock, and sleek pagination dots. | ![Idle](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/idle-v2.png) |
| **Camera Privacy** | Shows a green dot when an app is actively using your webcam. | ![Camera](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/camera-detected-v2.png) |
| **Mic Privacy** | Shows an orange dot when an app is actively using your microphone. | ![Mic](https://raw.githubusercontent.com/devcode90/Dynamic-Island-for-Windows/793cf954d51aa9748c39d544187fab6e25ecd0fb/previews/mic-detected-v2.png) |

---

## ✨ Core Features

- **Hardware Privacy Indicators:** A pulsing orange dot appears when your microphone is active, and a green dot when your camera is in use. Rate-limited polling ensures absolutely no CPU drain.
- **High-Res Clipboard & Notifications:** Instantly see what you copied or your latest Windows notifications, featuring crisp, high-fidelity 64px app icons extracted directly from system executables.
- **360Hz+ Dynamic Fluid Animations:** Ultra-smooth resizing and splitting with native support for high refresh rate monitors (up to 360Hz/500Hz+) and zero idle CPU drain.
- **Eight Curated Themes:** Obsidian, Graphite, Slate, Nord, Evergreen, Espresso, Plum and a light Porcelain, all switchable from the right-click menu's Theme submenu — or dial in your exact hex colors.
- **Clean Flat Material:** Surfaces are built from soft downward depth shading, a drop shadow, and an accent wash tinted live from your album art. No rim lighting, no glass highlights, no outlined cards — nothing traces a bright line along an edge.
- **Real Blur & Acrylic Backdrop:** Optionally paint genuine Windows blur or frosted acrylic behind the island so your desktop shows through it, exactly like the system's own surfaces.
- **Translucent Backgrounds:** Hex colors accept an alpha channel (`#RRGGBBAA`), so you can make the island see-through while keeping text and icons perfectly crisp.
- **Your Language:** The island's own labels follow your Windows display language across 12 languages, or you can pick one explicitly.
- **File Tray:** Drag any file onto the island to park it on a shelf, then click to open it. Files are only referenced, never copied or moved.
- **Typography & Clock Control:** Scale all island text independently of the island's size, choose 12- or 24-hour time, show seconds, and set a custom date pattern — including CJK forms like `yyyy年MM月dd日`.

---

## ⚙️ Usage & Settings

- **Hover & Scroll:** Hover over the island to seamlessly expand it. Use your mouse scroll wheel to swipe between the Media, Calendar, Weather, Hardware Monitor, and File Tray tabs.
- **File Tray:** Enable the File Tray module, then drag files onto the island. It jumps to the shelf to confirm the drop. Click a row to open that file; right-click the island to clear the shelf.
- **Right-Click Menu:** Right-click the island to access Theme presets, Transparency settings, and to pin the island open.
- **⚠️ Right-click choices are per-session:** The right-click menu is a quick way to try things out, not a place to configure the mod. **Theme**, **shape style** (Pill / Notch / Windows 11) and **pin open** are all re-applied from your Windhawk settings whenever the mod restarts — so a reboot, a mod update, or toggling the mod off and on will discard them. Anything you want to keep, set in the **Mod Settings** tab instead. (Transparency and Expand-on-hover do persist, but the settings tab is still the reliable place for them.)
- **Windhawk Settings:** Visit the Mod Settings tab to change the island's Position, Size Scale, Refresh Rate (Target FPS), Animation Style (Smooth/Default/Bouncy/Snappy), Animation Speed, and toggle specific modules. You can also perfectly align the island using the `Offset X` and `Offset Y` settings, and select exactly which monitor the island should appear on (including a "Follow Mouse" mode!).
- **Notifications:** Windows must allow apps to read notifications: turn on **Settings > Privacy & security > Notifications > "Let apps access your notifications"**. Without that permission Windows denies the listener and the module stays silent. Nothing needs to be added to the process inclusion list; the island runs in its own process and reads notifications from there.
- **Quick Hide/Show:** Right-click the island and choose "Hide Island" to collapse it completely — CPU usage drops to ~0% while hidden since the mod fully parks its render thread. Bring it back instantly with the configurable hotkey (default **Ctrl+Alt+D**, changeable in the **Shortcuts** settings tab). Because a hidden island can't be right-clicked, the hotkey is the *only* way back once hidden — if you turn the hotkey off while hidden, re-enable it (or disable the mod) from Windhawk's settings.

---

## 📝 Feedback & Credits

### Feedback / Support / Bug Reports
- Please use [Windhawk Mods Issues](https://github.com/ramensoftware/windhawk-mods/issues) or [dynamic-island-for-windows issues](https://github.com/devcode90/dynamic-island-for-windows/issues) to report bugs, request features, or share feedback.
- Clear descriptions, screenshots, or steps to reproduce help improve fixes and updates.
- Suggestions for UI/UX or new integrations are always welcome.

### Credits
- **[Sarthak Singh (sarthakaksh) @GitHub](https://github.com/sarthakaksh)**: Major feature overhaul including the right-click focus timer, hover clock, robust media controls, zero-CPU instant hide shortcut, Bluetooth battery integration, image clipboard thumbnails, and full-screen autohide fixes.
- **[ciizerr @GitHub](https://github.com/ciizerr)**: Improved the UI by refining layout alignment, fixing dashboard scaling, and enhancing calendar and weather module integration.
- **[ChrisSch-dev @GitHub](https://github.com/ChrisSch-dev)**: Added album title support, word wrapping for weather descriptions, sleep resume fixes, and various performance/movement stability improvements.
- **[thevioletto @GitHub](https://github.com/thevioletto)**: Added custom font support, Windows Do Not Disturb integration and status alerts, improved album art color sampling, reorganized settings, and addressed various UI/media edge cases.

### 🤝 Contributing
We love community contributions! To ensure high-quality updates, please follow these rules:
1. **Fork & Branch:** Fork the repository and apply your feature or fix.
2. **Respect Existing Features:** Do not outright remove or break other people's features unless fully explained why in your PR. You are highly encouraged to refine and improve existing features!
3. **Credit Yourself:** After completing your feature, add your name and a short summary of your contribution to the **Credits** section in both the Mod UI here and the GitHub `README.md`!

*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Appearance:
  - Position: top-center
    $name: Position
    $description: Where the island should appear on your screen.
    $options:
      - top-center: Top Center
      - top-left: Top Left
      - top-right: Top Right
      - bottom-center: Bottom Center
      - bottom-left: Bottom Left
      - bottom-right: Bottom Right
  - TargetMonitor: primary
    $name: Target Monitor
    $description: Select the screen to display the island. If a display isn't found, it safely falls back to the Primary Monitor.(Extra Displays are given even if they don't exist because windhawk settings are static)
    $options:
      - 'primary': Primary Monitor
      - '1': Display 1
      - '2': Display 2
      - '3': Display 3
      - '4': Display 4
      - '5': Display 5
      - 'follow': Follow Mouse (Active Monitor)
  - OffsetX: 0
    $name: Offset X
    $description: Adjust the horizontal position (in pixels). Positive values move it right, negative values move it left.
  - OffsetY: 0
    $name: Offset Y
    $description: Adjust the vertical position (in pixels). Positive values move it down, negative values move it up. Applies to every state unless the separate expanded offset below is turned on, in which case this becomes the collapsed/idle offset.
  - SeparateExpandedOffsetY: false
    $name: Use a separate Offset Y when expanded
    $description: Lets the island sit at one height while collapsed and a different one while expanded. Useful for tucking the idle pill up near the screen edge while keeping the expanded dashboard somewhere comfortable to interact with.
  - OffsetYExpanded: 0
    $name: Offset Y (expanded)
    $description: Vertical position in pixels while the island is expanded. Only used when the separate expanded offset above is on. The island eases between this and Offset Y as it expands and collapses, so the two never snap.
  - BorderMergedMode: false
    $name: Border-Merged Mode
    $description: Attach the island flush to the top edge of the monitor without a floating gap.
  - ShapeStyle: default
    $name: Island Shape Style
    $description: Change the overall shape of the island (Pill, Windows 11, or macOS Notch).
    $options:
      - default: Default (Apple Pill)
      - w11: Windows 11 (Rounded Box)
      - notch: macOS Notch (Top Edge Flush)
  - SizeScale: '1.0'
    $name: Size scale
    $description: Makes the entire island and its contents larger or smaller.
    $options:
      - '0.8': 0.8x
      - '1.0': 1.0x
      - '1.2': 1.2x
      - '1.5': 1.5x
      - '1.8': 1.8x
      - '2.0': 2.0x
      - '2.5': 2.5x
  - AutoDpiScale: true
    $name: Auto DPI scaling
    $description: Automatically scales the island to match your monitor's DPI. Recommended for 4K screens.
  $name: Appearance & Position
- Behavior:
  - AlwaysOnTop: true
    $name: Always on top
    $description: Keeps the island above all other windows. Turn this off if it blocks other apps.
  - ExpandOnHover: true
    $name: Expand on hover
    $description: Expand the island automatically when hovered. If disabled, click to expand.
  - AutoHideIdleSeconds: '0'
    $name: Auto-hide island (all states)
    $description: Hide the island (including idle, media, and other states) after this many seconds of inactivity. 0 to disable.
    $options:
      - '-1': Hide instantly
      - '0': Never hide (default)
      - '5': Hide after 5 seconds
      - '10': Hide after 10 seconds
      - '30': Hide after 30 seconds
      - '60': Hide after 60 seconds
  - AutoHideFullscreen: true
    $name: Hide on full screen
    $description: Automatically hide the island when playing a video or app in full screen mode.
  - AutoHideMaximized: true
    $name: Micro-notch on maximized window
    $description: Automatically contract the island to a sleek 6px bezel lip when a window is maximized, keeping tabs accessible. Expands on hover.
  - UnhideOnHover: true
    $name: Unhide on hover
    $description: Allow the hidden island to reappear when you hover your mouse over it.
  $name: Behavior & Visibility
- Animations:
  - TargetFPS: auto
    $name: Refresh rate / FPS
    $description: Set the animation frame rate. Choose Auto to dynamically match your active monitor's refresh rate (up to 360Hz/500Hz), or select a fixed FPS.
    $options:
      - auto: Auto (Match Monitor Refresh Rate)
      - '60': 60 FPS (Eco / Standard)
      - '90': 90 FPS
      - '120': 120 FPS
      - '144': 144 FPS
      - '165': 165 FPS
      - '240': 240 FPS
      - '360': 360 FPS (Ultra Smooth)
      - '500': 500 FPS (Maximum / Uncapped)
  - AnimationStyle: default
    $name: Animation bounciness / style
    $description: Control the spring physics and feel of the animation.
    $options:
      - smooth: Smooth (No bounciness / Critically damped)
      - default: Default (Balanced Apple-like spring)
      - bouncy: Bouncy (Dynamic elastic spring)
      - snappy: Snappy (High stiffness, quick settle)
  - AnimationSpeed: normal
    $name: Animation speed
    $description: How fast the island expands and collapses.
    $options:
      - very-slow: Very Slow (0.5x)
      - slow: Slow (0.75x)
      - normal: Normal (1.0x)
      - fast: Fast (1.35x)
      - very-fast: Very Fast (1.65x)
      - ultra-fast: Ultra Fast (2.0x)
  $name: Animations & Performance
- Themes:
  - ThemePreset: appledark
    $name: Theme preset
    $description: Select a curated color theme, or choose Custom to use your own hex colors below.
    $options:
      - appledark: Apple Dark (iOS Native - iPhone 16 Pro)
      - liquidglass: OS Liquid Glass 26
      - obsidian: Obsidian - true black
      - graphite: Graphite - neutral Windows 11 dark
      - slate: Slate - cool blue-grey
      - nord: Nord - the Nord palette
      - evergreen: Evergreen - deep green
      - espresso: Espresso - warm brown
      - plum: Plum - muted mauve
      - porcelain: Porcelain - light theme
      - custom: Custom Colors (Use Hex Below)
  - PillOpacity: 96
    $name: Pill transparency
    $description: 35 to 100. Lower values make the island more see-through.
  - TintIntensity: 72
    $name: Background tint intensity
    $description: 0 to 100. Controls how dark the background tint behind the island is.
  - BackdropMaterial: none
    $name: Backdrop material (real Windows blur)
    $description: Paints genuine Windows blur or acrylic behind the island so your desktop and windows show through it. Acrylic adds the frosted noise texture Windows uses for its own surfaces. Requires Windows 10 1803 or newer; on unsupported builds the island simply stays opaque. When this is on, use Backdrop fill opacity below to control how much of the blur comes through.
    $options:
      - none: Off (solid background)
      - blur: Blur
      - acrylic: Acrylic (frosted)
  - BackdropFillOpacity: 45
    $name: Backdrop fill opacity
    $description: 0 to 100. Only used when a Backdrop material is enabled. How opaque the island's own background stays on top of the blur - lower values let more of the blurred desktop through. Has no effect when the backdrop is Off.
  - BackdropTint: 55
    $name: Acrylic tint strength
    $description: 0 to 100. Only used by the Acrylic backdrop. How strongly your background colour tints the frosted layer.
  - MaterialDepth: true
    $name: Depth shading
    $description: Adds soft downward shading and the accent wash so the island has depth instead of looking like one flat fill. No edge highlights or rim lighting are involved. Turn off for a completely flat look.
  - DropShadow: false
    $name: Soft drop shadow
    $description: Casts a soft shadow beneath the island to lift it off the desktop. Turned off for razor-sharp Apple Dynamic Island edges.
  - AccentBloom: 100
    $name: Accent bloom intensity
    $description: 0 to 200. Strength of the soft accent-coloured wash bleeding in from the top of the island. Driven by album art when the accent mode is Auto. Set to 0 to remove it.
  - TextScale: 100
    $name: Text size
    $description: 70 to 160. Scales all island text independently of the overall Size scale, so you can keep the island compact while making the clock and labels easier to read.
  - AccentColorMode: auto
    $name: Accent color mode
    $description: How the glowing accent color is chosen. Auto extracts it from album art.
    $options:
      - auto: Auto, from album art
      - system: System (Device Accent)
      - custom: Custom hex
  - CustomAccentHex: "#4cc9f0"
    $name: Custom accent hex
    $description: The hex color to use when the accent mode is set to Custom.
  - CalendarAccent: red
    $name: Calendar accent color
    $description: Accent color used in the calendar view for month name, weekends, and today's date highlight.
    $options:
      - red: Default Red
      - system: System (Device Accent)
  - ClockAccentGlow: true
    $name: Show clock background circle/glow
    $description: Display the soft accent circle/glow behind the time in the expanded clock view. Turn off for a clean, minimal clock without background glow.
  - FontFamily: ""
    $name: Font family
    $description: Custom font family for island text (e.g. Segoe UI, Arial, Aptos, Consolas). Leave empty for system default.
  - ContourBorderMode: default
    $name: Contour border
    $description: Choose the island's border style. Default uses the theme's matching border, Auto extracts the border color from album art, and Borderless removes all outer border outlines.
    $options:
      - default: Default
      - auto: Auto (From album art)
      - borderless: Borderless
  - ContourBorderHex: "#0D0D0E"
    $name: Contour border hex color
    $description: 'Hex color for the island contour stroke. Changing it from the default overrides the selected Theme preset; restore the default to go back to the preset.'
  - PillBgColor: "#000000"
    $name: Pill background color
    $description: 'Hex color for the island background. Accepts 3, 4, 6 or 8 hex digits, so use the 8-digit RRGGBBAA form for a translucent background (for example 0D0D0FB0) while keeping the text and icons opaque. Changing it from the default overrides the selected Theme preset; restore the default to go back to the preset.'
  - TextPrimaryColor: "#FFFFFF"
    $name: Primary text color
    $description: 'Accessible hex color for titles and main text. Changing it from the default overrides the selected Theme preset; restore the default to go back to the preset.'
  - TextSecondaryColor: "#9B9BA5"
    $name: Secondary text color
    $description: 'Accessible hex color for artist names and muted labels. Changing it from the default overrides the selected Theme preset; restore the default to go back to the preset.'
  $name: Colors & Theming
- Indicators:
  - PrivacyDots: true
    $name: Show privacy indicators (Mic & Camera)
    $description: Master toggle to display the iOS-style privacy dots when microphone or camera is in use.
  - PrivacyDotsMic: true
    $name: Show microphone indicator (Orange dot)
    $description: Show the orange dot when microphone is in use. Turn off if background apps (like Discord/OBS) keep it permanently active.
  - PrivacyDotsMicHex: "#FF9500"
    $name: Microphone dot color
    $description: Custom hex color for microphone privacy indicator.
  - PrivacyDotsCam: true
    $name: Show camera indicator (Green dot)
    $description: Show the green dot when webcam is in use.
  - PrivacyDotsCamHex: "#34C759"
    $name: Camera dot color
    $description: Custom hex color for camera privacy indicator.
  $name: Privacy Indicators
- Modules:
  - Media: true
    $name: Media module
    $description: Shows album art, song info, and playback controls when music is playing.
  - MediaAutoExpand: false
    $name: Auto-expand on track change
    $description: Automatically expand the island when a new song or video starts playing. If disabled, album art updates smoothly in the collapsed pill without unprompted expansion.
  - Volume: true
    $name: Volume slider flyout
    $description: Shows a volume slider banner on the island when adjusting system volume. Disable if you prefer the default Windows volume flyout.
  - CapsLock: true
    $name: Caps Lock module
    $description: Shows an indicator when Caps Lock or Num Lock state changes.
  - Battery: true
    $name: Battery module
    $description: Shows the battery dashboard card when expanding the island and alerts when laptop battery is low.
  - BluetoothIndicator: true
    $name: Bluetooth connect/disconnect indicator
    $description: Shows a card with the device name, category icon, and battery level (if available) when a Bluetooth device connects or disconnects.
  - BluetoothShowBattery: true
    $name: Show Bluetooth battery level
    $description: Reads battery level over BLE GATT when the device supports it. Not all classic Bluetooth devices report battery this way — when unavailable, only a Connected/Disconnected label is shown.
  - Progress: true
    $name: Progress module
    $description: Shows a progress ring around the island for downloads or file copies.
  - TimerModule: true
    $name: Focus Timer module
    $description: Enables a Pomodoro-style focus/break timer, startable from the island's right-click menu.
  - Clipboard: true
    $name: Clipboard module
    $description: Shows a quick preview of the text or images you just copied.
  - StatusCountdownProgress: false
    $name: Status countdown progress bar
    $description: Shows a subtle countdown progress bar at the bottom of temporary status alert cards (such as Clipboard, Notifications, and Device alerts). Disabled by default.
  - DoNotDisturbIndicator: true
    $name: Do Not Disturb status alert
    $description: Shows a status alert card when Do Not Disturb is toggled in the Windows notification panel.
  - NotificationRespectDnD: true
    $name: Notifications respect Do Not Disturb
    $description: Suppresses Dynamic Island notification alerts when Windows Do Not Disturb is active.
  - HardwareMonitorModule: true
    $name: Include Hardware Monitor in scroll loop
    $description: Add CPU, GPU, RAM, FPS and Network stats card to mouse-wheel scroll loop.
  - GameOverlay: false
    $name: Enable game overlay mode
    $description: Replaces the clock with live stats like FPS, CPU, and RAM usage.
  - ShowMetricText: false
    $name: Show labels in metric chips
    $description: Adds text labels (like "CPU") inside the game overlay bars.
  - Weather: true
    $name: Weather module
    $description: Shows the weather on the right side of the pill. Turn off to only show the clock.
  - CustomLocation: ""
    $name: Custom Location (Optional)
    $description: Enter your city, municipality, airport code, or postal code (e.g. London, Tokyo, Manila, Sibalom, Antique, 90210, JFK). Leave empty for adaptive auto-detection.
  - WeatherFahrenheit: false
    $name: Use Fahrenheit
    $description: Display weather temperature and wind speed in imperial units.
  - Language: auto
    $name: Language
    $description: Language for the island's own text labels. Auto follows your Windows display language.
    $options:
      - auto: Auto (Follow Windows)
      - en: English
      - fr: Français (French)
      - es: Español (Spanish)
      - de: Deutsch (German)
      - pt: Português (Portuguese)
      - it: Italiano (Italian)
      - ru: Русский (Russian)
      - tr: Türkçe (Turkish)
      - hi: हिन्दी (Hindi)
      - zh: 简体中文 (Simplified Chinese)
      - ja: 日本語 (Japanese)
      - ko: 한국어 (Korean)
  - ClockFormat: system
    $name: Clock format
    $description: Choose 12-hour or 24-hour time, or follow your Windows locale setting.
    $options:
      - system: Follow Windows locale
      - 12h: 12-hour (3:07 PM)
      - 24h: 24-hour (15:07)
  - ShowSeconds: false
    $name: Show seconds on the clock
    $description: Include seconds in the expanded clock. Costs a little more CPU because the clock then redraws every second.
  - DateFormat: ""
    $name: Custom date format
    $description: 'Custom date pattern for the idle dashboard. Leave empty to follow your Windows locale. Supports yyyy (year), MM / M (month), dd / d (day), MMM (short month name), MMMM (full month name), ddd / dddd (weekday). Any other characters are printed as-is, so CJK formats like yyyy年MM月dd日 work.'
  - DateFirst: false
    $name: Show date above the time
    $description: Swap the idle dashboard so the date is the headline and the time sits beneath it.
  - FileTrayModule: false
    $name: File Tray (drag & drop shelf)
    $description: Adds a File Tray card to the scroll loop. Drag files onto the island to park them there, then click to open one, or use the right-click menu to clear the shelf. Files are only referenced, never copied or moved.
  - FileTrayMaxItems: 10
    $name: File Tray capacity
    $description: 1 to 25. How many files the shelf keeps before the oldest one drops off.
  - MediaExpandBlocklist: ""
    $name: Never auto-expand for these apps
    $description: 'Comma-separated list of app or site names that should never make the island expand on a track change, while still updating quietly in the collapsed pill. Matched loosely against the media source and title, for example: chrome, tiktok, youtube.'
  - GameOverlayShowFps: true
    $name: Game overlay - show FPS
    $description: Include the frame rate in the game overlay strip.
  - GameOverlayShowCpu: true
    $name: Game overlay - show CPU
    $description: Include CPU utilization in the game overlay strip.
  - GameOverlayShowGpu: true
    $name: Game overlay - show GPU
    $description: Include GPU utilization in the game overlay strip.
  - GameOverlayShowRam: true
    $name: Game overlay - show RAM
    $description: Include memory usage in the game overlay strip.
  - GameOverlayShowDisk: true
    $name: Game overlay - show disk
    $description: Include disk usage in the game overlay strip.
  - GameOverlayCompact: false
    $name: Game overlay - compact size
    $description: Shrink the game overlay to a narrow strip that fits neatly inside the taskbar area.
  $name: Modules & Features
- Shortcuts:
  - HideShowHotkeyEnabled: true
    $name: Enable hide/show hotkey
    $description: Toggle the island's visibility instantly with a keyboard shortcut. The hotkey is the only way to bring a hidden island back, so leave this on unless you're comfortable re-enabling it from Windhawk settings.
  - HideShowModifiers: ctrl_alt
    $name: Hotkey modifiers
    $description: Modifier keys combined with the letter/number key below.
    $options:
      - ctrl_alt: Ctrl + Alt
      - ctrl_shift: Ctrl + Shift
      - alt_shift: Alt + Shift
      - win_alt: Win + Alt
      - ctrl_alt_shift: Ctrl + Alt + Shift
  - HideShowKey: "D"
    $name: Hotkey letter/number key
    $description: A single A-Z or 0-9 key combined with the modifiers above. Falls back to "D" if left blank or invalid.
  $name: Shortcuts & Hotkeys
*/
// ==/WindhawkModSettings==

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

// Windhawk API
#include <windows.h>

// Modular Architecture Companion Headers
#if __has_include("island_common.hpp")
#include "island_common.hpp"
#include "palette_color_engine.hpp"
#include "icon_process_engine.hpp"
#include "weather_location_engine.hpp"
#include "telemetry_privacy_engine.hpp"
#include "media_smtc_engine.hpp"
#include "battery_dashboard.hpp"
#include "notification_engine.hpp"
#include "agy_telemetry_engine.hpp"
#include "bluetooth_dnd_engine.hpp"
#include "ui_painters_engine.hpp"
#include "window_hook_manager.hpp"
#else
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/island_common.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/palette_color_engine.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/icon_process_engine.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/weather_location_engine.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/telemetry_privacy_engine.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/media_smtc_engine.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/battery_dashboard.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/notification_engine.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/agy_telemetry_engine.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/bluetooth_dnd_engine.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/ui_painters_engine.hpp"
#include "C:/Users/ASUS/Personal Windhawk Mods/dynamic-island-jedmamosto-fork/window_hook_manager.hpp"
#endif

BOOL WhTool_ModInit() {
    LoadSettings();

    g_agyTelemetry.Initialize();

    if (!StartThreads()) {
        StopThreads();
        return FALSE;
    }

    g_layoutDirty = true;
    Wh_Log(L"Dynamic Island for Windows initialized.");
    return TRUE;
}

void WhTool_ModSettingsChanged() {
    LoadSettings();
}

void WhTool_ModUninit() {
    if (g_hwnd) {
        PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
    }
    g_notificationEngine.Stop();
    StopThreads();
    Wh_Log(L"Dynamic Island for Windows unloaded.");
}

//////////////////////////////////////////////////////////////////////////////////
// Windhawk tool mod implementation for mods which don't need to inject to other
// processes or hook other functions. Context:
// https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
//
// The mod will load and run in a dedicated windhawk.exe process.
//
// Paste the code below as part of the mod code, and use these callbacks:
// * WhTool_ModInit
// * WhTool_ModSettingsChanged
// * WhTool_ModUninit
//
// Currently, other callbacks are not supported.

bool g_isToolModProcessLauncher;
HANDLE g_toolModProcessMutex;

void WINAPI EntryPoint_Hook() {
    Wh_Log(L">");
    ExitThread(0);
}

BOOL Wh_ModInit() {
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) &&
        sessionId == 0) {
        return FALSE;
    }

    bool isExcluded = false;
    bool isToolModProcess = false;
    bool isCurrentToolModProcess = false;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    if (!argv) {
        Wh_Log(L"CommandLineToArgvW failed");
        return FALSE;
    }

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-service") == 0 ||
            wcscmp(argv[i], L"-service-start") == 0 ||
            wcscmp(argv[i], L"-service-stop") == 0) {
            isExcluded = true;
            break;
        }
    }

    for (int i = 1; i < argc - 1; i++) {
        if (wcscmp(argv[i], L"-tool-mod") == 0) {
            isToolModProcess = true;
            const wchar_t* modIdArg = argv[i + 1];
            if (wcsncmp(modIdArg, L"local@", 6) == 0) {
                modIdArg += 6;
            }
            const wchar_t* targetModId = WH_MOD_ID;
            if (wcsncmp(targetModId, L"local@", 6) == 0) {
                targetModId += 6;
            }
            if (wcscmp(modIdArg, targetModId) == 0) {
                isCurrentToolModProcess = true;
            }
            break;
        }
    }

    LocalFree(argv);

    if (isExcluded) {
        return FALSE;
    }

    if (isCurrentToolModProcess) {
        const wchar_t* targetModId = WH_MOD_ID;
        if (wcsncmp(targetModId, L"local@", 6) == 0) {
            targetModId += 6;
        }
        WCHAR mutexName[MAX_PATH];
        swprintf_s(mutexName, L"windhawk-tool-mod_%s", targetModId);

        g_toolModProcessMutex =
            CreateMutex(nullptr, TRUE, mutexName);
        if (!g_toolModProcessMutex) {
            Wh_Log(L"CreateMutex failed");
            ExitProcess(1);
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            Wh_Log(L"Tool mod already running (%s)", targetModId);
            ExitProcess(1);
        }

        if (!WhTool_ModInit()) {
            ExitProcess(1);
        }

        IMAGE_DOS_HEADER* dosHeader =
            (IMAGE_DOS_HEADER*)GetModuleHandle(nullptr);
        IMAGE_NT_HEADERS* ntHeaders =
            (IMAGE_NT_HEADERS*)((BYTE*)dosHeader + dosHeader->e_lfanew);

        DWORD entryPointRVA = ntHeaders->OptionalHeader.AddressOfEntryPoint;
        void* entryPoint = (BYTE*)dosHeader + entryPointRVA;

        Wh_SetFunctionHook(entryPoint, (void*)EntryPoint_Hook, nullptr);
        return TRUE;
    }

    if (isToolModProcess) {
        return FALSE;
    }

    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) {
        return;
    }

    WCHAR currentProcessPath[MAX_PATH];
    switch (GetModuleFileName(nullptr, currentProcessPath,
                              ARRAYSIZE(currentProcessPath))) {
        case 0:
        case ARRAYSIZE(currentProcessPath):
            Wh_Log(L"GetModuleFileName failed");
            return;
    }

    WCHAR
    commandLine[MAX_PATH + 2 +
                (sizeof(L" -tool-mod \"" WH_MOD_ID "\"") / sizeof(WCHAR)) - 1];
    swprintf_s(commandLine, L"\"%s\" -tool-mod \"%s\"", currentProcessPath,
               WH_MOD_ID);

    HMODULE kernelModule = GetModuleHandle(L"kernelbase.dll");
    if (!kernelModule) {
        kernelModule = GetModuleHandle(L"kernel32.dll");
        if (!kernelModule) {
            Wh_Log(L"No kernelbase.dll/kernel32.dll");
            return;
        }
    }

    using CreateProcessInternalW_t = BOOL(WINAPI*)(
        HANDLE hUserToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes, WINBOOL bInheritHandles,
        DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
        LPSTARTUPINFOW lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation,
        PHANDLE hRestrictedUserToken);
    CreateProcessInternalW_t pCreateProcessInternalW =
        (CreateProcessInternalW_t)GetProcAddress(kernelModule,
                                                 "CreateProcessInternalW");
    if (!pCreateProcessInternalW) {
        Wh_Log(L"No CreateProcessInternalW");
        return;
    }

    STARTUPINFO si{
        .cb = sizeof(STARTUPINFO),
        .dwFlags = STARTF_FORCEOFFFEEDBACK,
    };
    PROCESS_INFORMATION pi;
    if (!pCreateProcessInternalW(nullptr, currentProcessPath, commandLine,
                                 nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS,
                                 nullptr, nullptr, &si, &pi, nullptr)) {
        Wh_Log(L"CreateProcess failed");
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void Wh_ModSettingsChanged() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (g_isToolModProcessLauncher) {
        return;
    }

    WhTool_ModUninit();
    ExitProcess(0);
}

