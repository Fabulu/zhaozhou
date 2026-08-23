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
