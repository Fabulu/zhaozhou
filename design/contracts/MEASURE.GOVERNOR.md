# Contract — MEASURE.GOVERNOR (Screen-error LOD governor)

> Ledger: `design/blocks.yml` · owner ZH-047 · phase 8 · maturity SPECIFIED
>
> RTL: `fpga/rtl/measure/zhao_measure_governor.sv`
> Oracle: `reference/include/zref/zref_measure.hpp` (`zref::measure::LodGovernor`)
> Tests: `tests/measure/measure_governor_directed.cpp`,
> `tests/measure/measure_governor_random.cpp`,
> `tests/measure/measure_governor_lod.cpp` (composition with the LANDED
> TERRAIN.LOD)

## Purpose and exclusions

Per-camera screen-error policy for LOD selection, plus per-view degradation
under budget pressure that cannot cross between players.

**This contract was a STUB, and TERRAIN.LOD landed against it anyway.** That is
the first thing to know: `design/contracts/TERRAIN.LOD.md` records that this
file's only content was "Hysteresis/hold constants provisional until Wound Lab
evidence", so LOD had to CHOOSE the governor's policy shape itself. This block
is written to HONOUR that choice, not to replace it — and where it found a
disagreement it says so rather than quietly picking a side (see *The
discrepancy inside TERRAIN.LOD*).

**Out of scope, deliberately:** no threshold PREDICTION — charter §9 Version 1
is explicit that *"ARM predicts a pixel-error threshold per camera from prior
counters"*, and `SetView.pixel_error` already carries it on the wire; no error
histogram and no cutoff bucket (MEASURE.HISTOGRAM, charter Version 2, and see
*the upstream edge with nothing on it*); no priority heap (§9: *"Do not begin
with a global FPGA priority heap"*); no token accounting (MEASURE.TOKENS owns
the pools — this block takes a one-bit-per-view verdict from them); no camera
POSITIONS, no `dual`, no `edge_*`.

## THE SEAM — TERRAIN.LOD already chose it

TERRAIN.LOD's Notes law 8:

> *"The governor's per-camera policy is ONE ratio, not two numbers. Charter §9
> gives a 'projection scale' and a 'per-camera pixel-error threshold'; the
> ladder only ever uses their quotient, and carrying the quotient keeps the
> comparison exact. Carrying both would force a division or a rounding inside
> the block for no expressive gain."*

Honoured exactly. The two numbers arrive here, **the division happens here** —
once per frame per camera, the cheapest place in the machine for it — and
TERRAIN.LOD keeps its exact, rounding-free ladder.

### The discrepancy inside TERRAIN.LOD, reported rather than resolved

TERRAIN.LOD's ladder is `dev[L] · scale ≤ distance · h`, coarsest level wins.
`scale` multiplies the **deviation**, so a **larger scale makes the test harder
and the result FINER**.

`reference/include/zref/zref_terrain_lod.hpp` says the opposite in prose:
*"world-units of allowed error per world-unit of distance"* and *"Larger =
coarser."* **The prose is backwards relative to the arithmetic in the same
file.** The arithmetic is what ships, what `zhao_terrain_lod.sv` implements
(`lhs = dev * scale`, `rhs = dstv * h`), and what `terrain_lod_directed` pins
(*"scale = 256 (1.0), so the ladder is `dev ≤ distance`"*, with larger `dev`
failing). This block is written against the ARITHMETIC.

Dimensionally the arithmetic is the correct one:

```
projected pixels of error  ≈  dev · proj_scale / distance
admissible                <=>  dev · proj_scale / distance ≤ px_err
                          <=>  dev · (proj_scale / px_err) ≤ distance
```

so `scale = proj_scale / px_err` — pixels-per-unit-angle over allowed pixels,
i.e. **inverse tolerance**. It is not asserted, it is WORKED END TO END for the
shipped Duo canvas in the directed lane and it lands on the right pixel count:

> `spec/video_rules.md` §3.1: VIDEO_DUO is two 256×192 canvases. At 60° of
> horizontal FOV the projection scale is `(256/2)/tan 30° = 221.70` px per unit
> tangent = Q8.8 raw 56755. With a 2-pixel budget (`px_err = 2.0` fx16):
> `scale = round_half_up(56755 · 65536 / 131072) = 28378`. TERRAIN.LOD then
> admits a level while `dev · 28378 ≤ distance · 256`, so at 100 units the
> largest admissible deviation is 0.9021 units, which projects to **1.9999
> pixels**. That is the budget.

