# FINDINGS — PROJ / INPUT packet

Branch `gz/projinput`, from `64cba4c8`.
**Register at my branch point: 25** (12 tie-offs + 11 disconnected + 2 unbuilt).
**Register at my last pushed commit: 24.**

Lane: **I13**, **I14**, **INPUT.SNAC**.

---

## 1. GAPS CLOSED

### INPUT.SNAC — CLOSED. Register 25 → 24.

One of the two remaining NOT-BUILT-AT-ALL entries. Owner ruling **R7** is
explicit and it is what I built against; the constraint that survives it is the
2026-08-31 §6.6 sentence, *"It must emit the same canonical PadFrame and may not
create a second input semantics."*

**What I searched before building**, because this is a repo with fifteen false
absences: `snac` case-insensitively across every `.sv`, `.md`, `.yml`, `.py`,
`.cpp`, `.hpp`, `.zidl` and `.txt` outside `build/` and `.git/` (70 hits, none
of them RTL); `fpga/rtl/synth/` and every `probe`-named file; every
`zhao_input_*` in the tree (two: `snapshot`, `rumble`); `zref::SnacAdapter`
(declared in `design/blocks.yml`, listed as a PHANTOM in
`reports/PHANTOM_REFERENCES.md:112`, and genuinely absent). Nothing existed.
The contract read *"Deliberately unwritten"* in all fifteen sections.

**What it is.** A second ROUTE to the state INPUT.SNAPSHOT already latches:
`spec/input_rules.md` §2's atomic latch, §2.2's absent-pad law, §2.3's sequence
law and §4's button table are all unchanged and all still INPUT.SNAPSHOT's. I
added §7 for the PS1 bus, two normalisations and the merge, and nothing else.

Producer → implementation → consumer: the SNAC connector's five pins →
`zhao_input_snac`'s time-multiplexed serial engine and decode →
`zhao_input_snapshot` inside `u_shell`, which latches the merged bus at the
frame tick exactly as it latched the old one.

**It did not open another gap.** The pins are the class of edge `pad_buttons_i`
and `hps_req_*` are in, which `zhao_console_core.sv`'s HOST.REGWIN composition
already records is not a register boundary; the tie-off count is unchanged at
12. And the merge lives in the BLOCK, not the composer — a composer holding a
pad mux would be inventing an input law in the one file whose discipline is
that it invents none, and the ledger agrees from the other side
(`outputs: [pad_pins]`, `downstream: [INPUT.SNAPSHOT]`).

**It is the identity when idle.** Nothing populated → DAT idles high → every
poll times out → every slot absent → `pad_*_i` reaches the shell bit for bit.
That is why the smoke's numbers did not move, and it is asserted against a
fixture with distinct values per slot rather than argued.

---

## 2. INSTRUMENT DEFECTS FOUND — including one of my own

### 2a. MY OWN BUTTON TEST WAS A BROKEN INSTRUMENT, and the fault survived it.

The first `input_snac_directed` reported **37 checks passed** on its first run.
Before believing it I planted a fault in the RTL — `b[0] = ~lo[4]` (up) became
`~lo[5]` (right), a swapped entry in §4's button table.

**The test passed anyway.** The case held three buttons (START, LEFT, CROSS);
both `lo[4]` and `lo[5]` were RELEASED in that fixture, so the swap was
invisible. **A sixteen-entry table checked on three entries is not a checked
table**, and 37 green checks against a wrong mapping is precisely the
reassuring instrument this repo's CLAUDE.md is mostly about.

Fixed by adding **case 2b**, which walks a single active-low zero through both
PS1 bytes so every one of the sixteen is the only bit that differs, and **case
4b**, which sweeps eight axis code sets through the RTL. Both faults were then
planted again:

| planted fault | before | after |
|---|---|---|
| `up` ← `lo[5]` instead of `lo[4]` | 37/37 PASS | 2 FAIL |
| axis shift `{a,8'h00}` → `{1'b0,a,7'h00}` | (not covered) | 4 FAIL |

The tree was restored and `git diff` confirmed empty after each plant, in the
same invocation.

**The generalisable bit:** the fixture was chosen to be *readable* (three
recognisable buttons) rather than *discriminating*. A mapping test's stimulus
has to make each entry the only thing that moved.

