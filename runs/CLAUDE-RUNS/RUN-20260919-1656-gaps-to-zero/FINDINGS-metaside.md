# FINDINGS — METASIDE, 2026-09-27

Branch `gz/metaside`, worktree `C:\programmieren\zencrifice\gz-metaside`,
base `4d11ca7f`. Reported against the brief's nine numbered Deliverable items.

**The headline is a correction, and it is to the brief's own central sentence.**
The brief, entry `I55` and `FINDINGS-swapclose.md` all state that the
walk-arranged frame stops *because of* the missing per-triangle material
metadata, on the evidence `frags[covered/blended] = [149 0]`. **A healthy
console frame — 2,816 pixels, 11 tiles resolved — reads
`frags[covered/blended] = [1216 0]`.** The zero is what success looks like. The
378 bits are genuinely missing, genuinely §4-mandated, and **not what stops the
frame**; they were built anyway, and the real blocker is now named by a
committed probe rather than inferred from a counter.

---

## 1. THE REGISTER, MEASURED BARE

| | value |
|---|---|
| at base `4d11ca7f` | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |
| at my last pushed commit | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |

**`I55` did not close and this packet does not claim it did.** `GEOM_WALK_RASTER`
remains **parked at 0**, which is the shipped default; §5 says why, and it is not
the reason seven packets have given.

Nothing was closed by removing, narrowing, stubbing, tying off or disconnecting.

---

## 2. THE MECHANISM — a VERSIONED EXTENSION, not a sidecar, and the key is that there is no key

`reports/DECISION-20260927-TRIANGLEDESCRIPTOR-V3.md` carries the full record.

**`TriangleDescriptor` v3 — 48 bytes.** The first thirty-two are byte-identical
to v2 (and their first sixteen to v1); the third sixteen carry a 128-bit
`MATSTATE`, declared once in `zhao_pkg` as `ZHAO_MS_*_LO`/`_W`, with
`ZHAO_PARAMBUF_TD_SCHEMA = 3`.

**Where it lives:** in the record, at a fixed offset, fetched in the same burst.
**How it is keyed:** it is not. **Why that key is safe:** because there is no
second store to mis-key. §4's *"prove that eviction/reuse cannot change a
still-referenced identity"* is satisfied **structurally** rather than by
argument. A sidecar keyed by triangle id would have owed that proof against a
console that has already shipped `u_geom_tidq` **one behind**, mis-attributing
**74 of 75** triangles with every range guard passing.

**115 bits vary.** The other 263 are named constants (`aux_required` = 0,
`aux_surface_ctx` = 0, the LOD byte, the base alpha, five tail bits) and the
record's own **address**. Six fields are stored **already composed**, because
re-running their selection on the walk side would be a second *expression* of a
ratified rule — the objection that chose the time multiplex over a second back
end in the first place.

**It fits at full R7 capacity:** `VIEW_USED_B` 3,670,016 -> **3,932,160** against
a `VIEW_SPAN` of 4,194,304, **262,144 spare**. No region in
`spec/memory_rules.md` §5c moves. `TD_B = 48` stays a multiple of
`BURST_ALIGN_B = 16`, `48/8 = 6` is an exact beat count and fits the walker's
4-bit `r_beats_q`, and the 384-bit record still loads into the existing 512-bit
buffer. Declared cost: the triangle arm's SDRAM traffic goes **4 beats to 6**,
+50%, both directions.

### THERE IS ONE ROAD, AND IT IS STRONGER THAN A CHECKER

The console composes the **live** path through the *same* unpack functions the
walk uses. A field that is not in the `ZHAO_MS_*` layout cannot reach
`zhao_geom_bin_pipe_v2` on **either** path — so a future field that forgets this
record **fails to render** rather than rendering wrong, and "the walk silently
got a convenient zero" is not an available outcome.

This is deliberately not a checker. The natural checker here — compare the
walk's reconstruction against the live composition — would build both sides from
the same fields with the same code: **CLAUDE.md's detector wired to two operands
that move together**, blind to the one fault it exists to catch. *A missing road
cannot go blind.* (The three round-trip assertions that **are** present
difference a literal concatenation against fourteen indexed writes and reads —
different code, so a *layout* error moves exactly one side. What they cannot
catch is a wrong *value* fed to both, and that is stated beside them rather than
left for someone to assume they cover it.)

