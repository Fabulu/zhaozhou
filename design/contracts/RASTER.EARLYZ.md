# Contract — RASTER.EARLYZ (Early-Z reject)

> Ledger: `design/blocks.yml` · owner ZH-059 · phase 5 · maturity SPECIFIED

## Purpose and exclusions

Conservative early-Z rejection plus the coarse transparent-depth bins: kill occluded fragments **before** they are shaded, and publish the per-tile depth-bin occupancy the charter §8 pass-6 "coarse-depth-binned translucent geometry" needs. RTL: `fpga/rtl/raster/zhao_raster_earlyz.sv`.

Exclusions — none of these are in this block: the exact per-pixel depth test and any per-pixel depth state (RASTER.FRAGMENT owns those, together with the tile-store port they need); tile memory of any kind — this block touches **no memory at all**; the stencil test, the alpha test, shading and blending (RASTER.FRAGMENT); coverage (RASTER.EDGEWALK); bin **ordering** or tile scheduling (it publishes occupancy, the scheduler decides order); and any feedback path from RASTER.FRAGMENT — the floor is derived from what passes through here, never from what the tile store later contains.

### Why this is a block and not part of RASTER.FRAGMENT

The ledger says "Kept separate from RASTER.FRAGMENT by architect ruling (1.D)". The argument, so the ruling is defensible rather than merely cited:

The two blocks answer different questions with different resources. RASTER.FRAGMENT's depth test is **exact and per-pixel**; to make it, the block must read the tile store — it owns `tile_read` port A and performs a read-modify-write — so its answer is only available *after* a memory access has been spent and after the fragment's texel has been fetched, because it is the same pipeline stage that blends. This block's decision is **conservative and per-tile**: `inputs: [covered_fragments]` is the entire input list, there is no tile-store port anywhere in the file, and its state is a few hundred flops. That is exactly what lets it answer in `fixed:1` *before* the texture sample and the tile-store read.

Folding this into RASTER.FRAGMENT would make the exact test the only rejection in the machine, which happens after the bandwidth early-Z exists to save has already been paid. Keeping them separate also keeps the latency classes honest: `fixed:1` here (no memory), `variable` there (a memory round trip and a not-yet-built texture path). Two blocks, two contracts, two resource ownerships.

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release, in the style of every other block in this tree (`always_ff @(posedge clk or negedge rst_n)`). On reset: `floor_r` is 0 (the depth clear value), the accumulator and the bin mask are empty, the output stage is invalid, `z_reject_o` is low and both counters are 0. The directed suite pins the reset floor and bin mask.

## The conservatism invariant (normative — this is the block's whole correctness argument)

`z_floor_o` is a **lower bound on the stored depth of every one of the tile's 256 pixels**. Under `spec/qformats.md` §8's late test — `pass ⟺ d_new > d_old`, strict, ties fail — a fragment with `depth ≤ floor` therefore fails at *every* pixel of the tile, so rejecting it is exactly what the late test would have done and no pixel is ever lost.

The block **may be pessimistic** (keep a fragment the late test will kill). It **may never be optimistic**.

The invariant is established and preserved by two facts and one accumulator:

1. At `tile_begin` every pixel holds the tile's clear depth, so `floor := clear_depth` is exact.
2. A depth **write** only ever raises a pixel's depth, because the late test admits a write only when `d_new > d_old`. So min-over-pixels is non-decreasing, and a bound established once stays a bound forever. `z_force_far` writes the far constant 0, which is ≤ every depth, so it is folded in at its **written** value, not at the fragment's interpolated one.
3. **Raising** the bound needs evidence that *every* pixel was covered. `acc_mask_r` (256 bits) marks the pixels hit by fragments certain to write depth, and `acc_min_r` is the smallest depth among them. When the mask is all ones, every pixel has taken at least one depth write of at least `acc_min`, so `floor := max(floor, acc_min)` is sound; the accumulator then restarts. The case this is built for is the common one — the charter's pass-1 prefill and any full-tile opaque surface raise the floor for everything behind them in one sweep.

