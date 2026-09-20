# Wave 4 — THE ELEVEN DISCONNECTED MODULES

The register stands at **21 = 9 tie-offs + 11 disconnected + 1 unbuilt**. The
eleven disconnected are the bulk of what is left, and they are all the same
shape: **the module is built, tested and on disk, and nothing instantiates it.**

> `zhao_measure_governor`, `zhao_terrain_normalmap`, `zhao_terrain_velocity`,
> `zhao_terrain_bake_v2`, `zhao_terrain_lod`, `zhao_geom_parambuf`,
> `zhao_forge_shadow`, `zhao_forge_prim`, `zhao_forge_prim_eval`,
> `zhao_forge_cliff`, `zhao_post_gather`

Read `PACKET-PROTOCOL.md` in this folder first. What follows is what is specific
to composition work, and **most of it is about not making the number go down
dishonestly.**

---

## THE ONE RULE THAT OVERRIDES EVERY OTHER

**Never close a gap by removing, narrowing, stubbing or disconnecting function,
and never compose an older version of anything.**

Composition is the single most dangerous kind of work for this rule, because
**instantiating a module makes the register drop immediately**, and the register
cannot see what you tied off to achieve it.

### R159 — the register CANNOT see an undeclared tie-off

`completion_register.py` counts **declared** tie-offs: it parses the
`INCOMPLETE -- TIED OFF, AND WHY` comment block in `zhao_console_core.sv`
(lines ~406–4201), which is **hand-maintained**. A tie-off nobody writes down is
invisible, **so composing a block with undeclared tie-offs makes the count go
DOWN.** That is the flattering direction, by construction, and it is the exact
move this campaign forbids.

**So: if your composition creates a tie-off, you declare it in the INCOMPLETE
block IN THE SAME COMMIT.** Not afterwards, not in your findings file. The
coordinator checks the block against your diff when your packet lands.

C1 was about to compose GEOM.WARP's client port for a clean 21 → 20 and
**re-measured instead**, finding ten-plus tie-offs behind it — `v_attr_i`, all
nine warp-descriptor ports, the whole `f_*` port. It refused the composition and
said why. **That is the standard.** Five lanes have now declined a flattering
move; none has been criticised for it and several findings came out of it.

### R163 — a `_v2` file that exists but is NOT composed CREATES a violation

Measured by E1: a file containing nothing but an empty module makes the register
report `composed X superseded by X_v2` across **three production roots**. The
supersession check keys on the naming convention, not on whether anything was
actually superseded.

**A `_v2` must be BORN IN THE COMMIT THAT COMPOSES IT.** Do not create one early
and wire it later. `zhao_field_host_v2` cost a documented exception and two
rulings for exactly this.

### And the converse: BUILT is not CONNECTED, CONNECTED is not CORRECT

Your module already passes its own tests. That is not the question. The question
is whether the thing it is wired to actually produces what it consumes, at the
right width, at the right time. **`zhao_prod_top` has now been caught measuring
the wrong machine three times in one day** (R151/R161) — three different
parameters, each time because a default and an override disagreed and nothing
compared them.

**So after composing: regenerate `zhao_prod_top.sv` via
`tools/quartus/gen_prod_top.py`, and then READ THE GENERATED OUTPUT** to confirm
your parameters actually landed there. Put any composed selection in
`production_parameter_overrides`. A default and an override that agree are
cheap; a default nobody notices is what produced all three.

---

## THE GATES

All of these, green, before you report. Run them **after** you commit
(`mutant_copy_drift.py` compares COMMIT ORDER, so on a staged tree it answers
about the tree before your merge — R121):

```
python tools/quartus/check_console_inventory.py
python tools/quartus/check_prod_manifest.py
python tools/quartus/check_quartus17_syntax.py
python tools/budget/check_case_labels.py
python tools/budget/mutant_copy_drift.py
python tools/budget/mutant_drivers.py
python tools/budget/uncashed_cheques.py
python tools/design/check_counters.py
python tools/budget/refmodel_liveness.py
python tools/quartus/gen_prod_top.py --check
python tools/quartus/gen_console_board.py --check
python tools/design/gen_shell_paired_diff.py --check
```

**And the six smoke forms**, which is the only thing that proves the composed
core still carries traffic:

