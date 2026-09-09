# TERRAIN.SHADE — implementation report

2026-09-09, branch `zixxtrixx-v8-closeout`, run `RUN-20260909-2235-terrain-shade-rtl`.
Answers the brief: build the block whose absence means production terrain has
no lighting at all. Nothing was committed (standing order: owner reviews);
no Quartus fit was run (standing order); the ONE fit gate is named in §6.

Every number below is labelled **MEASURED** (an instrument ran this
session), **STRUCTURAL PREDICTION** (follows from the RTL's shape; the named
fit verifies it), or **UNKNOWN** (no instrument has answered; the instrument
is named).

---

## 0. The verdict, up front

**The exact law costs ZERO DSP, ONE M10K, and ~700 ALM (structural), at
II = 147 against a demand of one triangle per 833 clocks.** The old
estimate — ~730 ALM, **10 DSP**, II=1 — was paying 10 scarce DSP blocks for
833x more throughput than the machine asks for. The brief's instinct
("find out what the exact law actually costs") was right: the expensive
parts of `dot(n,L)/|n|` are three squares, three products, a floor-sqrt and
a floor-divide, and at once-per-triangle rate every one of them is
affordable as a sequenced add/sub walk plus one table.

Built and verified this session (working tree, uncommitted):

| file | status |
|---|---|
| `fpga/rtl/terrain/zhao_terrain_shade.sv` | NEW — the block, 491 lines |
| `tests/terrain/terrain_shade_rtl_directed.cpp` | NEW — 4,142-check differential vs the COMPILED law |
| `tests/CMakeLists.txt` | +3 ctest lanes (directed, `--break-oracle` WILL_FAIL, lint) |
| `design/contracts/TERRAIN.SHADE.md` | amendments A1–A6 (two of them fix the contract itself, §2) |
| `design/blocks.yml` | TERRAIN.SHADE row: latency, throughput RESOLVED, counters, tests |
| `design/prod_manifest.yml` | `zhao_terrain_shade: unused` — an OPEN deferral, §7 |
| `design/fit_targets.yml` | leaf target with the fit's question stated in advance |

MEASURED headline: **lint 0 diagnostics (`-Wall`, verilator_bin 5.051)**;
**Quartus-17 form gate clean (219 files)**; **4,142 / 4,142 checks** against
the compiled `shade_flat_tri_dir_unclamped`; **`--break-oracle` FAILS**
(1/4,142 — the checker seen to fail); **fixed latency 145 cycles**,
data-independent; **all four counters fired with EXACT counts**
(shaded 3,723 / degenerate 75 / saturated 23 / mismatch 31).

---

## 1. What "bit-exact against the law" concretely is

The ratified law (`reference/src/zrender/terrain.cpp:65`) takes VERTICES:
cross product → `rescale(.,16)` per lane → that half is **TERRAIN.NORMALS**,
already UNIT_VERIFIED at 41,731 checks. This block is the law's back half,
consuming the normal unchanged (the contract's "no normal recomputation"):

    nmag2 = fx^2 + fy^2 + fz^2          // UNSIGNED 64 — 3*2^62 = 1.38e19
                                        // overflows signed 64; the contract's
                                        // named RTL hazard
    if (nmag2 == 0) return 0            // the degenerate arm
    ndot  = fx*lx + fy*ly + fz*lz       // wide signed (s66 suffices: proof §3)
    return div_rhu_s128(ndot, isqrt_u64(nmag2))
                                        // floor((ndot + mag/2) / mag),
                                        // then the INT32 clamp

The test therefore runs TWO tiers (both MEASURED, both in the one suite):

* **Tier 1 — composed:** random/directed TRIANGLES; the DUT is fed
  `zref::terrain::face_normal` (the thin view TERRAIN.NORMALS is verified
  against) and compared against the **compiled**
  `shade_flat_tri_dir_unclamped` on the vertices, linked from
  `libzhao_zref.a`. Passing means NORMALS→SHADE reproduces the ratified
  vertex-to-light law end to end. 1,205 triangles including exact
  degenerates and the ±4096-world-unit fx16 band.
* **Tier 2 — full port domain:** raw normals with no triangle preimage
  (rails, `INT32_MIN` triples, one-LSB normals, zero) against the law's own
  pieces — `shade_nmag2`/`shade_ndot` (thin view), `isqrt_u64` (qformats
  §7.2 header), and the **compiled** `div_rhu_s128` — composed exactly as
  the law's statement sequence. 2,515 packets, sun drawn from the FULL
  int32 domain (which is the law's domain — the function accepts any int32
  light) plus the unit-sun moving band, biased near-degenerate and
  near-rail per the contract's randomized-test clause.

No second implementation of the law exists anywhere in the lane — the
contract's own history (a duplicate oracle whose 12 checks passed against
itself) is why the oracle is the compiled reference and not a model.

