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

## RETRACTION: the zero-work stall was mine, not the RTL's

I claimed and committed a report saying zero-work fragments never retire in the
composed island. **Wrong.** With a clean reset before the phase:

```
zero-work wrap: submitted 300, retired 300 (~4x slot wrap)
[island_v3_fault_directed] 9 checks passed
```

`quiesce()` never raises `fill_data_valid_i` — this harness deliberately does
not model texture memory, because phases 1-3 only need fragments *admitted*. So
every `sample_count = 1` fragment from those phases was admitted and could never
complete, and by phase 4 the machine was full of them. **16 was PERSPUV's
`NTOK`, reached by the leftovers.**

**How I got there:** I checked `FCTXN = 64`, found it did not match, found
`NTOK(16)`, and stopped — because 16 matched and the story was coherent. Every
step was true; the conclusion did not follow, because I never asked whether the
*earlier phases* had consumed those tokens.

The docket's law is *"the first explanation that absolves the design is the one
to check hardest."* This one **accused** the design and I gave it the same free
pass — a fresh defect in a freshly-restructured block is a satisfying story. The
check that settled it cost one reset and one rebuild.

The counters said so immediately once printed: `plan 9 | cache 1 | dispatch 0`
across the whole run is not what a zero-work-specific defect looks like. I had
the localisation tool and reached for the narrative first.

### What survives

The probe passes and is now **registered in ctest**, having been seen to fail
and then to pass. It is worth keeping:

* the first composed test in this tree to drive `sample_count = 0` at all —
  `island_composed_directed` drives 3, and 1-or-2, never 0;
* it wraps the owner slot space ~4x, which the 392-record paired run does not;
* `submitted >= 200` non-vacuity makes the pass mean something.

It is **still not a proof** of §5.1's implication — one schedule, free-running
consumer. Absence of a trace is not absence of the hazard.

### Current state

**6/6** across the texture gates: gate 2 (119), gate 3 paired (392 records),
oracle (119), expander (11), encoding freeze (13), fault probe (9).

### Open, small, real

`cnt_combine_jobs_o` reads 2,558,523,520 on a run that issued no combine jobs,
and a different garbage value on the previous run. Looks unreset or
X-propagating. Not investigated.

## I wedged the fast suite by running ctest against a busy build tree

The `-L fast` run sat with **ctest CPU frozen at 8 seconds and no test process
alive**. Not contention — wedged.

Cause: I launched several other `ctest --test-dir build` invocations while it
was running. They share `build/Testing/Temporary/`, and one of them overwrote
`LastTest.log` — which is also why the log showed six entries from my six-test
run instead of the suite's progress. Concurrent ctest on one build tree is not
safe, and I did it three times.

Killed and re-queued to run **after** the V1 fit, serialised, with nothing else
touching the build tree.

Worth writing down because the failure mode is quiet: a wedged ctest looks
exactly like a slow one, and I had already told myself "it is contending with
the fit" — a ready-made explanation that fit the evidence and stopped the
question. Same shape as the two false alarms above.

## Second false alarm corrected

`cnt_combine_jobs_o` is an **array of eight** (one per recipe). I printed it
with `%u`, which formats the array's address — two runs, two addresses, which
read exactly like a garbage counter. Summed properly it is **0**, correct for
zero-work fragments. My note calling it "unreset or X-propagating" is withdrawn.

## Two more of §3.2's schedules, both drivable from the boundary

`island_v3_fault_directed` is now **17 checks**, all passing:

* **Phase 5, consumer stalls mid-flight.** Sink shut for a 40-cycle window while
  fragments are certainly in flight — not at the drain, where a stall proves
  nothing. 120 submitted, 120 retired, none duplicated, no foreign tag, and
  **8 accepted while the sink was SHUT**. That last number is the non-vacuity
  check: without it the phase is just a slower phase 4, and the credit path
  (reservation versus acceptance, the M6 defect) is never exercised.
* **Phase 6, reset mid-flight then a fresh namespace.** 40 fragments admitted
  and deliberately left in the machine with the sink shut, then reset asserted,
  then 80 fresh fragments with tags disjoint from the abandoned ones.
  **80 retired, 0 stale** — nothing from before the reset came back, and the
  island did not return wedged.

Phase 6 is the observable half of the brief's §5 concern. What it does **not**
cover is the part the brief says is outside the owner's interface: an external
producer still delivering responses for the old namespace. That needs a
producer-side acknowledgement phase, which does not exist at this boundary.

The remaining §3.2 cases all need internal fault injection — stale/unsolicited
returns, duplicate returns at adjacent claim/publication distances,
range-invalid sample index 3, unauthorized FINAL. None is drivable from the
top's ports, because the paths that would carry them are internal to the cache
and the owner. That is a design question (an injection port, or leaf-level
tests against v3own directly), not something to fake from outside.

## M2's precondition was never met, and the ledger said otherwise

Reading the JSON row rather than the docket's summary of it:

```
zhao_texture_material_combine_v1
  status          ok
  ruleViolations  ['ALM 1475 > allowed 800',
                   'registers 893 > allowed 500',
                   'fmax 36.28 < required 125']
  partial         True   (analysis_and_synthesis)
```

