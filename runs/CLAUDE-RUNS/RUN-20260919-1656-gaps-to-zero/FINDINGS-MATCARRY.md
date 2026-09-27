# FINDINGS — MATCARRY, 2026-09-27

**Branch `gz/matcarry`.** Base `71122893`.
Commits `d855b914`, `8a4912d5`, `2b5cc974` (+ this file).
Answers the brief's nine numbered deliverables in order.

---

## 1. THE REGISTER, MEASURED BARE, AND WHETHER I34 CLOSED

| | value |
|---|---|
| before (`71122893`) | **2** — `I34`, `I55`. RC 1 |
| after | **2** — `I34`, `I55`. RC 1 |

Both `python tools/budget/completion_register.py` **bare**, exit code read from
the command itself and not from a pipeline. Mandatory RTL capabilities connected
in the console: **113**, with `BUILT BUT NOT CONNECTED`, `NOT BUILT AT ALL`,
`EXCUSED BY AN UNCITED FLAG` and `UNRESOLVABLE` all **0**. Re-measured bare after
editing the entry text, as the brief requires.

**`I34` DID NOT CLOSE, and it must not be reported as closing.** What closed is
the carriage its own (M7) named as the last open surface: the composed triple now
reaches the mosaic as a **per-triangle** value, proven in the composed console.
The register entry is classified `boundary` against `terr_pt_fld_*` —
TERRAIN.PATCH's field-height lane — which this packet did not touch and was not
asked to.

Being exact, because this entry has been half-closed before: **the transport is
built, tested, and proven per-triangle end to end.** A terrain triangle's triple
additionally needs the host to name a terrain material (`SetEnvironment
.terrain_material_set`) and a TILESET binding row behind it. §5 says why. The
console smoke drives both — `terrmat backed=128 orphan=0`, set `00abcd02` — so
the gate is open in the console's own stimulus, measured rather than assumed.

---

## 2. BOTH LEGS, AND THE FOURTH FILE THE DECISION DID NOT NAME

### Leg 1 — the rider, clipdoor → GEOM.CLIP

`GEOM_VID_RIDERW` **50 → 82**, the token added at the **top** so `[49:34]`
material_id, `[33:2]` R28 raster_state and `[1:0]` domain keep the exact bit
positions `u_geom_vertid` already reads.

**`zhao_geom_clipdoor` and `zhao_geom_clip` needed NO EDIT AT ALL.** Both are
already parameterised on `RIDERW` and treat the rider as strictly opaque —
`zhao_geom_clip.sv:176`: *"`RIDERW` is an OPAQUE per-primitive rider, carried
exactly as `src_id` is"*. Leg 1 is therefore one number in the composer.

I did **not** reuse the 30 bits written as zero on three client arms. They are
R28's `raster_state`, allocated and not spare.

### Leg 2 — the fork, GEOM.CLIP's output → the shell's door

`zhao_geom_setup` had a hard-coded `16` at **twelve sites** across three ports and
three pipeline registers, with no parameter. It now takes
`parameter int unsigned IDW = 16`; the console passes `GEOM_TRI_IDENTW = 50`
carrying `{domain[1:0], v1 token[31:0], src_id[15:0]}`. Default 16, so every other
instantiation and bench is bit-for-bit unchanged.

An elaboration guard refuses `IDW < 16` rather than truncating the id GEOM.CLIP
handed over — the "truncate a handle" directive §4 forbids by name. It sits inside
`initial begin ... end` because Quartus 17.0 rejects a bare module-scope guard,
and **that costs it as evidence, which is stated at the guard**: `--lint-only`
does not run `initial` blocks, so the clean lint says nothing whatever about it.

**`zhao_geom_attrpack` was ALREADY parameterised on `IDW`** and uses `[IDW-1:0]`
at every site, so the other half of the fork also cost no RTL — only the number.
Checked at the file, not assumed.

### The fourth file, and the reason is ALIGNMENT

**`zhao_terrain_clipfeed.sv`.** The decision names three files. It is four, and
the extra one is not sloppiness in the decision — it is a fact about the terrain
arm that no document in this tree had stated:

`tcf_tri_mat_a_w`/`_mat_b_w`/`_weight_w` sit **upstream of a serial converter**.
`zhao_terrain_clipfeed` "accepts one triangle, converts its three corners, packs,
emits, and only then accepts another" (its own header). A token muxed at the door
from those wires would be whatever the projector was offering **by then** — a
different triangle. That is the metadata-swap shape exactly, and it is the one
place this carriage could have been built wrong while every gate passed.