**AND THOSE THREE ASSERTIONS HAVE NOT BEEN SEEN TO FIRE.** They were live and
silent through a full console frame of 89 triangles — which is evidence the
round trip is exact, and is *not* evidence that they can go red. CLAUDE.md is
explicit that a detector reading zero is the claim to check hardest, so this is
declared as an open instrument rather than quoted as a clean result. Firing them
needs a fourth committed mutant (a field dropped from the pack), and it is the
cheapest piece of owed work this packet leaves behind.

### THE CAPTURE POINT IS PROVEN BY THE WINDOW'S OWN INTERLOCK

`zhao_geom_vertid` loads the state in `S_IDLE` on `tri_valid_i` — **the same
event that loads the whole record**. That the publication it samples is *this
triangle's* is structural: `u_material_window`'s `d_enter_i` is GEOM.CLIP's
input and `d_leave_i` is the door, and vertid consumes at GEOM.CLIP's **output**,
strictly between them. The window's own header states the consequence, and
`mat_win_err_unpublished_o` is the composed counter that watches the one thing
that would falsify it.

---

## 3. A PIXEL THROUGH SDRAM AT `GEOM_WALK_RASTER = 1` — **NOT DELIVERED, AND THE BRIEF'S EVIDENCE BAR IS ITSELF WRONG**

This is the deliverable I did not meet, and the reason is §5.

It is also the item where **the bar as written cannot be met by a working
console**. The brief asks for *"`frags[covered/blended]` with a NONZERO blended
count"*. `blended_fragments_o` counts fragments whose write was **not**
`BL_REPLACE` — `zhao_raster_fragment.sv:671`, in the counter's own comment:
*"A REPLACE write is not a blend."* Every span in this fixture is REPLACE. The
healthy frame reads **`[1216 0]`**. Demanding a nonzero there asks a correct
console to prove itself with a number that reads zero when it is correct.

---

## 4. THE IDENTITY TEST — triangle N's state is not substitutable by N-1's

`geom_paramarena_directed` **case 1c**, and its decisive check is an
**inequality**, not an equality:

```cpp
ckt(!ms_equal(r.tris[i].matstate, tris[i - 1].matstate),
    "case 1c: triangle N's material state is NOT triangle N-1's --"
    " the forbidden 'reuse the last publication' cannot pass here");
```

A round-trip equality **cannot** see §4's forbidden shortcut, because on a
fixture where adjacent triangles share a material the right answer and the wrong
one are the same bits. So the fixture gives every triangle a **distinct** state,
and three premises are asserted first, in case 1b's style: every state distinct;
**every one of the thirteen variable fields varies across the set** (so a
decoder pinning one to a constant — or to the profile default it would take with
no publication — fails on that field); and the 13-bit reserve is zero, so
`t_illegal_o` staying low is itself evidence the spare bits landed where
`ZHAO_MS_RSVD_LO` says.

**And the check is seen to FAIL.**
`tests/mutants/zhao_geom_paramwalk_holdstate_mutant.sv` is §4's forbidden
shortcut implemented — one substantive line, `t_matstate_o` driven from a
register holding the *previous* triangle's decode. Target
`geom_paramwalk_holdstate_fires`, polarity **inverted**. The mutant **replaces**
production in its source list (`list(REMOVE_ITEM ...)` plus a `FATAL_ERROR` if
production is still listed), so two walkers cannot elaborate together.

The inverted arm asserts **`substituted > 0` AND `survived == 0`** — the first
says the defect happened, the second says the seam engaged rather than compiling
production twice. CLAUDE.md records two combiner mutants that measured
**unmutated** production because a `-D` never reached the `ifndef` it aimed at,
with **no diagnostic**; a run where the seam did not engage produces zero
substitutions, which checked naively reads exactly like a pass.

**BOTH RUN, AND BOTH HALVES PROVEN (R60).**

| target | result |
|---|---|
| `geom_paramarena_directed` | **647 / 0 checks failed** (was 620) |
| `geom_paramwalk_holdstate_fires` | **638 / 0**, INVERTED — the substitution was OBSERVED |

The mutant run is the half that matters, and it proves two separate things:

* **`substituted > 0`** — the walker handed at least one triangle the PREVIOUS
  triangle's state. So case 1c's inequality is a check that CAN fail, and the
  forbidden shortcut is refused by evidence rather than by a comment.
