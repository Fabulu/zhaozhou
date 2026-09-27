# FINDINGS — MATERIALPATH, 2026-09-27

**Branch `gz/materialpath`.** Base `62d8b6a7`.
Commits: `73980d41` (the encoding), `36f260fb` (TERRAIN.MATJOIN + composition),
plus the head carrying the corrections in §8 and this file.

Answers the brief's nine numbered deliverables in order.

---

## 1. THE REGISTER, MEASURED BARE, AND WHETHER I34 CLOSED

| | value |
|---|---|
| before (`62d8b6a7`) | **2** — `I34`, `I55`. RC 1 |
| after | **2** — `I34`, `I55`. RC 1 |

Both measured with `python tools/budget/completion_register.py` **bare**, exit
code read from the command itself and not from a pipeline. (The brief warns
about `| tail`; my very first measurement in this packet hit it — see §8.3.)

**`I34` DID NOT CLOSE, and it must not be reported as closing.** What closed is
its material *channel* up to the compose cache; what remains is a reader past
`tcf_tri_mat_*_w`, which is `I13`'s seam. §7 states the refusal with the
measurement behind it.

What *did* move, and it is a real movement rather than a reclassification:
**mandatory RTL capabilities connected in the console went 112 → 113**, with
`BUILT BUT NOT CONNECTED`, `NOT BUILT`, `UNRESOLVABLE` and `EXCUSED BY AN
UNCITED FLAG` all still zero. The new capability is a real producer driving a
real implementation feeding a real consumer, so it registers as connected rather
than as a new gap.

---

## 2. THE ENCODING

**Where it lives — one law, three views:**

* `spec/qformats.md` **§14** — the prose. It is literally the two anchors
  `design/ops.yml` has cited since before they existed: **`material-ids`** and
  **`material-state`**, plus §14.1's token layout. `(P6)(a)` of entry `I34`
  asked for exactly this.
* `fpga/rtl/common/zhao_material_token_pkg.sv` — `zmt_encode`, `zmt_tag_ok`,
  `zmt_mat_a/_b`, `zmt_weight`, `zmt_triple`.
* `reference/include/zref/zref_fieldir.hpp` — `material_token_encode`,
  `material_token_tag_ok`, `material_token_decode`, beside the **untouched**
  `compose_material`, which remains the law and still speaks triples. The token
  is the transport.

```
 31        24 23        16 15         8 7          0
+------------+------------+------------+------------+
|  TAG 0xE1  |    matA    |    matB    |   weight   |
+------------+------------+------------+------------+
```

**How it is versioned.** The top byte carries the version **in band**. This is
the vacation directive's *"versioned extension"* rather than stolen bits: no
existing field is overloaded, no handle truncated, and nothing that decodes
today changes. **No `QFMT_VERSION` bump**, checked against §13's own terms
rather than asserted — no type's width or format changes, no rounding or
saturation law, no frozen table, no golden layout, no fixgen output. The 24-bit
triple is transcribed from shipping blocks and the 32-bit token was previously
unspecified in every bit, so there is nothing to be incompatible with. An
in-band tag is also strictly stronger than a global counter here, because a
decoder can refuse a value it does not understand and `QFMT_VERSION` cannot.

**Why the tag is not padding.** `spec/terrain_rules.md` §6.2 gives weight 0 and
255 meanings, so **every one of the 2^24 triples is a legal material state**. A
decoder without a tag cannot tell a real result from a lane that was never
written — and `32'd0`, which the adapter parks on an absent lane
(`zhao_field_earth_adapter.sv:1407`), would decode as the perfectly legal triple
`{0,0,0}` and render. The ratified rule is that *an absent output is not a write
of zero*, and the tag is what makes honouring it possible. A refused token is
**counted, never substituted**.

**The test that exercises it both ways with an asymmetric value.**
`tests/terrain/terrain_matjoin_directed.cpp` §0 and §6:

* encode → a literal (`0xE12A7CB3`), decode → the triple, with
  **`matA=0x2A, matB=0x7C, weight=0xB3`** — three distinct bytes, so a swapped,
  rotated or truncated layout cannot pass;
