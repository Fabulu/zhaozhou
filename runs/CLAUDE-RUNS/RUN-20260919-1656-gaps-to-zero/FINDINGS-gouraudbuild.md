# FINDINGS — GOURAUDBUILD

**2026-09-21. Branch `gz/gouraudbuild` off `claude/ceiling-architecture-20260912`
at `5b169a86`. Own worktree, never rebased.**
**Register 22 → 22** (9 tie-offs + 13 disconnected + 0 unbuilt + 0 uncited +
0 unresolvable), measured in this tree at both ends.

**Owner decision R234 D1, `(owner, explicit)`: SHIP GOURAUD. It is built.**

---

## ATTRLANE's map HELD. Every claim in it was re-verified here before building

R165 says nineteen lanes have corrected the coordinator's citations. This one
did not have to. Checked by hand in this tree, not grepped from the brief:

* **Slots 3–5 arrive FULL at `zhao_geom_attrpack` and stopped there.** The
  chain is `zhao_light_stream.rgb_{r,g,b}_o` (17 bits, Q0.16, `NDL_ONE =
  17'h1_0000`, asserted in range at that block) → `zhao_geom_vattr.lit_*_i` →
  `c_rgb_q` → `rep_data_o[95:64]/[127:96]/[159:128]` → `rp_st_*` →
  `rp_attr_* = {rp_st_*, 8'd0, rp_invw_*}` → `zhao_geom_clip` → the attrpack
  input port. **Confirmed hop by hop.**
* **GEOM.CLIP winding-swaps those slots and nothing else reads them.**
  Confirmed.
* **`vertex_rgb` is the SAME PATH, not an alternative to it.** Confirmed, and
  it fell out exactly as predicted — see below.
* **~1,420 ALM and +24 DSP for three lanes.** **Confirmed against the receipt
  itself**, not against the brief. `reports/synthesis/blockpaths/
  zhao_raster_texture_v3_fit_top@g8a.fit.rpt`, *Fitter Resource Utilization by
  Entity*:

  | entity | ALM | registers | DSP |
  |---|---|---|---|
  | `zhao_raster_attrgrad_v2:g_attr[0].u_attrgrad` | 488.1 | 473 | 8 |
  | `zhao_raster_attrgrad_v2:g_attr[1].u_attrgrad` | 463.6 | 471 | 8 |
  | `zhao_raster_attrgrad_v2:g_attr[2].u_attrgrad` | 467.8 | 471 | 8 |
  | *(of which `zhao_raster_attrdiv_v2:u_div`)* | 298.1 / 292.7 / 296.9 | | 0 |

  Sum **1,419.5 ALM and 24 DSP**. `rtlCleanAtHead: true`, truth device
  5CSEBA6U23I7.
* **`METAW` 1157 → 1877, 29 → 47 forty-bit slices.** Confirmed arithmetically
  and by elaboration: 298 + 48 + 32 + 47 + 12 + 6×240 = 1877, `META_SLICES =
  ceil(1877/40) = 47`, `META_PHYS_W = 1880`, `META_PAD_W = 3` — still ≥ the 2
  the profile verdicts need, so that guard holds unchanged.
* **`zhao_raster_toon` / `zhao_raster_fog` authored and uninstantiated.**
  Confirmed, and costed below.

**One thing in the brief I could not confirm and one I would correct:** see
*THE COST* below. The `~1,420` is right and is **not the whole bill**.

---

## What was built

### Geometry — the cheap end, and it is not free

`zhao_geom_attrpack` **3 → 6 lanes**. `SLOT_R/G/B` are named parameters beside
the three that were there; `LANES`, `LANE_W` and a packed `SLOT_OF` vector are
named localparams and every array bound, loop limit and terminal comparison
reads them. The elaboration check walks `SLOT_OF` instead of restating six
comparisons.

**No new arithmetic.** The shared `zhao_geom_attrsetup` core is
time-multiplexed, so the whole change to the datapath is a 3:1 operand mux
becoming 6:1, and the schedule going 7 → 13 clocks per triangle against ~41 of
budget at the owner-ruled 120,000 vertices/frame.

