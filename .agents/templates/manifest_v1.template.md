# Canonical MANIFEST_V1 Template & Specification

This document defines the canonical `MANIFEST_V1` structure for all Antigravity subagents. Subagents emit this block via `send_message` in their initial turn to the Coordinating Agent or Orchestrator.

---

## 1. Schema Definition

```yaml
MANIFEST_V1:
  # --- Operational Mutex & Machine Bounds ---
  task_id: "<TASK_ID>"
  subsystem_lock_path: "<PRIMARY_DIRECTORY_PATH>"
  discovered_skills:
    - "<SKILL_NAME>" # Steering: "Which available skill provides specialized workflows for this task?"
                     # Augmentation: Evaluate coordinator baselines; survey available skills to discover more relevant workflows.
                     # Rationale: Declare why the subagent chose, adopted, or discovered this skill.
  write_set:
    - "<EXACT_FILE_PATH_TO_CREATE_OR_MODIFY>"
  read_set:
    - "<EXACT_FILE_PATH_TO_INSPECT>"
  verification_commands:
    - "<DETERMINISTIC_CLI_COMMAND_EXITING_ZERO>"

  # --- Proactive Intent Alignment ---
  intent_alignment:
    interpretation: "<HOW_CONTRACTOR_INTERPRETS_USER_INTENT_AND_OUTCOME>"
    planned_approach:
      - "<CONCRETE_STEP_1>"
      - "<CONCRETE_STEP_2>"
    explicit_non_goals:
      - "<OUT_OF_SCOPE_ACTION_OR_FILE_AVOIDED>"
    assumptions_requiring_alignment:
      - "<CRITICAL_TECHNICAL_OR_PRODUCT_ASSUMPTION>"
```

---

## 2. Field Specifications

| Field | Type | Requirement | ASD-STE100 Description |
| :--- | :--- | :--- | :--- |
| `task_id` | `string` | **Mandatory** | Assigned backlog or dispatch task identifier. |
| `subsystem_lock_path` | `string` | **Mandatory** | Directory path owned exclusively by this contractor. |
| `discovered_skills` | `string[]` | **Mandatory** | Skills evaluated and chosen. Steering: "Which available skill provides specialized workflows for this task?". Subagents adopt coordinator baseline recommendations AND actively discover additional or more relevant workflows. Must declare explicit rationale for each skill (or technical justification if `['none']`). |
| `write_set` | `string[]` | **Mandatory** | Exact list of target files the contractor will touch. |
| `read_set` | `string[]` | **Mandatory** | Reference files inspected during execution. |
| `verification_commands` | `string[]` | **Mandatory** | Scoped CLI commands to verify changes (compiler, linter, tests). |
| `intent_alignment.interpretation` | `string` | **Mandatory** | Brief summary of user intent and expected business outcome. |
| `intent_alignment.planned_approach` | `string[]` | **Mandatory** | Numbered list of tactical steps to complete the task. |
| `intent_alignment.explicit_non_goals` | `string[]` | **Mandatory** | Actions and files intentionally avoided to protect human intent. |
| `intent_alignment.assumptions_requiring_alignment` | `string[]` | **Optional** | Assumptions that require confirmation if ambiguous. |

---

## 3. Actor Duties & Operating Rules

### Contractor (Subagent) Duties
1. **Discover & Present Skills**: Subagents (operating as expert contractors) MUST ask internally: "Which available skill provides specialized workflows for this task?", evaluate coordinator baseline recommendations, and survey available skills to discover more relevant workflows. Subagents MUST declare all chosen skills in `discovered_skills` with explicit rationale and call `view_file` on their `SKILL.md`.
2. **Declare Skill Choice Rationale**: Subagents MUST declare why they chose each skill (both baseline and newly discovered). Explicitly connect the skill's capabilities to task requirements. If no skill applies, declare `['none']` with clear technical justification.
3. **Send First via Message**: Subagents MUST call `send_message` with `MANIFEST_V1` to the parent orchestrator in their very first turn BEFORE executing any implementation or mutation tools.
4. **Execute Optimistically**: Subagents start tool execution immediately after sending `MANIFEST_V1`. Do NOT pause or wait for conversational approval.
5. **No Idle Pauses**: Do NOT pause or wait for permission unless an exception triggers.
6. **Never Bundle with Completion**: Never defer `MANIFEST_V1` to the end of execution or bundle it with `COMPLETION_MEMO_V1`.

### Coordinating Agent Duties
1. **Protect Intent & Encourage Skill Discovery**: The Coordinating Agent actively guards task intent and mandates skill usage. Welcome and validate contractor-discovered skills that elevate execution quality. Reject manifests that omit skill declarations or omit the rationale for choosing skills.
2. **Dual Audit**: Validate directory lock boundaries (<5ms) and inspect `intent_alignment` and `discovered_skills` rationale against required outcomes.
3. **Proactive Intervention**: If the contractor's plan or skill selection diverges from intent, send an immediate correction directive via `send_message`.
4. **Halt Violations**: If the contractor breaches directory locks, send an immediate `ABORT_ROLLBACK` directive.

---

## 4. Golden Samples

### Sample A: Frontend Component Implementation

```yaml
MANIFEST_V1:
  task_id: "FE-201"
  subsystem_lock_path: "src/components/landing/hero"
  discovered_skills:
    - "ui-craft" # Why: Token binding and strict adherence to DESIGN.md styling
    - "playwright" # Why: Viewport triad verification (375px, 768px, 1280px)
  write_set:
    - "src/components/landing/hero/HeroSection.tsx"
    - "src/components/landing/hero/HeroSection.module.css"
  read_set:
    - "src/design/tokens.ts"
    - "DESIGN.md"
  verification_commands:
    - "pnpm test src/components/landing/hero"
    - "pnpm type-check"

  intent_alignment:
    interpretation: "Implement responsive hero section matching DESIGN.md tokens without altering layout hierarchy."
    planned_approach:
      - "Import fluid typography clamp tokens from tokens.ts"
      - "Build accessible HeroSection component with semantic HTML"
      - "Verify viewport fidelity at 375px, 768px, and 1280px"
    explicit_non_goals:
      - "Do not touch global navigation bar or footer layout."
      - "Do not introduce third-party UI libraries."
    assumptions_requiring_alignment:
      - "Hero CTA button links to /signup per PRODUCT.md."
```

### Sample B: Backend Endpoint & Migration

```yaml
MANIFEST_V1:
  task_id: "BE-104"
  subsystem_lock_path: "packages/api/src/routers/auth"
  discovered_skills:
    - "supabase" # Why: Database schema migration and RLS session validation
    - "tdd-planner" # Why: Red-Green-Refactor test cycle for session revocation mutation
  write_set:
    - "packages/api/src/routers/auth/session.ts"
    - "packages/db/prisma/schema.prisma"
  read_set:
    - ".agents/docs/specs/auth/master_spec.md"
  verification_commands:
    - "pnpm test packages/api/src/routers/auth"
    - "pnpm prisma validate"

  intent_alignment:
    interpretation: "Add session revocation endpoint to satisfy security audit intent."
    planned_approach:
      - "Add revokedAt timestamp column to Session schema"
      - "Implement revokeSession TRPC mutation with Zod validation"
      - "Add unit tests verifying token invalidation"
    explicit_non_goals:
      - "Do not modify existing OAuth sign-in flow."
      - "Do not reset database migrations."
    assumptions_requiring_alignment:
      - "Revocation applies to current device session only unless allDevices flag is true."
```