* **a negative control on the asymmetry claim itself**: a byte-**rotated** token
  is asserted to decode *differently*, which is the test proving it could tell.
  Without that, "the asymmetric value protects us" is an assertion about the
  test rather than a property of it;
* token `0` is asserted **refused**, and asserted to leave the caller's state
  untouched;
* a legal-looking triple under a wrong tag (`0x002A7CB3`) is asserted refused;
* then 600 pseudo-random vertices drive the RTL decode against the reference.

**The low 24 bits were transcribed, not chosen**, and that is the finding that
made the encoding cheap. Three composed sites already commit to
`{matA[23:16], matB[15:8], weight[7:0]}` and were read at the line first:

| site | evidence |
|---|---|
| `zhao_terrain_compcache_front.sv` | `:587` packs `{mat_w_a_i, mat_w_b_i, mat_w_weight_i}`; `:701-703` unpacks `[23:16]/[15:8]/[7:0]` |
| `zhao_terrain_project.sv` | `:177-181` — the 42-bit rider payload, `[23:16] mat_a [15:8] mat_b [7:0] weight` |
| `zhao_texture_island_v3_top.sv` | `:1314-1315` — the mosaic's candidates from `base_rgb[23:16]`/`[15:8]` |

Inventing a different byte order would have been a rival statement of a settled
layout. See §6.3 for the brief's claim this corrects.

---

## 3. THE ARBITRATION RULE, AND THE TEST THAT REACHES IT

**The rule is `zref::fieldir::compose_material`, which is ratified: start from
authored layer E, and the LAST ENABLED WRITER WINS, in accepted command order.**
Nothing was invented. The reference header gives the reason the *order* is the
priority: *"hardware inventing an implicit material hierarchy would be a game
rule smuggled into silicon, and software can already express any precedence it
wants by choosing the order it submits."*

