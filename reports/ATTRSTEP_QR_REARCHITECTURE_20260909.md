# Exact attribute stepping, single-branch q/r — verification, cost, architecture

2026-09-09, branch `zixxtrixx-v8-closeout`. The owner's proposed representation
(N = qA + r with FLOOR q, one decomposition per plane axis, no sign branches,
no reseeds) — verified against this repo's ACTUAL implementations, costed
against the shipped `zhao_raster_attrstep`, and prototyped as RTL.

Nothing here is committed. Files this pass created:

| file | what |
|---|---|
| `tests/proofs/attrstep_qr_differential.cpp` | q/r law + recurrence vs the VERILATED `zhao_raster_attrdiv` vs the COMPILED `zref::render::div_rhu_s128` |
| `tests/proofs/attrwalk_rtl_differential.cpp` | the new walker RTL, both tie builds, pixel-for-pixel vs both actual truths |
| `fpga/rtl/raster/zhao_raster_attrwalk.sv` | the divider-free row walker (deliverable 5) |
| this report | |

---

## 1. VERDICT: the equivalence HOLDS against the RTL's own law — and the repo has TWO laws

### 1a. The q/r rounding law is EXACTLY the shipped RTL law

The proposed emit rule

    rounded = q
    if (2r > A)              rounded += 1
    else if (2r == A && q>=0) rounded += 1

was proven identical to `zhao_raster_attrdiv` — **the verilated RTL itself, not
any restatement** — on 11,636 divides in `attrstep_qr_differential.cpp` and a
further 3,015 walker-emitted pixels in `attrwalk_rtl_differential.cpp`: directed
exact halves of both signs (163 + 512 tie pixels), areas from 1 to 2^46−2,
numerators to ±2^77, sign crossings mid-row, zero mismatches. Algebraically:
for `q >= 0` both reduce to `q + [2r >= A]`; for `q < 0`, `|N| = (−q−1)A +
(A−r)` gives the away-form `q + [2r > A]`, which is the proposed rule with the
tie term disabled by `q < 0`. The recurrence, the row-base copy, and the
per-triangle pair-caching all check out (§3).

### 1b. But the reference implements a DIFFERENT tie, and every document claiming otherwise restated the oracle

`reference/src/zrender/rast.cpp:31` (`div_rhu_s128`, **unchanged since the file
was created at commit 993d8a9d**, verified with `git log -L`):

    if (d < 0) { n = -n; d = -d; }
    q = floor((n + floor(d/2)) / d)      // "floor semantics (§4)"

That is **ties toward +infinity** — and `spec/qformats.md` §1 says so in words:
*"ties round toward +infinity"*; §4 defines `round_half_up_s` identically. The
owner ruling's stated target, `floor((N + floor(A/2))/A)`, **is precisely this
law**.

`zhao_raster_attrdiv` implements **ties away from zero** instead, and its
header claims that is what rast.cpp does. The claim is false. It was never
caught because the whole chain is a closed loop of re-derivation:

* `tests/raster/raster_attrdiv_directed.cpp:37` — oracle re-derived (away form);
* `tests/proofs/attribute_step_equivalence.cpp:71` — *"restated from rast.cpp
  rather than included"*, restates the away form, then its FINDING 1 declares
  the ruling's floor form "not the shipped law" — **backwards**: the floor form
  is exactly what rast.cpp ships;
* `tests/proofs/attribute_plane_equivalence.cpp:42` — *"Restated rather than
  included"*.

**Nothing in `tests/`, `sim/` or `emulator/` links `div_rhu_s128`** (grep,
2026-09-09). The two new differentials are the first code in the repo to put
the actual reference function and the actual RTL in one binary. Measured
divergence: the two laws differ on **negative exact halves only**, zref = RTL
+ 1 there, everywhere else identical (302 divergent cases driven, all
characterized, zero uncharacterized).

### 1c. The divergence is not yet load-bearing — which makes now the cheap time to ratify

* The composed production path carries **flat** attributes (`zhao_raster_
  tile_pipe.sv`: "no attribute interpolation … GEOM.SETUP is not built"), so
  **no golden capture currently pins the RTL attribute law**. The tie is pinned
  only by block-level directed tests whose oracles are the re-derivation.
* Deeper than the tie: zref's inner loop does not divide per pixel at all. It
  divides at **row starts** and steps each attribute by a **once-rounded
  gradient** within the row (`rast.cpp:195` comment; stepping at lines
  442–455). Per-pixel-exact division — attrdiv's model AND the q/r model —
  diverges from those in-row values by accumulated gradient rounding
  regardless of tie law. So a frame-exact RTL-vs-zref attribute differential
  cannot pass today for EITHER tie choice; either zref moves to the per-pixel
  exact model (captures move) or the RTL adopts rounded-gradient stepping
  (numerically worse, and would make the q/r machinery unnecessary in-row).

