# The joint envelope: what the console is ALREADY promised to serve

Coordinator, 2026-09-29, on `design/zhaozhou-v2-rfc`.

**Status: extraction, not a proposal.** Every figure below is transcribed from a
named authority with its file and line. Nothing here reduces, reinterprets or
ratifies anything, and the four genuinely unresolved choices are collected at the
end for the owner rather than decided.

Requested by the external reviews: *"Extract prior numerical commitments and
current accepted content traces. Prepare a small number of combined profiles and
their visible consequences, preserving existing guarantees … Bring the owner a
proposed product envelope with the true unresolved choices, not a blank request for
an expert workload vector. Do not reduce it unilaterally."* (R4 §8, restated in
R5 §8 and R6 §5.)

---

## 0. The first finding is that the addends do not share a denominator

`tools/budget/envelope_extract.py` over all 140 contracts:

| frame length each contract prices itself against | contracts |
|---|---:|
| **1,666,666 clocks** — 100 MHz / 60 Hz | 13 |
| **1,333,333 clocks** — 80 MHz / 60 Hz | 9 |
| none stated at all | 6 |

and of **18** percent-of-frame claims, only **4** name the frame length they are a
percentage of.

**The ratified number is 1,666,666.** `design/budgets/workloads.yml:51-58` fixes
`computeClocksPerFrame: 1666666` and explains the rounding deliberately — *"100e6 /
60 = 1,666,666.67 … Rounding UP grants a clock the hardware does not have … a
budget that rounds in its own favour is the kind of number nobody re-checks."*
`design/contracts/FIELD.SEQ.EARTH.md:48-53` agrees on the design point.

So the nine contracts quoting 1,333,333 are inconsistent with the ratified budget.
**Note the direction before drawing a conclusion from it:** 1,333,333 is the
SHORTER frame, so those contracts' percentages are *pessimistic* — they overstate
their own cost against the budget the console actually keeps. That is the unusual
direction, and it means this is a tidiness defect, not a hidden overrun. It still
has to be fixed before any of those percentages is summed with any other.

**`workloads.yml` also names the trap that makes this worse than a units nit**
(`:20-25`): the video deadline for Z60 is `videoClocksPerFrameZ60: 251520`, and
*"Summing per-block cost against 251,520 is wrong by 6.6x. That mistake is recorded
in latency.md because it was made."* There are therefore **three** plausible
denominators in circulation and two of them are wrong for this purpose.

## 1. What is additive and what is not

This has to be settled before a single number is added, because the obvious
reading is wrong and it is wrong in the alarming direction.

`design/budgets/workloads.yml` gives, per block, a per-frame demand and a required
initiation interval. Multiplying them:

| block | demand/frame | II | clocks | of 1,666,666 | confidence |
|---|---:|---:|---:|---:|---|
| `zhao_geom_cull` | 333,333 evaluations | 5 | 1,666,665 | **100.0 %** | `ruled` |
| `zhao_terrain_project` | 1,572,864 projections | 1 | 1,572,864 | **94.4 %** | `derived` |
| `zhao_geom_skin` | 120,000 vertices | 1 | 120,000 | 7.2 % | `ruled` |
| `zhao_geom_project` | 120,000 vertices | 1 | 120,000 | 7.2 % | `inherited` |
| `zhao_surface_stamp` | 20,000 texels | 1 | 20,000 | 1.2 % | `derived` |
| `zhao_terrain_normals` | 2,000 normals | 833 | 1,666,000 | **100.0 %** | `derived` |
| `zhao_texture_tmu` | — | — | — | — | *the row says a single pair cannot describe it* |

Those add to well over 300 %, and **that sum is meaningless.** Each row is a
separate pipeline in its own silicon; the percentage is that block's own
OCCUPANCY, not a share of one pool. Three blocks each running at 100 % occupancy
is a correct machine, not an impossible one.

**What genuinely IS additive across the console:**

* **area** — ALM, DSP, M10K. One device, one budget: 41,910 ALM / 112 DSP /
  553 M10K on `5CSEBA6U23I7`.
* **shared-resource bandwidth** — SDRAM, the VRAM arbiter, grant-clocks on any
  single arbiter. `FORGE.SHADOW.md:707-716` is the model to copy here: it states
  *669,376 grant-clocks of 1,666,666 = 40.2 %* for four client-A arms plus
  terrain, worst case, on one arbiter, which is a real share of a real pool.
* **the clock** — `gpu_clk` is shared, so the console's Fmax is the **minimum**
  over blocks, never a sum. `GEOM.SKIN.md:186-190` says exactly this: *"`gpu_clk` is
  shared and a block that stops at 86 MHz caps every other block on the same
  clock."*

**What is NOT additive:** per-block occupancy, per-block percent-of-frame, and
anything quoted against a frame length its author did not name.

**And two of the rows above are at or past their own ceiling already, which is a
finding independent of any envelope.** `zhao_geom_cull` is `confidence: ruled` at
exactly 100.0 % — zero headroom by construction — and
`GEOM.MESHFETCH.md:336-346` records that **no arm of the block has ever met it**:
*"333,333 x 11 clocks is 275% of the reserved frame for the FITTED arm … It needs
re-ruling against the demand figure, and that is an owner call, not made here."*
That call is still open and it is listed in §4.

## 2. The scenario dimensions, each with its authority

These are the five things the reviews ask whether the console was promised
*simultaneously*. Each is a real, sourced commitment on its own.