**One row in 114** claims success while listing violations, and it is the row
M2's deletion trigger depends on. Every other row is consistent — the V3 island
reports `failed:structure` and lists its three.

The row's own note says how: *"Fitter and STA run by hand after the
run_block_fit watchdog was killed so the fit could outlive its 3000 s budget."*
Assembled manually, so whatever derives `status` from the rules never ran.

**I had repeated that `ok` in this morning's M2 report**, having read the status
field. The same note also records **unresolved hold slack −5.284 ns** and setup
slack −17.561 ns against a 10 ns clock, and warns the row *"reads about 2 MHz
low"* because it ran in BALANCED mode.

So the correction is not "V1's measurement is unverifiable" — it is that **V1
fails ALM by 84%, registers by 79% and Fmax by 3.4x against its own §15.5
variant-A bounds.** It is an undischarged claim, not a demonstrated replacement
for the refuted `zhao_texture_combine`. V2 is a complete fit at 870 ALM /
114.04 MHz, inside the bound V1 misses.

`tools/quartus/check_fit_ledger.py` now refuses to let the ledger contradict
itself. One invariant only: a row may not claim success while listing
violations. It does not re-derive rules or judge severity. Fire test built from
the real row, and it discriminates — the honest `failed:structure` row beside it
is not flagged.

This is the "broken instrument lies in ONE direction" law arriving at the single
place it could do the most damage: a docket entry, a report and a recommendation
all inherited a wrong `ok`.

## THE RECEIPT CHOOSES §6

The brief deferred the architecture choice to *"the first actual V3
composition's physical report"*. That report now exists, and it answers:

```
worst path  slack -2.134 ns
  from      frag_depth_i[14]
  to        zhao_raster_rcp24_svc:u_rcp|c_x[0][22]
```

**Raw fragment depth straight into RCP context state** — §0's first branch,
almost word for word. Cross-checks to 82.41 MHz, exactly the fit's headline
Fmax, so the gating path and the reported number are one measurement.

**The brief predicted this from the LEGACY island and explicitly refused to
reuse it** — *"conditional on fresh V3 attribution; does not blindly reuse
yesterday's critical-path diagnosis."* It is now confirmed on a different
composition, its own 4-hour fit, digest-verified specimen. A prediction made on
one design and confirmed on another is worth more than the same number twice.

§7 is not refuted, only deprioritised: one writer, three async readers, join
point already present in `rsp_dispatch`. It simply is not what gates the clock.
Its groundwork stands, still with no saving claimed.

Second-tier families (`cq_rp` -> palette `cold_o`/`stale_o`, and a RAM block)
recorded, **not** interpreted — M6 as amended says a family is evidence to look
at, not a conclusion to draw.

Written up in `reports/V3-PATH-CHOOSES-SECTION-6-20260908.md`.

## Correction, immediately: §6 alone buys ~0.2 MHz

I wrote "the receipt chooses §6" and pushed it before doing the check §6.1
explicitly demands — *"inspect data delay as well as slack"* for a virtual-input
path. Doing it changes the conclusion.

**Splitting all 200 paths by origin:** 113 start at a virtual pin (worst
−2.134 ns), **87 start inside the design (worst −2.093 ns)**. The port boundary
is worth **41 picoseconds**. The docket already learned this once — *"the
artefact was real and almost irrelevant"* — where it was worth 4 MHz of 36.
Here it is worth 0.041 ns.

The worst internal path is a different family:
`rsp_dispatch|cq_rp[0][0] -> palette_res|cold_o[26]`.

**But the data delay rescues the target for a better reason:**

| path | slack | skew | data delay |
|---|---|---|---|
| RCP `frag_depth_i` -> `c_x` | −2.134 | **+3.342** | **15.416 ns** |
| palette `cq_rp` -> `cold_o` | −2.093 | −0.556 | 11.477 ns |

The RCP path carries **3.9 ns more logic** and is being *flattered* by 3.3 ns of
favourable skew into looking merely tied. On combinational depth — the thing an
architecture change can actually move — it is 34% worse and is the deepest cone
in the design.

**Corrected conclusion, both halves required:** §6 is the right work AND it
will not lift the clock alone. Completing it moves 82.41 -> about 82.6, because
`cq_rp -> cold_o` becomes the gate 41 ps later. The palette/dispatch family is
**co-equal, not second tier**, and neither §6 nor §7 as written covers it.

Any plan that reports §6 as the road to 100 MHz is wrong. I had written exactly
that an hour earlier, on slack alone.

## POSITION BEFORE READING THE V1 RE-FIT

In hand: the §6 correction is written and pushed. **Next step regardless of
this result:** look at `rsp_dispatch|cq_rp -> palette_res|cold_o`, the co-equal
internal family that neither §6 nor §7 covers.

Expected from this fit: a row with a provenance digest and a `status` derived
from the rules rather than left at `ok` by a hand-assembled run.

## A second owner brief arrived, and it caught a real error of mine

`reports/ZHAOZHOU_TEXTURE_ISLAND_POSTFIT_ARCHITECTURE_2026-09-08.txt`, written
after reading today's results — including my retraction. Rebased onto it cleanly.

