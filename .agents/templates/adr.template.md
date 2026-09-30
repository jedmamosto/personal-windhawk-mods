---
id: "ADR-{{ADR_NUMBER}}"
title: "{{DECISION_TITLE}}"
status: "ACCEPTED" # PROPOSED | ACCEPTED | SUPERSEDED | DEPRECATED
date: "{{DATE_YYYY_MM_DD}}"
deciders:
  - "software-architect"
  - "orchestrator"
technical_story: "{{FEATURE_KEY_OR_ISSUE_REF}}"
budget_lines: 120
---

# ADR-{{ADR_NUMBER}}: {{DECISION_TITLE}}

## 1. Context & Problem Statement
{{PROBLEM_DESCRIPTION_AND_SYSTEM_CONTEXT}}

---

## 2. Decision Drivers
- *Driver 1*: {{PRIMARY_PERFORMANCE_OR_SCALE_REQUIREMENT}}
- *Driver 2*: {{MAINTAINABILITY_OR_TYPE_SAFETY_GOAL}}
- *Driver 3*: {{SECURITY_OR_DATA_ISOLATION_CONSTRAINT}}

---

## 3. Considered Options & Trade-Off Matrix
| Evaluation Dimension | Option 1: {{OPTION_1_NAME}} (Selected) | Option 2: {{OPTION_2_NAME}} | Option 3: {{OPTION_3_NAME}} |
| :--- | :--- | :--- | :--- |
| **Complexity / Maintenance** | Low (uses existing stack dependencies) | High (introduces new runtime service) | Moderate |
| **Type Safety & Compiler Parity** | High (end-to-end TypeScript inferencing) | Moderate | Low |
| **Blast Radius & Migrations** | Scoped strictly to `src/modules/{{MODULE}}` | Global schema refactor | Unpredictable |

---

## 4. Decision Outcome & Architectural Rationale
- **Chosen Option**: **Option 1: {{OPTION_1_NAME}}**.
- **Justification**: It satisfies Driver 1 and Driver 2 without introducing external dependency weight or database schema bifurcation.

---

## 5. Blast Radius & Impacted Subsystems
- **Impacted Subsystems**: `{{PATH_1}}`, `{{PATH_2}}`.
- **God Nodes Touched**: `{{GOD_NODE_NAME}}`. Call sites verified via Graphify.
- **Supersession Notice**: {{SUPERSEDES_PRIOR_ADR_OR_NONE}}.

---

## 6. Verification Invariants
- DO NOT bypass the chosen architectural boundary.
- ALWAYS enforce compile-time type verification across all callers.
