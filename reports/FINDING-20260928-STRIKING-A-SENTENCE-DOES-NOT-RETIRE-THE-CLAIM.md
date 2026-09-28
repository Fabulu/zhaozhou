# Seven sentences in the composer that its own tree refutes — and four were found a week ago

Coordinator, 2026-09-28. Found while triaging the 27 real `lint_zhao_console_board`
warnings one at a time. The lint found one of these; **reading around it found
six more**, and the file already contained a record of four of them.

Not one is caught by any gate.

**On direction, corrected against the file's own words.** A first draft of this
report said "every one overstates the work remaining". The file states it more
carefully at the end of its `(X4)` block, and the file is right:

> *"EVERY ONE of these makes the remaining work look SMALLER **or the entry look
> more BLOCKED-ON-A-PORT than it is.** 15.35's direction tell, holding across
> five independent sentences."*

Those are two different distortions and the sentences split between them. What is
uniform — and is the checkable invariant — is narrower: **each names a consumer,
a port or a converter as ABSENT when it is present.** Whether that makes the
total look bigger or smaller depends on which list the reader is holding.

Note also that this pattern is **already recorded** as HANDOVER 15.35's "direction
tell". What is new below is not the pattern; it is the DATES, and the fact that
the sentences recorded a week ago were never corrected at their sites.

---

## The sharpest instance: refuted 3 hours 27 minutes after it was written

`zhao_console_core.sv` at the GEOM.PARAMWALK instantiation:

> `// ---- THE FETCH ARM'S VERTEX FIELDS: TIED, SAME DECLARATION ----`
> `// … It has no consumer here for one measured reason: `job_*` IS NOT A`
> `// PORT. … so the multiplex has a back end and NO DOOR -- 2,065 wires`
> `// through two module boundaries to open it. See entry I55 …`

Measured:

| | commit | time | |
|---|---|---|---|
| claim written | `afafb3e4` | **2026-09-27 06:03:38** | MUXBUILD merge |
| claim refuted | `4513c7c5` | **2026-09-27 09:30:36** | SWAPCLOSE — "the raster reads its geometry back out of SDRAM" |

**Three hours and twenty-seven minutes.** `pw_t_a_invw_w` and its twenty-six
siblings are packed by `walk_attr_pack` into `pw_attr_a/b/c_w` and taken by
GEOM.SETUP and GEOM.ATTRPACK through the `tw_active_w` select. The door the
sentence says would cost "2,065 wires through two module boundaries" **was
opened that morning, by the very entry the sentence tells the reader to consult.**
The paragraph immediately above it, about "SCHEMA v2's five", is stale for the
same reason and in the same commit pair — `pw_t_area2_w` is read at `:21809`.

A reader who believed this would have costed a rebuild that was already done.

---

## The full list, all measured, all in the same direction

| site | the sentence | what the tree says |
|---|---|---|
| `:7284` | `ordinal 2 material -> NOBODY. The one open channel.` | `efa_material` → `u_terrain_matjoin` → `u_terrain_matpub` |
| `:30358` | `2 material -> NOBODY. The only channel still open` | same — **fixed 2026-09-28** (`7fda8717`) |
| `:30390` | `ans_present_o` is `PRODUCED, NOT CONSUMED` | `efa_present[2]` read at `:30491` |
| `:14703` | `The three adapter outputs with no consumer are left OPEN` | **one** is open (`nav_cost_o`, owner-ruled); two have consumers |
| `:8407` | `WHAT CLOSING THE REST NEEDS, unchanged` | three-quarters spent |
| `:32805` | SCHEMA v2's five "unconnected … same reason as the twenty-eight vertex fields below" | `pw_t_area2_w` read at `:21809` |
| `:32836` | "a back end and NO DOOR — 2,065 wires" | door opened `4513c7c5`, same morning |

*Line numbers above are at `f2881730`, the commit before the corrections — taking
this report's own rule 2 below. **All seven are corrected at their sites as of
2026-09-28**, along with the `(X4)` audit block that had recorded four of them.*

And one that is not prose at all but the identical failure in a wire —
`efa_ans_ready`, declared beside the live `efa_ans_valid`, **driven by nothing
and read by nothing**, while the adapter's real ready is `tvj_a_ready`. Verilator
was the only thing in the tree that objected, and only because it counts unused
signals. Fixed in `7fda8717`; it had already nearly attracted a `busy` term.

