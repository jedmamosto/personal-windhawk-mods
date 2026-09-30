---
feature: "{{FEATURE_NAME}}"
status: "IN_PROGRESS" # NOT_STARTED | IN_PROGRESS | BLOCKED | COMPLETED
plan_ref: "implementation_plan.md"
---

# Task Tracker: {{FEATURE_NAME}}

> **Operational Ledger**: Single source of truth for execution state. Survives context compaction.  
> Blueprint & boundary contracts are in [implementation_plan.md](file:///{{APP_DATA_DIR}}/brain/{{CONVERSATION_ID}}/implementation_plan.md).

---

## 1. Subagent Dispatch Matrix

Workers run with `Workspace: inherit` under non-overlapping directory locks ($N \le 3$).

| ID | Role / Stream | Mutex Scope | Dependencies | State | Verification Proof |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`W1-01`** | `orchestrator` | `.agents/**` | None | `[x] COMPLETED` | Dual artifacts initialized |
| **`W2-02`** | `backend-engineer` | `src/server/**` | `W1-01` | `[/] IN_PROGRESS`| `pnpm test tests/api` (exit 0) |
| **`W2-03`** | `frontend-engineer` | `src/client/**` | `W1-01` | `[ ] PENDING` | `pnpm test tests/ui` (exit 0) |
| **`W3-04`** | `code-reviewer` | Read-Only (`.`) | `W2-02`, `W2-03` | `[ ] PENDING` | Full CLI suite & diff audit |

*State Invariant: Mark active item as `[/]` (limit exactly one per stream). Mark `[x]` upon exit code 0.*

---

## 2. Execution Checklist

### Phase 1: Plan & Align
- [x] Align on intent and problem formulation with human lead.
- [x] Initialize `implementation_plan.md` and `task.md` in brain directory.

### Phase 2: Act (Surgical Implementation)
- [ ] Implement backend changes, data models, and server actions (`src/server/**`).
- [ ] Implement frontend components, state wiring, and design tokens (`src/client/**`).
- [ ] Verify Layer 1 scoped CLI checks pass with exit code 0.

### Phase 3: Verify & Audit
- [ ] Run full project build and test suite (`pnpm test ; pnpm type-check`).
- [ ] Audit git diff via `code-reviewer` for simplicity, security, and zero drift.
- [ ] Record verification proofs in `PROOF_BUNDLE_V1` and obtain human push authorization.

---

## 3. Decisions & Rulings ("Rulings, Not Stalls")

*Record autonomous rulings on ambiguities here to maintain forward momentum without chat pauses.*

- `RUL-01` (W1-01): Adopted modular feature slicing — Keeps blast radius isolated to `src/features/{{FEATURE_KEY}}`.
