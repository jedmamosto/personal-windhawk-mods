---
id: "TASK-XXX-01"
title: "[Brief Imperative Task Title]"
persona: "backend-engineer" # backend-engineer | frontend-engineer | software-architect | etc.
priority: "P0" # P0 (Blocker) | P1 (High) | P2 (Normal) | P3 (Low)
status: "ready" # backlog | ready | in_progress | review_gate | verified | archived
lock_paths:
  - "[Exact/Directory/Path/*]"
upstream_deps: []
downstream_deps: []
context_anchors:
  - "file:///{{WORKSPACE_ROOT}}/[file_path]#L1-L50"
verification_command: "pnpm test"
---

# TASK-XXX-01: [Task Title]

## 1. Plain-English Mental Model
[1-2 sentences describing what this task does in simple terms]

## 2. Invariant Rules (ASD-STE100 Pairing)
- DO NOT [forbidden action or failure mode].
- ALWAYS [deterministic required action].
- DO NOT [secondary forbidden action].
- ALWAYS [secondary required action].

## 3. Behavioral Acceptance Criteria (Given-When-Then)
- **AC-1 ([Scenario Name])**:
  - *Given* [initial state or preconditions],
  - *When* [action or trigger occurs],
  - *Then* [expected state change or output].
- **AC-2 ([Negative / Error Boundary])**:
  - *Given* [invalid input or unauthorized attempt],
  - *When* [action occurs],
  - *Then* [explicit rejection or error code].

## 4. Verification Harness
```powershell
[verification_command]
```
