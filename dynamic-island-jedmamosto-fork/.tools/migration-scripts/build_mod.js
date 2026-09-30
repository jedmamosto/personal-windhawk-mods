const fs = require('fs');

const srcPath = 'C:\\ProgramData\\Windhawk\\ModsSource\\dynamic-island-for-windows.wh.cpp';
const dstPath = 'C:\\Users\\ASUS\\.gemini\\antigravity\\brain\\cee54dc0-3ea0-48ea-812d-b83b5f7c8ec0\\scratch\\dynamic-island-jedmamosto-fork.wh.cpp';

let code = fs.readFileSync(srcPath, 'utf8');

// Normalize line endings to \n
const isCRLF = code.includes('\r\n');
code = code.replace(/\r\n/g, '\n');

// 1. Mod Header
code = code.replace(
    /@id\s+dynamic-island-for-windows/,
    '@id              dynamic-island-jedmamosto-fork'
);
code = code.replace(
    /@name\s+Dynamic Island for Windows/,
    '@name            Dynamic Island (jedmamosto-fork)'
);
code = code.replace(
    /@description\s+A living, breathing pill overlay[^\n]*/,
    '@description     Customized fork of Dynamic Island for Windows with Apple iOS Native OLED Black fidelity and clean zero-halo rendering.'
);
code = code.replace(
    /@author\s+Himanshu/,
    '@author          Himanshu & Jed Mamosto'
);

// 2. Settings YAML: Replace ThemePreset block with Apple Dark default and Liquid Glass option
const oldThemeBlock = `  - ThemePreset: obsidian
    $name: Theme preset
    $description: Select a curated color theme, or choose Custom to use your own hex colors below.
    $options:
      - obsidian: Obsidian - true black (Default)`;

const newThemeBlock = `  - ThemePreset: appledark
    $name: Theme preset
    $description: Select a curated color theme, or choose Custom to use your own hex colors below.
    $options:
      - appledark: Apple Dark (iOS Native - iPhone 16 Pro)
      - liquidglass: OS Liquid Glass 26
      - obsidian: Obsidian - true black
      - graphite: Graphite - neutral Windows 11 dark`;

if (!code.includes(oldThemeBlock)) {
    throw new Error('oldThemeBlock not found in normalized code!');
}
code = code.replace(oldThemeBlock, newThemeBlock);

code = code.replace(
    'ContourBorderHex: "#1E1E22"',
    'ContourBorderHex: "#0D0D0E"'
);
code = code.replace(
    'PillBgColor: "#08080A"',
    'PillBgColor: "#000000"'
);

// 3. Enum ThemePreset: Add LiquidGlass and AppleDark before Custom
const oldEnum = `enum class ThemePreset {
    Obsidian,
    Graphite,
    Slate,
    Nord,
    Evergreen,
    Espresso,
    Plum,
    Porcelain,
    Custom,
};`;

const newEnum = `enum class ThemePreset {
    Obsidian,
    Graphite,
    Slate,
    Nord,
    Evergreen,
    Espresso,
    Plum,
    Porcelain,
    LiquidGlass,
    AppleDark,
    Custom,
};`;

if (!code.includes(oldEnum)) {
    throw new Error('oldEnum not found!');
}
code = code.replace(oldEnum, newEnum);

// 4. kThemePalettes: Exactly matching Apple HIG and Liquid Glass
const oldPalettes = `static constexpr ThemePalette kThemePalettes[] = {
    {L"obsidian",  L"Obsidian (Default)", L"#08080A", L"#FFFFFF", L"#9B9BA5", L"#1E1E22"},
    {L"graphite",  L"Graphite",           L"#1C1C1E", L"#FFFFFF", L"#A8A8AE", L"#323236"},
    {L"slate",     L"Slate",              L"#111721", L"#E9EEF6", L"#92A2B8", L"#232C3A"},
    {L"nord",      L"Nord",               L"#2E3440", L"#ECEFF4", L"#A7B0C0", L"#3B4252"},
    {L"evergreen", L"Evergreen",          L"#0C1512", L"#E4F1EA", L"#8CAE9D", L"#1A2A23"},
    {L"espresso",  L"Espresso",           L"#1A1512", L"#F6EEE7", L"#B7A395", L"#2E2420"},
    {L"plum",      L"Plum",               L"#16111C", L"#F1EAF6", L"#AC9BB9", L"#281F33"},
    {L"porcelain", L"Porcelain (Light)",  L"#F5F6F8", L"#14161A", L"#5B6270", L"#D8DBE1"},
};`;