**Nothing in TERRAIN.LOD was changed.** The prose fix belongs to that block's
owner; this contract records the discrepancy and the evidence for which side is
real.

### The upstream edge with nothing on it

The ledger lists `upstream: [CMD.SCHEDULER, MEASURE.HISTOGRAM]`.
MEASURE.HISTOGRAM does not exist and is charter **Version 2**, whose cutoff
bucket this Version-1 governor by definition does not consume. That edge is
named as unbuilt rather than faked with an invented port. See
`design/contracts/MEASURE.HISTOGRAM.md` for why it was refused this increment.

### `reference_model` DOES NOT RESOLVE

`zref::MeasureGovernor` names nothing in this tree and never has — the **ninth**
phantom after `zref::CmdDma`, `zref::SurfaceStamp`, `zref::SurfaceSheet`,
`zref::AuxSource`, `zref::TerrainBake`, `zref::TerrainVelocity`,
`zref::ProgCache` and `zref::MeasureTokens`. Amended in `design/blocks.yml` to
`zref::measure::LodGovernor`. Like the tokens oracle it is a FIRST
implementation, not a view onto an executed reference: nothing in
`reference/src` runs a governor.

## Laws found

1. **The threshold is an INPUT.** Charter §9 Version 1, and `SetView 0x0010`'s
   `fx16 pixel_error`.
2. **Each camera provides a projection scale and a pixel-error threshold**
   (charter §9 "Inputs"). *But `proj_scale` has NO ABI FIELD* — `SetView`
   carries `mat4fx view_projection` and `fx16 pixel_error` only. Deriving the
   scale from the matrix is the producer's job and it is named here as a GAP:
   the port exists, the command that fills it does not.
3. **Every LOD path requires hysteresis, a minimum hold, and geomorph**
   (charter §9 "Stability").
4. **The split is per view.** `FORM_LANGUAGE_HARDWARE_CODESIGN.md` §12 declares
   `view left … budget 45% / view right … budget 45% / shared emergency budget
   10%`, and item 16: *"Split-screen budgets are declared and enforced as part
   of presentation."* Declared per view ⇒ enforced per view ⇒ degraded per view.
5. **`round_half_up(n/d) = floor((n + floor(d/2))/d)`** for `d > 0`, and one
   rounding per result (`spec/qformats.md` §3).
6. **Split screen is shipped, not hypothetical.** `spec/video_rules.md` §3/§3.1:
   `VIDEO_DUO`, two 256×192 canvases on the 512×240 raster.

## Laws CHOSEN, not found

Numbered identically in the RTL header and in the oracle.

**G1. `scale = round_half_up((proj << (16 − deg)) / px_err)`, clamped to 16
bits, with EXACTLY ONE rounding.**
*Rejected:* truncation (cheaper by one adder, and it is not the ratified
integer-division rounding — a governor rounding differently from the rest of the
machine would be a second rounding law); a reciprocal-and-multiply (two
roundings, forbidden by §3, and no cheaper at one divide per frame).

**G2. Degradation is a POWER-OF-TWO ladder on the numerator, so it is exact.**
`deg ∈ 0..3` shifts the numerator right, dividing `scale` by 2^deg and
multiplying the allowed pixel error by 2^deg. A shift introduces no rounding, so
G1's single rounding survives.
*Rejected:* a Q8.8 degrade multiplier (1.0, 1.25, 1.5, …). Finer control, and it
needs either a second rounding (banned by §3) or a 42-bit divide to fold the
multiply into the quotient — a four-times-wider divider for a knob no evidence
exists to set.

**G3. Each view's rung is a function of THAT VIEW'S pressure and nothing else.**
There is no path in the RTL from `starved1_i` to `cam0_scale_o`. This is charter
§9's Duo fairness sentence applied to the POLICY, the way MEASURE.TOKENS' law T2
applies it to the POOLS — the same shape deliberately.
*Rejected, and this one matters:* **a single global degrade rung** driven by
whichever view is worse off. Simpler, one register instead of two, and it is
exactly the failure the charter names — one player looking into a volcano would
coarsen the OTHER player's world. A global rung also cannot express the declared
45/45 split at all: two views with equal budgets and unequal load must degrade
unequally, and one number cannot say that.
*Enforced by:* `measure_governor_directed.cpp:test_volcano` **and**
`measure_governor_lod.cpp`, which proves it on emitted geometry against the
REAL TERRAIN.LOD.

