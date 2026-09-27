# MATFIELD — the field WRITES MATERIAL, and `COMPOSED_MATERIAL` gets built. This closes I34.

**Branch `gz/matfield`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**READ FIRST, IN FULL:**

1. **`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`** — the owner's
   ruling and **your acceptance bar**. Read the CORRECTION block too; it is mine.
2. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-NOPROG.md` — your
   immediate predecessor. **It made the console execute its first real field.**
3. `FINDINGS-FIELDACTIVE.md` and `FINDINGS-I34CLOSE.md`.

## WHERE IT STANDS — THE FIELD RUNS

```
fldearth runs=1089 noprog=0 faults=0 short=0      (was runs=0 noprog=1089)
```

**For ZERO added silicon** — no RTL, no port; both generated-file `--check`s
still fresh is the proof. The cause was two production guards pulling opposite
ways (`zhao_field_doorbell:429` wants the header **before** the commit;
`zhao_field_host_v2:1448` clears `hdr_loaded` **on** it), resolved by re-posting
the header, **no guard removed and no semantics cut.**

**The owner's six clauses, as they stand:**

| # | clause | status |
|---|---|---|
| 1 | real production Field program installed **and executed** | **MET** |
| 2 | the field **covers the intended terrain** | **MET** |
| 3 | material write produces a value that **cannot equal the authored baseline by accident** | **mechanism found, firing in the REFUSAL direction** |
| 4 | composed material **reaches the intended production consumer** | routing **MET** — 1,024 tag checks at the real consumer |
| 5 | **uncovered/control form restores the authored result** | substance **MET** — `pixels=2816` restored |
| 6 | **no-field forms unchanged** | **MET** — plain PASS |

## YOUR JOB — TWO THINGS, AND CLAUSE 3 IS THE POINT

1. **A MATERIAL-WRITING field program.** The executing program (`wave_pool`) is a
   height field. Clause 3's positive half needs a field that **writes material**,
   with a value **chosen so it cannot coincide with the authored baseline** — and
   **say why it cannot**, in the findings.
2. **Build `TERRAIN.COMPOSED_MATERIAL`.** The owner declined the refusal and
   commissioned it.

**Then answer all six clauses in the POSITIVE direction**, separately, in the
owner's own terms.

## SCOUTING — VERIFY, DO NOT INHERIT

* **The address is free**: `0x058B_0000` reserved, `DEVSTORE` ends exactly there.
* **`DEVSTORE` is the enactment pattern.**
* **matjoin's composed write face is the tap point.**
* **`zhao_terrain_patch_acc` is BUILT AND COMPOSED NOWHERE.** *Before
  commissioning a block, grep the tree for the thing it replaces* — check this
  one first.
* **The bench needs a new WRITABLE region.** FIELDACTIVE added HPS region 6 for a
  staging window the loader had advertised since composition **with nothing
  backing it**; copy that pattern.

## THE OWNER'S CONSTRAINTS — THEY OVERRIDE CONVENIENCE

* **Do not silently cut field capacity, semantics, update behaviour, or the
  destination** to meet the bandwidth budget.
* **Measure the actual cost, report it either way.** Targeted local measurement
  **authorised**; another full-console fit **not**.
* **If the COMPLETED implementation proves the specified publication semantics
  cannot meet the frame/bandwidth contract, STOP and escalate with that measured
  conflict.** Note the edge: **a refusal grounded in the old bandwidth estimate
  will be sent back; one grounded in your measurement of the built thing will be
  acted on.**
* **`I34` must become TRUE IN THE ASSEMBLED CONSOLE.**

## DO NOT MOVE THE REGISTER BY PROSE — AND THE REAL STRING IS NOT WHAT I SAID

I previously wrote that the register keys on `BOUNDARY`. **That was false.**
Measured (`completion_register.py:236-243`): `BOUNDARY` only sets the **label**,
and a `boundary` entry **is still a gap**. **The only string that removes an entry
from the count is `NOT a tie-off`, and only IN THE HEAD LINE.**

**So the forbidden edit is adding `NOT a tie-off` to `I34`'s head.** Do not. The
tool hard-fails a body occurrence precisely because an entry once read CLOSED
twice — once when its author used the phrase, and again **when they quoted it
while describing the first accident.**

## KNOWN DEFECTS — DECLARED, NOT YOURS TO SILENCE

* **`-FieldUncovered` exits 1 on `lane_desync_o` alone.** NOPROG did **not**
  silence it: that counter's meaning is **measured to differ from its own
  header**, and waiving it would manufacture a false green. **Leave it red unless
  you fix the counter, and if you do, prove the fix by firing it.**
* **A latent production defect**: `zhao_field_progcache` sets `cm_slot_o`
  nonblocking on the fire cycle while the host reads it **on** that cycle, so an
  insert clears the **previous** commit's slot — **masked only because reset 0
  equals slot 0.** If your work makes a second slot live, **this stops being
  latent.** Watch for it; report it if it bites.
* **The doorbell/host ordering contradiction** is worked around, not resolved.
* **`REGS=32` needs NO widening** — `wave_pool` and `impact_wave` lower clean at
  32 via `--scalar-base`. **No ALM purchase is owed here.**

## The fences

* **A concurrent packet (`UNPARK`) owns the SHELL, `zhao_post_lease`, the
  raster/phase path and the walk-arrangement control forms.** You own **TERRAIN,
  FIELD and the field stimulus**. `tests/prod/tb_zhao_console_core_smoke.sv`,
  `tests/prod/run_console_core_smoke.ps1` and
  `fpga/rtl/prod/zhao_console_core.sv` are **SHARED — stage the HUNK, not the
  file.** Those three have produced **two real merge conflicts today.**
* **EVERY NEW SWITCH NEEDS A TAG** in the smoke script's chain, or two forms
  silently share an object directory and one gate runs the other's binary.
* **Do not regress the plain smoke**: `pixels=2816`, `frames_admitted=1`,
  `fragments=1216`, `tile[max/or]=[6 7]`. **That is clause 6.**
* **Do not regress velocity** — `terrain_veljoin_directed` 19/0 on the repaired
  test, `part_terrain_tap_directed` 1297/0, `composepub_acceptance` 154/0.
* **DO NOT DECLARE AN ARRAY INSIDE A `generate for` LOOP** — 146,414 registers
  against 1,010. Generate-**IF** infers; module scope infers; **the LOOP is the
  killer.**

## Evidence bar

* **Clause 3 positive**, with the value's non-coincidence **argued from the
  layout**, not asserted. This console has failed this twice — a check passing on
  a **constant `0xFF`**, and `mat_cells` **counting cells rather than values**.
* **All six clauses, separately.**
* **Prove every counter you quote.** `fld_earth_noprog_o` is the cautionary
  example: it **ORs two causes**, so it could never have identified the fault it
  was quoted for. **Ask what a counter is summing before citing it.**
* **Check any control you add CAN FAIL.** And note three counters sat as **core
  ports, unread, since the fieldlist was composed** — an exported counter nobody
  reads is not evidence.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE**, reading python's own exit code.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code.** A
  **PowerShell exception leaves `$LASTEXITCODE` STALE**, and `RC=$?` after a
  pipeline reports the pipeline's last stage.
* **A control form builds its OWN model** (`-SkipVerilate` is wrong for it), and
  **a switch passed as a quoted string becomes POSITIONAL** — splat a hashtable.
  **Do not splat through `powershell -File`.**
* **The console smoke verilates into a TEMP directory** — a process filter on your
  worktree path will not find your own build.
* **Diagnostics must print BEFORE the assertion they diagnose.**
* **Do not edit the bench mid-verilate** — that run is worthless.
* **RUN GATE 31** if you touch a port. An unconnected output is `.port_o ()`,
  never an omission. A new core port costs **four** things: the port, **BOTH**
  wrapper mutants, the bench's wire, and a **reader**.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED** — use `-F <file>`.
* **Stop arming `until grep` poll loops** — eleven accumulated today. Use the task
  mechanism, and **read CPU as a RATE**, age and CPU together.

## Deliverable

1. **The six clauses, each answered separately, in the POSITIVE direction.**
2. **`TERRAIN.COMPOSED_MATERIAL` built**, or the measured conflict that stops it.
3. **The material value's non-coincidence, argued from the layout.**
4. **The cost, measured** — targeted local only.
5. **The register, measured BARE.** **Do not touch `I34`'s head line.**
6. **Every claim in this brief or the findings you found FALSE.** **Seven
   consecutive packets have found their brief wrong in a load-bearing place** —
   including the last one, which found a false claim in the owner decision
   record. Hunt deliberately.
7. **What you refused, and anything you got wrong and caught yourself.**
8. **If this is more than one packet, land what you have and DECLARE the rest.**
9. Branch and commit hash. **Push `gz/matfield` only.** Never `--force`.