**§2.2 rejects my repair C, correctly.** I wired
`if (accept_c && fq_full_c)` and defended it as *"unreachable if `f_ready_o` is
correct, which is exactly what makes it worth exposing."*

It is not unreachable. `accept_c = f_valid_i && !fq_full_c`, so the condition is
`f_valid_i && !fq_full_c && fq_full_c` — **identically false**. I replaced an
undriven port with a differently-dead one and called it a repair.

The brief's discriminating case: change `>=` to `>` in `fq_full_c` so the queue
admits a fifth entry — a real bug — and the old monitor **still cannot fire**,
because the bug moves acceptance and detection together.

**Policy C-b applied**: detect `fq_occ_c > FQD`, a state violation measured by
the extended pointer difference, derived without reference to `f_ready_o`. It is
the synthesizable counterpart of assertion `a_fq_in_range`. Expander now 12/12.

**Not demonstrated, and I will not claim it**: a firing trace. The mutation that
would produce one also indexes past the 4-entry array, corrupting the queue and
hanging the test. Establishing that the condition does not reduce to false is
the negative the brief demanded; a positive trace needs a fault-injection input
and remains open. On a monitor whose original defect was being reported as
working while constant zero, claiming an unheld demonstration would repeat the
defect one level up.

**The brief independently reaches my palette finding.** Its §4 is "separate
palette verdicts from statistics" — the `cq_rp -> cold_o` family I had flagged
as co-equal. It also reports the headline Fmax is optimistic: second slow corner
**81.58 MHz**, worst multicorner hold slack **-4.346 ns**.

**Experiment P-CNT implemented** (source only, not yet built): the palette's
three diagnostic counters now consume `l1_v_q`/`l1_stale_q`/`l1_res_q` — the
verdict already registered one line above — instead of the live classification,
taking a 32-bit increment off the end of that cone. The RAM read, `lu_valid_o`,
lookup data and accepted-BEGIN forwarding are untouched. The statistics now
trail by one cycle, declared here rather than discovered later.

## And I wedged the fast suite a SECOND time, the same way

`cmake --build` against the build tree while a ctest run was in flight. ctest CPU
frozen at 0, no test process. **Never touch the build tree while ctest is
running** — twice now. Restarted alone; P-CNT is deliberately unbuilt until it
finishes.

## Packet D may already be built — `zhao_raster_rcp24_v3` is instantiated nowhere

Chasing the brief's §6 turned up that the preparation split it specifies
**already exists**, is fitted, is registered in `prod_manifest.yml`, and is
wired into nothing. Docket M3's pattern repeating for the reciprocal service.

* brief's N0: *"reserve a context at acceptance and capture raw denominator,
  token, and context index"* -> `rcp24_v3.sv:457-461` does exactly that, from a
  free-context FIFO rather than a scan.
* its normalize works from the **registered** `a0_d_q` (:276-278). The
  instantiated `rcp24_svc` does the same search and shift **combinationally from
  the input pin** (:118-120) — that is the 15.416 ns cone.
* it also replaces the three context-wide scans with queues, which is the
  structure behind the −3.243 ns `c_val` round-robin its own comments record.
* leaf fits: `svc` −4.607 / 16.44 ns data delay; `v3@v3-full` −1.045 / 13.733.

**Written up with what it does NOT establish**, because "the work is already
done" is the comfortable reading: the leaf fits are not like-for-like (16 vs 20
ports, 8 vs 16 contexts), neither predicts the composed island, four ports
differ, 16 contexts plus queues may INCREASE area on an island already 13,133
against 7,500, and the block has never been instantiated — which is where every
defect this session came from.

Narrowed two caveats: it **elaborates at TOKW=14** (0 lint diagnostics — that is
elaboration only, not behaviour), and **no rcp/perspuv pair fixture exists**, so
the cheap measurement needs one written.

Recommended as an **owner decision**: reframe packet D as "evaluate integrating
rcp24_v3", and measure it with a pair fixture before spending a 4-hour composed
fit. Writing a second preparation pipeline when a fitted one exists would be the
more expensive error; swapping on two non-comparable leaf numbers would be the
other one.

## The fast-suite wedge: root cause, after three occurrences

**`build/Testing/Temporary/` held 58 files** — a `CTestCheckpoint.txt` and
dozens of orphan `LastTest.log.tmp*` — left behind by ctest runs I killed. The
next ctest blocked **at startup** on that state: the live process had started at
11:55:49 and accumulated **0.56 CPU seconds**, which is not a suite running
slowly, it is a process that never began.

Removing the checkpoint and the orphan temp files fixed it immediately: two test
processes within seconds.

**The chain of causation is mine and worth stating plainly.** The first wedge
came from running concurrent `ctest` invocations against one build tree. Killing
those left the temp state. Every subsequent "clean" restart then inherited it —
so my fix for the first mistake manufactured the second and third, and I
diagnosed each as a fresh mystery.

Two of the three wedges I attributed to *"I built while ctest was running"*.
That was true for one of them at most. The evidence I acted on — no test
process, frozen CPU — was consistent with both explanations, and I picked the
one I had already told myself.

**The rule that actually holds:** one ctest at a time per build tree, and if one
is killed, clear `build/Testing/Temporary/` before the next. The first half I
had; the second half is what cost three restarts.

