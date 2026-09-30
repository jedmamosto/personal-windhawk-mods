---
# Specification: https://stitch.withgoogle.com/docs/design-md/specification
# Reference: https://github.com/google-labs-code/design.md
version: "alpha"
system_version: "1.0.0"
name: "Personal Windhawk Mods Design System"
description: "Production Direct2D design tokens conforming to Google Stitch specification"
colors:
  primary: "#ffffff"
  secondary: "rgba(255, 255, 255, 0.54)"
  tertiary: "rgba(255, 255, 255, 0.35)"
  card-base: "rgba(255, 255, 255, 0.06)"
  card-raised: "rgba(255, 255, 255, 0.09)"
  card-hairline: "rgba(255, 255, 255, 0.08)"
  track-bg: "rgba(255, 255, 255, 0.10)"
  apple-green: "#34C759"
  apple-amber: "#FF9500"
  apple-red: "#FF3B30"
  load-amber: "#FFB021"
  load-red: "#FF5952"
typography:
  header:
    fontFamily: "Segoe UI, -apple-system, sans-serif"
    fontSize: "13px"
    fontWeight: 700
    lineHeight: 1.2
  label:
    fontFamily: "Segoe UI, -apple-system, sans-serif"
    fontSize: "9px"
    fontWeight: 600
    letterSpacing: "0.04em"
  value:
    fontFamily: "Segoe UI, -apple-system, sans-serif"
    fontSize: "12px"
    fontWeight: 600
    lineHeight: 1.2
rounded:
  card: "9px"
  track: "1.25px"
spacing:
  pad-x: "24px"
  col-gap: "9px"
  row-gap: "6px"
  row-h: "38px"
components:
  bento-card:
    backgroundColor: "{colors.card-base}"
    borderColor: "{colors.card-hairline}"
    rounded: "{rounded.card}"
    padding: "0px"
---

# Design System Specification

## Overview

Direct2D overlay design system for Windhawk desktop mods. It prioritizes legibility, dark glass surfaces, and microsecond rendering.

## Colors

The palette enforces high-contrast dark glass hierarchy:
- **Canvas & Surface**: Translucent card base (`{colors.card-base}`), raised base (`{colors.card-raised}`), and 1px hairline (`{colors.card-hairline}`).
- **Text Ink**: Primary white (`{colors.primary}`, 96%), secondary muted (`{colors.secondary}`, 54%), and tertiary (`{colors.tertiary}`, 35%).
- **Semantic States**: Apple Green (`{colors.apple-green}`) for normal and charging. Amber (`{colors.load-amber}`) for warnings. Red (`{colors.load-red}`) for critical load.

## Typography

- **Header Title**: Left-aligned 13px bold title anchored at top + 16px.
- **Category Labels**: 9px uppercase tracking above value with 0.60f muted opacity.
- **Metric Values**: 12px semi-bold white text occupying the central band.

## Layout

The Hardware Monitor and Battery Bento enforce identical 2x3 grid geometry:
- **Spatial Rhythm**: 2 columns by 3 rows displaying 6 metric cards.
- **Margins & Gaps**: Left/right padding `pad-x: 24px`, column gap `col-gap: 9px`, row gap `row-gap: 6px`.
- **Card Sizing**: Fixed row height `row-h: 38px`. Card width equals `(totalWidth - 48px - 9px) / 2`.
- **Grid Origin**: Anchors at `top + 38px * scale` beneath the header.

## Elevation & Depth

- **Level 0 (Pill Base)**: Translucent acrylic surface fill with subtle soft shadow.
- **Level 1 (Cards)**: Translucent fill (`{colors.card-base}`) with 1px hairline border (`{colors.card-hairline}`).
- **Level 2 (Notch Outline)**: Adaptive 1.2px bright stroke on dark/black backgrounds when collapsed.

## Shapes

- **Card Radius**: Uniform `9px * scale` rounded rectangle.
- **Load Track Radius**: Compact `1.25px * scale` rounded corners on 2.5px height bars.

## Components

- **Metric Cards**: 38px height container with 15px icon anchor on left, label above value, and bottom load bar.
- **Icon Anchors**: Strict 15px bounding diameter centered at `(card.left + 16px, card.top + 15px)`.
- **Load Tracks**: Continuous 2.5px track for bounded metrics; omitted (`fraction = -1.0f`) for categorical modes like Power Mode.

## Do's and Don'ts

- **DO**: Use uppercase text for category labels with 0.60f opacity.
- **DO**: Keep 15px icon anchors strictly centered at `card.left + 16px`.
- **DO**: Omit progress bars for categorical states (Power Mode).
- **DON'T**: DO NOT use identical icons for Battery Level and Power Flow.
- **DON'T**: DO NOT compound label alpha twice.
