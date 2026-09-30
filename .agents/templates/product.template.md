---
# Specification: https://product.md/
$id: https://example.com/product.md
$type: Product
$context: https://schema.org.ai
productmd: 1           # Spec format version (fixed)
version: "1.0.0"       # Product release version (bump on release)
register: product
---

# Acme App

Operational workflow automation for distributed engineering teams. One dashboard, zero status meetings.

## Register

product

## Users

Senior software engineers and engineering leads managing 3 to 10 microservices. They prioritize deterministic command-line feedback, keyboard shortcuts, and deep terminal integration over dense GUI wizards.

## Problem

Engineering teams lose 5+ hours weekly manually updating issue trackers, coordinating release status across chats, and parsing disconnected CI logs.

## Product Purpose

- **Mission**: Eliminate manual status reporting and synchronize developer intent directly with production reality.
- **Core Value Proposition**: Real-time delivery tracking synthesized autonomously from git commits and test suites.
- **Success Criteria**: 90%+ engineering updates generated without manual context switching.

## Brand Personality

Direct, disciplined, quiet. The voice of an experienced staff engineer who values clarity over hype.

### Tone

Active voice. Sentences strictly under 20 words. No unbacked superlatives. Status messages always state the concrete next step.

## Anti-references

- MUST NOT use generic AI marketing hype ("transform your synergy").
- MUST NOT default to dark mode without full sunlight contrast verification.
- Generated output MUST NOT use dense multi-step configuration wizards.

## Design Principles

1. **Show Real Work Fast**: Eliminate onboarding fluff; present active telemetry immediately.
2. **Deterministic Clarity**: Every state transition and error condition must be explicit.
3. **Restraint Over Decoration**: Use whitespace and typographic hierarchy instead of ornaments.
4. **Resilient Ergonomics**: Keyboard-first navigation and clear tab indices throughout.

## Accessibility & Inclusion

- **Target Standard**: WCAG 2.1 Level AA compliance across all surfaces.
- **Visual Contrast**: 4.5:1 minimum text contrast; 3:1 for graphical UI elements.
- **Accommodations**: Support `prefers-reduced-motion` and clear keyboard focus rings.

## Offer

Acme Developer is $19/seat/month. The telemetry collector API is metered with a hard ceiling.

```json product.md#pricing
{ "model": "subscription", "price": 19, "unit": "usd-per-month" }
```

## Boundaries

- Not a general-purpose project management suite (e.g. Jira replacement).
- Not a timesheet or employee surveillance tool; we never track keystrokes or activity minutes.

## Stack

- [AGENTS.md](AGENTS.md): Build conventions and runtime agent constraints.
- [DESIGN.md](DESIGN.md): Visual hierarchy, tokens, and Google Stitch design system.