## POSITION BEFORE READING THE P-CNT PALETTE FIT

In hand: packet D qualified downward (2.1 ns not 3.5); fast suite running clean
after the temp-file fix. **Next step regardless of this result:** verify P-CNT
functionally — it has been fitted but never simulated, because the suite has
held the build tree all afternoon.

Caveat to apply when reading: the 540 ALM / 628 reg / 98.06 MHz baseline has
**no provenance digest**, so it cannot be tied to the pre-P-CNT file.

## P-CNT MEASURED, and the endpoint moved

| | baseline | P-CNT | |
|---|---|---|---|
| ALM | 540 | **542** | +2 |
| registers | 628 | **648** | +20 |
| Fmax | 98.06 | **104.08** | **+6.02** |

The leaf crosses 100 MHz. But the **path** evidence is what makes it causal:

| | worst path | slack |
|---|---|---|
| before | `gen_r[3][0] -> cold_o[4]~reg0` | **−0.198** |
| after | `gen_r[1][6] -> l1_stale_q` | **+0.392** |

**The old gate ended at `cold_o`** — the counter P-CNT removed from the live
cone. The new gate ends at `l1_stale_q`, the registered verdict P-CNT made the
terminus. The endpoint the experiment targeted stopped being the endpoint.

Under the OLD M6 rule this would have been rejected for changing families. The
brief's amendment is what lets it count, and it is right: a successful repair
should change which path is worst.

It also settled the missing-digest caveat **structurally**: the previous row's
gating endpoint is `cold_o`, which the current source no longer has on that
cone, so that row can only describe the pre-P-CNT design.
`worst_path_index.py`'s `previous` field — added this morning because the next
fit overwrites the report — decided it on first use.

Caveats held: leaf fit, and the island cone starts at `rsp_dispatch|cq_rp`
outside this block, so **+6.02 does not convert to island Fmax**; recorded leaf
seed noise is ~4.70 MHz, so the Fmax delta alone would be weak; and **P-CNT is
fitted but not yet simulated** — the counters now trail a cycle and that needs
functional confirmation. A fit is not a correctness result.

## Next fit launched: rcp24_v3 at the ISLAND'S profile

`-TopParameters NCTX=8,TOKW=14 -RowLabel island-profile`. The existing v3 rows
are at NCTX=16, which is not the configuration a swap would use, and I said the
leaf rows were not comparable — this makes one that is. `run_block_fit.ps1`
warns that Quartus silently ignores directives, so **verify the parameters took**
before believing the row: at NCTX=8 the context storage must be visibly smaller
than the NCTX=16 rows, or the override was dropped.

## The fast-suite wedge was MINE, introduced this morning

Fourth wedge, and this time the temp directory was clean and tests had already
run. `LastTest.log` ends at **`island_v3_paired`** — the gate 3 test I added to
ctest a few hours ago.

Run alone it passes in **0.77 s**. The defect is what I attached to it:

```cmake
DEPENDS "island_composed_directed;island_v3_composed_directed"
```

**`gate3_paired.py` runs both executables itself** — that is the entire design,
so the comparison cannot drift from a separate harness. It needs them BUILT,
which the target dependency already guarantees, not RUN. The DEPENDS was
redundant, and under `-j2` it wedged the suite at that test every time.

Removed. The suite now runs past it.

### Three wrong diagnoses before the right one

1. *"I built while ctest was running."* True once, and the cause of wedge one.
2. *"Stale `CTestCheckpoint.txt` and 58 orphan temp files."* Real, and the cause
   of wedges two and three — but debris from killing wedge one, so still
   downstream of my own doing.
3. *"The environment cannot run the full suite."* I was one step from writing
   that down as a limitation of the machine.

The actual cause was a line I wrote this morning. Each explanation fit the
evidence I had — no test process, frozen ctest CPU — and each was reached
without the one cheap check that would have separated them: **run the suspect
test alone.** That took eight seconds when I finally did it.

This is the third time today the pattern has repeated (zero-work stall, combine
counter, this). In every case the RTL findings that survived came from
differential tests; every wrong call came from reasoning about symptoms.

## The packet D answer was already in the ledger

Two rows I had not looked for:

* **`@v3-nctx8`, registers 1,414** against `@v3-full`'s 1,944. NCTX=8 maps and
  the parameter visibly took. So the block never rejected NCTX=8 — my
  `NCTX=8,TOKW=14` failure implicates the **two-parameter** path. I was writing
  up a fit to discover something already recorded.
* **The complete head-to-head:**

| block | ALM | reg | DSP | Fmax |
|---|---|---|---|---|
| `rcp24_svc` — in both islands | 1041 | 1101 | **6** | **68.46** |
| `rcp24_v3@v3-rh` | **1023** | 1460 | **3** | **90.41** |

**18 ALM smaller, half the DSP, +21.95 MHz.** My caveat that 16 contexts plus
queues would probably increase area was **wrong** for this variant.

**The DSP figure touches a live rule failure.** The island fits 17 against
`max_dsp: 14`. RCP 6 -> 3 gives 17 − 3 = **14**, exactly at the rule. Arithmetic
on leaf numbers, not a measured island result — M7 is precisely about DSP
inference not being a multiply count — but a falsifiable prediction worth one
composed fit.

