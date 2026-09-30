---
title: "Specification: {{FEATURE_NAME}}"
status: "DRAFT | APPROVED | IMPLEMENTED"
feature_slug: "{{FEATURE_SLUG}}"
lead: "orchestrator"
last_updated: "{{DATE}}"
---

# Feature Specification: {{FEATURE_NAME}}

> [!IMPORTANT]
> **Living Specification Standard (ADR-002)**: Keep this document under 120 lines total. Use code anchors to reference real files on disk rather than copy-pasting code blocks.

---

## 1. Intent & Problem Formulation
- **The Problem**: What friction or gap does the user experience?
- **Desired Outcome**: What observable business or user outcome defines success?
- **Out of Scope**: What related capabilities are explicitly deferred?

---

## 2. UX Flows & The 5-State Matrix
- **Primary User Flow**: Step 1 (Trigger) $\to$ Step 2 (Action) $\to$ Step 3 (Outcome).
- **The 5 Visual States**:
  - *Loading*: Skeleton screen or minimal spinner layout.
  - *Empty*: Helpful zero-data message with plain CEFR A2 action CTA.
  - *Partial*: Progressive rendering during streaming or pagination.
  - *Error*: Direct, actionable recovery copy (no technical stack traces).
  - *Success*: Immediate optimistic feedback and confirmed state.

---

## 3. Visual Layout & Design Tokens
- **ASCII Wireframe**:
```text
+-------------------------------------------------------------------+
|  [Header / Navigation Bar]                                        |
+-------------------------------------------------------------------+
|  [Sidebar]          |  [Main Content Card / Form Surface]         |
|                     |  - Headline / Purpose Statement             |
|                     |  - Primary Action CTA (OKLCH Accent)        |
+-------------------------------------------------------------------+
```
- **Design Tokens**: Bind all surfaces to semantic OKLCH tokens in `DESIGN.md`.
- **Active Route Wiring**: Mount target: `src/app/...` or active router tree.

---

## 4. Architecture & Data Contracts
- **Persistence Changes**:
  - Additive migrations in `supabase/migrations/` or local database schema.
  - Foreign keys, indexes, and row-level security (RLS) policies.
- **API Contracts / Server Actions**:
  - Endpoint / Function: `POST /api/{{endpoint}}`
  - Input DTO: `See type definition in [types/...](file:///...)`
  - Response Shape: Strictly-typed result with error boundary handling.

---

## 5. Verification Matrix (Acceptance Criteria)

| Scenario ID | Category | Given (Precondition) | When (Trigger) | Then (Observable Outcome) |
| :--- | :--- | :--- | :--- | :--- |
| `AC-001` | Happy Path | User is authenticated | Submits valid form | Record created, UI updates optimistically |
| `AC-002` | Validation | Input is invalid or empty | Submits form | Inline error displayed in plain CEFR A2 copy |
| `AC-003` | Edge Case | Network disconnects | Submits form | Error state shown with retry button |

### Automated Sign-Off Commands
```powershell
# 1. Scoped Unit / Component Tests
pnpm test <test-file>

# 2. Type Check & Linter Gate
pnpm type-check ; pnpm lint

# 3. Layer 1 Full Build
pnpm build
```
