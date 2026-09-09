# Memory for ALM and DSP — can M10K buy back the scarce resources?

2026-09-09. Owner's question, verbatim: *"Can we not find a design that uses
memory instead? We need both ALM and DSPs desperately."*

Device 5CSEBA6U23I7: 41,910 ALM / 112 DSP / 553 M10K. Manifest DSP sum ~154
(MEASURED, floor — 42 blocks unfitted), island M10K ~48. M10K is the abundant
resource; the question is what it can buy bit-exactly.

**Answer in one paragraph.** Yes, there is a bit-exact memory design for the
8×8 unit multipliers — the quarter-square identity, one M10K per multiplier,
zero approximation — and it applies cleanly to the LIVING combiner
(`material_combine_v2`, 2 DSP → 0 for 2 M10K). But the per-call-site audit
shows the larger totals do not fall to memory: `zhao_texture_combine`'s 8 DSP
are already scheduled for DELETION by a standing manifest instruction whose
trigger has fired (that is +8 DSP and +494 ALM for **zero** M10K), and
`zhao_project_core`'s 66 DSP (two instances × 33) have unconstrained 32-bit
operands on both sides, which kills every table scheme at a sane exchange
rate — its real levers are width-narrowing, core-sharing and a trailing-zeros
rewrite, none of which spend memory. One modest memory-for-ALM move does exist
inside the core (the divider's rider delay-line into an M10K shift register).

Rules honoured: no RTL edited, no Quartus/cmake/ctest/lint run (a fit is
live). Every number below is labelled MEASURED / ARITHMETIC / PREDICTION, and
predictions state what MOVES, not how far — two magnitude predictions were
falsified in this repo yesterday and CLAUDE.md's law stands.

---

## 1. The per-call-site audit (the load-bearing evidence)

### 1a. `zhao_texture_combine.sv` — the twelve `unit_mul` instantiations

`unit_mul` is defined at `fpga/rtl/texture/zhao_texture_combine.sv:109-120`
(the frozen `((a*b)+128)>>8`, clamp 255). The brief's "7 direct + 5 via
mul2x9" split is not quite the structure; the true count is 12 in three
groups of four, one group per product recipe, **mutually exclusive per
fragment** (one `unique case` arm at line 216 executes per beat):

| # | sites | file:line | operand a | operand b |
|---|---|---|---|---|
| 4 | R_MODULATE | zhao_texture_combine.sv:221-222 | `r0/g0/b0/a0` = registered s0 channel (line 138-139) | `r1/g1/b1/a1` = registered s1 channel (142-143) |
| 4 | R_MODULATE2X via `mul2x9` (163-169, `unit_mul` at 166) | calls at 225-226 | same s0 channels | same s1 channels — **identical operand pairs to MODULATE** |
| 4 | R_LERP via `lerp8` (173-187, `unit_mul` at 182) | calls at 231-232 | `mag` = \|s1−s0\| per channel (line 181) | `w` = `s_w_q` ← `f_weight_i` port (line 66) |

**Operand ranges: unconstrained.** s0/s1 are full 8-bit texture sample
channels straight from the TMU; `f_weight_i` is a full unit8 per-fragment
weight ("unit8, kLerp only" — the port contract, line 66). No operand is
drawn from a small set. A direct 2-operand table (65,536 × 8 = 524,288 bits)
is ≥52 M10K by capacity and worse by geometry (widest deep mode is 2048×5, so
65,536×8 needs 64 slices per read port) — dead, per multiplier, as the brief
suspected. ARITHMETIC.

**What survives the audit:** (1) mutual exclusivity — only ONE group of four
fires per fragment, and MODULATE/MODULATE2X even share operand pairs exactly,
so four physical multipliers with 2:1 operand muxes compute everything twelve
sites compute; and (2) the single-operand-pair shape fits the quarter-square
identity (§2, candidate A) with a 512-entry table. Both bit-exact.

**But the block is condemned.** `design/prod_manifest.yml:69` marks it
"REFUTED (D19q); delete when v1 is measured", the deletion trigger has FIRED
(`reports/COMBINE-SUPERSESSION-LEDGER-20260907.md` §1: v1's fit landed), and
the island already uses `material_combine_v2`. The best design for these
twelve multipliers is the one the manifest already ordered: deletion.

### 1b. `zhao_texture_material_combine_v2.sv` — the living combiner's two sites

The island's actual combiner has exactly two `*` operators, both registered:

```
fpga/rtl/texture/zhao_texture_material_combine_v2.sv:510-511
  m_p0 <= 17'({8'd0, o_a0} * {8'd0, o_b0}) + 17'd128;
  m_p1 <= 17'({8'd0, o_a1} * {8'd0, o_b1}) + 17'd128;
```

Operands `o_a0/o_b0/o_a1/o_b1` are muxed per recipe/phase at lines 415-489;
same unconstrained 8-bit ranges as 1a (samples, scratch intermediates, the
LERP weight `p_w`). V2 already performed the consolidation that 1a's mutual
exclusivity permits — two lanes, paired phases. MEASURED: 870 ALM / 2 DSP /
6 M10K / 114.04 MHz (`reports/synthesis/zhao_block_fit.json`; note
`rtlCleanAtHead: false` on that row — the number is real but its provenance is
a dirty tree, flagged per CLAUDE.md's receipts law).

### 1c. `zhao_project_core.sv` — nine `mul32` sites and two `fx_mad` products

`mul32` defined at `fpga/rtl/common/zhao_project_core.sv:262-265`. The nine
sites are lines 359-364, three per row sum:

| operand | source | range |
|---|---|---|
| `mat[view_i][0..2, 4..6, 12..14]` | cfg-written registers (lines 331-352), fx16 view-projection coefficients | full signed 32-bit, runtime-writable; only **18 live words** at any moment (2 views × 9), but each is 32 bits wide |
| `vx_i / vy_i / vz_i` | per-vertex ports (lines 214-216), fx16 world coordinates | full signed 32-bit, one new triple per clock at II=1 |

The two viewport products are lines 599-600:

```
prod_x_c = ext32m(s5_ndc_x) * $signed({{(MAD_W-27){1'b0}}, vp_w[s5_view], 15'b0});
```

`s5_ndc_x` is a full s32 saturating quotient; the other operand is
`vp_w`/`vp_h` — **12 bits** (line 327) — padded with 15 literal zero bits to
27. MEASURED total: 33 DSP per instance, twice instantiated
(`reports/DSP-BUDGET-CENSUS-20260908.md`), consistent with 9×3 + 2×3
(calibration: `calib_mul_s32` = 3 DSP, `calib_mulasym_s32x27` = 3 DSP) —
that per-site split is ARITHMETIC from the totals, as the census itself
cautions.

**Operand verdict: unconstrained on both sides at 32 bits.** The coordinate
is per-vertex and full-width; the coefficient is runtime config. Every
whole-product table dies here (2^64 entries; quarter-square needs 2^33
entries), exactly as the brief predicted might happen, and I am saying so
plainly: **no bit-exact memory design replaces these products at a favourable
rate.** The decompositions that remain (§2, candidates D/E) spend the OTHER
desperate resource — ALM — to save DSP, on a block already measured 39% short
of the product clock (`reports/D22-GEOM-PROJECT-FIT-20260907.md`), and are
recommended against.

One genuine memory-for-ALM pocket exists in the core: the divider's rider
signals (§2, candidate C). And one non-memory bit-exact DSP saving fell out
of the audit (§2, candidate F).

---

## 2. Candidate designs

### The bit-exact memory primitive all of this rests on: quarter-square

For unsigned integers a, b ∈ [0,255]:

```
a*b = floor((a+b)^2 / 4) - floor((a-b)^2 / 4)
```

Exact for ALL integer pairs — (a+b) and (a−b) have the same parity, so the
two floors either both lose nothing or both lose exactly 1/4, and the errors
cancel identically. ARITHMETIC (provable identity, not an approximation).
One table: **512 entries × 16 bits** of `floor(x²/4)` for x ∈ [0,510]
(max value ⌊510²/4⌋ = 65,025 < 2^16). Two reads per product (index `a+b`,
9 bits; index `|a−b|`, ≤8 bits).

M10K accounting by §D's rule (`reports/ZHAOZHOU_THE_DECRUFTER_FPGA_TEXTURE_ISLAND_2026-09-08.txt:131-137`):
width 16 → ceil(16/40) = **1 slice**; depth 512 is a native mode. The two
reads per cycle want ONE M10K in true-dual-port mode, whose per-port width
limit is ×20 (512×20 — the mixed-width table the decrufter cites as [V02]),
and 16 ≤ 20. So **1 M10K per multiplier**, both ports reading. Like the
metadata bank in §D, "compatible with one block" still needs the fit to
confirm the mapping. Fallback if TDP inference misbehaves: duplicate the
table into two simple-dual-port ROMs — 2 M10K per multiplier, still cheap
against ~505 free blocks.

The table need not be an init-file ROM: ⌊x²/4⌋ is computable incrementally
(x² = prev + 2x − 1), so a 512-cycle post-reset FSM with one adder fills it
through the write port — no `$readmemh`, no init-file portability question,
conservative-subset friendly.

The clamp in `unit_mul` stays unreachable exactly as before (255×255+128 <
2^17) and is kept defensively, unchanged.

---

### Candidate A — quarter-square ROMs in `zhao_texture_material_combine_v2`
**THE RECOMMENDED MEMORY DESIGN. Bit-exact.**

Replace the two `*` at v2:510-511 with two quarter-square M10K ROMs.

* **Table geometry:** 2 × (512 × 16), true-dual-port, 1 M10K each → **2 M10K**
  (fallback 4). ARITHMETIC by the §D width rule.
* **DSP removed:** the block's DSP count moves to **0** — structural, no `*`
  operators remain. Block MEASURED today at 2 DSP. The island's combiner
  contribution −2.
* **ALM:** moves UP slightly, not down — per lane a 9-bit adder, an 8-bit
  absolute-difference, and a 16-bit subtract land in fabric (the DSP used to
  absorb the add). Direction is a PREDICTION; magnitude deliberately not
  predicted.
* **Added latency: zero clocks.** The current design computes the product
  combinationally between the O and M registers (v2:492-511). The ROM's
  address port register sits at the same O→M edge the product register sat
  at, and read data is valid through the M cycle — the RAM's synchronous
  read IS the M stage. The 16-bit subtract and the `+128` fold into the F
  cone (v2:523-552). PREDICTION on timing: the O-side cone SHORTENS (the
  multiply leaves it), the F-side cone LENGTHENS (subtract joins it). v2
  measured 114.04 MHz against a ~100 MHz product clock, so there is margin;
  whether it survives is the fit's question, not mine.
* **Bit-exact: YES** — provable identity, same 17-bit `p`, same rounding,
  same clamp. Acceptance gate when the build lane reopens: the existing
  oracle differential
  (`tests/texture/material_combine_v2_diff.cpp:test_every_recipe_matches_the_oracle`)
  plus an exhaustive 65,536-pair sweep of the new product path — cheap in
  Verilator, and per CLAUDE.md the detector must be shown to FIRE (mutate one
  table entry, watch the diff catch it, restore).

**WHAT WOULD KILL THIS:**
1. Quartus 17.0.2 failing to map a dual-read 512×16 with one init-writer into
   TDP mode — fallback doubles M10K, which does not kill it, but a fallback
   into FABRIC (LEs) would; the calibration's `ram` family shows sync-read
   patterns that infer and async/reset patterns that catastrophically do not
   (22,000+ ALM). The RTL must use the known-inferring shape and
   `check_ram_inference.py` must see it.
2. The F-cone subtract dropping island Fmax below the product clock — v2 owns
   the island's former worst path; a redesign that reopens that wound costs
   more than 2 DSP buys.
3. The 512-cycle table-fill FSM interacting badly with the island's reset /
   ready protocol (combiner must refuse work until filled — one more state,
   but a real one).

### Candidate B — execute the standing deletion of `zhao_texture_combine`
**Not a memory design; the audit's largest immediate finding.**

The twelve-multiplier block the owner's question points at is REFUTED
hardware whose deletion condition is met and unexecuted
(`design/prod_manifest.yml:69`, `reports/COMBINE-SUPERSESSION-LEDGER-20260907.md` §1).
Executing it — and retiring v1 to `excluded: superseded` once `zhao_prod_top`
instantiates v2 (ledger §2) — moves the manifest sum:

* delete `zhao_texture_combine`: **−8 DSP, −494 ALM, 0 M10K spent** (MEASURED
  row, recovered full-fit: 494 ALM / 100.12 MHz / 8 DSP,
  `reports/GATE1-AND-MAPONLY-BATCH-20260909.md`).
* v1 → v2 in the top: **−793 ALM** (1,663 → 870), DSP unchanged at 2
  (both MEASURED; v1's row map-only, v2's row dirty-tree — flagged).
* then Candidate A takes the remaining 2 DSP.

Combined with A: **texture-combiner arithmetic contributes 0 DSP to the
machine, for 2 M10K.** Deletion is an owner call — the ledger says so and I
repeat it; this report only ranks it.

**WHAT WOULD KILL THIS:** an owner ruling that the II=1 combiner is still
wanted as a block in its own right. (If so, Candidate A's scheme applies to
it too: 4 consolidated lanes × 1 M10K = 4 M10K for −8 DSP, +1 clock latency
because its arithmetic is currently single-stage — zhao_texture_combine.sv:
122-134 — unlike v2's registered product. The consolidation is licensed by
the mutual exclusivity in §1a.)

### Candidate C — the divider's rider delay-line into an M10K shift register
**The one real memory-for-ALM move in `zhao_project_core`. Bit-exact.**

The 31-stage restoring divider carries per-vertex signals that are never
touched between stage 3 and stage 5: `neg[2:0]`, `sat[2:0]`, `behind`,
`view`, `pay[15:0]` — 24 bits at PAYLOAD_W=16 (zhao_project_core.sv:434-441,
482-508). Registers: 24 × 31 = 744 per core, 1,488 across both instances
(ARITHMETIC; the block's 7,250 measured registers are dominated by the
divider — 63×3×31 = 5,859 for the working lanes alone). The working lanes
(`dv`, recomputed every stage) and `d` (read by every stage's compare,
line 471) **cannot** move to memory; the riders can — a pure 31-deep delay
line, `en_i` as the clock enable, exactly what M10K-based shift registers
(altshift_taps or inferred) implement.

* **Geometry:** 31 deep × 24 wide → 1 slice by the §D rule → **1 M10K per
  core, 2 total** (24 ≤ 40; single read, single write, SDP).
* **Saves:** ~744 registers per core; the ALM saving is a PREDICTION in
  direction only (registers and their packing pressure move down; the ALM
  number depends on whether register or ALUT packing limits the block, which
  only a fit answers). DSP: 0. Latency: 0 — same 31-stage delay.
* **Why it doesn't happen today:** every rider register has an async reset
  (`negedge rst_n`, line 482), which blocks shift-register extraction. The
  design change is removing reset from the rider chain — bit-exact for every
  consumed output because `valid` (which must STAY a real registered chain)
  gates consumption of the riders, so post-reset garbage sits behind
  `valid=0` and is never read.
* **Bit-exact: YES** — a delay line is a delay line.

**WHAT WOULD KILL THIS:**
1. `busy_o` and the valid chain must remain FF-based (they are read per
   stage, lines 710-718) — only the blind riders move; if PAYLOAD_W grows the
   saving grows, if a future consumer taps a mid-pipeline rider the scheme
   dies for that signal.
2. X-propagation in simulation from the unreset memory annoying the
   differential suites — needs the mutant/assertion discipline, not a design
   retreat.
3. If register pressure is NOT what limits the core's ALM count, the saving
   rounds to little — this is the candidate most exposed to the
   falsified-magnitude trap, hence no number.

### Candidate D — chunked quarter-square for the nine 32×32 products
**Feasible, bit-exact, and recommended AGAINST.**

Decompose each 32×32 into 16 8×8 partials, each via a quarter-square M10K:
16 M10K per multiplier, **144 M10K per core, 288 for both** (fits the ~505
free — capacity is genuinely not the constraint). But the 16 shifted partials
must be summed in fabric: a ~64-bit compressor tree per product, nine per
core. That is a large ALM SPEND (direction certain, magnitude not predicted)
to save 27 DSP per core, on a block measured 61.09 MHz against ~100 with the
worst path already core-internal (D22). It trades the resource the owner is
desperate for in one column against the other, and worsens the third
(timing).

**WHAT KILLS IT (already dead on these):** the ALM bill lands in the same
cone that is 39% short; and signed handling (magnitudes + sign fixup, or
Baugh-Wooley in fabric) adds more fabric still.

### Candidate E — radix-16 distributed arithmetic on the matrix operand
**The "one operand is semi-constant" idea, followed to its end. Rejected.**

The 18 live matrix words invite precomputation: per coefficient, a 16-entry
table of `m*d` (d = 0..15, 36-bit entries), rewritten by shift-add over 16
cycles at cfg-write time; a product then needs 8 nibble-lookups + a shifted
8-term sum. II=1 requires 8 reads/cycle → 4 TDP pairs... but at 36-bit width
the TDP ×20 limit forces 2 blocks per 2 reads: **8 M10K per coefficient, 72
per core** — and the same fabric adder trees as D, per row. Same kill: ALM
and Fmax on the wrong block. The audit's honest conclusion stands — the
coefficient being one of 18 values does not make it NARROW, and width is what
tables price.

### Candidate F — non-memory riders the audit surfaced (recorded, not claimed)

* **fx_mad trailing zeros:** the viewport products multiply by a 12-bit value
  padded with 15 literal zeros to 27 bits (core:599-600). Rewriting as
  `(ndc * vp_w) <<< 15` makes the product s32×12. Calibration MEASURES
  s32×27 at 3 DSP and s32×18 at **2 DSP** (`calib_mulasym_s32x18`), and the
  33-DSP total is consistent with the fitter NOT exploiting the zeros. If it
  holds: −1 DSP × 2 sites × 2 cores = **−4 DSP, 0 M10K, bit-exact** (the
  shift distributes over the identical integer product). PREDICTION until a
  MapOnly rules; the census's warning about inferred per-site attribution
  applies squarely.
* The census's standing levers — narrow to ≤27 bits (33 → 11 per core,
  needs the OWNER's world-coordinate proof, `docs/OWNER_DOCKET.md`
  2026-08-24), share one core (−33, an architecture/schedule question), and
  time-multiplex rows (−12ish, breaks II=1) — remain the only routes to the
  66-DSP prize. None is a memory design; none is this report's to decide.
* A reciprocal table for the divider is EXCLUDED by the core's own law:
  "No reciprocal reproduces it bit-for-bit, so a real divider is required"
  (core:101-102). Any log/antilog or PWL scheme anywhere in scope is a spec
  change needing an owner ruling; none is proposed because bit-exact
  alternatives exist wherever memory helps at all.

---

## 3. Recommendation, ranked by (DSP + ALM bought) per M10K spent

Bit-exact designs first, per the brief — and every entry here IS bit-exact;
no approximate scheme earned a place at any rank.

| rank | candidate | DSP | ALM | M10K spent | latency | bit-exact |
|---|---|---|---|---|---|---|
| 1 | **B: execute the texture_combine deletion (+ v1→v2 in the top)** | −8 | −1,287 | **0** | n/a | n/a (removal) |
| 2 | **A: quarter-square ROMs in material_combine_v2** | −2 | +small | 2 | +0 clk | YES |
| 3 | **F: fx_mad trailing-zeros rewrite in project_core** | −4 (predicted) | ~0 | 0 | +0 clk | YES |
| 4 | **C: divider rider delay-line → M10K, both cores** | 0 | −(direction only) | 2 | +0 clk | YES |
| — | D: chunked quarter-square, project_core rows | −54 | **+large (spends)** | 288 | +0 clk | YES — rejected on ALM/Fmax |
| — | E: radix-16 DA on matrix operand | −54 | **+large (spends)** | 144 | +0 clk | YES — rejected on ALM/Fmax |

Rank 1 is an owner call already teed up by the supersession ledger; ranks
2-4 are engineering that batches into one fit gate per the
fit-at-subsystem-boundaries rule. Together, ranks 1-3 take the combiner
arithmetic to 0 DSP and the manifest sum down ~14 for 2 M10K — real, but the
honest close is that **memory does not reach the project_core prize.** The
remaining ~28 overshoot (plus 42 unmeasured blocks) is answered by the
non-memory levers in Candidate F's second bullet, of which width-narrowing
(−44 across both cores, per the measured 27-bit cliff) is the largest and
waits on the owner's world-coordinate bound, not on any table.

## 4. Claim provenance

* **MEASURED:** all fit rows quoted (block_fit.json; texture_combine's
  recovered full-fit row; v2 870/2/6/114.04 — dirty tree, flagged; the two
  project rows at 33 DSP — terrain's dirty, flagged; calibration DSP bands
  incl. the 27→28 cliff and s32x18 = 2); island M10K ~48; manifest DSP sum
  154-as-floor.
* **ARITHMETIC:** quarter-square exactness; table geometries and §D slice
  counts; the 65,536×8 full-table death; 2^33/2^64 table deaths at 32 bits;
  the 9×3+2×3 = 33 decomposition (inference from totals, as the census
  labels it); rider register counts (24×31×2).
* **PREDICTION (direction only, per the falsified-magnitude law):** A's ALM
  up-tick and F-cone timing; C's ALM saving; F's −4 DSP pending MapOnly;
  TDP mapping of a dual-read 512×16 in Quartus 17.0.2.

Verification debts when the build lane reopens: MapOnly the fx_mad rewrite
and the quarter-square lane (RowLabel MANDATORY — GATE1's Mistake 1);
exhaustive 64K product sweep + oracle differential for A; fire-test the
sweep on a mutated table entry (committed mutant, per CLAUDE.md).
