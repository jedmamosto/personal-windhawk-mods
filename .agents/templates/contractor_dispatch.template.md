# Standard Contractor Dispatch Contract Template

This document defines the canonical prompt contract for subagents operating under `subagent-coordinator`.
The Coordinating Lead passes this structured contract when invoking subagents via `invoke_subagent` (`Workspace: inherit`).

---

## 1. Dispatch Contract Prompt Structure

```markdown
[TASK_ID]: <Backlog or dispatch task identifier, e.g. AUTH-01>
[ROLE]: <Functional domain specialist title, e.g. Senior Systems Engineer>
[INTENT]: <What and Why in ASD-STE100 English (<20 words)>
[MUTEX_LOCK]: <Assigned directory path owned exclusively, e.g. packages/api/src/routers/auth>
[TARGET_WRITE_SET]: <Explicit files contractor will touch, e.g. session.ts, schema.prisma>
[REFERENCE_READ_SET]: <Reference specs and token definitions, e.g. .agents/specs/auth/spec.md>
[TOOL_PREREQUISITES]: <Required tools, e.g. enable_mcp_tools: true for Playwright/Browser>
[NEGATIVE_CONSTRAINTS]: <Explicit non-goals, e.g. DO NOT modify OAuth flow; ALWAYS preserve existing sessions>
[MANIFEST_INSTRUCTION]: On Turn 1, send MANIFEST_V1 via send_message to orchestrator BEFORE calling implementation tools. Proceed immediately and optimistically unless an intervention message arrives.
[SKILL_INSTRUCTION]: Baseline recommendation: <suggested_skills (e.g. supabase)>. Treat as baseline guidance. Steering: "Which available skill provides specialized workflows for this task?". Survey available skills to discover more relevant workflows. Declare all adopted skills and explicit rationale in discovered_skills (e.g. - "supabase" # Why: Database schema migration; - "tdd-planner" # Why: Red-green test cycle).
[VERIFICATION]: <Discovered test command that must pass with exit code 0, e.g. pnpm test tests/auth>
[ASSUMPTIONS_POLICY]: Declare any critical technical assumptions in intent_alignment.assumptions_requiring_alignment. Proceed optimistically without conversational pauses.
[DEFINITION_OF_DONE]: <Observable Given-When-Then criteria & emit COMPLETION_MEMO_V1 upon exit code 0>
```

---

## 2. Field-to-Manifest Parity Mapping

| Dispatch Contract Field | MANIFEST_V1 Target Field | Turn 1 Contractor Duty |
| :--- | :--- | :--- |
| `[TASK_ID]` | `task_id` | Echo exact task ID. |
| `[MUTEX_LOCK]` | `subsystem_lock_path` | Echo assigned exclusive lock path. |
| `[SKILL_INSTRUCTION]` | `discovered_skills` | Evaluate baseline skills, survey available skills, and declare chosen skills with `# Why: <rationale>`. |
| `[TARGET_WRITE_SET]` | `write_set` | List all target files to touch inside mutex. |
| `[REFERENCE_READ_SET]` | `read_set` | List reference files inspected. |
| `[VERIFICATION]` | `verification_commands` | List deterministic CLI commands exiting 0. |
| `[INTENT]` | `intent_alignment.interpretation` | State plain-English interpretation of user intent. |
| `[INTENT]` | `intent_alignment.planned_approach` | Outline tactical execution steps. |
| `[NEGATIVE_CONSTRAINTS]` | `intent_alignment.explicit_non_goals` | State out-of-scope actions and protected files. |
| `[ASSUMPTIONS_POLICY]` | `intent_alignment.assumptions_requiring_alignment` | Surface assumptions without stalling. |

---

## 3. Contractor Operating Rules (Inherited)

1. **Turn 1 Handshake**: Contractor emits [`MANIFEST_V1`](file:///C:/Users/ASUS/.gemini/config/templates/manifest_v1.template.md) via `send_message` to Coordinating Lead before executing mutation tools.
2. **Optimistic Concurrency**: Contractor starts tool calls immediately after sending `MANIFEST_V1`. Do NOT pause or wait for chat approval.
3. **Boundary Invariant**: Contractor MUST NOT touch files outside assigned `subsystem_lock_path` / `write_set`.
4. **Autonomous Skill Discovery**: Coordinator-suggested skills are a starting baseline, not a ceiling. Contractors MUST evaluate available skills, discover any additional or more specialized workflows that improve execution quality, and declare all selected skills with explicit rationale.
5. **Exception Sparring**: If an unrecoverable blocker or contract divergence occurs, halt edits and send `EXCEPTION_SPAR` with 3-part format (Facts, Systems Rationale, Ranked Options).
6. **Completion Proof**: Emit `COMPLETION_MEMO_V1` only after discovered verification commands pass with exit code 0.
