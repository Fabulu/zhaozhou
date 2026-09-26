# PATCHV2 — I34's LAST blocker is consumer-side, and the nav decision just shrank it

**Branch `gz/patchv2`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Read entry `I34` in `fpga/rtl/prod/zhao_console_core.sv`**
(`grep -n '^// I34\.'`, never a line number). It is long, it has been narrowed
five times, and **every earlier blocker on it is spent.** Read the EARTHADAPT
and FABRICSINK sections; they are measurements.

## WHAT THE ENTRY ACTUALLY MEASURES — the campaign had this wrong for a week

**`I34`'s register row is TERRAIN.PATCH's FIELD-HEIGHT LANE (`terr_pt_fld_*`)
and its section 9.1 list intake — an FPGA boundary tie-off.** NAVSERVICE
established this on 2026-09-26: **navigation was never what that row measured**,
and a week of discussion treating I34 as "the material and nav channels" was
talking about something adjacent to it.

## The state, which is unusually clean

**THE PRODUCER SIDE IS DONE.** `zhao_field_earth_adapter` is built and composed
as `u_field_earth_adapter`, client 3 of the one `u_field_host`. It produces
**ALL FOUR out-lanes from ONE evaluation** — `height_o`, `velocity_o`,
`material_o`, `nav_cost_o`, ordinals 0..3 of field-ir 7.1's earth out record.
Three uniforms `zhao_cmd_exec` had been publishing to nobody since 2026-09-21
are read, joined on the same handshake as the field list.

**THE CONSUMER SIDE IS THE WHOLE REMAINING GAP.** `zhao_terrain_patch` has an
input for **exactly one** of the four. The other three are **OPEN at the
instantiation and declared in the INCOMPLETE block rather than dropped**, so the
wires are already named for you.

