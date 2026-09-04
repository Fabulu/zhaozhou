# Contract — RASTER.FRAGMENT (Fragment pipeline)

> Ledger: `design/blocks.yml` · owner ZH-025 · phase 4 · maturity SPECIFIED

## Purpose and exclusions

Shade one covered fragment and perform the depth / stencil / blend work — the charter §4 exemplar block. RTL: `fpga/rtl/raster/zhao_raster_fragment.sv` (+ `zhao_raster_blend.sv`, one instance per colour channel).

Exclusions — none of these are in this block: coverage (RASTER.EDGEWALK); early-Z (RASTER.EARLYZ, by architect ruling 1.D); attribute interpolation — colour, alpha, depth and UV all arrive **already interpolated**, which is GEOM.SETUP's job; texture sampling of any kind (see *What is not built* below); ordered dither and the framebuffer write (RASTER.RESOLVE); tile memory (RASTER.TILESTORE — this block is a *master* on its ports, not an owner); tile scheduling; MSAA; and **fog** (see below — that is the spec's decision, not an omission).

### Fog is not computed here — **AND OWNER RULING D-5 REVERSES THIS**

> **STOP. This section describes the pre-D-5 law and the block still implements
> it. Owner ruling D-5 (2026-09-03, `reports/OWNER-RULINGS-20260903-FUNDAMENTALS.md`)
> replaced the fog route:**
>
> * The colour arriving here will be **unfogged lit RGB**, not "already fogged".
> * A **fog factor** arrives as a separate interpolant, transported through
>   `GEOM.PARAMBUF` and interpolated through `ATTRSTEP`.
> * Fog is applied to the **final source colour after texture/material
>   combination and before alpha or additive blending** — D-5's step 5, ahead of
>   the framebuffer blend at step 6. **That is per-fragment work, in this block's
>   cone.**
> * Fog-exempt classes take an **explicit bypass**, replacing the "honoured by
>   construction" argument below — a block that *can* fog must be *told* not to.
>
> **A GREEN TEST CURRENTLY DEFENDS THE SUPERSEDED LAW.**
> `tests/raster/raster_fragment_directed.cpp:test_fog_is_a_vertex_operation`
> requires the colour to reach the tile **unaltered**, and its own comment says
> *"a block that grew a fog stage would double-apply it and that case would go
> red."* Under D-5 that block **must** grow a fog stage, so **that test is
> expected to go red, and the correct response is to rewrite the test, not to
> revert the ruling.** It is the only test in the tree known to enforce a
> decision that has been overruled.
>
> **Not implemented in this pass.** D-5 costs a per-fragment mix that the
> contract below correctly records as "not costed"; costing and building it is
> G4/G6 work. What is fixed here is that the contract no longer presents the
> superseded route as current law.

The text below is retained as the record of what changed.


`design/blocks.yml`'s purpose line for this block reads "depth/stencil/blend/**fog**", and that line predates the ratified fog law. `spec/qformats.md` §8, added 2026-08-17, freezes fog as a **per-vertex** operation in GEOM.PROJECT and says so in as many words:

> "fog is a vertex-colour operation in GEOM.PROJECT, ordered AFTER lighting and AFTER the global tint … The fogged colour rides the ordinary Gouraud path — the factor is not a separate interpolant and there is no per-fragment fog anywhere in v1 (a per-pixel form would be a RASTER.FRAGMENT recipe change: **not costed, not built**)."

So the colour arriving on `frag_vert_rgb_i` is *already fogged*, this block applies no fog of its own, and building a per-fragment fog stage here would contradict a ratified spec. The §8 exempt list (the sky family, additive emissive, the HUD planes) is honoured **by construction**: a block that cannot fog cannot fog the wrong thing.

This is pinned rather than merely asserted: `tests/raster/raster_fragment_directed.cpp:test_fog_is_a_vertex_operation` computes the frozen §8 vertex mix itself across a factor sweep and requires the result to reach the tile *unaltered*. A block that grew a fog stage would double-apply it and that case would go red.

The charter §4 exemplar's own purpose line, incidentally, says "depth/stencil/blend" with no fog — the ledger's is the outlier. Correcting a ledger field is a validator-gated edit and not this increment's call.

### What is not built, named rather than faked

