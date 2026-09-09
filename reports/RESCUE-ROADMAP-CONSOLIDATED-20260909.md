# The rescue, consolidated: what landed, what is open, and who has to decide

2026-09-09. One document instead of fourteen. Every number here is either
reproduced by me today or explicitly labelled as somebody's claim. Where a
figure is a structural prediction rather than a measurement, it says so — the
campaign has already been damaged twice by a number that was quoted without its
qualifier, in both directions.

## The device, which is the whole argument

| resource | device | counted | verdict |
|---|---:|---:|---|
| ALM | 41,910 | 58,359 | **139% — the binding problem** |
| DSP | 112 | 192 | **171%, owner target ≤ 94** |
| registers | (packed in ALM) | 81,925 | the third problem |
| M10K | 553 | ~147 | **27% — ~400 blocks idle** |

The thesis is not "the console needs 80k ALM". It is that a collection of
independent engines is the wrong architecture for a device this asymmetric.
Spend memory; buy back ALM and DSP.

**Caveat that belongs at the top: 34 of 66 roots are UNPRICED**, and ten more
are map-only — DSP known, ALM and M10K unknown, never zero. `zhao_texture_tmu_pipe`,
the shipping TMU, has **no measured ALM of any kind** (its fit row is
`failed:quartus_fit.exe` after 13,358 s). The 58,359 is a partial mixed total,
not a floor and not a ceiling.

---

# 1. Landed today, with the evidence I reran myself

Not one of these is quoted from an agent's report. Each was rebuilt and rerun.

| work | evidence | saving |
|---|---|---|
| **terrain bake v2** — 7 multiplier sites → 1 muxed 34×34; 1,089-flop `meets` plane → 1 M10K | v1 267/267, v2 267/267, **committed mutant 13/267 FAILED** | DSP 17 → 3–6 *(structural)*; regs ≈ −610 *(structural)*; **ALM unknown** |
| **tmu_pipe palette** — flat 4096-entry `ramstyle M10K`, synchronous read | 82/82; **positive control 18/82 FAILED** | ≈ 7,300 registers, 8 M10K *(structural)*; **ALM unknown** |
| **terrain bump mapping** — answers `bumomapping.md` | 4,738 checks 0 failures; **`-GDELTA_SHIFT=23` control 1,830/4,738 FAILED** | **costs** 0 DSP, ~450 ALM, ~14 M10K |
| **shared projector** `zhao_project_service` | lint clean; adoption gated on the arena | −33 DSP *(structural)* |
| **pair-pipe swap** | six checks incl. 392 byte-identical records | −1,092 ALM, −2,396 regs, +14 MHz *(measured)* |
| **`uncashed_cheques.py`** | self-test 4 fire / 4 no-fire + anti-vacuity gates, **fired deliberately** | finds this class permanently |

## The corrections, which matter as much as the savings

* **`geom_skin MUL_LANES=1` is not a −6 DSP lever.** It delivers 38,965
  vertices/frame against the ruled 120,000 — 32%, and its contract says it "is
  kept because it fails", with a committed test asserting so. **Struck.**
* **Mosaic's −4 DSP is already banked**, and a stale gate comment said otherwise.
* **The arena I commissioned duplicated `zhao_vertex_arena`**, which already had
  58 formal assertions and a committed proof. Kept as a design study; its
  dense-fill idea belongs in the existing primitive as a `VALID_MODE`.
* **`zhao_geom_wcache` drops `w`.** Its 75-bit payload predates the 2026-09-03
  DEPTHQUANT correction by three days. Producers were fixed; the cache between
  them was not.
* **`prod_fit_sources.txt` is read by nothing** — a confident wrong conclusion
  about a failing production fit was drawn from it, and nearly acted on.

---

# 2. The DSP path, corrected

    192  counted
    -33  shared projector          STRUCTURAL, needs the arena first
     -3  rcp24_svc -> rcp24_v3     MEASURED, owner already ruled it
     -2  retire material_combine_v1 owner call, no measurement needed
    ----
    154
    -17  pose_decode to one lane   STRUCTURAL; needs an owner ruling on
                                   "1 decoded bone per clock"
    ----
    137  against a target of 94  ->  GAP 43

**The gap is 43, not 37.** A plan built on the earlier figure was spending six
DSP that do not exist.

## Where 43 could come from — every candidate, with its status