* **`survived == 0`** — and *no* triangle got its own. That is the seam's own
  negative control: had the `-D` failed to select the mutant and production been
  compiled twice (CLAUDE.md records exactly that, twice, with **no
  diagnostic**), every triangle would have carried its own state and this check
  would have gone red. It did not, so the block under test is the mutant.

The two totals differing — 647 against 638 — is the corroborating signal that
the `ifdef` engaged on the **C++** side as well.

---

## 5. WHERE THE WALK-ARRANGED FRAME ACTUALLY STOPS — MEASURED, NOT INFERRED

### 5.1 — `blended = 0` IS WHAT SUCCESS LOOKS LIKE

Entry `I55`, the brief and `FINDINGS-swapclose.md` §5 all say:

> *"`frags[covered/blended] = [149 0]`: fragments are generated and NONE EVER
> BLEND, so the tile pipe never empties, never reaches RS_SWAP, `resolved_tiles`
> is 0 and `job_ready_o` never returns."*

**All three clauses are false, and the refutation was already on disk in this
repository before SWAPCLOSE ran:**

```
reports/synthesis/arenabin/smoke_plain_arenainfer.log:75
SMOKE: rasterdiag jobs[started/sunk]=[101 0] tilestore_refs=5248 resolved_tiles=11
                  earlyz[covered/rejects]=[1216 0] frags[covered/blended]=[1216 0]
```

That is a **healthy** frame — 11 tiles resolved, 2,816 pixels — reading
**blended = 0**. `blended_fragments_o` is a **counter**; it gates nothing, and
nothing in `ordinary_pipe_empty_w` reads it.

Note the shape: this is CLAUDE.md's *"a number that is exactly zero is a broken
instrument until proven otherwise"* arriving from **the other side**. The zero
was sound; the **reading** of it was not. And the direction is the usual one —
it made the remaining work look like exactly what the brief had already assigned.

### 5.2 — THE PROBE, AND IT NAMES ITS OWN CAUSE

`SMOKE: walkwedge` / `SMOKE: walkquiet` print all eight terms of
`ordinary_pipe_empty_w`, the four of `producer_quiet_w` inside it, and
`rs_state_q`. Measured in the composed console at `GEOM_WALK_RASTER = 1`:

```
SMOKE: walkwedge rs_state=3 last=1 job_ready=0 | empty=1 <- ew_done=1 prodquiet=1
                 ezcand=0 skid=0 stgcand=0 texquiet=1 stgfrag=0 fragidle=1
SMOKE: walkquiet ew_job_ready=1 rowhold=0 attr_idle=63 attr_qv=0 attr_bundle=0
                 | abort=0 seqabort=0 seqmis=0
```

* **`rs_state = 3` is `RS_SWAP`**, not `RS_ACTIVE`.
* **`empty = 1`** — `ordinary_pipe_empty_w` is TRUE. Every one of its eight terms
  is satisfied, including `texquiet` and `fragidle`. **The pipe emptied.**
* **`last = 1`**, and **no abort**: `abort=0 seqabort=0 seqmis=0`.

So the tile pipe **reached the swap correctly** and is stuck in

```systemverilog
  assign resolve_start_w = (rs_state_q == RS_SWAP) && !abort_now_w;
  assign ts_swap_w       = resolve_start_w && resolve_ready_w;
  ...  if (ts_swap_w && ts_swap_ready_w) rs_state_q <= RS_IDLE;
```

— the **tile-store / RASTER.RESOLVE swap handshake**, which the flat request,
the continuation tail and the fragment state do not gate at all.

**This kills the AUX hypothesis as well**, and that one can be refuted by
reading: `mat_flat_request_c` drives `aux_required` from the literal `1'b0` and
`aux_surface_ctx` from `224'd0`, **on both paths**, published or not. A fragment
on the walk path cannot ask for AUX, so *"a fragment that asked would never
retire"* cannot be why these 149 did not. SWAPCLOSE labelled it *"consistent
with the evidence and NOT PROVEN"*, which was the right label; the entry and the
brief then quoted it without the label.

### 5.3 — AND THE SECOND PROBE NAMES THE ROOT CAUSE TO ONE LINE

`SMOKE: walkswap` separates the three candidates the first probe cannot.
Measured at `GEOM_WALK_RASTER = 1`, with SCHEMA v3 in place:

```
SMOKE: walkswap resolve_ready=0 ts_swap=0 ts_swap_ready=1
                | fb_valid=1 fb_ready=0 fb_last=0
                  tr_valid=0 tr_ready=1 tr_data_valid=0
```

* **`ts_swap_ready = 1`** — the tile store is READY. It is not the tile store.
* **`resolve_ready = 0` with `fb_valid = 1` against `fb_ready = 0`** —
  RASTER.RESOLVE started, has a pixel in hand, and **the framebuffer sink is not
  accepting.**

And it never will:

```systemverilog
  zhao_shell_top_v2.sv:1653
    assign rpx_ready = !post_phase_w && fbw_px_ready;
```

The shell muxes **one** framebuffer writer between the raster and the post pass
— its own comment says *"45-bit mux instead of a second FBWRITE (~300 ALM
saved)"* — and `post_phase_w` (`phase_post_o`, `:1753`) hands it to POST. **The
walk runs during the DRAIN, after the frame's geometry has sealed, by which time
the post phase has opened and `rpx_ready` is a structural zero.**

So the walk-arranged console **produces its pixels correctly and has nowhere to
put them.** `raster pixels=0 bursts=0` is not a rendering failure; it is a
scheduling one.

**This is the same family as TERRAINVISIBLE's** — *"terrain's 65 references were
pushed into the binner's arena AFTER the frame had been serialised … a stimulus
ORDER fault and not a console one."* Here it **is** a console one: the drain
window and the post window overlap, and the on-chip drain never noticed because
it runs *before* the seal.

**What I55 actually needs now** is a window in which `rpx_ready` can be high —
run the sweep before the post phase opens, or defer the post phase until the
sweep has drained. That is a **phase** question in `zhao_shell_top_v2`, a
different subsystem from everything eight packets have worked on. It is **not** a
second framebuffer writer: the mux exists to save ~300 ALM on a console already
measured at 350% of the shipping part.

**And SCHEMA v3 changed nothing about the walk's behaviour**, which is the
result I wanted: `tiles=3 empty=23 jobs=13 door=13`, `tris=14`, `vread=45`,
`earlyz[149 0]` — identical to the pre-v3 run. The metadata carriage is neutral
to the wedge, which is the cleanest possible demonstration that the two were
separate problems.

### 5.4 — AND THE SHIPPED CONSOLE IS UNREGRESSED, WHICH IS THE CHECK THAT MATTERS MOST

SCHEMA v3 touches the descriptor every frame writes, so the question that
outranks everything above is whether the **parked, shipping** arrangement still
draws. Measured on my own tree at the pushed commit:

```
SMOKE: clip       submitted=144 clipped=69 culled=0 setup_submitted=75
SMOKE: raster     pixels=2816 bursts=176 issued=94976 retired=94976 drained=1
SMOKE: binrefs    tile_references=101 max_tile_list_depth=59 overflow=0
SMOKE: rasterdiag jobs[started/sunk]=[101 0] tilestore_refs=5248 resolved_tiles=11
                  earlyz[covered/rejects]=[1216 0] frags[covered/blended]=[1216 0]
SMOKE: renderlease leases_granted=1 refused=0 clears=1 frames_admitted=1
SMOKE: tilewalk   PARKED (GEOM_WALK_RASTER=0): the sweep is a STRUCTURAL ZERO
SMOKE: PASS
```

**`raster pixels = 2816` and `frames_admitted = 1`** — the gate list's exact
required values. `setup_submitted = 75` is the reference's own count, and the
parked invariant holds as a structural zero: `tilewalk tiles=0 jobs=0 door=0`,
`paramwalk dirs=0 chunks=0 tris=0`.

**And this run reproduces §5.1 independently.** `frags[covered/blended] =
[1216 0]` on a run that ends `SMOKE: PASS`, from my own tree, at my own commit —
so the headline finding no longer rests on a log another packet left behind. The
zero is what success looks like, twice, measured separately.

### 5.5 — SO THE PARK STANDS, AND ITS REASON CHANGES

`GEOM_WALK_RASTER = 0` remains the shipped default. What changes is the entry's
account of *why*: not "378 bits of metadata", but "the tile never resolves, and
the tile-store swap is where it stops."