So the block gains one opaque input and one output and latches the token **by the
same enable, in the same arm, as the corners and `mset_q`**. The composer encodes
it once with `zmt_encode`. Alignment is then correct by construction rather than
by an argument about two handshakes.

Counted by hand first, because `tests/shell/v3_closure_inherited.vlt` waives
UNUSEDSIGNAL across whole directories and a dead wire there raises nothing:
`_mat_b_w` and `_weight_w` had exactly two occurrences each (declaration, driver);
`_mat_a_w` had four, two of them comments. No reader, confirmed.

### The consumer, and the alignment at the far end

`base_rgb[23:8]` and `recipe_weight` were SPAN values — `MAT_BASE_RGB_C`, a named
constant white, and the window's published weight. They now come from the token
when **all three** hold, and each is load-bearing:

1. **`zmt_tag_ok`** — the encoding's own presence law. Every 24-bit pattern is a
   legal material state under `terrain_rules` 6.2, so only the tag separates a
   composed result from a lane that was never written.
2. **the producer domain is TERRAIN** — the decision asks for the mux to be "by
   the rider's domain", and requiring both makes a disagreement between the two
   fields a fault rather than a silent pick.
3. **`mw_pub_sample_count != 0`** — §5. This is the one that would have broken
   pixels.

Non-terrain arms present `32'd0`, tag `8'h00`, which `zmt_tag_ok` refuses. Not a
convenient zero: mesh, forge and particle primitives genuinely have no layer-E
composed triple, and the tag byte exists so absence is representable — *"an
absent output is NOT a write of zero."*

**The consumer end is aligned by the same register bank, checked rather than
argued.** `zhao_raster_tile_pipe_v2.sv:1456` loads `flat_request_q` in the same
`always_ff` arm and on the same enable as `source_id_q`, `area2_q` and
`job_first/last` — a per-triangle job load. So a per-triangle flat request is
latched with its triangle by construction; I did not have to make it so.

---

## 3. THE PER-TRIANGLE EQUALITY, AND ITS OWN VACUITY CHECK

`terrain_clipfeed_mat_directed` **section 7** — built and run: **41 checks, 0
failures, rc 0**.

Twelve triangles. Triangle *k* carries `src_id = 0x1000 + k` and
`token = zmt_encode(0x11 + 0x27k, 0x93 − 0x3Dk, 0x05 + 0x5Bk)` — three bytes at
three different rates with **matB moving downward**. The pair is asserted
**together, per triangle, against the producer**, under varying backpressure on
both the offer and the sink side.

**Why asymmetric values, and not a count of arrivals.** The fault class is
`u_geom_tidq`'s: a value that stays **in range** and **decodes cleanly** while
belonging to the previous triangle. A monotone or constant stimulus passes such a
defect, and so does a variety check on the consumer.

**The stimulus checks itself first.** Section 7's opening assertion is that
consecutive tokens **differ** — without it, every assertion after it would pass
under exactly the defect it exists to catch.

### The equality is SEEN TO FIRE, on a committed mutant

`tests/mutants/zhao_terrain_clipfeed_tokenskew_mutant.sv` — a **WRAPPER**, not a
copy, so there is no body to go stale; `mutant_copy_drift.py` still reports 78
copies, correctly not charging it. Its one change hands the production block the
**previous accepted triangle's** token: the tidq fault reproduced in this arm.