**G4. The ladder climbs IMMEDIATELY and recovers only after a hold.**
A starved frame raises the rung at once — a stall is visible now, so the
response is now. Recovery waits `DEG_HOLD` unstarved frames. The asymmetry IS
charter §9's *"no visible threshold flicker"* gate.
*Rejected:* a symmetric hold (the oscillation delayed, not removed); immediate
recovery (oscillation at frame rate); never recovering (one bad frame would
coarsen a whole level).
*Measured:* 200 frames of alternating starvation move the rung **exactly 3
times** and then never again.

**G5. The stability constants are PARAMETERS of this block and PROVISIONAL** —
the ledger already says so. They are not ABI fields (no command carries a
hysteresis band), so a port would only move the invention to a producer with
nothing to base it on either.

| constant | value | why, and what was rejected |
|---|---|---|
| `HYST_Q88` | 320 (1.25×) | TERRAIN.LOD reads anything below 256 as 256, i.e. as NO hysteresis, and charter §9 says every LOD path REQUIRES it — so 256 is not an option. *Rejected:* 512 (2×), a full level of slack, which makes the ladder lag a whole rung behind a moving camera. |
| `MIN_HOLD` | 6 frames | 100 ms at 60 Hz. |
| `MORPH_STEP` | 10923 (Q16/frame) | **Chosen to satisfy a theorem, not picked.** `MIN_HOLD · MORPH_STEP ≥ 65536` (6 · 10923 = 65538), so a geomorph ALWAYS reaches unity before the minimum hold can permit the next change: no level is ever replaced mid-morph. *Rejected:* the floor 65536/6 = 10922, which leaves the morph 4/65536 short at exactly the frame the hold expires — the one frame in which a half-morphed subpatch could be re-targeted. |
| `DEG_HOLD` | 12 frames | Twice `MIN_HOLD`. A rung change moves EVERY subpatch's target at once, so it must be rarer than a per-subpatch change or it becomes the dominant source of level churn. |
| `DEG_MAX` | 3 | Four rungs: 1×, 2×, 4×, 8× the allowed pixel error. |

**G6. `px_err == 0` yields 0xFFFF and `proj == 0` yields 0.** Neither is a
special case bolted on — they are the limits. Zero allowed pixel error demands
infinite precision and the finest the ladder can be asked for is the largest
scale; a camera with zero projection scale puts nothing on screen and the
coarsest ladder is right for it.
*Rejected:* refusing the frame — the governor would have no targets to present
and TERRAIN.LOD would sample stale ones, i.e. silence in place of an answer.
(The divisor is separately forced to 1 in the `px_err == 0` case so the
restoring divider's `rem < den` invariant holds and its remainder cannot run
past 32 bits; the RESULT is overridden either way.)

**G7. The counter is FRAMES SPENT AT EACH DEGRADE RUNG, summed over enabled
views.** `lod_representation_counts` needs a source, this block makes no
per-object representation decision, and the rung IS the representation allowance
the Measure granted that frame. Four rungs, four lanes — the same shape
TERRAIN.LOD's four lanes have.
*Rejected:* not driving the counter and recording a ledger deviation. The rung
is a genuine measurement of what the Measure allowed, and a post-mortem asking
"how long was player 2 degraded" has nowhere else to look.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.

**Reset publishes a SAFE, VALID policy rather than zero.** `cam*_scale_o` reset
to 256 — the ladder's own neutral point, `dev ≤ distance` — because a zero scale
would make TERRAIN.LOD admit every level and put the whole world at its coarsest
before the first frame ever arrives. `cam0_en_o` resets high, `cam1_en_o` low
(the single-view default). Rungs, holds and counters reset to zero.

## Input and output packet layouts

`dispatch` in (CMD.SCHEDULER). `frame_i` is a one-cycle pulse: decide now.

| field | width | meaning |
|---|---|---|
| `view_count_i` | 2 | `SetPresentationContract.view_count` |
| `px_err0_i` `px_err1_i` | 32 | `SetView.pixel_error`, fx16 unsigned |
| `proj0_i` `proj1_i` | 16 | camera projection scale, Q8.8 unsigned — **no ABI field yet** (found-law 2) |
| `src_id_i` | 16 | `source_ids: true` |

