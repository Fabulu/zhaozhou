# G1-A — the actual survivors, and the gap nobody had added up

2026-09-05. G1-A asks two questions: *close storage inference*, and *choose the
actual survivors*. The second one has to be answered first, because it changes
what the first one is worth doing to.

---

## 1. `zhao_texture_tmu_pipe` is not in the approved architecture at all

`reports/islandrearchitecture5.md` §3.3 lists the approved island as **eleven**
components. `zhao_texture_tmu_pipe` is **not one of them**. Its functions are
split across *binding tables + TMU planner v2*, *class router + decode stores*,
*serial bilinear channel engine*, *material combiner* and *AUX v2*, and its
palette becomes a separate block — **transactional resident palette**, budgeted
at 250 ALM / 200 reg / 4–8 M10K.

So the 65,536-bit palette cache sitting in flip-flops inside `tmu_pipe` (D19m)
**must not be repaired.** The roadmap's own warning applies exactly:

> Preserve the diagnostic lesson, but do not spend days polishing an obsolete
> monolith if the approved production split replaces that storage with the
> resident-palette path.

It does. `zhao_texture_palette_res` already exists and is measured.

**This makes `design/prod_manifest.yml` and the approved architecture
disagree.** The manifest selects `zhao_texture_tmu_pipe` "over
zhao_texture_tmu"; the architecture retires both. That is a G0-class conflict
found in G1, and it is recorded here rather than silently resolved, because
changing a production selection is a decision, not a tidy-up.

**The D19m diagnostic lesson survives regardless** — it is what produced
`QUARTUS_GOTCHAS.md` §14 (what actually decides M10K inference), and §14 is what
predicted `cache_pipe`'s outcome correctly before its fit returned.

---

## 2. The survivor table

Targets from `islandrearchitecture5.md` §3.3. Measurements from the census,
each checked against its own source date per D19o.

| approved component | ALM target | measured | row | module |
|---|---:|---:|---|---|
| FRAGROB + token fabric | 900 | **1,676** | current | `zhao_texture_fragrob` |
| RCP24 scheduler v2 | 650 | **1,041** | current | `zhao_raster_rcp24_svc` |
| perspective pair pipeline | 900 | 2,204 | **STALE** | `zhao_raster_perspuv_svc` |
| binding tables + TMU planner v2 | 700 | **1,142** | current | `zhao_texture_tmu_plan` |
| synchronous texture cache v2 | 900 | **1,633** | current | `zhao_texture_cache_pipe` |
| class router + decode stores | 350 | **806** | current | `zhao_texture_rsp_dispatch` |
| transactional resident palette | 250 | **540** | current | `zhao_texture_palette_res` |
| serial bilinear channel engine | 250 | **125** | current | `zhao_texture_bilerp_lane` |
| Mosaic CSD pipeline | 500 | **310** | current | `zhao_texture_mosaic` |
| **material combiner** | 650 | — | — | **NOT BUILT** |
| AUX v2 | 550 | **1,182** | current | `zhao_texture_aux_pipe` |
| **nominal architecture total** | **6,600** | | | |

**Nine current rows sum to 8,455 ALM.** That is already **13% above the 7,500
ALM hard redline**, and it excludes:

* the **perspective pair**, whose only row is stale and last read 2,204 against
  a 900 target;
* the **material combiner**, which does not exist and is budgeted at 650.

A straightforward reading puts the island near **11,000 ALM against a 6,600
nominal and a 7,500 redline** — roughly **1.7×** the approved envelope.

Two components are **under** target: `bilerp_lane` (125 vs 250) and `mosaic`
(310 vs 500, after the C21 CSD rewrite). Everything else is over, most of it
substantially: `rsp_dispatch` +130%, `palette_res` +116%, `aux_pipe` +115%,
`perspuv_svc` +145% on its stale row.

---

## 3. What this number is, and what it is not

**It is an upper bound, and every reason it might be too high is stated:**

* These are **leaf fits with virtual pins**. Virtual pins cost logic that a
  composed island does not pay, and every row here carries hundreds of them.
* **Per-block fits do not share.** Common arithmetic, shared decode and shared
  control are counted once per block here and once in the island.