`terrain_clipfeed_tokenskew_mutant`, **polarity inverted** — **9 checks, rc 0**.
It asserts the precise **one-behind signature** (every triangle from the second on
received exactly its predecessor's token), not a mismatch total, because a total
would also be produced by a scrambled token and that is a much easier fault.

**And it asserts every other instrument in the block stays silent** —
`src_id_mismatch_o`, `mat_id_orphan_o`, `mat_backed_o` and the triangle/emitted
counts all read exactly as in the clean run. That is the half that makes the
control worth having: the token equality catches a fault nothing else in the block
can see.

**Negative control:** the same bench without `ZHAO_MUT_TOKEN_SKEW`
(`terrain_clipfeed_mat_directed`, 0 mismatches). Required because a `-D` that
never engaged fails **silently**; the selector is a plain object-like `ifdef`,
which `-D` does reach, and only the module NAME sits inside it so the two builds
cannot differ in anything else.

### The composed console's own check stopped being vacuous

The finding I would keep if I could keep one.

`a_attrpack_setup_same_triangle` differenced the two halves of the SETUP/ATTRPACK
fork, and **its own comment conceded it could not fail**:

> "STATED WITHOUT OVERCLAIMING: on a frame drawn from a single source every
> triangle carries the same `src_id`, so this cannot fail there and is not
> evidence about the join on such a frame."

Honest, and load-bearing: a detector that could not fire on the console's own
smoke. The compared field is now the widened identity whose middle 32 bits are the
material token — and **a material token varies per triangle on exactly such a
frame**, because each terrain triangle comes out of its own cell. The caveat is
struck in place with the correction stated.

Its two operands are `zhao_geom_setup`'s s3 register and `zhao_geom_attrpack`'s
`src_id_q` — **two modules, two enables** — so this is not the co-clocked shape
CLAUDE.md forbids quoting the silence of.

### And the arrival is proven in the COMPOSED CONSOLE, not only at a leaf

The console smoke's mosaic line, before and after the carriage:

```
before  mosaic fills[tileset/mesh/stray]=[23 21 0]  tile[max/or]=[0 0]  -> FATAL
after   mosaic fills[tileset/mesh/stray]=[23 21 0]  tile[max/or]=[6 7]  -> PASS
```

`tile_or = 7` is `1|2|5|6` — bits from **both** matA's authored range {1,2} and
matB's {5,6}, so both arms of `pick = p < weight ? mat_a : mat_b` were exercised,
per cell. **With the old constant `base_rgb` this line could only have read 255.**
The tile the binding resolver displaces by is now derived from the page's own
layer-E bytes, carried per triangle through clipdoor, clip, setup and the shell.
`raster pixels=2816`, `frames_admitted=1`, `stray=0`, fill counts unmoved.

---

## 4. THE `-MapOnly` ROWS

Four leaf rows, **`rtlCleanAtHead: true`** on every one, `measuredDevice`
**`5CSEBA6U23I7`** — the shell QSF's own part, the shipping part. Two pairs, each
differing in **exactly one parameter**, with an identical `sourceDigest` inside
each pair and `sourceCommit 8a4912d5`.

| row | ALUTs | registers | DSP | mem bits |
|---|---|---|---|---|
| `zhao_geom_setup@matcarry-idw16` | 500 | 1,340 | 4 | 0 |
| `zhao_geom_setup@matcarry-idw50` | 500 | 1,442 | 4 | 0 |
| **leg 2 delta** | **0** | **+102** | **0** | **0** |
| `zhao_geom_clip@matcarry-riderw50` | 1,456 | 3,276 | 2 | 0 |
| `zhao_geom_clip@matcarry-riderw82` | 1,456 | 3,372 | 2 | 0 |
| **leg 1 delta** | **0** | **+96** | **0** | **0** |

**TOTAL: 0 ALUTs, +198 registers, 0 DSP, 0 memory bits.**

`+96` is 32 bits × 3 clip stages and `+102` is 34 bits × 3 setup stages — the
structure's own prediction, so there is no surprise logic hiding in the widening.
**Zero combinational ALUTs on both legs**, because both fields are pure
carry-throughs: they cost flops and no logic. The decision declared itself
unpriced and expected "bits, not datapath"; it now is priced and that expectation
holds.

**I do not refuse on the number**, and the ground for that is the delta being
zero ALUTs rather than any claim about the console's headroom.

**A NUMBER I HAD QUOTED AND SHOULD NOT HAVE, corrected here rather than left
standing.** An earlier draft of this section said "the console needs to shed
209,532 ALUTs". That figure is DERIVED against the shipping part, and the
handover labels that column **indicative**, not device-exact: Quartus replaces
multipliers a small part cannot hold, so DSP reads low and ALUTs high. The
device-exact fact is the fitter's own sentence on the console snapshot row --

    Error (170011): Design contains 293352 blocks of type combinational node.
    However, the device contains only 227120 blocks.

-- and that row is `incomplete:failed`, so it produced **no ALM and no Fmax at
all**. The honest statement is therefore narrow: this widening's ALUT delta is
**zero, measured**, so it cannot be the thing that decides the console's fit
either way, and I make no claim about how much the console must shed.

I corrected this after the coordinator relayed that DOORCOST measured I55's
raster door at **28 wires against the 2,065 six packets had inherited** -- an
estimate 74x off in the ALARMING direction. That is the opposite polarity from
the broken-instrument law's usual one, and the lesson it carries is not about
I55: it is that a number which has been quoted down a chain of packets is not a
measurement, however many times it has been repeated. The 209,532 above was
exactly such a number, arriving in my own findings.

Three things declared rather than left to be re-derived:

* **`-Device` was deliberately OMITTED.** The QSF's part *is* the shipping part
  and the runner records it as `measuredDevice` when the flag is absent
  (`run_block_fit.ps1:882`). Passing it explicitly ALSO stamps
  `notTargetDevice = $true` and a sizingNote claiming the row is not the target,
  **unconditionally, without comparing the value** (`:884-887`) — which would be
  false on these rows. So the device is named here and in the receipt rather than
  flagged wrongly.
* **`treeCleanAtHead` is false and that is correct.** Only `tests/` was dirty when
  the maps ran; no RTL was. That is the distinction the two fields exist for, and
  `rtlCleanAtHead` is the one to read.
* **Only the DELTAS are quoted.** The absolute numbers are leaf maps at module
  defaults except the one parameter, and include virtual pins; a map row carries
  no ALMs and no Fmax by design. Both rows in each pair share device, digest and
  closure, so the differences are sound and the absolutes are not a budget.

---

## 5. THE STALE "ZERO HITS" CLAIM, STRUCK IN PLACE AT THREE SITES

(M8) announced this correction in entry `I34`'s newest block. Right, and **not
enough**: the three places a reader actually *meets* the claim still asserted it,
one of them with the words *"re-measured at this tree"* and its own positive
control. Struck in place, with the correction and the new measurement beside it,
at all three sites in `zhao_console_core.sv` — the 2026-09-22 obligation 2, the
trimerge discharge of obligation 2, and the composer's comment on
`.c_material_mode_i`.

```
material_set|material_id under fpga/rtl/terrain/   -> 4 code hits
    zhao_terrain_clipfeed.sv:391  output o_material_set_o
    zhao_terrain_clipfeed.sv:392  output o_material_id_o
    zhao_terrain_clipfeed.sv:810  assign o_material_set_o = mset_q;
    zhao_terrain_clipfeed.sv:811  assign o_material_id_o  = mid_q;
POSITIVE CONTROL: `material` in the same directory -> 72 hits across 9 files
```

The zero was **true when taken**; TERRAINMAT landed after it. A document cannot go
stale loudly, and this one carried its own control, which is what made it
persuasive for four refusals.

### The second half of that claim was false in the OTHER direction, and it matters more

The same sentence said MATMODE_NONE *"is the only mode terrain can lawfully
present"*. **It is not.** `zhao_terrain_clipfeed.sv:812` derives
`(mset_q != 32'd0) ? MATMODE_BACKED_C : MATMODE_NONE_C`, and `mat_set_i`/`mat_id_i`
are real producers — host-authored `SetEnvironment.terrain_material_set`, decoded
by CMD.EXEC, latched at `zhao_console_core.sv:18004-18012`.

**Load-bearing, not a tidy-up.** `base_rgb` is the published **texel RGB** when
`sample_count == 0`, and `recipe_weight` is the RECIPE_LERP blend weight; a terrain
span reaches a non-zero `sample_count` only on the BACKED arm. A reader who
believed "NONE is the only mode" would conclude this carriage can never be
consumed — and a packet that substituted the token unconditionally would have
**repainted the surface with two material indices as colour channels**. That is
why the third gate condition exists, and I nearly shipped without it (§8).

---

## 6. VELOCITY STILL REACHES ITS CONSUMER

`terrain_veljoin_directed` — **19 checks, rc 0**, on the REPAIRED test. It printed
a hardcoded `0 failures` with `exit_hard(0)` until MATERIALPATH fixed it, so its
green is now a measurement rather than a literal. Re-run and believed, as the
fence asks.

Also still green and unchanged by this packet:

* `composepub_acceptance` — **154 checks, 0 failures** (the owner's acceptance
  test: a Field material write changes the consumer's served triple).
* `terrain_matjoin_directed` — **50 checks**, 600 random vertices differenced
  against `compose_material`, 247 enabled and 51 refused.

I did not touch nav, did not build a `zhao_terrain_patch_v2`, and did not raise
`FAB_LANES`, `FAB_GROUP_PTS` or `FRONT_PTS`. All four fences held.

---

## 7. CLAIMS I FOUND FALSE

1. **The decision and brief say the clipdoor→clip leg "touches three files".** It
   touches **one** — the composer. `zhao_geom_clipdoor` and `zhao_geom_clip` are
   already parameterised on `RIDERW` and never read inside the rider.
2. **The decision says the carriage is three files.** It is **four**:
   `zhao_terrain_clipfeed` must carry the token, because the terrain arm's triple
   is upstream of a serial converter and is not at the door. Neither the brief nor
   the decision states this, and it is the one place the carriage could have been
   built wrong while every gate passed.
3. **The "ZERO hits" claim, at three sites.** §5. Four code hits, control
   verified at 72 hits / 9 files.
4. **"MATMODE_NONE is the only mode terrain can lawfully present."** False —
   `:812` derives BACKED from the host's environment. §5.
5. **Both `.*` wrapper mutants carried `GEOM_VID_RIDERW = 16 + 32 + 2`** and would
   have instantiated the core with a 50-bit rider while the composer slices
   `[81:50]`. The brief warns a new core PORT costs both wrappers; **a changed
   parameter VALUE costs them too**, and nothing in the gate set says so until a
   mutant smoke form runs. Both updated; `wrapper_port_parity.py` 1611 = 1611,
   stale 0.
6. **THE CONSOLE SMOKE NEVER AUTHORED LAYER E.** Bytes [2242, 21320) of every
   terrain page were the zero fill from `tb_zhao_console_core_smoke.sv:5071`, so
   all 1,024 layer-E cells read `{matA 0, matB 0, weight 0}`. Two instruments read
   right for the wrong reason and hid it:
   * `terrmat mat_cells=1024` asserts the fill is COMPLETE and was true
     throughout — 1,024 cells really were written, with zero. It counts **cells,
     not values**.
   * the mosaic's own anti-vacuity check `tsfill_tile_max_q != 0` passed because
     `base_rgb` was the constant `24'hFF_FF_FF`, so matA and matB reached the pick
     as `8'hFF` **regardless of the page** and every fill landed in tile 255. **A
     composer constant was standing in for the terrain's material and the check
     could not tell.**
   Authoring the plane makes that assertion **strictly stronger**: it stops being
   satisfiable by a composer-side constant and starts requiring the page's own
   bytes to traverse the whole chain. Values are named editable constants, matA
   and matB in disjoint ranges so the picked tile says which arm of the pick was
   taken, weight varying per cell. Fill counts did not move, `stray` stayed 0,
   `raster pixels` stayed 2,816.
7. **`mat_refused_o` on the projector is not about the material triple.** It counts
   refused writes to the projection **matrix** configuration
   (`zhao_project_core.sv:604-606`). I checked it because a material-validity
   signal would have changed how the token is tagged; "mat" is matrix there.
8. **`tcf_tri_mat_*_w` "have EXACTLY TWO occurrences"** — true of `_mat_b_w` and
   `_weight_w`; `_mat_a_w` has four, two of them comments. The substance (no
   reader) is correct and I confirmed it by hand.
9. **PACKET-PROTOCOL's shared-index warning does not apply between worktrees.**
   Each worktree has its own index — mine is `.git/worktrees/gz-matcarry/index`,
   the coordinator's is `.git/index` — so `git add <file>` here cannot sweep in
   another packet's staging or working tree. The rule is right about a shared
   *checkout*, which is where it was learned. I reviewed every diff before
   committing anyway, which is the substance of it.

---

## 8. WHAT I REFUSED, AND WHAT I GOT WRONG AND CAUGHT MYSELF

### What I got wrong

**I nearly substituted the token unconditionally, and it would have broken
pixels.** My first design muxed `base_rgb[23:8]` and `recipe_weight` on the
token's tag alone. What stopped me was the handover's CARRIAGE section
(2026-09-26): *"wiring it would have been HARMFUL, not merely useless. `base_rgb`
becomes the published texel RGB at `sample_count == 0`, and `recipe_weight` is the
blend weight under `R_LERP`. Both are shut today only by coincidence."* That is in
direct tension with my brief's claim that the carriage to the mosaic is live, and
**the brief is newer**, so the pull was to prefer it. Both are true: the transport
is live *and* the fields are overloaded. The third gate condition is that
correction, and without the older document I would have shipped the fault.

**I asserted the wrong mutant count.** I first wrote `mismatches == kN - 1`,
reasoning triangle 0 would match. It does not: `skew_q` resets to zero, so
triangle 0 receives `32'd0` and mismatches too. Corrected to `kN`, with the
one-behind signature asserted separately so the arithmetic is not hidden inside
one number.

**I wrote an `ifdef` that split a parameter list across its branches** — the first
version of the DUT selector put `.ATTRS (7),` in both arms and the other eight
parameters in only the `else`. Caught by reading it back before compiling.

**Four line-ending and indentation failures**, every one caught by the edit
scripts' `once()` guard rather than by producing a wrong edit. The geometry files
are CRLF and `zhao_console_core.sv` is LF; each patch script translates its
patterns to the file's own ending, so a line-ending mismatch cannot read as an
absent pattern.

**I hit the heredoc trap twice** — `bash -c` with quote-heavy Python — which
PACKET-PROTOCOL warns about explicitly. Both times it failed loudly and cost only
a retry, and I switched to writing scripts to files.

### What I refused

**I did not add a synthesizable core counter for the token.** The natural one —
"the token at the shell's door disagrees with the token upstream" — needs a
second, independently-clocked copy to difference, and the copy that exists is
`zhao_geom_attrpack`'s, which the **composed assertion already differences**.
Adding a port would have cost the port, both wrapper mutants, the smoke's wire and
a reader, to restate in silicon what a live simulation assertion, a committed
mutant and the smoke's own mosaic readout already establish. I state the boundary
plainly: the per-triangle evidence is a **simulation** assertion, a **leaf**
directed test, a **committed mutant** and a **console-level tile readout** — not a
shipped counter.

**I did not touch `zhao_geom_bin_pipe_v2`, `zhao_raster_tile_pipe_v2` or
`zhao_geom_binner_v2`** (DOORCOST's files), and no file under `fpga/rtl/texture/`
or `fpga/rtl/raster/`. Verified in both my commits and my working tree. The
decision's claim that none is needed held: I read
`zhao_raster_tile_pipe_v2.sv:760-781` and
`zhao_texture_island_v3_top.sv:1314-1316` and changed neither.

---

## 9. BRANCH AND COMMITS

**`gz/matcarry`**, pushed. Never `--force`, never `--force-with-lease`.

| commit | what |
|---|---|
| `d855b914` | `zhao_geom_setup`'s `IDW` parameter — leg 2's leaf |
| `8a4912d5` | the carriage: rider 50→82, the fork, the clipfeed's token, the consumer mux, three struck claims, both generated artifacts regenerated |
| `2b5cc974` | the committed mutant and its inverted driver, the layer-E fixture the carriage exposed, both wrapper-mutant rider widths |

### GATE TABLE AT THE PUSHED HEAD

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** (I34, I55), RC 1 — no higher than at start |
| console smoke, plain | **PASS**, `raster pixels=2816`, `frames_admitted=1` |
| `terrain_clipfeed_mat_directed` | 41 checks, 0 failures (negative control) |
| `terrain_clipfeed_tokenskew_mutant` | 9 checks, 0 failures (inverted polarity) |
| `terrain_veljoin_directed` | 19 checks — the repaired test |
| `composepub_acceptance` | 154 checks, 0 failures |
| `terrain_matjoin_directed` | 50 checks |
| `check_console_closure_lint.py` (**gate 31**) | OK, 296 sources, self-test fired 5/5 |
| `check_quartus17_syntax.py` | RC 0, self-test 13 fire / 22 no-fire |
| `check_console_inventory.py` | OK |
| `check_prod_manifest.py` | OK |
| `wrapper_port_parity.py` | OK, 1611 = 1611, stale 0 |
| `gen_prod_top.py --check` | fresh (regenerated, 88 instances) |
| `gen_console_board.py --check` | fresh (regenerated, 1616 ports / 127 params) |
| `gen_shell_paired_diff.py --check` | fresh |
| `check_case_labels.py` | OK |
| `mutant_copy_drift.py` | OK, **run after the commit** (R121), 78 copies |
| leaf `-MapOnly` | 4 rows, `rtlCleanAtHead` true, `5CSEBA6U23I7`, 0 ALUTs / +198 reg |

`npm run abi:check` not run: `spec/commands.zidl` untouched.

**One gate row I could not clear and it is not mine.** The console-board lint
(`tests/prod/run_console_board_lint.ps1`) is recorded in PACKET-PROTOCOL as an
**inherited red** at this tree — exit 1 with `262 warning(s)`, DECLFILENAME and
PINCONNECTEMPTY under `-Wall`, identical at `f4b4a653` and `gz/terrainvisible`.
The protocol's own row for it says so.
