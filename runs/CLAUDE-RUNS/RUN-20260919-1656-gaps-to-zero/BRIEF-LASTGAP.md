# LASTGAP — `I34` is the only entry left. Clause 3, then `COMPOSED_MATERIAL`.

**Branch `gz/lastgap`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**THE REGISTER READS 1. `I55` CLOSED TODAY — the console draws all 2,816 of its
reference-derived pixels from SDRAM. `I34` IS THE LAST GAP IN THE CAMPAIGN.**

**READ FIRST, IN FULL:**

1. **`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`** — the owner's
   ruling and **your acceptance bar**. Read its CORRECTION block; it is mine.
2. **`reports/DECISION-20260927-I34-CONSTANT-POOL.md`** — **I have taken the
   constant-pool decision. Do not re-litigate it; build it.**
3. `runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-MATFIELD.md` — your
   immediate predecessor. It found the blocker and built the program.
4. `FINDINGS-NOPROG.md`, `FINDINGS-FIELDACTIVE.md`.

## WHERE I34 STANDS — FIVE OF SIX CLAUSES

| # | clause | status |
|---|---|---|
| 1 | real Field program **installed and executed** | **MET** — `runs=1089 noprog=0 faults=0`, real capsule over the real bridge |
| 2 | field **covers the intended terrain** | **MET** — `tp_covers=1` |
| 3 | material write **cannot equal the authored baseline by accident** | **value BUILT and PROVEN, blocked by the constant pool** |
| 4 | reaches the **intended production consumer** | routing **MET** |
| 5 | **uncovered control restores the authored result** | **MET** — `runs=0 skipped_uncovered=1089`, `pixels=2816`, `tile[6 7]` |
| 6 | **no-field forms unchanged** | **MET** — plain `PASS`, all four numbers unmoved |

**`scorch_wash` exists** — the tree's first legal material-token program, whose
out-lane 2 is a **v1 material token** rather than a height.

**And clause 3's non-coincidence is already argued from the layout**, which is
what the owner asked for: authored bytes `{1,2,5,6} ∪ [0x30,0xCF]` — **164
values** — against the program's **50**, **intersection EMPTY**, so no swap,
rotation or single-byte coincidence can fake a pass. A detector that **parses the
bench** enforces it, **and it has been fired.**

## THE BLOCKER, AND THE DECISION I HAVE TAKEN

**The composed console never delivers a field program's CONSTANT POOL to the
execution register file.** The value exists, the path exists, **the number never
arrives.**

**Fix it in the LIBRARY: `lower()` emits folded constants as `LDC` uops. No
RTL.** Reasons, in full, in the decision record — the short version is that the
console is **293,352 ALUTs against 227,120 present**, so every RTL alternative
spends area to carry a number **the instruction set already carries**, and the
register high-water is **28 of 32**.

**`LdUniform` is disposed of, not untried.** MATFIELD built it, **disproved it by
reading `ring_svc`, and reverted it before committing** rather than ship a
non-repair under a repair's name. **Do not re-attempt it** without addressing
what `ring_svc` actually does.

**Two constraints on the fix:**

* **"Zero silicon" is a CLAIM and you owe its evidence** — `gen_prod_top --check`
  and `gen_console_board --check` both fresh, as NOPROG demonstrated for its own.
* **Re-measure the register high-water.** Folding constants into `LDC` uops
  **adds register pressure by construction**; 28 of 32 is the *before* number.

**This is the one compiler change the hardware lane needs. It is not a licence to
widen the compiler's scope** — Nanquan is provisional and the standing rule is to
stop compiler overengineering.

## THEN: `TERRAIN.COMPOSED_MATERIAL`

**Still commissioned, still unbuilt, and the constant-pool fix does not
substitute for it.** MATFIELD left its shape: **the owner's own 256 × 8 KiB**, the
enactment pattern, the tap point, and why the **tagged** payload is the right one.
`zhao_terrain_patch_acc` is **built and composed nowhere** — check it before
commissioning anything new.

## THE OWNER'S CONSTRAINTS

* **Do not silently cut field capacity, semantics, update behaviour, or the
  destination.**
* **Measure the actual cost, report it either way.** Targeted local measurement
  **authorised**; a full-console fit **not**.
* **If the COMPLETED implementation proves the publication semantics cannot meet
  the frame/bandwidth contract, STOP and escalate with that MEASURED conflict.**
  MATFIELD labelled its bandwidth figure **derived, not measured**, and therefore
  did **not** offer it as an escalation — that is the standard. **A refusal on an
  estimate will be sent back.**
* **`I34` must become TRUE IN THE ASSEMBLED CONSOLE.**

