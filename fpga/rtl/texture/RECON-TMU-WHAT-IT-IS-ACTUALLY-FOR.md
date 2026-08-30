# TEXTURE.TMU — recon before the respec

> # CORRECTED 2026-08-30 BY THE V2 RESPEC. READ THIS BOX BEFORE THE FILE.
>
> This recon was right that 850,000 came from Sacrifice's three-layer
> `tile + detail + lightmap` format and is not Zhaozhou terrain demand.
>
> **It then overcorrected.** 276,480 is the TERRAIN-PRIMARY component only. It
> is not the console's TMU workload, and this file's "1.21x clear" reading is
> against terrain alone. Ratified Z60 work already known:
>
>     terrain     CLUT8 nearest        276,480
>     sky backdrop CLUT8 nearest        92,160
>     stars       CLUT8 nearest        128,000
>     clouds      ARGB4444 bilinear     45,000
>     ----------------------------------------
>     known subtotal                   541,640
>
> and that is BEFORE creatures, a giant filling most of a Duo view, beams, and
> cache misses — on 8 km maps with hundreds of textured creatures and objects.
> Against 541,640 the readings become:
>
>     old CLUT II 6      277,778   0.51x
>     today's II 5       333,333   0.62x
>     nearest II 3       555,556   2.6% margin -- not a resting place
>     nearest II 2       833,333   provisional margin
>     nearest II 1     1,666,667   the cache port's own ceiling
>
> **850,000 stays as a named SYNTHETIC whole-frame stress profile.** It is not a
> derived terrain law and must never again be quoted as one.
>
> Also corrected: this file cites `texture_bilerp.sby` P3 as proving the nearest
> bypass. **That BMC does not currently close on the factored filter** — P1..P4
> are marked unproved. The endpoint identity is still exact, but it needs its
> own proof or a citation to the exhaustive equivalence, not to a green P3.
>
> **AND A SECOND CORRECTION, 2026-08-30 (ruling 8).** This file treats "one
> primary sample" as a settled law and reasons from it. It is the cheap DEFAULT
> and must NOT be read as a permanent hardware maximum: the amended rule allows a
> bounded recipe of up to THREE samples through the same primary TMU, because
> Sacrifice's sampled DETAIL layer has no substitute in this machine (the
> lightmap does -- the 33x33 vertex tint -- and AUX returns {tag, strength}, not
> RGB). See reports/MATERIAL_ARCHITECTURE.md. The three-sample known frame is
> 1,094,600 samples, which is tight but not obviously impossible.
>
> The conclusion that survives: the feature set is broadly right, the WORKLOAD
> MODEL was wrong, the one-request-at-a-time implementation is wrong, and the
> expensive filtered path is wrongly in series with the dominant nearest one.

**Status: SUPERSEDED IN ITS NUMBERS by the v2 respec. The feature analysis below
stands; the demand arithmetic is corrected in the box above.**

Fabian and the reviewer concluded 2026-08-30 that the TMU was
"specced wrong from the beginning, able to do too much" and that 850,000 is not
the number it needs. Written while waiting for the replacement spec, to put the
facts that respec will rest on in one place.

`OWNER-DIRECTION-TMU-TARGET-CLOSURE.md`, beside this file, is the OLD direction.
It is a good document about the wrong target — its engineering (resident
palettes, acceptance over latency, pipelined address generation, fill multicast)
survives a respec; its *number* does not. Read this file first.

---

## 1. Where 850,000 came from, and why it is not our number

`design/budgets/workloads.yml` derives it from `docs/OWNER_DOCKET.md` 2026-08-23:

    Z60 384x240, overdraw 3.0 -> 829,440 samples/frame; 850,000 with headroom

    384 x 240            =  92,160 pixels
    x 3.0 overdraw       = 276,480 fragments
    x 3 texture layers   = 829,440 samples      <-- this multiplier
    -> ruled up to 850,000

The docket states the third multiplier's provenance in the same entry:

> "Terrain is layered **tile + detail + lightmap** (`sacmap.d:136-174`), so **at
> least 3 samples per terrain pixel**."

**`sacmap.d` is Sacrifice's map format.** The multiplier is a measurement of the
game we are inspired by, not of the console we are building. It entered the
budget as a fact and has been treated as one ever since.

### Our machine does not do that, and says so in three places

**The charter, §26:**

> "A stable world-space ordered/noise pattern chooses which candidate material
> supplies the pixel. **The TMU performs one primary detail sample rather than
> sampling and blending every candidate.** At 240p the stippled transition
> becomes intentional style."

