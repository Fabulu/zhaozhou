# The ledger's 96 schema errors: 54 repaired, 42 left red ON PURPOSE

Coordinator, 2026-09-29, worked entirely outside the console fit's closure while it ran.
`npm run -w tools/ledger check` went **96 → 42** errors over 136 blocks.

**Nothing was silenced.** Every value written came from the tree, a contract, or git, and
every script refused before writing when its evidence did not hold. What remains red is
the part that **cannot** be fixed with a value, and this document exists so the next
reader does not close it with a plausible string.

---

## Why this mattered beyond tidiness

`ledger:check` has been red long enough that its output stopped being read, and
`eb5ebfde` recorded the consequence exactly: *"a gate red for an unrelated reason gives
cover to every other red inside it."* That is not hypothetical here. Correcting three
blocks' invalid `maturity` value immediately **exposed two defects that value had been
masking** — `GEOM.VERTID` owing a random-test arm, and two blocks pointing `directed` and
`random` at the same file. Neither was visible while the enum error fired first.

## What was repaired, and how each value was established

**Eight maturity commits that were packet or branch names** — `terrabake`, `PARAMARENA`
(×2), `ARENAID`, `gz/bandbuild`, `gz/twodcmd` (×2), `gz/postgather`. `eb5ebfde` refused
these after `git log --grep` returned confidently wrong answers, and that refusal was
right about the hazard: the same two wrong hits come back. What it got wrong was
concluding the sha is unfindable. **`git merge <branch>` writes the subject
"Merge <branch>…" itself**, so a merge whose SUBJECT names the packet is git's own record
of that integration rather than a human mentioning a word in a paragraph. Each name
resolves to exactly one such merge, with the entry's own `evidence` file present in that
merge's tree.

**`pending` on SW.CPUCOLL resolved through its evidence instead** — the file
`reference/include/zref/zref_nav.hpp` was added by `34dfa74d` on 2026-09-26, the exact
date the entry claims, with "113 green checks" in its subject.

**Five `maturity_log` entries with no commit** — pinned to the commit that ADDED each
entry's own evidence file, because a state becomes demonstrable when its evidence lands.
All five corroborate the date already recorded, to the day; `MATERIAL.RESOLVE`'s
REFERENCE_COMPLETE commit even says "32 checks", the figure that entry quotes.

**Three blocks pinned to the DIRECTED TEST rather than the RTL.** The obvious pin is the
commit that added the block's RTL, and `git ls-tree` shows the test was not there yet for
two of three. `UNIT_VERIFIED` is a claim about verification, so the RTL commit would have
dated the verification before its evidence existed — plausible, authoritative, false.

**Six off-ladder maturity words in one file.** `BUILT` on three blocks, `COMPOSED` on
PART.CLIPFEED twice (its log entry and, missed on the first pass, the field above it), and
"BUILT and COMPOSED" inside INPUT.SNAC's prose. The ladder is being written from memory
rather than read. `BUILT → UNIT_VERIFIED` and `COMPOSED → INTEGRATED`, each with the
reasoning beside it, and neither chosen upward for convenience: *an understated maturity
is also a false record — it tells planning a block needs work it has already had.*

**Three stale `source_ids` flags.** My first test grepped the RTL for `src_id`, found zero
in all five candidates, and was about to report five RTL gaps. **The field is `client`, of
type `zhao_client_e`.** Checking a block the flag says `true` and finding the same zero is
what caught it. Three of the five demonstrably carry an identity and the flag was simply
never updated.

**Two `superseded_by` paragraphs, three latency values, one `blocked_on` paragraph,
INPUT.SNAC's prose log, one illegal `note:` key** — all restructured with every word
preserved as a comment beside the field. Nothing was summarised away.

**One schema widening.** `tests.additionalProperties` false → a path schema, because five
blocks carry nine arms outside the five recognised kinds and every one names a real
committed test. Demoting them to comments would delete the machine-readable record of
nine tests; remapping them onto the five would put false labels in the ledger, since two
blocks simply have **two directed arms** and the schema has room for one.
`required: [directed, random]` is untouched, so this widens what may be ADDED and relaxes
nothing about what is OWED.

**One category error.** SW.CPUCOLL's `reference_model: zref::nav::Service` removed: a
software block IS the reference, not something with one. Measured rather than argued — 15
blocks are `kind: software` and it was the only one carrying the field.

## The 42 that remain, and what each actually needs

| count | error | what it really is |
|---:|---|---|
| 13 | `must match "then" schema` | umbrella; clears when the rows below do |
| 10 | `tests.random` required | **the test does not exist** |
| 9 | `reference_model` required | **no reference model exists** |
| 3 | `reference_model must be string` | the value is `null`, a deliberate "there is none" |
| 2 | `tests.random must be string` | same, `null` |
| 3 | `downstream` pattern | **the target block id does not exist** |
| 2 | `source_ids` const | the rule is over-broad, not the data |

**1. Nine blocks assert a maturity whose required random-test arm does not exist.** The
schema requires `directed` AND `random` past SPECIFIED. This is build-or-lower, per block,
and it is not a ledger edit. Note the trap: `"OWED -- NOT WRITTEN"` already appears in two
blocks and **satisfies rule V4**, because V4 tests truthiness. So the established
placeholder silences the gate while asserting nothing, and propagating it to nine more
blocks would convert nine real gaps into nine green lies. It was not done.

**2. Five blocks name no reference model their contract knows about.** Same shape: either
the model exists and must be found, or it does not and the maturity is overstated.

**3. Three `downstream` edges point at ids that do not exist.** `TERRAIN.GROUP_SEQ` and
`GEOM.GROUP_SEQ` are not block ids — the ledger has `TERRAIN.SEQ` and `FIELD.SEQ.*`, and
GEOM has **no sequencer block at all**. "GROUP_SEQ" is an architecture-prose name used in
contracts (`GEOM.WARP.md:127`) that never became an id. The pattern caught it only because
of the underscore. **Guessing the intended edge would write a wrong edge into the
dependency graph that tools then read as fact**, so the graph question goes to whoever owns
it.

**4. `source_ids` on two blocks is the RULE being wrong.** V4 demands `true` of every RTL
block. `TERRAIN.MATJOIN` and `TERRAIN.EDGERECON` issue no guarded memory traffic at all, so
there is no request for an identity to ride on and `true` would assert a property the RTL
does not have. Narrowing V4 to blocks that actually issue guarded traffic is the repair,
and changing a rule is not something to do as a side effect of clearing a gate.

**5. `null` on `reference_model` and `tests.random` is a schema question.** `null` says
"there is deliberately none", which is information; absence says nothing. The schema
forbids `null` and permits absence, which is backwards for a field whose whole point is to
record a considered "no". Widening it is defensible on the same grounds as the `tests`
widening — but it would convert five type errors into five required-property errors, so it
changes the diagnosis rather than the count, and it belongs with the decision in (1) and
(2) rather than ahead of it.

## Instruments left behind

* `tools/ledger/schema_gap_inventory.py` — splits the errors into STRUCTURAL / PRESENT BUT
  UNDECLARED / GENUINELY ABSENT, which is what made this tractable. It writes nothing and
  decides nothing, and it asserts at import that it can read known fields back from a known
  block. **A schema gate is easy to silence with a plausible string; that split is the
  guard against it.**
* Every repair script verified its evidence immediately before writing and aborted
  otherwise. Four aborted on real problems: a `state:` regex blind to the list dash, a
  same-line evidence pattern that could not see a folded scalar, a whole-record search that
  matched the same path under `tests:`, and an over-broad "no BUILT anywhere" assertion that
  would have forbidden the comment explaining the fix.
