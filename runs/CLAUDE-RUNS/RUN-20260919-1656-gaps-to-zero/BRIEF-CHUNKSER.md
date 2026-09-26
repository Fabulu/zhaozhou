# CHUNKSER — I54's serialiser, and I56's guaranteed giant

**Branch `gz/chunkser`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**Two of the register's seven. `I55` is explicitly OUT OF SCOPE** — see the last
section, and do not quietly take it on.

## Read the entries first

`fpga/rtl/prod/zhao_console_core.sv`, entries **I54**, **I55** and **I56**, in
full. **Find them with `grep -n '^// I5[456]\.'` rather than by line number** —
that file gained 441 lines in the TERRVEL merge alone, and a stale line number is
how a reader ends up in the wrong entry. They are long because four packets have
narrowed them, and **two of those packets amended the entry by measuring a
premise false.** That is the shape of this work.

Also read `design/contracts/GEOM.PARAMBUF.md` — §"The content guarantee this
exists to keep" and the R7 note near line 455.

## I54 — the chunk serialiser

**ARENAID removed the disqualifying blocker on 2026-09-25 and left the shape
standing.** The entry used to say the binner's triangle store is POST-clip while
the arena's descriptors are PRE-clip — *"different sets with different
cardinality and no mapping in the tree"*. **That is gone.** The arena's
descriptors are now post-clip: `u_geom_vertid` builds them from `u_geom_clip`'s
accepted packet, and `u_geom_paramarena.td_id_o` gives each one its arena index
at the clock it is allocated, re-exported on `u_geom_vertid.tri_id_o`. **The
mapping the entry says is absent now exists and is a port.**

(ARENAID also measured the premise under it false outright: `zhao_geom_clip` does
whole-primitive near-plane **rejection** and *"never produces more than one
triangle for one triangle in"*. §4's clipping-lineage language describes a
machine this console is not.)

**What remains is the SHAPE, and the entry is right about it:**

* `zhao_geom_binner_v2`'s arena is `ref_ram [0:(CHUNKS*CHUNK_REFS)-1]` with
  `CHUNKS = 256`, `CHUNK_REFS = 4`, a ref of **`TRI_W` = seven bits**, plus
  `next_ram [0:CHUNKS-1]`.
* R7's chunk is **64 bytes**: a u32 `next`, a u16 count, a u16 generation and
  **fourteen u32 ids**.
* The binner's 7-bit ref indexes **its own `TRI_CAP = 128` store**, not the
  arena.

So the work is **a serialiser that translates the binner's per-tile reference
order into chunks of arena triangle ids and stamps the generation**, with
`tri_id_o` as the translation table's one input. Plus the ports on
`zhao_geom_binner_v2` that let a reader see a chunk at all — the entry's estimate
is the chunk index and slot at each ref write, the chain link, and the per-tile
head, and **that estimate has already been wrong once in the flattering
direction, so re-derive it rather than quoting it.**

### THE FAILURE THIS ENTRY EXISTS TO PREVENT — quoted, because it is your trap

> *a binner slot is 0..127, every one of those is far below any plausible sealed
> `tris`, so `zhao_geom_parambuf`'s `ck_illegal_o` and `td_illegal_o` would both
> **PASS** and the walk would return **WRONG DESCRIPTORS THAT DECODE CLEANLY**.
> Every counter would balance. No picture would look wrong, because nothing takes
> the walker's triangles yet — and when something does, it would draw the wrong
> geometry with a clean bill of health.*

**Both range guards are structurally blind to this fault**, because a wrong id
inside the legal range is not an out-of-range id. **Your evidence therefore
cannot be "the guards pass".** It must be that a chunk's ids are the ids of the
triangles that tile actually references — checked against the arena's own
descriptors, not against your own serialiser's bookkeeping.

## I56 — the guaranteed giant, and it is PROTECTED

The frame-end half closed at ARENAWIRE. **The open half is the quota**, and it is
the one the owner's directive names by hand.

`design/contracts/GEOM.PARAMBUF.md`:

> *A giant is a separate quota: reserve **at least 32,768 tile references** for
> one giant **before** ordinary kMesh allocation. If the giant consumes the
> view's budget, ordinary creatures demote by declared LOD priority — **the giant
> is never silently truncated.***

Today the composed seal is **the arena's own capacity**, which reserves nothing.
The entry says so deliberately: *"a reservation that silently is not happening
looks exactly like one that is."*

**The owner's directive lists "shrink the guaranteed giant" among the things this
campaign's standing authority explicitly does NOT cover.** So the quota is not a
number to tune down to fit; if it cannot be met, that is an escalation.

**RE-ASK THE BLOCKER.** The entry says the quota *"waits on a Measure with
somewhere to publish a number"*. I looked at `zhao_measure_tokens` — it is
composed (1,223 ALUTs in the console) and it is a **token budget** block:
`budget_geom0/1`, `budget_frag0/1`, `budget_shared`, a geometry/fragment
`req_class_i`, an §9 ladder rung. **It has no tile-reference quota port and
nothing that looks like one.** That is a seam, not a conclusion, and there are
three shapes the answer could take — decide which, and say why:

