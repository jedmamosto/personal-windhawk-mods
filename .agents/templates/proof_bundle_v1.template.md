# Canonical PROOF_BUNDLE_V1 Template & Specification

This document defines the canonical `PROOF_BUNDLE_V1` structure for all Antigravity subagents and orchestrators.
Subagents emit or record this block upon task completion to provide machine-verifiable proof of work before advancing task state to `COMPLETED`.

---

## 1. Schema Definition

```yaml
PROOF_BUNDLE_V1:
  # --- Operational Identity & Attribution ---
  task_id: "<TASK_ID>"
  feature_key: "<FEATURE_KEY>"
  verified_by: "<SPECIALIST_ROLE>"
  timestamp: "YYYY-MM-DDTHH:mm:ssZ"

  # --- Deterministic Machine Verification (LLM-Modulo) ---
  verification:
    cli_command: "<SCOPED_CLI_COMMAND>"
    exit_code: 0 # MANDATORY: Must be integer 0. Non-zero values invalidate completion.
    execution_duration_ms: <INTEGER_MILLISECONDS>
    stdout_summary: "<COMPACT_SNIPPET_OF_PASSING_TEST_OR_LINT_OUTPUT>"

  # --- Physical Artifact Evidence on Disk ---
  artifacts_created:
    - "<EXACT_FILE_PATH_CREATED>"
  artifacts_modified:
    - path: "<EXACT_FILE_PATH_MODIFIED>"
      symbol_or_lines: "<MODIFIED_FUNCTION_OR_LINE_RANGE>"

  # --- Acceptance Criteria Compliance ---
  acm_validation:
    given: "<PRECONDITION_OR_STATE>"
    when: "<TRIGGER_ACTION_OR_MUTATION>"
    then: "<OBSERVED_OUTCOME_PROVEN_BY_TESTS>"
```

---

## 2. Field Specifications

| Field | Type | Requirement | ASD-STE100 Description |
| :--- | :--- | :--- | :--- |
| `task_id` | `string` | **Mandatory** | Backlog or dispatch task identifier (matches `MANIFEST_V1.task_id`). |
| `feature_key` | `string` | **Mandatory** | Canonical feature or refactor slug. |
| `verified_by` | `string` | **Mandatory** | Specialist role of the executing contractor or critic. |
| `timestamp` | `string` | **Mandatory** | ISO-8601 UTC timestamp of verification execution. |
| `verification.cli_command` | `string` | **Mandatory** | Exact terminal command executed to verify changes. |
| `verification.exit_code` | `integer` | **Mandatory** | CLI exit status. MUST equal 0. |
| `verification.execution_duration_ms` | `integer` | **Optional** | Execution run duration in milliseconds. |
| `verification.stdout_summary` | `string` | **Mandatory** | High-density stdout summary (e.g. `14 passed (14), 0 failed`). |
| `artifacts_created` | `string[]` | **Mandatory** | Absolute or repo-relative paths of new files created on disk. |
| `artifacts_modified` | `object[]` | **Mandatory** | File paths and specific symbol/line ranges modified. |
| `acm_validation.given` | `string` | **Mandatory** | Initial system context or precondition. |
| `acm_validation.when` | `string` | **Mandatory** | Action, payload, or mutation executed. |
| `acm_validation.then` | `string` | **Mandatory** | Verified deterministic outcome proven by test logs. |

---

## 3. Verification Operating Rules

1. **LLM-Modulo Primacy**: Verbal statements claiming code "works" or "looks good" are legally inadmissible. Verification requires external symbolic execution exiting with `code 0`.
2. **Physical Disk Verification**: The coordinating agent MUST confirm physical file presence and inspect diffs on disk before marking tasks completed in `task.md`.
3. **No Unbounded Log Dumps**: DO NOT dump thousands of lines of raw compiler output into chat or the proof bundle. Retain only the concise summary line and status code.
4. **Pre-Wave Gate**: Subsequent waves in the DAG schedule cannot unlock until preceding tasks record a valid `PROOF_BUNDLE_V1`.

---

## 4. Golden Samples

### Sample A: Backend Service & Database Migration
```yaml
PROOF_BUNDLE_V1:
  task_id: "BE-104"
  feature_key: "auth-session-revocation"
  verified_by: "backend-engineer"
  timestamp: "2026-09-20T22:30:00Z"
  verification:
    cli_command: "pnpm vitest run packages/api/src/routers/auth"
    exit_code: 0
    execution_duration_ms: 1840
    stdout_summary: "Test Files 1 passed (1) | Tests 8 passed (8) | Duration 1.84s"
  artifacts_created:
    - "packages/db/prisma/migrations/20260920_revoke_session/migration.sql"
  artifacts_modified:
    - path: "packages/api/src/routers/auth/session.ts"
      symbol_or_lines: "revokeSessionMutation (L45-L78)"
    - path: "packages/db/prisma/schema.prisma"
      symbol_or_lines: "Session.revokedAt (L32)"
  acm_validation:
    given: "An active user session exists in the database."
    when: "revokeSession is called with a valid sessionId."
    then: "revokedAt is populated and subsequent auth assertions return 401 Unauthorized."
```

### Sample B: Frontend Responsive Component
```yaml
PROOF_BUNDLE_V1:
  task_id: "FE-201"
  feature_key: "landing-hero-redesign"
  verified_by: "frontend-engineer"
  timestamp: "2026-09-20T22:35:00Z"
  verification:
    cli_command: "pnpm test src/components/landing/hero && pnpm type-check"
    exit_code: 0
    execution_duration_ms: 3200
    stdout_summary: "Component test passed across 375px, 768px, and 1280px viewports. 0 type errors."
  artifacts_created:
    - "src/components/landing/hero/HeroSection.tsx"
    - "src/components/landing/hero/HeroSection.module.css"
  artifacts_modified:
    - path: "src/pages/index.tsx"
      symbol_or_lines: "LandingPage (L12-L18)"
  acm_validation:
    given: "A user accesses the landing page across mobile, tablet, or desktop viewports."
    when: "The hero section mounts."
    then: "Typography clamps correctly and CTA button triggers signup route without layout shift."
```