**And the arrangement is now BUILDABLE by anyone.** SWAPCLOSE measured
arrangement 1 by hand-editing the literal in a 33,000-line file, so its entire
FINDINGS is about numbers nobody could reproduce. The constant now carries an
`ifdef` and the smoke a `-WalkRaster` switch **with its own build-directory
tag** — which that script's own comment says every new switch needs, and which
has been forgotten once already. The default is unchanged and the park is
unchanged: *a parked arrangement nobody can build is not more parked, only less
measurable.*

---

## 6. CLAIMS IN THE BRIEF OR THE RECORDS FOUND FALSE

### 6.1 — "the frame stops at `frags[...] = [149 0]` BECAUSE the 378 bits are missing" — FALSE

§5. The brief's own words: *"The frame stops at `frags[covered/blended] = [149 0]`
— fragments produced, none blend, no tile resolves — **because** the
per-triangle flat request (298), continuation tail (48) and fragment state (32)
are in NEITHER back end."* The 378 bits are real and are §4-mandated; the causal
claim is not. Direction: flattering — it made the remaining work exactly the
work already assigned.

### 6.2 — the brief's EVIDENCE BAR asks for a number a working console reads as zero

§3. *"`frags[covered/blended]` with a NONZERO blended count"* cannot be produced
by a correct console on this fixture. A gate that a correct machine fails is the
mirror of a gate a broken one passes, and it is the rarer half of this
repository's own law.

### 6.3 — SWAPCLOSE's recommended repair is refuted by SWAPCLOSE

`FINDINGS-swapclose.md` §5.1: *"The right repair is architectural and it is not a
sidecar … the walk should **re-ask the window per triangle**."* The window
resolves by `{material_set, material_id}`; three of the fields are **not**
functions of the material — `vertex_alpha` (R89, `zhao_forge_shadow.sv:295`,
latched **per caster**), `detail` (`zhao_terrain_clipfeed`'s per-primitive
declaration) and the mosaic's `base_rgb`/`recipe_weight` (TERRAIN's **per-cell**
triple). The same document's last paragraph says so: *"Part is a keyed lookup;
part is genuinely per-primitive."* **The document recommends a mechanism it then
proves insufficient, three paragraphs apart** — HANDOVER §15.35's own law
recurring inside the document that named it.

### 6.4 — the sidecar was priced against the WRONG payload

SWAPCLOSE refused a sidecar at *"48 bytes x 16,384 = 786,432 against 524,288
free"*. The arithmetic is right and the payload is not: 225 of the 378 bits are
a structural constant zero (`aux_required` + `aux_surface_ctx`), 18 are the
record's own address, and the rest are named localparams. **The variable part is
115 bits — 16 bytes — and it fits with 256 KiB to spare.** A refusal priced
against an un-minimised payload is a refusal nothing checked, and CLAUDE.md's
chapter on refusals says a wrong one *"is caught by nothing at all"*.

### 6.5 — `gen_prod_top.py` SILENTLY DROPS a block whose port width reaches a package constant

Declaring `[ZHAO_TD_MATSTATE_W-1:0]` on three leaves made the generator print
`SKIPPED ... width unresolved` and emit a top **without those three blocks** —
`89 instances` became `86`. `--check` then reports **fresh**, because the file
does match the generator. The only tell is the instance count moving, in a line
nobody reads, and the consequence is that a real production block is not fitted.
`_eval_parameter_expr` resolves against **module** parameters only; package
constants are not in its table.

Worked around here (literal port widths plus `p_matstate_width` asserting the
package at **elaboration** — not lint, which does not run `initial` blocks), and
reported rather than absorbed. **The repair belongs in the tool**: it already
loads and strips every file under `fpga/rtl`, so seeding `values` from
`zhao_pkg`'s own localparams is the same shape as the package-import scan
`check_prod_manifest.py` already carries.

### 6.6 — the gate list's `check_localparam_comments.py` path is wrong, as `check_entry_claims.py`'s was

SWAPCLOSE §6.3 found `check_entry_claims.py` is in `tools/design/`, not
`tools/budget/`. **`check_localparam_comments.py` is the same**: `tools/design/`,
and run from `tools/budget/` it returns *"can't open file"* with RC 2 — which a
batch loop reads as a failing gate. Two of them now; the pattern is worth a
sweep rather than a third discovery.

### 6.7 — a stale capacity comment in the bench, and four in the RTL