## 2. Two errors found in the standing documents (and one in the brief)

**E1 — the contract's sun format was s1.15; the law's is Q16.16.** The
packet table said `signed 16, s1.15` for the sun and `s1.15 ±32767` for
`base_o`. That is the exact error the oracle's own header records catching
on 2026-09-03 ("a factor of two in the relief"): the renderer's light is
Q16.16 — 26758/53521/26758 — and **53521 is odd**, so no s1.15 port can
carry it at all; and a fully lit face is `0x10000`, which s1.15 cannot
hold. The thin view and the oracle test were corrected then; the contract's
tables never were — the frozen-copies corollary, live again. Fixed in
place with amendment markers (A1/A2), so the wrong row cannot be quoted
onward.

**E2 — the brief inherits E1.** "Turn a triangle's face normal into a light
term, `dot(n, L)/|n|` in s1.15" — bit-exactness against
`shade_flat_tri_dir_unclamped` and an s1.15 output are mutually exclusive;
the law returns Q16.16 int32. The brief's own non-negotiable (bit-exactness)
wins, per its own instruction. Ports are Q16.16 signed 32 both ways.

**E3 — the ledger's Pareto question is stale twice over.** `blocks.yml`
said "the producer delivers ~1 triangle per 3 clocks"; the producer has
been II=8 since its one-multiplier sequencing plus product register. And
the II=1-vs-II=3 question priced DSP shapes only; the rescue's §14.1/F4
ruling (exact law, no rsqrt/NR) plus the demand number dissolve it — see §4.

## 3. The arithmetic decision — how |n| is handled and what it costs

**The M10K is spent where a table is EXACT; the root and divide need no
table at all.**

* **The six 32x32 products** (three squares for `nmag2`, three sun products
  for `ndot`) all come from ONE quarter-square table:
  `Q[s] = floor(s^2/4)` for `s in [0,511]`, 16 bits wide
  (`floor(510^2/4) = 65025 < 2^16`), and for any bytes `a, b`:

      a*b = Q[a+b] - Q[|a-b|]

  exact, because `(a+b)` and `(a-b)` share parity — both even means both
  squares divide by 4 exactly; both odd means each floor drops exactly 1/4
  and the difference cancels. **Shown, not asserted:** the suite checks the
  identity EXHAUSTIVELY over all 65,536 byte pairs before driving a single
  packet (MEASURED), and the 3,720 end-to-end packets exercise the table on
  every product. A 32-bit operand is four bytes, so a square is 10
  byte-products (4 self + 6 doubled cross) and a full product 16; the walk
  is 3·10 + 3·16 = **78 table cycles** on the ROM's two ports (one product
  per cycle). Operands are magnitude/sign split at accept (|INT32_MIN| =
  2^31 fits unsigned 32), signs applied per accumulation.
  The table is **rebuilt after every reset in 512 cycles** from the
  `(s+1)^2 = s^2 + 2s + 1` recurrence — pure adds, no init file, memory
  untouched by reset so it stays M10K-inferable (rescue §4.1; the bump
  block is the committed precedent). `table_ready_o` gates `tri_ready_o`
  (MEASURED: ready in 512 cycles, ready low before).
* **|n| is computed EXACTLY, not approximated**: `isqrt_u64` is the law's
  own restoring digit recurrence (qformats §7.2), 32 add/sub iterations
  over the u64 sum. It costs zero DSP, zero M10K, and — because `nmag2` is
  complete after the 30 square steps while the sun walk still has 48 to
  go — **zero cycles**: the root finishes at walk cycle ~63 of 79.