| lever | claim | source | status |
|---|---:|---|---|
| **projector row time-multiplex** | **~21** | `dsp.md:141`, lever 1 of 4 | **never cashed.** Prerequisite (the composed frame budget) satisfied TODAY. Objection is **Fmax**, not throughput. **Architect running.** |
| matrix operand → **18** bits | −18 / −22 | `PROJECT-CORE-OPERAND-WIDTH`, core header | needs a **53° FOV floor** ruling that has never been put to the owner |
| `geom_cull` → ~2 lanes | ~−9 | my derivation from 333,333 ruled evaluations × ~6 products | unfitted; the derivation is mine, not the repo's |
| `terrain_bake` v2 | −11 to −14 | today's build | **structural until fit gate T1** |
| ~~`FILT_LANES=1` on the TMU~~ | ~~−3~~ | `TEXTURE.TMU.md:524` | **STRUCK — see the correction below.** The block is not in the machine |

### Correction, same day: `FILT_LANES=1` is not money

I listed it above and then checked it. The measurement is real and the lever is
not, for the same reason twice over:

* **Neither island instantiates a TMU at all.** `zhao_texture_island_v3_top` and
  `zhao_texture_island_top` mention `zhao_texture_tmu` only in comments — they
  transcribe `decode16` inline. So the frontier's DSP are not inside the
  island's 17.
* **`zhao_texture_tmu` is instantiated by exactly one file**, and it is
  `fpga/rtl/synth/zhao_pair_tmu_cache.sv` — a bench characterization probe. The
  console does not contain this block. Its manifest entry says so:
  `superseded by zhao_texture_tmu_pipe`.

So `FILT_LANES=1` would save 3 DSP on hardware that is not in the machine, which
is the same error as the `material_combine_v1` charge and the `geom_skin`
frontier: **a real measurement of a block the console will not contain.**

**And the check turned up something worth more than the lever.** The SHIPPING
TMU, `zhao_texture_tmu_pipe` — instantiated at `zhao_prod_top.sv:4578` — has
**no measured numbers of any kind**: its row is `failed:quartus_fit.exe`, DSP
null, ALM null. It carries exactly ONE non-comment multiply operator against
`zhao_texture_tmu`'s eleven, so its DSP is probably small; but *probably* is not
a measurement, and **it is not in the 192.** The 192 is an undercount by however
much the shipping TMU costs.

One thing genuinely banked and worth recording: `zhao_texture_tmu@pre-rearch`
measures **28 DSP** against today's 6. That rearchitecture already took 22 out,
and the frontier rows are what remains of the evidence for it.

**These three levers all sit on the same nine products** — sharing, row
multiplexing, and width. Whether they add, multiply or overlap is the question
the running architect must answer, and **double-counting them is the obvious way
to produce a fictitious path to 94.**

---

# 3. The ALM path — bigger, and much less measured

| item | claim | status |
|---|---:|---|
| **`tmu_pipe` palette** | 72,824 regs ≈ 18,206–38,300 ALM | **repaired today**, unmeasured |
| `forge_cliff` | 7,664 ALM estimated | 83% of its regs are two bitmaps; compaction deletes the alive state entirely. Honest speedup 2.0–3.3×, **not 8×** |
| texture owner residency | ~1,200–1,450 regs | endorsed; ready-claimed elimination **conditionally** — existing forwarding gives lost-ticket deadlock |
| `residency_v2` | 2,234 ALM, **fails timing −6.597 ns** | statram split landed today; inference is fit territory |
| **D3 fit-top split** | not stated | absent entirely; **3,214 virtual pins** contaminate every shell attribution |
| `zhao_texture_cache_v2` | brief claims several thousand ALM | **the premise is 4× stale** — patching already captured most of it. Residual is target-vs-measurement, not a demonstrated saving |

---

# 4. Still uncashed — `uncashed_cheques.py` output, live

    zhao_raster_rcp24_v3   3 DSP, 986 ALM   owner-ruled on DSP, but BLOCKED -- see below
    zhao_project_service   fit target       66 DSP -> 33
    zhao_terrain_cmd       1,069 ALM        fitted, uncomposed
    zhao_terrain_loadq       734 ALM        fitted, uncomposed
    zhao_terrain_mipfeed     343 ALM        fitted, uncomposed

### Correction: `rcp24_v3` is NOT merely awaiting execution

I listed it as "owner-ruled, fit-confirmed, not swapped", which reads as a swap
somebody just has to perform. It is not. **Measured by me today, at the island's
own parameters:**

| NCTX | saturated clk/recip | its own 4.6 threshold | checks |
|---:|---:|---|---|
| **8 — what the island instantiates** | **5.78** | **FAILS** | **1/52 failed** |
| 12 | 4.38 | passes | 52/52 |

`zhao_texture_island_v3_top.sv:625` instantiates
`zhao_raster_rcp24_svc #(.NCTX(8), .TOKW(14))`. Dropping v3 in at NCTX=8 installs
a tile that **fails the acceptance threshold it declares for itself.**

