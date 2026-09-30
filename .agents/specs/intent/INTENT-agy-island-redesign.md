---
title: "Feature Intent: AGY Section & Dynamic Island Telemetry Redesign"
id: "INTENT-agy-island-redesign-001"
version: "1.2.0"
status: "APPROVED"
appetite: "SMALL_BATCH"
owner: "jed"
created_at: "2026-09-30"
---

# Feature Intent: AGY Section & Dynamic Island Telemetry Redesign

## 1. Problem Statement & Baseline Story
- **Who is Affected**: Developers monitoring active AGY 2.0 and Antigravity sessions through the Dynamic Island.
- **Status-Quo Failure**: The collapsed notch circle clips the outer capsule border. The expanded view is cluttered with subagent rosters and task tables while omitting conversation title, last message sender, Gemini usage limits, and context compaction warnings.
- **Measurable Pain**: Visual overlap defects on the collapsed notch, excessive telemetry clutter, and sudden unexpected context compactions.
- **Target Outcome**: A polished, glanceable island view showing the conversation title, active turn, last message sender, active model usage limits, and early compaction warnings.

---

## 2. Appetite & Resource Boundaries
- **Time Box / Appetite**: SMALL_BATCH (1–2 days).
- **Blast Radius**: Dynamic Island overlay rendering and telemetry bridge.
- **Trade-Off Stance**: Visual clarity and zero quota overhead take priority over deep history.

---

## 3. Target Experience & Core Capabilities

### A. Compact Notch & Island Geometry
- Correct optical alignment and padding of the indicator orb within the compact capsule.
- Eliminate border clipping and overlap defects across all display scales.

### B. Header & Conversation Identity
- Display the clean, human-readable conversation title.
- Show active turn or step progress indicator.
- Display a brief snippet of the latest message with clear role attribution (User vs. Agent).

### C. Active Model Usage Limits (Gemini Focus)
- Surface usage limits for the active model tier (Gemini Models):
  - Weekly Limit Remaining (percentage and refresh countdown).
  - Five-Hour Limit Remaining (percentage and refresh countdown).

### D. Context Compaction Warning
- Provide a proactive threshold indicator alerting the user before conversation context triggers automatic compaction.

---

## 4. Rabbit Holes & Out-of-Scope (No-Gos)

- **Rabbit Hole 1**: Executing LLM completions or consuming quota to fetch telemetry data.
- **No-Go 1 (User Confirmed)**: Detailed turn history tables are strictly excluded.
- **No-Go 2 (User Confirmed)**: Subagent fleet rosters and multi-task status lists are strictly excluded.
- **No-Go 3**: In-island interactive quota billing or plan upgrade workflows.

---

## 5. RFC 2119 Normative Invariants

- **MUST**: Render the compact notch indicator with internal padding so it never overlaps the border.
- **MUST**: Display the current conversation title in place of raw session IDs.
- **MUST**: Clearly distinguish whether the latest message originated from the User or the Agent.
- **MUST**: Display 5-Hour and Weekly usage limits for the active Gemini model.
- **MUST**: Alert the user visually before context memory triggers automatic compaction.
- **MUST**: Source all telemetry and quota metrics from existing local session files or cached state with zero LLM API calls.
- **MUST NOT**: Block the Win32 message queue while polling telemetry or quota state.
- **MUST NOT**: Display subagent fleet rosters or task tracker tables.

---

## 6. Acceptance Thresholds & Definition of Done

- [ ] Notch circle overlap defect is eliminated in both 100% and high-DPI scaling.
- [ ] Active conversation title and step/turn status render legibly.
- [ ] Latest message preview displays correct role tag (User / Agent).
- [ ] Five-hour and weekly usage limit percentages display accurately.
- [ ] Context compaction warning triggers before conversation compression.
- [ ] Subagent fleet rosters and task tables are absent.
- [ ] Zero model quota is consumed by telemetry retrieval.
- [ ] Human lead explicitly confirmed and approved this document.