* **The divide is `div_rhu_s128` verbatim**: `h = ndot + (mag >> 1)`, then
  FLOOR(h / mag) as a 64-step restoring divide over |h| with the
  floor-adjust (`q = -(uq + (rem != 0))`) on the negative side, then the
  law's INT32 clamp. The quarantined normalmap draft died on exactly this
  divider (32 steps over a 64-bit numerator — quotient bits 63..32, every
  base silently zero); the overhead-sun directed case pins it (MEASURED:
  flat face, L=(0,1,0) → exactly `0x10000`).
* **The priced alternatives, refused with reasons:**
  * *rsqrt table in M10K + Newton (the old 10-DSP shape):* no table of
    reciprocal roots reproduces the exact quotient for every input;
    refused by the exact-law ruling (rescue §14.1, bump report F4) — and
    unnecessary, since the exact root is an add/sub iteration anyway.
  * *avoid the divide via a scaled threshold comparison:* the consumer
    needs the VALUE — TERRAIN.NORMALMAP adds its detail term to `base`
    before the per-light clamp, and the colour fold multiplies by it. A
    predicate is not a light term. Refused on what the consumer actually
    needs.
  * *DSP multipliers (the producer's own pattern, one shared 33x33):*
    correct shape at NORMALS' II=8, but 3 DSP blocks per the calibration
    table on a console at 171% DSP commitment, to buy throughput §4 shows
    nobody demands. Refused by the rescue's axis priorities.

**Width proofs** (the contract's overflow clause, carried in the RTL
header): products ≤ 2^62; `nmag2 ≤ 3·2^62 = 1.38e19` — past signed 64,
inside unsigned 64 (rail case MEASURED: all-`INT32_MAX` and all-`INT32_MIN`
normals, exact); `|ndot| ≤ 3·2^62` fits signed 66; `|h| < 2^64` so the
divide numerator is the u64 magnitude with the sign beside it; the quotient
register is the full 64 bits because a one-LSB normal under a rail sun
legitimately reaches the law's INT32 clamp (MEASURED: both clamp rails hit
and exact).

**Zero DSP is grep-verifiable:** the only `*` operators in the file are
elaboration-time constants (`510*510`, `3*SQ_STEPS`) and the power-of-two
`8*ia_c` inside an indexed part-select, which is wiring. No nonconstant
multiply exists to infer a DSP from. (STRUCTURAL — the fit confirms.)

## 4. The per-frame rate argument

* Demand: **2,000 terrain triangles/frame** against
  `computeClocksPerFrame = 1,666,666` (`design/budgets/workloads.yml`;
  `zhao_terrain_normals`' header derives the same number and calls its own
  II=8 "833x over-provisioned"). That is **833 clocks available per
  triangle** at matched rate.
* This block: latency **145 cycles MEASURED**, fixed, data-independent
  (asserted equal across a trivial, a rail, and a degenerate packet);
  II = 147 STRUCTURAL (145 + one output-handshake + one accept cycle).
* Capacity: 1,666,666 / 147 ≈ **11,300 triangles/frame = 5.7x demand**.
* Chain view, honestly: SHADE at II=147 is the terrain-geometry chain's
  slowest link (NORMALS is II=8), so 2,000 triangles take ~294k clocks of
  chain time — commensurate with the 276,480-fragment terrain raster
  stream (~138 clocks of fragments per triangle) and 18% of the frame. If
  a future composition needs the chain faster, the named levers are: a
  second quarter-square M10K (halves the walk to ~40 cycles), or 2-bit
  divide steps (−32 cycles) — both stay at zero DSP. Per the contract's
  ordering rule, that unused parallelism deliberately lives HERE, not in
  the per-fragment blocks.
* The contract's II=1 (10 DSP) and II=3 Pareto rows are therefore not
  fitted: both answer a rate 100–800x above demand with a resource the
  rescue says the machine does not have. Recorded as amendment A3.

## 5. The bill

**MEASURED this session** (instrument named):