`zhao_geom_paramarena`'s `TD_B`, `TRI_CAP_B`, `CHUNK_OFF_B` and `VIEW_USED_B`
comments were all correct for v2 and all had to move; that file's own paragraph
says **nothing checks them**, because they reach a package import and
`check_localparam_comments` skips a constant it cannot resolve. The same number
in `geom_paramarena_directed.cpp` (`CHUNK_OFF_B // 2,621,408`) was **already
stale by 32** before I touched it. All corrected in the same commit as the
change, per CLAUDE.md.

---

## 7. WHAT I REFUSED, AND WHAT I GOT WRONG AND CAUGHT MYSELF

### Refused

* **Un-parking `GEOM_WALK_RASTER`.** The console does not draw at 1. The brief's
  fence and directive §7 both say what to do, and this is it.
* **Claiming `I55` closed.** The metadata is carried; the frame still does not
  resolve a tile, for a reason that has nothing to do with the metadata.
* **A sidecar keyed by triangle id.** Two round trips on every triangle the
  raster draws, against v2's own recorded reasoning, and it would owe the
  eviction proof that a field inside the record does not.
* **Re-asking the material window per triangle.** §6.3 — it cannot express three
  of the fields.
* **Truncating the 23-bit chunk id to the tail's 18-bit field on the argument
  that the top five bits are always zero.** They are, for every chunk this
  console writes, and that is exactly the premise §4 refuses. `tri_id_wide_o`
  counts the records where it is false.
* **Any console or full-device fit**, and any ALM or Fmax claim. The leaf map
  below is analysis & synthesis only and carries neither; see §8a.
* **Retiring the binner's drain (Deliverable 8).** It is the *complete oracle*
  directive §7 requires be retained, and at `GEOM_WALK_RASTER = 0` it is the
  thing drawing the picture. Retiring it while the walk arrangement renders zero
  pixels would be closing a gap by removing function, which rule 1 forbids. It
  becomes retirable when the console draws at 1, and not before.

### Got wrong and caught myself

1. **I launched a `cmake --preset` configure and then edited files inside its
   closure** — `tests/CMakeLists.txt` and `tb_zhao_geom_paramarena.sv`. That is
   SWAPCLOSE's lesson 10, which I had read the same morning, and it means that
   configure's output describes a tree that moved underneath it. **Nothing
   from it was quoted**; it was re-run from scratch and the re-run is what
   built the tests above. The re-run then failed loudly on an `ENDLABEL` —
   my mutant's `endmodule : zhao_geom_paramwalk` still carried production's
   name, because the rename matched `module <name>` and an end label is
   `endmodule : <name>`. Caught by the toolchain in seconds, which is the good
   kind of failure.
2. **I hit the documented heredoc trap FOUR times.** SWAPCLOSE hit it three
   times and wrote *"written down, warned about, done anyway — for the third
   packet running."* Fourth. The rule is in `PACKET-PROTOCOL.md` and the remedy
   (write the file, then run it) works every time. The fix is not more
   discipline; it is to stop reaching for `bash -c` with quote-heavy Python at
   all.
3. **I quoted `RC=$?` after a pipeline** on my very first register measurement —
   `python ... | tail -30; echo RC=$?` reports **`tail`'s** status. The build
   note names this exact trap. Caught before it was written down as a result.
4. **I declared a port width as a derived package expression and it silently
   removed three blocks from the production top.** §6.5. Caught by reading the
   generator's output instead of its exit code — which was **0** for the run
   that dropped them, because writing a top with 86 instances is a success.
5. **My first guess at the wedge was the same as SWAPCLOSE's** — that the
   material metadata made fragments hang. I wrote the probe before building on
   that guess, and the probe refuted it. Had I built first and measured after, I
   would have shipped 378 bits of correct work with a wrong causal story
   attached, which is how this entry acquired the last one.

---

## 8. THE LEAF `-MapOnly` ROW (Deliverable 5)

```
zhao_geom_paramwalk@metaside-tdv3
  status            map_only          partial: analysis_and_synthesis
  rtlCleanAtHead    true              <- READ THIS FIRST, ALWAYS
  treeCleanAtHead   true
  measuredDevice    5CSEBA6U23I7      <- the SHIPPING part, not a sizing device
  sourceCommit      c2c18757          <- this packet's pushed HEAD
  sourceDigest      cbe05c6dc893...   over 2 declared sources
  registers         2,756
  blockMemoryBits   0
  dspBlocks         0
  virtualPins       2,078
  seconds           74.7
```