**A second coverage hole, found by asking the same question again.** Every case
up to that point drove connector 0, and case 1's timeout counter fires whether
one port or two are being polled — so **an engine permanently parked on port 0
would have passed all six**, and the round-robin was asserted nowhere. Case 7
attaches a pad to port 1 ONLY, with a different button held so port 0's answer
cannot be mistaken for it, and requires slot 1 present, slot 0 falling back to
the host route, and port 0 still being polled. 46 checks now.

The pattern in both is the same and it is worth carrying: **ask what a broken
version of the thing would have done to this test.** A swapped table entry and
a parked round-robin both passed the version I had written.

### 2b. Every counter in the new block was fired on LEGAL stimulus.

`snac_timeouts_o` (case 1, an empty port), `snac_polls_o` (case 2),
`snac_bad_header_o` (case 5, mode `0x53`), `input_snac_input_sequence_gaps_o`
(case 6, ticks outrunning the bus). None is unreachable, so none owes a mutant.

The gap counter was written against CLAUDE.md's two-operands law deliberately:
its two sides are loaded by DIFFERENT events (the serial engine's completion,
the frame tick), so it can catch a TIMING fault and not merely a value one.

### 2c. THE STALE-BINARY TRAP FIRED ON ME, and asserting the SOURCE was not enough.

Restoring the RTL after the third planted fault, `cmake --build` printed
**`ninja: no work to do`** and re-ran the PLANTED binary. The restored source
was byte-identical to `HEAD` — `git diff` empty, the `PLANTED` marker gone, the
round-robin line back — and the test still reported 4 failures.

CLAUDE.md's build note says to assert the tree is what you think before
measuring, and I did: I counted the marker (0) and the restored line (1) and
ran `git diff` (empty). **All three were green and the measurement was still
wrong**, because the thing that was stale was not the source. Touching the
file's `LastWriteTime` — the remedy the same note gives — did not help either:
a second build also said `no work to do`.

What settled it was looking at the GENERATED output instead of the input:
`Vzhao_input_snac___024root__0.cpp` still carried a timestamp from the planted
build. The fix was the other remedy in that note — delete every
`Vzhao_input_snac.dir` across the tree and regenerate through
`cmake --preset` — after which the restored tree reported 46/46.

**The generalisable bit, which the note does not currently say:** when you have
just built the same target twice within a second or two, a source-side
assertion cannot distinguish "rebuilt" from "not rebuilt". The cheap check that
can is the timestamp (or content) of the file Verilator GENERATED, not the file
you edited. The two earlier plants in this same session restored correctly, so
the failure is intermittent — which is exactly why a source-side assertion
reads as sufficient right up until it is not.

The first two controls' restores were verified by their rebuild lines and their
green re-runs; the third's was not, and that is the one that bit.

### 2d. My own lint command was checked before it was quoted.

`verilator_bin --lint-only -Wall` on the new block returned 0 diagnostics
immediately, which is the shape of a command that did not find the file. Ran it
again with a bogus `--top-module`; it errored and suggested `zhao_input_snac`.
The clean lint is about the block.

---

## 3. FALSE CLAIMS FOUND IN ENTRIES I WAS SENT TO ACT ON

The brief said to check whether each entry's stated cause is still true. Three
of the four things I checked were false or stale.

### 3a. FALSE PRESENCE (the #15 shape): "the id-to-rectangle table is `spec/video_rules.md`'s".

Asserted in **three** places — `zhao_console_core.sv` entry I14, the same file's
matrix-bank composition block, `zhao_cmd_exec.sv:1576` — and **in owner ruling
R30 itself**, whose wording is *"MOVE the viewport-id→rectangle table FROM
`video_rules.md` into the ABI"*.

**There is no such table and there never was.** `spec/*.md` + `spec/*.zidl`
searched case-insensitively for *viewport*: five hits, four of them
`viewport_mask` on `DrawSky`/`DrawForm`, one the `SetView.viewport_id` field
declaration itself. `spec/video_rules.md` does not contain the word.

This matters beyond tidiness: R30's ruled action is *impossible as worded*, and
a packet that inherits the sentence will go looking for a file section that
does not exist. It is the handover's claim-#15 shape — an asserted PRESENCE —
and it survived into an owner ruling.

