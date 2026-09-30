# Code Review Instructions & Policy

This document defines the review criteria enforced by `code-reviewer` prior to committing diffs.

---

## 1. Review Passes
The reviewer MUST execute three distinct passes and tag every finding:

1. **Pass 1: Logic & Regressions (`[LOGIC]`)**:
   - Logic errors, broken edge cases, unhandled null/undefined values.
   - Anti-orphan verification: new UI components must be wired to active routes.
   - Strict typing: zero `any`, zero `@ts-ignore`, explicit return types.

2. **Pass 2: Security & Data Integrity (`[SECURITY]`)**:
   - Injection vulnerabilities, missing authentication/authorization checks.
   - Secrets or credentials in diffs (`.env`, private keys).
   - Sensitive user data or PII in console logs or error messages.

3. **Pass 3: Spec & Traceability Compliance (`[COMPLIANCE]`)**:
   - The diff strictly satisfies the **Cross-Cutting Traceability Matrix** in `master_spec.md`.
   - The diff conforms to `DESIGN.md` OKLCH tokens and `PRODUCT.md` user journeys.

---

## 2. Severity Classification
- **BLOCKING (Must Fix)**: Breaks behavior, leaks data, introduces `any` types, fails build/tests, or violates `master_spec.md`.
- **NIT (Non-Blocking)**: Minor naming conventions or stylistic suggestions.

---

## 3. The 5-Nit Rule (Preventing Review Fatigue)
- Report at most **5 nits per review**.
- Summarize any additional minor suggestions as a single aggregate count without line-by-line pedantry.

---

## 4. Ignored Paths & Pre-Enforced Assets
Do not flag issues in:
- Generated files (e.g. `src/gen/`, `dist/`, `.next/`, `build/`).
- Package lockfiles (`pnpm-lock.yaml`, `package-lock.json`).
- Issues already caught and failed by deterministic compiler/linter errors.