| item | value | instrument |
|---|---|---|
| lint | 0 diagnostics, `-Wall` | verilator_bin 5.051 |
| Quartus-17 form gate | clean, 219 files (self-test 3 fire / 6 no-fire) | `tools/quartus/check_quartus17_syntax.py` |
| bit-exactness | 4,142 checks, 0 failures, vs COMPILED law | `tests/terrain/terrain_shade_rtl_directed.cpp` + `libzhao_zref.a` |
| checker alive | `--break-oracle` FAILS (1/4,142) | same suite, positive control |
| quarter-square identity | exact over all 65,536 byte pairs | same suite, exhaustive pre-check |
| counters | all four fire, EXACT: 3,723 / 75 / 23 / 31 | same suite, tallied not asserted |
| latency / fill | 145 fixed / 512 cycles | same suite, counted |
| backpressure | packet holds under stalls (valid/base/src stable) | same suite, stall injection |
| manifest | **OK — every module counted once or declared absent** (215 modules; mid-session the check showed 2 UNACCOUNTED from a concurrent lane, §7, which that lane then registered) | `tools/quartus/check_prod_manifest.py` |

**STRUCTURAL PREDICTION** (fit gate G-SHADE1 verifies):

| piece | ALM | DSP | M10K |
|---|---|---|---|
| quarter-square table 512x16, TDP (port A read-during-fill-write) | ~15 | | **1** |
| accumulators (u64 + s66) + 7-way shift mux | ~180 | | |
| isqrt (64-bit compare/sub pair) | ~120 | | |
| divide (64-bit shift regs, 33-bit compare/sub) + finalise/clamp | ~160 | | |
| capture/abs, walk control, fill sequencer | ~120 | 0 | |
| handshake + 4 counters + status | ~90 | | |
| **block total** | **~700 (ceiling 800)** | **0 (ceiling 0)** | **1 (ceiling 1)** |

**UNKNOWN**, with the instrument for each: fitted ALM / actual RAM
inference / Fmax — the risk cones are the 64-bit `is_res+is_bit`
compare-subtract and the 66-bit accumulator add, each single-stage
(G-SHADE1; the repair if one misses is pipelining that iteration to 2
cycles, latency +32/+64, still 3–5x over demand); `quartus_map` acceptance
beyond the form gate (a block never through quartus_map is not shown
synthesizable); **the LOOK** — see §8.

### Build/run recipe (standalone, mirrors the terrain/bump lane pattern)

```
# PowerShell: . ./tools/env/zhao-env.ps1 first (or the equivalent PATH)
verilator_bin --lint-only -Wall fpga/rtl/terrain/zhao_terrain_shade.sv        # 0
python tools/quartus/check_quartus17_syntax.py                                # clean
verilator_bin -cc --exe --build -j 4 -Wall --Mdir build/standalone/shade_obj \
  --prefix Vzhao_terrain_shade fpga/rtl/terrain/zhao_terrain_shade.sv \
  tests/terrain/terrain_shade_rtl_directed.cpp tests/harness/zhao_sim.cpp \
  -CFLAGS "-std=gnu++17 -IC:/programmieren/zencrifice/zhaozhou/reference/include -IC:/programmieren/zencrifice/zhaozhou/tests/harness" \
  -LDFLAGS "C:/programmieren/zencrifice/zhaozhou/build/reference/libzhao_zref.a"
# run with the WINLIBS runtime first on PATH (see §7 footnote):
./build/standalone/shade_obj/Vzhao_terrain_shade.exe                          # 4,142 / 0
./build/standalone/shade_obj/Vzhao_terrain_shade.exe --break-oracle           # FAILS: instrument proven
```

The ctest registrations (`terrain_shade_rtl_directed`,
`terrain_shade_break_oracle` as WILL_FAIL, `lint_terrain_shade`) run the
same binaries through the main build tree.

## 6. Implementation order, with the ONE fit gate

1. **DONE (this session):** RTL + differential + registrations + contract
   amendments.
2. **The art-law gate, still standing and NOT superseded by any number
   here:** the amended oracle goes into the ZRef renderer and the owner
   LOOKS at the island under a MOVING sun at 240p. The contract put this
   gate BEFORE RTL; building the block does not retire it — it retires
   only when the owner has looked.
3. **The consumer seam** (§7) — its own contract, then wiring, then
   `gen_prod_top.py` regeneration (the manifest moves off `unused` only
   then).
4. **FIT GATE G-SHADE1 — the ONE fit this block spends**, batched with the
   terrain-lighting subsystem (same batch as the bump organ's G-BUMP1, per
   the fit-batching rule). The question, stated in advance:
   *"Does the exact-law shade hold 0 DSP / ≤800 ALM / exactly 1 M10K
   (512x16 TDP with read-during-fill on port A), and do the 64-bit
   root/divide carry cones close ≥100 MHz?"* Everything else this block
   needed to know, Verilator answered today in seconds.

