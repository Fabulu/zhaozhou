# Naming: Form and Nanquan

**Decision (wave 1, plan 1.F — hybrid, docs-only):** the programming language
for the Zhaozhou console is **Form**. The *nanquan* repository will later
become the standalone **toolchain distribution** for Form. No wave-1 commits
touch `nanquan/` or `untitled-game/`.

## Why

- The engineering charter (§3.2) uses "Form" pervasively; charter vocabulary
  wins inside `zhaozhou/`. Introducing a second name for the language would
  fork the spec.
- The nanquan README ("the programming language for the Zhaozhou console")
  and untitled-game ("written in Nanquan") predate this decision; the intent
  recorded here is that nanquan repurposes as the *distribution* channel
  (installer, pinned toolchain versions, release packaging) rather than the
  language's home.
- The Form language itself lives in this repository under `compiler/`
  (charter §22) and its specification in `spec/form/`.

## Vocabulary inside zhaozhou

| Term | Meaning |
|---|---|
| Form | the programming language (spec: `spec/form/*.md`) |
| Field IR | Form's fixed-point field intermediate representation (spec: `spec/form/field-ir.md`) |
| ZRef | the C++17 golden-reference library (`reference/`) |
| ZEmu | the desktop console shell (`emulator/`) |
| nanquan | sibling repo; future standalone Form toolchain distribution |

When the nanquan repurposing actually happens (post wave 1), that repo's
README should say "the standalone distribution of the Form toolchain" and
link here; this file is the recorded source of that decision.