`screen_error_stats` in — **one bit per view**, and each reaches only its own
view's rung (G3): `starved0_i`, `starved1_i`. MEASURE.TOKENS is the natural
producer: a view was starved iff it had a request denied against its own
guaranteed pool.

`lod_targets` out — **HELD registers, not a stream.** TERRAIN.LOD samples them
with each descriptor and requires them stable across a patch job, so they change
only at `targets_valid_o` and the old values stand until then. That is checked
on every frame of every lane, not asserted.

| field | width | TERRAIN.LOD port |
|---|---|---|
| `cam0_scale_o` `cam1_scale_o` | 16 | `cam0_scale_i` / `cam1_scale_i` |
| `cam0_en_o` `cam1_en_o` | 1 | `cam0_en_i` / `cam1_en_i` |
| `hyst_o` | 16 | `hyst_i` |
| `min_hold_o` | 8 | `min_hold_i` |
| `morph_step_o` | 17 | `morph_step_i` |
| `src_id_o` | 16 | rides the decision |
| `deg0_o` `deg1_o` | 2 | — (capture / post-mortem) |
| `targets_valid_o`, `busy_o` | 1 | — |

**The camera POSITIONS, `dual` and `edge_*` are NOT here.** TERRAIN.LOD's
contract groups them under `lod_targets`, but they are not policy: the eyes come
from `SetView.view_projection`, `dual` is a property of the page, and `edge_*`
are the neighbouring patches' border levels. None of them can come from a block
that sees one number per frame per camera, and inventing ports for them would
make this block claim ownership of three things it does not own.

## Backpressure rules

`ready_valid` per the ledger, degenerate by construction: one decision per
frame, `busy_o` high while a decision is in flight, `frame_i` ignored while
busy. There is no queue and nothing to drop — a second frame pulse inside a
decision is the caller violating a once-per-frame protocol, and the block
finishes the decision it started rather than abandoning it.

## Memory ownership

**None.** No VRAM port, no cache, no M10K. The whole state is two rungs, two
holds, the divider (33-bit numerator, 32-bit divisor, 32-bit remainder, 33-bit
quotient, a 6-bit step), the latched frame operands and the published targets.

## Q formats and rounding

`proj` Q8.8 unsigned, `px_err` fx16 unsigned, `scale` Q8.8 unsigned.
**Exactly one rounding in the block**, `round_half_up` on the division
(qformats §3). The degrade is a shift and is exact; the clamp is a comparison
and is exact; nothing else rounds.

## Latency (fixed or variable)

