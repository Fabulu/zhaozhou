# TAGPHASE — clause 6, and it is the LAST THING IN THE CAMPAIGN

**Branch `gz/tagphase`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**THE REGISTER READS 1. `I34` IS THE ONLY ENTRY LEFT, AND FIVE OF ITS SIX
CLAUSES ARE MET.** `I55` closed — the console ships drawing all 2,816 of its
reference-derived pixels from SDRAM.

**READ FIRST, IN FULL:**

1. **`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`** — the owner's
   six clauses are the acceptance bar.
2. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-MATPUB.md` — your
   immediate predecessor. **It built the publisher and diagnosed what stops
   clause 6.**
3. `FINDINGS-LASTGAP.md`, and `FINDINGS-phasefix.md` for the shell.

## THE ONE THING LEFT

**With the publisher armed, the plain smoke exits 1 on the sample gate: 1216
against 1213.** That is **clause 6** — *"the no-field forms remain unchanged"* —
and it is the only clause not met.

**MATPUB did NOT disarm the gate to go green**, and that was right: disarming
would disconnect function and hide a defect the field forms were already hitting.
**Do not disarm it either.**

**It is NOT the field and NOT the material, and that is measured rather than
argued:**

* the publisher's **64 writes reproduce both symptoms in the PLAIN run, with no
  field at all**;
* and they **disappear** from `-FieldUncovered` — `samples=1216`,
  `tagged_beats=0`.

**It is phase sensitivity to SDRAM traffic.** The stray-tag pixels are **all
`rgb565=0000`** — **uncovered pixels whose colour cleared and whose tag byte did
not.** The repair lives in **`zhao_shell_top_v2`**, which MATPUB did not own.

**You own it.**

**And this is explicitly NOT the owner's bandwidth escalation** — the publisher
measures **0.15% of frame**. Do not present it as one.

## WHAT IS ALREADY TRUE — DO NOT REBUILD IT

| # | clause | evidence |
|---|---|---|
| 1 | installed and executed | `runs=1089 noprog=0 faults=0` |
| 2 | covers the terrain | `tp_covers=1` |
| 3 | value cannot coincide | `field_composed=1024 token_refused=0`, **zero silicon** |
| 4 | reaches the consumer | `tile[max/or]=[212 222]`, read off **the address the island issued**, `stray=0` |
| 5 | uncovered restores | `[6 7]`, the plain run's tiles exactly |
| 6 | **no-field forms unchanged** | **THIS ONE. 1216 → 1213.** |

**`TERRAIN.COMPOSED_MATERIAL` is BUILT AND LIVE**: `cells=1024 commits=1
published=1 bursts=64 denied=0`, ~900 ALM, **0 DSP**, 6 M10K.

**MATFIELD's two formerly-dead assertions now EXECUTE and PASS**, and each arm is
the others' fail-demonstration — 212 / 6 / 6. **Keep that property.**

## AND RUN WHAT MATPUB DECLARED IT DID NOT RUN

It named these rather than implying coverage. **They are yours:**

`-Mutant`, `-BadVertex`, `-NoEchoArm`, `-BadTraceArm`, `-TerrainFlatLattice`,
`test_cmd_exec_directed`, and the TypeScript suite.

**Gate 31 confirms no missing pin, but that is ELABORATION, not those forms.**

**Expect `-TerrainFlatLattice` to need thought**: UNPARK made it
arrangement-aware by adding the walk's **term** (`50 = 14 + 36`), and the
publisher may move its traffic again. **If it does, add a term — do not add a
number.**

## The fences

* **DO NOT DISARM THE SAMPLE GATE.** Fix the machine.
* **DO NOT REGRESS `I55`.** `paramwalk tris=101`, `fetcharm vread=303` with its
  **asserted** invariant, `tilewalk tiles=11 jobs=101`, `raster pixels=2816`. The
  console ships drawing from SDRAM; if your change moves any of those, **say so
  loudly.**
* **Do not regress the other five clauses**, and keep clauses 4 and 5 reading the
  **same instrument in opposite directions** — that is what makes them evidence.
* **`-FieldUncovered` exits 1 on `lane_desync_o`**, a counter measured to mean
  something other than its header says. **Do not silence it.**
* **Do not touch `zhao_geom_*`** beyond reading.
* **`gather_frag_untagged_o` means `tag[7:6]==0`, NOT `tag==0`** — that gate was
  **passing on a coincidence**. Do not build on its old meaning.
* **A latent defect goes live if you make a second slot live**:
  `zhao_field_progcache` sets `cm_slot_o` nonblocking on the fire cycle while the
  host reads it **on** that cycle, masked only because reset 0 == slot 0.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — 146,414 registers
  against 1,010.
* **No console or full-device fit.** That one is mine and it comes **after this
  entry closes.** Leaf `-MapOnly` only, device named.

## DO NOT WRITE THE HEAD-LINE DECLARATION

`NOT a tie-off` in `I34`'s **head line** is the one string that removes an entry
from the register's count. **Do not write it.** When the machine is true **I will
declare it, having measured it myself** — as I did for `I55`, where the
register's own guard caught **me** putting the phrase on the wrong line and
refused the commit.

## Evidence bar

* **Clause 6 met with the publisher ARMED**, by repair rather than by disarming.
* **All six clauses, each separately, in the positive direction.**
* **The declared-unrun forms RUN**, with verdicts.
* **Prove every counter you quote.** Cautionary, all from this entry:
  `fld_earth_noprog_o` **ORs two causes**; `err_unpublished_o` fires on
  publication **existence**, not on an empty publication; `gather_frag_untagged_o`
  tests **two bits**, not eight. **Ask what a counter sums before citing it.**
* **Check any control you add CAN FAIL.**
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE**, python's own exit code.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code.** A
  **PowerShell exception leaves `$LASTEXITCODE` STALE**; `RC=$?` after a pipeline
  reports the pipeline.
* **These scripts speak through `Write-Host`, so `2>&1` DROPS THEIR OUTPUT** —
  use `*>&1`.
* **A control form builds its OWN model**; a switch passed as a quoted string
  becomes **POSITIONAL** — splat, not through `powershell -File`.
* **EVERY NEW SWITCH NEEDS A TAG**, and `-WalkRaster` is an **appending
  modifier**, not an arm — an `elseif` cannot express a modifier.
* **Do not edit the bench mid-verilate** — four packets lost runs to this.
* **Hoist a diagnostic ABOVE the gate it explains.**
* **A new guard client must join `check_guard_verdict`'s CLIENTS list IN THE SAME
  CHANGE** — the gate refuses to skip an unregistered file, and it caught the
  publisher at the merge rather than in the packet.
* **RUN GATE 31** if you touch a port. An unconnected output is `.port_o ()`.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED**, `&` breaks a heredoc
  — use `-F <file>`.
* **Stop arming `until grep` poll loops**; **read CPU as a RATE.**

## Deliverable

1. **Clause 6 met with the publisher armed**, and how.
2. **All six clauses, each separately.**
3. **The declared-unrun forms, run, with verdicts.**
4. **`I55` unregressed**, with its four numbers.
5. **The register, measured BARE.** **Do not write the head-line declaration.**
6. **Every claim in this brief or the records you found FALSE.** **Ten
   consecutive packets found theirs wrong in a load-bearing place**, including
   one that would have produced a **false pass** on the owner's anti-vacuity
   clause. Hunt deliberately.
7. **What you refused, and anything you got wrong and caught yourself.**
8. **What the full console fit will need to know** — you are the last packet
   before it runs.
9. Branch and commit hash. **Push `gz/tagphase` only.** Never `--force`.