const newPalettes = `static constexpr ThemePalette kThemePalettes[] = {
    {L"obsidian",    L"Obsidian",                     L"#08080A", L"#FFFFFF", L"#9B9BA5", L"#1E1E22"},
    {L"graphite",    L"Graphite",                     L"#1C1C1E", L"#FFFFFF", L"#A8A8AE", L"#323236"},
    {L"slate",       L"Slate",                        L"#111721", L"#E9EEF6", L"#92A2B8", L"#232C3A"},
    {L"nord",        L"Nord",                         L"#2E3440", L"#ECEFF4", L"#A7B0C0", L"#3B4252"},
    {L"evergreen",   L"Evergreen",                    L"#0C1512", L"#E4F1EA", L"#8CAE9D", L"#1A2A23"},
    {L"espresso",    L"Espresso",                     L"#1A1512", L"#F6EEE7", L"#B7A395", L"#2E2420"},
    {L"plum",        L"Plum",                         L"#16111C", L"#F1EAF6", L"#AC9BB9", L"#281F33"},
    {L"porcelain",   L"Porcelain (Light)",            L"#F5F6F8", L"#14161A", L"#5B6270", L"#D8DBE1"},
    {L"liquidglass", L"OS Liquid Glass 26",           L"#141418", L"#FFFFFF", L"#A0A0AA", L"#383844"},
    {L"appledark",   L"Apple Dark (iOS Native)",      L"#000000", L"#FFFFFF", L"#86868B", L"#0D0D0E"},
};`;

if (!code.includes(oldPalettes)) {
    throw new Error('oldPalettes not found!');
}
code = code.replace(oldPalettes, newPalettes);

// 5. Default ThemePreset initialization in Settings struct
const oldSettingDefault = '    ThemePreset themePreset = ThemePreset::Obsidian;';
const newSettingDefault = '    ThemePreset themePreset = ThemePreset::AppleDark;';
if (!code.includes(oldSettingDefault)) {
    throw new Error('oldSettingDefault not found!');
}
code = code.replace(oldSettingDefault, newSettingDefault);

// 6. Inherit Theme auto-configuration in LoadSettings
const oldGraphiteTranslucency = `    if (next.themePreset == ThemePreset::Graphite && localOpacity < 0) {
        next.pillOpacity = 0.88f;
    }`;

const newThemeAutoConfig = `    if (next.themePreset == ThemePreset::AppleDark) {
        if (localOpacity < 0) {
            next.pillOpacity = 1.0f;
        }
        next.materialDepth = false;
        next.dropShadow = false;
        next.accentBloom = 0.0f;
    } else if ((next.themePreset == ThemePreset::Graphite || next.themePreset == ThemePreset::LiquidGlass) && localOpacity < 0) {
        next.pillOpacity = 0.88f;
    }`;

if (!code.includes(oldGraphiteTranslucency)) {
    throw new Error('oldGraphiteTranslucency not found!');
}
code = code.replace(oldGraphiteTranslucency, newThemeAutoConfig);

// 7. ThemeIndexFromId alias recognition
const oldThemeAliases = `    if (iequals(id, L"oled-black")) return 0;     // -> Obsidian`;
const newThemeAliases = `    if (iequals(id, L"apple") || iequals(id, L"apple-dark") || iequals(id, L"appledark") || iequals(id, L"ios")) return 9; // -> AppleDark
    if (iequals(id, L"liquidglass") || iequals(id, L"liquid-glass")) return 8; // -> LiquidGlass
    if (iequals(id, L"oled-black")) return 0;     // -> Obsidian`;

if (!code.includes(oldThemeAliases)) {
    throw new Error('oldThemeAliases not found!');
}
code = code.replace(oldThemeAliases, newThemeAliases);

// 8. Eliminate muddy drop shadow halo for razor-sharp Apple edges
const oldDropShadowMeta = `  - DropShadow: true
    $name: Soft drop shadow
    $description: Casts a soft shadow beneath the island to lift it off the desktop.`;
const newDropShadowMeta = `  - DropShadow: false
    $name: Soft drop shadow
    $description: Casts a soft shadow beneath the island to lift it off the desktop. Turned off for razor-sharp Apple Dynamic Island edges.`;