"Certain to write depth" is deliberately narrow: `blend == REPLACE` (opaque), depth writes enabled, no alpha test, stencil function ALWAYS, and the fragment survived this block's own reject. Anything that *could* be killed downstream (a masked star disc, a stencilled decal) contributes nothing, and anything blended contributes nothing either — every additive recipe in `spec/sky_and_beams.md` §2 and `spec/stars_and_flares.md` §1 is "Z-write OFF" by construction, so it is no evidence about depth.

## Input and output packet layouts

| channel | fields | meaning |
|---|---|---|
| tile_begin (`tile_begin_i`) | `tile_clear_depth_i` 24 | this tile now holds the clear word everywhere; reset the floor to its depth |
| covered_fragments (`frag_valid_i` / `frag_ready_o`) | `frag_addr_i` 8, `frag_depth_i` 24, `frag_state_i` 32, `frag_src_id_i` 16, `frag_payload_i` `PAYLOAD_W` | one covered fragment |
| shaded_candidates (`cand_valid_o` / `cand_ready_i`) | `cand_addr_o` 8, `cand_depth_o` 24, `cand_state_o` 32, `cand_src_id_o` 16, `cand_payload_o`, `cand_bin_o` 3 | a survivor, one cycle after acceptance |
| z_reject (`z_reject_o`) | `z_reject_addr_o` 8 | a one-cycle **pulse**, not a channel — see below |
| status | `bin_mask_o` 8, `z_floor_o` 24 | per-tile bin occupancy; the conservative floor |
| counters | `early_z_rejects_o` 32, `covered_fragments_o` 32 | saturating |

`frag_addr_i` is `{row[3:0], col[3:0]}`, row 0 = top, col 0 = left — RASTER.TILESTORE's address. `frag_depth_i` is `invw24` (`spec/qformats.md` §8): larger is closer, clear value 0.

**`z_reject` is an event, not a channel.** A rejected fragment produces no downstream beat at all — that is the entire point of the block — so it is a one-cycle pulse with no handshake, and `early_z_rejects_o` is its durable form. A ready/valid reject channel would reintroduce exactly the downstream cost early-Z exists to remove.

**The payload is opaque.** This block decodes exactly three things: the address, the depth, and six bits of `frag_state_i`. Everything else the fragment carries (colour, alpha, tag, texel, stencil reference) rides through `frag_payload_i` → `cand_payload_o` untouched and unexamined, the same way RASTER.TILESTORE stores 64 bits it never decodes. That keeps the shading packet's layout owned by exactly one block (RASTER.FRAGMENT) and keeps this contract to three fields. `PAYLOAD_W` is a parameter (default 88) so this block never has to be edited when that packet grows.

### The six state bits this block decodes

The fragment state word's layout is defined by `design/contracts/RASTER.FRAGMENT.md` and `fpga/rtl/raster/zhao_raster_fragment.sv`. This block reads `[0] Z_TEST_EN`, `[1] Z_WRITE_DIS`, `[2] Z_FORCE_FAR`, `[4:3] BLEND`, `[7] ATEST_EN` and `[17:16] STEN_FUNC`, and nothing else.

### The coarse transparent-depth bins

Eight bins, `bin = depth[23:21]` — the top three bits of the `invw24` depth, so bin 7 is nearest and bin 0 farthest, the same sense as the depth. Each **surviving** fragment carries its bin out beside it; `bin_mask_o` accumulates which bins the tile has seen since `tile_begin`. A **rejected** fragment does not mark its bin: it contributes nothing to the translucent pass, so the scheduler must not be told its bin is occupied. That is the occupancy the charter §8 pass-6 binning and `spec/sky_and_beams.md` §1's deterministic sun-before-cloud sub-order need in order to walk bins back-to-front without sorting anything. The bins are derived, not stored per pixel: this block owns no per-pixel memory, so a bin is a classification of the fragment in flight and the mask is 8 flops.

