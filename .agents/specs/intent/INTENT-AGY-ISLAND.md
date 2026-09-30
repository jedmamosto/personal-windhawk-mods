---
title: "Feature Intent: Antigravity Dynamic Island Telemetry"
id: "INTENT-AGY-ISLAND-001"
version: "1.0.0"
status: "APPROVED"
appetite: "SMALL_BATCH"
owner: "Jed Mamosto"
created_at: "2026-09-30"
---

# Feature Intent: Antigravity Dynamic Island Telemetry

## 1. Problem Statement & Baseline Story
- **Who is Affected**: Developers orchestrating complex agent workflows in Antigravity while working across the desktop.
- **Status-Quo Failure**: The Antigravity island does not render. The desktop overlay receives no telemetry from active sessions.
- **Measurable User Pain**: Developers cannot track context window utilization during complex coding sessions. They cannot anticipate context compaction. They miss optimal intervention points to initiate session handoffs or spawn subagents with isolated context windows.
- **Root Cause Baseline**: The desktop mod expects structured telemetry snapshots. The agent runtime does not publish these snapshots automatically.
- **Context Window Mental Model**: The context window represents a perishable operational budget. When token consumption reaches saturation, the system compacts history, discarding granular reasoning context.
- **Cognitive Ergonomics**: Developers require ambient, peripheral awareness of context consumption without constantly checking terminal prompts.
- **Target Outcome**: The Dynamic Island renders a sleek satellite indicator when an agent session is active. Expanding the island reveals live context consumption, active subagent rosters, running background tasks, and current workspace branches.

---

## 2. Appetite & Resource Boundaries
- **Time Box / Appetite**: Small Batch (1–2 days of development effort).
- **Blast Radius**: Desktop overlay Direct2D rendering pipeline, telemetry listener engine, and local agent lifecycle hooks.
- **Trade-Off Stance**: Fixed schedule with flexible scope. Prioritize dependable single-session telemetry before attempting multi-session carousel management.
- **Resource Constraints**: Direct2D rendering must remain locked at 60 FPS minimum without CPU stutter or memory leaks.
- **Energy Efficiency**: The telemetry listener must use zero CPU when no updates arrive.
- **Memory Footprint**: The telemetry engine memory usage must remain negligible.

---

## 3. Rabbit Holes & Out-of-Scope (No-Gos)
Pre-emptively flag technical traps and non-goals to prevent scope creep:

- **Rabbit Hole 1 (Polling Raw Disk Transcripts)**: Continually reading large transcript logs from disk consumes excessive CPU cycles and risks file lock contention.
- **Rabbit Hole 2 (Scraping Window Handles)**: Tracking IDE window titles is fragile across UI updates and cannot extract token counts.
- **Rabbit Hole 3 (Direct Process Memory Inspection)**: Scanning process memory triggers security alarms and leads to unstable pointers.
- **No-Go 1 (Direct Chat Injection)**: The island MUST NOT inject prompts, steer conversations, or alter chat histories from the overlay.
- **No-Go 2 (External Network Calls)**: All telemetry data MUST remain local to the user workstation. No cloud endpoints are permitted.
- **No-Go 3 (Monolithic Transcript Viewer)**: The island MUST NOT render full chat logs or conversation trees. It MUST remain a glanceable status dashboard.
- **No-Go 4 (Heavy External Dependencies)**: The telemetry bridge MUST NOT introduce heavy web runtimes or node daemons into the desktop injection path.

---

## 4. RFC 2119 Normative Invariants
Normative behavioral rules governing the problem space:

- **MUST**: The active conversation MUST be identified via an agent lifecycle hook that pushes real-time telemetry to the desktop overlay.
- **MUST**: The telemetry system MUST provide a passive local file fallback if the real-time push channel is disconnected.
- **MUST**: The island MUST surface current context token usage, total model context limits, and calculated utilization percentages.
- **MUST**: The island MUST visually signal elevated (60%–80%) and critical (80%+) context pressure levels before compaction occurs.
- **MUST**: The island MUST display active subagent names, assigned roles, and current execution states.
- **MUST**: The island MUST report the current count and status of active background execution tasks.
- **MUST**: The island MUST automatically hide or enter zero-CPU parking mode when no active agent session exists.
- **MUST**: The island MUST update immediately upon session termination or when an agent completes its turn.
- **MUST NOT**: Telemetry transport and parsing MUST NOT block the Direct2D render loop or agent execution steps.
- **MUST NOT**: Telemetry readers MUST NOT place exclusive file locks on active session resources.
- **SHOULD**: The island SHOULD execute smooth spring animations between collapsed indicator and expanded dashboard views.
- **SHOULD**: The overlay SHOULD reflect telemetry state changes within 100 milliseconds of an agent lifecycle event.
- **SHOULD**: The system SHOULD allow manual cycling between concurrent conversations if multiple sessions are active.

---

## 5. Acceptance Thresholds & Definition of Done
Conditions required before this intent transitions to technical specification:

- [x] Push-based lifecycle hook established as primary active session detector.
- [x] Passive local snapshot established as secondary fallback.
- [x] Small-batch appetite approved by human lead.
- [x] Negative boundaries and No-Gos locked.
- [x] RFC 2119 normative invariants defined without implementation details.
- [x] Feature intent document formally approved and committed.