code = code.replace(oldDropShadowMeta, newDropShadowMeta);

code = code.replace(
    'bool dropShadow = true;         // soft shadow under the island',
    'bool dropShadow = false;        // soft shadow under the island (disabled for razor-sharp contour)'
);

// 9. Clean bypass of DrawSoftShadow (stops all fuzzy halo pixels in padding)
const oldShadowImpl = `    void DrawSoftShadow(D2D1_RECT_F rect, float radius) {
        // A backdrop material clips the window to the island's silhouette, so
        // anything drawn out in the padding would be cut off anyway.
        if (!g_settings.dropShadow || g_settings.backdropMaterial != BackdropMaterial::None) {
            return;
        }

        const float spread = Clamp(14.0f * g_settings.sizeScale, 6.0f, kRenderPadY - 4.0f);
        const float yOffset = spread * 0.35f;
        constexpr int kSteps = 7;

        ComPtr<ID2D1SolidColorBrush> brush;
        if (FAILED(target_->CreateSolidColorBrush(material_.shadow, &brush)) || !brush) {
            return;
        }

        for (int i = kSteps; i >= 1; --i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSteps);
            const float grow = spread * t;
            // Quadratic falloff keeps the core dense and the outer edge feathered.
            const float alpha = material_.shadow.a * (1.0f - t) * (1.0f - t) * 0.55f * settingsOpacity_;
            if (alpha <= 0.002f) {
                continue;
            }
            brush->SetOpacity(alpha);

            const D2D1_RECT_F shadowRect = D2D1::RectF(
                rect.left - grow, rect.top - grow * 0.55f + yOffset,
                rect.right + grow, rect.bottom + grow + yOffset);
            FillIslandShape(shadowRect, radius + grow, g_settings.w11Style,
                            g_settings.notchStyle, brush.Get());
        }
        brush->SetOpacity(1.0f);
    }`;

const newShadowImpl = `    // Soft drop shadow is cleanly bypassed. Concentric multi-step fill approximation
    // produces a muddy, dirty stepped halo over dark windows and wallpapers.
    // Bypassing guarantees the surrounding render padding (kRenderPadX, kRenderPadY)
    // clears to absolute alpha 0.0, giving the island crisp, razor-sharp edges
    // with zero outer halos or rectangular artifacts.
    void DrawSoftShadow(D2D1_RECT_F rect, float radius) {
        UNREFERENCED_PARAMETER(rect);
        UNREFERENCED_PARAMETER(radius);
        return;
    }`;

if (!code.includes(oldShadowImpl)) {
    throw new Error('oldShadowImpl not found!');
}
code = code.replace(oldShadowImpl, newShadowImpl);

// 10. Apple hairline stroke width (0.5px) in DrawPillSurface
const oldStrokeWidth = `            float strokeWidth = settings.w11Style ? 1.0f : 0.8f;`;
const newStrokeWidth = `            float strokeWidth = settings.w11Style ? 1.0f : (settings.themePreset == ThemePreset::AppleDark ? 0.5f : 0.8f);`;
if (!code.includes(oldStrokeWidth)) {
    throw new Error('oldStrokeWidth not found!');
}
code = code.replace(oldStrokeWidth, newStrokeWidth);

// 11. Authentic Apple Privacy Dot Colors
code = code.replace(
    'D2D1_COLOR_F micDotColor = D2D1::ColorF(1.0f, 0.584f, 0.0f, 1.0f); // #FF9500',
    'D2D1_COLOR_F micDotColor = D2D1::ColorF(1.0f, 0.624f, 0.039f, 1.0f); // Apple systemOrange #FF9F0A'
);
code = code.replace(
    'D2D1_COLOR_F camDotColor = D2D1::ColorF(0.204f, 0.780f, 0.349f, 1.0f); // #34C759',
    'D2D1_COLOR_F camDotColor = D2D1::ColorF(0.188f, 0.820f, 0.345f, 1.0f); // Apple systemGreen #30D158'
);

// Restore CRLF if needed
if (isCRLF) {
    code = code.replace(/\n/g, '\r\n');
}

fs.writeFileSync(dstPath, code, 'utf8');
console.log('SUCCESS: Generated master dynamic-island-jedmamosto-fork.wh.cpp with Apple Dark fidelity and zero-halo rendering.');