```
tests/prod/run_console_core_smoke.ps1            # and -LintOnly -Mutant
tests/prod/run_console_core_smoke.ps1 -BadVertex # -NoEchoArm -BadTraceArm
```

### NEW, AND YOU ARE THE FIRST PACKETS TO HAVE IT

```
python tools/design/packet_h_tieoff_audit.py fpga/rtl/prod/zhao_console_core.sv
```

It lists **every literal connection in a port map** and sorts them into
declared / reasoned / covered-by-a-group-comment / SILENT. Run it before and
after your composition and **account for every row you added.** The core is
currently at `7 declared, 1 reasoned, 10 by group comment, 1 SILENT`.

**Read R166 in `reports/OWNER-RULINGS-20260919-EVENING.md` before you trust it.**
Pointed at the core for the first time it reported `0 declared, 19 silent` and
**all nineteen were the tool's own blindness** — it could see 18 of 88
instantiations, it knew the marker `TIE:` but not the core's `REAL:` (163 uses
against 0), and it did not understand a reason written above a *run* of
connections. It is calibrated now. The lesson is the one that matters for you:

> **Before quoting ANY tool pointed at something new, measure whether it can see
> its subject.** One `grep -c` of the two instantiation forms is what caught it.

A tool firing on everything is as broken as one firing on nothing. If your
composition makes this audit report a pile of new silent rows, **read three of
them by hand before believing the total.**

---

## R60, AND IT IS NOT NEGOTIABLE

**A directed test must BUILD AND RUN, not merely lint.** `--lint-only` does not
execute `initial` blocks, so it says nothing whatever about an elaboration
guard. A clean lint is one tool's opinion about syntax.

And **a block that has never been through `quartus_map` has not been shown to be
synthesizable**, however clean its Verilator lint. Two SystemVerilog forms that
lint with 0 diagnostics and fail `quartus_map` outright:

* a bare module-scope `if (...) $fatal(...)` — Quartus 17.0 needs it inside
  `initial begin ... end`;
* an implicit generate — it needs explicit `generate` / `endgenerate`.

`check_quartus17_syntax.py` catches both. Run it.

---

## CITE BY SYMBOL, NOT BY LINE NUMBER

**Line numbers in this tree have a half-life of hours.** Today: S1 corrected
three of the coordinator's, F1 three more, FORGESHADOW three in production RTL,
E1 three more, and C1 found a brief's references ~340 lines stale. E1's summary
of the coordinator's own brief is the thing to avoid: *"Every statement was true;
every citation was wrong."*

Any line number below is **a hint that is expected to rot.** Search for the
symbol. And if you find a citation wrong, **say so in your findings** — four
lanes doing that is why the rulings file is worth reading.

**Verify every claim in YOUR OWN TREE.** A brief written from the coordinator's
working tree may describe commits your branch does not have (R106, R144).

---

## PACKET A — FORGE (four modules, one subsystem)

`zhao_forge_shadow`, `zhao_forge_prim`, `zhao_forge_prim_eval`,
`zhao_forge_cliff` — all under `fpga/rtl/forge/`.

**You have a fit measurement in hand, and it is the reason this packet is worth
running now.** `reports/FIT-PLAN-AT-ZERO.md` records F-CLIFF-GOLDEN with its
outcome bands written BEFORE the run:

> `zhao_forge_cliff` fits at **6,674 ALM** against the RAM candidate's **976** —
> **5,698 ALM and 3,086 registers saved, fit-minus-fit**, which is **13.6% of
> the 41,910-ALM device.**

So composing `zhao_forge_cliff` as it stands spends 13.6% of the device on a
structure a committed alternative does in one seventh the area. Read
`reports/FORGE-CLIFF-BITMAP-RAM-20260910.md`,
`reports/FORGE-CLIFF-REARCH-ARCHITECTURE-20260909.md` and
`reports/PREDICTION-forge-cliff-ram.md` **before composing anything**, and say
in your findings which one you composed and why.

**This is a real decision and it is yours to make and state, not to skip.** The
device ceiling is 41,910 ALM / 112 DSP / 553 M10K and it is the pass/fail line.
If the RAM candidate is not ready to compose, say exactly what is missing —
that is a more useful packet result than composing the expensive one quietly.

`zhao_forge_prim_eval` has `reports/FORGE-PRIM-EVAL-IMPLEMENTATION-20260909.md`.

