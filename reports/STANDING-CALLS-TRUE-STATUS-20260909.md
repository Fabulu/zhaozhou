# The "four standing calls" are not four, and two of them are not what they look like

2026-09-09. Every summary today has carried this line:

> the four standing calls (delete texture_combine RTL, retire
> material_combine_v1, geom_skin MUL_LANES=1, the ~53 degree FOV floor)

I checked all four. **One is void, two are bookkeeping rather than silicon, and
one is the real decision.** Listing them together as four comparable "calls"
makes the bookkeeping look like savings and buries the one that matters.

---

## 1. `geom_skin MUL_LANES=1` — **VOID. Not a call. Remove it from the list.**

Struck earlier today, in full at
`reports/DSP-PATH-TO-94-20260909.md` addendum 3.

`GEOM.SKIN.md:447-462` records the frontier, and the disqualifying column sits
two columns right of the DSP one:

| `MUL_LANES` | DSPs | vertices/frame | vs the ruled 120,000 |
| ---: | ---: | ---: | --- |
| 1 | 3 | 38,965 | **fails, 32%** |
| **3** | **9** | **124,514** | passes, 104% |

> "`MUL_LANES = 1` is kept because it fails, and it fails by a mile."
> and at `:218` — "kept, built and differentiated **because it fails** ...
> **section 8 of the directed test asserts that this configuration is below the
> demand**."

There is nothing to decide. A committed test asserts the failure. And the
contract adds that these frontier rows "**must be excluded from any DSP total**
-- they are the same block", so it was never −6 either.

## 2. `delete texture_combine RTL` — **HOUSEKEEPING. Zero resource impact.**

Verified: `zhao_texture_combine` is instantiated by **nothing**. Its only
remaining mention anywhere in `fpga/rtl/` is a comment inside
`zhao_texture_material_combine_v1.sv:7` describing what it used to do. The
manifest already carries it as `excluded: superseded  refuted II=1 form`.

So it is **already out of the count.** Deleting the file removes dead source and
one stale reference; it frees no ALM, no DSP, and no M10K, because nothing was
ever attributing any to it. `tools/budget/uncashed_cheques.py` correctly reports
it as CLOSED.

This belongs on a tidy-up list, not beside a DSP lever. Ranking it with the FOV
question implies a comparability that does not exist.

## 3. `retire material_combine_v1` — **AN OVERCOUNT CORRECTION, not a saving.**

This is the subtle one and the summaries have had it backwards.

Verified today:

* **Both islands use v2.** `zhao_texture_island_top` and
  `zhao_texture_island_v3_top` each instantiate
  `zhao_texture_material_combine_v2` exactly once and `v1` **zero** times.
* **`v1` is instantiated in exactly one place:** `zhao_prod_top.sv:4518` — the
  **generated resource/PINMISSING harness**, driven from an LFSR.
* Its measured row is real: 1,663 ALM / 2 DSP / 69.75 MHz, clean tree.

So `v1`'s 2 DSP and 1,663 ALM are **charged to a block the console will not
contain.** Retiring it does not free silicon — **the console never had it.** It
corrects an overcount: the counted bill goes 192 → 190 DSP and 58,359 → 56,696
ALM because the count was wrong, not because hardware left.

That still matters — an honest denominator is the point of the whole census — but
it must not be booked as progress toward ≤ 94 in the same column as a lever that
actually removes multipliers.

**And this is the fourth instance today of the same error shape:** a real,
clean, correctly-measured row belonging to hardware that is not in the machine.
The others were `geom_skin MUL_LANES=1`, `FILT_LANES=1` on `zhao_texture_tmu`
(whose only instantiator is a bench probe), and `zhao_texture_combine`. The
measurements are all genuine. **What is wrong every time is the inference from a
measurement to an available saving.**

## 4. `the ~53° FOV floor` — **THE REAL DECISION, and the only one of the four.**

Full ruling request: `reports/OWNER-QUESTION-FOV-FLOOR-20260909.md`.

It is worth **−18 DSP taken alone** (−10 marginally, after the shared projector
and the row multiplex, because all three levers act on the same eleven products
and **multiply rather than add**). It needs no measurement, no fit and no
engineering — only an answer. And it **had never been asked**: `grep` finds
"53 degree" in no other document in the repository.

I also corrected its arithmetic today. The source report names the constraint as
`cot(fov/2)/aspect`, which at 4:3 passes 2.0 at **41.11°** and therefore
*understates* it. The binding coefficient is `m11 = cot(fov/2)`, giving **53.13°**
— and because aspect only ever reduces `m00`, **the floor is video-mode
independent**, so one ruling covers all three modes.

---

## The corrected list

| # | item | what it actually is | worth |
|---|---|---|---|
| 1 | **53° FOV floor** | **a decision only you can make** | **−18 DSP alone, −10 marginal** |
| 2 | retire `material_combine_v1` | overcount correction | −2 DSP / −1,663 ALM **off the count**, not off the silicon |
| 3 | delete `texture_combine` RTL | housekeeping | nothing |
| — | ~~`geom_skin MUL_LANES=1`~~ | **void** — disqualified configuration | nothing |

**Only item 1 needs you.** Items 2 and 3 I can execute on a word, and neither
changes what the console contains.

## Why this document exists

The four-item list has been repeated all day, in every status summary, with the
void item and the housekeeping item sitting beside the one real decision as
though a reader should weigh them together. That is the same failure as every
other stale artefact found today: **a list that was true when written, repeated
long enough to become the thing people reason from.** The fix is the same one —
write down what changed, where the list lives.