* One row is **stale**; `cache_pipe` fell from 5,903 to 1,633 when re-measured,
  which is exactly how much a stale row can be worth.

**It is not too low for any reason I can find**, and that asymmetry matters. The
composed island must also carry glue that no leaf fit contains.

**It is not a fit of the island.** Nobody has fitted these eleven together. The
roadmap is explicit that the 5,500–6,600 envelope and the 7,500 redline are
*targets, not measurements already achieved*, and this file does not turn a sum
of leaves into a composed result.

---

## 4. What G1 actually is, given this

The work is **not** "add the missing combiner and fit". On these numbers, adding
the combiner to the current implementations lands at roughly 1.7× the redline.

The honest G1-A conclusion:

1. **Retire `tmu_pipe` from selection** rather than repair it — an owner-visible
   manifest change, not a silent one.
2. **Re-measure the perspective pair.** Its row is stale and it is the largest
   single overrun; the number being acted on may not exist any more.
3. **The overrun is broad, not local.** Eight of nine current components exceed
   target. That is a systematic difference between what the architecture
   budgeted and what the implementations cost — not one bad block to fix.
4. **Build the material combiner**, which is the one component whose absence
   makes three-sample material support impossible (`TEXJOIN` returns sample zero
   for every recipe today).

Item 3 is the one that needs a decision before effort is spent: either the
implementations come down substantially, or the envelope was optimistic and
needs re-stating against measured leaves. **Deciding that from a sum of stale
and virtual-pinned leaf fits would be exactly the mistake this repository keeps
recording**, which is why the next concrete step is a composed measurement, not
another round of leaf tuning.

---

## 5. G1-B — FRAGROB already satisfies the structural rule; its overrun is elsewhere

The roadmap's requirement for the tokenized fragment path is explicit:

> Wide fragment data should live in RAM-backed records rather than repeatedly
> circulating through control queues.

**FRAGROB already does this.** Computed from its declarations at `DEPTH = 16`:

| array | bits | role |
|---|---:|---|
| `desc_u_m` / `desc_v_m` | 1,536 + 1,536 | wide payload |
| `res_rgb_m` / `res_a_m` | 1,152 + 384 | wide payload |
| `ctx_m` | 1,024 | wide payload |
| `desc_met_m` | 576 | wide payload |
| `auxrgb_m` / `auxa_m` | 384 + 128 | wide payload |
| **wide payload total** | **6,720** | |
| nine small control arrays | 976 | correctly flops |

Measured `blockMemoryBits` = **6,464** across **13 M10K**. That is **96% of the
declared wide payload already in memory.** The records are RAM-backed; the queues
carry slot indices, not payload.

**So the 2,631 vs 2,500 overrun is 131 registers — 5.2% — of CONTROL state**, not
misplaced payload. The gate's message said otherwise ("state that belongs in
memories is in flip-flops"), which is the second time that canned diagnosis has
been wrong; `tools/quartus/fit_rules.ps1` now reports the measurement and points
at the RAM Summary instead of asserting a cause.

### The recommendation is NOT to re-budget it

The roadmap permits "optimized **or** explicitly re-budgeted with whole-island
evidence". The whole-island evidence now exists (§2 above) and it argues the
opposite way: **the island is roughly 1.7× its envelope, so no gate should be
loosened.**

And FRAGROB is the wrong place to spend reduction effort. Ranked by overrun
against target:

    perspuv_svc     2,204 / 900   +145%   (stale row, re-measuring now)
    rsp_dispatch      806 / 350   +130%
    palette_res       540 / 250   +116%
    aux_pipe        1,182 / 550   +115%
    cache_pipe      1,633 / 900    +81%
    fragrob         1,676 / 900    +86%   <- registers 5% over; ALMs 86% over
    tmu_plan        1,142 / 700    +63%
    rcp24_svc       1,041 / 650    +60%

**FRAGROB's register gate is the smallest miss in the island and its structure is
correct.** Its *ALM* figure is 86% over target, which is the same broad problem
every other block has — and that is an island-level conversation, not a
FRAGROB-level one.

**Left as a failing gate deliberately.** It fails honestly, it costs nothing to
leave failing, and lowering the bar while the island is 1.7× over would be
exactly the "green box" management the roadmap warns against.