## Backpressure rules

`ready_valid` on the fragment and candidate channels. `frag_ready_o = !cand_valid_o || cand_ready_i` — a function of the **output** channel's ready, which is the permitted direction; no valid anywhere is a function of its own channel's ready.

A rejected fragment leaves the output stage empty, so a stream of pure rejects still retires one per clock — the ledger's "1 reject decision per clock", measured at **256 rejects in 257 cycles** by the directed suite. A survivor stalled by `cand_ready_i` blocks the next acceptance, which is what keeps `fixed:1` true with respect to acceptance.

`tile_begin_i` is accepted **unconditionally**, exactly like RASTER.TILESTORE's `clear`, and like that clear it must not be issued with work in flight: it is the caller's contract that a tile begins when the previous tile's fragments have drained. `zhao_raster_tile_pipe` honours it by construction (the clear happens in `RS_CLEAR`, before any coverage beat). In-cycle ordering: `tile_begin` is applied **last**, after that cycle's fragment, so a fragment accepted in the same cycle uses the pre-begin floor. The output stage is deliberately *not* cleared by `tile_begin` — a candidate already decided belongs to the tile it was decided in and must still be delivered.

## Memory ownership

**None.** No RAM, no VRAM, no arena, no external memory, no DMA, no tile-store port. The entire state is `floor_r` (24 flops), `acc_mask_r` (256), `acc_min_r` (24), `bin_mask_r` (8), the output register (~150 with the default payload) and two 32-bit counters. That is the property that makes the block's `fixed:1` possible and the property that separates it from RASTER.FRAGMENT.

## Q formats and rounding

`invw24` (`spec/qformats.md` §2/§8): `U 0.0.24`, saturate `0xFFFFFF`, **larger is closer**, clear value 0. There is no arithmetic on it beyond a comparison, a minimum and a three-bit slice, so **there is no rounding anywhere in this block** and no format question to get wrong. The only other arithmetic is the two saturating counters.

The comparison is `depth ≤ floor` — `≤` and not `<`, because §8's late test is strict (`d_new > d_old`, ties fail): a fragment exactly at the floor loses at every pixel too. One LSB is the whole margin `spec/stars_and_flares.md` §3 relies on when it puts `STAR_DEPTH` at "sky-prefill far + 1", and both sides of it are pinned by directed cases.

## Latency (fixed or variable)

**`fixed:1`**, exactly as the ledger declares: a fragment accepted in cycle *N* presents its decision in cycle *N+1* — either `z_reject_o` pulses, or `cand_valid_o` rises. The driver **checks** this rather than tolerating it: every cycle after an acceptance it asserts that a decision appeared.

## Target throughput

One decision per clock — the ledger's "1 reject decision per clock" — sustained when the downstream is ready, and sustained for rejects unconditionally (a reject never occupies the output stage). Measured: 256 consecutive rejects in 257 cycles.

## Overflow and malformed-input behaviour