---

## The part that matters: this was found a week ago and recorded instead of fixed

`:6309` already carries an audit headed

> `(X4) FOUR MORE SELF-REFUTING SENTENCES IN THIS FILE, struck where they`
> `     stand rather than quietly overwritten:`

listing four of the seven. **All four still stand at their sites today.** And the
audit has itself gone stale: it points at `:29232`, `:29370`, `:29249` and
`:13990`, none of which resolve any more — the real lines are `:30358`, `:30492`,
`:30390` and `:14703`. So the record of the rot rotted, in the same file, by the
same mechanism it was written to document.

This is not the first time. `:8418` carries a block headed *"READ THIS BLOCK
FIRST. RE-MEASURED 2026-09-21 … EVERY RECORDED BLOCKER BELOW IS SPENT"*, which
says four assertions are false and are "struck inline where they appear" — and
then records the decisive detail:

> *"Owner ruling R165 found two of three spent on 2026-09-19; the count today is
> higher, not lower, and the entry had **re-asserted one of them AFTER R165
> struck it**."*

**A claim that was struck came back.** That is the whole finding.

---

## The law

**Striking a sentence in place does not retire the claim.** A struck sentence is
still a sentence sitting in the file, in the reader's path, phrased as a fact. It
gets quoted by the next pass, copied into the next entry, and — demonstrably here
— re-asserted after being struck. Three separate audits (R165 on 2026-09-19, the
re-measure on 2026-09-21, the X4 block) each found the same disease, recorded it
accurately, and left the sentences where they were. The count went **up**.

Note the direction, because it is the tell this repository already names. Every
one of these overstates what is left to build. CLAUDE.md's own refusal law says
why none of them was caught: *"A wrong BUILD gets caught by a gate, a bench or a
fit. A wrong REFUSAL is caught by nothing at all — it produces no output to be
wrong, no counter to read zero, no red. It is invisible to every instrument in
the tree by construction."* A comment asserting a blocker is a refusal with no
author left to argue with.

**Three rules, each earned above:**

1. **Delete the claim or derive it — do not strike it.** If the sentence is
   wrong, the correct edit removes the assertion and states what is true. A
   strike-through leaves a live sentence and adds a second thing to maintain.
2. **A line number in a comment must be ANCHORED TO A COMMIT, or not written.**
   All four of the X4 audit's pointers were bare and all four rotted within a
   week, which makes a true finding unverifiable.

   **The counter-example is in the same file and it is the model.** The
   `tps_v_cell_fire_c` attribution note writes *"By SOURCE: at `62d8b6a7`,
   :27814 is `assign tps_v_cell_fire_c = …` and :28412 is
   `.mat_we_i (tps_v_cell_fire_c),`"*. Checked at that commit on 2026-09-28:
   **both lines are exactly as quoted.** A bare `:28412` against HEAD lands on
   `zhao_part_project` — so the anchor is doing the whole job, and an earlier
   draft of this report mistook that pointer for a fourth rotted one by reading
   it against the wrong revision. Anchored line numbers survive; bare ones do
   not. Prefer names anyway, because they need no anchor.
3. **A producer-side comment must not describe its consumers.** The file says
   this about itself, at `:30350`, and then did it anyway — twice, in the two
   paragraphs on either side of the sentence that says it. The consumer list is
   derivable in one grep and authoritative only when derived.

---

## The missing half, and it is mechanisable

Every one of these is **checkable**. "Signal `X` has no consumer" is not an
opinion — it is a grep, and Verilator already computes it. A tool that extracted
claims of the form *"`sig` … NOBODY / no consumer / not consumed / left OPEN"*
from comments and tested each against the elaborated design would have found all
seven, and would have found the `efa_ans_ready` decoy as a bonus.

That is the `.gitignore` lesson exactly: the knowledge was written down, in
detail, by people who knew precisely what they were deferring, and **nothing in
the tree ever read it back.** Three audits proved a human sweep does not hold.

Not built here — it wants a design pass of its own, and the immediate value was
in the corrections. Recorded so the next sweep builds the instrument instead of
being the fourth one.
