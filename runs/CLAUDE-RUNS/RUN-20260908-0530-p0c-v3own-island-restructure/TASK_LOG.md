# Task Log: RUN-20260908-0530 - [Describe objective here]

**Created:** 2026-09-08 05:30 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-0530-p0c-v3own-island-restructure/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 05:30 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-0530
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## P0-C: replacing fragrob with v3own inside the texture island

Owner direction 49fc32e9 governs: *"Finish the texture island. Terrain,
projection, broad fit-sheet evacuation, and unrelated measurement-tool
expansion are not the current implementation priority."* Nothing here leaves
texture.

### Three gates, and where they stand

1. **v3own's own 541-check suite on the UNMODIFIED file** -- PASSES. The
   restructure is confined to the island top and a new expander; `v3own.sv` and
   the old `island_top.sv` are both untouched, the second because it is the
   end-to-end ORACLE for gate 3.
2. **`island_v3_composed_directed`** (the same test source as the oracle's,
   built with `ISLAND_V3=1`, so the stimulus is identical by construction) --
   **119 checks, 10 failing.** Flow control is closed; what remains is colour.
3. **The paired run** -- not started. Gate 2 first.

### Stage B, closed

`zhao_texture_frag_expand`: 10/10 directed checks, including the request
sequence element for element against a model of fragrob's expansion, exact
per-fragment issue counts, and 16 zero-sample fragments accepted issuing
nothing. Leaf fit **ok, 323 ALM, 451 reg, 364 memory bits, 0 DSP**, worst path
**+1.623 ns** (`cur_q.count[0]` -> `iss_tmu_valid_o`).

Its fit target carries `max_dsp: 0` and **no `max_alms`**. The architecture's
own estimate is a range (200-600); a gate written from a range is docket M7 --
a rule written from belief, which has already failed two correct blocks.

### The defect class this restructure keeps producing

Five times now, and every one identical in shape: **a bit-slice that was
correct until the MEANING of the bits changed.** The v3own re-key moved the
owner handle from `{slot[3:0], gen}` to `{slot[5:0], gen}`, and each of these
was a place that had taken the old identity apart:

| site | what it sliced | why it survived elaboration |
|---|---|---|
| `uvw_m` index | old slot width | in-range, wrong row |
| `fc_wp` / `fc_rp` | queue pointers | in-range, wrong entry |
| `rsp_class_i` | `[15:14]` | in-range, wrong class |
| AUX return token | `[AUX_TOKW-1 -: 4]` of a 6-bit slot | in-range, wrong owner |

None failed lint. None failed elaboration. All produce plausible wrong data.
**A re-key's real cost is not in the port list -- it is in every place that
ever took the old identity apart**, and no tool in this tree finds those.

### The two fixes that closed flow control

* The AUX return token **is** the owner handle. It was being re-assembled from
  a stale slice; it needed taking whole.
* The COMBINE handshake had **two different alignment terms**: `f_valid_i` on a
  registered `mat_rdy_q`, `cmb_ready_i` on a combinational compare. A valid and
  a ready computed from different notions of "the material is here" cannot
  agree, and 32 retired became 0. The registered flag is **deleted**, not
  repaired -- the fault was having a second source of truth, and fixing one of
  two would have left the trap armed.

Colour mismatches per phase across that fix: 9/2/5/4/5/1/10 -> 2/2/1/1/1/2. The
residue is a DIFFERENT fault, not the same one smaller.

### Next step, written down before reading anything else

Aux sheet coordinates come out **17 where 22 are expected** -- five aux requests
lost in the ISLAND's wiring, not the expander's (its own suite passes 22/22
under randomised backpressure on both sinks). Start there: it is a counter, and
counters localise better than colours.

One failing check is the TEST's, not the design's: *"FRAGROB accepted
fragments: expected 1, got 0"*. fragrob is deleted in v3 by design; that check
needs an `ISLAND_V3` arm before it means anything.

### A tool of mine read flattering on its first real use