- **Every address is legal.** The address is 8 bits and the tile is exactly 256 words.
- **Every depth is legal.** 24 bits, and every value is a meaningful `invw24`.
- **Every state word is legal.** The 32-bit encoding has no reserved holes, so there is no malformed state to reject and the random lane filters nothing.
- **A `tile_begin` mid-stream** is legal and defined (it wins over the same cycle's fragment and does not clear the output stage); issuing one with fragments in flight is a caller error whose effect is well-defined but not useful, and the composition never does it.
- **The floor cannot be corrupted by input.** It is only ever assigned the clear depth or raised to a `max`; there is no input that makes it exceed a stored depth, which is the invariant above.
- **Both counters saturate** at `0xFFFF_FFFF` per `spec/counters.md` §4 — a counter never wraps.
- There is no input that can make the block hang: there is no state machine, only a registered decision.

## Counters and traces

`early_z_rejects_o` counts rejections; `covered_fragments_o` counts **every accepted fragment**, kept or killed. Both saturate at 32 bits.

The catalog id and the `frame_tick` shadow latch (`spec/counters.md` §3/§5) are **not** implemented here, exactly as RASTER.EDGEWALK and RASTER.TILESTORE deferred theirs. `covered_fragments` is claimed by three blocks in `design/blocks.yml` — RASTER.EDGEWALK, this block and RASTER.FRAGMENT — and reconciling multiple producers of one catalog entry is a `spec/counters.md` amendment that belongs with the DEBUG.COUNTERS integration wave, not with this block. What ships is the local, saturating, differentially-verified counter.

## Scalar reference function

`zref::EarlyZ` (`reference/include/zref/zref_earlyz.hpp`, `reference/src/zrender/earlyz.cpp`).

Like `zref::TileStore` and unlike `zref::EdgeWalk`, this **is** a second implementation — nothing earlier in this repository maintains a hierarchical-Z floor — so it is written in the plainest way that can be checked by eye: one bound, one 256-entry bitset with a popcount, one running minimum. The RTL's registered output stage, its backpressure and its saturating counters have no counterpart in it, which is exactly what makes "RTL == oracle" test those mechanics.

It shares exactly one thing with the RTL: the fragment state word's bit positions, which it reads through `zref::FragmentPipeline::State::unpack` rather than re-deriving.

## Directed tests

`tests/raster/raster_earlyz_directed.cpp` (driver `tests/raster/raster_earlyz_dev.hpp`) — **63 checks**, every one of them stepped through the RTL and `zref::EarlyZ` with every decision, every carried field, both counters, the bin mask and the floor compared.

Cases: reset and `tile_begin` (the floor becomes the clear depth exactly, and a second tile resets it **downward** as well as upward — a floor that only rose would reject the next tile's far geometry wholesale); the depth test **off**, where `sky_backdrop` survives the deepest possible floor while a test-enabled fragment at the same depth dies; **strictness**, with `floor−1`, `floor` and `floor+1` all pinned and the sky/star pairing (a full `sky_backdrop` sweep followed by `STAR_DEPTH = far+1`, which must win, and a star at the sky's own depth, which must not); the **floor rise**, where 255 of 256 covered pixels move it not at all, a fragment behind those 255 is still kept, the 256th completes the cover, and the floor rises to the **smallest** qualifying depth rather than the first, last or mean; **qualification**, where four separate disqualifiers each get a full-tile sweep that must move nothing plus a control sweep that does; `z_force_far`, which contributes the **written** far constant and not the carried depth; the eight coarse bins, the per-tile mask, and a rejected fragment not occupying its bin; the 88-bit opaque payload at both rails and a walking pattern; six backpressure patterns that must agree with each other exactly; and the counters.

## Randomized differential tests

`tests/raster/raster_earlyz_random.cpp` — deterministic from two fixed seeds (the PCG shape of the other random lanes).

**Lane A** — free traffic with random addresses, random **state words** and depths drawn from a deliberately narrow window around the tile's floor, so `depth == floor` (the tie that must fail) and `depth == floor + 1` (the one-LSB margin that must survive) are common events rather than 1-in-16-million accidents.

**Lane B** — the prefill duty cycle: sweep an opaque depth-writing surface across every pixel in a PCG-permuted order (so the completing pixel is not always address 255), watch the floor rise, then draw geometry behind and in front of it. Some batches deliberately leave one pixel out, and some interleave a full sweep of **non-qualifying** fragments that must move nothing.

The lane asserts its own coverage and **fails if any bucket is empty**: at the fast setting it fires **2,195 ties at the floor, 2,166 one LSB above, 3,701 rejects against 37,817 keeps, 49 floor rises and 29 blocked full covers**. Default 400 + 60 batches (CTest `fast`); `--nightly` 6,000 + 900. Failing vectors are serialized per charter §29-17.

## Mutation evidence (2026-08-18)

Two deliberate RTL defects were injected one at a time, each with the built `.exe` hash asserted to have changed before the lanes were run:

| mutation | earlyz directed | earlyz random | tile_pipe directed | tile_pipe random |
|---|---|---|---|---|
| rejects a fragment it should KEEP (`≤ floor` widened to `≤ floor+1`) | RED | RED | RED | RED |
| passes a fragment it should KILL (the strict tie let through) | RED | RED | RED | RED |

Both composed lanes were **green** on both mutations before this increment hardened them, and each hole was fixed rather than noted:

- the composed lanes drove `state == 0` for every job (depth test off), and their depths were drawn uniformly over 24 bits, which makes the one-LSB boundary a 1-in-16-million event. The random lane now draws half its jobs at a random legal recipe and half its depths from a window around the tile clear depth (124 ties and 131 one-above in the fast run, both asserted non-zero); the directed lane straddles the boundary explicitly at −1 / 0 / +1.
- the composed picture cannot see early-Z **pessimism** at all — a reject that stops happening leaves RASTER.FRAGMENT to kill the fragment exactly, and the tile is identical, *by design*. What the block may not do is disagree with its contract about how many it rejected, since `early_z_rejects` is a budgeted counter. Both composed lanes now diff `early_z_rejects` against the composed oracle's own prediction on every case.

## Formal properties

**None on this block, and that is a statement rather than an omission.** The interesting content here is the *invariant* — "the floor is a lower bound on 256 stored depths" — and that is a statement about state this block does not hold: the depths live in RASTER.TILESTORE, which this block cannot see. Proving it would mean modelling the tile store and RASTER.FRAGMENT's write rule inside the harness, at which point the proof is about the model and not about these bytes. The decision itself is a single 24-bit comparison against a register; a bounded model check of it would restate the RTL rather than prove anything.

What *is* proved formally in this subsystem is the arithmetic core that has real content: `tests/formal/raster_fragment_blend.sby` on `zhao_raster_blend`. Per the charter's own standard, a proof with nothing to cover is worse than no proof.

## Synthesis / resource ceiling

Budget group `tile`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers, and it is deliberately not in `fpga/files.qip`. The shape is ~460 flops: `acc_mask_r` 256, `floor_r` 24, `acc_min_r` 24, `bin_mask_r` 8, two 32-bit counters, and the output register (~150 at the default `PAYLOAD_W` of 88). No RAM and no DSP.

Two things to watch at fit time, neither measured: `acc_mask_r` has a 256-fanout clear and a 256-input AND (`&acc_mask_next`) in the same cycle as the address decode that sets one of its bits, which is the block's longest combinational path; and the output register is as wide as the shading packet, so it grows with `PAYLOAD_W`. If either becomes the critical path the honest fix is a registered popcount rather than a combinational all-ones test — the accumulator's timing is not load-bearing for correctness, only for the cycle it completes in.

## Integration capture cases

None yet on hardware. **Composed, in simulation only (2026-08-18):** `fpga/rtl/raster/zhao_raster_tile_pipe.sv` instantiates this block between RASTER.EDGEWALK's coverage expansion and RASTER.FRAGMENT, with `tile_begin` riding the same cycle as RASTER.TILESTORE's clear and the clear word's depth field supplying the initial floor. `tests/raster/raster_tile_pipe_directed.cpp` and `..._random.cpp` diff the whole chain against five composed oracles. It is Verilator-only — not in `fpga/files.qip`, no Quartus fit, no capture, never on hardware. Simulated is not synthesized and neither is on-hardware.

## Notes

Kept separate from RASTER.FRAGMENT by architect ruling (1.D); the argument is under Purpose above rather than only the citation.

Deliberately not built in this block, so the next wave knows: no per-pixel depth state and no Z-pyramid deeper than one tile-wide bound; no feedback port from RASTER.FRAGMENT or RASTER.TILESTORE (the floor is derived only from what passes through); no bin ordering, sorting or scheduling; no stencil or alpha pre-test; no `frame_tick` shadow latch or catalog-id binding; no formal property (reasoned above).