**The lesson for me:** I spent two rounds reasoning about what a swap might cost
and launched a fit to measure it, while the rows that answered it sat in
`zhao_block_fit.json`. Reading the ledger is cheaper than adding to it.

## POSITION BEFORE READING THE TOKW=14 FIT

In hand: packet D head-to-head settled from existing rows (v3-rh is 18 ALM
smaller, half the DSP, +21.95 MHz vs svc). **Next regardless of this result:**
P-CNT functional verification, still blocked on the build tree.

This fit answers one thing only — whether v3 holds up at the island's actual
token width, which is the one parameter a swap definitely needs.

## Fast suite discharged: 455 tests, 449 pass, 5 fail, ONE mine

Ownership established from git rather than assumed:

* **`ledger_check`** — 4 V20 errors. **One is mine** (`island_v3_top:2720`, a
  file I created): an invariant claim with no machine-resolvable enforcer.
  Fixed by naming `zhao_texture_v3own.sv:a_out_in_order`, which exists at
  `v3own.sv:2022`. **4 -> 3.** The other three are in `v3own` and `v3rq`, files
  **no commit of mine touches**; line 1677 last changed `384c3e77` on 09-07.
* **`format_check`** — clang-format drift in `zref_fragment.hpp` (09-07) and
  `zref_island.hpp` (09-06). No commit of mine touches `reference/` today.
* **`zcap_roundtrip` / `golden_abi_info` / `abi_golden`** — goldens last changed
  `5cd55827`, 09-07.

**The texture work broke nothing.** 449 pass around it.

Two of those messages read **"expected 0x1E7, got 0x1E7"** and **"expected 0x20,
got 0x20"** — equal values reported as mismatches. Not mine, not chased, but
flagged: a failure message showing two identical numbers costs someone an
afternoon later.

Also removed `captures/failures/` from tracking — a failing test's OUTPUT that
`git add -A` swept into a commit. Same reasoning as `*.rgb`.

### A diagnostic error of mine, corrected

I twice called the suite "stalled" while `cppcheck` was running at 300+ CPU
seconds. **My process filter matched `test_*` and never matched `cppcheck`** —
"0 procs" meant "none of the ones I looked for", not "nothing running". The
suite was fine both times.

## Seed check launched on P-CNT

`-Seed 7 -RowLabel @pcnt-seed7`, same source. P-CNT measured **+6.02 MHz**
against a docketed leaf seed noise of **~4.70 MHz** — close enough that the
number alone is weak, which I said when reporting it. A second seed point on the
SAME design says whether 104.08 is representative or a lucky placement.

The endpoint migration (`cold_o` -> `l1_stale_q`) is structural and does not
depend on this. But if seed 7 lands near 98, the **Fmax** claim needs
withdrawing even though the path claim stands.

## P-CNT CLOSED, and its Fmax claim withdrawn

**Functionally verified at last:** both composed tops pass **119/119** with P-CNT
in place, including the palette lookup/stale/cold total assertions
(`palette +96` in phase 5). **15/15** across the texture gate set.

**And the Fmax claim is withdrawn.** Seed 7 on identical RTL:

| | Fmax |
|---|---|
| baseline (pre-P-CNT) | 98.06 |
| P-CNT, default seed | 104.08 |
| P-CNT, **seed 7** | **90.74** |

**13.34 MHz between two placements of the same source.** My +6.02 MHz is inside
that, and seed 7 lands 7.32 MHz BELOW the baseline I claimed an improvement on.

What stands is the endpoint migration — `cold_o` ceased to be the gating
endpoint, `l1_stale_q` became it. Structural, not placement. But **"the endpoint
moved" and "the block got faster" are different claims and I merged them.**

The docketed leaf seed noise of ~4.70 MHz is also too small for this block: it
came from a different block and I used it as though it bounded this one. When I
reported +6.02 as "above 4.70 but not enormously", the right reading was that
the comparison was **unfounded**, not marginal.

## Three never-run tests now run, all pass

`texture_v3_window_identity` 35, `texture_v3rq_probe_sanity` 2,
`raster_ticketq_rh_directed` 22. Plus `metajoin_directed` **7** — packet C's new
bank survives the falsifier written specifically to catch the off-by-one I made
in it.

The claim "the texture work broke nothing" is now supported across the three
that were previously only "Not Run".

## Manifest gate fixed

Four blocks I added were UNACCOUNTED. Declared with reason codes —
`island_v3_top: probe` (composition, mirroring `island_top`), `frag_expand:
unused`, `metajoin: unused`, `ident_probe: probe`. **manifest check OK.**

## Packet C: three readers moved, two attempted and reverted

**Moved and verified** — all sourced from `disp_clut_meta`, the one queue the
alignment falsifier actually validated:

* `lu_slot_i` / `lu_gen_i` — the palette binding pair, the "64-owner palette
  binding selection" on the island's worst INTERNAL path
  (`rsp_dispatch|cq_rp -> palette_res|cold_o`, −2.093 ns)
* `clut_meta_c` — the CLUT sample metadata