**TEXTURE.TMU does not exist.** Four of the six ratified recipes name a sampler — `beam_additive_fade` says "bilinear TMU mandatory", the star recipes say "CLUT8 nearest+mips" and "nearest mandatory — bilinear must never touch a palette". None of that is in this block and none of it is imitated: there is no sampler, no filter, no palette lookup, no mip selection, no wrap/clamp/mirror mode. What the block does instead is take the **sampled texel** as three fields of the fragment packet — `frag_texel_rgb_i`, `frag_texel_a_i`, `frag_texel_idx_i` — and consume them exactly as the recipes say to consume a sampled texel: modulate, alpha-weight, index-test, and take the tag strength from the CLUT intensity. That is the clean interface TEXTURE.TMU fills in when it lands. The ledger already has this block requesting from TEXTURE.TMU downstream; wiring that request channel is that block's increment. Nothing here changes when it does — the texel arrives from a port instead of from a driver.

**PART.SOFT does not exist either, and needs no port.** The ledger lists `soft_particles` among this block's inputs. A soft particle's contribution is a depth-proximity **fade**, i.e. an alpha modulation, and that arrives on `frag_vert_a_i` like every other alpha, with PART.SOFT computing the factor. No port is invented for it: adding one would be inventing a second alpha lane for a block that already has the right one.

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release (`always_ff @(posedge clk or negedge rst_n)`). On reset both pipeline stages are invalid, `idle_o` is high, `wr_valid_o` and `rd_valid_o` are low, `fragment_error_o` is low and both counters are 0. The block holds no tile state, so there is nothing else to clear.

## The fragment state word (32 bits, layout defined HERE and in the RTL header)

Every field is encoded so that **`state == 0` is the plain opaque write**: depth test off, depth written, blend REPLACE, no alpha test, stencil ALWAYS + REPLACE, tag written from the packet. That is not cosmetic — it is what lets `zhao_raster_tile_pipe`'s phase-4 flat-colour behaviour survive this block's arrival bit for bit, with the whole pre-existing directed and random suite unchanged. Hence the two `_DIS` (disable) bits: an enable bit would have made the zero state a no-op instead of a write.

| bits | field | meaning |
|---|---|---|
| `[0]` | `Z_TEST_EN` | 1 = the §8 strict test; 0 = always pass |
| `[1]` | `Z_WRITE_DIS` | 1 = keep the destination depth |
| `[2]` | `Z_FORCE_FAR` | 1 = the depth **written** is the far constant 0 |
| `[4:3]` | `BLEND` | 0 REPLACE, 1 ALPHA, 2 ADD, 3 ADD_MOD |
| `[5]` | `SHADE_MOD` | 1 = `src.rgb = unit_mul(texel.rgb, vertex.rgb)` |
| `[6]` | `ALPHA_MOD` | 1 = `a = unit_mul(texel.a, vertex.a)` |
| `[7]` | `ATEST_EN` | 1 = kill ⟺ `texel_index == ATEST_REF` |
| `[15:8]` | `ATEST_REF` | the sentinel index |
| `[17:16]` | `STEN_FUNC` | 0 ALWAYS, 1 EQUAL, 2 NOTEQUAL, 3 NEVER |
| `[19:18]` | `STEN_OP` | 0 REPLACE, 1 KEEP, 2 INCR_SAT, 3 DECR_SAT |
| `[20]` | `TAG_WRITE_DIS` | 1 = keep the destination tag |
| `[21]` | `TAG_FROM_TEXEL` | 1 = `tag = {TAG_CHANNEL, texel_index[5:0]}` |
| `[23:22]` | `TAG_CHANNEL` | the `stars_and_flares` §1 effect channel; GLOW = `2'b01` |
| `[31:24]` | `STEN_MASK` | masks EQUAL / NOTEQUAL only |

**There are no reserved holes**, deliberately: every 32-bit value is a legal state, so the randomized lane filters nothing and there is no malformed-state path to specify. The layout is mirrored bit for bit by `zref::FragmentPipeline::State`.

### The six ratified recipes

Every recipe is **one value of this word**; none needed a datapath mode of its own, which is the whole argument that this encoding is the right one. Each is a named constructor on the oracle (`zref::FragmentPipeline::sky_backdrop()` …), so tests name recipes rather than magic constants.

