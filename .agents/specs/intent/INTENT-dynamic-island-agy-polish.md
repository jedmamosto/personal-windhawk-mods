---
title: "Feature Intent: Dynamic Island AGY Polish & Telemetry"
id: "INTENT-dynamic-island-agy-polish-001"
version: "1.2.0"
status: "APPROVED"
appetite: "SMALL_BATCH"
owner: "Jed"
created_at: "2026-10-01"
---

# Feature Intent: Dynamic Island AGY Polish & Telemetry

## 1. Problem Statement & Baseline Story
- **Who is Affected**: Developers monitoring Antigravity agent runs via Dynamic Island.
- **Status-Quo Failure**:
  - The Step Card shows redundant step numbers without actionable context.
  - The expanded island card has an awkward empty gap at the bottom.
  - The header displays only the folder path rather than the conversation title.
  - The compact notch uses a flat solid fill disconnected from the active theme.
  - Active Time measures elapsed wall-clock time instead of model computation time.
  - The AGY module parks during inactivity and fails to wake on new events.
  - Reading full transcripts repeatedly causes memory spikes and buffer growth.
- **Measurable Business Pain**:
  - Developers lose visibility into active tool operations and subagent roles.
  - Spiking RAM in the host process degrades desktop system responsiveness.
  - Hardcoded or flat visual styles clash with user theme selections like Apple 16 Dark.
  - The island fails to restore context during long sessions without manual reloads.
- **Target Outcome**:
  - Compact notch styled dynamically to match the active mod theme (e.g., Apple 16 Dark).
  - Live Tool & Role Execution Card replacing the redundant Step Card.
  - Dynamic container height clamping with zero dead space.
  - Dual header displaying project name and conversation title.
  - Accurate cumulative model active time.
  - Resilient event-driven resume with bounded memory usage.

---

## 2. Appetite & Resource Boundaries
- **Time Box / Appetite**: SMALL_BATCH (1-2 days).
- **Blast Radius**: Dynamic Island overlay rendering, AGY painters, and transcript ingestion.
- **Trade-Off Stance**:
  - Scope is fixed to the six polished AGY items.
  - If conversation titles exceed display limits, truncate with an ellipsis.

---

## 3. Rabbit Holes & Out-of-Scope (No-Gos)
- **No-Go 1 (Clipboard Widget)**:
  - Do NOT implement a clipboard manager or widget.
- **No-Go 2 (Wall-Clock Uptime)**:
  - Do NOT display idle session duration as active work time.
- **No-Go 3 (Monolithic AST Hooks)**:
  - Do NOT modify unrelated system hardware or battery telemetry hooks.
- **Rabbit Hole 1 (Hardcoded Theme Overrides)**:
  - Avoid hardcoding static acrylic colors that bypass user theme choices.
- **Rabbit Hole 2 (Full Transcript Buffering)**:
  - Avoid loading entire multi-megabyte JSONL files into memory.
  - Prevent RAM spikes by seeking and reading only tail lines.
- **Rabbit Hole 3 (Unbounded Memory Caches)**:
  - Avoid storing unbounded historical step lists in memory.

---

## 4. RFC 2119 Normative Invariants
- **MUST**:
  - Display both the project name and the active conversation title.
  - Calculate Active Time strictly from cumulative model generation intervals.
  - Clamp container height dynamically to eliminate empty bottom voids.
  - Replace the Step Card with an Active Execution Card showing subagent role and tool action.
  - Inherit and render styling from the active island theme (e.g., Apple 16 Dark).
  - Display dynamic status halos or accents consistent with active theme palettes.
  - Wake and resume rendering immediately upon new session activity.
  - Enforce bounded memory buffers to prevent RAM spikes.
- **MUST NOT**:
  - MUST NOT implement any clipboard widget or clipboard interception.
  - MUST NOT leave unused vertical gaps in the expanded card.
  - MUST NOT keep the AGY module asleep when new transcript entries arrive.
  - MUST NOT allocate unbounded heap buffers when parsing large transcripts.
  - MUST NOT hardcode static colors that clash with active theme settings.
- **SHOULD**:
  - Gracefully truncate long conversation titles with ellipsis.
  - Provide smooth height transitions during dynamic container resizing.

---

## 5. Acceptance Thresholds & Definition of Done
- [x] Active Execution Card displays live subagent role and current tool action.
- [x] Compact notch visual styling harmonizes with active theme (e.g., Apple 16 Dark).
- [x] Host RAM usage remains flat during large transcript processing.
- [x] Active Time verified against genuine model generation periods.
- [x] Dual header shows both project name and conversation title.
- [x] Expanded AGY card renders with zero dead space.
- [x] Inactive island wakes reliably on new agent events.
- [x] Clipboard widget completely excluded from scope.
- [x] Human lead explicitly confirmed and approved this intent document.