`worst_path_index.py`, written this session to preserve M6 evidence, recorded
**slack 0.000 with empty node names** for a block whose true worst path is
**+1.623 ns**. Its three-column regex also matched rows in the per-path DETAIL
tables further down the report. Its self-check only caught TOTAL failure
(fewer than three modules), never per-row garbage -- and a clean zero is
exactly the kind of number nobody audits.

The second attempt, an eight-group lazy regex, backtracked catastrophically and
hung on a real report. Two wrong parsers is the argument for not matching a
fixed-width table with a pattern at all: it now SPLITS on `;`, requires all
eight columns, and rejects endpoint-less rows. It carries a fire test built
from the real debris, and that test was verified to FIRE by mutating the column
count back to three and watching it refuse.

## All three gates PASS

| gate | what it asks | result |
|---|---|---|
| 1 | v3own's 541-check adversarial suite on the **unmodified** file | passes |
| 2 | `island_v3_composed_directed` -- the oracle's own 119-check source, `-DISLAND_V3` | **119/119** |
| 3 | `island_v3_paired` -- both tops, identical stimulus, retired streams compared | **392 records byte-identical, order included** |

Plus `frag_expand_directed` 11/11 and the ORACLE still at 119/119, so nothing
done to shared blocks damaged the thing being compared against.

### The four defects gate 2 found, in the order they surfaced

1. **Undriven ports.** `fr_tmu_valid` on the planner and AUX sides had no
   driver at all. Later, sweeping all 42 outputs found two more --
   `cnt_fragments_o` and `cnt_fragrob_id_errors_o`, both fragrob's. An undriven
   output does not fail elaboration or lint; it reads as a clean zero, and a
   counter reading zero looks exactly like a stage that is quiet.
2. **Owners leaking at admission** -- `own_adm_valid_c` did not require
   `rcp_v_ready`.
3. **The AUX return token**, re-assembled from a 4-bit slice of a 6-bit slot.
4. **Two alignment terms on one handshake**, and then **two stage misalignments**
   -- the AUX world coordinates and the palette tables.

### The two misalignments are the same mistake, and it is not the re-key's

The five stale slices were all the identity change. These two are different and
worth separating, because the lesson is not the same one:

* the AUX request's world coordinates came from `own_out_ctx` -- the context of
  whatever the OUTPUT stage was emitting. A different fragment entirely.
* `palslot_m`/`palgen_m` were written at ADMISSION (an input-stage event) from
  `f_pal_slot_c`/`f_pal_gen_c` (planner-stage values), while `mat_m` beside them
  was written from the input ports. The two per-owner tables disagreed about
  which fragment they described.

**Both are "an attribute read from the wrong stage", and this island's own test
already had a check for that in prose** -- "travels with its fragment instead of
being read off the input pin twelve clocks late". The check existed; the defect
was reintroduced next to it.

The palette one showed as only THREE stale lookups because a phase holds one
palette slot for most of its fragments, so the misalignment is invisible
wherever old and new happen to be equal. **A defect mostly masked by uniform
stimulus is not a small defect** -- it is a large one with a lucky workload.

### STALE MEASUREMENT, stated rather than quoted

The expander's leaf fit -- **323 ALM, 451 reg, 0 DSP, +1.623 ns** -- finished at
05:21. The `f_ctx_i`/`aux_ctx_o` ports and the 64-bit `ctx` field in the 4-deep
fragment queue landed at 05:48. **That number therefore describes a block that
no longer exists**, and it is short by roughly 256 flops of queue plus routing.

It is recorded here as stale rather than repeated as a result, because "never
compare a current file to an old measurement" is exactly the trap, and a fit
number carries a reassuring air of having been measured. Re-fit is queued behind
the Stage C island fit.

### Running now

`zhao_texture_island_v3_top`, registered under the ORACLE'S OWN REDLINE --
`max_alms: 7500`, `max_registers: 9000`, `max_m10k: 64`, `max_dsp: 14`. Same
rules as `zhao_texture_island_top`, because the question is whether the
restructure fits the budget the island already had; softer rules would answer a
different question while producing a number that looks comparable.

## WHERE I WAS, written before reading any fit result