| recipe | source | state |
|---|---|---|
| `sky_backdrop` | `sky_and_beams` §1.1 pass 1 — "Z-test off, Z-write far, blend off, effect-tag init" | `Z_FORCE_FAR`, blend REPLACE, tag from the packet, stencil KEEP |
| `sky_cloud_fade` | §1.1 layer `sky_` — "Z-test on, Z-write off, alpha blend `out = dst·(1−a)+src·a`, `a = tex.a × vertex.a`" | `Z_TEST_EN`, `Z_WRITE_DIS`, blend ALPHA, `ALPHA_MOD`, `TAG_WRITE_DIS` |
| `sun_additive` | §1.1 layer `sun_` — "`dst = sat(dst + src·tex.a)`, glow effect-tag write on" | `Z_TEST_EN`, `Z_WRITE_DIS`, blend ADD_MOD, `TAG_CHANNEL = GLOW` |
| `beam_additive_fade` | §2 — "`colour = tex.RGB × vertex.RGB; dst = sat(dst + src)`" | `Z_TEST_EN`, `Z_WRITE_DIS`, blend ADD, `SHADE_MOD`, `TAG_WRITE_DIS` |
| `star_disc_masked` | `stars_and_flares` §1 — "CLUT8 nearest, alpha-test index 0, Z-test on / Z-write off, glow-tag write with strength = the texel's CLUT intensity" | `Z_TEST_EN`, `Z_WRITE_DIS`, blend REPLACE, `ATEST_EN` with `ATEST_REF = 0`, `TAG_FROM_TEXEL`, `TAG_CHANNEL = GLOW` |
| `star_halo_additive` | §1 — "same sampling, `dst = sat(dst+src)`" | as above with blend ADD and no alpha test (§4: `pal_h[0]` is black, the additive identity, so the halo needs no mask) |

`sun_additive`'s tag is **constant**, not from a texel index, and that is a real distinction rather than an oversight: the sun quad's texture is "64×64 ARGB4444" (§1.1) — direct colour, with no CLUT index to read a strength from. Only the star recipes sample CLUT8, and only they can honour §1's "strength = source texel's CLUT intensity".

## Input and output packet layouts

| channel | fields | meaning |
|---|---|---|
| shaded_candidates (`frag_valid_i` / `frag_ready_o`) | `frag_addr_i` 8, `frag_depth_i` 24, `frag_state_i` 32, `frag_src_id_i` 16, `frag_vert_rgb_i` 24, `frag_vert_a_i` 8, `frag_tag_i` 8, `frag_sten_ref_i` 8, `frag_texel_rgb_i` 24, `frag_texel_a_i` 8, `frag_texel_idx_i` 8 | one candidate from RASTER.EARLYZ |
| tile_read master (`rd_valid_o` / `rd_ready_i`) | `rd_addr_o` 8, `rd_src_id_o` 16 | RASTER.TILESTORE port A request |
| tile_read response | `rd_valid_i`, `rd_data_i` 64 | that port's fixed 1-cycle response |
| tile_write master (`wr_valid_o` / `wr_ready_i`) | `wr_addr_o` 8, `wr_data_o` 64 | the surviving fragment's word |
| fragment_error (`fragment_error_o`) | — | one-cycle pulse; see below |
| status | `idle_o` | nothing anywhere in the pipe |
| counters | `covered_fragments_o` 32, `blended_fragments_o` 32 | saturating |

`frag_vert_rgb_i` is the interpolated, lit, tinted and **already-fogged** vertex colour. `frag_vert_a_i` is a `unit8` and is also where PART.SOFT's depth fade would arrive. The tile word is RASTER.TILESTORE's, charter §8 order: `[63:40]` RGB, `[39:32]` effect tag, `[31:8]` depth, `[7:0]` stencil.

## The pipeline, and the hazard that is not here

Two stages over a read-modify-write on RASTER.TILESTORE port A:

- cycle *N* — a candidate is accepted; it lands in stage 0.
- cycle *N+1* — stage 0 issues the tile-store **read**.
- cycle *N+2* — the store presents the destination word (its `latency: fixed:1`). Stage 1 runs the whole test-and-blend chain combinationally and issues the tile-store **write** in the same cycle.

