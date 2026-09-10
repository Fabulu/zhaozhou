# POSE-DECODE SEQUENCED — owner ruling R4 implemented, with corrections

Run: `RUN-20260909-2343-pose-decode-sequenced` · lane: `zhao_geom_pose_decode`
+ `zhao_geom_quat2mat` + `zhao_geom_mat3x4_mul` + tests · **no fit run, no
commit made** (both per brief). Ruling: Fabian, 2026-09-09 — *"Pose question:
relax the 1 bone/clock rule. It must have been arbitrary."*
(`reports/OWNER-RULINGS-20260909-2300.md` §R4, `GEOM.POSE.md` throughput line.)

## What shipped (working tree, awaiting review)

| File | Change |
|---|---|
| `fpga/rtl/geometry/zhao_geom_quat2mat.sv` | `MUL_LANES` param: **1 (default)** = one 16x16 lane, 9-step operand-mux sequencer, 10-cycle walk; **9** = the original spatial arm, kept verbatim under `generate` |
| `fpga/rtl/geometry/zhao_geom_mat3x4_mul.sv` | `MUL_LANES` param: **1 (default)** = one 32x32 lane, issue/commit pipeline, 37-cycle walk; **3** = the original element-serial arm, kept verbatim |
| `fpga/rtl/geometry/zhao_geom_pose_decode.sv` | `MUL_LANES_QUAT`/`MUL_LANES_MAT` pass-through params (defaults 1/1). **FSM, ports, handshakes untouched.** Header cost text updated to measured numbers |
| `tests/geometry/geom_quat2mat_directed.cpp` | latency-tolerant wait + exact walk-length law (`ZHAO_QUAT2MAT_WALK`, default 10) — see "brief corrections" §B4 |
| `tests/geometry/geom_mat3x4_mul_directed.cpp` | walk pin 12 → `ZHAO_MAT3X4_WALK` (default 37) — §B4 |
| `tests/geometry/geom_pose_decode_directed.cpp` | additive only: `palettes_decoded_o` now asserted to fire (was a hopeful zero), measured-cycles info line |
| `tests/geometry/geom_quat2mat_mutant_control.cpp`, `geom_mat3x4_mul_mutant_control.cpp` | inverted-polarity positive controls, new |
| `tests/mutants/zhao_geom_quat2mat_mutant.sv`, `zhao_geom_mat3x4_mul_mutant.sv` | committed mutants, one substantive line each, new |
| `tests/CMakeLists.txt` | 5 new targets: `geom_quat2mat_spatial`, `geom_mat3x4_mul_elem`, `geom_pose_decode_spatial` (legacy arms on the same oracle), two mutant controls |
| `design/contracts/GEOM.POSE.md` | latency section corrected + measured; R4 note gains a three-point correction; relaxed target declared: **one bone per ≤120 clocks on miss**; directed-tests section names the new instruments |

`zhao_prod_top.sv` instantiates `zhao_geom_pose_decode` with **no parameter
overrides and an unchanged port list** (u28_i, `fpga/rtl/prod/zhao_prod_top.sv:1857`),
so the new defaults reach production composition with **no regeneration needed**
and no PINMISSING exposure.

## A. The brief needed corrections — four of them

The brief asked for the ninth refused claim. There are four, and the largest
changes the headline number.

**B1. "The old target forced the ~12-product decode to be spatial" — half
false.** `zhao_geom_mat3x4_mul` was *already* element-serial (12-cycle walk,
3 products/cycle) and *already* shared across both matrix multiplies by
pose_decode's FSM. Measured at the legacy parameters: **56.2 cycles/bone**. The
pre-R4 RTL never met 1 bone/clock; the target shaped the two engines' internal
widths (9 spatial quat products; 3 products/cycle) but the throughput it named
never existed in the tree. This *supports* the owner's "arbitrary" — the target
was already fictional — but it means the ruling bought less than the census
suggested.