Three asynchronous response-side selections replaced by registered queue
payload. Gate 2 **122 checks**, gate 3 **392 byte-identical**, shadow 1,176/0,
alignment 792/0.

**Attempted and REVERTED**: the bilinear and nearest `sampmeta_m` readers.
Gate 2 failed immediately — *ARGB4444/bilinear alpha wrong on 3 fragments*.

### The mistake, stated plainly

**My alignment falsifier validated the CLUT queue only.** It compares
`disp_clut_meta`'s palette fields against the live tables at `disp_clut_tok`:
792 responses, zero disagreements. That is evidence about **one class queue**.

I then moved the bilinear and nearest readers as though it covered them. It did
not. The bilinear lane sequences four channels, so a fragment's taps arrive as
several responses, and whether each carries its own correct metadata through
that queue is a different question — now an open one rather than an assumed one.

**Gate 2 caught it in one run**, which is the system working. But the evidence I
had and the change I made did not match, and I did not notice until the test
told me. Building a falsifier for one path and then acting on three is the same
error as reading a `status` field and not its `ruleViolations`.

### What that means for the remaining two readers

They need their own alignment falsifiers — per class queue, not one for CLUT —
before moving. The brief's credited-reservation design would make the property
structural instead of per-queue-incidental, which is the better fix and the one
it actually asks for.

## Packet C integrated; the milestone fit is running

**Four of five asynchronous response-side reads are on the class queue**, each
moved only after a falsifier for ITS OWN queue passed:

| queue | checked | wrong | reader |
|---|---|---|---|
| CLUT | 792 | 0 | moved — palette binding pair + `clut_meta_c` |
| nearest | 192 | 0 | moved |
| bilinear | 768 | **32** | **held** |

The palette binding pair is the one that matters: it is the "64-owner palette
binding selection" on the island's worst INTERNAL path
(`rsp_dispatch|cq_rp -> palette_res|cold_o`, −2.093 ns), and it is now a
registered queue payload rather than an asynchronous 64-entry selection.

### The bilinear disagreement, characterised rather than guessed

Only the FRACTIONS differ (`fv 50/fu 80` queued vs `fv 10/fu 30` in the table);
nibble, format and byte-select agree. Three hypotheses eliminated by
measurement:

* **not slot recycling** — the bank's generation check reports **0** mismatches
  across the suite. That was my first hypothesis and the counter refutes it.
* **not the bank** — the shadow shows bank and tables agree at the common
  stream, 1,176/0.
* **not the queue mechanism** — nearest carries the same payload through the
  same dispatcher at 192/0.

What remains: the table changes between the common stream and the bilinear
queue's exit for the same slot/sidx/generation. Which value is correct for a tap
is SEMANTIC, and gate 2 answered it empirically — moving the reader broke three
ARGB4444 alphas, so the decode depends on the table's later value. That is what
the brief's credited-reservation design exists to settle.

### The milestone fit

Launched on the island with **P-CNT and packet C together**. Per the brief:
*"Compose independently validated changes at an explicit milestone ... label the
result as a combined change rather than inventing individual MHz
contributions."* So whatever it reports is the COMBINED effect of the palette
counter move and four readers leaving the asynchronous tables — **not
attributable to either alone**, and the P-CNT Fmax claim stays withdrawn
regardless of what this says.

Baseline to compare against: **13,133 ALM / 20,561 reg / 45 M10K / 17 DSP /
82.41 MHz** (G1D 4.3h), digest `6094a4292eee`.

## The bilinear mystery is solved, and the defect was mine

`rsp_dispatch` takes responses into a **raw FIFO**, then dispatches from it into
the per-class queues. Data and token are read from the FIFO. My packet C write
was:

```
if (dispatch_fire) cq_m[head_cls][cq_wp[head_cls]] <= rsp_meta_i;   // WRONG
```

`rsp_meta_i` is the **current input**; the response being dispatched entered the
FIFO earlier. **Data and token travelled the FIFO; metadata did not.**

### It explains every measurement rather than contradicting one

When the raw FIFO is empty the dispatched response IS the one at the input, so
the metadata is accidentally right. Hence CLUT 792/0, nearest 192/0, gate 2 122,
gate 3 byte-identical — and bilinear **768/32**, because four-channel sequencing
is what puts several responses in flight. **4.2% is the fraction of dispatches
with a non-empty FIFO.**

Not slot recycling (generation check 0), not the bank (shadow 1,176/0), not the
queue mechanism. The enqueue SOURCE.

### Where the tests were

The composed suite hit this path 1,752 times over three queues and called two of
them clean, because those two rarely see a busy FIFO. **A leaf test with four
evenly-loaded lanes found it in one run**, 239 of 240 wrong. That is the case
for leaf tests, made concretely rather than as a principle.

The leaf test cost two false starts of its own first: a harness ordering bug,
and `TOKW` left at the default 16 against the island's 18, which truncated
tokens into 180 mismatches that were mine. Both found by reading the failure
instead of believing it — the same discipline that killed three hypotheses about
the bilinear path.

### Not fixed yet, and why