and §15's meshlet carries "one primary material".

**The reference renderer**, `reference/src/zrender/rast.cpp`, terrain branch,
comment verbatim in the shipping code:

> "**ONE primary sample (charter §15/§26)**: mirrored-repeat fold to the texel,
> optional per-texel Mosaic pick between the cell's two candidates, then the
> modulation with ONE rounding per channel."

The Mosaic pick **chooses a tile id and then samples once**. It does not sample
both candidates. Lighting is not a lightmap fetch either — it rides the
interpolated Gouraud lane (`m.gouraud ? cr : tex->mod_r`).

**The hardware**, `zhao_raster_fragment.sv`: the fragment packet carries
`frag_texel_rgb_i`, `frag_texel_a_i`, `frag_texel_idx_i` — **one texel**. There
is no second or third texel port, no accumulator, and no multi-sample material
state anywhere in the raster path. A three-layer demand has no consumer.

### So the honest demand, on the machine as designed

    384 x 240                    =  92,160 pixels
    x 3.0 overdraw               = 276,480 fragments
    x 1 primary sample (charter) = 276,480 samples/frame

    1,666,667 clocks / 276,480   = 6.03 clocks per sample

**And both remaining multipliers are upper bounds:**

* **Not every fragment is textured.** `rast.cpp` has an untextured-Gouraud
  branch and a sky path beside the textured one. Every pixel that takes them
  costs the TMU nothing.
* **Early-Z runs before texture.** `RASTER.EARLYZ` sits ahead of the sampler and
  the old direction insists it stay there. Fragments it rejects never sample, so
  the TMU sees less than the geometric overdraw of 3.0.

### What that does to every throughput verdict on record

| | clocks/sample | samples/frame | against 850,000 | against 276,480 |
|---|---|---|---|---|
| TMU as of 2026-08-23 | 6 | 277,778 | 0.33x — "no clock fix reaches this" | **1.005x** |
| TMU today (resident palette) | 5 | 333,333 | 0.39x | **1.21x** |
| with the nearest bypass, unbuilt | 3 | 555,556 | 0.65x | **2.01x** |

**The block was never at a third of what this console needs. It was at parity,
and today it is 21% clear.** Every "0.33x", every "no clock fix reaches this",
and the whole II=1 target in the old direction are artefacts of a multiplier
borrowed from another game's map format.

**This is not yet a licence to stop.** 6.03 clocks/sample is an average over a
frame, and it assumes a hit. Misses, the raster rejoin and the real bilinear
fraction all still have to be measured. What it does mean is that the shape of
the problem is completely different: this is a **margin** question, not a
**5x-shortfall** question, and the answer may be "keep it and spend the silicon
elsewhere" rather than any of the rebuilds in the old direction.

---

## 2. "Able to do too much" — the feature surface against the ratified recipes

The block's own header enumerates every recipe it must serve. Tabulated against
what it implements:

| ratified recipe | format | filter | mips | wrap |
|---|---|---|---|---|
| terrain Mosaic tile (terrain_rules §6.2) | CLUT8 64x64 | **nearest** | — | mirror |
| sky drum bands (§1.1) | CLUT8 1024x128 | **nearest** | yes | u-mirror, v-clamp |
| star_disc_masked / star_halo_additive (stars §1) | CLUT8 | **nearest** | yes | — |
| beam_additive_fade (§2) | RGB565 / ARGB4444 16x64 | **bilinear** | — | — |
| sky cloud sheet (§1.1) | ARGB4444 256x256 | bilinear (alpha is a 4th channel) | yes | repeat |
| sun quad (§1.1) | ARGB4444 64x64 | — | — | — |

Three things fall straight out of that table.

**A. Bilinear never touches the full screen.** Every CLUT recipe is
nearest-mandatory — stars §1 says "Nearest mandatory — bilinear must never touch
a palette", and beams are "deliberately not CLUT" for the same reason. So the
palette formats are ALWAYS nearest and the direct-colour formats are where
bilinear lives. **Terrain and sky — the full-screen, overdraw-dominant content —
never use the filter at all.** The filter serves beams, one cloud sheet and a
sun quad: small-area, additive, few-fragment effects.

That inverts the cost story the docket tells. The 6 DSPs and the 21.432 ns
critical path (`q_fmt_r[0] -> smp_a_o[4]`, a registered format bit through
decode, the channel mux and the whole factored bilinear) are spent on the
content that covers the LEAST screen, and they sit in the path of the content
that covers the most.