**The Gouraud lanes are NOT branched on `tri_untex_i`, deliberately.** R197
declares absent TEXTURE COORDINATES. An untextured primitive is still lit, and
in the reference oracle it is the untextured case that carries pre-lit colour
on these very lanes (`reference/src/zrender/internal.hpp`, the `ScreenV::cr`
comment). Zeroing them would make every untextured surface black — removing
function, not declaring absence. **`geom_attrpack_directed.cpp` now pins that
as a property.**

### Carriage

`zhao_console_core` gains `GEOM_ATTR_SLOT_R/G/B` and three wires. The fork and
its lockstep assertion are untouched — it still differences two counters driven
by two different enables in two different modules, which is the comparison that
can actually see a broken fork.

`zhao_shell_top_v2` and `zhao_geom_bin_pipe_v2` gain three 240-bit ports each.
`METAW` is now written as `META_FIXED_W + META_PLANES * META_PLANE_W` with an
elaboration check that it equals 1877, so the next change is one term rather
than a literal that moved for no stated reason.

**The Gouraud planes are appended ABOVE v/w in the metadata concatenation**, so
lanes 0–2 are bit-identical to the three-plane layout and the tile pipe's
existing unpack ranges did not move. That is why this is additive rather than a
re-layout.

### Raster — where the price lives

`zhao_raster_tile_pipe_v2` runs **six** `zhao_raster_attrgrad_v2` lanes.
`ATTR_LANES`, `START_CLEAR`, `START_N` and `LANE_{INVW,UOW,VOW,R,G,B}` are
named localparams; the three-lane version carried nineteen literal `3`s and two
`5`s.

* the coordinate-agreement cone is a loop over the lanes, with the loop variable
  declared **inside** the `always_comb` (`check_quartus17_syntax.py` FORM 7 —
  a module-scope loop variable assigned inside a conditional `always_comb` is a
  latch Quartus 17.0 refuses the whole design for, while Verilator lints it
  clean);
* lane 1 keeps its `ZHAO_PACKET_D_LANE1_COL` indirection, because that macro is
  how the committed coordinate mutant reaches the comparison;
* the metadata plane unpack is a `generate for` producing bit-identical slices
  for lanes 0–2 — [508:437], [580:509], [676:581] — rather than eighteen
  hand-written ranges, which is where an off-by-72 hides;
* the per-lane start strobe moved **into** the `g_attr` generate, beside the
  lane it starts, so a seventh lane cannot be instantiated without one.

**The range test stayed lane 0's.** It asserts the depth lane's quotient fits
invw24, which is a property of THAT attribute; u/w, v/w and the three colour
channels have no such 24-bit law and never did.

---

## `vertex_rgb` fell out of the same path, exactly as predicted

```systemverilog
continuation_w.post_earlyz =
    zhao_raster_continuation_tail_v2_t'(continuation_tail_bits_q);
continuation_w.post_earlyz.vertex_rgb = {lit_unit8(attr_join_q_q[LANE_R]),
                                         lit_unit8(attr_join_q_q[LANE_G]),
                                         lit_unit8(attr_join_q_q[LANE_B])};
```

**Nothing downstream of the tile pipe changed.** No new fragment port, no new
continuation field, no packet width moved, no `zhao_render_texture_pkg` edit.
PROJOUT's "either/or" was false and ATTRLANE's correction was right.

### The one conversion, and why it saturates rather than faults

A lane carries Q0.16. `zhao_raster_fragment.frag_vert_rgb_i` wants unit8 with
255 full, and feeds `unit_mul(texel, vertex)` when the fragment is textured
(`s0_state_r[5]`) or takes it as the source colour directly when it is not.

That is the oracle's own account of these lanes: *"per-channel light GAIN,
unity = 1<<16"* textured, *"pre-lit COLOUR on the 255 scale"* untextured —
which this console produces as `unit_mul(base_rgb, gain)`, because ruling R11
already makes `base_rgb` the VERTEX's colour and `zhao_texture_combine` falls
back to it when `sample_count == 0`.

`lit_unit8` **clamps**: an over-bright pixel is a pixel, and a
frame-terminating range fault would be the wrong trade. Negative clamps to 0
for the same reason — `zhao_raster_toon`'s own header says an unlit fragment
can legitimately carry a negative lane.

**It is a named function specifically so RASTER.TOON can go in front of it.**

---

## THE COST — my own estimate, and where it differs from the brief

**The +24 DSP is exact and confirmed. The ~1,420 ALM is right for what it
measures and is NOT the whole bill.**