The obvious hazard — two fragments one cycle apart at the *same* pixel, the second reading before the first writes — cannot happen, and not by luck. The second fragment's read is issued in cycle *N+2*, the exact cycle the first fragment's write is issued, and RASTER.TILESTORE's ordering rule 3 is write-first: *"a read returns NEW data for a same-cycle same-address write … because a read-modify-write fragment pipeline must never see the pixel it just wrote as stale."* This block **is** that pipeline, and that rule was written for exactly this cycle. So back-to-back fragments at one pixel are correct with no forwarding path, no scoreboard and no stall — which is what makes "1 accepted fast-path fragment per clock" reachable at all.
ENFORCED-BY: `tests/raster/raster_fragment_directed.cpp:test_same_pixel_raw` (16 back-to-back additive fragments at one address must accumulate to 160, not to 10) and `raster_fragment_random.cpp` lane B.

### The stall that re-issues its own read

Stage 1 stands on `rd_data_i` **combinationally** — that is what collapses read, test, blend and write into one cycle and buys the immunity above. But `rd_data_i` is not a value that persists: RASTER.TILESTORE re-registers its RAM word every cycle from whatever address the port is presented with, so a stage 1 that simply froze would be standing on the *next* cycle's read.

So the stall re-issues the read it is waiting on. While stage 1 cannot retire (the store refuses the write — its `wr_ready_o = !clear_valid_i`), the read port is pointed back at stage 1's own address instead of stage 0's. Nothing has been written yet, so every re-read returns the identical destination word and stage 1 stands on a fresh response every cycle of the stall; stage 0 does not advance meanwhile, so no read is lost. The two uses of the port are mutually exclusive by construction.
ENFORCED-BY: `tests/raster/raster_fragment_directed.cpp:test_write_stall` (six stall densities, identical tile bit for bit).

## Backpressure rules

`ready_valid` on the candidate, read and write channels. `wr_valid_o` is a function of registers and `rd_data_i` only — **never** of `wr_ready_i`. `frag_ready_o` and `rd_valid_o` do depend on `wr_ready_i` and `rd_ready_i`, which are *other* channels' readies and therefore the permitted direction; RASTER.TILESTORE closes no loop back into this block (its read ready is a constant 1 and its write ready is a function of its own clear input only).

A killed fragment retires without a write and does not stall.

## Memory ownership

**None.** This block is a master on RASTER.TILESTORE's ports and owns no memory: no RAM, no VRAM, no arena, no DMA. Its whole state is two pipeline registers (~110 bits each) and two counters. That the tile store is a separate block with its own contract is what makes the write-first rule above a *citable law* rather than an internal assumption.

## Q formats and rounding

**Depth** — `invw24` (`spec/qformats.md` §2/§8): `U 0.0.24`, larger is closer, clear value 0. The test is §8's, quoted: *"pass ⟺ `d_new > d_old` (strict; ties fail; decals use explicit bias)"*. That is the **only** depth comparison the spec defines, and the recipes add exactly one more state — `sky_backdrop`'s "Z-test off". There is no LESS, EQUAL, LEQUAL or NEVER here, because no spec in this repository defines one and a comparison enum invented in RTL would be law made in the wrong place. `Z_TEST_EN` is therefore a bit and not a function code; if a future spec ratifies more comparisons it widens here and this section records the amendment.

The strictness is load-bearing and easy to get wrong in the safe-looking direction. `>=` would let a coplanar decal z-fight instead of losing, which is precisely why §8 says "decals use explicit bias" rather than relaxing the compare; and `sky_backdrop` writes the far constant 0 while `STAR_DEPTH` is "sky-prefill far + 1" (`stars_and_flares` §3), so a star at depth 1 beats the backdrop at 0 by exactly one LSB — a `>=` would additionally let the sky overwrite itself.

**Colour and alpha** — `colour8` and `unit8` (`spec/qformats.md` §2). A `unit8`'s **value is `raw/256`, not `raw/255`**, and the single rounding primitive is `rescale_s(x, 8) = (x + 128) >>> 8`, round-half-up, ties toward +infinity, arithmetic shift (§3/§4). The four blend modes, in `zhao_raster_blend`:

```
REPLACE  out = src
ALPHA    out = sat_u8(dst + rescale_s((src − dst)·a, 8))     ≡ dst·(1−a) + src·a
ADD      out = sat_u8(dst + src)
ADD_MOD  out = sat_u8(dst + rescale_u(src·a, 8))             ≡ dst + unit_mul(src, a)
```

ALPHA is the *same shape* as the two lerps ratified before it — the fog mix (`c' = sat_u8(c + rescale_s((fog_c − c)·f8, 8))`, §8) and the global-tint mix (`sky_and_beams` §4a) — so nothing new is invented; the blend is that frozen form with the source colour in place of the fog/tint colour.