1. the quota is **allocator policy inside the arena**, a reservation the seal
   applies, with the 32,768 a named constant — no Measure involved;
2. the quota is genuinely a Measure output and `zhao_measure_tokens` needs the
   port — then it is a change to a composed block and must be declared as one;
3. something else already publishes it and the entry is stale, which in this tree
   happens at a rate near one in two.

**Whatever you choose, the demotion law comes with it**: under pressure ordinary
creatures demote **by declared LOD priority** and the giant stays whole. A quota
that reserves but has no demotion path is half the law, and half a law that
*looks* enforced is worse than none.

## I55 IS OUT OF SCOPE, and here is why you must not drift into it

I55 is the **raster-path swap** — replacing `zhao_geom_binner_v2`'s on-chip chunk
arena in the live raster path with the external walk. The entry declines it
twice, for two reasons that are both still true:

* it is **an area and throughput change to the block the fit budget is tightest
  on** (`zhao_geom_bin_pipe_v2` is 30,266 ALUTs and 86 DSP inside the shell —
  see `reports/synthesis/console_entity_attrib.md`);
* *"doing it half-way, with the walker's triangles ORed into the live stream,
  would produce a picture and prove nothing."*

**It also needs your I54 landed first** — the entry is explicit that this is *"not
an ordering preference but a correctness one"*. If your work makes I55 look easy,
write that down as a finding and stop. It is the next packet, not this one.

## Evidence bar

* **A chunk whose ids are the right ids**, checked against the arena's
  descriptors. Not "the guards pass" — see the trap above.
* **A live chain of real production modules**, wired port-for-port as
  `zhao_console_core` wires them. `tests/terrain/composepub_acceptance.cpp` and
  `tests/prod/terrainaux_acceptance.cpp` are the two patterns to copy.
* **Prove every counter you quote**, and **check what clocks the two sides of any
  comparison** — a checker whose operands move together cannot fire. If a guard
  is unreachable with legal stimulus it needs a **committed mutant** under
  `tests/mutants/`, renamed so no source list elaborates it, driver polarity
  inverted so it passes when the counter FIRES. Register positive controls with
  the right polarity; a recent packet registered two backwards and ctest caught
  it in twenty seconds.
* **Do not write a test that asserts the bug.** Assert the correct behaviour and
  keep the control separate.
* **Re-run `completion_register.py` BARE after editing any entry text.** It
  classifies entries by scanning their prose, and a recent packet relabelled an
  entry by writing "TIED TO ZERO" about a tie it had just removed. The tool was
  right and the prose was the bug.

## Traps

* **THE GATES DO NOT BUILD.** `gate_sweep.py` RC 0 means *nothing moved against
  the baseline*. Run `cmake --preset windows-native` yourself, from PowerShell
  with `tools/env/zhao-env.ps1` sourced. **Read the build's exit code, not the
  pipeline's.**
* **A new block is in no running fit's closure**, so write and test it
  standalone; that is the cheapest thing you can do and it is why I54 is shaped
  this way.
* **`zhao_geom_binner_v2` is inside the shell's fit closure.** Ports on it are a
  change to another subsystem's closure — declare it, regenerate
  `zhao_prod_top.sv` (`tools/quartus/gen_prod_top.py`) and re-run
  `tools/quartus/check_prod_manifest.py`. Registering a block in the ledger, the
  manifest and the production source list are **three different acts**.
* **Check the committed mutant-copy count of any file before your first edit** —
  `mutant_copy_drift` keys on COMMIT TIME, editing even a comment stales every
  copy, and reverting does not clear it.
* **Verilator lint-clean is not Quartus-synthesizable.** Quartus 17.0 needs an
  elaboration check inside `initial begin ... end` and explicit
  `generate`/`endgenerate`. Both forms passed lint with 0 diagnostics.
* **DO NOT start a console or full-device fit.** Those are the coordinator's.
  Verilator answers every question you have, in seconds.
* **One `ctest` at a time per build tree**; after killing one,
  `rm -f build/Testing/Temporary/CTestCheckpoint.txt
  build/Testing/Temporary/LastTest.log.tmp*` before restarting.

## Deliverable

Commit as you go. Your final commit message is your FINDINGS. State:

1. **The register before and after, measured BARE**, and which entries closed.
2. **The chunk evidence** — ids checked against the arena's descriptors, with
   counts, and what would have happened had you trusted the range guards.
3. **Your I56 decision**, which of the three shapes it took, and why. If the
   giant's quota cannot be met, say so and escalate rather than trimming it.
4. **Every claim in this brief or in the entries that you found FALSE.** This is
   the most valuable thing you can return; the "X does not exist" claims in this
   tree run false at a rate near one in two.
5. **What you refused**, including I55 if it started to look easy.
6. **Anything you got wrong and caught yourself.**
7. The branch and commit hash. **Push `gz/chunkser` only.** Never rebase or push
   the shared branch, and never `--force`.