## 7. What still has to happen before terrain is actually lit on screen

This block gives TERRAIN.NORMALS its first consumer, but the light still
dead-ends one seam later — scoped here, deliberately not built (out of
lane; `fpga/rtl/geometry/` and the bake/normalmap files were touched by
other lanes today):

* **TERRAIN.PROJECT has no colour port.** `base_o` must fold into the flat
  vertex colour the raster interpolates — the fold point where ambient is
  ADDED (`SetEnvironment 0x0311` carries ambient beside the sun; the
  contract's Notes forbid a floor). That is a new seam contract: pack
  `base` (sign-clamped only THERE, per light) with the sheet tint into the
  fragment's colour, and route `degenerate_o` so a collapsed cell shades
  ambient-only.
* **The sun feed:** per-triangle `sun_*_i` currently rides the packet; the
  driver owes the per-frame Q16.16 sun word (the moving sun) — same ABI
  lane as the bump organ's `SetTerrainDetail` conversion, HPS side.
* **The composed shell contains no terrain at all** (`zhao_shell_fit.qsf`),
  so lighting the screen also waits on terrain entering the composed
  island — the subsystem-boundary fit this block's gate batches into.
* **TERRAIN.NORMALMAP's seam** then adds its per-fragment delta to `base`
  BEFORE the per-light clamp (the ruled order), which is why `base_o`
  keeps its sign.
* **Manifest note (resolved during the session):** `check_prod_manifest.py`
  mid-session reported two UNACCOUNTED forge modules — a CONCURRENT live
  lane's uncommitted files (`RUN-20260909-2216-forge-prim-eval` appeared
  while this lane ran; verified by differential stash: without my edits 3
  errors, with them 2). Deliberately NOT registered from here; that lane
  registered them itself, and the final check is **OK** across all 215
  modules.
* **Toolchain footnote for whoever reruns the standalone suite:** run the
  exe with the winlibs runtime first on PATH
  (`C:\programmieren\dsstuff\mingw64\bin`). With the oss-cad-suite `bin`
  ahead of it the exe dies at load with a bare RC=127 and NO output — a
  silence that looks exactly like a broken test and is only a DLL shadow.

## 8. Not verified, each with its instrument

* **Fitted ALM / M10K inference / Fmax / quartus_map acceptance** —
  G-SHADE1. Until then every resource number in §5's second table is
  structure, not silicon.
* **The LOOK** — the moving-sun 240p render of the island through the
  amended oracle, judged by the owner's eye. No gate substitutes; the
  contract ordered it before RTL and it has NOT happened. The bit-exact
  RTL changes nothing about that obligation.
* **The seam** (colour fold, ambient addition, clamp placement at unit8) —
  the seam's own contract and directed test, when built.
* **Composed behaviour with the real producer** — a NORMALS→SHADE chain
  test exists only in oracle form (tier 1 uses the thin view, not the
  NORMALS RTL); the chain differential belongs to the composition lane,
  instrument: a `zhao_pair_normals_shade` bench like the tess/normals pair.
* **Fmax of the M10K read-during-write port arrangement** on real silicon
  timing — G-SHADE1's report, specifically the RAM's output cone into the
  16-bit subtract.
* **The counters under composed traffic** — exact counts are proven under
  this suite's stimulus; the composed island's traffic goes through the
  same tallies when the chain bench exists.

## 9. Verified vs inherited

**Verified this session:** everything in §5's MEASURED table, by running
it; the format errors E1–E3 by reading the law's source and the constants'
parity; the width bounds by arithmetic in the RTL header; the concurrency
finding in §7 by differential stash.

**Inherited, not re-verified:** TERRAIN.NORMALS' own bit-exactness (41,731
checks, its lane); the 2,000-triangles/frame demand number
(`design/budgets/workloads.yml`, confidence as recorded there); the
1,666,666-clock compute frame; `libzhao_zref.a`'s freshness (timestamp
newer than every reference source it compiles — checked — but the archive
was built by the main cmake tree, not rebuilt here); the rescue's resource
percentages (ALM 139% / DSP 171% / M10K ~27%).