A lane word is **enabled** on three independent terms — `f_covers_i`
(TERRAIN.PATCH's §9.1 answer, taken and never re-decided), `f_present_i` (the
record's ordinal-2 presence), and a v1 tag.

**"What happens when both arrive in the same cycle"** — the brief's question —
has a structural answer: the authored face and the lane face are **mutually
exclusive by construction**. `zhao_terrain_patch.sv:313-314` gives
`vtx_ready_o = !busy && out_free` and `fld_ready_o = busy`, so on the accept
cycle `busy` is still low and no lane word can be offered. The block does not
rely on that anyway: a word arriving in the same cycle as the emit is folded in
combinationally, so the answer is independent of another block's handshake —
which is exactly the kind of thing this file must not quietly depend on.

### AND THE BRIEF ASKED FOR THE ARBITRATION IN THE WRONG PLACE

The brief says to find *"the second writer and its arbitration"* on
`compcache_front`'s material write face. **A second writer into the authored
plane is the specific thing the architecture forbids**, and the sentence is in
the reference the brief itself points at — `zref_fieldir.hpp`'s sinks header:

> *"ALL THREE ARE LIVE COMPOSITION, NEVER PERSISTENT MUTATION. … a field
> evaluated every frame must never rewrite a VRAM page every frame, which is the
> failure this separation exists to prevent."*

So the arbitration belongs where **height's already is**: at the compose point,
once per frame, leaving the authored value untouched. TERRAIN.COMPCACHE is the
per-frame compose cache — it is already where TERRAIN.PATCH deposits
field-composed *heights* — so composing material into the same plane is
**symmetric with height** rather than a mutation of anything.

**The test that reaches the rule** is `terrain_matjoin_directed` §4: the same
two words presented in **both orders**, asserting the answer follows the order;
plus a later word that is *disabled* and a later word that is *refused*, neither
of which may displace an earlier enabled one — which is where "last writer" and
"last **enabled** writer" actually differ. Each order is also differenced
against `compose_material` on the same lane stream.

---

## 4. THE OWNER'S TEST — A FIELD MATERIAL WRITE CHANGING THE INTENDED CONSUMER

> *"Material continues separately through its real Field-to-material path. Do
> not close that half merely because authored terrain materials render; verify
> that a Field material write changes the intended consumer."*

**Met, and measured at the CONSUMER rather than at the producer.**
`tests/terrain/composepub_acceptance.cpp` **case 12**, through the real
`zhao_field_earth_adapter` — the same bench whose header said, in advance, that
it exported `efa_material_o` because *"this bench is where a later packet's
material/nav reducer gets its first observation."*

The bench's layer-E plane had been tied to zero with the note *"TEXMAT's seam,
not this bench's subject"*. It is un-tied on **both** faces, exactly the way
TERRVEL un-tied the velocity plane immediately above it, and the triple is read
back through **TERRAIN.COMPCACHE's own serve port at its own parity** — never
the join's output wire, which would only prove a value was produced.

```
composepub_acceptance: 154 checks, 0 failures        (was 122)

  12a  no field           authored triple at (5,9) = {9D,EA,B4}
  12b  live Earth field   SERVED triple at (5,9)    = {2A,7C,B3}
                          field_composed=1024  lane_no_cell=65
  12c  wrong-tag token    refused on 1024 cells, authored triple kept
  12d  ordinal-2 ABSENT   authored survived, nothing composed
```

Four things make this evidence rather than a green tick:

* **12a runs first**, because *"the consumer reads X"* is only evidence if the
  consumer did **not** read X before. The authored triple is asserted to pass
  through untouched with no field.
* the change is asserted to be **a change** — `12b` separately asserts the
  authored and field triples differ, so the case cannot pass vacuously if the
  fixture ever hands back the same bytes;
* **`field_composed_o == 1024`**, i.e. every cell of a whole-patch footprint,
  not one lucky cell;
* **the height lane is asserted unmoved** — out-lane 0 is zero in this case, so
  a material write that moved the ground would fail here.

`terrain_matjoin_directed` adds 50 checks at the block's ports, including the
600-vertex differential against `compose_material` with **247 enabled and 51
refused words asserted** — so the sweep is shown to have *reached* both paths
instead of passing trivially.

**And the test itself was fire-tested**, because a test that cannot go red is
the shape this campaign keeps finding. A planted `ck(false, …)` returns **rc 1**
and prints `1/51 checks FAILED`; removed, it is back to `50 checks passed`.

---

## 5. WHAT HAPPENED TO VELOCITY'S CHAIN

**Not regressed, and demonstrated rather than asserted.**

* `terrain_veljoin_directed` — **19 checks passed, rc 0**, with both fired
  controls intact (`vtx_mismatch=988 against control 0`, `sweeps_aborted=1`).
* the console smoke's velocity assertions are untouched and still pass.
* `FAB_LANES`, `FAB_GROUP_PTS` and `FRONT_PTS` were **not** raised;
  `u_field_host` keeps `.FAB_LANES(1)` and the scalar front.
* `nav_cost_o` was not touched. `efa_nav_cost` stays produced and classified.

**But the instrument that reports velocity's health was broken, and my brief
quoted it.** See §9 — this is the most important thing in this section.

The material join takes its lane beat from the *same* fork TERRVEL built
(`tvj_p_valid && tvj_p_ready`) and reads `terr_pt_fld_covers_o` rather than
re-deciding §9.1, so it adds no second walk and no second footprint law.

---

## 6. CLAIMS IN THE BRIEF OR THE ENTRY THAT I FOUND FALSE

**Six, and the first two matter most.**

### 6.1 `zhao_terrain_clipfeed` HAS a material input port, and its outputs are NOT constants

The brief states:

> *"**`zhao_terrain_clipfeed` HAS NO MATERIAL INPUT PORT AT ALL.** Its material
> outputs are the GEOM `{set, id, mode}` trio, driven **from constants** at that
> file's `:695-697`."*

**Both halves are false at this tree.** Measured:

* `fpga/rtl/terrain/zhao_terrain_clipfeed.sv:354-355` —
  `input var logic [31:0] mat_set_i,` / `input var logic [15:0] mat_id_i,`
  (TERRAINMAT, 2026-09-26; the host's `SetEnvironment.terrain_material_set`/`_id`
  decoded by CMD.EXEC and forked by the composer);
* `:810-812` — `o_material_set_o = mset_q`, `o_material_id_o = mid_q`, and the
  **mode is derived** (`(mset_q != 0) ? MATMODE_BACKED_C : MATMODE_NONE_C`).
  Registers, not constants;
* `:695-697` is a comment about **tint** ports.

The brief inherited a measurement taken before TERRAINMAT landed.

### 6.2 The blocker four refusals in `I34` rest on is RETIRED

`zhao_console_core.sv` carries, in three separate blocks, the sentence that
`material_set`/`material_id` *"under `fpga/rtl/terrain/` return ZERO hits"*,
re-measured each time as the reason the rider route cannot be laid. **It is four
code hits today**, all in `zhao_terrain_clipfeed.sv` (`:391`, `:392`, `:810`,
`:811`), with `material` at 8 files in the same directory as the positive
control. Terrain **does** have a material identity now.

This does not by itself open the rider route — §7 names the obstacle that
actually binds — but the *stated cause* of four inherited refusals is no longer
true, which is precisely what `A REFUSAL IS AN INSTRUMENT` says to check.

### 6.3 "Nothing in this tree packs three u8s into that u32 and nothing unpacks it. No encode, no decode, no table, no bit layout."

**Exactly true of the 32-bit form; too strong about the 24-bit one.** There is
no `u32` law — that part re-measures true and is what I built. But there *is* a
committed `{matA, matB, weight}` **bit layout**, in three independent composed
places (§2). The distinction is not pedantic: it is the difference between
authoring a layout and *transcribing* one, and had I taken the sentence at face
value I would have been free to pick a byte order and would have created a
fourth, rival convention.

### 6.4 The `tcf_tri_mat_*_w` line numbers have moved

The brief gives the declaration at `:19568` and the projector's write at
`:19705-19707`. At this tree they are **`:21511`** and **`:21648-21650`** — the
blocks added to entry `I34` since shifted them ~1,940 lines. **The substance
holds**: exactly two occurrences each outside comments, and **no reader**. I
re-counted by hand, because `tests/shell/v3_closure_inherited.vlt` waives
`UNUSEDSIGNAL` across whole directories and the linter cannot see it.

### 6.5 The composepub bench is not where the brief says

The brief and the campaign's notes point at `tests/prod/tb_terrain_composepub.sv`.
It is **`tests/terrain/tb_terrain_composepub.sv`**; `tests/prod/` holds no
composepub bench.

### 6.6 The arbitration was asked for in the wrong place

Covered in §3. The brief asks for a second writer and its arbitration on
`compcache_front`'s write face; that is the persistent mutation the reference
header forbids by name.

---

## 7. WHAT I REFUSED

**One thing: carrying the composed triple past `tcf_tri_mat_*_w` to the mosaic.**
Asked as DECISION-or-BUILD, per `CLAUDE.md`: it is a **BUILD**, the decision is
not open, and what I am refusing is **doing it in this packet and in this lane**.
The measurement, so the next packet inherits numbers rather than a verdict:

* **The 32 bits already have complete, live carriage.** `base_rgb[23:8]` plus
  `recipe_weight` ride `tri_flat_request_c` per triangle from `zhao_console_core`
  into the shell, through `zhao_geom_binner_v2`'s metadata bank
  (`zhao_geom_bin_pipe_v2.sv:333`, `zhao_geom_binner_v2.sv:572/:832`), are
  decoded at `zhao_raster_tile_pipe_v2.sv:770-781` (`base_rgb = [43:20]`,
  `recipe_weight = [276:269]`) and reach `zhao_texture_mosaic_v2`.
  **No file under `fpga/rtl/texture/` or `fpga/rtl/raster/` needs to change.**
* **What is missing is a per-triangle SOURCE.** `MAT_BASE_RGB_C` is a named
  constant white (`zhao_console_core.sv:31213`) and `mw_pub_recipe_weight` is
  per **span**, latched once per material-window publication.
* **The drain price is NOT owed**, and this kills the objection that made the
  route look unaffordable. `zhao_material_window.sv:486-492`'s `match_c` has
  five terms — mode, vertex alpha, detail, frag state, material set, material id
  — and **neither `base_rgb` nor `recipe_weight` is one of them**. A per-triangle
  triple muxed into those bits costs **zero drains**.
* **The obstacle that actually binds is ALIGNMENT AT THE SHELL'S DOOR, and no
  document in this tree had named it.** `tri_flat_request_c` escapes alignment
  only because it is a **level held constant for a whole span** — the file says
  so itself at `:31256-31261`. A *per-triangle* value has no such excuse: it
  must stay aligned with its triangle through GEOM.SETUP and GEOM.ATTRPACK, and
  the only per-triangle sideband that survives that trip today is a **16-bit
  source id** (`zhao_geom_setup.sv:150`, `:179`, `:260` — hard-coded 16 bits,
  no parameter).
* `GEOM_VID_RIDERW` is `16 + 32 + 2 = 50` and **all fifty are allocated**
  (`zhao_console_core.sv:11339`, `:18564`, `:18576-18579`, `:20189`,
  `:20206-20207`). The 30 bits written as zero on three of the four client arms
  are R28's `raster_state`, **allocated and not spare**.

**Why not here.** The two candidate carriers are `zhao_geom_setup.sv` (widen the
setup rider) or a console-local aligned FIFO; which is correct is an engineering
choice, not a fact in the tree. Both land in **GEOM**, which is `SWAPBUILD`'s
lane in this run, and the seam is **`I13`'s**, not `I34`'s. Laying it from here
would mean editing files another live packet owns, to close an entry that is not
mine, on a design choice that deserves its own brief.

**I also did not touch nav** beyond leaving it produced and classified, and did
not build a `zhao_terrain_patch_v2`. Both were fenced, both fences held.

---

## 8. WHAT I GOT WRONG AND CAUGHT MYSELF

**Four. The first cost real work and is the one worth reading.**

### 8.1 I asserted a handshake ordering that is false, and my own guard caught it

The join's header claimed *"accept(V+1) is strictly after the state publish of
V"*, and I emitted the composed write on the state publish on that basis. **It
is false.** `zhao_terrain_patch.sv:285` reads `out_free = !r_valid ||
st_ready_i`, so `vtx_ready_o` rises on the **same cycle** the held record
retires — an accept and a publish coincide routinely; it is the steady state.

`held_overrun_o` therefore fired **992 times per patch** in composepub and
**10,239 times** in the console smoke, on a perfectly correct walk.

Three things about this are worth keeping:

* **the data was never wrong.** The emit is lossless on that path, so `12b`
  produced the correct triple and `field_composed=1024` throughout. Only the
  *alarm* was wrong;
* **I reached the false sentence by confident reasoning about RTL I had read**,
  which is `CLAUDE.md`'s "the confident one-line summary is where the error
  lives", with me as the author;
* **it was found by RUNNING the thing**, not by re-reading. The block's lint was
  clean, its directed test was green at 50 checks, and both said nothing —
  because a directed test drives the handshake the author imagined. The console
  case drove the real patch.

Repaired: the guard carries `!st_fire_i`, which is the genuinely unreachable
fault (an accept needs `!r_valid || st_ready_i`, so either the record is gone
and this block has already emitted, or it is retiring this very cycle). The
corrected paragraph is in the block header and in the contract, stated as a
correction rather than silently rewritten.

### 8.2 `token_refused_o` double-counted

It fired for lane words belonging to the 65 cell-less lattice vertices — 1,089
instead of 1,024 — so it did not partition cleanly against `field_composed_o`.
Caught by case 12c. Now gated on a held cell; the cell-less words are counted
once, by `lane_no_cell_o`.

### 8.3 I hit the `| tail` trap the brief warns about, on my first measurement

`python tools/budget/completion_register.py 2>&1 | tail -30; echo $?` printed
`REGISTER_RC=0` — **tail's** exit code. The register's own rc is 1 while gaps
remain. The number was right; the rc was a measurement of the wrong process.
Every register reading in this file is bare.

### 8.4 I wrote a script and did not run it

I wrote the `spec/qformats.md` §14 append to disk, was interrupted by a returning
agent, and staged the commit without it. The tell was `git status` showing no
change to a file I "had just written" — the same tell `CLAUDE.md` records for the
`[IO.File]` trap, arriving from the opposite direction. Caught before the commit
by reading `git diff --cached --name-only`, which the shared-index chapter says
to do before every commit and which earned its place here for a different reason
entirely.

---

## 9. INSTRUMENT DEFECT FOUND, AND ITS GUARD

**`tests/terrain/terrain_veljoin_directed.cpp` could not go red.** Its tail was:

```cpp
std::printf("terrain_veljoin_directed: %d checks, 0 failures\n", checks);
zhao::exit_hard(0);
```

`zhao::check` does **not** abort — it increments `g_failures` and prints a
`FAIL:` line (`tests/harness/zhao_sim.cpp:55-63`). `zhao::exit_hard(0)` calls
`std::_Exit(0)` **unconditionally**. And `0 failures` is a **hardcoded string**.
So that test printed a clean verdict and returned **success** with checks
failing, and the only trace was a line in stdout that ctest never reads.

**It sits on the block guarding entry `I34`'s velocity chain, and my own brief
quotes it as evidence** — *"it reaches `zhao_part_collide` today —
`terrain_veljoin_directed` 19/0"*. That `19/0` was a literal, not a measurement.

Repaired to `zhao::report_and_exit`. **It passes honestly — 19 checks, rc 0 —
so nothing was being hidden; but nothing was being watched either.** A tree-wide
sweep (`exit_hard(0)` plus a hardcoded `0 failures` and no `report_and_exit` or
`check_failures`) found this was the **only** instance, so it is a defect and
not a class.

---

## GATES AT THE PUSHED HEAD

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** — `I34`, `I55`. rc 1, unchanged from base |
| `check_console_inventory.py` | OK — 407 declared, 287 elaborated, 296 fit sources |
| `check_prod_manifest.py` | OK. It went **red on exactly one error** — my unaccounted module — which is how I know it was green at base |
| `gen_prod_top.py --check` | fresh, 88 instances |
| `gen_console_board.py --check` | fresh, 1,613 core ports, 127 parameters (regenerated for the three new counters) |
| `gen_shell_paired_diff.py --check` | fresh, harness and mutant |
| `check_quartus17_syntax.py` | rc 0 over 666 files |
| `check_case_labels.py` | OK |
| `mutant_copy_drift.py` | OK — 78 copies, run **after** the commit per ruling R121 |
| `terrain_matjoin_directed` | **50 checks, 0 failures**, fire-tested |
| `composepub_acceptance` | **154 checks, 0 failures** (was 122) |
| `terrain_veljoin_directed` | **19 checks, 0 failures**, now with a real verdict |
| `lint_terrain_matjoin` | rc 0 under bare `--lint-only -Wall`, **no waiver file** |

**On the lint.** `tests/shell/v3_closure_inherited.vlt` waives `UNUSEDSIGNAL`
across whole directories including `fpga/rtl/common`, so the new package's
accessors would have been silenced by a blanket nobody chose. They carry a
**narrow, reasoned pragma** instead and the pair lints clean with no waiver file
at all. `zhao_texture_ident_pkg`'s identical accessors are covered by the
blanket today; this is the standard the campaign is trying to get back to.

---

## 10. A PRE-EXISTING DEFECT IN THE AUTHORED LAYER-E PATH, FOUND AND REPAIRED

This is not a material-*channel* defect and has nothing to do with fields. It
has been live in the composed console, and **no test looked at it.** It is
reported separately from §6 because it is a defect in the tree rather than a
false claim in a document.

### The measurement

The console smoke streams **eleven** lattices and **places one**:

```
ps_lattices=11  ps_vertices=11979  ps_cells=11264
place_patches=1  pt_samples=1089  cc_records=1089
mat_cells=8192
```

`zhao_terrain_compcache_front.sv:729-733` states both the contract and why the
counter exists:

> *"`mat_cells_o` is NOT a tautology of the write enable and is the one number
> that says the layer-E fill is COMPLETE rather than merely happening: a patch
> owes exactly (LAT_W-1)*(LAT_H-1) = 1,024 cells."*

**It read 8,192 — eight times the contract.** The counter was printed by nothing
and asserted by nothing.

### Attribution, settled two ways

**By source.** At `62d8b6a7`:

* `:27814` — `assign tps_v_cell_fire_c = tpsx_v_valid && tpsx_v_ready && tps_v_cell;`
* `:28412` — `.mat_we_i      (tps_v_cell_fire_c),`

**By measurement.** A throwaway worktree at that commit reports the **identical
`mat_cells=8192`** and **passes** (`BASE_SMOKE_RC=0`).

**The defect is pre-existing.** This packet inherited it exactly, by preserving
the old key one-for-one.

### The mechanism — and it is not the one I reached for first

`tps_v_cell_fire_c` is the **demuxed page-stream beat**: every cell the streamer
hands out, to either arm of `u_terrain_psmux`, for lattices this console never
places. **It is not TERRAIN.PATCH's accept beat**, and in one fill the two differ
by 8×.

**I got the repair wrong once, and the wrong version is recorded because it is
the instructive half.** My first fix gated the old key on `tpc_placed`, reasoning
from `:27662` — `tps_v_ready = tpc_placed ? (tpt_vtx_ready && !tfl_patch_stall) :
1'b1` — which genuinely does discard unplaced lattices. It moved `held_overrun`
**10,239 → 8,191** and left **`mat_cells` at 8,192**.

That is the tell this project already names: *a measurement that did not move
after a change that must have moved it.* `tpc_placed` is not a per-lattice gate
on that beat.

**And the evidence was already on screen.** `pt_samples=1089` against
`ps_cells=11264` — the patch composes 1,089 vertices while the streamer emits
11,264 cells. Differencing those two counts answers the question immediately. I
reasoned about handshakes three times instead of subtracting two numbers that
were printed side by side.

### The repair

Key the join on **the block whose vertices it composes**:

```systemverilog
.a_we_i (tpt_vtx_valid && tpt_vtx_ready && tps_v_cell),
```

That is TERRAIN.PATCH accepting a vertex — the same block and the same handshake
whose field-lane beat (`tvj_p_valid && tvj_p_ready`) this join already pairs
against — so the pairing is correct **by construction** rather than by an
argument about two handshakes that turned out not to be the same one.
`tps_v_cell` keeps the last lattice column and row out, exactly as before.

The smoke now **asserts** `mat_cells == 1024` on a completed fill rather than
printing it. An upper bound would not have caught this in either direction,
which is why it is an equality.

**Note the direction, because it decides whether this is a reduction in
function.** It removes writes that describe lattices the console **decided not to
compose**. It narrows nothing the console renders, and it moves the counter
**onto** its documented contract rather than away from it.

### Why it survived, and why I repaired it rather than only reporting it

**Every other counter in that block balanced perfectly.** `cc_records=1089`,
`cc_overrun=0`, `lat_oob=0`, `cs_oob=0` — heights, substance and velocity were
all correct. Only material was wrong, and nothing read the one counter that says
so.

**And this file already contained the sentence that explains it.**
`zhao_console_core.sv:7650`, *inside entry `I34` itself*:

> *"The composer discards an entire unplaced patch (`tps_v_ready = tpc_placed ?
> … : 1'b1`)"*

Written down correctly, in a **deadlock** argument about `cmd_tfld_ready_w`, and
never carried across to the layer-E write enable. That is `CLAUDE.md`'s own law
about knowledge nothing reads back — arriving inside the entry that states it.

I repaired it rather than only reporting it because **my block now owns that
write face**, and carrying a known 8× breach of a documented contract forward
under a new module's name is the uncashed-cheque shape this campaign keeps
paying for. The standing directive is explicit that the architect decides,
records the rationale, implements and tests.

**It was found by being blocked by it.** The guard I added fired 10,239 times and
I chased the alarm instead of silencing it. That is the only reason anyone looked
at `mat_cells` at all — and my own first diagnosis of it was wrong, which is
recorded above rather than tidied away.