**B. Two formats have no recipe at all.** `CLUT4` and `ARGB1555` are implemented
in the RTL, in `zref::Tmu`, and in the directed and random suites. No ratified
recipe in `spec/` names either one. They appear only in the sampler's own
plumbing. Whether they are wanted is a decision, not a fact — but they are
currently paid for in decode logic, in the mode word's format field, in every
test matrix, and in the critical path through `decode16`.

**C. The demand-critical path was labelled wrong.** The docket calls CLUT
demand-critical "because terrain is CLUT8" — correct — and then targets the
whole block at the bilinear filter's throughput. But **the CLUT path does not
use the filter.** With the palette resident, a CLUT8 nearest sample is: one
cache access, one palette read, one decode. No products. No filter passes. No
DSPs. The thing that makes the block big and slow is not on the path that
carries the load.

---

## 3. What is genuinely known about cost and timing

Recorded so a respec argues from measurement rather than from this file's
opinion:

* **Fmax 36.11 MHz**, fitted worst path `q_fmt_r[0] -> smp_a_o[4]` at 21.432 ns:
  a registered format bit through `decode16`, the channel select and the entire
  factored bilinear expression to the output. **Against a 100 MHz `gpu_clk`.**
  This is a real problem and it is independent of the demand number.
* **DSPs, measured, not argued:** 28 (old arithmetic) -> 12 (4 lanes) -> **6 (2
  lanes, shipping)** -> 3 (1 lane), with Fmax essentially flat at 36.92 / 36.38 /
  36.11 / 35.62. **Multiplier count was never the timing problem.** Pipeline
  boundaries are — the output is combinationally behind decode, channel select
  and the filter.
* **Cache:** four independent lanes, 16 lines x 16 bytes = **256 bytes a lane**,
  one fill engine, no MSHR, no hit-under-miss, all-or-nothing acceptance. A line
  fill is eight halfword beats, so a miss costs roughly nine blocked clocks.
* **Palette residency landed today** (commit 9059eeb): CLUT 6 -> 5 clocks, with
  an invalidate proved by mutation. That change is orthogonal to the respec — it
  removes an access the sampler should never have been making, at any target.

---

## 4. The questions the respec should answer

Written as questions because they are the owner's and the reviewer's to settle,
not this file's.

1. **What is the real sample budget?** If it is one primary sample per textured
   fragment, the arithmetic above gives ~276,480/frame and ~6 clocks each. Is
   overdraw 3.0 still the right stress figure once early-Z is in front, and what
   fraction of fragments are textured at all?
2. **Does bilinear stay?** It exists for beams, one cloud sheet and a sun quad.
   It costs 6 DSPs and owns the critical path. A cheaper option is a
   restricted filter for direct-colour only, off the CLUT path entirely.
3. **Do CLUT4 and ARGB1555 stay?** No ratified recipe uses either.
4. **Should the CLUT nearest path and the direct bilinear path be one datapath
   at all?** They share address planning, the cache, the wrap laws and the
   output. They share no arithmetic. The block is currently one FSM that walks
   both, and the expensive half is in series with the cheap one.
5. **What is the mip story?** Three recipes ask for mips and the LOD comes in on
   the request. Nothing in the machine computes an LOD yet.
6. **Is 100 MHz the constraint?** Fmax 36.11 is a third of it. Whether that is
   fixed by pipelining the TMU or by something else is a console-level call.

## 5. What NOT to lose from the old direction

Independent of the number, and each already paid for:

* **Resident palettes.** Landed, tested, mutation-verified. Right at any target.
* **Acceptance rate, not latency.** The Field engine measured four latency
  reductions that each made its worst program slower, and one acceptance-rate
  increase worth 31%. Ask of any TMU change whether it makes the block accept
  sooner or merely finish sooner.
* **Nearest must not pay for the filter.** At fu = fv = 0 the filter is the exact
  identity on tap 0 — proved in `tests/formal/texture_bilerp.sby` P3 — so a
  nearest sample currently spends filter passes computing its own input. This is
  the cheapest remaining change and it helps every full-screen recipe.
* **Early-Z stays in front of the sampler.**
* **Never identify a record by `src_id`**, and make tests reuse source IDs
  deliberately.
* **A rejection is only valid against the bottleneck it was measured on.** The
  Field lane rebuilt three changes it had already rejected once the wall moved,
  and two of them then paid.