The owner's ruling — *"halve the DSPs even if it costs. I think they'll be more
bottleneck than we'd like them to be"* — was about **area and register cost.** It
did not address a functional throughput threshold, because nobody had put one in
front of it. `reports/RCP-V3-THROUGHPUT-IS-NCTX-DEPENDENT-20260908.md` measured
this the day before and the swap has been carried in every summary since as
though the ruling settled it.

**What is genuinely settled:** the DSP halving is parameter-invariant — 6 in
every `svc` row ever produced, 3 in every `v3` row, across NCTX 8/12/16 and TOKW
8/14. So the −3 DSP is real *if the swap happens*; the question is what it takes
to make the swap legal.

**The precise question, which is the owner's:**

1. swap **and raise the island to NCTX=12**, paying for four more contexts of
   per-context state (`svc` holds that state in flops, so this is not free); or
2. swap at NCTX=8 and **re-rule the 4.6-clock threshold** to something 5.78
   meets; or
3. do not swap, and find the 3 DSP elsewhere.

Note that Fmax does not transfer either: the island's reported 62.83 MHz is set
by a pin path unrelated to the tile.

**This is the sixth claim checked and refused today**, and the most instructive,
because the refusal is of my own summary rather than an agent's. "The owner
already ruled it" is exactly the shape of explanation that stops further
checking.

Plus, from check 2 — **fixed and never re-measured**: `terrain_normals` (still
described by a dirty row asserting 18 DSP against an actual 3), `terrain_tess`,
`raster_fragment`, `terrain_project`, `project_core`, `texture_cache_pipe`.

---

# 5. Not built at all, and therefore not in any total

`GEOM.LIGHT`, `TERRAIN.SHADE`, `MATERIAL.RESOLVE`, `FORGE.SHADOW`, `MEM.UPLOAD`,
`FORGE.PRIM.EVAL`. **The machine is incomplete by this much before any of it is
optimised**, and `TERRAIN.SHADE` is the one the bump-mapping work depends on.

Also: **Field v3 has never been fitted at all.** Twelve leaves have targets;
none has run; the only Field rows are superseded ancestors. It is the largest
unmeasured area and DSP block in the tree.

---

# 6. What needs YOU, ranked by what it unblocks

1. **The 53° vertical FOV floor.** Unblocks −18 DSP. The docket has framed width
   narrowing as blocked on bounding the playable world since 2026-08-24; **half
   the prize was never blocked on that.** The question has never been asked.
2. **Relax "1 decoded bone per clock"** in `GEOM.POSE`. Costs 2.9% of a frame,
   returns 17 DSP.
3. **Retire `material_combine_v1`** — both islands use v2. −2 DSP, one word.
4. **`rcp24_v3`: pick one of three.** Your ruling covered DSP cost, not the
   throughput threshold the tile declares. At the island's NCTX=8 it measures
   5.78 clk/recip against its own 4.6 and **fails 1 of 52 checks**; at NCTX=12 it
   passes 52/52. So: raise the island to NCTX=12 and pay for four more contexts
   of flop-held state, or re-rule the 4.6 threshold, or drop the swap. (Your
   ruling also **lives only in a commit message**, not in the docket.)
5. **The redline**, the twelve queued fits, and 8 km terrain (blocked by its own
   Step 0).

## Two corrections you should know about, because they are in documents you read

* **The docket's headline `66 → 33 → 11` is wrong at the third term.** 27-bit
  narrowing buys **zero** — measured: `32×27` costs the same 3 DSP as `32×32`.
  Only ≤18 pays. It has not been amended, and it is the most-read file here.
* **`islandrearchitecture4.md` is byte-identical to `islandrearchitecture5.md`.**
  The docket records a five-step supersession chain that actually describes
  **two** documents, and marks doc 4 "since replaced" — replaced by a copy of
  itself. A reader who skips it as superseded skips the live P0 brief.

---

# 7. The one law under all of it

**A thing built is not a thing installed, and a thing fixed is not a thing
measured.** Three of today's findings are the same shape: the projector
consolidation planned two weeks ago and never performed; the `tmu_pipe` palette
whose tripwire landed and whose repair did not; and bump mapping, **80%
architected two days before it was requested**.

None was forgotten through carelessness. Each was written down, in detail, by
someone who understood exactly what they were deferring. They were forgotten
because **nothing read them back.** That is now `tools/budget/uncashed_cheques.py`
and a chapter in `CLAUDE.md`, so the next one surfaces in seconds instead of
weeks.
