# SPEC v1: Reconcile the DSP census with the merged RTL, then GEOM.SKIN

**Run ID:** RUN-20260823-0934
**Created:** 2026-08-23 09:34 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

`reports/synthesis/zhao_block_fit.json` is the file the V23 census reads and the
file people act on. Success is that every row in it was written by the tool, at
a commit whose RTL is unchanged, and that the census total reflects the RTL that
is actually in main.

Then: `zhao_geom_skin` from **72 DSPs to 12-18**, measured, with the console
total moving toward the ruled policy of **<=85-90 DSPs, warning line >95**.

---

## Scope

**In Scope:**

- merging `wp/field-dsp` into main and pushing it
- restoring measurement rows that were destroyed by failed or concurrent fits
- re-fitting `zhao_field_seq` on HEAD so its row matches its RTL and gains a
  real Fmax from the new `quartus_sta` stage
- `zhao_geom_skin` rearchitecture, delegated, with two or three parameterised
  resource points measured rather than one architecture implemented
- `ctest -L fast` and the ledger check on merged main
- `STATUS.md`

**Out of Scope:**

- **any game behaviour for the particle-simulation, compositor or 2D blocks.**
  Those need Fabian's decisions and are recorded as owner-docket items.
- GEOM.CULL, SURFACE.STAMP, TMU, NORMALS, TERRAIN.PROJECT — queued behind
  GEOM.SKIN and handled serially
- Nanquan / compiler work. Hardware is the centrepiece and stays ahead.

---

## Constraints

- **Never hand-edit a measured number into the report.** Re-run the fit.
- **One constrained Quartus fit at a time.** Three concurrent fits exhaust this
  24 GB machine; that is how `zhao_geom_project`'s row was lost permanently.
- **Verify a fit was constrained** by finding `10.000  clk` in the log. Its
  absence silently invalidated the area and Fmax columns of 47 earlier rows.
- **Share arithmetic within a subsystem only, never console-global** (owner
  ruling). Smallest local multiplier farm per subsystem.
- **DSP allocation justified by sustained frame demand**, not by one-clock
  placeholder throughput. The ruled figure is ~120,000 skinned vertex instances
  per 60 Hz frame.
- One implementation agent at a time; parallel only for read-only recon.

## Don't Retry

- **Do not "prove" a regression by reverting a file and re-running.** Done once
  for `zhao_terrain_lod`; the revert forced a rebuild that also cleared mutant
  residue a sweep had left in unscored consumers, so the pass tracked the
  rebuild and not the RTL. A false regression was published and withdrawn.
- **Do not trust `(* multstyle = "logic" *)`.** Quartus 17.0.2 accepts it and
  does nothing, with no diagnostic. Write the multiply as a shift-add instead.
- **Do not carry "free" operand slack.** 72-bit operands cost 28 DSPs where the
  proven-sufficient 64-bit width cost 18.
- **Do not score a mutant that failed to compile as caught.** The executable
  lives outside the target directory, so a failed build re-runs the previous
  binary. That inflated a sweep from a real 22/23 to a reported 21/22.
- **Do not run the block fit with the shell's SDC.** It names
  `gpu_clk`/`vid_clk`/`audio_clk`; 63 of 71 clock ports are named `clk`.

## Open Questions

- Owner docket, still open: SDC clock-group cut; `starvation_o` on the worst
  path (-1.991 ns); the earth-field WRITE law; the meshlet descriptor format.
- Does `zhao_geom_skin` share the campaign's single cause (parallel multipliers
  outrunning the block's real rate), or is it genuinely throughput-bound? To be
  answered from the 120k-vertex budget before any RTL is written.