Branch `gz/forge4`. Expect these four to share a seam with each other; compose
them together rather than one per commit if the seam demands it, but **one
commit per module wherever they are separable.**

---

## PACKET B — TERRAIN (four modules, one subsystem)

`zhao_terrain_normalmap`, `zhao_terrain_velocity`, `zhao_terrain_bake_v2`,
`zhao_terrain_lod` — all under `fpga/rtl/terrain/`.

**`zhao_terrain_normalmap` is BLOCKED ON AN OWNER DECISION (R115): ratify
TERRAIN.NORMALMAP or supersede it.** Do not compose it on your own authority and
do not let it block the other three. Report what you would do under each
outcome; that is what makes the decision cheap for the owner to make.

**`zhao_terrain_bake_v2` is a `_v2`** — so R163 applies directly and it is
already born, which means it is already producing a supersession relationship.
Check what `superseded_in_roots` says about it **before** and after you compose,
and make sure the number moves the way you expect.

Watch for the version trap generally: this tree supersedes **both ways** — by
SUFFIX (`zhao_texture_cache_pipe_v2`) and by INFIX (`zhao_field_v3_len`). A grep
for `_v2$` finds half of it. `ONLY THE LATEST VERSION GETS COMPOSED OR FITTED`
is an owner ruling in `CLAUDE.md`, said after the console was found composing the
entire v1 FIELD datapath while fourteen v3 modules sat outside the closure.

Branch `gz/terrcomp`.

---

## WHAT TO DO IF A COMPOSITION WILL NOT CLOSE HONESTLY

**Refuse it, name the blocker, and measure the blocker.** A refusal with a
measured blocker is a good packet result and has been the most useful output of
several lanes today.

What makes a refusal worth reading:

* **name the specific missing producer or port**, not "it is not ready";
* **say what would have to exist**, and whether it is an owner decision, a
  missing module, or a port change;
* **re-measure any blocker you inherit from this brief.** R165: two of I34's
  three recorded blockers had EXPIRED — one was answered by an owner directive
  that postdates the entry, and one predates a `BIND_PROGRAM` that now exists.
  **A refusal is a claim about a moment**, and this tree moves fast enough that
  a blocker written yesterday may be spent today.

## GIT

The coordinator merges. **Packets never rebase.** Branch from the coordinator
head you are given, commit and push to your own branch, report.

`zhao_console_core.sv`, `tests/CMakeLists.txt`, `design/fit_targets.yml` and
`design/prod_manifest.yml` are **SHARED**. Concurrent packets are editing them.
Read the `CLAUDE.md` section on the shared index before you stage anything:
`git add <file>` stages whatever is in the working tree including work you did
not write; `git checkout -- <file>` **discards** another agent's uncommitted work
with no reflog; and a private-index `read-tree`/`commit` pair **must be atomic**
or you will silently revert whatever landed in between.

**Check `git diff --cached --name-only` before every commit.** A plain
`git commit` commits the whole index, not what you just added.

And **`[IO.File]` ignores `cd`** — pass it an ABSOLUTE path or your writes land
in the coordinator's checkout, silently, where no gate of yours can see them.
The tell is a clean `git status` after a write that reported success.

---

## CORRECTION, 2026-09-20 21:20 — FOUR GATE PATHS IN THIS FILE WERE WRONG

The gate list above originally named `check_console_inventory.py`,
`check_quartus17_syntax.py`, `check_case_labels.py` and `mutant_drivers.py`
under `tools/design/`. **None of the four is there.** Corrected above:

```
tools/quartus/check_console_inventory.py
tools/quartus/check_quartus17_syntax.py
tools/budget/check_case_labels.py
tools/budget/mutant_drivers.py
```

I found this by running the list myself on the merged tree, where four of nine
returned `RC=2 -- can't open file`. **A gate that cannot be found reports a
non-zero exit code that is not a failing gate**, and a packet reading only the
number would have chased a phantom regression; a packet reading it as "the
script is missing, so skip it" would have shipped without the check. Both
readings are wrong and both are available.

This is the brief-citation failure this file warns YOU about, committed by the
coordinator in the same file that warns about it. **Run `--help` or check the
path exists before concluding a gate is red.** The full set, verified green on
`75b96d26`, is in the run's `TASK_LOG.md`.