**And then it got worse, in the useful direction.** I derived the table from §1
and §3/§3.1 and went looking for anywhere else it had been derived. **Three
places had already derived it, identically, and none could cite a table:**

* **`reference/src/zrender/internal.hpp:41` — `zref::render::viewports_of()`.**
  The reference oracle's own executable form, dated to the 2026-08-15
  ratification: `VIDEO_DUO` → `{0,0,256,192}` and `{0,192,256,192}` returning
  2, every other mode → `{0,0,canvas_w,canvas_h}` returning 1. That is the
  whole table INCLUDING the viewport count that makes an out-of-range id
  detectable.
* `tests/terrain/terrain_project_directed.cpp:462` —
  `zref::render::Viewport vp1{0, 192, 256, 192};  // video_rules §3.1 stacked Duo`
* `design/contracts/GEOM.BINNER.md:25` — the same two rectangles, used to pick
  the tile-grid anchor.

**So I14's refusal is wrong twice over.** It says deriving the rectangle *"would
be this file inventing a layout, which is the thing the whole ledger exists to
refuse."* It would invent nothing: **the console's reference oracle — the thing
RTL is supposed to be verified against — has held the table since August.** The
refusal outlived its cause in the same shape the handover records for I23 (*"the
refusal survived its own cause by four files"*), and it cost the entry a month.

This also makes R30 cheap in exactly R73's way: **no ABI change is needed at
all.** `viewports_of()` is the executable definition and §3.2 is its written
form, so the lowering differentials against the oracle and no capture has to be
regenerated. (`viewports_of` currently lives in `reference/src/`, not the public
`reference/include/zref/`; promoting it is the implementing packet's first small
step, and it is a header move.)

**What I did about it.** Wrote the table into `spec/video_rules.md` as **§3.2**,
citing all three prior derivations and naming `viewports_of()` as the thing to
differential against. No RTL change; nothing composed; the register did not
move. It is the input the implementation needed and did not have.

### 3b. STALE: I14's claim that `geometry_tokens`/`fragment_tokens` have no path.

The entry reads: *"`pixel_error` wants MEASURE.GOVERNOR and
`geometry_tokens`/`fragment_tokens` want MEASURE.TOKENS, none of which is
composed."*

**MEASURE.TOKENS IS composed** — `zhao_console_core.sv:11690`, `u_measure_tokens`
— and those two fields already traverse:

```
SetView.geometry_tokens  -> zhao_cmd_exec sv_gtok[view]  (line 1203)
SetView.fragment_tokens  -> zhao_cmd_exec sv_ftok[view]  (line 1205)
                         -> tok_vreq_geom_o / tok_vreq_frag_o (1529/1530)
                         -> u_measure_tokens.vreq_geom_i / vreq_frag_i
```

`zhao_cmd_exec.sv:173` says so in its own words, and the smoke prints
`SMOKE: tokens contracts=1 views=1 ... clamped=1`. This closed with the CMDMEM
packet under R18/R33 and **I14 was never re-read afterwards.** Two of "SetView's
OTHER FOUR FIELDS" are not open.

### 3c. UNDERSTATED: I13's "(b) terrain's `invw24`" is four slots short.

The entry says what remains is *"(a) the two-producer triangle merge into
GEOM.CLIP and (b) terrain's `invw24` from GEOM.DEPTHQUANT."* (a) is right.
(b) is much larger, and the difference decides whose lane this is.

`GEOM_CLIP_ATTRS = 7`. GEOM.CLIP's ratified attribute packet is
**invw24, u/w, v/w, lit r, g, b, alpha** per corner, and a terrain triangle
entering `tri_*` must fill all seven. Terrain today has:

| slot | terrain's producer | state |
|---|---|---|
| screen x/y, behind, src_id, view | `proj_out_*` | **present** |
| invw24 | needs a DEPTHQUANT on `proj_out_aw/bw/cw` | the composed one (`zhao_geom_depthquant_stream`) is inside GEOM.VATTR on the geometry lane's tagged schedule |
| u/w, v/w | — | **NO PRODUCER ANYWHERE** |
| lit r, g, b | `terr_light_base_o` | it is **ONE signed 32-bit SCALAR shade**, not three channels |
| alpha | R48's named constant | **not a gap** |

Searched `fpga/rtl/terrain/**` for `u_over_w`, `v_over_w`, `out_u_o`, `tex_u`:
**zero hits**. The projector carries terrain's `mat_a`/`mat_b`/`weight` — the
Mosaic layer-E triple — which names WHICH materials blend, not WHERE on them to
sample. A terrain texture-coordinate law does not exist in this tree, and
turning a scalar shade into lit rgb needs the material colour, which is the same
missing binding `tri_flat_request_i` is waiting on.

So I13 is blocked on **two absent laws**, not on one absent wire.

### 3d. Checked and still TRUE: `proj_en_i` has no producer.

Searched every instantiation of `zhao_proj_subsystem` (2 in RTL:
`zhao_console_core`, `zhao_terrain_pipe`) and `zhao_project_service` (1). Every
one passes `en_i` straight through from its own port. Searched `fpga/rtl` for
`render_en`, `gpu_en_`, `proj_enable`, `projector_en` and `_en_o`: nothing that
could drive it. The smoke bench sets `proj_en_i = 1'b1` after reset — a literal
tie-off, correctly counted by I14.
---

## 4. GAPS REFUSED, with the exact blocker

### I14 — REFUSED. It cannot close in this lane, and R67 says so itself.

R67: *"I14 still also needs R26's `pixel_error` and a `proj_en_i` producer, so
R30 alone does not close it."* After §3b above, the entry has **three** open
items, not four:

1. **the viewport rect (cfg 16/17)** — R30/R67. Mine. Not implemented; see why
   below. The TABLE is now written (§3a), which is the part that was missing
   before anyone could implement it.
2. **`pixel_error` → MEASURE.GOVERNOR.** `zhao_measure_governor` is one of the
   eleven BUILT-BUT-NOT-CONNECTED blocks and it is **not in my lane** — the
   packet queue assigns it to POST3/MEASURE, and R68 assigns its `thresh_q8`
   half to the GEOM.LOD packet. Its `px_err0_i`/`px_err1_i` are exactly
   `SetView.pixel_error`. **This is a hard cross-lane blocker: I14 cannot close
   until that block composes, whatever I do.**
3. **a `proj_en_i` producer** — an OWNER/ARCHITECTURE DECISION, §5 below.

**Why I did not implement the viewport rect, stated plainly so it is a decision
and not an omission.** The work is: `mode` into CMD.EXEC, a ROM, two more
view-walk steps (cfg 16/17), `cmd_exec_directed` case 8 rewritten (it asserts
all 32 words land once in address order), the smoke fixture moved to Duo per
R67, and the reference-derived pixel count regenerated through
`smoke_geom_fixture_gen.cpp`.

**I checked my own reason before writing it down, and the first version of it
was too flattering.** My first reason was *"it changes `raster pixels=2560`,
the gate value every other lane asserts, and three lanes were running."* That
is weaker than it sounds: the other lanes gate in THEIR worktrees at THEIR
commits, where the fixture is unchanged, so nothing of theirs would have gone
red. Only the coordinator's merged-tree gate moves, and R67 anticipates exactly
that. The comfortable reason arrived first and explained almost all of the
evidence, which is this repo's own tell. The reasons that survive checking are:

1. **It does not close I14.** Item 2 (`pixel_error` → `zhao_measure_governor`)
   is another lane's disconnected block, so the entry stays open whatever I do
   to the viewport. The register does not move for any of this work.
2. **It cannot be implemented without deciding D-2 in RTL.** CMD.EXEC has to
   index the table with a `video_mode`, and §1.1's latch law makes "which mode"
   a real, unratified choice between two defensible readings (see D-2 below).
   A composer picking one silently is precisely how a layout gets invented, and
   it is the thing entry I14's own text refuses to do.
3. **It is a large change on a shared bench** — CMD.EXEC's view walk, a ROM,
   `cmd_exec_directed` case 8 rewritten, the fixture moved to Duo, and the
   pixel count regenerated through `smoke_geom_fixture_gen.cpp` — and half of
   it landing would leave the run's only integration bench in an unknown state.

**Recommended sequence:** settle D-2, compose `zhao_measure_governor`
(POST3/MEASURE) and settle `proj_en_i` FIRST, then do the viewport rect, the
Duo fixture and the reference-derived pixel-count regeneration as ONE commit
with nothing else in flight on that bench. The table (§3a) is the input that
work needed and did not have.

### I13 — REFUSED. Blocked on two absent LAWS, not on wiring. See §3c.

* **(a) the two-producer merge into GEOM.CLIP** is buildable as soon as (b) is:
  `rp_o_valid` from GEOM.REPLAY is GEOM.CLIP's only producer today and there is
  no merge.
* **(b) terrain's attribute packet** needs a terrain **texture-coordinate law**
  (no producer anywhere in the tree) and a **scalar-shade-to-lit-rgb law** (the
  light lane emits one scalar). Both are TERRAIN-lane laws with art content, and
  the entry's own text already says the remainder is "terrain-lane work, named
  here so it is not mistaken for wiring". Building either here would be
  inventing a layout in a composer.

I did not wire `proj_out_*` into anything, because the only port that shape fits
wants edge functions, and an adapter in the composer is the thing the ledger
exists to refuse.

---

## 5. OWNER DECISIONS FOUND

### D-1. `proj_en_i` — what owns the shared projector's rigid-pipeline enable?

**Evidence.** `zhao_project_core.sv:53` and its port comment at 309: *"the
rigid-pipeline enable, owned by the CALLER... The caller derives this from
wherever ITS back-pressure boundary is."* It is a BACKPRESSURE semantic. But
inside `zhao_proj_subsystem` **both callers are now internal** — client A
(GEOM.GROUP_SEQ through PART.PROJECT) and client B (TERRAIN.GROUP_SEQ) — and
`zhao_project_service` already derives grants and `core_ready` from them
(`take_a = en_i && grant_a && core_ready`). There is no caller left outside the
subsystem to own the port, and no producer exists anywhere (§3d).

**Why it is not mine to decide.** The three readings are all defensible and they
are not equivalent:

* **(a) The subsystem derives its own enable and the port is removed.** R27's
  precedent exactly (*"it carries no function, so removing it removes none"*) —
  IF it carries none. It is the cheapest and it is the one I would take if the
  next reading did not exist.
* **(b) It becomes a CONFIG-VALID gate**: hold the projector off until a view's
  matrix bank has been written, so nothing is projected through an unwritten
  bank (every vertex to screen centre with garbage depth). This is real function
  with a fireable counter, and **the smoke bench already implements it as fake
  stimulus** — `geom_camera_ready_q` gates its draw "so no descriptor can be
  culled against an unwritten bank". Fake stimulus standing in for a producer is
  exactly what the campaign's standard says is not present.
* **(c) It becomes a THROTTLE** owned by MEASURE.GOVERNOR. Plausible, and it
  would make item 2 and item 3 of I14 the same packet.

**Recommendation: (b), implemented as a small named block, with (a) rejected.**
The bench's own `geom_camera_ready_q` is evidence that the semantic is needed
rather than invented, and (b) is the only reading under which removing the port
would remove function. If (b) is taken, `proj_en_i` stops being a tie-off and
I14's item 3 closes. **Cheap to reverse either way** — one block and one wire.

### D-2. Which mode indexes the viewport table? (raised by §3a's new §3.2)

`spec/video_rules.md` §1.1 latches the mode only at frame start, effective the
NEXT frame, while a `SetView` in the same packet commits immediately. So does
`SetView.viewport_id` index the table with the mode on screen or the mode
`SetPresentationContract` just set?

**Recommendation: the mode the contract set** — the view being configured is the
view of the frame that contract governs, and indexing with the outgoing mode
gives a Duo frame's second view a Z60 rectangle for exactly one frame. Recorded
as OPEN in §3.2 rather than decided in RTL, because a composer choosing between
two defensible readings is how a layout gets invented quietly.

### D-3. R30's wording needs correcting in the rulings file.

R30 says to MOVE a table that does not exist (§3a). The ruled GOAL is right and
is now satisfiable; the MEANS as worded is not. Recorded so the next reader does
not go looking. Note also that the table is **derived, not an ABI field** —
R73's distinction — so it costs no zidl change and no capture regeneration.

---

## 6. WHAT I DID NOT DO

* **No Quartus.** The coordinator fits at completion.
* **No zidl change**, so `ZHAO_ZIDL_SHA256` is untouched and the five golden
  captures do not need regenerating. The queued `DebugTraceArm` and R77
  `tmu_mode` comment fixes are still waiting for a packet that changes the zidl
  for a real reason; this was not one.
* **No smoke-fixture change**, so `raster pixels=2560` is unmoved and no other
  lane's gate value shifted under it.
* **No random differential for INPUT.SNAC.** The directed suite already sweeps
  both mappings EXHAUSTIVELY (16 button bits, 256 axis codes), so a random
  differential over the same two functions adds no decode coverage; what it
  would add is bus-timing interleavings, which is a bench-model question. The
  contract records this as the next thing the block wants rather than claiming
  it is present, and the ledger's `random:` test row was removed rather than
  left naming a file that does not exist.

---

## 7. ENTRY TEXT I CORRECTED IN THE TREE

Comment-only, no logic moved, the register did not change. Recorded because the
next packet reads these entries and not this file.

* **`fpga/rtl/prod/zhao_console_core.sv` entry I14** � rewritten from "two
  things still open" to three, with the false `video_rules.md` claim replaced
  by what is actually true (the oracle has the table, the table is now in
  �3.2, what remains is the lowering and one decision), the expired
  MEASURE.TOKENS clause struck with the traversal spelled out, and
  `pixel_error`'s cross-lane blocker named as its own item.
* **the same file's matrix-bank composition block** � the same false sentence,
  corrected in place.
* **the same file's entry I13** � "(b) terrain's `invw24`" replaced by the
  seven-slot table of what a terrain triangle must actually fill, with the two
  slots that have no producer anywhere marked as such.
* **`fpga/rtl/command/zhao_cmd_exec.sv`** line 1574's comment � the third site
  of the same false claim.

R30's own wording in `reports/OWNER-RULINGS-20260919-EVENING.md` is left alone:
that file is the owner's record of what was ruled, and a packet editing it
would be rewriting the ruling rather than reporting on it. The correction is
D-3 above.

---

## 8. GATE STATE at my last pushed commit, measured in this worktree

```
completion_register        24   (12 tie-offs + 11 disconnected + 1 unbuilt)  [entered at 25]
check_console_inventory    OK
check_prod_manifest        OK
check_quartus17_syntax     RC 0   (self-test 10 fire / 16 no-fire PASSED)
mutant_copy_drift          OK     (49 copies, no new drift)
check_case_labels          OK
gen_prod_top --check       fresh  (70 instances)
gen_console_board --check  fresh  (1205 core ports, 72 parameters)
gen_shell_paired_diff      fresh
smoke -LintOnly            elaborates
smoke                      PASS -- raster pixels=2560, frames_admitted=1
smoke -Mutant              PASS -- terr_pl_slot_overflow_o fired 1 time
smoke -BadVertex           PASS -- holes=1, groups_poisoned=2, replay_poisoned=8
smoke -NoEchoArm           PASS
smoke -BadTraceArm         PASS -- the reserved bit refused whole, nothing armed
smoke -BadDescriptor       PASS -- the run failed rc=1 on one corrupted byte, as it must
cmd_exec_directed          677 checks, 0 failed          (ruling R60)
input_snac_directed         46 checks, 0 failed          (3 planted faults caught)
input_snapshot_directed    556 checks, 0 failed
input_rumble_directed     2247 checks, 0 failed
input_random            233353 checks, 0 failed
lint zhao_input_snac       -Wall, 0 diagnostics
```

No `COMPILE FAILED` transient was seen in any of the seven smoke runs (ruling
R76); the five variants were run SEQUENTIALLY as that ruling asks.

**What the smoke does NOT show, said plainly:** the adapter is the identity
function in that bench, because nothing is plugged into the modelled connector.
That is the correct and intended behaviour there, and it is why `raster
pixels=2560` did not move � but it means the INTEGRATION bench is evidence
about transparency only. The decode is evidence from `input_snac_directed`,
where a modelled PS1 pad actually answers.

**Unmeasured:** area and timing. Nothing in this run has been fitted, by
design, so `zhao_input_snac`'s cost is an estimate and the contract says so.