**Why this divides by 256 and the resolve divides by 255.** A frequent and expensive confusion, so it is written down. `zhao_raster_div255` and `zhao_raster_quant` divide by 255 because they are **quantizers**: they map an 8-bit channel whose full scale *is* 255 onto a 5- or 6-bit field whose full scale *is* 31 or 63, and 255 is the ratio of those scales. This block divides by 256 because it is a **weighting**: its factor is a `unit8`, whose value §2 defines as `raw/256`. Different lanes of the same spec; neither is a rounding of the other. A blend that used /255 would disagree with the fog and tint mixes, which are the same operation on the same `colour8` lanes.

The visible consequence, stated so no test is surprised by it and pinned by a directed case: **`a = 255` is 255/256, not 1.0.** ALPHA at `a = 255` with `dst = 0`, `src = 255` gives **254**, exactly as the ratified fog mix does at `f8 = 255`. `a = 0` is the exact identity.

The sign in the ALPHA lerp is load-bearing: `rescale_s` rounds ties toward +infinity, so on the *darkening* half an exact half rounds toward zero. Rescaling the magnitude unsigned and re-applying the sign rounds those ties the other way and differs by one LSB. That is why the RTL is written signed, and the formal lane covers an exact negative-half tie.

**The saturation is the recipe, not an overflow.** `sky_and_beams` §2 and `stars_and_flares` §1 both spell the additive recipes as `dst = sat(dst + src)`: a beam crossing a bright sky is *supposed* to blow out, and charter §26's no-OIT refusal is moot precisely because addition commutes and saturates. `fragment_error_o` therefore does **not** fire on a railed blend.

**The shade stage** is exactly two `unit_mul`s the recipes name — `beam_additive_fade`'s "`colour = tex.RGB × vertex.RGB`" and `sky_cloud_fade`'s "`a = tex.a × vertex.a`" — and `unit_mul` is §3's frozen `((u32)a·b + 128) >> 8`, called on the oracle side rather than restated.

## The alpha test is an index test

`stars_and_flares` §1 does not say "alpha < ref"; it says `star_disc_masked` is "CLUT8 nearest, **alpha-test index 0**", and §3's bake law says "Index 0 transparent; intensity 1..63". The transparency of a CLUT8 texel is a property of its **index**, before any palette lookup — testing the palette's alpha instead would sample a palette this machine deliberately never samples for transparency. So the test is `kill ⟺ texel_index == ATEST_REF`, with 0 the ratified case and the field existing so a recipe with a different sentinel needs no new RTL.

This is pinned from both sides: a texel with **index 0 and alpha 0xFF** must die, and one with **index 1 and alpha 0x00** must live. A block comparing the alpha byte fails both.

## The stencil, and the op set that is deliberately missing

Charter §8 gives the tile word an 8-bit stencil; no spec in this repository defines a stencil function set, so this contract defines a minimal one: test `{ALWAYS, EQUAL, NOTEQUAL, NEVER}` over `(dst & mask) vs (ref & mask)`, and op `{REPLACE, KEEP, INCR_SAT, DECR_SAT}` applied when the fragment **survives**.

There are **no separate stencil-fail / depth-fail ops**, and that is a decision with a reason rather than a gap. RASTER.TILESTORE has no byte enables — its contract argues that at length — so a fragment that draws no colour but must still bump the stencil would have to write the *whole* word back, i.e. spend a full write cycle to change one byte. That is a second recipe with its own cost, and none of the six ratified recipes asks for one. When something does, it is a contract amendment plus a write path, not a quiet extension of this enum. A fragment that fails **any** of the three tests writes nothing at all, stencil included, and the directed suite pins that.

## Latency (fixed or variable)

**`variable`**, as the ledger declares — but the fast path is worth stating precisely: **exactly two cycles** from acceptance to the tile write, unconditionally, when the store accepts the write. It becomes variable because the block stalls whenever the store refuses the write (`wr_ready_o = !clear_valid_i`), and because the texture path, when it exists, is variable by nature. The fast path being fixed is a property, not a contradiction of the ledger.

## Target throughput

**One accepted fast-path fragment per clock**, the ledger's figure, sustained with no forwarding path because of the write-first rule above. Measured: **120 fragments in 122 cycles** at full readiness (the two extra cycles are the pipeline fill), rising to 173 cycles under the densest write-port stall pattern.