The receipt rows above price the three `zhao_raster_attrgrad_v2` INSTANCES.
They do not price the carriage, and **no existing receipt can**, because the
G8A wrapper drives `job_meta_w` from constants — so the tile pipe's own plane
registers fold away in that fit. Its `u_tile` row shows **357.7 ALM / 665
registers** for three lanes' worth of glue, against 3 × 240 = 720 plane
flip-flops alone. That row is measuring a constant-folded machine.

Counted by hand from the RTL, the flip-flops this change adds outside the three
lanes:

| where | new flip-flops |
|---|---|
| `zhao_raster_tile_pipe_v2` `plane_{n0,dndx,dndy}_q` | 3 × 240 = 720 |
| `zhao_raster_tile_pipe_v2` `attr_join_*` (32+4+4+1+1+1 per lane) | 3 × 43 = 129 |
| `zhao_geom_attrpack` `va/vb/vc_q` | 3 × 3 × 32 = 288 |
| `zhao_geom_attrpack` `n0/dndx/dndy_q` | 3 × 240 = 720 |
| `zhao_geom_binner_v2` `meta_q` + `d_meta_r` | 2 × 720 = 1,440 |
| **total** | **≈ 3,297** |

At four registers per Cyclone V ALM that is **~825 ALM as a floor**, and
register-only packing rarely reaches it; **~1,000–1,300 ALM is the honest
range**, plus the 6:1 operand mux and the widened metadata routing.

> **So my estimate for the whole decision is ~2,300–2,700 ALM, +24 DSP, +18
> M10K — roughly 900 ALM more than the headline.** The headline is not wrong;
> it is the price of the three lanes, and the carriage was never costed. **This
> is an ESTIMATE. I did not run Quartus — `PACKET-PROTOCOL.md` forbids it and
> the protocol outranks the brief.**

**M10K: +18.** The bank is `META_SLICES` independent 40-bit × `TRI_CAP` RAMs;
the composed shell instantiates `zhao_geom_bin_pipe_v2` with the default
`TRI_CAP = 128`, so each slice is 128 × 40 = 5,120 bits. 29 → 47 slices.

**Per-triangle bank time does NOT change**, which is worth saying because
ATTRLANE flagged it as a throughput risk: the slices are written and read in
PARALLEL (`generate for … g_meta_slice`, one `meta_ram` per slice, all on the
same `tri_we` / `meta_ra`), so widening the record costs RAMs, not clocks.

---

## What `zhao_raster_toon` and `zhao_raster_fog` would still need

Read before designing, as instructed, and the shapes they expect are the shapes
they got.

**`zhao_raster_toon`** takes `r_i`/`g_i`/`b_i` as `signed [31:0]` — exactly
`attr_join_q_q[]` — **in exactly the Q0.16 scale the lanes now deliver**: its
authored ramp is `thresholds {43000, 57000}`, `levels {28000, 50000, 82000}`
against a 65536 unity. It belongs BETWEEN the join and `lit_unit8`, and nothing
else in the tile pipe would move. Still missing:

1. **A producer for `cfg_bands_i` / `cfg_thr0/1_i` / `cfg_lvl0/1/2_i`.** This
   is per-MATERIAL state. The flat request carries `material_recipe` and
   `recipe_weight` and **no ramp fields**, so this is a real bank plus a CMD
   path, not a wire.
2. **A throughput check.** It retires one cel fragment every **4.11 clocks
   measured, streamed**, against the tile pipe's one-fragment-per-clock join.
   Only cel fragments reach it, so the question is the cel share of the frame —
   answerable in Verilator, not a fit.
3. **NO FIT ROW EXISTS.** I searched `reports/synthesis/zhao_block_fit.json`
   and `reports/synthesis/blockpaths/`: zero hits for `zhao_raster_toon`. It is
   registered in `design/fit_targets.yml` (with `zhao_raster_toon_div.sv`) and
   has never been fitted. **Its area is unknown, not small.**

**`zhao_raster_fog`** is **247 ALM, 3 DSP, 166 registers**, `status: ok`,
`rtlCleanAtHead: true` (`zhao_block_fit.json`, `sourceCommit 764a1596`). It
takes unit8 colour and sits AFTER texture combination and BEFORE the blend,
which in this console is inside `zhao_raster_fragment`'s source-colour stage —
**not** in the tile pipe's attribute stage. Still missing:

1. **`fogf_i`, an interpolated FOG FACTOR, Q16.16, which HAS NO PRODUCER AND
   NO SLOT.** Owner ruling D-5 made it *"a separate interpolant"*, and
   GEOM.CLIP's ratified seven-slot packet has no room for it — slot 6 is alpha.
   So fog needs `GEOM_CLIP_ATTRS` 7 → 8, a **SEVENTH** attrpack lane and a
   **SEVENTH** raster lane: **+~470 ALM, +8 DSP, `METAW` 1877 → 2117 (47 → 53
   slices, +6 M10K)** — plus a per-vertex fog factor producer, which does not
   exist either.
2. **`cfg_en_i` and the fog colour**, per draw, with §8's frozen EXEMPT LIST
   decided upstream (*"Exemption is a per-class property decided upstream, so
   this block does not try to infer it"*).

> **The honest summary for the next lane: TOON is a bank plus a schedule proof
> and an unmeasured area. FOG is a seventh attribute lane end to end.** Neither
> was composed here, and composing them did not fall out naturally.

---

## R89 did not transfer, and was not allowed to

R89 refused a fourth attrpack plane for a value that *"does not vary across the
primitive."* Varying across the primitive is what Gouraud IS, and R230 records
the distinction.

**Alpha stays flat, per-primitive, on `tri_continuation_tail_i`'s
`vertex_alpha`, where R89 put it.** That is why this build adds THREE lanes and
not four, and why slot 6 is still the one slot `zhao_geom_attrpack`
deliberately does not read. `ALPHA_C` remains R48's named seam.

**`GEOM_CLIP_ATTRS` STAYS 7.** PROJOUT's refusal to narrow it and ATTRLANE's
re-affirmation are honoured and are now load-bearing in the strongest possible
way: **six of the seven slots have a reader.**

---

## Tie-offs

**None created, so none declared.** `tools/design/packet_h_tieoff_audit.py
fpga/rtl/prod/zhao_console_core.sv` reads *"literal connections: 8 declared,
1 reasoned, 10 by group comment, **0 SILENT**"* — unchanged from the start of
the packet.

**Nothing was removed, narrowed, stubbed or disconnected.** The one field that
changed producer — `vertex_rgb` — went from a per-triangle constant with no
producer to a real per-fragment value, which is the opposite direction.

---

## Register before → after: 22 → 22

Unchanged, and that is correct: the Gouraud severance was **never a register
entry**. It had no tie-off (`packet_h_tieoff_audit` read 0 SILENT before and
after) and no disconnected port — the planes were computed, carried and
*dropped by omission*, which R228 is explicit that the instrument cannot see.
Entry I13's prose carried it, and that prose is now updated to record the
build.

**This is worth stating plainly because it is the register's blind spot working
exactly as R159 describes**: driving the number to zero would not have closed
this, and closing this does not move the number.

---

## THE FINDING THAT ONLY A TEN-FORM SWEEP COULD PRODUCE: D1 BREAKS `-GlowTag`

**`tests/prod/run_console_core_smoke.ps1 -GlowTag` is a committed positive
control for owner ruling R195's glow law, and owner decision D1 silently
invalidates the way it delivers its colour.** Nothing in the static gate set,
the directed benches or the plain smoke form can see it.

The mechanism, and it is exactly the severance being repaired, running the
other way:

* `-GlowTag` sets **two fields of one boundary port**: `effect_tag = 0x7F` and
  **`vertex_rgb = 0xB5AAB5`**, both on `tri_continuation_tail_i`.
* The colour is load-bearing, not decorative. The bench's own header says why:
  *"The glow BORROWS the fragment's own resolved colour (R195 decision 1), so a
  BLACK fragment has a black halo and lights nothing … the tag alone would have
  produced `lit=2560` and a plane of zeros, which is a counter moving and a
  picture that cannot change."* With **fragment state 0** the modulate bit is
  off and `s1_src_rgb_r <= s0_vrgb_r` — **the vertex colour IS the pixel.**
* D1 makes `zhao_raster_tile_pipe_v2` overwrite `vertex_rgb` per fragment from
  lanes 3–5. **The tail's colour is now dead from that block onward**, so
  `smk_glow_px_q` — the bench walking the framebuffer for 0xB556 — goes to
  zero and its `$fatal` fires.

**It is repaired by INVERTING the assertion, not by deleting it.** The tail
still carries 0xB5AAB5 and `smk_glow_px_q` must now be **zero in every form**:
that is D1's severance of the stand-in measured from the other side, and a
single 0xB556 word would mean the overwrite is not happening and the
reconnection is cosmetic.

The two-instrument cross-check survives over a different quantity. A new
`smk_lit_px_q` counts framebuffer words that are **drawn and not black**, and
`gather_frag_lit_o != smk_lit_px_q` still differences a counter inside R195's
law against this bench walking memory, sharing no logic.

**And the plain form gains the one check in the tree that can see D1 working in
the COMPOSED console.** `smk_lit_px_q` was **structurally zero** in every
non-glow form before 2026-09-21 — the `-GlowTag` measurement records it
exactly: *"1,062 snapshot words are 0xB556 and every one of the other 1,498
drawn words is 0x0000."* It now counts covered pixels carrying GEOM.LIGHT's
own interpolated output.

> **It deliberately does not predict the colour.** The per-pixel oracle is
> `geom_bin_pipe_v2_directed`. A predicted constant here would be the defect
> the sentinel-overwrite count in this same bench already had to be rescued
> from once — its comment says so.

---

## Method notes

**I did not run Quartus.** `PACKET-PROTOCOL.md` §3 says *"Do not run Quartus"*
and my brief asked for an ALM/DSP estimate; **the protocol outranks the
brief**, which is the precedence ATTRLANE established and the coordinator
endorsed. Every number above is either read off a committed receipt or counted
by hand from the RTL, and which one it is, is stated each time.

**I ran the BASELINE of every test I could not attribute.** Four `tests/tools`
files are red at `5b169a86` in a pristine worktree, and it would have been easy
to report those as regressions or to quietly re-pin them. Measured instead, in
a clean `git worktree add --detach … HEAD`:

| file | baseline | this tree |
|---|---|---|
| `test_render_texture_packet_d.py` | 3 failures + 1 error | 3 failures + 1 error |
| `test_render_texture_packet_e.py` | 5 failures + 1 error | 5 failures + 1 error |
| `test_packet_h_sibling_diff.py` | 1 failure | 1 failure |
| `test_g8a_dsp_rescue.py` | 1 failure | 1 failure |

The one failure I **did** add — `test_d3_rtl_shape_and_selector_controls` — is
repaired rather than waived, and its positive controls all still fire.

**`fpga/rtl/prod/zhao_prod_top.sv`'s protected hash was ALREADY stale at
baseline** (pinned `bd49c6b3`, actual `037ee062`) and my regeneration moved the
actual value to `4384a5a7`. **Not re-pinned, deliberately:** it is a generated
file that three live lanes are about to move again, and a hash re-pin in a
shared protected list is a merge conflict waiting to happen. Flagged here
instead.

**The line-ending trap fired and was caught.** `pathlib.write_text` on Windows
translates `\n` to `\r\n`, and three of the files I edited — `zhao_console_core.sv`,
`zhao_geom_bin_pipe_v2.sv`, `zhao_raster_tile_pipe_v2.sv` — are pinned
`text eol=lf` in `.gitattributes`. The tell was
`gen_raster_texture_v3_fit_top.py` refusing to run: *"G8A generator input is
not checkout-stable LF text"*. Every subsequent edit read bytes, remembered the
file's own ending and restored it.

---

## Gates, and why that set

**Everything, because this packet changed RTL BEHAVIOUR and PORTS.** R227 scales
the set to the change; the change is a six-lane interpolation path, three new
240-bit ports on two blocks and a 720-bit widening of a RAM-backed ABI, so the
scaling argument does not buy anything here.

### The thirteen static gates — all RC 0

```
check_console_inventory.py   check_prod_manifest.py      check_quartus17_syntax.py
check_case_labels.py         mutant_copy_drift.py        wrapper_port_parity.py
mutant_drivers.py            uncashed_cheques.py         check_counters.py
refmodel_liveness.py         duplicate_functions.py      completion_register.py (RC 1, 22 gaps — normal)
packet_h_tieoff_audit.py fpga/rtl/prod/zhao_console_core.sv  -> 0 SILENT
```

`mutant_copy_drift.py` was run **AFTER the commit** (R121): *"57 copies checked
against 362 production modules … every committed mutant copy is at least as new
as the module it copies."* Run before committing it would have answered about
the tree before my work, which is a green that means nothing.

`check_quartus17_syntax.py` matters most here and it is why the coordinate cone
declares its loop variable inside the `always_comb`: **FORM 7 catches exactly
the construct this packet needed to write**, and Verilator lints it clean.

### Ports changed → every generator check

`gen_prod_top.py`, `gen_console_board.py`, `gen_shell_fit_top.py` (both the
legacy shell and `--name-prefix shell_v2`), `gen_raster_texture_v3_fit_top.py`,
`gen_shell_fit_ports_v2.py`, and `gen_shell_paired_diff.py` `--check` **and**
`--check --mutant`. All were STALE and all are regenerated; all now return
fresh.

**Two of them refused to run rather than producing a wrong answer, and both
refusals were correct:**

* `gen_shell_fit_ports_v2.py`: *"new input 'tri_r_plane_i' has no declared
  driver. A port owned by nothing is a port driven by a constant, and the
  fitter folds those away — the area comes back LOW, which is the direction
  nobody audits."*
* `gen_shell_fit_top.py`: *"generator handler inventory differs from shell
  inputs: missing=['tri_b_plane_i', 'tri_g_plane_i', 'tri_r_plane_i']"*.

**The stimulus for the three new planes ROTATES the edge coefficients' roles**
(lane 3 is edge 0 read as `(kc, kx, ky)` rather than `(kx, ky, kc)`). There are
only three edges, and driving lanes 3–5 from the same triples as 0–2 would make
three lanes bit-identical to three others, which lets the fitter share logic
between them and reports area LOW. Same reason the G8A wrapper's new plane
words are a mid-grey `n0 = 2^39` rather than zero.

**`design/fit_targets.yml` was NOT touched.** It is re-read live at each fit
preflight and my change added no new source file.

### Directed tests — R60, built and run at the commit pushed

| test | result |
|---|---|
| `geom_attrpack_directed` | **PASS** — 6 cases, `triangles=6 planes=36`, 0 failures |
| `geom_bin_pipe_v2_directed` (`pd_full`) | **PASS** — 5 tests, **107,273 checks**, 28,879 cycles |
| `pd_coord` | `Packet-D coordinate mutant FIRED` |
| `pd_quiet` | `Packet-D omit-V3-quiet mutant FIRED` |
| `pd_ident` | `Packet-D identity/cancel control FIRED` |
| `pd_old` | `Packet-D old-ready mutant FIRED` |
| `pd_cancel` | `Packet-D skip-cancel mutant FIRED` |

**`pd_coord` is the control that had to be re-earned.** The coordinate-agreement
cone went from six hand-written comparisons to a loop over `ATTR_LANES`, and
`ZHAO_PACKET_D_LANE1_COL` is how that mutant reaches it. The macro stayed on
lane 1 specifically and the mutant still fires.

`cmake --preset windows-native` reconfigured clean, which elaborates every
verilate target in the tree — the console core, both shells, the paired diff
and every fit top — and is the cheapest proof that the port change is connected
everywhere.

---

## Neighbours — where my file set touches the live lanes

**SHADOWSUB is the nearest neighbour and we DO overlap.** FORGE.SHADOW is in
GEOMETRY and will compose blocks into the same file I did:

* **`fpga/rtl/prod/zhao_console_core.sv`** — shared. My hunks are a
  three-parameter addition to the parameter block, three `wire` declarations
  and three connections in each of two instantiations, plus prose in entry I13.
  They are contiguous and far from any FORGE region.
* **`fpga/rtl/prod/zhao_prod_top.sv`**, **`fpga/rtl/prod/zhao_console_board.sv`**,
  **`tests/shell/zhao_shell_paired_diff.sv`**,
  **`tests/mutants/zhao_shell_paired_diff_mutant.sv`** — all four are
  GENERATED from the core's port/parameter list. If SHADOWSUB changes a core
  port these four conflict, and **the resolution is always to regenerate, never
  to merge the text.**
* **`tests/mutants/zhao_console_core_*_mutant.sv`** — both wrappers mirror the
  core's parameter block and each header says to regenerate it when that block
  changes. **They have DIFFERENT LINE ENDINGS (one LF, one CRLF)** — R220 — so
  a scripted patch that does not read each file's own bytes will silently miss
  one. Mine does.

**DELTALAW** (terrain/surface stamp depth law) should not intersect: nothing in
my set is under `fpga/rtl/terrain/` or `fpga/rtl/surface/`, and the only shared
file is the core.

**Not touched, deliberately:** `tests/CMakeLists.txt`, `design/blocks.yml`,
`design/prod_manifest.yml`, `design/fit_targets.yml`. The build needed no new
target, no new block registration and no new fit source.

---

## For the coordinator, if a fit is spent on this

**Name the question first.** The one this packet cannot answer in Verilator is
**area and DSP on the composed console**, and my estimate above has a ~900 ALM
spread that only a fit closes. The three sub-questions, in order of how much
they would change a decision:

1. **Does the carriage cost what I counted?** ~3,300 flip-flops outside the
   three lanes. If it lands near the floor (~825 ALM) the total is ~2,250; if
   packing is poor it is ~2,700.
2. **Did the binner's metadata bank infer as 47 M10K, or did Quartus spill
   some of it to logic?** Each slice is 128 x 40 = 5,120 bits — comfortably one
   M10K — but the bank is now 47 of them and `Fitter RAM Summary` is the only
   place that answers it. A spilled slice is ~5,000 flip-flops, and it would
   read as "the Gouraud lanes cost far more than priced".
3. **Fmax on the six-lane join.** The coordinate-agreement cone is wider by
   three lanes and TIMING4 D2 moved it behind a register for a reason. The
   Timing3 census named the tile-control family; this is the same cone.

**A leaf fit of `zhao_geom_attrpack` would answer none of them** and should not
be spent. The lanes are in the raster; the carriage is spread across four
blocks; only a composed fit sees either.

---

## Owner decisions found

**One, and it is small, cheap to reverse, and it is the owner's to make.**

**THE UNTEXTURED COLOUR SCALE.** The reference oracle's Gouraud lanes carry two
different quantities depending on the primitive
(`reference/src/zrender/internal.hpp`):

* **textured** — *"per-channel light GAIN, unity = 1<<16 — the interpolated
  replacement for `TextureSpan::mod_*`"*;
* **untextured** — *"pre-lit COLOUR on the 255 scale (material x gain)"*.

This console produces **the gain in both cases**, and gets the untextured
colour as `unit_mul(base_rgb, gain)` in `zhao_raster_fragment`, because
`zhao_texture_combine` returns `base_rgb` when `sample_count == 0`
(`untextured_c` → `{cr,cg,cb} = base_rgb_q`) and ruling R11 already makes
`base_rgb` the VERTEX's colour rather than the material's.

**That is arithmetically the oracle's `material x gain`, in the opposite
order,** and the two differ only in ROUNDING: the oracle multiplies at Q16.16
and rounds once; this console converts the gain to unit8 first
(`lit_unit8`) and then does a unit8 `unit_mul`, so it rounds twice.

> **The recommendation is to LEAVE IT**, and to record that it is a known
> one-step difference rather than discover it as a bit-exactness failure later.
> Fixing it means carrying the gain at full Q16.16 through Early-Z and the
> texture round trip — widening `vertex_rgb` from 24 bits to 96 — to save at
> most one LSB per channel on untextured surfaces. **The owner has already paid
> for the expensive option once today; this one buys nothing you can see.**
>
> It is cheap to reverse: `lit_unit8` is a named function at a single site.

## False-absence claims found

**None in my lane.** ATTRLANE's map was correct in every particular I checked,
which is unusual enough to be worth recording on its own — R165 counts nineteen
lanes that had to correct the coordinator, and this is not one of them.

**One PRESENCE claim corrected, in the opposite direction:** ATTRLANE and my own
brief both describe the geometry side as costing nothing. *"Six lanes add no
arithmetic here at all"* is true and is not the same as *"no area"* — the
attrpack grows by ~1,008 flip-flops and the binner's metadata path by ~1,440.
See **THE COST**.

## Instrument defects found

**One, and it is the `-GlowTag` control described above.** It was not defective
when written; owner decision D1 invalidated the path it used to deliver its
colour. It is repaired by inverting one assertion rather than deleting it, and
the repair makes the control STRONGER: `smk_glow_px_q` must now read zero in
every form, which is D1's severance of the stand-in measured from the side
nothing else can see.
