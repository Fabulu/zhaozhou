# What actually limits the clock — 22 blocks, split by where the path runs

*2026-09-07. Written after `min_fmax_mhz` fired for the first time, on
`zhao_terrain_pagestream`, the first block that carried it. Every number below
comes from `tools/quartus/split_setup_paths.py` over reports already on disk.
No fit was run to produce this.*

---

## The number that started it, and why it was not the answer

`zhao_terrain_pagestream` refitted at **93.91 MHz** against the 100 MHz product
clock, and the rule added the same morning failed it. Read straight,
`reports/synthesis/zhao_block_fit.json` says the tree is in far worse shape than
that: **43 of the 51 rows carrying an `fmaxMhz` are below 100 MHz**, several by
a factor of three.

That field has existed for as long as the rows have. Until today **nothing read
it.** So it was never right or wrong; it was simply never asked.

Asked now, it turns out to be almost unusable as written, for the reason
CLAUDE.md already records from the composed island: a leaf fit wraps its block in
**virtual pins**, and `fmaxMhz` is whatever the single worst path says —
including paths that begin or end at an imaginary pad with an imaginary clock
network attached. On PAGESTREAM those pins carry **−6.29 ns of reported clock
skew** against −0.5 ns internally.

## The split, and the two times it changed its own answer

Each summarised path is classified at **both** ends: `port` (a top-level pin of
the block under fit — virtual, in a leaf fit), `reset` (launched from `rst_n` —
a recovery path, a real constraint but a different one with a different fix), or
`core` (everything else, including anything hierarchical, which is inside a
submodule by construction).

This was worth doing three times, because the first two answers were wrong:

* **Ends only** — 7 blocks looked internally limited.
* **Both ends** — `zhao_texture_aux_pipe` jumped **63.63 → 120.37 MHz**. Its
  worst path *started* at an input pin and ended inside a submodule, so an
  end-only split kept it.
* **Reset separated** — `zhao_texture_aux_div6` went **87.45 → 103.00**,
  `raster_rcp24_v3` **90.54 → 129.18**. Their worst non-boundary path was
  launched from `rst_n`.

## The result

| block | reported | **core data path** | limited by |
|---|---:|---:|---|
| `zhao_texture_material_combine_v1` | 29.74 | **29.74** | itself |
| `zhao_terrain_residency_v2` | 61.38 | **61.38** | itself |
| `zhao_texture_island_top` | 67.57 | **77.30** | itself |
| `zhao_texture_v3own@v3-full` | 75.79 | **89.09** | itself |
| `zhao_raster_tilestore` | 96.12 | **96.12** | itself |
| `zhao_texture_palette_res` | 98.06 | 100.32 | boundary |
| `zhao_texture_aux_div6` | 85.68 | 103.00 | reset (87.45) |
| `zhao_texture_fragrob` | 103.10 | 103.10 | — |
| `zhao_raster_perspuv_svc` | 96.62 | 103.22 | boundary |
| `zhao_terrain_pagestream` | 93.91 | 107.38 | boundary |
| `zhao_texture_cache_pipe` | 101.69 | 107.50 | — |
| `zhao_texture_tmu_plan` | 88.54 | 110.57 | boundary |
| `zhao_terrain_loadq` | 107.33 | 110.98 | — |
| `zhao_texture_mosaic` | 79.22 | 113.57 | boundary |
| `zhao_terrain_cmd` | 90.87 | 115.09 | boundary |
| `zhao_texture_aux_pipe` | 63.63 | 120.37 | boundary |
| `zhao_texture_bilerp_lane` | 99.69 | 125.88 | boundary |
| `zhao_texture_rsp_dispatch` | 110.90 | 126.06 | — |
| `zhao_raster_rcp24_v3@v3-full` | 90.54 | 129.18 | reset (113.93) |
| `zhao_geom_mem_adapter` | 122.10 | 141.32 | — |
| `zhao_terrain_mipfeed` | 121.08 | 169.00 | — |
| `zhao_texture_combine` | 100.12 | — | every summarised path touched a boundary |

**Twenty of twenty-two miss the clock as reported. Five miss it with no
boundary to blame.** The panic was mostly artefact; the residue is not.

## The five, and what each one is

### 1. `zhao_texture_material_combine_v1` — 29.74 MHz. Three and a half times short.

Worst path `Decoder5~9 → Add46~41`, **33.46 ns of data delay** against a 10 ns
period, core to core, nothing at either end to excuse it. This is the worst
timing block in the tree by a wide margin and it is not close.

**It is also the block the roadmap already wanted a DSP measurement for, and
that is not a coincidence — it is the same defect seen twice.** CLAUDE.md
records the fit reporting 8 DSP against a rule of 2, the comfortable diagnosis
being that Quartus ignored `multstyle = "logic"`, and the truth being that the
block contains about fourteen multipliers because `unit_mul_logic(...)` sits
inside every arm of two seven-arm case statements. A decoder feeding a
forty-sixth adder is exactly what that structure looks like from the timing
side. **The DSP count and the clock are one finding, and one fix answers both.**

### 2. `zhao_terrain_residency_v2` — 61.38 MHz.

Worst path `s0_set[6] → altsyncram:g_bank[1].keyra` — a set index arriving at an
M10K address port. A refit of this block is running as this is written, for an
unrelated reason (its `min_memory_bits` FAIL sits on a row two commits stale),
so a fresh number is imminent. **The stale row's Fmax is equally stale and this
entry will need re-reading, not just its memory line.**

### 3. `zhao_texture_island_top` — 77.30 MHz core-to-core.

The composed G1-D island, and the one row here that is not a leaf fit. Its worst
path is `perspuv_svc → fragrob`: **a genuine inter-block path with real wiring**,
which is precisely what the composed fit was built to expose. Reported 67.57 and
limited internally at 77.30 — still **23% short of the product clock**.

`reports/G1D-COMPOSED-ISLAND-20260905.md` §4.3 records this island's headline as
an ALM number against 6,600 nominal / 7,500 redline. **§4.3 has no clock line at
all**, and that row is now four commits stale besides.

### 4. `zhao_texture_v3own@v3-full` — 89.09 MHz. 5. `zhao_raster_tilestore` — 96.12 MHz.

Marginal: 11% and 4% short. Real, but a different order of problem from the
first three.

## What this changes

**`min_fmax_mhz` as I wrote it this morning is measuring the wrong thing on a
leaf fit, and PAGESTREAM is the proof.** It failed a block whose core data path
runs at 107.38 MHz. The rule is not wrong to exist — five blocks here genuinely
miss the clock and nothing was saying so — but a single worst-path number cannot
tell "this block is too slow" from "this block's pretend pads are". Keeping it
opt-in was right for a reason better than the one I gave.

The honest gate is the **core-to-core** number, and `split_setup_paths.py` now
computes it. Wiring that into the rule is the obvious next step and is
deliberately not taken here: it would change how every future fit is judged, and
that decision should be made on this evidence rather than in the same pass that
produced it.

**Nothing here proves the assembled console makes 100 MHz.** A leaf fit's
core-to-core number excludes the boundary because the boundary is fake — but in
the real design that boundary is replaced by a *real* one, with real routing
between blocks that a leaf fit never sees. The island row is the only line in
the table that measures any of that, and it is the third-worst entry.

## Provenance

`tools/quartus/split_setup_paths.py`, committed rather than thrown away — the
last probe that answered a question like this was written once, used, discarded,
and its numbers were unreproducible. Its classifier asserts at import against
eight real node names, and the assertion was **shown to fire** by inverting the
hierarchical-name rule on purpose and watching it stop the run.
