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
| `zhao_terrain_residency_v2` | 61.38 | **61.38** | itself |
| `zhao_texture_island_top` | 67.57 | **77.30** | itself |
| `zhao_texture_v3own@v3-full` | 75.79 | **89.09** | itself |
| `zhao_raster_tilestore` | 96.12 | **96.12** | itself |
| `zhao_texture_palette_res` | 98.06 | 100.32 | boundary |
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

**Twenty of twenty-two miss the clock as reported. Four miss it with no
boundary to blame.** The panic was mostly artefact; the residue is not.

### A correction, and the tool made the mistake it was written to prevent

The first version of this report opened with
`zhao_texture_material_combine_v1` at **29.74 MHz**, called it the worst-timed
block in the tree by a wide margin, and said so to the owner.

**That number came from a file called
`zhao_texture_material_combine_v1-2974-superseded.setup.summary.rpt`** — an
explicitly retired artefact from an older version of the block. The tool globbed
every summary in the folder and trusted the filename. That is exactly CLAUDE.md's
*"never compare a current file to an old measurement"*, committed by the tool
written to stop it being done by hand — and it landed in the alarming direction,
which is the only reason it got a second look at all.

The pairing is now **checked**: every summary's own worst slack implies an Fmax,
the row states one, and a disagreement means the summary is not that row's. Two
files fail it and are no longer split. One comparison catches a superseded run, a
renamed variant and a half-written report alike, which is why it replaced any
idea of blacklisting the word "superseded".

## The four, and what each one is

### The roadmap's COMBINE.V1 item, which turns out to be already answered

The roadmap has wanted *"COMBINE.V1's DSP measurement"* for some time. **It is
done, and it passes.** The live row reads **2 DSP against a rule of 2.**

CLAUDE.md records the failure it came from: the fit reported 8 DSP, the
comfortable diagnosis was that Quartus ignored `multstyle = "logic"`, and the
truth was about fourteen multipliers, because `unit_mul_logic(...)` sat inside
every arm of two seven-arm case statements. That has been repaired — the block
now hoists one product per lane above the case, and lines 492–499 of the file
carry the whole story including the line it violated while quoting it.

**What is NOT answered is the clock and the area.** The live row is 1,475 ALM
against `max_alms: 800` and **36.28 MHz**, and there is **no path summary on
disk for it**, so it cannot be split and the 33.46 ns `Decoder5 → Add46` path
belongs to the superseded version, not this one. A refit with path capture is
what is needed, and it is now queued behind the others.

### 1. `zhao_terrain_residency_v2` — 61.38 MHz.

Worst path `s0_set[6] → altsyncram:g_bank[1].keyra` — a set index arriving at an
M10K address port. A refit of this block is running as this is written, for an
unrelated reason (its `min_memory_bits` FAIL sits on a row two commits stale),
so a fresh number is imminent. **The stale row's Fmax is equally stale and this
entry will need re-reading, not just its memory line.**

### 2. `zhao_texture_island_top` — 77.30 MHz core-to-core.

The composed G1-D island, and the one row here that is not a leaf fit. Its worst
path is `perspuv_svc → fragrob`: **a genuine inter-block path with real wiring**,
which is precisely what the composed fit was built to expose. Reported 67.57 and
limited internally at 77.30 — still **23% short of the product clock**.

`reports/G1D-COMPOSED-ISLAND-20260905.md` §4.3 records this island's headline as
an ALM number against 6,600 nominal / 7,500 redline. **§4.3 has no clock line at
all**, and that row is now four commits stale besides.

### 3. `zhao_texture_v3own@v3-full` — 89.09 MHz. 4. `zhao_raster_tilestore` — 96.12 MHz.

Marginal: 11% and 4% short. Real, but a different order of problem from the
first two.

## Fifteen blocks cannot be split at all

They have a `.sta.rpt` and no path summary, so nothing above applies to them and
saying nothing would read as "all fine". Several are the low rows that started
this: `zhao_pair_tess_normals` **31.10**, `zhao_field_seq` 58.99,
`zhao_field_v2_front` 59.22, `zhao_raster_rcp24_svcseed3` 63.93,
`zhao_raster_rcp24_svc` 68.46, and `zhao_texture_material_combine_v1` 36.28.

**`zhao_pair_tess_normals` at 31.10 MHz is now the worst recorded number in the
tree**, and it is unexamined. It is also a terrain-path block, which puts it
directly in front of the standing goal.

## The four pair wrappers, which nothing was gating

`fpga/rtl/synth/` has held four "registered characterisation wrappers" since
2026-08-23 — registered stimulus, DUT, registered hash sink, built because the
budget audit said *"raw leaf blocks with hundreds of virtual pins are poor
physical models"*. That is the conclusion this report reached independently
today, three weeks late.