**B2. "12 multiplies/bone … 2.9% of a frame" — undercounted 6.75×.** The chain
per bone (reference's own order, `zref::creature::decode_pose`) is 9 quat
products + 36 (A_parent·LR) + 36 (A_b·inv_rest) = **81 products/bone** (45 for
bone 0), plus ~34 cycles/bone of non-multiplier walk (13-cycle ancestor-store
read, 12-cycle store, handshakes). Measured worst legal frame: **28.4%**, not
2.9% (§C). Still meets demand — but with 3.5× headroom, not 34×.

**B3. "~18 → ~3, expected return 17" — internally inconsistent and unreachable
as stated.** 18−3 is 15, not 17, and both endpoints are wrong at the measured
calibration (verified directly from `tools/budget/calibration.json`,
`dspBlocks`: s16→1, s27→1, s28→3, s32→3):

| Arrangement | DSP | Return | Cost |
|---|---|---|---|
| Pre-R4 (9 × 16x16 + 3 × 32x32) | 18 | — | measured baseline (inherited) |
| **This work: 1 × 16x16 + 1 × 32x32** | **4** | **14** | 115.4 cycl/bone, module boundaries intact, tests intact |
| Quat products folded into the 32x32 lane | 3 | 15 | crosses the module boundary; quat2mat loses standalone identity and its directed suite; +1 DSP for that price — named, not taken |
| One ≤27-bit lane, 32x32 as four ≤17-bit partials | 1 | 17 | ~4 cycles/matrix-product → ~330 cycles/bone → **~81% of a frame at full churn**; thin margin, large complexity — named, not taken |

**The honest return is 14 DSP, not 17.** The DSP-path ledger in
`OWNER-RULINGS-20260909-2300.md` ("−17 pose_decode") overstates this lever by 3;
whoever holds that ledger should re-run the 109-against-94 sum with −14
(→ 112 against 94).

**B4. "The existing directed tests must pass unchanged" — impossible as
written.** Three spots were latency-PINNED, not latency-tolerant: quat2mat's
`diff()` read `m_o` one tick after accept with no look at `m_valid_o`; quat2mat
§8 asserted `m_valid` high one tick after accept; mat3x4 §7 asserted
`cycles == 12` exactly. "Latency may grow" and "tests unchanged" cannot both
hold. Resolution: **every bit-exactness check is byte-identical**; the three
timing pins moved to the new declared walks and stayed *exact* (10 and 37, not
`<=` loosenings), with the macro moving in lockstep with `MUL_LANES` in the
legacy variants. Nothing was weakened; the walk is still a law.

Also checked as instructed: **128 tuples/frame is a clamp, not a capacity
misread** — `GEOM.POSE.md` overflow section: "a frame demanding more is a
content-tier violation, counted and clamped deterministically". It is the legal
worst-case decode load. The army-economy corroboration (shared decoded pose per
type/clip/tick) is consistent with it; accepted.

## B. Verification — all run standalone, shared `build/` untouched

Per the coordinator's contention warning: every build below is a standalone
verilator build into gitignored `build-poselane/<target>/` with its own Mdir
(`-std=gnu++17`, absolute `-I`, space-free path). No `ctest`, no shared-tree
build, no edit to `design/fit_targets.yml` or `design/prod_manifest.yml`.

| Gate | Result |
|---|---|
| `verilator --lint-only -Wall`, defaults and legacy params | clean, 0 diagnostics |
| `tools/quartus/check_quartus17_syntax.py` (220 files) | clean (elaboration guards in `initial begin`; explicit `generate/endgenerate`) |
| `tools/quartus/check_prod_manifest.py` | OK — "215 modules, 66 tops, 78 inside, 71 excluded; every module counted once or declared absent" |
| `geom_quat2mat_directed` @ MUL_LANES=1 | 352 checks passed (incl. exact 10-cycle walk) |
| `geom_quat2mat_directed --random 500` @ 1 | 6,500 checks passed (full-s16 adversarial range) |
| `geom_quat2mat_directed` @ MUL_LANES=9 (walk 0) | 352 checks passed — legacy arm bit-identical |
| `geom_mat3x4_mul_directed` @ 1 | 176 checks passed (incl. exact 37-cycle walk, saturation rails, mid-walk refusal) |
| `geom_mat3x4_mul_directed --random 300` @ 1 | 3,600 checks passed |
| `geom_mat3x4_mul_directed` @ MUL_LANES=3 (walk 12) | 176 checks passed |
| `geom_pose_decode_directed` @ 1/1 | 694 checks passed; **32-bone chain = 3,694 cycles (115.4/bone)** |
| `geom_pose_decode_directed --random 40` @ 1/1 | 8,352 checks passed (whole-chain differential vs `zref::creature::decode_pose`) |
| `geom_pose_decode_directed` @ 9/3 | 694 checks passed; 32-bone chain = 1,799 cycles (56.2/bone) |
| `geom_quat2mat_mutant_control` (INVERTED) | PASSES: differential **FIRES**, 4/12 elements diverge on the strong vector, 0/12 on identity (the weak-vector lesson), walk still 10, counters balance |
| `geom_mat3x4_mul_mutant_control` (INVERTED) | PASSES: differential **FIRES**, 7/12 elements diverge, element 0 exactly right (boundary-fault signature), `products_done_o` still 1 — counters blind, differential is the only detector |

The checker was therefore **seen to fail** on two deliberate breaks, one per
new sequencer, each a single substantive line, each a fault class the pre-R4
spatial arms could not exhibit (a schedule has to exist to be mis-slotted; an
accumulator has to exist to go uncleared). `palettes_decoded_o`, previously
asserted nowhere, is now seen to advance once per palette in every directed
fixture and every random iteration.

Bit-exactness argument, in one line per engine: quat products are exact s32
integers whichever cycle each is computed in, the pair-sums and single
`rescale(·,11)` are shared text between arms; matrix partial sums are exact
s67 integers (2 products ≤ 2^63 < 2^66), associativity of integer addition
gives the same s67 total the one-cycle sum formed, then the same single
`rescale(·,16)`. Sequencing moves cycles, never bits — and 19,000+ differential
checks against the frozen `quat16` law (qformats §7.6 amendment C1) agree.

## C. Demand and throughput, re-derived in clocks

- Denominator: `computeClocksPerFrame = 1,666,666`
  (`design/budgets/workloads.yml:51`). **[verified]**
- Worst legal frame: 128 decoded tuples (the contract's clamp) × 3,694 cycles
  (measured, 32-bone palette, defaults) = **472,832 cycles = 28.4%** of the
  frame, on this block's own dedicated sequencer — utilisation of an otherwise
  idle unit, not contention for a shared one. Misses stall only the requesting
  instance stream (contract backpressure law, unchanged).
- Typical frame: the Phase-9 health metric is ≥~90% pose-cache hit rate, so
  ≤~13 misses/frame ≈ 48k cycles ≈ **2.9%** — the ruling's number turns out to
  describe the *typical* frame at the *new* arrangement, coincidentally.
- Declared walks: quat 10 cycles accept→valid; matrix 37; whole bone ≈115.4
  measured (the extra ~34 over 81 products is ancestor-store read 13 + store 12
  + handshake edges). Contract now declares **one bone per ≤120 clocks on
  miss**.
- Pre-R4 comparison: 56.2 cycles/bone. The relaxation costs a factor 2.05 in
  decode latency, worst-frame 13.8% → 28.4%, for −14 DSP.

## D. DSP bill — MEASURED / STRUCTURAL / UNKNOWN

- **MEASURED (instrument named):**
  - calibration points `calib_mul_s16_n1_ioreg` dspBlocks=1,
    `calib_mul_s32_n1_ioreg` dspBlocks=3, `s27`→1, `s28`→3 (Quartus 17.0.2 map,
    `tools/budget/calibration.json`) — read directly, not quoted from prose;
  - all cycle counts and walk lengths above (Verilator, exact-latency checks);
  - bit-identity (19k+ differential checks vs `zref::PoseBank`'s decode chain).
- **INHERITED, not re-measured:** the 18-DSP baseline (9+9 for the two
  submodules) — from the brief/census; consistent with the calibration
  (9×1 + 3×3) but this lane did not re-run the census.
- **STRUCTURAL PREDICTION:** post-change total = **4 DSP** (one s16 site, one
  s32 site; zero multipliers elsewhere in the three files — grep-verified one
  `*` per generate arm). ALM cost of the two sequencers and the 12:1×32-bit
  operand muxes: modest, unpriced. Fmax: the mat operand mux feeds the
  multiplier's input cone; `zhao_project_core` chose a shifting hold bank over
  exactly this shape for Fmax — **recorded fallback if the fit gate shows the
  cone gating**.
- **UNKNOWN until the gate:** actual DSP/ALM/Fmax/M10K of the composed block.

## E. Implementation order and THE ONE fit gate (not run)

1. ✅ RTL (both arms), tests, mutants, contract — this working tree.
2. Review + commit (owner side; this lane made no commit).
3. Shared-tree CMake configure via `cmake --preset` when the island lane's
   `build/` contention clears, then the five new ctest targets once. (Their
   standalone equivalents already ran green; this step is graph hygiene, not
   new evidence.)
4. **FIT GATE (the one, batched at the geometry-subsystem boundary per the
   fits-at-subsystem-boundaries law):** the existing leaf target
   `zhao_geom_pose_decode` (`design/fit_targets.yml:1534`, closure = exactly
   the three RTL files of this lane). **The question it answers:** *"Does the
   composed decode chain at MUL_LANES defaults measure 4 DSP, keep the
   ancestor store in M10K, and hold geometry_mantle Fmax with the 12:1
   operand-mux cone on the 32x32 lane?"* Expected: 18 → 4 DSP; if Fmax fails,
   the hold-bank fallback (structural, no law change).
5. After the fit: whoever owns `design/blocks.yml` / the DSP census updates the
   pose rows (this lane was ordered out of `blocks.yml` and did not touch it;
   its census figures for quat2mat/mat3x4 are now stale-by-design until step 4
   — the FIXED-NEVER-RE-MEASURED detector will rightly flag them).

## F. Not verified, item by item, instrument named

- **DSP = 4:** structural prediction only. Instrument: the step-4 leaf fit
  (`quartus_map` DSP row). Not run — brief forbade fits.
- **Fmax / ALM delta:** unknown. Same instrument, same gate.
- **Quartus synthesizability of the new arms:** `check_quartus17_syntax.py`
  clean + the two known-fatal forms avoided (guards in `initial begin`,
  explicit `generate/endgenerate`) — but "a block that has never been through
  `quartus_map` has not been shown to be synthesizable" (CLAUDE.md). The 33 s
  map at step 4 settles it loudly.
- **M10K inference of the ancestor store:** unchanged code, but inference is a
  fit-time fact. Step-4 instrument.
- **The 18-DSP baseline:** inherited from the census, not re-measured here.
- **Shared-tree ctest of the five new CMake targets:** not run (`build/`
  contended by the island lane at hand-off). Standalone builds of identical
  source/parameter combinations ran green; the CMake stanzas are pattern
  copies of neighbouring targets. Instrument: one `ctest -R "geom_(quat2mat|
  mat3x4|pose_decode)"` after `cmake --preset` regeneration.
- **Interaction with per-instance pose-override plans (CapeProvisions D6):**
  unaffected by reasoning (cache key and patch layer sit above this block);
  no instrument exists yet.

## G. Waste ledger

`build-poselane/` (gitignored by `build-*/`) holds the eight standalone build
dirs (~200 MB with objects). Safe to delete wholesale after review; every
number it produced is recorded here and in the run's TASK_LOG. The purge tool's
48 h grace covers it either way.
