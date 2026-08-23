# Run Archive

Completed runs are logged here (newest first). Working directories remain in
`runs/CLAUDE-RUNS/<RUN-ID>-<slug>/` indefinitely — never auto-deleted.

---

## Entry Template

```markdown
### [RUN-YYYYMMDD-HHMM] Brief Description

**Archived:** YYYY-MM-DD HH:MM EST
**Created:** YYYY-MM-DD HH:MM EST
**Completed:** YYYY-MM-DD HH:MM EST (optional)
**Duration:** ~X hours/minutes (optional)
**Working Directory:** `runs/CLAUDE-RUNS/<RUN-ID>-<slug>/`
**Branch:** branch-name (optional)

**Code Duplication:** X.XX% (optional — project-specific metric)

**Summary:**
[Brief description of what was accomplished]

**Deliverables:**
- [List of key files created/modified]

**Notes:** (optional)

**Outcome:** [Final result and any follow-up context]

---
```

---

### [RUN-20260823-1415] SURFACE.STAMP: the coverage geometry off the DSP farm, 28 -> 0 DSPs

**Archived:** 2026-08-23 UTC+02:00
**Created:** 2026-08-23 14:15 UTC+02:00
**Working Directory:** `runs/CLAUDE-RUNS/RUN-20260823-1415-surface-stamp-dsp-rearchitecture/`
**Branch:** main

**Summary:**
`zhao_surface_stamp` fitted at 28 DSP blocks on a 112-DSP device, against a
ledger target of "1 stamp texel per clock" that was a PLACEHOLDER rather than a
demand. Sacrifice's own SCAR system gives the real figure -- 128x128 god scars
over 64x64 land tiles, 9 tiles per impact, once per impact -- so the derived
demand is 20,000 stamp texels per frame, one texel per ~83 clocks. The block was
provisioned for one per clock.

All six multiplies were in the COVERAGE GEOMETRY; none in the blend, the
material conversion or the age/decay path, which the block's own contract had
already kept multiplier-free on purpose. Two of the six were per-stamp constants
that still got silicon, because they were written as combinational expressions
off the command port and nothing in the RTL said they were rare.

Rearchitected: the two texel-centre products become first-order accumulators
(exact mod 2^41 for EVERY input -- no domain argument owed), and the four
squares -- r*r, r_inner*r_inner, dx*dx, dz*dz -- share one sequential shift-add
squarer accumulating mod 2^64, which is exactly the truncation the shipped
product already performed. No `*` operator survives in any of the three files.
The datapath is NOT narrowed to the stated domain: the DSPs come out without
paying that risk.

**MEASURED, four constrained fits (Info 332111 captured for each):**
28 -> **0 DSP blocks**, and **Fmax 32.33 -> 87.54 MHz**. The second number is the
finding the run did not expect: the contract said the throughput target was "met,
measured", which was true about CYCLES and false about TIME -- the block was
holding the shared gpu_clk at 32% of its constraint. ALMs 947 -> 993, registers
496 -> 1,018. Throughput 538,045 -> 37,784 texels/frame, still 1.89x the demand.
The SQ_RADIX frontier: 1 / 2 / 4 give 87.54 / 87.44 / 82.37 MHz at 37,784 /
71,706 / 122,808 texels/frame, so radix 2 is nearly free on the clock -- and the
default stays at 1 anyway, because spending 36 ALMs to exceed a met demand is
the same error the 28 DSPs came from.

**Deliverables:**
- `fpga/rtl/surface/zhao_surface_sq.sv` -- new, the shared sequential squarer
- `fpga/rtl/surface/zhao_surface_stamp.sv` -- rearchitected, `SQ_RADIX` in {1,2,4}
- `tests/surface/surface_sq_directed.cpp` -- the squarer checked where its VALUE
  is visible, not where it has been reduced to a boolean
- `tests/surface/surface_stamp_directed.cpp` -- the odd-leg rim-exact case, and
  the throughput asserted against the DERIVED DEMAND rather than a cycle count
- `tests/surface/surface_dev.hpp` -- the hang guard derived once, not guessed
- `tests/CMakeLists.txt` -- the frontier BUILT at three settings, not argued
- `tools/sweep_surface_stamp.sh` + preflight + `..._consumers.py` -- 32 mutants,
  seven guards, a variable-aware guard 7, and `ZHAO_SWEEP_ONLY`
- `tools/quartus/run_block_fit.ps1` -- per-invocation workspace uniquifier
- `design/contracts/SURFACE.STAMP.md`, `design/blocks.yml`,
  `docs/OWNER_DOCKET.md`, `reports/REMAINING_BLOCKERS.md`

