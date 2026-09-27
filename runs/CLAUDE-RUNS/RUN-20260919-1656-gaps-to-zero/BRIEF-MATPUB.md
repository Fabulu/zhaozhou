# MATPUB — the publisher and one pixel. This is the LAST GAP IN THE CAMPAIGN.

**Branch `gz/matpub`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**THE REGISTER READS 1. `I34` IS THE ONLY ENTRY LEFT.** `I55` closed today — the
console draws all 2,816 of its reference-derived pixels from SDRAM.

**READ FIRST, IN FULL:**

1. **`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`** — the owner's
   ruling. **Your acceptance bar is his six clauses.**
2. **`reports/DECISION-20260927-I34-COMPOSED-MATERIAL-PUBLISHER.md`** — the tap,
   the payload and the cost are **decided**. Build it; do not re-open it.
3. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-LASTGAP.md` — your
   immediate predecessor. **It delivered the constant pool and met clause 3.**
4. `FINDINGS-MATFIELD.md`.

## FIVE CLAUSES ARE MET AND MEASURED

| # | clause | status |
|---|---|---|
| 1 | installed **and executed** | **MET** — `runs=1089 noprog=0 faults=0` |
| 2 | **covers the intended terrain** | **MET** — `tp_covers=1` |
| 3 | value **cannot equal the baseline by accident** | **MET** — `field_composed=1024 token_refused=0`, at **zero silicon** |
| 4 | reaches the **intended production consumer** | **MET** — `tile[max/or]=[212 222]`, 212 and 30 being exactly `SFF_MAT_A`/`SFF_MAT_B` against an authored plane topping out at **6**, read off **the address the island issued**, `stray=0` |
| 5 | uncovered **restores the authored result** | substance **MET** — uncovered gives `[6 7]`, the plain run's tiles exactly |
| 6 | **no-field forms unchanged** | **MET** — plain `PASS` |

**Clause 5 uses the SAME instrument as clause 4, in the opposite direction.**
That is what makes the pair evidence rather than two numbers. **Preserve that
property.**

## YOUR TWO JOBS

### 1. THE ONE-PIXEL TAG DEFECT

**The positive form stops on it.** LASTGAP **excluded six candidate causes by
measurement** and handed the remainder over as an **explicit hypothesis** rather
than a guess — read its findings for the six, so you do not re-exclude them.

**And it flagged `err_unpublished_o`'s zero as a claim it did NOT fire.** That is
the first thing to check: *a detector reading zero is a claim, and it is the
claim to check hardest.* **Fire it deliberately before quoting its silence.**

### 2. `TERRAIN.COMPOSED_MATERIAL` — BUILD THE PUBLISHER

**Decided and waiting**: `[0x058B_0000, 0x05AB_0000)`, **2 MiB as 256 × 8 KiB**,
tapping **`matjoin`'s composed write face**, with the **tagged** payload.
**`zhao_terrain_patch_acc` is ruled OUT** — wrong topology, checked and disposed
of, so do not re-scout it.

**COMPOSEPUB refused the SIBLING regions on two grounds and NEITHER carries
here** — this is measured, not argued by symmetry:

* **"No consumer"** — *"a DMA into unused memory is not a consumer."* **The
  fabric consumer is proven live end to end**: `field_composed=1024`,
  `token_refused=0`.
* **"Bandwidth"** — 124% of frame for the siblings' write alone. **Measured for
  material: 64 requests per published patch, break-even at 129 patches, and at
  this console's real dirty fraction 0.15% OF FRAME.** It fits.

## THE OWNER'S CONSTRAINTS

* **Do not silently cut field capacity, semantics, update behaviour, or the
  destination.**
* **Measure the actual cost, report it either way.** Targeted local measurement
  **authorised**; a full-console fit **not** — that one is mine, and it comes
  after this entry closes.
* **If the COMPLETED implementation proves the publication semantics cannot meet
  the frame/bandwidth contract, STOP and escalate with the MEASURED conflict.**
  Note that LASTGAP already measured this question in the *other* direction and
  it resolved toward building — **so an escalation now needs new measurement, not
  the old estimate.**
* **`I34` must become TRUE IN THE ASSEMBLED CONSOLE.**

## TWO ASSERTIONS THAT HAVE NEVER EXECUTED

MATFIELD's clause-3 and clause-4 consumer assertions are **committed and still
unreached**. LASTGAP measured the numbers they assert on, printed them above the
gate, and **refused to claim they passed** — its words: *"a number printed above
a gate is not an assertion executed."* **That refusal was right.**

**Your job is to make them EXECUTE, see them PASS, and show they CAN FAIL.** This
campaign shipped four never-executed assertions in one week; do not add two more.

## DO NOT WRITE THE HEAD-LINE DECLARATION

`NOT a tie-off` in `I34`'s **head line** is the one string that removes an entry
from the register's count. **Do not write it.** When the machine is true **I will
declare it, having measured it myself** — as I did for `I55`, where the
register's own guard caught **me** putting the phrase on the wrong line and
refused the commit.

## The fences

* **You are the only packet running.** TERRAIN, FIELD, the field stimulus and the
  publisher are yours. **Do not touch `zhao_geom_*` or the shell** beyond reading.
* **DO NOT REGRESS `I55`.** The console now **ships drawing from SDRAM**:
  `paramwalk tris=101`, `fetcharm vread=303` with its **asserted** invariant,
  `tilewalk tiles=11 jobs=101`, `raster pixels=2816`. If your change moves any of
  those, **say so loudly.**
* **Do not regress the plain smoke** — `pixels=2816`, `frames_admitted=1`,
  `fragments=1216`, `tile[max/or]=[6 7]`. **That is clause 6.**
* **Do not regress velocity** — `terrain_veljoin_directed` 19/0,
  `part_terrain_tap_directed` 1297/0, `composepub_acceptance` 154/0.
* **`-FieldUncovered` exits 1 on `lane_desync_o`** — a counter measured to mean
  something other than its header says. **Do not silence it.**
* **A latent defect goes live if you make a second slot live**:
  `zhao_field_progcache` sets `cm_slot_o` nonblocking on the fire cycle while the
  host reads it **on** that cycle — masked only because reset 0 == slot 0.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — 146,414 registers
  against 1,010. **The LOOP is the killer**; one flat array at a module's own
  scope (`zhao_dc_sdp_ram`) is the remedy.

## Evidence bar

* **All six clauses in the POSITIVE direction, each separately.**
* **The two assertions executed, passing, and shown able to fail.**
* **`err_unpublished_o` fired** before its silence is quoted.
* **The publisher's cost measured**, not inherited from the 0.15% figure — that
  was the *decision's* estimate of your build; **you have the build.**
* **Prove every counter you quote.** `fld_earth_noprog_o` **ORs two causes** and
  could never have identified the fault it was cited for. **Ask what a counter
  sums.**
* **Check any control you add CAN FAIL.**
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE**, python's own exit code.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code.** A
  **PowerShell exception leaves `$LASTEXITCODE` STALE**; `RC=$?` after a pipeline
  reports the pipeline.
* **These scripts speak through `Write-Host`, so `2>&1` DROPS THEIR OUTPUT** —
  use `*>&1`. A lint run wrote a **zero-byte log under RC=0**.
* **`.gitattributes` can silently reshape a generated fixture** — proved with a
  probe, 16 bytes becoming 14. **Check byte counts.**
* **Do not edit the bench mid-verilate** — three packets lost runs to this.
* **Hoist a diagnostic ABOVE the gate it explains** — LASTGAP put one on the
  wrong side and paid a full build.
* **RUN GATE 31** if you touch a port. An unconnected output is `.port_o ()`.
  A new core port costs **four** things: the port, **BOTH** wrapper mutants, the
  bench's wire, and a **reader**.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED**, and `&` breaks a
  heredoc — use `-F <file>`.
* **Stop arming `until grep` poll loops.** Use the task mechanism; **read CPU as
  a RATE.**

## Deliverable

1. **The six clauses, each answered separately, in the POSITIVE direction.**
2. **The one-pixel tag defect fixed**, with `err_unpublished_o` fired.
3. **`TERRAIN.COMPOSED_MATERIAL` built**, or the MEASURED conflict that stops it.
4. **The two assertions executed, passing, and shown able to fail.**
5. **The publisher's cost, measured on the built thing.**
6. **The register, measured BARE.** **Do not write the head-line declaration.**
7. **Every claim in this brief or the records you found FALSE.** **Nine
   consecutive packets found theirs wrong in a load-bearing place** — the last
   one found a claim of mine that would have produced a **false pass** on the
   owner's own anti-vacuity clause. Hunt deliberately.
8. **What you refused, and anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/matpub` only.** Never `--force`.