The fix is a `raw_m` array beside `raw_d`/`raw_c`, written at input acceptance
and read at `raw_rp` on dispatch. **`zhao_texture_rsp_dispatch` is inside the
running fit's closure**, and editing a file under a running fit is the live-tree
trap. It goes in when the fit lands.

### What the running fit is therefore measuring

A design whose metadata payload has a misaligned SOURCE. The registers and queue
entries are identical either way, so ALM/register/M10K and Fmax remain a fair
reading of packet C's cost. Written down so the receipt is not later quoted as
having measured a correct design.

## The raw-FIFO fix is written and staged, not applied

Prepared as a patch script (scratchpad `rawfifo_fix.py`), verified against the
real push site — `raw_d`/`raw_t`/`raw_c` written together under `if (psh)` — so
it mirrors `raw_t` exactly:

1. `logic [METAW-1:0] raw_m [RAWN];` beside `raw_c`
2. `if (META_EN) raw_m[raw_wp] <= rsp_meta_i;` with the other three pushes
3. dispatch reads `raw_m[raw_rp]` instead of `rsp_meta_i`

**Not applied**: `zhao_texture_rsp_dispatch` is in the running island fit's
closure. Applying it means the fit is measuring one design while the tree holds
another, which is the live-tree trap the hook exists to prevent.

### The acceptance test already exists and currently FAILS

`rsp_dispatch_meta_directed` reports **239 of 240 metadata words wrong** against
today's RTL. When the fix lands it should read 0, and the composed bilinear
falsifier should fall from 32/768 to 0 — at which point the fifth reader can
move and packet C's five-to-one port reduction is complete.

That is a falsifier written before the fix, failing for the right reason, with a
predicted post-fix value. The ordering is deliberate: it is the only way to know
the test can see the thing it is testing.

## POSITION BEFORE READING THE PACKET C MILESTONE FIT

**Next step regardless of the number:** apply the staged raw-FIFO fix (validated
on a copy, lints clean at META_EN 0 and 1), rebuild, and check three predicted
values — `rsp_dispatch_meta_directed` 239/240 -> 0, composed bilinear 32/768 ->
0, gate 3 unchanged at 392.

**How this result must be read:** it is P-CNT and packet C measured TOGETHER,
per the brief's instruction not to invent individual contributions. And it
measures a design with a known misaligned metadata SOURCE — same registers and
queue entries either way, so area and Fmax are fair, but the receipt is not a
measurement of a correct design.

Baseline: 13,133 ALM / 20,561 reg / 45 M10K / 17 DSP / 82.41 MHz, digest
`6094a4292eee`. This fit's digest is `b8f0ff0007ce` over 19 files.

## PACKET C COMPLETE — and it took a defect that could not be fixed alone

All five asynchronous response-side reads are on the class queue. Gate 2 **122**,
gate 3 **392 byte-identical**, and every falsifier reads zero:

| falsifier | result |
|---|---|
| bank vs tables at the common stream | 1,176 / **0** |
| CLUT queue alignment | 792 / **0** |
| nearest queue | 192 / **0** |
| bilinear queue | 768 / **0** (was 32, then 256) |
| dispatcher leaf | **5/5** (was 239/240 wrong) |

### The sequence, because each step needed the one before it

1. bank built, leaf-tested, shadow-verified in situ
2. dispatcher widened; a falsifier per class queue
3. readers moved on CLUT-only evidence -> **gate 2 caught it** -> per-queue
   falsifiers -> bilinear localised to 32/768
4. a leaf test on the dispatcher found the raw-FIFO defect: metadata captured
   from the current input rather than the FIFO entry being dispatched,
   **239 of 240 wrong**
5. fixing it made the composed island **WORSE** — 32 -> 256. **Two errors were
   cancelling**: the dispatcher's late capture partly compensated for the bank
   answering one cycle after its read was launched
6. the **credited read join** removed both causes at once

### What the credited join actually is

* a response is accepted from the cache **only when the join stage can hand on
  what it holds** — that is the reservation, and it is the part the brief warns
  cannot be deferred
* the bank read fires **only on an accepted beat**, so it never answers for a
  response the dispatcher cannot take
* the delayed response and the bank's answer *for that response* reach the
  dispatcher together

### The receipt is now stale in the direction that matters

The milestone fit measured **+2,778 ALM / −8.43 MHz** — on a design where the
metadata was misaligned AND the readers were still on the tables. It priced the
cost of the join without any of its benefit. **Re-fit launched** as
`@pktC-fixed`; that number decides whether packet C stays.

## I rebuilt a tool the repository already had

`tools/quartus/check_fit_rules.ps1` recomputes every fit verdict from the
recorded numbers against `design/fit_targets.yml`, ignoring the `status` field
entirely, and marks rows whose source has changed since the fit:

```
10 pass, 17 FAIL, 3 unmeasured, 9 STALE
```

That is strictly better than the numeric comparison I added to
`check_fit_ledger.py` an hour earlier, and **`fit_rules.ps1` names it in its own
header** — *"check_fit_rules.ps1 applies the identical law to already-recorded
rows"* — one grep from the file I was editing.

The numeric half is retired. What stays is the one thing the PowerShell tool
does not do: flag a row that is **self-contradictory on its face** — a non-empty
`ruleViolations` list beside a `status` of `ok`, which is the signature of a
hand-assembled row. `zhao_texture_material_combine_v1` was exactly that, one row
in 114.