**Notes:**
Mutation sweep **32 attempted / 32 accounted / 32 caught / 0 discarded**, one
run end to end. The first complete run scored 29/32; the three survivors are
what produced the squarer's own suite, and both logs are kept because a
repository that keeps only the final number loses the reason the suite exists.
`ctest -L fast` 269/270, the one failure being the pre-existing V16 ledger
baseline that was red at run start.

The three survivors were not careless: `covered = !(d2 > r_outer2 || d2 < r_inner2)`
is SCALE-INVARIANT and all three terms come from the same squarer instance, so a
uniformly doubled or halved squarer is invisible from the stamp. Two of them
were a real gap -- a uniform halving that only holds while every magnitude is
even, and every operand the suite drove WAS even -- closed by a constructed
Pythagorean-triple rim case with an odd leg.

Three separate instances of build/harness state masquerading as design
behaviour, all logged: a hang guard written for the old one-texel-per-clock rate
that truncated every stamp and produced fifteen confident wrong failures; a fit
harness that keyed its workspace on $PID alone and silently destroyed the
evidence for two of three fits; and a background sweep whose wrapper the agent
harness killed while the process kept running, producing two false DISCARDED
lines. Every time, the guards refused to score the bad result.

**Outcome:** DSP target met with margin (0 against a 0-2 target), and the
console-wide census falls 188 -> 160. Fmax nearly tripled, which was not the
task but is the more valuable half. `texture_tmu` (28) and `terrain_normals`
(18) remain, and the docket's "three blocks with no demand figure" is now two.

---

### [RUN-20260823-0937] GEOM.SKIN: multiplier farm sized to the frame demand, 72 -> 9 DSPs

**Archived:** 2026-08-23 13:10 UTC+02:00
**Created:** 2026-08-23 09:37 UTC+02:00
**Working Directory:** `runs/CLAUDE-RUNS/RUN-20260823-0937-geom-skin-dsp-rearchitecture/`
**Branch:** main

**Summary:**
`zhao_geom_skin` fitted at 72 DSP blocks on a 112-DSP device -- 64% of the chip
for one stage. The owner's ~120,000 skinned vertices per 60 Hz frame gives 13.88
clocks per vertex against 18 products, so the honest multiplier count is 1.30
and the block had eighteen: over-provisioned 13.9x. Rearchitected as a
`MUL_LANES`-wide farm local to the block, with lanes bound to TERMS rather than
rows (which deletes the coordinate mux entirely) and the blend as one shared
shift-add unit whose walk overlaps the issue tail.

**Measured, both constrained fits:** 72 -> **9 DSP blocks** at MUL_LANES=3,
**3** at MUL_LANES=1. ALMs 1,801 -> 2,187 (they ROSE; the campaign's standing
"ALMs fell every time" claim does not extend to this block and the log says so).
Fmax **58.45 MHz**, which misses the block's own vertex demand and is the open
problem -- diagnosed to `br[1] -> o_y_o[14]~reg0`, 17.639 ns of data delay over
10 logic levels, all 200 worst paths in one endpoint family.

**Deliverables:**
- `fpga/rtl/geometry/zhao_geom_skin.sv` -- rearchitected, `MUL_LANES` in {1,3,6}
- `tests/geometry/geom_skin_directed.cpp` -- operand extremes, the rate as a
  law, and the oracle's int64 narrowing handled explicitly
- `tests/CMakeLists.txt` -- the frontier BUILT at all three points, not argued
- `tools/sweep_geom_skin.sh` + preflight -- 28 mutants, seven guards
- `tools/quartus/run_block_fit.ps1` -- `-TopParameters` / `-RowLabel` / `variantOf`
- `.gitattributes` -- `*.sh text eol=lf`, which fixed EVERY sweep in the repo
- `reports/QUARTUS_GOTCHAS.md` -- new section 8
- `docs/OWNER_DOCKET.md` -- the reference's int64 narrowing, three options, none taken
- `design/contracts/GEOM.SKIN.md`, `design/blocks.yml`

**Notes:**
Mutation sweep 28 attempted / 28 accounted / 26 caught / 2 equivalent, run in a
git worktree per the standing ruling -- which exposed that CRLF checkout had
silently broken every sweep in the repository. M27 was caught only by the
MUL_LANES=1 build, which is the concrete argument for building a frontier
rather than arguing it.

**Outcome:** DSP target met with margin. Fmax is the open item, diagnosed with
the fix specified (three-stage blend, II 10 -> 12, needs 86.4 MHz) but not
implemented.

---

<!-- Entries go above this line, newest first -->