**Per the assignment's stop rule:** the q/r formulation reproduces the
assignment's stated law and the RTL boundary EXACTLY — so the architecture
proceeds. It does NOT reproduce `zref`'s actual law at negative exact halves,
and no per-pixel-exact scheme reproduces zref's in-row values. Both facts are
now measured, and the walker makes the tie a **one-parameter ratification**:
`TIE_TOWARD_POSITIVE = 0` is today's RTL law, `= 1` is zref/spec §4 — the
latter build was proven equal to the compiled zref on every driven pixel,
ties included. The zref tie is also the cheaper circuit (the tie term loses
its sign input). The owner ratifies; nothing else in the architecture cares.

### 1d. Bonus defect, found by the differential: attrdiv wraps silently in [2^31, 2^32)

`zhao_raster_attrdiv` flags overflow as `|q_r[QPOS-1:32]` — bits ≥ 2^32 — but
`q_o` is 32-bit **signed**. A quotient magnitude in [2^31, 2^32) emerges
**sign-flipped with `q_overflow_o = 0`**: measured q = 2^31+6 → q_o =
−2147483642, no flag (and the mirrored negative case wraps positive). zref
saturates the same case to INT32_MAX. Legal S 8.24 attributes span 33 signed
bits, so the header's convexity argument does not exclude the band. The
directed test never drove it. `zhao_raster_attrwalk` closes the hole for this
path with a full-width emit check (`rounded[QW-1:31]` must equal the sign);
the attrdiv repair is one comparison and belongs with the tie ratification.

---

## 2. What the current attrstep spends, attributed to lines

`fpga/rtl/raster/zhao_raster_attrstep.sv`, per instance (one attribute plane):

**Dual-branch magnitude tracking** — exists only because the magnitude
representation breaks at zero:
* lines 159–160: `qxp_r, qxn_r` (2×96 FF) + `rxp_r, rxn_r` (2×49 FF) — the
  x-step decomposition stored TWICE, once per sign branch: 290 FF, ~145 of
  them pure duplication;