`variable` per the ledger. **MEASURED: 69 clocks** from the frame pulse to
`targets_valid_o` — 1 (accept) + 33 (camera 0's restoring divide) + 1 (load) +
33 (camera 1) + 1 (publish). It is data-independent: the second decision costs
the same as the first, checked directly.

## Target throughput

`1 decision per frame per camera`. At 69 clocks per frame for both cameras and a
60 Hz frame at any plausible gpu clock, the block is idle more than 99.99% of
the time. **The ledger target is met with about four orders of magnitude of
headroom, and that is the reason the division was accepted here at all.**

## Overflow and malformed-input behaviour

- The quotient CLAMPS at `0xFFFF` (G1) — tested at the clamp edge, one past it,
  and with a 32-bit-wide true quotient.
- `px_err == 0` and `proj == 0` are the two limits (G6), not errors.
- The rung SATURATES at `DEG_MAX`; further starvation changes nothing.
- The recovery hold saturates rather than wrapping; a starve inside the hold
  re-arms it from zero.
- Counters saturate at `0xFFFF_FFFF` (`spec/counters.md` §4).
- `view_count_i == 0` turns both cameras off; the ratios are still computed and
  published, because a disabled camera's policy is still the policy it will have
  when it is enabled.

## Counters and traces

`lod_representation_counts` — four lanes, `lod_rep_count0_o`…`3_o`: frames at
each degrade rung, summed over ENABLED views (law G7). Both views at the same
rung add two to that lane.

## Scalar reference function

`zref::measure::LodGovernor`. `frame()` is one decision: the rungs move FIRST
(from the frame that just ended) and the ratios are computed at the NEW rung.

## Directed tests

`tests/measure/measure_governor_directed.cpp` — **4,758 checks, all green.**

1. **The worked Duo frame** — the hand-computed 28378, then made to MEAN
   something: 1.9999 projected pixels against a 2.0 budget.
2. **The rounding tie, CONSTRUCTED** — `px_err = 2.0` fx16 makes the divisor
   even, so an ODD `proj` leaves the remainder at exactly `d/2`. 32 of them, all
   rounding up, and each one is a value where truncation gives a different
   answer. Uniform random operands reach this with probability ≈ 2⁻³².
3. **The limits and the clamp** (G6, G1), including the clamp edge and one past.
4. **The degrade ladder is exact** — 4096 → 2048 → 1024 → 512, and it saturates.
5. **THE VOLCANO** — 20 starved frames on view 1 drive it to rung 3; view 0's
   scale does not move by one LSB.
6. **The hold boundary, CONSTRUCTED** — no recovery on the 11th clean frame,
   recovery on the 12th; and a starve inside the hold restarts the whole clock.
7. **Anti-thrash, MEASURED** — 200 alternating frames, exactly 3 rung changes.
8. **The morph theorem** — `6 · 10923 = 65538 ≥ 65536`, read off the ports.
9. **Agreement with the landed TERRAIN.LOD** — hysteresis strictly above 256,
   morph step non-zero, every field inside LOD's declared port domain.
10. **Latency (69 clocks) and the mid-decision hold.**
11. **Enables and the rung counter.**

## Randomized differential tests

`tests/measure/measure_governor_random.cpp` — **130,039 checks green**, two
lanes of 4,000 frames each (24,000 under `--nightly`), **0 mismatching frames
and 0 hold violations in either lane**.

Starvation arrives in **bursts**, not independent per-frame coin flips:
independent flips never produce the sustained pressure a real overloaded view
creates, and the hold's expiry needs a long clean run to be reachable at all.

| construction | lane A (workload) | lane B (limit) |
|---|---|---|
| R1 remainder exactly `d/2` (the round-half-up tie) | 682 | 688 |
| R2 just under the tie | 671 | 640 |
| R3 quotient exactly `0xFFFF` | 698 | 665 |
| R4 quotient past the clamp | 683 | 673 |
| R5 `px_err == 0` | 661 | 646 |
| R6 `proj == 0` | 674 | 632 |
| R7 hold expiring on exactly its last frame | 182 | 85 |
| R8 top rung re-starved | 1,736 | 3,847 |
| frames that actually clamped | 1,267 | 2,026 |
| frames at the bottom rung | 3,458 | 3,988 |
| recoveries | 287 | 92 |

Law G3 is additionally checked on **every frame**: an unstarved view's rung
never rises, whatever the other view did.

## Composition — GOVERNOR → the LANDED TERRAIN.LOD

`tests/measure/measure_governor_lod.cpp` — **72 checks green.** Both blocks
REAL, wired port-for-port with **no adapter**; a width or type change on either
side is a compile error here before it is a silent truncation in the machine.

- **The volcano, END TO END.** 60 frames of view 1 starved drive it to rung 3
  (scale 28378 → 3547). The ground TERRAIN.LOD emits stays at **1,616 triangles,
  unchanged, not one triangle lost**, because charter §9's third rule takes the
  FINER of the two cameras and view 0 still needs it fine. *That is "one player
  looking directly into a volcano cannot make the other player's army
  disappear", measured in triangles.*
- **And the degrade is NOT vacuous.** Starve BOTH and the same patch falls to
  **92 triangles — 5.7%**. Without this, the line above would also hold for a
  governor whose degrade did nothing.
- **Recovery, and a MEASURED result that contradicted the first assertion
  written.** After 400 clean frames the patch recovers to **1,514 triangles,
  93.7% of the cold-start count, and stays there for 364 frames.** It does NOT
  return to 1,616 — and it should not. TERRAIN.LOD's hysteresis is a BAND and a
  level inside the band is RETAINED, so a patch arriving from the coarse side
  settles on a different, coarser stable point than the same patch from a cold
  start. That is the definition of hysteresis and the whole reason charter §9
  requires it. The first version of this file asserted exact return and was
  **wrong**: it asserted the ABSENCE of the property the consumer's contract
  requires. It now asserts the three things that are true — the recovery is
  real, it never overshoots past the cold-start state, and it is stable.

**A second wrong assumption, also recorded:** the settle originally ran 12
frames. That is nowhere near enough — LOD walks its ladder ONE rung at a time,
each rung costing a geomorph (6 frames) plus the minimum hold (6 more), and the
patch spans three rungs. The count was still drifting when the volcano phase
began, which showed up as a *false* failure of the fairness check. The settle
now runs 200 frames and PROVES it settled (≥ 20 identical frames) before
anything is compared.

## Formal properties

**NONE, and that is a decision.** The ledger names no formal lane for this
block, and the block's central law is not a good formal target: it is a
33-step restoring divide, which is precisely the shape that cost this repo a
banked property — `terrain_bake_delta.sby`'s BMC sat 10.7 hours on a 17-step
restoring divide unrolled into SMT. A proof that cannot finish is worse than
none. The fairness law G3 *would* be tractable in isolation, but it is already
proved twice over by construction (there is no wire from `starved1_i` to view
0's rung) and demonstrated on emitted geometry through a real TERRAIN.LOD, which
is stronger evidence than a one-step induction over two 2-bit registers.

## Synthesis / resource ceiling

One 33-step restoring divider (a 33-bit shifter, a 33-bit comparator, a 32-bit
subtractor), two 2-bit rungs, two 8-bit holds, four saturating 32-bit counters
and the published target registers. No RAM, no DSP, no multiplier. Conservative
SystemVerilog subset (charter §2); **no function-call result is indexed
anywhere in this file.** Simulated and linted; **not synthesized and not timed
on hardware.**

## Integration capture cases

None yet. The first worth having is a `VIDEO_DUO` frame where one view is
starved and the other is not, captured through GOVERNOR → TERRAIN.LOD →
TERRAIN.TESS so the fairness result appears in golden geometry rather than in a
test's triangle count. That needs a Duo capture harness, which does not exist.

## Notes

**MUTATION-CHECKED.** Four defects injected ONE AT A TIME, each proved to have
relinked by hashing all three test binaries, each reverted. Baselines (SHA-256,
first 16): directed `0BE8EA41D1872A37`, random `5EED5B7CFF625E03`, composition
`B6A0AA0DD349BAD2`.

| mutation | directed | random | composition |
|---|---|---|---|
| the divide TRUNCATES instead of rounding half-up | 100 red | 4,749 red | green |
| view 0's rung is driven by `starved0 \| starved1` (the fairness leak) | 168 red | 33,283 red | **57/72 red** |
| recovery is immediate — no anti-thrash hold | 1,350 red | 49,308 red | green |
| the quotient clamp is dropped | **3 red** | 2,422 red | green |

Mutated hashes, in the same order — directed / random / composition:
`19CAD5986DA02984` `31EA9B3D49ADF87A` `421573304AD592B3`;
`4400736926999384` `01EB709AEF7B31F6` `3D626933B13B30DF`;
`E1ACBCC29C3EF327` `E9FAF9FDB205AF0A` `3AA0D4D50A028631`;
`E19F2833E2849C55` `1F4D19D1053158D8` `04C0C3969B5D52DC`.
After the revert: `F7E0D89161364A67` `6FAE842D0CD51CAE` `46EF8AFBFC482B0A`, all
green.

**The second row is the whole argument for the composition test.** A governor
with a shared rung is internally consistent and its oracle can be made to agree
with it. What it destroys is a promise to a *player*, and only a test that runs
the real TERRAIN.LOD on a real patch can say so in the only language that
matters: *view 0's ground lost triangles on a frame where view 0 asked for
nothing.*

**The fourth row is the argument for CONSTRUCTING boundaries.** Dropping the
clamp is visible only when the true quotient exceeds 16 bits. The directed lane
sees exactly **3** such checks — because it builds them by hand. It would take
an enormous amount of uniform-random luck to produce a `proj`/`px_err` pair that
overflows the port.

**THE REVERT TRAP FIRED, AND IS RECORDED.** Restoring the pristine RTL with a
file copy left the restored file's mtime OLDER than the build outputs, so
`ninja` did no work, the binaries on disk were still the mutated ones, and the
"reverted" run reported the mutation's failures. It was caught by the hashes
being IDENTICAL to the mutated build — which is exactly why this project hashes
the binary rather than trusting the build. The fix was a real content rewrite
(mtime naturally becomes now), not an mtime touch, and the relink is proved by
the three new hashes above.

**Maturity stays SPECIFIED.** The RTL exists, lints clean, agrees with its
oracle over 134,000 checks, and its central promise is demonstrated on emitted
geometry through the landed consumer — but it is simulated, not synthesized and
not on hardware, and `proj_scale` has no command to fill it yet.