**READ THE ROW CORRECTLY, in three parts.**

1. **It carries NO ALMs and NO Fmax.** `-MapOnly` stops after analysis &
   synthesis, and `run_block_fit.ps1` refused the run until I passed
   `-RowLabel` precisely so this row could not overwrite a full-fit row and
   discard its area and timing. Nothing here is an area claim.
2. **THERE IS NO BASELINE TO DIFFERENCE IT AGAINST.**
   `zhao_geom_paramwalk` has **never been mapped or fitted** — this is the only
   row for it in either database. That is the same hole SWAPBUILD found for
   `zhao_geom_attrpack` (*"had never been mapped or fitted — zero rows in
   either database"*), one block over. **So SCHEMA v3's cost on this block is
   NOT measured by this row**; the row is a *new baseline* for whoever changes
   it next, and saying it is a delta would be this campaign's own
   "declared-today versus measured-a-week-ago" error with no measurement on the
   other side at all.
3. **It is a LABELLED row, so `ruleViolations: []` on it is SILENCE, not
   compliance.** CLAUDE.md: labelled rows are never rule-checked — 0 of 26
   carry violations against 12 of 92 unlabelled. Do not quote its empty
   violation list as a pass.

What it *does* say, cleanly and from a clean tree on the shipping part: the
walker at SCHEMA v3 infers **no DSP and no block memory**, and holds 2,756
registers. The 128-bit descriptor widening did not push `td_buf_q` into a RAM
and did not buy a multiplier.

---

## 9. GATES AT THE PUSHED COMMITS

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** — `I34`, `I55`; no higher than the 2 I started at |
| `check_console_inventory.py` | **OK** |
| `check_prod_manifest.py` | **OK** |
| `gen_prod_top.py --check` | **fresh (89 instances)** — and see §6.5 for why that number is the gate |
| `gen_console_board.py --check` | **FRESH (1624 core ports)** — this packet adds **no** console port |
| `gen_shell_paired_diff.py --check` | **fresh**, harness and mutant |
| `check_quartus17_syntax.py` | **RC 0**, 671 files, self-test 13 fire / 22 no-fire |
| `check_case_labels.py` | **OK** |
| `check_entry_claims.py` | **OK** (at `tools/design/`) |
| `check_localparam_comments.py` | **OK** (at `tools/design/`, §6.6) |
| `mutant_copy_drift.py` | **OK**, 80 copies, **run AFTER each commit** (R121) |
| console smoke `-WalkRaster -LintOnly` | **RC 0** |
| console smoke, PLAIN (the SHIPPED, parked arrangement) | **PASS -- raster pixels=2816, frames_admitted=1, resolved_tiles=11, setup_submitted=75, tilewalk a STRUCTURAL ZERO, and frags[covered/blended]=[1216 0] on my own tree** |
| console smoke `-WalkRaster` (full) | **RAN** — see §5.2/§5.3; still 0 pixels, root cause named |
| `geom_paramarena_directed` | **647 / 0** (was 620) — case 1c included |
| `geom_paramwalk_holdstate_fires` | **638 / 0**, INVERTED — substitution observed, seam proven |

**The one remaining gap is Deliverable 5**: no Quartus row. Declared in §7
rather than estimated.

---

## 10. BRANCH AND COMMITS

**Branch `gz/metaside`. Pushed. Never `--force`, never `--force-with-lease`.
Not merged to the integration branch.**

| commit | what |
|---|---|
| `bf649794` | `feat(METASIDE)`: arrangement 1 is BUILDABLE, and the zero it stops at is what SUCCESS looks like |
| `15bef47a` | `feat(METASIDE)`: TriangleDescriptor v3 carries the material state, and BOTH paths go through one layout |
| `80195076` | `test(METASIDE)`: the SUBSTITUTION check, and a committed mutant so the refusal is evidence |
| `c2c18757` | `fix(METASIDE)`: entry I55's account of where the frame stops was wrong in every clause -- here is the line |
| `13dc9f3a` | `docs(METASIDE)`: the leaf map receipt for the walker at SCHEMA v3, and it is a BASELINE not a delta |
