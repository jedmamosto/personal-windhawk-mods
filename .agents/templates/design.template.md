---
# Specification: https://stitch.withgoogle.com/docs/design-md/specification
# Reference: https://github.com/google-labs-code/design.md
version: "alpha"          # Google Stitch format version (fixed)
system_version: "1.0.0"   # Design system release version (bump on release)
name: "Acme Design System"
description: "Production design tokens conforming to Google Stitch specification"
colors:
  primary: "#1e293b"
  secondary: "#64748b"
  accent: "#2563eb"
  neutral-bg: "#ffffff"
  neutral-surface: "#f8fafc"
  neutral-border: "#e2e8f0"
  neutral-text: "#0f172a"
  neutral-muted: "#64748b"
typography:
  h1:
    fontFamily: "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"
    fontSize: "2.5rem"
    fontWeight: 700
    lineHeight: 1.15
    letterSpacing: "-0.02em"
  h2:
    fontFamily: "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"
    fontSize: "1.5rem"
    fontWeight: 600
    lineHeight: 1.25
    letterSpacing: "-0.01em"
  body:
    fontFamily: "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif"
    fontSize: "1rem"
    fontWeight: 400
    lineHeight: 1.5
    letterSpacing: "normal"
rounded:
  sm: "4px"
  md: "8px"
  lg: "12px"
spacing:
  xs: "4px"
  sm: "8px"
  md: "16px"
  lg: "24px"
  xl: "32px"
components:
  button-primary:
    backgroundColor: "{colors.accent}"
    textColor: "{colors.neutral-bg}"
    rounded: "{rounded.md}"
    padding: "8px 16px"
  button-secondary:
    backgroundColor: "transparent"
    textColor: "{colors.neutral-text}"
    rounded: "{rounded.md}"
    padding: "8px 16px"
  card-surface:
    backgroundColor: "{colors.neutral-surface}"
    textColor: "{colors.neutral-text}"
    rounded: "{rounded.lg}"
    padding: "24px"
---

# Design System Specification

## Overview

Disciplined, high-contrast visual hierarchy prioritizing operational clarity over decorative clutter.

## Colors

The palette enforces the 90/10 neutral-to-accent rule:
- **Canvas & Surface**: High legibility base (`{colors.neutral-bg}`) with structural cards (`{colors.neutral-surface}`) and crisp hairlines (`{colors.neutral-border}`).
- **Primary Ink**: Deep slate (`{colors.neutral-text}`) for headlines; muted slate (`{colors.neutral-muted}`) for metadata.
- **Accent**: Electric blue (`{colors.accent}`) reserved strictly for primary interactive actions ($\le 5$ per viewport).

## Typography

Typographic scale and hierarchy:
- **Headings**: Tightly tracked headlines (`letterSpacing: -0.02em`) with strong presence.
- **Body**: Relaxed line height (1.5) optimized for long-form technical scanning.
- **Casing**: Sentence case default across all labels and controls. Title Case is forbidden.

## Layout

- **Spatial Rhythm**: 4px/8px incremental base spatial grid (`{spacing.xs}` to `{spacing.xl}`).
- **Container Sizing**: Fluid responsive boundaries with constrained max-width reading columns.

## Elevation & Depth

Avoid heavy drop-shadows:
- **Level 0 (Base)**: Flat canvas base.
- **Level 1 (Cards)**: Hairline 1px border (`{colors.neutral-border}`) with solid surface fill.
- **Level 2 (Popovers/Overlays)**: Subtle ambient blur (`0 4px 20px -2px rgba(0,0,0,0.08)`) with matching border.

## Shapes

- **Border Radii**: Strict hierarchical progression (`rounded.sm` for badges $\to$ `rounded.md` for inputs/buttons $\to$ `rounded.lg` for cards).

## Components

- **Buttons**:
  - `button-primary`: Solid accent fill with high-contrast text.
  - `button-secondary`: Hairline border with neutral text.
- **Cards**: Flat surface fill with 1px neutral border.
- **Iconography**: Clean Lucide SVG icons sized to 16px or 20px.

## Do's and Don'ts

- **DO**:
  - Use Sentence case for all headings and buttons.
  - Maintain $\ge 90\%$ grayscale neutral foundation with a single accent color.
  - Keep interactive transitions under 200ms with fast exit animations.
- **DON'T**:
  - DO NOT use generic purple-indigo neon gradients.
  - DO NOT exceed 5 saturated accent elements in a single viewport.
  - DO NOT float unbordered white surfaces over light gray backgrounds.
