---
title: "Feature Intent: {{FEATURE_NAME}}"
id: "INTENT-{{FEATURE_SLUG}}-001"
version: "1.0.0"
status: "DRAFT" # DRAFT | REVIEW | APPROVED
appetite: "SMALL_BATCH" # MICRO (hours) | SMALL_BATCH (1-2 days) | BIG_BATCH (1-2 weeks)
owner: "{{OWNER_ROLE}}" # user | orchestrator
created_at: "{{CURRENT_DATE}}"
---

# Feature Intent: {{FEATURE_NAME}}

## 1. Problem Statement & Baseline Story
- **Who is Affected**: {{TARGET_USERS_OR_SYSTEMS}}
- **Status-Quo Failure**: {{DESCRIBE_WHAT_FAILS_TODAY}}
- **Measurable Business Pain**: {{TIME_LOST_ERRORS_OR_FRICTION}}
- **Target Outcome**: {{WHAT_DOES_SUCCESS_LOOK_LIKE}}

---

## 2. Appetite & Resource Boundaries
- **Time Box / Appetite**: {{MICRO_OR_SMALL_OR_BIG_BATCH}}
- **Blast Radius**: {{AFFECTED_REPOSITORIES_OR_SUBSYSTEMS}}
- **Trade-Off Stance**: Scope is variable; appetite and timeline are fixed. If scope exceeds appetite, trim secondary requirements.

---

## 3. Rabbit Holes & Out-of-Scope (No-Gos)
Pre-emptively flag technical traps and non-goals to prevent scope creep:

- **Rabbit Hole 1**: {{KNOWN_TECHNICAL_TRAP_TO_AVOID}}
- **Rabbit Hole 2**: {{OVER_COMPLICATED_PATTERN_TO_AVOID}}
- **No-Go 1**: {{EXPLICITLY_UNSUPPORTED_FEATURE_OR_EDGE_CASE}}
- **No-Go 2**: {{DEFERRED_CAPABILITY_FOR_FUTURE_MILESTONES}}

---

## 4. RFC 2119 Normative Invariants
Normative behavioral rules governing the problem space:

- **MUST**: {{MANDATORY_REQUIREMENT_1}}
- **MUST**: {{MANDATORY_REQUIREMENT_2}}
- **MUST NOT**: {{FORBIDDEN_ACTION_OR_SECURITY_BOUNDARY_1}}
- **MUST NOT**: {{FORBIDDEN_ACTION_OR_DATA_LEAK_2}}
- **SHOULD**: {{RECOMMENDED_CONVENTION_OR_BEHAVIOR}}

---

## 5. Acceptance Thresholds & Definition of Done
Conditions required before this intent transitions to the Discovery and Strategy phase:

- [ ] Problem statement verified with concrete baseline friction evidence.
- [ ] Appetite agreed upon with human lead.
- [ ] No-Gos signed off by product owner.
- [ ] RFC 2119 invariants reviewed without implementation prescribing.
- [ ] Human lead explicitly confirmed and approved this document.
