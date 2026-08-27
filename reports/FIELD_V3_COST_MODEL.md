# FIELD v3 — the regenerated cost model

*Phase 1 deliverable of `reports/Fieldv3.md`. Supersedes the admission
arithmetic of `reports/FIELD_V2_MODEL.md` (whose Scenario C assumed free
transport and a single binding resource). Every number below reproduces from
the COMMITTED script:*

    node tools/report/field_v3_cost_model.mjs

*(repo root, `compiler/dist` built). The script hash-pins the three committed
Earth programs against their goldens and refuses to model a stale build.*

Evidence discipline: every number is labelled **measured**, **derived**,
**target**, or **arithmetic**.

---

## 1. Inputs

| input | value | provenance |
|---|---|---|
| v2 leaf Fmax | **59.22 MHz** (restricted) | measured — `reports/synthesis/zhao_block_fit.json` row `zhao_field_v2_front`, sourceCommit `a03d336`, rtlCleanAtHead true |
| v2 leaf timing at 100 MHz | setup **−6.886 ns** (TNS −5,134 ns), hold **−1.938 ns** | measured — `reports/synthesis/blockpaths/zhao_field_v2_front.sta.rpt` |
| v2 leaf resources | 7,870 ALM / 10,482 reg / 33 M10K / 15 DSP | measured — same fit row |
| point transport | 12×1,089 in + 13×1,089 out = **27,225 clocks/assoc** | measured from `zhao_field_v2_front.sv` (fill/run/drain; 12 lanes @1 clk, 4 out lanes @3 read phases +1 handshake) |
| committed programs | impact_wave `0x82f5f4e4` (30), wave_pool `0x8bdceb63` (27), crater_ring `0x484add8d` (28) | measured — built by the sanctioned chain, hash-pinned |
| v2 long-unit IIs | RING 48, LEN/DIST2 42, CURVE 23, DCURVE 20, SPLINE 39, RCP 9, NORMALIZE3 59 (measured) | derived — `reports/FIELD_RESOURCE_MODEL.md`, ±2 |
| v3 service IIs | CURVE ≤14, DIST2 ≤20, vector MUL II 1, prepared RING 9 slots/group | **target** — Phase 3 probe gates, not measurements |
| active vertices | 1,089/full patch → **273** four-wide groups | arithmetic — 33×33 lattice |
| frame | 128 associations, 60 Hz, 20 % reserve | ratified stress frame |

## 2. What v2 actually costs (the honest model)

Per association: transport + worst serialized long unit (four SIMD lanes
through one ready-only-when-idle scalar unit). At the **measured** 59.22 MHz
the usable frame is 789,600 clocks:

| program | transport | worst unit | clocks/assoc | 128-assoc frame | % of usable @59.22 |
|---|---:|---|---:|---:|---:|
| impact_wave | 27,225 | CURVE 96,921 | 124,146 | 15,890,688 | **2,012 %** |
| wave_pool | 27,225 | LEN/DIST2 45,738 | 72,963 | 9,339,264 | **1,183 %** |
| crater_ring | 27,225 | RING 52,272 | 79,497 | 10,175,616 | **1,289 %** |

The earlier Scenario A percentages (930/439/502 %) were computed against a
hypothetical 100 MHz **and without transport**; the honest numbers above are
what the committed RTL does at the clock it actually fits. Patch reduction on
the vertex-major seam adds 1+n clocks/vertex at TERRAIN.PATCH (2,178/assoc at
n=1), and table loads add table_bytes/4 words per program swap — both dwarfed
by the columns above but both now in the model.

## 3. The v3 demand vectors (per `spec/form/cost-model.md` §5)

Uniform/varying split by forward taint from lanes x,z (kill on overwrite) —
matches `FIELD_V2_MODEL.md`'s measured split exactly:

| program | varying | uniform | demand vector (per full association) |
|---|---:|---:|---|
| impact_wave | 16 | 13 | issue 16/grp, vmul 11, curve_req 1, dist_req 1, cold 0, uniform 13, tables 288 B, vreg 5, sreg 3 |
| wave_pool | 17 | 9 | issue 17/grp, vmul 11, curve_req 0, dist_req 1, cold 0, uniform 9, tables 0 B, vreg 6, sreg 4 |
| crater_ring | 13 | 14 | issue 13/grp, vmul 18 (RING prepared = 9), curve_req 0, dist_req 1, cold 0, uniform 14, tables 96 B, vreg 5, sreg 6 |

Notes the vectors make visible:

- **The RCP is uniform in all three programs** (smoothstep's edge reciprocal)
  and vanishes into preparation; no varying RCP remains → cold lane 0.
- wave_pool's SIN/COS are **uniform** in the committed program (they read
  `phase`); its varying loop is pure MUL/ALU + one DIST2.
- crater_ring's RING becomes a **prepared ring**: radii uniform, midpoint and
  both reciprocals prepared once, nine separately-rounded vector products
  remain — 9 multiplier slots/group, rounding bit-identical.
- vreg high-water 5–6 after uniform elimination: the reduced 32-register hot
  file has ~5× headroom over the committed corpus.

## 4. Per-association cost under the v3 targets

Service occupancy per association (273 groups × per-group demand × II):

| program | issue | vmul | curve | dist | binds on | clocks/assoc |
|---|---:|---:|---:|---:|---|---:|
| impact_wave | 4,368 | 3,003 | 3,822 | 5,460 | **DIST service** | **5,460** |
| wave_pool | 4,641 | 3,003 | 0 | 5,460 | **DIST service** | **5,460** |
| crater_ring | 3,549 | 4,914 | 0 | 5,460 | **DIST service** | **5,460** |

All three bind on the two-bank distance service at its **target** II ≤ 20 —
which is why the brief's hard per-association target is **≤ 6,000 clocks**
(5,460 + scheduling/masks/plan margin), and why the DIST2 probe is decisive:
if its fitted II lands above 20, the binding number moves 273 clocks per II
step and the envelope must be re-run, not argued with.

## 5. Frame roll-up and the clock decision rule

128 × 6,000 + patch init/finalise (128 × 2 × 273 = 69,888) = **837,888**;
acceptance ceiling **≤ 850,000** including plan/table/setup margin:

| Field clock | usable clocks/frame (20 % reserve) | 850 k usage |
|---|---:|---:|
| 100 MHz | 1,333,333 | **64 %** |
| 90 MHz | 1,200,000 | 71 % |
| 80 MHz | 1,066,667 | 80 % |
| 59.22 MHz (current v2) | 789,600 | **108 % — fails** |

The rule stands as the brief states it: design for the shared 100 MHz GPU
domain; accept a lower private clock only when the COMPLETE measured Earth
slice finishes the frozen worst frame inside 80 % of its actual cycles.
~80 MHz is the lowest credible clock; 59 MHz is not.

## 6. What would falsify this model

1. **A fitted DIST2 probe II > 20** — the binding column moves; re-run the
   script with the measured II before believing any closure.
2. **A fitted CURVE probe II > 14** — impact_wave's curve occupancy passes
   DIST at II 21 and becomes the binder.
3. **Vector issue not reaching 1 group-instruction/clock** in the fitted
   FIFO-scheduler probe — every issue column scales up together.
4. **Cold-lane demand appearing in real programs** (varying NORMALIZE/NOISE/
   ROT/SPLINE) — the 40-clock/point pessimistic cold model makes any such
   program bind on the cold lane immediately; that is the admission test
   doing its job, not a model failure.
5. **The planner's demand vectors disagreeing with runtime counters** —
   prediction undercounting real demand is a mutation-sweep target
   (`reports/Fieldv3.md`, target 13).