* lines 200–216: recover-Euclidean-pair block (48-bit compare, two adds);
* lines 223–244: the negative branch's derivation (negations, `d_r − rp_c`);
* lines 258–259: per-pixel 96-bit + 49-bit branch muxes on `sign_r`;
* line 273: 32-bit negate mux on every emitted value;
* lines 250–255: `acc_n_r + dndx_r` — a **second full 96-bit adder every
  pixel** whose product is one bit (the next pixel's sign);
* line 404: the sign compare that triggers reseeds.

**Per-row reseed** — lines 355–369, 372–387: every covered row enters `S_SEED`
and occupies the divider for a full divide (36 clocks radix 2, ~20 radix 4)
before the first pixel can move. A 16-row tile pays 16 divides *and* 16 stalls.

**Zero-crossing reseed** — lines 399–407: up to one more divide + stall per
row, plus `seeded_r` and its hang-avoidance special case (lines 362–368).

**Divider launches** — lines 129–145: a private radix-4 `zhao_raster_attrdiv`
instance per attrstep (≈50-bit compare/subtract ×3, 80-bit numerator shifter,
~230 FF). Launches per tile-attribute: 1 (x-step, `S_STEP`) + covered rows +
crossings. The proof measured **0.099 divides/pixel**; a 7-attribute
configuration replicates seven private dividers or serializes on a shared one.

**Row walking cost:** 36 + 16 clocks per covered row (seed + walk), plus ~36
per crossing.

---

## 3. The replacement architecture

### Representation
Per plane: `N = q·A + r`, `0 ≤ r < A`, q = floor quotient. Continuous across
zero — sign tracking, branch duplication, crossing reseeds, and the shadow
accumulator all vanish. Rounding is applied at EMIT on the exact pair (§1a),
so the walk itself is representation-exact at every pixel (invariant
`q·A + r == N` asserted live in sim).

### The three decompositions (per plane)
* `(q0, r0)` — plane at the tile's first **pixel centre** (the ½-step law is
  applied where RASTER.INTERP applies it, at the seed, and stays exact because
  ATTRSETUP's gradients are multiples of 256);
* `(dqx, drx)` — one pixel of x step;
* `(dqy, dry)` — one row of y step.

### The walker (`zhao_raster_attrwalk.sv`, built and proven)
State: base pair (96+47), x pair (96+47), y pairs ×4 for rows 1/2/4/8
(96+47 each, from 3 doubling clocks at job accept), walk pair (96+48), area
(47), control. **No divider, no numerator, no sign state.** Per pixel: one
96-bit add + one 48-bit add/compare/conditional-subtract — strictly less
logic than attrstep's two 96-bit adds + branch muxes. Row start: **copy** the
job base and apply ≤4 pair-adds (the coverage row index's bits) — 4 clocks
against 36, order-independent, no divider dependence. Row cost: 4+16 vs
36+16 clocks → ~2.6× walking throughput, and N walkers no longer contend for
seeds mid-tile. Emitted range is checked full-width per covered pixel
(`q_error_o` + `range_errs_o`, fired 4/4 by legal stimulus in the test —
closing §1d's hole). The tie law is the one named parameter (§1c).

If the 4 stored y-pairs are judged too much state, the alternative is
monotone row stepping (one pair-add per row walked past); that trades 3×143
FF for ≤15 idle clocks per tile. Both are exact; fit decides.

### Where the seed divides go, and the budget (the owner's warning, taken seriously)
Pairs compose exactly: `pair_add`, `pair_double`, and signed `pair_scale`
(binary shift-add) — 2,000/2,000 random anchor→tile reconstructions matched
direct division, tile coordinates ±2048, ½-steps included. Therefore:

* **Per triangle per attribute: exactly 3 true divides** (anchor, dNdx, dNdy),
  in the existing tagged service (`zhao_raster_attrdiv_svc` is already the
  right shape). A floor-pair variant of attrdiv is one small change: same
  iteration count, no `+A/2` pre-add; or keep attrdiv and apply the proven
  conversion identities (attrstep lines 52–67).
* **Per tile: ZERO divides.** Binner-order traversal advances tile origin by
  +16 px: ONE precomputed 16-pixel pair (4 doublings at triangle setup) and
  one pair-add per tile step. Random tile access: ≤26 pair-ops via
  `pair_scale`. Either way, adds only.
* **Per row / per crossing: ZERO divides** (the whole point).

Seed-rate arithmetic, frame budget 1,333,333 clocks (ruling 3), triangles
12k–20k post-cull (`RASTER_Polygon_Budget_Proposal.md`), radix-4 divide ≈ 20
clocks → **≈66,666 divides/frame per serial unit**:

| scenario | divides/frame | radix-4 units at ≤70% duty |
|---|---|---|
| 12k tri × 3 live attrs × 3 | 108,000 | 3 |
| 20k tri × 3 × 3 | 180,000 | 4 |
| 20k tri × 7 (everything live) × 3 | 420,000 | 9 — or 1 pipelined II=1 divider (420k clk, 32%) |

Notes that keep the budget honest: (i) `invw24` is the only pre-Z attribute —
u/v/colour seeds can be **deferred until a tile of the triangle survives
early-Z**, which cuts the 7-attr worst case sharply; (ii) seeds for triangle
T+1 run while T's tiles walk (a small pair FIFO per lane decouples them —
never let the walker wait, the fit-lane lesson); (iii) microtriangles
(~4.5 px) pay ~3 seed divides either way — the win there is the removed
per-row stall and the removed per-instance divider, not divide count; for
terrain-scale triangles the count drops from ~17–33 per tile-attribute to 0.
(iv) A Barrett-style exact reciprocal (one divide per TRIANGLE, then 2
multiplies per decomposition) would cut divides ×3·attrs further but spends
scarce DSP; only worth pricing if the svc unit count fits badly.

### Named fit gates (not run now — owner said no fits)
1. ALM+Fmax of `zhao_raster_attrwalk` vs `zhao_raster_attrstep` + private
   divider, at 1 and at 7 instances (the 96-bit add path is the Fmax
   question).
2. ALM of the seed service at the unit count the ratified attribute mix needs.

---

## 4. Evidence log

`attrstep_qr_differential.cpp` (all sections pass): 4,204 law checks — q/r vs
verilated attrdiv 0 mismatches; ties 76 pos / 87 neg; q/r vs compiled zref 87
differences, all negative-tie +1, 0 uncharacterized; wrap defect demonstrated;
7,424 walked pixels vs RTL 0 mismatches with 0 walk divides (19 crossings,
2,867 r-wraps); 2,000/2,000 pair-reachability; wrong-tie positive control
caught 8/8.

`attrwalk_rtl_differential.cpp` (all sections pass): away build vs verilated
attrdiv 0/3,015 covered-pixel mismatches under random backpressure and sparse
masks; pos build vs compiled zref 0/3,015; ties 497/15, crossings 8, 15
pixels where the two laws differ (walker followed its configured law at each);
range detector fired 4/4, counter 0→4.

`zhao_raster_attrwalk.sv`: verilator `--lint-only -Wall` clean;
`tools/quartus/check_quartus17_syntax.py` clean (216 files scanned). Never
been through `quartus_map` — per the standing rule, not yet shown
synthesizable.

Build recipe (scratchpad, no repo build tree touched): verilate
`zhao_raster_attrdiv` (`-GRADIX=4`) and `zhao_raster_attrwalk` (twice,
`--prefix Vattrwalk_away` / `Vattrwalk_pos -GTIE_TOWARD_POSITIVE=1`); compile
each proof with `-I reference/src/zrender -I reference/include -I
runtime/include`, together with `reference/src/zrender/rast.cpp`, and link
`build/reference/libzhao_zref.a` plus a one-line `sc_time_stamp` stub.

## 5. What needs an owner ruling

1. **The tie law** (§1b/§1c): away-from-zero (today's RTL boundary) or
   toward-+infinity (rast.cpp + spec §4 + the 2026-08-31 ruling's own
   formula). One parameter; zref's law is the cheaper circuit; the divergence
   is confined to negative exact halves.
2. **The in-row model** (§1c): per-pixel exact (RTL blocks, this
   architecture) vs zref's rounded-gradient accumulation. They cannot both be
   the frame-exact law; whichever side moves, it should move before GEOM.SETUP
   composes attributes into the tile pipe and captures start pinning it.
3. **attrdiv's [2^31, 2^32) silent wrap** (§1d): repair alongside whichever
   law is ratified.
