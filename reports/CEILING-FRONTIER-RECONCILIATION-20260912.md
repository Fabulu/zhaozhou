# Ceiling frontier reconciliation — the 99-DSP path is not a two-view shipping point

2026-09-12. This packet corrects the resource frontier after checking the
committed two-view terrain workload against the projector's initiation interval.
It does **not** claim a current production measurement. The selected census is
partial, mixed-age evidence, and most proposed savings below remain structural
until their named subsystem fit.

## Verdict

* **99 DSP is struck as a legal two-view shipping frontier.** It depends on
  `ROWS_PER_PASS=1`, whose projector initiation interval is three clocks. Dense
  two-view terrain alone then needs 1,990,656 clocks in a 1,666,666-clock frame,
  before one geometry vertex is accepted.
* **111 DSP is the current plausible legal structural frontier.** It keeps the
  shared projector at `ROWS_PER_PASS=3`, applies `MATW=18`, the two-lane cull,
  and the conservative end of terrain-bake v2's predicted range.
* **111 is conditional, not measured.** The shared projection subsystem is not
  adopted and has not completed its composed fit; terrain-bake v2 has not been
  fitted; the cull's six-DSP default is structurally derived rather than fitted.
* The gap is therefore **23 DSP against the roadmap allocation of 88**, **17
  against the owner's looser cap of 94**, and only one below the physical 112-DSP
  device ceiling. This is neither closure nor comfortable headroom.

## 1. The evidence base, without pretending it is a production total

The committed census selected by `tools/budget/dsp_census.py` and rebucketed by
`tools/budget/domain_scoreboard.py` currently reads:

| resource | selected fitted/mixed evidence | roadmap allocation | device |
|---|---:|---:|---:|
| ALM | 58,359 | 36,000 | 41,910 |
| DSP | 192 | 88 | 112 |
| M10K | 147 | 464 | 553 |

The scoreboard reconciles exactly to `dsp_census.totals` on the same bill. That
only proves the rebucketing did not drop rows. It does **not** make the bill
current: 34 production roots have no DSP price, 45 have no fitted ALM, and the
selected rows include stale historical implementations. Unpriced ALM is unknown,
never zero. The 58,359 figure is neither a floor nor a ceiling.

The roadmap allocation is **88 DSP**, not 94. The six-DSP difference is reserved
headroom under the owner's cap, not money already available to a domain.

## 2. Why `ROWS_PER_PASS=1` is illegal for the retained two-view workload

`reports/PROJECTION-ADOPTION-20260910.md` measured the terrain identity set and
states the dense-fill stress explicitly:

```
2 views × 256 patches × 16 subpatches × 81 fills = 663,552 fills/frame
```

At `ROWS_PER_PASS=1`, the projector accepts one vertex every three clocks. The
same report verifies bit identity at that setting, but correctness is not
capacity:

```
663,552 fills × 3 clocks/fill = 1,990,656 clocks
1,990,656 / 1,666,666         = 119.4% of the raw frame
```

That is terrain fill alone, before geometry, arbitration loss, or reserve. The
configuration cannot ship the two-view dense-subpatch workload. A future design
may make RPP=1 legal only by changing a premise — for example reducing the fill
count with sparse/per-level sealing, changing the ruled two-view workload, or
adding another projector lane. None is part of the present frontier.

The previous 99-DSP arithmetic was:

```
140 corrected post-R3/R4/material/shared-projector baseline
-18 RPP=3 -> RPP=1 at MATW=32
 -3 MATW=18 marginal after RPP=1
 -9 cull two lanes
-11 terrain bake, conservative
---
 99
```

The arithmetic adds correctly; the design point does not meet the workload.
Therefore 99 is a useful non-shipping experiment at most, not a ceiling plan.

## 3. The legal structural frontier

The retained path keeps `ROWS_PER_PASS=3`, where the shared projector's structural
lattice is 33 DSP at `MATW=32` and 24 at `MATW=18`. `MATW=18` is therefore worth
**nine**, not the three-DSP marginal quoted after the now-illegal RPP=1 point.

| step | DSP | delta | evidence class and dependency |
|---|---:|---:|---|
| selected mixed census | 192 | — | partial/stale selected evidence, not a current production measurement |
| one shared projector, RPP=3/MATW=32 | 159 | -33 | structural; composed RTL exists, adoption and fit pending |
| `rcp24_v3`, `NCTX=12` | 156 | -3 | owner-ruled and implemented; underlying DSP delta measured, composed bill stale |
| retire `material_combine_v1` charge | 154 | -2 | census/composition correction, not a new silicon saving |
| sequenced pose decode, 18 -> 4 | **140** | -14 | implemented; throughput verified, DSP value structural until fit |
| shared projector, RPP=3/MATW=18, 33 -> 24 | 131 | -9 | implemented parameter/refusal law; composed adoption and fit pending |
| geometry cull default, 15 -> 6 | 122 | -9 | implemented; exact behavior/rate verified, DSP structural until fit |
| terrain bake v2, 17 -> 6 conservative | **111** | -11 | implemented; exact behavior verified, 34x34 DSP and RAM inference unfitted |