**None of the four had a fit target, so no number they produced was ever
judged.** They now carry one rule, the product clock, because a meaningful
clock number is the single thing they exist to produce. All four fail:

| wrapper | Fmax | pair ALM / DSP |
|---|---:|---:|
| `zhao_pair_tess_normals` | **31.10** | 1,523 / 9 |
| `zhao_pair_tmu_cache` | **37.25** | 2,383 / 6 |
| `zhao_pair_fragment_tilestore` | **55.52** | 1,188 / 7 |
| `zhao_pair_setup_binner` | **88.79** | 1,405 / 10 |

All four pair rows are **fresh** — no commit has touched their sources since
they were measured — so these clock numbers stand, and so does the gate.

### WITHDRAWN: the leaf-sum argument, which was built on stale rows

An earlier version of this section carried a fourth column, the sum of each
pair's leaf rows, and argued from it that the wrappers must be folding real
logic away: *"these gaps are 12–38% of ALM and 30–62% of DSP, and a virtual pin
has never consumed a DSP block."*

**Every one of the four sums contained a stale row, and two also contained
leaves with no row at all, silently counted as zero.** The comparison is void
and the conclusion is withdrawn.

The case it led with is the clearest. `zhao_terrain_normals`'s row records **18
DSP** at commit `96c0394a`. Two commits later comes `bfc74710`, *"TERRAIN.NORMALS:
one shared multiplier instead of six"*. Six 33×33 multipliers became one. The
pair's 9 DSP is TESS's 6 plus that single shared multiplier's 3 — **an exact
match, with no folding required at all.** The 24 → 9 "reduction" was a stale row
wearing the shape of a finding.

This is the third time in one day that CLAUDE.md's *"never compare a current
file to an old measurement"* has bitten, and the sharpest, because **the
staleness check that would have caught it was added to
`check_fit_rules.ps1` the same morning and simply was not run on these rows.**
A law you have just implemented is not a law you have applied.

So `tools/quartus/compare_rows.py` now exists and it **refuses**. A sum
containing a stale or missing row is not a number with a caveat; it is not a
number, so the tool prints which rows are bad and exits 2 rather than offering
a total with a footnote. It was shown to fire in both directions: refused on
the exact comparison published above, allowed on `pair_tess_normals` against
`terrain_tess` alone, where both rows are fresh.

**What is now unknown rather than established:** whether these wrappers
under-build the design at all. They do tie inputs constant — `cs_substance_i`
is wired to `2'd0`, job fields are sliced from one 32-bit stimulus word — so
folding is *plausible*. It is no longer *measured*. Settling it needs fresh
leaf fits for the six stale blocks, which is a queue of six, not an argument.

### THE REFIT DID NOT MOVE IT. The multiply was not the limit.

**31.10 -> 32.42 MHz. A 4.2% move**, for +56 ALM and +185 registers. The
prediction was wrong, and the comment written into the RTL said exactly what a
non-move would mean.

The pair now has a path summary it never had. Splitting its 1,803 paths by
which block each END sits in:

| paths | count | worst slack | implied |
|---|---:|---:|---:|
| wrapper lattice memory -> **TESS** | 129 | -20.848 | **32.42 MHz** |
| **TESS -> TESS** | 625 | -14.931 | **40.11 MHz** |
| NORMALS -> NORMALS | 693 | -3.752 | 72.72 MHz |
| TESS -> NORMALS *(the seam)* | 66 | +2.780 | 138.50 MHz |
| NORMALS -> TESS | 1 | +5.165 | 206.83 MHz |

**TESS is the limiter, twice over**, and NORMALS was never the binding
constraint. The worst path runs from the lattice memory into
`u_tess|vy[0][16]` through `u_tess|Add65~*` carry chains: the lattice read
turned into a vertex Y in one cycle, `vy[pend_slot] <= m_y` with
`m_y = fx_add_sat(...)`, on the same edge as the read that feeds it.

**The seam is fine** -- 138 and 206 MHz both ways -- which is worth stating
because it is the opposite of what PAGESTREAM -> PATCH looked like.

Two things this does not say. Not that the NORMALS change was worthless: a
33x33 multiply feeding a 67-bit adder in one cycle is a real hazard, it is
gone, and it bought 1.3 MHz because something twice as bad sat in front of it.
And not that NORMALS is now fine -- **72.72 MHz is still 27% short**, so even a
perfect TESS leaves this pair failing. Whether 72.72 is an *improvement* is
**unmeasured**: the pre-change fit had no path summary at all.

**31.10 MHz on the terrain geometry path is the most consequential number in
this report**, and it is the one that had no gate. That number does not depend
on anything withdrawn above: `zhao_pair_tess_normals`'s own row is fresh, and
the wrapper is the registered-stimulus/hash-sink shape built precisely so that
a clock number from it means something.

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
