---
title: "Feature Intent: Dynamic Island Tri-Mode Notch and Telemetry Pipeline Fix"
id: "INTENT-DYNAMIC-ISLAND-TELEMETRY-001"
version: "1.0.0"
status: "APPROVED"
appetite: "SMALL_BATCH"
owner: "Jed"
created_at: "2026-10-02"
---

# Feature Intent: Dynamic Island Tri-Mode Notch and Telemetry Pipeline Fix

## 1. Problem Statement & Baseline Story
- **Who is Affected**: Windows 11 desktop users running the Dynamic Island mod with AI agent telemetry overlays.
- **Status-Quo Failure**: The minimized notch displays an overlapping halo aura that bleeds beyond pill boundaries. Telemetry streaming has stopped updating live values. The header mistakenly shows task IDs instead of the project folder and conversation title. Context window token tracking shows zero tokens. Active execution state displays stale metrics.
- **Measurable Business Pain**: Visual clipping degrades desktop aesthetics. Developers cannot monitor active agent operations, token usage, or conversation identity.
- **Target Outcome**: The notch supports three distinct display modes: Minimized, Normal, and Expanded. Minimized mode uses a solid fill without aura overlap. Normal mode retains the glowing status orb. Expanded mode displays streaming project identity, conversation title, active token consumption, and live execution status.

---

## 2. Display Modes & Visual Hierarchy
- **Minimized Notch Mode**:
  - Compact pill geometry for minimal desktop footprint.
  - Renders a clean solid fill without outer halo rings or overlapping strokes.
  - Eliminates graphical clipping against the main island notch.
- **Normal Notch Mode**:
  - Standard pill geometry providing glanceable agent state.
  - Retains the status orb core, specular glint, and ambient aura halo.
  - Shows current turn, step, or active tool action label.
- **Expanded Dashboard Mode**:
  - Full card overlay providing comprehensive session insights.
  - Primary header displays project folder name and human-readable conversation title.
  - Secondary sections show context token gauge, active session time, and live execution card.
- **Inactivity Lifecycle**:
  - The satellite notch remains visible during active work and stays visible for 2 minutes after completion.
  - After 2 minutes of inactivity, the notch shrinks to zero width and auto-parks.
  - Any new agent action or transcript write immediately restores the notch.

---

## 3. Telemetry Streaming & Information Architecture
- **Project & Conversation Identity**:
  - The upper header line displays the active project directory name.
  - The lower header line displays the human-readable conversation title.
  - Internal task IDs and UUID strings must not appear in the primary header.
- **Context Window Utilization**:
  - Displays actual context tokens consumed against maximum window capacity.
  - Displays token percentage and compaction warning thresholds accurately.
- **Live Stream Continuity**:
  - Telemetry pipeline continuously ingests live events from the agent session.
  - Eliminates static freezes and stale execution cards during active runs.

---

## 4. Appetite & Resource Boundaries
- **Time Box / Appetite**: Small Batch (1 to 2 days).
- **Blast Radius**: Dynamic Island telemetry streaming, satellite notch geometry, and UI rendering.
- **Trade-Off Stance**: Visual polish and data streaming accuracy take precedence over adding new metrics.

---

## 5. Rabbit Holes & Out-of-Scope (No-Gos)
- **Rabbit Hole 1**: Over-engineering IPC protocols or external daemon services to stream file updates.
- **Rabbit Hole 2**: Buffering entire multi-megabyte log files into process memory.
- **Rabbit Hole 3**: Blending distinct notch modes into complex fractional intermediate layouts.
- **No-Go 1**: DO NOT display raw UUID strings or task IDs in the primary header title.
- **No-Go 2**: DO NOT draw halo rings or clipping strokes when in minimized notch mode.
- **No-Go 3**: DO NOT add unrelated system widgets or clipboard cards to the island.
- **No-Go 4**: DO NOT block UI rendering loops on disk read operations.

---

## 6. RFC 2119 Normative Invariants
- **MUST**: The notch MUST render three mutually exclusive visual modes: Minimized, Normal, and Expanded.
- **MUST**: Minimized mode MUST render a clean solid fill without overlapping outer rings or glow clipping.
- **MUST**: Normal mode MUST retain the status orb core, glint, and glow aura.
- **MUST**: Status colors MUST indicate model state (active, compaction warning, critical pressure, or idle).
- **MUST**: The header MUST display the active project folder name and human-readable conversation title.
- **MUST**: Telemetry streaming MUST update live data continuously without stalling on static values.
- **MUST**: Context window tracking MUST display accurate token usage and total context capacity.
- **MUST**: Active execution status MUST reflect live agent and subagent execution states.
- **MUST**: The satellite notch MUST automatically dismiss after 2 minutes of continuous inactivity.
- **MUST**: The satellite notch MUST wake up immediately upon receiving new agent events.
- **MUST NOT**: Minimized notch graphics MUST NOT bleed outside the pill boundaries.
- **MUST NOT**: Telemetry parsers MUST NOT display internal task IDs in place of conversation titles.
- **MUST NOT**: Telemetry readers MUST NOT lock transcript files or stall UI rendering threads.

---

## 7. Acceptance Thresholds & Definition of Done
- [ ] Minimized notch renders a clean solid fill with zero halo overlap.
- [ ] Normal notch renders the status orb with aura and glint.
- [ ] Expanded dashboard shows project folder and human-readable conversation title.
- [ ] Token tracking shows live context tokens and utilization percentage.
- [ ] Telemetry stream updates dynamically as tasks progress.
- [ ] Active execution card accurately displays current executing role and action.
- [ ] Satellite notch automatically dismisses after 2 minutes of continuous inactivity.
- [ ] Satellite notch wakes up immediately upon receiving new agent events.
- [ ] Human lead explicitly reviews and approves this intent specification.