**Every other recorded blocker is spent:** S1 (the uniforms) 2026-09-21, S2 (the
handle→hash map) by FIELDARM, S3 (the probes' promotion) 2026-09-20, the section
9.1 list intake by FIELDARM, and the evaluation itself by EARTHADAPT. **S5 is
the only one left and it has not moved an inch.**

## AND THE OWNER'S NAV DECISION CHANGED WHAT S5 MUST OWN

**This is the new information and it is why this packet is being run now.**
`reports/OWNER-DECISION-20260926-I34-NAV.md` — **read it** — moves navigation to
the CPU. So of the four lanes:

* **HEIGHT** — has an FPGA consumer today. The lane the register row names.
* **VELOCITY** — `efa_velocity` is out-lane 1 on a named wire, and it **already
  reaches `zhao_part_collide`'s relative-velocity term over fabric**. Real
  producer, real consumer, no SDRAM.
* **MATERIAL** — its fabric route is **live and composed for eight hops** and
  dies at `proj_out_*`, which is **entry I13, not a missing consumer**. It is
  **per-triangle** there. TERRAINTEX is working that seam right now — **do not
  duplicate it.**
* **NAV — NO LONGER NEEDS AN FPGA CONSUMER AT ALL.** The CPU owns it.
  `nav_cost_o` stays produced and is **classified, not deleted**; NAVSERVICE
  already recorded the classification with its reason in the port comment.

**So the entry's own prescription — *"what closes this entry is directive 13.2's
`zhao_terrain_patch_v2` owning four channels"* — is now questionable, and
settling it is your first job.** Does patch_v2 still need to own four, or the
ones that have live FPGA destinations? **Measure and decide under the
delegation; do not inherit the four-channel sentence just because it is
written down.**

**What you may NOT do**, and the entry quotes the directive for it: *"DO NOT
CLOSE I34 BY WIRING ONLY HEIGHT while declaring the other three channels present
because they have spare bus bits."* Height alone is not closure. **A channel is
present when it reaches a real owner, or when a recorded decision says its owner
is elsewhere — and nav is now the second kind, with a document to cite.**

## The blocks you are composing already exist

**`zhao_terrain_field_walk` and `zhao_terrain_patch_acc` are BUILT and both
`pending_compose`.** The entry's own words: those responsibilities *"are already
built and must be INSTANTIATED, not rewritten."* **Grep before you write
anything** — this tree's most repeated failure is building a second copy of
something it already has, and five packets found a false absence this week.

## THE MEASUREMENT THIS ENTRY HAS OWED FOR DAYS

**`fld_earth_stall_cycles_o` against the 10,416-clock allowance.** The entry
states the cost as arithmetic and has never measured it: `zhao_field_host`'s
front holds **one point in flight**, so a covered vertex-lane is order **80–100
clocks**, and a single field covering a whole 33×33 patch is order **10⁵ clocks
against a 10,416-clock allowance.**

**That number decides whether the FIELD-MAJOR machine has to be built before
terrain fields can run at frame rate.** It is the entry's own stated purpose for
exporting the counter.

**And it is currently unmeasurable by the smoke, which is the trap:** with no
TerrainField issued the list is empty, `fields_active_o` is 0, the lane is never
raised, and the adapter costs the frame nothing — **so the counter reads zero
for a reason that has nothing to do with the cost.** A zero here is the
broken-instrument shape. **Issue a real TerrainField and measure, or say plainly
that you could not and why.**

## The fences

* **Height alone is not closure.** See the directive quote above.
* **Do not delete any nav path.** Classify, cite the decision record, keep
  `FIELD.WRITE.NAV` and its behaviour.
* **Do not duplicate TERRAINTEX's work** on the material seam at `proj_out_*`.
  If you need that seam, say so and stop rather than racing it.
* **Do not build a frame-wide field-by-vertex matrix**, and **do not make
  another frame-sized flip-flop store** — the directive's words, and the device
  is at ~97%+ of its ALUT budget with no measured console figure yet.
* **Preserve the numerical policy**: command order, saturating operations,
  material as an **opaque full-width u32** (never narrowed to fit an older
  consumer), nav as command-ordered saturating Q16.16, presence travelling with
  the result — **an absent optional output is NO WRITE, not a write of zero**,
  and **material token zero is a real value when present.**
* **Preserve the exact initial/final height and underside clamp rules**, the
  closed footprint test, and dirty-border attribution. **Live Earth fields
  affect the TOP lattice; do not silently deform or recolour the underside.**
* **Do NOT start a console or full-device fit** — one is running. `-MapOnly` on
  a block is yours, **every map must name its `-Device`**, and **read
  `rtlCleanAtHead` before quoting ANY row.**

## Evidence bar

* **A pixel or a height that moves**, through composed production modules, on an
  acceptance bench of your own. `tests/prod/terrainaux_acceptance.cpp` is the
  pattern.
* **The stall measured**, or an honest statement of why it could not be.
* **Prove every counter you quote**, and **check that any control you add CAN
  FAIL** — SEALPLAN registered a `$fatal` guard as a build target this week and
  it returned RC=0 because `$fatal` fires at run.
* A guard unreachable with legal stimulus needs a **committed mutant** under
  `tests/mutants/`, renamed so no source list elaborates it, polarity inverted
  so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text.**

## Traps

* **THE GATES DO NOT BUILD.** Run `cmake --preset windows-native` yourself from
  PowerShell with `tools/env/zhao-env.ps1` sourced, and **read the BUILD's exit
  code, not a pipeline's.**
* **GATE 31: `tools/quartus/check_console_closure_lint.py`** — run after any
  port change. It caught 7 PINMISSING this week.
* **Three concurrent smoke forms is this box's ceiling.** Five gave
  `cc1plus: out of memory`, and one exited `verilator returned 3` **with no
  `%Error` line at all.**
* **`check_entry_claims` keys on PROSE** — reword rather than baseline.
* **`v3_closure_inherited.vlt` waives `UNUSEDSIGNAL` across five directories**,
  so a dead signal raises nothing there — **count occurrences by hand.**
* **`git checkout -- <file>` discards uncommitted work with no reflog.**
* **`[IO.File]` ignores `cd`** — absolute paths only.
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I34` closed.
2. **What patch_v2 owns, and WHY that set** — settled by measurement, not by
   inheriting the four-channel sentence.
3. **The stall measurement**, or why not.
4. **What you composed vs. what you found already built.**
5. **Every claim in this brief or the entry you found FALSE.** Every packet this
   week found at least one; most were mine.
6. **What you refused.**
7. **Anything you got wrong and caught yourself.**
8. Branch and commit hash. **Push `gz/patchv2` only.** Never `--force`.
