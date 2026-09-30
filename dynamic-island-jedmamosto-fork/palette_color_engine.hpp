#pragma once

#ifndef PALETTE_COLOR_ENGINE_HPP
#define PALETTE_COLOR_ENGINE_HPP

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
#include <d2d1.h>
#include <dwrite.h>
#include <cmath>
#include <algorithm>
#include <string>
#include <string_view>
#include <array>
#include <optional>

#include "island_common.hpp"

// ── Color Space & Conversion Functions ────────────────────────────────────────

inline D2D1_COLOR_F ColorFromHex(std::wstring text, D2D1_COLOR_F fallback) {
    // Trim surrounding whitespace: a stray space used to make the whole value
    // fail to parse and silently fall back to the default color.
    const size_t firstChar = text.find_first_not_of(L" \t\r\n");
    if (firstChar == std::wstring::npos) {
        return fallback;
    }
    text = text.substr(firstChar, text.find_last_not_of(L" \t\r\n") - firstChar + 1);

    if (!text.empty() && text[0] == L'#') {
        text.erase(text.begin());
    }

    // Accepted forms: RGB, RGBA, RRGGBB, RRGGBBAA. The alpha-bearing forms are
    // how a translucent island background is specified independently of the
    // global pill transparency slider.
    const size_t digits = text.size();
    if (digits != 3 && digits != 4 && digits != 6 && digits != 8) {
        return fallback;
    }
    if (text.find_first_not_of(L"0123456789abcdefABCDEF") != std::wstring::npos) {
        return fallback;
    }

    auto nibble = [](wchar_t c) -> int {
        if (c >= L'0' && c <= L'9') return c - L'0';
        if (c >= L'a' && c <= L'f') return c - L'a' + 10;
        return c - L'A' + 10;
    };

    int r = 0, g = 0, b = 0, a = 255;
    if (digits == 3 || digits == 4) {
        // Shorthand, each digit doubled so 'f' means 0xff.
        r = nibble(text[0]) * 17;
        g = nibble(text[1]) * 17;
        b = nibble(text[2]) * 17;
        if (digits == 4) {
            a = nibble(text[3]) * 17;
        }
    } else {
        r = nibble(text[0]) * 16 + nibble(text[1]);
        g = nibble(text[2]) * 16 + nibble(text[3]);
        b = nibble(text[4]) * 16 + nibble(text[5]);
        if (digits == 8) {
            a = nibble(text[6]) * 16 + nibble(text[7]);
        }
    }

    return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

inline D2D1_COLOR_F WithAlpha(D2D1_COLOR_F c, float alpha) {
    c.a = Clamp(alpha, 0.0f, 1.0f);
    return c;
}

inline D2D1_COLOR_F MixColor(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b, float t) {
    return D2D1::ColorF(a.r + (b.r - a.r) * t,
                        a.g + (b.g - a.g) * t,
                        a.b + (b.b - a.b) * t,
                        a.a + (b.a - a.a) * t);
}

inline float HueToRgb(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

inline D2D1_COLOR_F HslToRgb(float h, float s, float l, float a = 1.0f) {
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    s = Clamp(s, 0.0f, 1.0f);
    l = Clamp(l, 0.0f, 1.0f);

    if (s <= 1e-5f) {
        return D2D1::ColorF(l, l, l, a);
    }

    const float q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
    const float p = 2.0f * l - q;
    const float hNorm = h / 360.0f;

    const float r = Clamp(HueToRgb(p, q, hNorm + 1.0f / 3.0f), 0.0f, 1.0f);
    const float g = Clamp(HueToRgb(p, q, hNorm), 0.0f, 1.0f);
    const float b = Clamp(HueToRgb(p, q, hNorm - 1.0f / 3.0f), 0.0f, 1.0f);

    return D2D1::ColorF(r, g, b, a);
}

inline void RgbToHsl(float r, float g, float b, float& h, float& s, float& l) {
    r = Clamp(r, 0.0f, 1.0f);
    g = Clamp(g, 0.0f, 1.0f);
    b = Clamp(b, 0.0f, 1.0f);

    const float maxVal = std::max({r, g, b});
    const float minVal = std::min({r, g, b});
    const float delta = maxVal - minVal;

    l = (maxVal + minVal) * 0.5f;

    if (delta <= 1e-5f) {
        h = 0.0f;
        s = 0.0f;
        return;
    }

    s = (l > 0.5f) ? (delta / (2.0f - maxVal - minVal)) : (delta / (maxVal + minVal));

    if (maxVal == r) {
        h = ((g - b) / delta) + (g < b ? 6.0f : 0.0f);
    } else if (maxVal == g) {
        h = ((b - r) / delta) + 2.0f;
    } else {
        h = ((r - g) / delta) + 4.0f;
    }
    h *= 60.0f;
    if (h < 0.0f) h += 360.0f;
    if (h >= 360.0f) h -= 360.0f;
}

inline double RelativeLuminance(D2D1_COLOR_F c) {
    auto toLinear = [](float channel) -> double {
        const double v = Clamp(channel, 0.0f, 1.0f);
        return (v <= 0.04045) ? (v / 12.92) : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * toLinear(c.r) + 0.7152 * toLinear(c.g) + 0.0722 * toLinear(c.b);
}

inline double ContrastRatio(D2D1_COLOR_F a, D2D1_COLOR_F b) {
    const double lumA = RelativeLuminance(a);
    const double lumB = RelativeLuminance(b);
    return (std::max(lumA, lumB) + 0.05) / (std::min(lumA, lumB) + 0.05);
}

// Nudges an accent until it clears 3:1 against the surface it will sit on.
// The step direction is chosen from the background's luminance so light themes
// keep a legible accent.
inline D2D1_COLOR_F EnsureContrastAgainstBackground(D2D1_COLOR_F candidate, D2D1_COLOR_F bgColor) {
    float h = 0.0f, s = 0.0f, l = 0.0f;
    RgbToHsl(candidate.r, candidate.g, candidate.b, h, s, l);

    if (s > 0.01f) {
        s = std::max(s, 0.35f);
    }

    const double bgLum = RelativeLuminance(bgColor);
    const bool onDark = bgLum < 0.45;

    // Clamp toward the half that can actually move away from the background.
    l = onDark ? std::min(l, 0.85f) : std::max(l, 0.15f);

    const float step = onDark ? 0.02f : -0.02f;
    const float limit = onDark ? 0.95f : 0.06f;

    candidate = HslToRgb(h, s, l, candidate.a);

    double contrast = ContrastRatio(candidate, bgColor);
    // Bounded by `limit` in both directions, so this terminates either way.
    while (contrast < 3.0 && (onDark ? (l < limit) : (l > limit))) {
        l += step;
        candidate = HslToRgb(h, s, l, candidate.a);
        contrast = ContrastRatio(candidate, bgColor);
    }

    return candidate;
}

inline D2D1_COLOR_F GetSystemAccentColor() {
    DWORD color = 0;
    BOOL opaque = FALSE;
    using DwmGetColorizationColor_t = HRESULT(WINAPI*)(DWORD*, BOOL*);
    auto proc = reinterpret_cast<DwmGetColorizationColor_t>(
        GetProcAddress(GetModuleHandleW(L"dwmapi.dll"), "DwmGetColorizationColor"));

    if (proc && SUCCEEDED(proc(&color, &opaque))) {
        return D2D1::ColorF(
            ((color >> 16) & 0xff) / 255.0f,
            ((color >> 8) & 0xff) / 255.0f,
            (color & 0xff) / 255.0f,
            1.0f);
    }

    return D2D1::ColorF(0x4cc9f0);
}

// ── Dominant Color Extraction ────────────────────────────────────────────────

// Extracts the dominant colorful accent from BitmapPixels using 16x16x16 bucket
// clustering, vibrancy weighting, and neighbor-bucket pooling, then ensures contrast
// against the island background.
inline D2D1_COLOR_F ExtractDominantColor(BitmapPixels& pixels, D2D1_COLOR_F bgColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)) {
    if (pixels.bgra.empty() || !pixels.width || !pixels.height) {
        return pixels.sampledAccent;
    }

    if (bgColor.a <= 0.0f) {
        bgColor = g_settings.pillBgColor;
        if (bgColor.a <= 0.0f) {
            bgColor = D2D1::ColorF(0.031f, 0.031f, 0.039f, 1.0f); // Obsidian default #08080A
        }
    }

    struct Bucket {
        uint32_t count = 0;
        uint32_t r = 0;
        uint32_t g = 0;
        uint32_t b = 0;
        uint32_t satSum = 0;  // accumulated saturation for vibrancy-weighted winner
    };

    std::array<Bucket, 16 * 16 * 16> buckets{};
    for (size_t i = 0; i + 3 < pixels.bgra.size(); i += 4) {
        const uint8_t alpha = pixels.bgra[i + 3];
        const uint8_t blue  = pixels.bgra[i + 0];
        const uint8_t green = pixels.bgra[i + 1];
        const uint8_t red   = pixels.bgra[i + 2];
        if (alpha < 32) {
            continue;
        }

        const int maxc = std::max({red, green, blue});
        const int minc = std::min({red, green, blue});
        const int luminance = (54 * red + 183 * green + 19 * blue) / 256;
        const int saturation = maxc - minc;
        // Reject near-black, near-white, and near-gray pixels.
        // Raise luminance floor slightly (36 instead of 28) so very dark-but-colorful
        // pixels don't swamp vibrancy scoring on dark-mood album art.
        if (luminance < 36 || luminance > 220 || saturation < 28) {
            continue;
        }

        const size_t bucketIndex = ((red >> 4) << 8) | ((green >> 4) << 4) | (blue >> 4);
        Bucket& bucket = buckets[bucketIndex];
        const uint32_t weight = 1 + static_cast<uint32_t>(saturation / 32);  // stronger vibrancy weight
        bucket.count  += weight;
        bucket.r      += red   * weight;
        bucket.g      += green * weight;
        bucket.b      += blue  * weight;
        bucket.satSum += static_cast<uint32_t>(saturation) * weight;
    }

    // --- Vibrancy-weighted winner selection ---
    // Raw frequency alone lets large muted regions (e.g. a beige background)
    // beat a smaller vivid color that's visually "the" accent.
    // Strategy: collect the top 3 by count, then pick whichever has the
    // highest average saturation — cheap, no extra image pass required.
    struct Candidate { const Bucket* b = nullptr; size_t idx = 0; };
    Candidate top[3];
    for (size_t i = 0; i < buckets.size(); ++i) {
        const Bucket& bk = buckets[i];
        if (bk.count == 0) continue;
        for (int s = 0; s < 3; ++s) {
            if (!top[s].b || bk.count > top[s].b->count) {
                for (int t = 2; t > s; --t) top[t] = top[t - 1];
                top[s] = {&bk, i};
                break;
            }
        }
    }

    // Among those top-3, pick the one with the highest avg saturation.
    const Bucket* best = nullptr;
    size_t bestIdx = 0;
    float bestVibrancy = -1.0f;
    for (int s = 0; s < 3; ++s) {
        if (!top[s].b) break;
        const float vibrancy = static_cast<float>(top[s].b->satSum) /
                               static_cast<float>(top[s].b->count);
        if (vibrancy > bestVibrancy) {
            bestVibrancy = vibrancy;
            best = top[s].b;
            bestIdx = top[s].idx;
        }
    }

    if (best && best->count > 0) {
        // --- Neighbor-bucket merging ---
        // A color straddling a bucket boundary splits its votes across up to 8
        // adjacent cells. Pool the winning bucket with all 26 face/edge/corner
        // neighbors before averaging so the final RGB is stable and representative.
        uint64_t poolCount = 0;
        double poolR = 0, poolG = 0, poolB = 0;

        const int bi = static_cast<int>((bestIdx >> 8) & 0xF);  // red index
        const int gi = static_cast<int>((bestIdx >> 4) & 0xF);  // green index
        const int bli = static_cast<int>( bestIdx       & 0xF); // blue index

        for (int dr = -1; dr <= 1; ++dr) {
            for (int dg = -1; dg <= 1; ++dg) {
                for (int db = -1; db <= 1; ++db) {
                    const int ni = bi + dr, nj = gi + dg, nk = bli + db;
                    if (ni < 0 || ni > 15 || nj < 0 || nj > 15 || nk < 0 || nk > 15) continue;
                    const size_t nIdx = (static_cast<size_t>(ni) << 8) |
                                        (static_cast<size_t>(nj) << 4) |
                                         static_cast<size_t>(nk);
                    const Bucket& nb = buckets[nIdx];
                    if (nb.count == 0) continue;
                    poolCount += nb.count;
                    poolR += nb.r;
                    poolG += nb.g;
                    poolB += nb.b;
                }
            }
        }

        if (poolCount > 0) {
            const float invC = 1.0f / static_cast<float>(poolCount);
            const float rawR = static_cast<float>(poolR * invC) / 255.0f;
            const float rawG = static_cast<float>(poolG * invC) / 255.0f;
            const float rawB = static_cast<float>(poolB * invC) / 255.0f;

            pixels.sampledAccent = EnsureContrastAgainstBackground(
                D2D1::ColorF(rawR, rawG, rawB, 1.0f),
                bgColor);
        }
    }

    return pixels.sampledAccent;
}

// ── Material Token System ───────────────────────────────────────────────────

// Every surface in the island is built from one small token set so the whole
// UI shares a single visual language. Tokens adapt to background luminance so
// a light custom background gets dark separators instead of washed-out white ones.
struct MaterialTokens {
    D2D1_COLOR_F base{};          // the pill's own fill
    D2D1_COLOR_F raised{};        // inner cards, chips, wells
    D2D1_COLOR_F raisedStrong{};  // pressed / active chips
    D2D1_COLOR_F hairline{};      // 1px separators between content
    D2D1_COLOR_F stroke{};        // outer contour
    D2D1_COLOR_F shadow{};        // drop shadow tint
    D2D1_COLOR_F textPrimary{};
    D2D1_COLOR_F textSecondary{};
    D2D1_COLOR_F textTertiary{};
    D2D1_COLOR_F accent{};
    D2D1_COLOR_F accentSoft{};
    bool onDark = true;
    float opacity = 1.0f;
};

inline MaterialTokens BuildMaterialTokens(const Settings& settings, D2D1_COLOR_F currentAccent, float opacity) {
    MaterialTokens t;
    t.opacity = opacity;
    t.base = settings.pillBgColor;

    // Light surfaces need dark scrims/edges, dark surfaces need light ones.
    const bool onDark = RelativeLuminance(t.base) < 0.45;
    t.onDark = onDark;

    const D2D1_COLOR_F white = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    const D2D1_COLOR_F black = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    const D2D1_COLOR_F lift = onDark ? white : black;

    // Cards carry a slightly stronger fill than before. They used to be
    // outlined with a hairline ring, and losing that ring is what the fill
    // now has to compensate for so a card still reads as a distinct surface.
    t.raised = WithAlpha(lift, onDark ? 0.085f : 0.062f);
    t.raisedStrong = WithAlpha(lift, onDark ? 0.150f : 0.105f);
    t.hairline = WithAlpha(lift, onDark ? 0.100f : 0.085f);
    t.shadow = WithAlpha(black, 0.55f);

    t.stroke = settings.contourBorderColor;
    t.accent = currentAccent;
    t.accentSoft = WithAlpha(currentAccent, 0.18f);

    t.textPrimary = WithAlpha(settings.textPrimaryColor, 0.98f);
    t.textSecondary = WithAlpha(settings.textSecondaryColor, 0.88f);
    // Derived rather than configured, so a third tier always reads as
    // quieter than "secondary" whatever the user picked.
    t.textTertiary = WithAlpha(MixColor(settings.textSecondaryColor, t.base, 0.35f), 0.72f);

    return t;
}

inline MaterialTokens ResolveMaterialTokens(const Settings& settings,
                                            const SharedState& state,
                                            std::optional<D2D1_COLOR_F> customAccentOverride = std::nullopt) {
    D2D1_COLOR_F targetAccent = settings.customAccent;
    if (customAccentOverride.has_value()) {
        targetAccent = *customAccentOverride;
    } else if (settings.accentMode == AccentMode::System) {
        targetAccent = GetSystemAccentColor();
    } else if (settings.accentMode == AccentMode::Auto && !state.media.art.bgra.empty()) {
        targetAccent = state.media.art.sampledAccent;
    } else {
        targetAccent = EnsureContrastAgainstBackground(targetAccent, settings.pillBgColor);
    }

    return BuildMaterialTokens(settings, targetAccent, settings.pillOpacity);
}

#endif // PALETTE_COLOR_ENGINE_HPP