## Overflow and malformed-input behaviour

- **Every address, depth, colour, alpha, texel and state word is legal.** The state encoding has no reserved holes; the random lane filters nothing.
- **A saturating blend is not an error.** It is the recipe (see above), and `fragment_error_o` does not fire on it.
- **`fragment_error_o`** pulses for one cycle when the tile store fails to present the response stage 1 is standing on — a **protocol** violation by the store, not an arithmetic condition. It should never fire, and every test asserts that it never does.
- **A refused write** is retried, not dropped: the block holds stage 1 and re-issues its read (above). A store that refused a write forever would stall this block forever, which is correct — it never corrupts and never drops a fragment.
- **Both counters saturate** at `0xFFFF_FFFF` per `spec/counters.md` §4 — a counter never wraps.
- There is no input that can make the block hang other than a downstream that never accepts.

## Counters and traces

`covered_fragments_o` counts **every accepted fragment**. `blended_fragments_o` counts fragments that **survived and whose write combined the source with the destination** — a REPLACE write is not a blend, and a fragment killed by any of the three tests is not one either, because it wrote nothing at all. The directed suite pins that distinction with 25 blended-state fragments that are stencil-killed and must *not* be counted.

The catalog id and the `frame_tick` shadow latch (`spec/counters.md` §3/§5) are **not** implemented here, exactly as the other RASTER blocks deferred theirs; `covered_fragments` has three producers in `design/blocks.yml` and reconciling them is a `spec/counters.md` amendment for the DEBUG.COUNTERS wave. The charter §4 exemplar also lists `texture_stalls`, which is **not** implemented and cannot be: there is no texture path to stall.

## Scalar reference function

`zref::FragmentPipeline` (`reference/include/zref/zref_fragment.hpp`, `reference/src/zrender/fragment.cpp`).