| dimension | the commitment | authority |
|---|---|---|
| **Earth stress** | ≤ 850,000 Field/Earth-slice clocks for the **128-association** stress frame; hard per-association target ≤ 6,000 clocks per full 1,089-vertex association | `FIELD.SEQ.EARTH.md:167-173` |
| **Maximal geometry** | 120,000 vertices/frame demand, served at 166,666 by `MUL_LANES=6`; the acceptance test is **rate, not clock** | `GEOM.SKIN.md:169-190`, `:366` |
| **The guaranteed giant** | its **32,768 tile references** are reserved before ordinary kMesh; a frame explicitly containing no giant may release them | `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt:369-380` |
| **Texture workload** | the TMU's own row declines to be described by one `itemsPerFrame`/`requiredII` pair | `workloads.yml:60-76` |
| **Duo** | two independent 256×192 view canvases **side by side** on a 512×240 raster (view 0 at x ∈ [0,255], view 1 at x ∈ [256,511], both vertically centred at y ∈ [24,215]); cull rejects only when the sphere is outside **every** active camera | `spec/video_rules.md:117-126`; `GEOM.MESHFETCH.md:22` |

**Duo is the dimension that multiplies the others** rather than adding to them: a
second active camera means a second cull decision and a second view's vertex ids
for the same meshlet (`GEOM.ASSEMBLE.md:114-143`). Whether the 120,000-vertex
demand is per FRAME or per VIEW is not stated anywhere I can find, and it is the
single highest-leverage ambiguity in this document — the two readings differ by 2×
on the largest geometry number the console has.

### A contract describes the Duo layout wrongly, and still reaches the right answer

`GEOM.BINNER.md:25` cites `spec/video_rules.md` §3.1 for *"Duo's two 256×192 view
blocks **STACKED at rows 0 and 192**"*. §3.1 says the opposite arrangement: the two
canvases sit **side by side** on a 512×240 raster, at x ∈ [0,255] and
x ∈ [256,511], both vertically centred at y ∈ [24,215] with 48 border rows.

**Its conclusion survives its premise**, which is why nobody caught it. BINNER
anchors the tile grid on the SURFACE rather than on the viewport, and each view's
surface is 256×192 either way, so the 16-alignment argument holds. Under the
premise as written it would hold too. Worth noting that the displayed y origin is
**24**, which is *not* a multiple of 16 — so a grid anchored on the displayed
viewport, the alternative BINNER considers and rejects, would in fact straddle in
Duo. The rejected option is more wrong than the file says, and the chosen one is
right for a reason the file states inaccurately.

Recorded here rather than silently corrected: the fix belongs in `GEOM.BINNER.md`,
and a sentence in an envelope document is not an amendment to a contract.

## 3. Combined profiles, stated as consequences rather than as a choice

Three profiles, in increasing severity. **None of these is proposed as the
envelope**; they exist so the owner can say which one was meant.

**Profile A — "each dimension at its own worst, one at a time."**
Every commitment above is met in a frame that stresses that dimension alone.
*Consequence:* this is what the contracts, read literally, individually promise,
and it is the weakest reading. It also happens to be the reading under which
`zhao_geom_cull`'s ruled row is already unmet, so even Profile A has an open
defect in it.

**Profile B — "Z60 single view, everything at once."**
Earth stress at 128 associations, 120,000 vertices, the giant resident, full
texture demand, one camera.
*Consequence:* the additive quantities are the ones that bind — area and arbiter
share. Earth's 850,000 clocks is 51 % of the compute frame on its own, and the
Field slice is a separate pipeline from geometry, so the two coexist on time; they
compete on **ALM and on gpu_clk**, which is exactly where the console is already
351 % over on ALUTs.

**Profile C — Profile B in Duo.**
*Consequence:* if the 120,000-vertex demand is per view, geometry doubles and
`GEOM.SKIN`'s `MUL_LANES=6` arm (166,666/frame) fails at 240,000. If it is per
frame, Duo costs a second cull pass and a second set of view ids and geometry does
not double. **The reading decides whether the shipped skinning arm is adequate or
short by 44 %**, which is why §4 asks it first.

## 4. The genuinely unresolved choices — for the owner, not decided here

Only four, and each changes a product guarantee rather than an implementation
detail.

1. **Is the 120,000-vertex demand per FRAME or per ACTIVE VIEW?** Decides whether
   Duo doubles geometry, and with it whether `MUL_LANES=6` is adequate. No source
   in the tree states it.
2. **Was Duo promised at the same content density as Z60**, or is a lower density
   acceptable in two-view mode? Nothing reduces anything in Duo today.
3. **`zhao_geom_cull`'s ruled 333,333 evaluations at `requiredII: 5`.**
   `GEOM.MESHFETCH.md` says no arm has ever met it and that re-ruling it is an
   owner call. It is also the one row at exactly 100.0 % occupancy.
4. **Is the guaranteed giant simultaneous with Earth stress?** The directive
   reserves its 32,768 references unconditionally and lets a frame *without* a
   giant release them; it does not say a 128-association Earth frame is a frame
   that may be without one.

## 5. What this document deliberately does not do

* It does not sum per-block occupancy, for the reason in §1.
* It does not recompute any contract's percentage against a denominator its author
  did not use. `envelope_extract.py` refuses this too, by construction.
* It does not reduce any commitment, which is outside the standing delegation
  (`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt`: not authority to *"shrink
  the guaranteed giant"* or *"call reduced work equivalent"*).
* It does not price V2. The envelope is what any V2 must serve; what a V2 costs is
  the separate experiment now running on the Field slice.
* It does not assert that any transcribed figure is still correct. Several carry
  `confidence: derived` and the docket's own instruction is *"overturn on sight"*.
  A stale number quoted with its authority is still a quotation, and the authority
  is the place to fix it.
