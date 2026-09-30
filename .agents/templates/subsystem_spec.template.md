---
title: "{{SUBSYSTEM_TITLE}}"
id: "SPEC-{{SUBSYSTEM_KEY}}-001"
status: "APPROVED" # DRAFT | REVIEW | APPROVED | DEPRECATED
version: "1.0.0"
owner: "{{OWNER_ROLE}}" # software-architect | product-ux-strategist | orchestrator
write_mutex_locks:
  - "{{LOCK_PATH_1}}"
  - "{{LOCK_PATH_2}}"
ux_fsm_ref: "PRODUCT.md#{{UX_FSM_ANCHOR}}"
design_token_ref: "DESIGN.md#{{DESIGN_TOKEN_ANCHOR}}"
acm_matrix:
  - id: "ACM-01"
    given: "{{GIVEN_NOMINAL_PRECONDITION}}"
    when: "{{WHEN_NOMINAL_ACTION}}"
    then: "{{THEN_NOMINAL_ASSERTION}}"
  - id: "ACM-02"
    given: "{{GIVEN_ERROR_PRECONDITION}}"
    when: "{{WHEN_ERROR_ACTION}}"
    then: "{{THEN_RECOVERY_ASSERTION}}"
---

# Specification: {{SUBSYSTEM_TITLE}}

## 1. Executive Mental Model
{{PLAIN_ENGLISH_MENTAL_MODEL}}

## 2. Invariant Operating Rules (ASD-STE100)
- DO NOT {{FORBIDDEN_ACTION_1}}.
- ALWAYS {{REQUIRED_ACTION_1}}.
- DO NOT {{FORBIDDEN_ACTION_2}}.
- ALWAYS {{REQUIRED_ACTION_2}}.

## 3. 5-State UX Finite State Machine (FSM)
| State Name | Trigger / Precondition | Visual Presentation | Recovery / Action |
| :--- | :--- | :--- | :--- |
| **Empty** | Zero items in dataset | Empty state hero with action CTA | Tap create button |
| **Loading** | Network request in-flight | Skeleton placeholder pulse | Auto-cancel after timeout |
| **Success** | Data fetched / mutation ok | Render interactive component tree | User interacts with UI |
| **Error** | 4xx / 5xx / Network offline | Inline actionable error banner | Tap retry button |
| **Edge** | Stale cache / Token expiring | Subtle warning badge | Background silent refresh |

## 4. Architectural Contracts & Schemas
```typescript
// Strict Interface / Schema Contract
export interface {{CONTRACT_NAME}} {
  id: string;
  status: "{{STATUS_ENUM_1}}" | "{{STATUS_ENUM_2}}";
  createdAt: string;
}
```

## 5. Behavioral Acceptance Criteria (Given-When-Then)
- **ACM-01 (Nominal Path)**:
  - *Given* {{GIVEN_NOMINAL_PRECONDITION}},
  - *When* {{WHEN_NOMINAL_ACTION}},
  - *Then* {{THEN_NOMINAL_ASSERTION}}.
- **ACM-02 (Error / Edge Boundary)**:
  - *Given* {{GIVEN_ERROR_PRECONDITION}},
  - *When* {{WHEN_ERROR_ACTION}},
  - *Then* {{THEN_RECOVERY_ASSERTION}}.

## 6. Verification Harness
```powershell
{{VERIFICATION_COMMAND}}
```
