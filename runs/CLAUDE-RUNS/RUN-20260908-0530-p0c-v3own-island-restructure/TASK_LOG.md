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