**The pattern is the same one as reading the ledger before adding to it.** Twice
today I generated work to discover something already recorded: the `@v3-nctx8`
row that answered the NCTX question, and now a rules checker. Both times the
existing artefact was one search away.

### And the real verdicts, which correct my reporting

`check_fit_rules.ps1` says **17 FAIL**, including the V3 island (registers
20,561 > 9,000; ALM 13,133 > 7,500; DSP 17 > 14) and `combine_v1` (ALM 1,663 >
800; registers 1,269 > 500). It also marks **9 rows STALE**, the V3 island and
v3own among them — *"their verdicts describe an older block; refit before
believing either half."*

So when I described fits as `ok` today, the authority in this repository already
disagreed. Its closing line is the standing correction: **"A fit that meets Fmax
while violating its memory/DSP structure is not a pass."**

## The staleness ledger, and one debt it confirms

`check_fit_rules.ps1` marks **9 rows STALE** — measured from a source that has
since changed:

| row | commits since |
|---|---|
| `zhao_texture_island_v3_top` | 6 (re-fitting now) |
| `zhao_texture_v3own` | 6 |
| `zhao_texture_fragrob` | 5 |
| `zhao_texture_frag_expand` | **3** |
| `zhao_texture_island_top` | 2 (the oracle, from the tie-off) |
| plus rcp24_svc, texjoin_v2, tilestore, terrain_loadq | 1 each |

**The expander row is the debt I declared myself this morning** and never
cleared: its 323 ALM / 451 reg / +1.623 ns figures were measured at 05:21, and
the `f_ctx_i` port plus the 64-bit ctx queue entry landed at 05:48, with policy
C-b's monitor after that. I wrote then that the number "describes a block that
no longer exists". The tool now says the same thing independently, which is a
useful check on whether that kind of note actually gets acted on. It did not,
for eight hours.

**Re-fit queued** behind the island, so the toolchain does not idle between
them.

`zhao_texture_v3own` at 6 commits is worth noting precisely: **I have never
edited that file** — gate 1 depends on it being untouched, and `git log`
confirms no commit of mine names it. The staleness count therefore tracks
commits to the fit's CLOSURE, not to the block. That distinction matters before
anyone reads "6 commits" as "the owner block changed six times".

## Conservation across the credited join: 1,176 in, 1,176 out

Gate 2 is now **124 checks**. The join delays without losing, which is the
property that matters — a dropped response is not a wrong colour, it is a
fragment that never retires and an owner that never frees, surfacing as a hang
far from its cause.

**My first version of the check was wrong.** I compared
`cnt_cache_hits_o + cnt_cache_misses_o` (2,144) against dispatched (1,176) and
reported a leak that does not exist. Those count texel LOOKUPS — a bilinear
fetch is four lookups in one response.

The tell was visible: 1,176 dispatched exactly equals the shadow's comparison
count, and the shadow counts accepted responses. The join's real endpoints are
the bank read and the dispatcher accept.

**Third instance today of the same error**: a number from an adjacent
measurement standing in for the one actually needed —

* the packet D leaf comparison (different NCTX, different port counts)
* the M1 noise floor (measured on a different block)
* this conservation check (lookups, not responses)

Caught quickly this time only because the ratio was suspiciously near 2:1.

## The dispatcher leaf now stalls its lanes

`rsp_dispatch_meta_directed` tied all four `*_ready_i` high, so a queued record
was always read out on the cycle it became valid. A metadata word that failed to
HOLD beside its response through a stall would not have shown.

It matters here specifically: `cq_m` is indexed by the same read pointer as
`cq_d`/`cq_t`, and if it were ever indexed by anything else, **only a stalled
lane would reveal it**. With randomised backpressure on all four lanes the test
still reports 240 dispatched, 60 per lane, every word beside its own response.

Same lesson as gate 2's credit phase and the non-vacuity checks: a test whose
stimulus never applies pressure measures the easy case and reports it as the
general one.

## I killed a healthy test run, on a heuristic I had already seen fail

Diagnosed the `-L fast` suite as wedged from "ctest CPU frozen, 0 child
processes" and stopped it. **It was progressing normally**: the task output shows
`9/457` complete, with shell path tests legitimately taking 250-514 seconds
each, and `shell_duo_markers_fast` just started.

**Second time today the same heuristic lied.** Earlier I called the suite stalled
while `cppcheck` burned 300+ CPU seconds — my filter matched `test_*` and never
matched `cppcheck`. Here the gap was between-test timing and a filter that again
did not match what was running.

**The reliable signal was in the task output the whole time**: `N/457 Test #...
Passed`. A process count is an inference about liveness; the progress line is a
statement of it. I reached for the inference twice after it had already been
shown wrong once.

Restarted. Progress will be read from the log.

### Everything else verified clean while it runs

* undriven-output sweep over all five packet C files: **145 ports, all driven**
* `check_prod_manifest.py`: **204 modules, all counted or declared**
* `check_forbidden_sources.py`: no production closure names a fixture
* dispatcher leaf under randomised backpressure on all four lanes: **5/5**