The endpoint uses the conservative six-DSP terrain-bake estimate. A three-DSP
mapping would improve the point to 108, but that is not banked before the named
fit. Conversely, any failed inference raises the point.

### What is measured versus merely present in RTL

* `MATW=18` has legal-content differential evidence, deterministic refusal, and a
  committed mutant that fires the checker. Its RPP=3 DSP count is structural:
  9 row sites × 2 DSP + 2 viewport sites × 3 DSP = 24.
* `zhao_geom_cull` defaults to two 33x33 lanes and passes the same behavior across
  one-, two-, and four-lane forms. Six DSP follows from two measured three-DSP
  lane shapes, but the default itself is unfitted.
* `zhao_terrain_bake_v2` reduces seven mutually exclusive multiplier sites to one
  muxed 34x34 site and moves the meets plane toward one M10K. Verilator settles
  behavior; only Quartus settles whether that site costs three through six DSP
  and whether the RAM infers.
* The shared projector is composed provisionally in `zhao_proj_subsystem`, but it
  is not yet the production composition. Its saving cannot be called installed
  until the terrain-owned composition passes and the manifest flips.

## 4. What remains to crack the DSP ceiling

Even if every 111-DSP dependency lands exactly as predicted, the design still
needs at least:

* **1 DSP** to fit the bare device;
* **17 DSP** to meet the owner's cap of 94;
* **23 DSP** to earn the roadmap's 88-DSP allocation and preserve its six-DSP
  headroom.

No unmeasured candidate should be chosen by subtracting hoped-for deltas from
111. The next DSP architecture must instead name its workload, legal initiation
rate, multiplier sites and composed fit boundary before its saving enters this
table.

## 5. The ALM ceiling remains less known and more severe

The domain scoreboard's fitted-row-only ALM sums are:

| domain | fitted ALM | allocation |
|---|---:|---:|
| Shell/raster/video/memory | 16,778 | 8,000 |
| Texture | 12,500 | 7,000 |
| Projection/result arenas | 12,267 | 4,500 |
| Geometry/lighting | 6,922 | 5,200 |
| Terrain/forge/maintenance | 6,888 | 3,500 |
| Complete FIELD | 0 | 4,500 |
| 2D/particles/surfaces/post | 993 | 1,800 |
| Integration/support | 2,011 | 1,500 |

These rows include obsolete duplicated structures while omitting 45 fitted ALM
prices, so their total cannot support either a subtraction forecast or a closure
claim. In particular, the shell bucket is contaminated by a virtual-pin-heavy
fit harness. D3, the truthful shell fit-top split, is therefore measurement
architecture: it separates wrapper cost and pruning behavior from shell logic so
the next real shell/raster ALM reduction can be selected. D3 is not itself an
8,778-ALM saving and must never be entered as one.

## 6. Named gates, and what they are allowed to prove

The Quartus 17.0.2 Build 602 installation is now available, but availability does
not justify speculative fits.

1. **Projection/terrain composition gate, owned by the terrain lane:** does the
   completed two-view downstream composition meet rate and map the retained
   RPP=3/MATW=18 shared projector to the expected 24 DSP, with the arena memory
   shape intact? Manifest adoption follows only if this gate passes.
2. **Terrain subsystem gate T1:** does bake v2 use at most six DSP, infer the
   meets plane as one M10K, and reduce ALM/registers relative to the matched
   baseline? This is the existing gate from
   `reports/TERRAIN-REARCHITECTURE-20260909.md`, not a new leaf fit.
3. **D3 shell-characterization gate:** to be fixed by the reviewed
   `SHELL-FIT-TOP-SPLIT-ARCHITECTURE-20260912.md`. It must answer shell-versus-
   harness ALM attribution at one subsystem boundary, not be multiplied into a
   four-fit campaign by default.

The cull's leaf DSP question should ride a geometry/projection subsystem boundary
unless a later architecture packet demonstrates that a separate fit changes a
decision.

## 7. Supersession and not-verified ledger

This report supersedes only frontier claims that treat **99 DSP as legal for the
retained two-view dense workload**. It does not rewrite historical reports or
erase their useful differential evidence.

Not verified here:

* no Quartus command was run;
* 111 DSP is not a composed receipt;
* shared-projector adoption is pending the terrain lane;
* the cull's six DSP and bake's six DSP are structural predictions;
* no current whole-machine ALM total exists;
* no workload trace establishes a cheaper average terrain fill count;
* no new 23-DSP reduction to the 88-DSP allocation has yet been architected.

The next ceiling ledger must start from **111 conditional**, not 99 shipping, and
must keep every subsequent number labelled until a clean committed composed
receipt exists.