Like `zref::TileStore` and unlike `zref::EdgeWalk`, this is a second implementation of a *contract* — no earlier code here performs a depth/stencil/blend on a charter §8 tile word — so it is written in the plainest possible way, and everything it *can* delegate it does: the unit8 product is `zref::unit_mul` (§3's one frozen multiply), the tile word is `zref::TileStore::Word`, and the lerp is §4's `rescale_s(x, 8)` written once. What it does **not** share with the RTL is any structure: the RTL has two pipeline stages, a stall that re-issues its own read, and three `zhao_raster_blend` instances; the oracle has one function and a `switch`. So "RTL == oracle" tests the RTL's mechanics rather than a shared clever idea.

## Directed tests

`tests/raster/raster_fragment_directed.cpp` (driver `tests/raster/raster_fragment_dev.hpp`) — **93 checks**. Every case compares the **ordered write list**, the whole 256-word tile and both counters against the oracle. The ordered list matters as much as the tile: a block that wrote the destination back unchanged instead of skipping a killed fragment's write would leave the tile identical, and only the list would catch it.

Cases: both depth comparison modes the spec defines, with `D−1`, `D`, `D+1`, the far constant, the nearest depth and a mid depth all pinned and the tie pinned as a **failure**; depth write on, off (a *passing* fragment must leave the destination depth exactly as it found it) and `Z_FORCE_FAR`, plus the precedence of `Z_WRITE_DIS` over it; all four stencil functions under a mask (including a mask that *hides* a difference in both directions) and all four ops with both saturation rails, plus the no-fail-op case; all four blend modes against longhand anchors, the additive rail (`100+200 = 255`, `254+1 = 255`, white+white = white), the unit8 endpoint (254, not 255), the exact identity at `a = 0`, the negative-half rounding tie, and a sweep proving neither additive mode can darken; the alpha test as an **index** test from both sides plus a non-zero reference and the disabled case; both `unit_mul` shade paths with hand-computed products; the tag in all three modes including the six-bit strength clamp; the fog case above; all six recipes plus a `sky_backdrop` → `star_halo_additive` layering that must add, saturate, keep the backdrop's depth and stamp `(GLOW<<6)|63`; the same-pixel read-after-write chain; six write-stall densities; and the counters.

The tile store is **modelled** in the driver rather than instantiated, with its own documented timing (1-cycle response, write-first same-cycle bypass). That is deliberate and is the same choice `raster_resolve_dev.hpp` made: it lets this lane drive responses the real store would never produce — stalling the write port, refusing a response — which is how the stall path and `fragment_error_o` get exercised at all. The two blocks *are* wired together in `zhao_raster_tile_pipe`, and `tests/raster/raster_tile_pipe_*.cpp` is where that pairing is checked against the real store.

## Randomized differential tests

`tests/raster/raster_fragment_random.cpp` — deterministic from three fixed seeds. Three lanes, because the block has three regimes a single uniform stream would visit badly:

- **Lane A — free traffic.** Every field random including the state word, over a wide address range. Reaches odd combinations no recipe asks for (a stencilled additive with a masked texel and forced-far depth).
- **Lane B — the same pixel.** Addresses drawn from a window of 1..4, so read-after-write at one pixel is the traffic rather than a 1-in-256 accident. This is where the no-forwarding claim is actually tested.
- **Lane C — the recipes.** Only the six ratified states, which lane A would visit once in millions of draws.

Colour and alpha channels are railed often on purpose, so the saturation and the unit8 endpoints are common. The lane **asserts its own coverage and fails if any bucket is empty**: at the fast setting it fires **10,509 writes; 4,761 depth kills, 7,890 stencil kills, 1,398 alpha-test kills; 6,862 / 5,457 / 6,676 / 5,563 across the four blend modes; 3,166 additive rails; 19 depth ties; 3,232 same-pixel chains**. Default 260 + 200 + 200 batches (CTest `fast`); `--nightly` 4,000 + 3,000 + 3,000. Failing vectors are serialized per charter §29-17.

## Mutation evidence (2026-08-18)

Five deliberate RTL defects were injected one at a time, each with the built `.exe` hash asserted to have changed before the lanes were run (a previous increment in this tree got identical failure lists for three different mutations because ninja skipped re-verilation after a same-tick edit):

| mutation | fragment directed | fragment random | tile_pipe directed | tile_pipe random |
|---|---|---|---|---|
| depth comparison inverted (`>` → `<`) | RED | RED | RED | RED |
| depth written when writes are disabled | RED | RED | **green — structural, see below** | **green — structural** |
| blend operands swapped (`src−dst` → `dst−src`) | RED | RED | RED | RED |
| additive not saturating (the rail deleted) | RED | RED | RED | RED |
| alpha test comparing the wrong field (index → alpha byte) | RED | RED | RED | RED |

Three of those five were green in the composed lanes before this increment hardened them, and each hole was **fixed** rather than noted: the composed lanes drove `state == 0` for every job (so an inverted depth compare had nothing to invert), their recipe vector never railed (so a deleted saturation was invisible), and their depths were drawn uniformly over 24 bits (so the one-LSB boundary was a 1-in-16-million event). See `design/contracts/RASTER.EARLYZ.md` for the matching early-Z pair.

**Why the composed lanes cannot see "depth written when writes are disabled", stated plainly.** It is a property of the composition, not a gap in its tests. RASTER.RESOLVE does not resolve depth — charter §8, "no external full-screen depth buffer in the normal tile path", and its RTL sinks `tr_data_i[31:0]` explicitly — so a wrong depth in the tile is invisible in the resolved picture and the tile CRC. It could only be observed by a *later fragment reading it*, and `zhao_raster_tile_pipe` is one clear plus one triangle per tile, with coverage giving at most one fragment per pixel, so no fragment ever reads a depth another fragment wrote. Multi-triangle accumulation into one tile is in that block's own "what this is not" list. The two lanes the ledger requires for this block both go red, which is the bar; the composed lanes will gain the ability when the composition gains multiple primitives per tile.

## Formal properties

`tests/formal/raster_fragment_blend.sby` + `raster_fragment_blend_fv.sv`, proved on **`zhao_raster_blend`** — the module this block instantiates three times per fragment, so the proof and the silicon are the same bytes (the `zhao_raster_quant` precedent).

Seven properties, all unconditional over free inputs:

- **P1** REPLACE is exactly the source.
- **P2** ALPHA is *exactly* `dst + rescale_s((src−dst)·a, 8)` computed in a wide signed lane — this catches the magnitude-then-sign rounding error, swapped operands, a /255 divide and a missing rounding term.
- **P3** that lerp **never overshoots** either endpoint. A real theorem, not a restatement: it is false for the obvious near-misses (a rounding term of 255 overshoots at the top of the range), and it is what justifies the RTL's comment that ALPHA cannot leave `[0, 255]`.
- **P4** ADD is exactly `min(255, dst + src)` — two-sided, so it catches both a wrap and an early clamp.
- **P5** ADD_MOD is exactly `min(255, dst + unit_mul(src, a))`.
- **P6** neither additive mode can **darken**. This is what makes charter §26's no-OIT refusal moot for the beam and halo layers: addition commutes *and* is monotone, so draw order cannot lose energy.
- **P7** no mode can leave the 8-bit field.

The input parametrisation is **total, not sampled**: `dst` and `src` are `u8` because the charter §8 working colour is one byte per channel, `a` is a `u8` because it *is* a `unit8`, and the mode field is exactly two bits. Free `(mode, dst, src, a)` is all **67,108,864** inputs the block can ever be handed, and the harness is combinational, so depth 2 is the full state space rather than a bound.

**All 13 cover statements are reached**, and the cover task is load-bearing: the additive rail actually firing (without it P4's `min` and P7's bound would also hold for a blend that never reaches 255), the alpha lerp reaching both endpoints and moving in both directions (without which P3 holds for a blend that never moves), the unit8 endpoint (`a = 255` gives 254), and an exact rounding **tie on the negative half** — the case a magnitude-then-sign implementation gets wrong by one LSB.

**What is not proved**, plainly: the mode *encoding* (which recipe selects which mode), the three tests, the tag and stencil write paths, the two-stage pipeline, the stall that re-issues its read, and the counters. Those are covered by the differential lanes and the mutation evidence above. What *is* proved is the arithmetic every blended fragment in the machine flows through, three times over.

## Synthesis / resource ceiling

Budget group `tile`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers, and it is deliberately not in `fpga/files.qip`. The shape is ~220 flops of pipeline register (two stages × ~110 bits), two 32-bit counters, three `zhao_raster_blend` channels (each one 9×8 signed multiply, one 8×8 unsigned multiply, an adder and a clamp — 6 small multipliers total, DSP or LUT at the tool's discretion) and the test/mux logic.

The thing to watch at fit time, not measured: **stage 1 is one long combinational path** — the tile-store output register, through unpack, the three tests, three blend channels with their multiplies, the tag/stencil/depth muxes, and back out to the write port in the same cycle. That is deliberate (it is what makes the write-first hazard immunity work and throughput 1/clock), but it is the block's critical path by a wide margin. If it does not close, the honest fix is a third stage plus a real forwarding path for the same-pixel case — not a quiet retime, because the two-cycle spacing is exactly what the write-first rule depends on.

Two smaller notes: the fragment packet is 128 bits wide before the tile word, which is a wide bus for a per-pixel channel; and `frag_state_i` is carried **per fragment** although it is draw-constant. Carrying it in band is what keeps the block stateless with respect to the recipe, which is what makes it testable one fragment at a time; in integration it would be a register written per draw and the saving is 32 bits of pipeline.

## Integration capture cases

None yet on hardware. **Composed, in simulation only (2026-08-18):** `fpga/rtl/raster/zhao_raster_tile_pipe.sv` instantiates this block on RASTER.TILESTORE's read port A and write port, downstream of RASTER.EARLYZ. `tests/raster/raster_tile_pipe_directed.cpp` and `..._random.cpp` diff the whole chain against five composed oracles, including all six recipes end to end and the additive rail resolving to white. Re-measured throughput of the composition after this block's arrival: **316 cycles/tile back-to-back and 559 serial** for 16 full tiles, against 313 and 556 before — +3 cycles a tile in both, the fragment chain's fill-and-drain paid once per tile, with steady state unchanged at one covered pixel per clock.

It is Verilator-only — not in `fpga/files.qip`, no Quartus fit, no capture, never on hardware. Simulated is not synthesized and neither is on-hardware.

## Notes

Fixed-point blend exactly per `spec/qformats.md`; no floating-point anywhere in the path (§26).

Deliberately not built in this block, so the next wave knows: no texture sampler of any kind (TEXTURE.TMU — the texel is a packet field); no soft-particle port (PART.SOFT's fade rides the existing alpha lane); no fog (reasoned above, and it is the spec's decision); no attribute interpolation (GEOM.SETUP); no depth comparison modes beyond the two the spec defines; no stencil fail/zfail ops (reasoned above); no MSAA; no `texture_stalls` counter; no `frame_tick` shadow latch or catalog-id binding; no third pipeline stage or forwarding path (the write-first rule makes both unnecessary at the current timing).