## TWO ASSERTIONS ALREADY COMMITTED BUT NEVER EXERCISED

MATFIELD's **clause-3 and clause-4 consumer assertions are committed and
UNREACHED** — the run fatals above them. **Once the constant arrives they become
reachable. They must be seen to PASS, and their ability to FAIL must be shown.**
An assertion that has never executed is not evidence, and this campaign has
shipped four of those in a week.

## DO NOT MOVE THE REGISTER BY PROSE

`BOUNDARY` only sets the **label**; a `boundary` entry is still a gap. **The only
string that removes an entry from the count is `NOT a tie-off`, and only IN THE
HEAD LINE** — which is a **declaration**, not prose. **Do not write it.** When
`I34`'s machine is true, **I will declare it, having measured it myself**, as I
did for `I55` today — where the register's own guard caught *me* putting the
phrase on the wrong line.

## The fences

* **You are the only packet running.** All of TERRAIN, FIELD, the field stimulus
  and the compiler lowering are yours. **Do not touch `zhao_geom_*` or the
  shell** beyond reading them.
* **Do not regress the plain smoke**: `pixels=2816`, `frames_admitted=1`,
  `fragments=1216`, `tile[max/or]=[6 7]`. **That is clause 6.**
* **Do not regress `I55`**: `paramwalk tris=101`, `fetcharm vread=303` with its
  asserted invariant, `tilewalk tiles=11 jobs=101`. **The console now ships
  drawing from SDRAM** — if your change moves those, say so loudly.
* **Do not regress velocity** — `terrain_veljoin_directed` 19/0,
  `part_terrain_tap_directed` 1297/0, `composepub_acceptance` 154/0.
* **`-FieldUncovered` exits 1 on `lane_desync_o`** — a counter whose meaning is
  measured to differ from its header. **Do not silence it.**
* **A latent defect becomes live if you make a second slot live**:
  `zhao_field_progcache` sets `cm_slot_o` nonblocking on the fire cycle while the
  host reads it **on** that cycle, masked only because reset 0 == slot 0.

## Evidence bar

* **Clause 3 MET in the console**, with the two unreached assertions now
  **exercised**.
* **All six clauses, separately, in the owner's own terms.**
* **The zero-silicon claim proven**, and the register high-water re-measured.
* **Prove every counter you quote.** `fld_earth_noprog_o` **ORs two causes** and
  so could never identify the fault it was cited for. **Ask what a counter sums
  before citing it.**
* **Check any control you add CAN FAIL**, and note three counters sat as **core
  ports, unread, since the fieldlist was composed**.
* **Do not write a test that asserts the bug.**
* **Re-run `completion_register.py` BARE**, reading python's own exit code.

## Traps

* **THE GATES DO NOT BUILD.** `cmake --preset windows-native` from PowerShell
  with `tools/env/zhao-env.ps1` sourced; **read the BUILD's exit code.** A
  **PowerShell exception leaves `$LASTEXITCODE` STALE**, and `RC=$?` after a
  pipeline reports the pipeline's last stage.
* **These scripts speak through `Write-Host`, so `2>&1` DROPS THEIR OUTPUT** —
  use `*>&1`. A lint run wrote a **zero-byte log under RC=0** today.
* **`.gitattributes` can silently reshape a fixture** — MATFIELD proved it with a
  probe, 16 bytes becoming 14. **Check byte counts on generated binaries.**
* **A control form builds its OWN model**; a switch passed as a quoted string
  becomes **POSITIONAL** — splat, and not through `powershell -File`.
* **Do not edit the bench mid-verilate** — three packets lost runs to this.
* **Backticks in `git commit -m` are COMMAND-SUBSTITUTED**, and `&` breaks a
  heredoc — use `git commit -F <file>`.
* **Stop arming `until grep` poll loops.** Use the task mechanism, and **read CPU
  as a RATE.**

## Deliverable

1. **The six clauses, each answered separately, in the POSITIVE direction.**
2. **The constant pool delivered, in the library**, with the zero-silicon claim
   proven and the register high-water re-measured.
3. **The two previously-unreached assertions exercised**, and shown able to fail.
4. **`TERRAIN.COMPOSED_MATERIAL` built**, or the MEASURED conflict that stops it.
5. **The cost, measured.**
6. **The register, measured BARE.** **Do not write the head-line declaration.**
7. **Every claim in this brief or the records you found FALSE.** **Eight
   consecutive packets found theirs wrong in a load-bearing place** — including
   one that found a false claim in an owner decision record. Hunt deliberately.
8. **What you refused, and anything you got wrong and caught yourself.**
9. Branch and commit hash. **Push `gz/lastgap` only.** Never `--force`.
