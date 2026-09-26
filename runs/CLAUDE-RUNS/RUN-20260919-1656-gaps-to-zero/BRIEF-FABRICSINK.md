# FABRICSINK — I34's material and nav: find them a FABRIC consumer, or return a clean negative

**Branch `gz/fabricsink`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Your job may be to come back with a measured NO.** That is a real deliverable
here and it is stated first so you do not feel pushed toward a yes. Four packets
this week refused their entry with measurements and every one of those refusals
was worth more than a forced build.

## Read these, in this order

1. `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` **§1** — it decides I34, and
   it is the specification.
2. `reports/OWNER-ESCALATION-20260926-I34.md` **and its ADDENDUM-2**. The
   addendum strikes one of the escalation's two questions. Read it before
   inheriting anything from the escalation itself.
3. Entry `I34` in `fpga/rtl/prod/zhao_console_core.sv` (`grep -n '^// I34\.'`,
   never a line number).

## What is DECIDED and not yours to reopen

* **Material is an OPAQUE, FULL-WIDTH u32.** The directive's numerical policy
  says so and adds *"Keep the full material token; do not narrow it to fit an
  older consumer."* **Narrowing it to layer-E's `{u8 a, u8 b, u8 weight}` is
  prohibited.**
* **That triple is the SINK INPUT, not a rival encoding.** `ops.yml`
  `FIELD.WRITE.MATERIAL` reads *"2 candidate material IDs + blend weight per
  cell; **resolved deterministically by TERRAIN.PATCH**"* — it names the mapping
  in its own sentence. `field-ir.md` 7.1's `material:u32` is that resolution's
  output. **My escalation called these "two incompatible encodings with nothing
  mapping between them" and that was false.**
* **Nav cost is command-ordered saturating Q16.16 accumulation**, per the same
  policy.
* **Presence travels with the result.** Absent optional output is **no write**,
  not a write of zero. **Material token zero is a real value when present.** An
  output promised by the program but missing at retirement is **a fault**.
* **All four channels stay.** *"KEEP ALL FOUR CHANNELS AND GIVE THEM REAL
  HOMES."* Ruling material or nav out of the Earth record is **refused** and is
  not on your table — it deletes a feature, first on the list of what the
  delegation does not cover.

## WHY YOU ARE NOT BUILDING THE DIRECTIVE'S SDRAM REGIONS

The directive names destinations and exact addresses —
`COMPOSED_MATERIAL [0x058B0000, 0x05AB0000)` and
`COMPOSED_NAV [0x05AB0000, 0x05CB0000)`, 2 MiB each, 256 slots of 8 KiB — and
**the bus measurably cannot pay for them.** `tools/budget/sdram_bandwidth.py`:

| | SDRAM cycles | frame |
|---|---:|---|
| today | 330,474 **free** | 19.83% headroom |
| + composed VELOCITY publish | 406,806 **over** | 24.41% oversubscribed |
| + the fill-side read that makes it a consumer | 1,291,542 **over** | 77.49% oversubscribed |

**One 2 B/vertex plane costs 737,280 cycles against 330,474 free — 2.2× the
entire headroom on its own.** Material as u32 and nav as fx are each about twice
velocity's width. The directive's own rule covers this: *"A measured engineering
impossibility is a finding, not permission to invent a pass."*

**RE-MEASURE IT.** That table is mine and this campaign's briefs have a poor
record on confident numbers — including the one GIANTQUOTA refuted yesterday. If
the bandwidth picture is different from what I state, **that changes the packet**
and you should say so before building anything.

## THE JOB — walk the routes, do not assume them

**Velocity was in exactly this position a week ago** and closed **with no SDRAM
at all**: composed height already reached its consumers through fabric, so
velocity rode the same route to `zhao_terrain_heighttap`'s §4.3 cell and out to
the particle tap. Real consumer, number that moves, no region, no bandwidth.
**Read how velocity actually landed before you design anything** — copying a
proven route beats designing one, and GIANTQUOTA's finding yesterday was exactly
that about a floor that already shipped.

Then:

1. **MATERIAL** — the plausible consumer is the **mosaic path**.
   `zhao_texture_mosaic_v2` is composed and resident (`u_mosaic` at
   `zhao_texture_island_v3_top.sv:1303`). **Walk it. Does a resolved u32 material
   token have anywhere real to arrive?**
2. **NAV** — the plausible consumer is **SW.CPUCOLL's mirror**. The directive
   says CPUCOLL *"retains the canonical simulation mirror under the shared
   deterministic law"* and `ops.yml` calls nav *"Consumed on the FPGA side AND
   MIRRORED by SW.CPUCOLL"*. **Walk it.**

**I HAVE NOT WALKED EITHER AND I AM NOT ASSERTING THEY EXIST.** In this tree "X
does not exist" runs false at a rate near one in two — **and it runs false in
both directions.** The escalation you are correcting contains a false absence I
wrote. A false PRESENCE is worse, because nobody re-asks a thing already said to
be there, and CELLCARRY caught one of those this week too.

## The fences

* **Do NOT ship a computed-and-unread lane.** Option 1 — the accumulator owning
  material and nav as patch-local state with no consumer — **is not yours to
  adopt.** It is the false presence this campaign exists to refuse, and
  `completion_register.py` counts a disconnected implementation as a gap on
  purpose. If the hunt comes back empty, **report the negative** and I take the
  choice back to the owner with your measurement.
* **Do not narrow the material token.** Do not silently deform or recolour the
  underside; live Earth fields affect the **top lattice**.
* **Do not build a frame-wide field-by-vertex matrix**, and **do not make
  another frame-sized flip-flop store** — both are the directive's words.
* **Do NOT start a console or full-device fit.** `-MapOnly` on a block is yours,
  and **every map you quote must name its `-Device`.**

## Evidence bar

* **A number that MOVES at a real consumer**, through composed production
  modules, on an acceptance bench of your own.
  `tests/prod/terrainaux_acceptance.cpp` is the pattern.
* **Or a clean, specific negative**: which routes you walked, what terminates
  each, and what it would cost to finish one. **Name the files and lines.**
* **Presence semantics exercised**, not asserted: an absent optional output
  producing **no write**, and a material token of **zero** surviving as a real
  value. Those are two different tests and the second is the one that gets
  skipped.
* **Prove every counter you quote**, and **check what clocks the two sides of any
  comparison.** A guard unreachable with legal stimulus needs a **committed
  mutant** under `tests/mutants/`, renamed so no source list elaborates it,
  polarity inverted so it passes when the counter FIRES.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE after editing entry text** — it
  classifies by scanning prose, so editing an entry can relabel it.

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself from PowerShell
  with `tools/env/zhao-env.ps1` sourced, and **read the build's exit code, not
  the pipeline's** — `| tail` reports `tail`'s status, and a PowerShell
  *exception* leaves `$LASTEXITCODE` carrying the previous command's value.
* **`gate_sweep` does not run the console smoke controls.** Use `@splat`; a
  hashtable splat through `powershell -File` stringifies the switches so they
  never start. That has produced false GREENS and false REDS here on one day.
* **`[IO.File]` ignores `cd`** — pass it ABSOLUTE paths or your writes land in
  the coordinator's tree. The tell is a clean `git status` after a successful
  write.
* **Stage your HUNK on any shared file**, never `git add <file>`, and never
  `git checkout --` on one.
* **Regenerate `zhao_prod_top.sv` after ANY port change**; re-run
  `tools/quartus/check_prod_manifest.py`.
* **Check a file's committed mutant-copy count before your first edit.**
* **Verilator lint-clean is not Quartus-synthesizable.**
* **One `ctest` at a time per build tree.**

## Deliverable

1. **The register before and after, measured BARE**, and whether `I34` moved.
2. **The two routes, walked** — what you found at the end of each, with files and
   lines. **A measured NO is a full deliverable.**
3. **Whether my bandwidth table above still holds**, re-measured.
4. **Every claim in this brief, the entry, or the escalation you found FALSE.**
   Every packet this week found at least one; most were mine.
5. **What you refused.**
6. **Anything you got wrong and caught yourself.** GIANTQUOTA ran a sweep against
   its own draft finding and killed it; that is the standard here.
7. Branch and commit hash. **Push `gz/fabricsink` only.** Never `--force`.