Owner brief `ZHAOZHOU_TEXTURE_ISLAND_NEXT_REARCHITECTURE_2026-09-08.txt` filed
at repo root (matching the 2026-09-07 master's convention), checks extracted to
`tools/checks/next-rearchitecture-20260908/` and re-run clean. Plan written to
`reports/PLAN-AFTER-OWNER-BRIEF-20260908.md`.

**In progress when the fit result lands:** extending
`tools/quartus/undriven_outputs.py` from *direct driver presence* to
*transitive live fan-in* — the brief's §3.1 note, and the exact gap that let
three dead error paths pass a checker I had just proved fires.

**Next after that, in the brief's order:** §3 repairs A/B/C, which all touch
`zhao_texture_island_v3_top.sv` and are therefore BLOCKED until the fit
releases its closure. The brief is explicit: *"Do not change the source under
that run."*

## Brief incorporated; fault test written ahead of the repair

* Brief + checks filed and committed; checks re-run clean, `sha256sum -c` OK,
  every check carries a mutant it detects.
* **Evidence-port audit: 30 ports, 3 with dead fan-in** -- exactly the three the
  brief named. My proximity heuristic said four; `err_aux_degenerate_o` was a
  false positive (it merely shares an `always_ff`). The outside review was more
  accurate than the local mechanical check.
* **A generalised fan-in checker was started and reverted.** It flagged 23
  signals in one file, ~4 real, the rest arrays written as `mat_m[idx] <= ...`
  and input ports. The brief forbids the parser project in the same paragraph
  that names the gap. Reverted to the narrow port check that was proved to fire.
* **`tests/texture/island_v3_fault_directed.cpp` written**, built as a target,
  and deliberately **not** registered with `add_test`: it is written to FAIL
  against today's RTL, and a red suite hides regressions. `add_test` lands in
  the same commit as the §3.1 repair, so the test is seen to fail and then pass.
* M6 amended in the tool that embodied it; the +10.90 MHz claim conceded as
  over-stated; the 7,500 ALM redline retained as the historical-rule judgment
  only, not as a physical cliff.

**Blocked on the fit:** all three §3.1 repairs touch
`zhao_texture_island_v3_top.sv`, inside the running fit's 18-file closure. The
brief: *"Do not change the source under that run."* The frozen specimen's digest
still matches the committed file byte for byte.

## The fault test is written and FAILS, as intended

`build/tests/test_island_v3_fault_directed.exe` -> **1/5 checks FAILED**, and it
is the right one:

* clean traffic admitted, flag clear — pass
* invalid-class fragments **accepted at ingress** — pass (this is the check that
  makes the next one unambiguous: without it, "the counter did not move" could
  just mean "nothing arrived")
* **`err_class_invalid_o` moved — FAIL.** Expected 1, got 0. The port is a
  constant zero.
* legitimate traffic still admitted afterwards — pass

Not registered with `add_test`. It lands with repair A, in one commit, so it is
seen to fail and then to pass.

## Still blocked

The Stage C fit is still running — longer than the expander's 26 minutes, which
is expected for a composed island. All three §3.1 repairs touch
`zhao_texture_island_v3_top.sv`, inside its 18-file closure, and the brief says
not to change the source under that run. Nothing else in the §3 work is
blocked — the audit, the fault test, the docket entry and the plan are all done
and pushed.

## Background lanes tidied, and one debt created by doing it

Five stale poller tasks stopped. Two kept: the fit itself and a monitor that
fires when the `quartus_fit` process exits — which covers failure as well as
success, because either way the process goes away.

**Debt:** one of the stopped tasks was a full `ctest -L fast` sweep whose result
I never read. The geometry subset passed 66/66 separately, and the three texture
gates pass, but **the whole fast suite is unverified since the shared-block
edits** (`cache_pipe`'s SRCW threading, `rcp24_svc`). Re-run it once the fit
frees the CPU. Written down because a cancelled check is exactly the kind of
thing that silently becomes "we ran the tests".

**Fit health, measured not assumed:** `quartus_fit` PID 11768 burned 46 CPU
seconds over 45 wall seconds — saturating a core, so it is placing, not wedged.
No `.rpt` writes for 20 minutes is normal mid-placement.

## COMBINE lane, worked while the island fit holds its closure

The roadmap's "COMBINE.V1 DSP measurement" is **already landed**: V1 is `ok` at
1,475 ALM / 893 reg / 36.28 MHz. What is actually outstanding is M2's deletion
trigger, whose preconditions I checked rather than assumed.

**Written up in `reports/M2-COMBINE-DELETION-PRECONDITIONS-20260908.md`.** Two
findings:

* **V1's fit predates the provenance guard**, so there is no `.sources.sha256`
  and the recorded number cannot be confirmed against the file on disk. A
  deletion trigger reading *"delete when v1 is measured"* should not fire on a
  measurement nobody can tie to a file. **Re-fit queued** — it launches
  automatically when the island fit's process exits, so the toolchain does not
  sit idle.
* **`zhao_prod_top` instantiates V1 at 36.28 MHz in a 100 MHz machine**, while
  the island already uses V2 at 870 ALM / 114.04 MHz. I compared the interfaces:
  V2 is a **strict superset** — 26 shared ports, no direction disagreements, one
  extra output. The swap needs no port work, only the manifest plus a
  `gen_prod_top.py` regeneration. **-605 ALM, +77.76 MHz.**

Deletion and production-composition changes are the owner's call; the analysis
is done and the recommendation is written.

### A wrong call I made and withdrew, in the same hour

I first reported V1's fit **stale** because the source mtime is newer than the
fit summary. Wrong test. Git shows **no commit touching that file since
2026-09-06**, and this tree's CRLF normalisation moves mtimes for reasons that
have nothing to do with content.

Using mtime as a proxy for "the file changed" is the same class of error as
measuring a drawing to get a 3D radius: a real number standing in for the thing
actually meant. The genuine problem turned out to be adjacent and worse — not
that the measurement is stale, but that **nothing can tell us either way**.

## Brief §4.1 landed: the identity encoding freeze

`fpga/rtl/texture/zhao_texture_ident_pkg.sv` — named pack/unpack for the four
encodings, plus a probe module and `ident_encoding_directed`: **13 checks over
all 262,144 combinations**, the same space the brief's own bundled model check
covers, so the two are directly comparable.

Non-vacuity is asserted, not assumed: slot bits 4 and 5 exercised 131,072 times
each (the two bits the 4-to-6 widening created, and the ones every stale slice
dropped), generation 0 and 255 1,024 times each, illegal sample index 3 65,536
times.

**Shown to fire.** The T2 ticket was rebuilt in owner order — the exact
confusion the package exists to prevent, since `{slot,gen}` and `{gen,slot}` are
both 14 bits and no width check can separate them. Two checks failed across
essentially the whole space; the package was restored from a byte backup and
`git diff` is empty.

**One honest weakness in that test**, noted rather than papered over: the
"ticket differs from owner" assertion compares the C++ model's ticket against
the model's owner, so it documents that the two encodings genuinely differ but
does **not** exercise the RTL. It did not move under the mutation. The two
checks that did catch it are RTL-vs-model comparisons, which is the right shape.

It is registered in ctest — the package is new, nothing depends on it yet, and
it passes today. Adopting it inside the island is a follow-on that must change
no bit; that is what the bit-identical test is for.

## Brief §4.2 and §4.3 delivered while the fit holds its closure

* **`reports/V3-RECORD-STAGE-CONTRACT-20260908.md`** — the field/event/stage
  table §4.2 asks for, derived from source. The finding worth keeping: the
  island stores fragment data under **two keys** (`fc_wp` front-end queue,
  owner slot), and **both copies of the base colour are justified** —
  `fbase_m[fc_rp]` feeds the planner's `req_mat_*`, `mat_m[owner]` feeds COMBINE
  after arbitrary reordering, by which time the queue entry has been recycled.
  Deleting either as "duplicate storage" breaks the machine. That is the §8
  saving that would have been claimed without a read-site check.
* **`reports/V3-ADAPTER-INVENTORY-20260908.md`** — §4.3. Confirmed the six
  `SRCW` sites in `cache_pipe`: two ports and **four internal**
  (`rq_src`, `c1_src`, `c2_src`, `rs_src`). Also recorded that the file's slack
  is now literally zero (`SRCW-2-$clog2(DEPTH)-2-GENW = 0`), so a 6-bit slot has
  no room left — read that before widening anything again.
  **Five of the eleven listed boundaries were defects found during this
  integration.** That ratio is why the inventory is a file rather than care.
* Two raw-slice sites in the expander (`:197`, `:208`) are exactly
  `make_token(make_sample_handle(...))` from the new package. **Not changed** —
  the expander is inside the fit's closure.

## Fast suite

Running at `-j1` to avoid starving the fit. This is the debt recorded earlier:
the full fast label has been unverified since the shared-block edits to
`cache_pipe` and `rcp24_svc`. Result pending.

## Brief §5.1: the zero-work lifetime probe, written

The brief's second contract hole, and it is careful about its own status:

> *"Evidence classification: SOURCE-PROVEN mismatch between two completion
> domains; REACHABILITY/IMPACT of a destructive same-slot reuse schedule NOT
> independently proved here."*

v3own makes an owner with `adm_req_i == 0` ready **at admission** — correct for
its leaf contract, since it owes no TMU or AUX source. The composed top still
sends that fragment through RCP, PERSPUV and the expander, which hold its token
and can read owner-keyed sidecars. Those are two different completion
conditions, and the implication *"owner may retire -> no front-end reader
remains"* is not established.

Added as phase 4 of `island_v3_fault_directed`: **300 zero-sample fragments**,
wrapping the 64-slot owner space about 4.7 times while front-end stages are
mid-flight, checking that every fragment retires exactly once with its own tag.

**What this can and cannot show, stated up front so the result is not
over-read:** a FOUND trace is a reproduced defect, which is what the brief says
nobody has yet. Absence of one is *not* a proof that the implication holds — it
is one schedule out of many, and the brief itself notes that 392-record ordinary
parity does not explore prolonged stalls and wrap schedules either. The
non-vacuity check (`submitted >= 200`) at least guarantees the wrap actually
happened rather than the phase quietly testing nothing.

Building behind the fast suite to avoid three-way CPU contention with the fit.

## §7 groundwork, and repair A confirmed implementable

**`reports/V3-METADATA-JOIN-CANDIDATE-20260908.md`** — the read-site inventory
for the brief's §7. `sampmeta_m[64][3]` is 192 x 21 bits with **one writer and
three asynchronous readers** (bilinear :1626, CLUT :1809, nearest :1934), each
indexed by a different token.

The useful discovery is that **the join point already exists**:
`zhao_texture_rsp_dispatch` is one input stream splitting into four class
outputs. So §7 is not a new stage — it moves an existing read to where the data
already flows, and the three async ports collapse to one synchronous read.

**No saving is claimed**, per the brief: the V3 MAP report does not exist yet and
the P0-E census describes a different machine. The cost side is written down too
(each class queue's payload grows by the metadata width), because a candidate
that only lists its benefit is not a candidate.

**Repair A verified implementable** without new plumbing: `own_adm_accept` is
already a live v3own output used at :1228 and :2338, and `frag_class_i` is an
input port, so the brief's specified form —
`own_adm_accept && (frag_class_i == CLS_ERR)` — drops straight in beside the
`class_m`/`palslot_m` writes that were fixed this morning. Blocked only on the
fit releasing the closure.

## POSITION BEFORE READING THE STAGE C FIT RESULT

Written first, because fit results redirect the work and the half-finished thing
in hand is what gets lost.

**In flight:**
* zero-work lifetime probe (§5.1) — written and committed, chained to build
  after the fast suite finishes. Not yet run.
* full fast-label ctest at `-j1` — running, tests passing so far. This is the
  debt from the shared-block edits (`cache_pipe` SRCW, `rcp24_svc`).
* V1 re-fit — queued, launches automatically now that the island fit's process
  has exited.

**Next step regardless of what the receipt says:** the §3.1 repairs, now that
the closure is free. A is confirmed implementable
(`own_adm_accept && frag_class_i == CLS_ERR`); its test already fails 1/5
against current RTL and flips to 5/5 with the fix, and gets its `add_test` line
in that same commit.

## STAGE C RECEIPT — and the §3 repairs landed on top of it

**`zhao_texture_island_v3_top`: 13,133 ALM / 20,561 reg / 45 M10K / 17 DSP /
82.41 MHz, `failed:structure`, 14,351 s.** Written into G1D as **4.3h**.

**The like-for-like baseline is `@p0c-stageA`, and I verified it by hash rather
than by reading comments.** Its recorded digest MATCHES the current
`zhao_texture_island_top.sv`; the plain `island_top` row's does not. The plain
13,601/66.77 numbers predate Stage A's `uvw_m` conversion, which the V3 top
carries — comparing against them would have credited this restructure with a
saving somebody else made.

| | oracle `@p0c-stageA` | V3 | delta |
|---|---|---|---|
| ALM | 11,562 | 13,133 | **+1,571 (+13.6%)** |
| registers | 19,203 | 20,561 | +1,358 |
| M10K | 39 | 45 | +6 |
| DSP | 17 | **17** | **0** |
| Fmax | 84.03 | 82.41 | **−1.62 MHz** |

**The DSP failure is inherited, not caused.** Identical 17 on both sides, and 17
appears as far back as G1D 4.3c. And `@p0c-stageA` reports `ok` at 17 against
`max_dsp: 14` only because its target carried no rules when it ran — **the V3
fit is the first island fit these historical limits have ever gated.** A first
application of a rule is not a regression.

Both judgments kept separate per brief §2.3: **historical-rule result FAIL**
(recorded, gate unchanged); **product-allocation decision NOT MADE** (needs
count-once whole-console accounting and owner approval). The gap that matters
most is 82.41 against 100 MHz, not the area.

### §3.1 repairs A, B, C — applied, and the three gates held

* **A** — `err_class_invalid_o` now counts `own_adm_accept && frag_class_i ==
  CLS_ERR`: the ingress beat and that beat's own raw class. **Its positive fault
  test flipped from FAIL to pass.**
* **B** — the identity sticky covers all **six** named categories (range, stale,
  unsolicited, duplicate, illegal issue, unauthorized final), and
  `cnt_fragrob_id_errors_o` now sums six where it summed three.
* **C** — the expander exposes a **real** capacity violation (`accept && full`),
  unreachable if `f_ready_o` is correct. Backpressure is deliberately not
  counted; the brief is explicit that conflating them is how a port gets tied to
  zero and called preserved.
* The dead FRAGROB declarations are retired.

Gates after: **gate 2 119/119, expander 11/11, oracle 119/119.** Nothing broke.

### A stale binary nearly reported the repair as a failure

The first post-repair run showed the fault test still failing 1/5 — *identical*
to before. The build had failed with `missing terminating " character`, the
heredoc escape trap for the third time today, so I was reading the OLD binary.
Caught only because I read the build output instead of the test result. This is
CLAUDE.md's stale-binary trap arriving at the exact moment it would have been
most convincing.

### The probe found something, and it is not what the brief hypothesised

`reports/V3-ZEROWORK-STALL-20260908.md`: **zero-work fragments never retire in
the composed island** — 16 admitted, 0 retired, where the same loop at
`sample_count = 1` retires normally.

**Gate 2 has never driven `sample_count = 0`** — it drives 3, and 1-or-2. The
case is covered only by the expander leaf, which is exactly the insufficiency
the brief named. It sat behind 119 composed checks, 541 owner checks and a
392-record paired run because none of them ever presented the input.

Mechanism NOT traced, and stated as such. 16 smells like a queue depth, not a
semantic bound. It is also **not** the destructive slot-reuse trace §5.1
hypothesised — nothing completes, so no slot frees early. That question is still
open.

**V1 re-fit is running**, launched automatically when the island fit exited.
