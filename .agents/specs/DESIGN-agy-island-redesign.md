# Visual Design Specification: AGY 2.0 Dynamic Island Redesign

**Spec ID**: `DESIGN-agy-island-redesign-001`  
**Status**: `APPROVED`  
**Author**: `ui-art-director` (Principal Design Systems Architect & Art Director)  
**Date**: 2026-09-30  
**Target Mod**: `dynamic-island-jedmamosto-fork` (Windhawk Direct2D / DirectWrite)  
**Reference Snapshot**: [`snapshot_agy.png`](file:///c:/Users/ASUS/Personal%20Windhawk%20Mods/snapshot_agy.png)  
**Interactive Mockup**: [`agy_island_mockup.html`](file:///C:/Users/ASUS/.gemini/antigravity/brain/d42ea03e-3357-4d18-93a4-ee4503963b71/agy_island_mockup.html)  

---

## 1. Executive Summary & Root Cause Analysis

### 1.1 Status-Quo Defect: Collapsed Notch Border Clipping
- **Root Cause**: In the collapsed notch/satellite pill (height $H = 34\text{px}$, corner radius $R = 17\text{px}$), the previous code placed a secondary dot at offset $(X_c + 11.9\text{px}, Y_c - 11.9\text{px})$ with dot radius $2.8\text{px}$.
- **Geometric Collision**: At vertical offset $\Delta y = 11.9\text{px}$, the capsule boundary has horizontal distance from the cap arc center of only $\sqrt{17^2 - 11.9^2} = 12.1\text{px}$. With the arc center at $X_c + 6\text{px}$, the pill's curved perimeter is only $18.1\text{px}$ from the pill center. Any element extending beyond $14.5\text{px}$ clips or visually bleeds directly into the white hairline border.
- **Architectural Fix**: 
  - Strict **Internal Safe Inset** enforced: All graphics must remain strictly within an inner safe radius $R_{\text{safe}} = 10.0\text{px}$ centered at $(X_c, Y_c)$.
  - Outer ambient ring radius is clamped to $9.5\text{px}$ (stroke $1.8\text{px}$), maintaining an absolute clearance of $\ge 7.5\text{px}$ to the nearest capsule border at all angles.

---

## 2. ASCII Wireframe Architecture

### 2.1 Collapsed Mode (Pill A: Compact 54×34px & Pill B: Extended 148×34px)

```
+-- Pill A (Compact 54x34) --+     +----- Pill B (Extended Telemetry 148x34) -----+
|       .---------.          |     |  .---------.                                 |
|      (   ( O )   )         |     | (   ( O )   )  Turn 5 * Step 140             |
|       `---------'          |     |  `---------'                                 |
+----------------------------+     +----------------------------------------------+
(R=17px, Orb R=4px, Arc R=9.5px)   (R=17px, Orb centered at Left+17px, Text 10px)
```

### 2.2 Expanded Mode (AGY 2.0 Dashboard 420×230px)

```
+--------------------------------------------------------------------------------+
|  * Personal Windhawk Mods                             ( * Turn 5 * Step 140 )  |  <- Zone A: Header (24px)
+--------------------------------------------------------------------------------+
|  [ AGENT ]  Just now                                                           |
|  Rendered Direct2D offscreen snapshot with zero notch border clipping.         |  <- Zone B: Message Box (50px)
+--------------------------------------------------------------------------------+
|  +-- 5-Hour Limit --------+  +-- Weekly Limit ---------+                       |
|  |   .---.   5-HOUR LIMIT |  |   .---.   WEEKLY LIMIT  |                       |
|  |  ( 80% )  80% remaining|  |  ( 48% )  48% remaining |                       |  <- Zone C: Quota Bento (64px)
|  |   `---'   Resets 4h 27m|  |   `---'   Resets 5d 23h |                       |
|  +------------------------+  +-------------------------+                       |
+--------------------------------------------------------------------------------+
|  CONTEXT TOKENS                         156k / 200k (78%) * Compaction at 80%  |
|  [======================================|-------] (Threshold Hairline at 80%)  |  <- Zone D: Compaction Bar (36px)
+--------------------------------------------------------------------------------+
```

---

## 3. W3C DTCG Semantic Design Tokens

```json
{
  "color": {
    "surface": {
      "canvas": { "$value": "#0C0E14", "$type": "color" },
      "pill-base": { "$value": "rgba(15, 17, 24, 0.96)", "$type": "color" },
      "card-base": { "$value": "rgba(255, 255, 255, 0.055)", "$type": "color" },
      "card-raised": { "$value": "rgba(255, 255, 255, 0.090)", "$type": "color" },
      "card-hairline": { "$value": "rgba(255, 255, 255, 0.085)", "$type": "color" },
      "card-hairline-high": { "$value": "rgba(255, 255, 255, 0.160)", "$type": "color" },
      "track-bg": { "$value": "rgba(255, 255, 255, 0.090)", "$type": "color" }
    },
    "ink": {
      "primary": { "$value": "#FFFFFF", "$type": "color" },
      "secondary": { "$value": "rgba(255, 255, 255, 0.60)", "$type": "color" },
      "tertiary": { "$value": "rgba(255, 255, 255, 0.38)", "$type": "color" }
    },
    "status": {
      "apple-green": { "$value": "#34C759", "$type": "color" },
      "apple-amber": { "$value": "#FFB021", "$type": "color" },
      "apple-red": { "$value": "#FF5952", "$type": "color" },
      "gemini-cyan": { "$value": "#4CC9F0", "$type": "color" },
      "gemini-purple": { "$value": "#9B72CB", "$type": "color" },
      "user-indigo": { "$value": "#818CF8", "$type": "color" }
    }
  },
  "typography": {
    "title": { "fontFamily": "Segoe UI", "fontSize": "13px", "fontWeight": 700 },
    "body": { "fontFamily": "Segoe UI", "fontSize": "11px", "fontWeight": 400 },
    "pill": { "fontFamily": "Segoe UI", "fontSize": "10px", "fontWeight": 600 },
    "label": { "fontFamily": "Segoe UI", "fontSize": "8.5px", "fontWeight": 700 }
  }
}
```

---

## 4. Direct2D Micro-Layout Coordinates (1.0x Scale)

| Component | Target Primitive | Coordinate / Rect Bounds | Style & Ink Notes |
| :--- | :--- | :--- | :--- |
| **Collapsed Notch (Compact)** | `RoundedRect` | `RectF(X, Y, X + 54, Y + 34)`, `R = 17.0f` | Obsidian glass fill + 1.0px hairline |
| **Collapsed Status Orb** | `FillEllipse` | Center `(X + 27, Y + 17)`, Radius `4.0f` | Emerald `#34C759` or Amber `#FFB021` |
| **Collapsed Status Ring** | `DrawGeometry` | Center `(X + 27, Y + 17)`, Radius `9.5f`, Stroke `1.8f` | Clearance to capsule boundary $> 7.5\text{px}$ |
| **Expanded Island Base** | `RoundedRect` | `RectF(X, Y, X + 420, Y + 230)`, `R = 20.0f` | 10px soft ambient drop shadow |
| **Zone A: Sparkle Icon** | `DrawText` | `RectF(X + 18, Y + 16, X + 36, Y + 40)` | Glyph `✦`, Cyan `#4CC9F0` |
| **Zone A: Conversation Title**| `DrawText` | `RectF(X + 38, Y + 16, X + 260, Y + 40)` | Segoe UI 13px Bold, `#FFFFFF` |
| **Zone A: Turn/Step Pill** | `RoundedRect` | `RectF(X + 266, Y + 17, X + 402, Y + 39)`, `R = 11.0f` | 3.5px pulsing dot + Segoe UI 10px |
| **Zone B: Message Bento** | `RoundedRect` | `RectF(X + 18, Y + 50, X + 402, Y + 100)`, `R = 9.0f` | 5.5% white glass container |
| **Zone B: Role Tag** | `RoundedRect` | `RectF(X + 28, Y + 58, X + 74, Y + 73)`, `R = 4.0f` | Agent: `#34C759` badge, User: `#818CF8` |
| **Zone C: 5-Hour Quota** | Bento Card | `RectF(X + 18, Y + 108, X + 206, Y + 172)`, `R = 9.0f` | Ring Radius `18.0f`, Stroke `3.2f`, Cyan `#4CC9F0` |
| **Zone C: Weekly Quota** | Bento Card | `RectF(X + 214, Y + 108, X + 402, Y + 172)`, `R = 9.0f` | Ring Radius `18.0f`, Stroke `3.2f`, Purple `#9B72CB` |
| **Zone D: Compaction Box** | Bento Card | `RectF(X + 18, Y + 180, X + 402, Y + 216)`, `R = 9.0f` | Text + Progress Track + Threshold Line |
| **Zone D: 80% Threshold** | `DrawLine` | $X = \text{Left} + 10 + 364 \times 0.80 = X + 319.2$, $Y = [Y+200, Y+208]$ | Red `#FF5952` 1.5px vertical line |

---

## 5. Verification Checklist

- [x] Notch circle overlap defect solved: internal clearance $\ge 7.5\text{px}$ across all angles.
- [x] Conversation title and active turn/step badge legibly anchored in header.
- [x] Message snippet with clear `AGENT` / `USER` role attribution badge.
- [x] Dual Gemini quota progress rings (5-Hour and Weekly) matching Google Antigravity quota UI.
- [x] Early token compaction warning track with 80% threshold marker.
- [x] Clang++ Direct2D offscreen snapshot compiled and verified (`snapshot_agy.png` exiting code 0).
- [x] Self-contained interactive HTML mockup artifact deployed in brain artifacts.
