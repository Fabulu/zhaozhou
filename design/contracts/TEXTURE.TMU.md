# Contract — TEXTURE.TMU (Primary TMU)

> Ledger: `design/blocks.yml` · owner ZH-027 · phase 5 · maturity SPECIFIED

## Purpose and exclusions

**The** primary texture unit — nearest / bilinear / mip / CLUT / direct / wrap — and, by charter §26, the only unrestricted sampler this machine will ever have. RTL: `fpga/rtl/texture/zhao_texture_tmu.sv` plus `fpga/rtl/texture/zhao_texture_bilerp.sv` (the filter channel, a separate file so it can be proved).

Charter §15's Primary TMU capability list is exactly this block's job list: "nearest fast path; bilinear filtered path; mip selection; palette and direct colour; wrap/clamp/mirror".

Exclusions — none of these are in this block:

- **The perspective divide.** `spec/qformats.md` §8's per-pixel `u = rescale((s64)u_over_w · rcp_u24(invw24_interp))` happens upstream; U and V arrive divided. Doing it here would put a reciprocal in the sampler and duplicate the one the interpolator already owns.
- **TEXTURE.MOSAIC's `mosaic_pick`.** The ledger lists it as an input and this block grows **no port** for it, for the same reason RASTER.FRAGMENT grew none for `soft_particles`: a Mosaic pick selects *which candidate tile* a texel comes from, and a tile's identity in this machine is its **base address** — `req_base_i`. So the pick rides the base, as a mux in front of this block, and TEXTURE.MOSAIC (phase 6) owns that mux. Putting the two-candidate choice inside the sampler would give this block terrain's material policy, which belongs to a phase-6 contract.
- **The VRAM fetch.** Misses are TEXTURE.CACHE's; this block only ever asks.
- Trilinear; anisotropic (charter §26 refuses it by name); block-compressed formats (charter §15 items 6/7, "if resources permit" — last in the order and asked for by no recipe); texture writes; **LOD derivation**; border colour (the three wrap modes are the charter's three); per-request scissor.

### §26 is an architectural constraint, and it shapes the file

The charter refuses "a second unrestricted TMU" and the ledger repeats the refusal twice — here and in TEXTURE.AUX's purpose ("deliberately NOT a general second TMU"). The consequence is a design rule, not a comment: **every sampling mode the machine will ever have has to fit through this request channel, because there is nowhere else for one to live.** So the block is one datapath with a mode *word* rather than a family of samplers — one address generator serving nearest and bilinear (bilinear is the same generator run on a footprint instead of a point), one wrap function shared by both axes and all four taps, one mip selector in front of all of it, one format decoder where CLUT and direct colour meet. A second sampler would have been the easy way to add bilinear, and it is precisely what §26 forbids.

## The recipes this block must serve

`design/contracts/RASTER.FRAGMENT.md` records which sampling each ratified recipe requires; this is the other half of that pair.

| recipe / layer | spec | mode this pins |
|---|---|---|
| `beam_additive_fade` | sky_and_beams §2: "Bilinear TMU mandatory (nearest 16-texel ramp = visible stairs). Texture 16×64 **direct colour** RGB565/ARGB4444 — deliberately not CLUT, so bilinear never touches a palette" | `FILTER_BILINEAR` + `FMT_RGB565`/`FMT_ARGB4444`, **non-square** — which is why `LOG2W` and `LOG2H` are separate fields |
| `star_disc_masked`, `star_halo_additive` | stars §1: "CLUT8 nearest+mips … **Nearest mandatory** — bilinear must never touch a palette" | `FMT_CLUT8` + `FILTER_NEAREST` + `MIP_EN`; the **raw index** is returned |
| sky drum bands | sky_and_beams §1.1: "1024×128 CLUT8, u-**mirror**, v-**clamp**, +mips" | per-axis wrap modes — which is why `WRAP_U` and `WRAP_V` are separate fields |
| sky cloud sheet | §1.1: "256×256 ARGB4444, u/v-repeat, +mips", recipe `a = tex.a × vertex.a` | ARGB4444's alpha must survive the filter as a fourth channel |
| sun quad | §1.1: "64×64 ARGB4444, alpha pre-baked" | — |
| terrain Mosaic tile | terrain_rules §6.2 | 64×64 CLUT8, `WRAP_MIRROR`, nearest |

## Clock and reset semantics

Single clock `clk` in the `gpu` domain; no CDC. Reset is `rst_n`, async assert / sync release. On reset: the state machine is `ST_IDLE`, no cache request is outstanding, `mode_error_o` is low and `texture_samples_o` is 0.

## Input and output packet layouts

| channel | fields | meaning |
|---|---|---|
| texture_requests (`req_valid_i` / `req_ready_o`) | `req_u_i` 32, `req_v_i` 32, `req_base_i` 32, `req_pal_base_i` 32, `req_mode_i` 32, `req_lod_i` 8, `req_src_id_i` 16 | one sample request |
| cached_texels — master (`cac_valid_o` / `cac_ready_i`) | `cac_en_o` 4, `cac_addr_o` 4 × 32, `cac_src_id_o` 16 | one TEXTURE.CACHE access |
| cached_texels — response (`cac_valid_i` / `cac_ready_o`) | `cac_data_i` 4 × 16 | one halfword per lane |
| texture_samples (`smp_valid_o` / `smp_ready_i`) | `smp_rgb_o` 24, `smp_a_o` 8, `smp_idx_o` 8, `smp_src_id_o` 16 | the sample |
| mode_error (`mode_error_o`) | — | one-cycle pulse; a malformed request, never a silent fallback |
| status / counters | `idle_o`, `texture_samples_o` 32 | saturating |

**The output fields ARE `zhao_raster_fragment`'s `frag_texel_rgb_i`, `frag_texel_a_i` and `frag_texel_idx_i`.** That interface was designed for this block and **is not changed by its arrival** — the texel now arrives from a port instead of from a driver, exactly as `design/contracts/RASTER.FRAGMENT.md` predicted.

### The sampler mode word (32 bits, layout defined HERE and in the RTL)

Encoded so that **`mode == 0` is `CLUT8, nearest, repeat/repeat, level 0`** — a 1×1-wrap 8-bit paletted point sample, this machine's most common texel by a wide margin (terrain Mosaic tiles, the sky drum, star discs) and the first format in charter §15's implementation order.

| bits | field | values |
|---|---|---|
| `[2:0]` | `FORMAT` | 0 CLUT8, 1 RGB565, 2 CLUT4, 3 ARGB1555, 4 ARGB4444 (charter §15's order); 5..7 undefined → error |
| `[3]` | `FILTER` | 0 nearest, 1 bilinear |
| `[5:4]` | `WRAP_U` | 0 repeat, 1 clamp, 2 mirror (3 reserved, behaves as repeat) |
| `[7:6]` | `WRAP_V` | same |
| `[11:8]` | `LOG2W` | level-0 width = `1 << LOG2W` |
| `[15:12]` | `LOG2H` | level-0 height = `1 << LOG2H` |
| `[19:16]` | `MAX_LEVEL` | highest legal mip level |
| `[20]` | `MIP_EN` | 0 = level 0 always (`req_lod_i` ignored) |
| `[31:21]` | reserved | must be zero |

## Q formats and rounding

### The UV format — found, not invented

`spec/qformats.md` §8 fixes the perspective divide but never names the format of its **result**. The repository does, in the only place a texture coordinate is actually consumed today: `zref::terrain::mirror_texel(int32_t u_raw)` (frozen 2026-08-16) documents "u is Q16.16 TILE units" and computes `m = u_raw >> 10` for a 64-texel tile — i.e. `floor(u · 64)` with `u = u_raw/65536`. So a texture coordinate in this machine is **S 15.16 in TEXTURE units, 1.0 = one full wrap**, and this block adopts exactly that.

Converting to texels at level *L* is then a **shift, not a multiply**: with `size = 1 << log2s`, the texel coordinate in Q16.16 is `u_raw << log2s`, whose integer part for `log2s = 6` is `u_raw >> 10` — bit for bit the frozen helper. **Nearest takes that floor directly, so nearest sampling IS the §6.2 law**, and `tests/texture/texture_tmu_directed.cpp:test_mirror_is_the_frozen_fold` asserts it against the frozen helper over a full period rather than against a restatement.

### The half-texel bias — A CHOICE (no spec states one)

A texel owns `[k, k+1)` under that floor, so its **centre** is at `k + ½`. Bilinear therefore samples at `texel_q16 − 0x8000` before taking its floor and fraction, which makes the two filters **agree at every texel centre**.

Without the bias a bilinear ramp would sit half a texel away from every nearest-sampled thing beside it — and `spec/sky_and_beams.md` §1.1 puts a bilinear beam and a nearest CLUT drum in the same frame. The alternative (no bias) is the other defensible convention and is what a naive implementation does; it is rejected for that reason. Pinned by `test_half_texel_bias`.

### The bilinear filter — A DERIVED LAW, not a cited one

**No spec in this repository defines bilinear weights or their rounding.** Searched and found empty: `spec/qformats.md` (§2/§3/§4 define `unit8`, `unit_mul` and `rescale`, and §8 defines perspective UV, but no filter), `spec/sky_and_beams.md` §2 (says "Bilinear TMU mandatory" and nothing about how), `spec/stars_and_flares.md` §1 (says only where bilinear may *not* go), `spec/terrain_rules.md` §6.2 (the mirrored fold and the Mosaic pick — nearest), and the charter, which lists "bilinear filtered path" (§15) and "bilinear rounding" (§20.1, as a thing ZRef must be exact about) without stating either.

So the law below is **derived** from the three that do exist, and this section is the record so a future ratification amends one file:

1. `spec/qformats.md` §2 — a `unit8` is `U 0.0.8` and its **value is raw/256**. The sub-texel fractions are unit8s, so the complement of `fu` is `256 − fu`, which needs 9 bits and **reaches 256**. That is what makes the weights sum to exactly 65,536 with no correction term.
2. `spec/qformats.md` §3, the **single-rounding law** — "any multiply-then-add instruction computes the EXACT wide-integer expression and rounds EXACTLY ONCE via `rescale(·,k)` at the end. Double rounding is *rejected*." A bilinear filter is a multiply-then-add of four products, so the two-lerps-then-a-lerp formulation every textbook writes (three roundings) is **refused by that law**.
3. `spec/qformats.md` §4 — `rescale_u(x, k) = (x + (1 << (k−1))) >> k`, round-half-up. With Q16 weights that is `(Σ + 32768) >> 16`.

```
w00 = (256 − fu)·(256 − fv)    w10 = fu·(256 − fv)
w01 = (256 − fu)·fv            w11 = fu·fv          ⎫ Σw = 65,536 exactly
out = (t00·w00 + t10·w10 + t01·w01 + t11·w11 + 32768) >> 16
```

The same Q16 shape is already shipping in this repository — the software raster's alpha blend is `(dst·ia + src·a + 32768) >> 16` with `ia = 65536 − a` (`reference/src/zrender/rast.cpp`) — so this is that frozen form widened from two taps to four, not a new numeric idea.

**Endpoints, stated because they surprise people:** `fu = fv = 0` gives `w00 = 65,536` and the result is **exactly** `t00` — the filter is the identity at a texel's own sample point, which is what lets `FILTER_NEAREST` force both fractions to zero and take **the same datapath** rather than a parallel one. `fu = 255` is 255/256, **not** 1.0, so `t10` is never weighted fully — the identical unit8 endpoint `zhao_raster_blend` documents for `a = 255`.

### How it is COMPUTED — the factored form, 8 products to 3 (2026-08-23)

The law above is what the filter must equal. It is not how the filter is built, and the difference is 20 multipliers.

Written literally, each channel is four texel-by-weight products plus four weight products — **eight multiplies**, four instances, **32 products of which 12 are literal duplicates**. `zhao_texture_bilerp` now computes the identical integer by factoring:

```
A = t00·(256−fu) + t10·fu  =  (t00 << 8) + (t10 − t00)·fu    1 product
B = t01·(256−fu) + t11·fu  =  (t01 << 8) + (t11 − t01)·fu    1 product
S = A·(256−fv)   + B·fv    =  (A   << 8) + (B   − A  )·fv    1 product
out = (S + 32768) >> 16                                      1 rescale
```

Expanding `S` gives `t00·w00 + t10·w10 + t01·w01 + t11·w11` term for term. **Three products a channel, twelve across the block, and there are no weights left to duplicate — `w00` does not exist any more.**

**THIS IS NOT THE FORM §3 REFUSES, and the distinction is the whole argument.** The single-rounding law rejects "two lerps then a lerp" because the textbook writes each lerp as a **rounded** unit8 blend — three `rescale` calls, bits lost at every stage. Nothing above is rounded: `A` and `B` are exact integers in a signed-18 lane (their true range is `[0, 65,280]`), `S` is the exact weighted sum in a signed-27 lane, and there is **exactly one** `(S + 32768) >> 16`, on the whole sum. This is algebraic factoring of the wide-integer expression §3 *demands*, not staged rounding of it.

**The operand widths are the honest ones**, which is `reports/QUARTUS_GOTCHAS.md` §5 applied rather than quoted: `(t10 − t00)` is a signed 9, `fu`/`fv` are signed 9s (a unit8 with its sign bit clear), `(B − A)` is a signed 18. So the three multiplies are **9×9, 9×9 and 18×9**. The form this replaced declared its texel products as `{17'd0, t00_i} * {8'd0, w00}` — a **25×25** multiply whose real need was 8×17, and §5 measured that exact class of slack costing `zhao_geom_lod` ten DSP blocks.

**The HARNESS did not have to move — but the PROOF no longer closes, and the two are not the same claim.** `zhao_texture_bilerp`'s ports are unchanged, so `tests/formal/texture_bilerp.sby` and `texture_bilerp_fv.sv` needed **not one line changed** and the cover task still passes; **the bmc task ran 3,300 s without an answer**, so P1–P4 are currently unproved on this filter. See THE HARNESS DID NOT MOVE, THE bmc TASK NO LONGER CLOSES under Formal properties, and the **total** two-part equivalence that stands in its place. Factoring was still the right choice over the weight hoist this contract sanctioned in 2026-08-21 — it removes 20 of the 32 products where hoisting removes 12, and changes no interface — but "the proof follows for free" was too strong.

**The one-LSB traps, named so the tests can aim at them:** truncate instead of round (the tie `t = (0,255), fu = 128, fv = 0` gives `Σ + 32768 = 8,388,608` exactly, so round gives 128 and truncate gives 127 — pinned by name); swapped weights (`w10` paired with `t01`, invisible whenever `fu == fv` or the footprint is symmetric, so the tests use asymmetric fractions); a `/255` scale (weights are unit8 **products**, so the scale is 256·256 — the same /256-vs-/255 distinction `zhao_raster_blend` argues at length: this is a **weighting**, not a quantizer).

### Colour expansions

- **RGB565** — the frozen `c8 = (c5<<3)|(c5>>2)`, `(c6<<2)|(c6>>4)` of `spec/stars_and_flares.md` §2, implemented as `zref::sky::rgb565::to_rgb888` and, in arithmetically identical form, as `(c·255 + half)/max` in `reference/src/zrender/rast.cpp` and `tools/reel`. The oracle **calls** that helper rather than copying it.
- **4-bit and 1-bit lanes (ARGB4444, ARGB1555) — A CHOICE.** No text states them. Bit replication (`(c4<<4)|c4`; the alpha bit becoming 0 or 255) is the consistent extension of the two that *are* written: it is the unique expansion that round-trips both 0 and full scale exactly.
- **A CLUT texel's alpha is 255.** Its transparency is a property of its **index**, tested by RASTER.FRAGMENT (that contract's THE ALPHA TEST IS AN INDEX TEST), never of a palette alpha the RGB565 page has no room for.
- **A direct-colour texel's index is 0.** It has none; the two state bits that read one (`ATEST_EN`, `TAG_FROM_TEXEL`) are set by no direct-colour recipe.

## Mip selection — the LOD arrives, it is not derived here

Nothing in this repository defines a texture LOD formula, and the machinery the usual one needs does not exist: screen-space derivatives require 2×2 pixel quads, and RASTER.EDGEWALK emits per-pixel coverage with no quad structure anywhere in its contract. What *does* exist is charter §9, the Measure — screen-space LOD as "the console's central law", computed upstream with hysteresis. So the request carries an explicit `req_lod_i` (**U 4.4**) and this block selects

```
level = min(lod >> 4, MAX_LEVEL, min(LOG2W, LOG2H))
```

**Floor, and no trilinear — both CHOICES.** Floor because `rescale` is a rounding primitive for *values* and a level is an *index*; rounding a level to nearest would make a surface flip mip one texel earlier on one side of a triangle than the other for no gain anyone asked for. No trilinear because no spec asks for it, it doubles the fetch, and charter §26's cut order exists for exactly this class of "nice but uncosted".

**The third clamp term is a correctness bound, not taste.** The mip chain stops at the level where the *smaller* dimension reaches 1 texel: a 16×64 beam ramp has levels 0..4 and no more. The closed-form level offset below is exact only inside that range.

### Where a level lives — the closed form

Levels are packed in order after level 0. The first texel of level *L* sits at texel offset

```
Σ_{i<L} (W>>i)(H>>i) = W·H · (4^L − 1)/(3·4^(L−1)) = REP4[L] << (LOG2W + LOG2H − 2(L−1)),  L ≥ 1
```

with `REP4[L] = (4^L − 1)/3` = 0, 1, 5, 21, 85, 341, … — the base-4 repunits. A 16-entry constant table and one variable shift replace an eleven-deep add chain, and the value is bounded by `(4/3)·W·H < 2^21` for every legal texture, so the whole computation fits a 32-bit lane. `zref::Tmu::level_offset_texels` is the **plain summation loop**, so "RTL == oracle" is a real check of that identity; the directed suite also checks the loop against a running sum at every legal `(LOG2W, LOG2H, level)`.

### Texture layout — ROW-MAJOR, a CHOICE

Charter §15's Layout bullet asks for "swizzled/Morton-order small blocks", but **no Morton formula is ratified anywhere in this repository**, while the only concrete texture layout that exists — `zref::render::Tileset` (`reference/include/zref/zref_render.hpp:166` — `tiles[t][(ty<<6)+tx]`, consumed by `reference/src/zrender/rast.cpp:250`) — is row-major. Matching the shipping reference beats inventing a swizzle the asset compiler does not emit. Changing it later is a change in this block's address generator and in the asset compiler, and nothing else.

## Wrap modes

Applied to the integer texel coordinate at the selected level, per axis, per tap, with `mask = size − 1`:

- **REPEAT** — `t & mask`. Two's complement AND **is** the floored modulo, so a negative coordinate wraps correctly with no correction.
- **CLAMP** — to `[0, size−1]`.
- **MIRROR** — `spec/terrain_rules.md` §6.2's **frozen** fold, generalised: `per = floored t mod 2·size`, `texel = per < size ? per : 2·size−1−per`. With `size` a power of two this is a mask, a bit test and a subtract. At `size = 64` it is `zref::terrain::mirror_texel` exactly, and the directed suite proves that over 280 coordinates spanning a full period.

## Backpressure rules

`ready_valid` on the request, cache-access and sample channels.

`req_ready_o = (st_r == ST_IDLE)`; `cac_valid_o` is a function of registers only, never of `cac_ready_i`; `smp_valid_o = (st_r == ST_OUT)`, never a function of `smp_ready_i`. The sample outputs are driven combinationally off registered state, which is what keeps `ST_OUT` a single cycle rather than two.

## Latency (fixed or variable)

**`variable_bounded:16`**, as the ledger declares, with the bound belonging to this block and the variability belonging to the cache. Measured by the directed suite against a cache that answers in one cycle: **worst accept-to-retire = 5 cycles** (direct colour 3, CLUT 5 — the palette entry cannot be addressed until the index arrives, so the two accesses are unavoidably serial). A cache miss adds TEXTURE.CACHE's `latency: variable`, which is that block's, not this one's.

## Target throughput

**The ledger says "1 sample per clock (bilinear = 1 request)". Half of that is met and half is not, and the shortfall is stated rather than papered over.**

- **"bilinear = 1 request" — met.** One request beat produces one sample beat; the four taps are one cache access on four lanes, not four serialised lookups.
- **"1 sample per clock" — NOT met, and the ledger's number is not the one that matters anyway.** The block is a state machine with no pipelining, so on an always-hit cache it sustains **one sample per 4 clocks** for direct colour at `FILT_LANES = 4` (5 at 2, 7 at 1) and **one per 6** for CLUT at every setting, because a palette is never filtered and the CLUT path therefore never enters `ST_FILT`.

### The demand is DERIVED from Sacrifice, and it is not one per clock — it is one per two

`docs/OWNER_DOCKET.md`, "THE THREE DEMAND NUMBERS", entry 2. Terrain is layered **tile + detail + lightmap** (`sacmap.d:136-174`), so **≥3 samples per terrain pixel**; tiles are 64×64 CLUT8, detail textures 256×256, one 256×256 lightmap per map, and there is **no alpha splatting between tile types** — the tile index is per-cell and hard-edged, which is a simplification in our favour. At 92,160 pixels (Z60) with 3× overdraw that is **829,440 samples/frame**, rounded up to **850,000** for headroom.

Against `design/budgets/latency.md`'s **compute** budget of **1,666,667 clocks/frame** — *not* the 251,520 raster period, which is 6.6× smaller and is also called "gpu cycles" — the demand is **one sample every two clocks.**

### Measured against it, and the shortfall is 3×

**MEASURED 2026-08-23 by `tests/texture/texture_tmu_directed.cpp:test_throughput_against_the_derived_demand`**, which asserts both intervals *exactly* so neither can drift. Samples/frame is `min(Fmax, 100 MHz) / (60 × II)`; this block's constrained Fmax is 199.72 MHz, so the shared `gpu_clk` is the binding term and the figure is `1,666,667 / II`:

| path | II | samples/frame | vs. 850,000 |
| --- | ---: | ---: | ---: |
| direct colour (`FILT_LANES` 4 / 2 / 1) | 4 / 5 / 7 | 416,667 / 333,333 / 238,095 | 0.49× / 0.39× / 0.28× |
| **CLUT — the demand-critical path** | **6** | **277,778** | **0.33×** |

**Terrain is CLUT8, so the 0.33× row is the one to read.** Note also that this shortfall is **not** what the 28 DSPs were buying: every multiplier was in the filter, and the CLUT path does not use the filter at all.

### What reaching the demand takes, designed but not built

**II = 2 is the cache access port's own floor, and it coincides exactly with the demand.** A CLUT sample needs two accesses — texel then palette, unavoidably serial because the palette address is a function of the returned index — and `zhao_texture_cache` accepts one access per clock (`acc_ready_o = (need_c == 0) && !fill_busy_r && (!s1_v_r || smp_ready_i)`, a 1-deep response pipeline). Two accesses through a one-per-clock port is II = 2, which is 833,333 samples/frame: **1.005× the derived 829,440 and 0.98× the 850,000 it was rounded up to.**

The three things that needs, which this block does not have:

1. a **2-entry in-flight record** — format, CLUT flag, the two fractions, byte/nibble select, palette base, source id and the four returned halfwords, about 134 bits an entry. The 128-bit address bank is *dead* once the access is issued and would not be duplicated;
2. an **issue arbiter** over the single cache port, palette-before-texel so that order is preserved;
3. an **in-order completion**, because a direct-colour request behind a CLUT one would otherwise finish first and the sample channel would go out of order.

`mode_error_o` would keep pulsing in the cycle after acceptance, which is how `tests/texture/texture_tmu_dev.hpp` attributes it; at II = 2 acceptances are two cycles apart, so attribution stays unambiguous.

**One thing outside this block would also have to move, and it is a test, not RTL.** `texture_tmu_dev.hpp` models a **strictly one-outstanding** cache (`if (cac_busy) add(err, "a cache access while one was outstanding")`), which the real `zhao_texture_cache` is not. Bringing the model up to the real block's 1-deep pipeline is a faithfulness *improvement*; it is named here in advance so it cannot later be mistaken for a test relaxed to fit an RTL bug.

**It is a datapath change to this file only** — the ports, the mode word and `zref::Tmu` are all unaffected.

**One cycle of it is nearly free and is deliberately NOT taken on its own.** `req_ready_o = (st_r == ST_IDLE)` refuses the next request during `ST_OUT`, so every sample pays a cycle for a handshake that could overlap; `(st_r == ST_IDLE) || (st_r == ST_OUT && smp_ready_i)` would make it 3 and 5 instead of 4 and 6, and the ready-depends-on-ready path it creates is one `zhao_texture_cache` already has and documents (`acc_ready_o` line 331). It is left alone because it is **not enough to change the answer** — 5 clocks is still 0.39× the demand — while it *is* enough to change the Backpressure rules section above, the `variable_bounded` measurement and every mutation score. It belongs in the II = 2 change, as one line of it, not as a separate increment that buys 20% and costs a full re-verification.

## Overflow and malformed-input behaviour — it is LOUD

Charter phase-5 gate: "no unsupported state silently falls back". Each malformed state below produces a **defined sample AND a one-cycle `mode_error_o` pulse**; none is silent and none corrupts an address (charter §8: "overflow stays correct and becomes slower rather than corrupting memory").

1. **Bilinear on a palette** — `spec/stars_and_flares.md` §1's "bilinear must never touch a palette" is a hard law, so it is **enforced in the fabric** rather than trusted to the caller: a CLUT format with `FILTER_BILINEAR` samples **nearest** and pulses the error. Forcing nearest rather than refusing the request is the fail-safe direction — a palette read through a filter is a banded smear of unrelated colours, which is the visible corruption the law exists to prevent.
2. **A set reserved bit** (`[31:21]`) — the caller believes in a mode this block does not have.
3. **`MAX_LEVEL` beyond the chain** — clamped to `min(LOG2W, LOG2H)`.
4. **An undefined format code (5..7)** — decodes as RGB565 and pulses the error.

Beyond those: every U/V value is legal (negative coordinates wrap by law); every `req_lod_i` is legal (clamped); every base address is legal (MEM.GUARD owns the region check); the counter saturates at `0xFFFF_FFFF` per `spec/counters.md` §4; and there is no input that can make the block hang — the only wait states are the cache's readiness and the consumer's.

## Counters and traces

`texture_samples_o`, saturating 32-bit, incremented once per **retired** sample. The catalog id and the `frame_tick` shadow latch are not implemented here, exactly as the RASTER blocks deferred theirs; `texture_samples` is claimed by four blocks in `design/blocks.yml`, and reconciling multiple producers of one catalog entry belongs with the DEBUG.COUNTERS integration wave.

`mode_error_o` is an **event**, not a channel: a malformed request still produces its sample, so a handshake would be a second sample lane for a condition that already rides one.

## Scalar reference function

`zref::Tmu` (`reference/include/zref/zref_texture.hpp`, `reference/src/zrender/texture.cpp`).

It restates no arithmetic it can delegate: the RGB565 expansion is `zref::sky::rgb565::to_rgb888`, and MIRROR is the generalisation of `zref::terrain::mirror_texel` that the directed suite pins against the frozen helper itself. It shares **no structure** with the RTL — no state machine, no cache handshake, no registered mode word, and the mip offset is a summation loop where the RTL uses a closed form — so "RTL == oracle" tests the RTL's mechanics and that identity, not a shared clever expression.

`Tmu::plan()` exposes the addressing a request implies before any memory is touched, so the tests can pin mip offsets, wrap folds and lane assignment without needing a texture in memory to look at.

## Directed tests

`tests/texture/texture_tmu_directed.cpp` (driver `tests/texture/texture_tmu_dev.hpp`) — **76 checks**, every one stepped through the RTL and `zref::Tmu` with the RGB, the alpha, the CLUT index, the source id and the `mode_error` verdict compared. **The same file is built three times**, at `FILT_LANES` 4, 2 and 1 (`texture_tmu_directed`, `texture_tmu_lanes2`, `texture_tmu_lanes1`); not one sampled byte differs between them, which is what makes `FILT_LANES` a resource axis rather than a behaviour one.

Cases: all five formats with their expansions pinned by value (0xF800 → pure red, 0xFFFF → full white, 0x1234 ARGB4444 → 11/22/33/44, ARGB1555's alpha bit → 0 or 255, CLUT4's nibble selection across all four texels of a row); **CLUT index 0 end to end** — the eight texels of a row report indices 0..7 exactly, and feeding those three fields into `zref::FragmentPipeline::star_disc_masked` kills exactly one fragment, writes seven, and gives each survivor a glow tag carrying **its own** CLUT intensity; the three wrap modes at every boundary including negative coordinates, plus the sky drum's per-axis `u-mirror / v-clamp`; **MIRROR against `zref::terrain::mirror_texel`** over 280 coordinates spanning a full period at a third of a texel a step; nearest as the bilinear identity at a fractional coordinate; the bilinear **rounding tie** by name, both endpoints, and 24 asymmetric footprints that a weight swap would move; the half-texel bias (bilinear == nearest at all four texel centres); mip selection boundaries (0x0F → level 0, 0x10 → level 1, 0x1F → 1, 0x20 → 2, 0x30 → 3), the `MAX_LEVEL` clamp, `MIP_EN` off, and the level-offset closed form against the summation loop at every legal `(LOG2W, LOG2H, level)`; all four mode-error cases including the proof that the bilinear-on-palette case really does sample nearest; the non-square 16×64 beam ramp; and **nine timing patterns** that must agree byte for byte, with the `variable_bounded:16` claim measured.

**And, since 2026-08-23, the THROUGHPUT — which nothing measured before.** `test_backpressure_and_latency` asserted accept-to-retire and byte stability; the ledger's "1 sample per clock" and this contract's "one per 4 / one per 6" were **prose**, which is precisely the shape `reports/REMAINING_BLOCKERS.md` now tells every block to distrust, sitting inside the suite that was supposed to catch it. `test_throughput_against_the_derived_demand` measures both initiation intervals on an always-hit cache and asserts them **exactly** — 6 for CLUT at every setting, `3 + PASSES` for direct colour — and prints samples/frame against the docket's derived 850,000. The integer division it uses is exact rather than approximate, and the case says why: the drain is strictly less than the interval on every path (3 < 4, 5 < 6, 6 < 7).

## Randomized differential tests

`tests/texture/texture_tmu_random.cpp` — deterministic from fixed seeds. Three lanes:

**Lane A — free traffic.** Every mode-word field drawn independently: all five formats plus the undefined codes, both filters, all four wrap encodings, every legal texture shape, every lod. Malformed states arrive as a matter of course. UV coordinates are drawn from a **narrow window around texel boundaries**, so `fu = 0` (the identity), `fu = 128` (the rounding tie) and the wrap folds are common events rather than 1-in-4-billion ones.

**Lane B — the ratified recipes.** The four sampler states the specs actually name (star disc, sky drum, cloud sheet, beam ramp), each with a footprint walking the surface.

**Lane C — the filter, hammered.** One bilinear surface with fractions sweeping 0..255 in both axes against random texel content.

The lanes assert their own coverage and fail if any bucket is empty; at the fast setting they fire **3,749 bilinear samples (476 exactly at a rounding tie, 26 exactly at a texel centre), 2,052 mipped samples, all three wrap modes (8,588 / 2,036 / 1,535), all five formats, and 1,122 malformed against 5,499 clean modes**. Default 260 + 80 + 80 batches (CTest `fast`); `--nightly` 60,000 + 18,000 + 18,000, which runs in **9 s** and fires 1,518,128 samples — 859,582 of them bilinear, 116,681 exactly at a rounding tie — across 470,478 mipped samples, all three wraps and 260,993 malformed modes. Failing vectors are serialized per charter §29-17.

## Mutation evidence (2026-08-23) — 30 mutants, 29 caught, one true equivalent

`tools/sweep_texture_tmu.sh`, run detached in a git worktree at the shipping
commit, against **all four consumers** of both RTL files — the directed suite at
`FILT_LANES` 4, 2 and 1, plus the random lane. Guards 1–7 carried unchanged from
`tools/sweep_surface_stamp.sh`; every mutant preflight-linted at all three
settings before any was scored.

    linted 30 mutants at FILT_LANES (4, 2, 1), 0 do not build
    pristine models 59747279bbfe/59747279bbfe, 4 lanes green
    attempted=30 expected=30 accounted=30 caught=29

`attempted == accounted == expected == 30`: every mutant re-elaborated (its
model-directory hash differed from pristine), every one linked, no discards, and
the worktree's RTL was restored byte-identically afterwards.

**13 on `zhao_texture_bilerp`** — the three named one-LSB traps in the shapes
they take in the factored form (rounding dropped, halved, or shifted 15), the
transposed footprint, both differences reversed, both place values reduced to
128, the two fractions crossed, and the wrong tap or row seeding each lerp.
**All 13 caught.**

**17 on `zhao_texture_tmu`** — six on the channel mux and pass counter, three on
the sample assembly, eight re-running the behaviour the 2026-08-18 table
covered (the wrap folds, the mip level and its offset, the CLUT index, the
half-texel bias, the stars §1 palette override, the four-lane fetch). **16
caught.**

### The frontier builds are what caught six of them, and no single setting reaches all six

At `FILT_LANES = 4`, `PASSES` is 1, so `ST_FILT` is never entered (the ST_TEX
transition is `st_r <= (PASSES > 1) ? ST_FILT : ST_OUT`), `pass_r` is therefore
provably 0 for ever, `pass_c` is the constant `PASSES−1 = 0`, and
`sel_base = 0 << LANE_SHIFT` is constantly 0. Each of the six is then invisible
for a reason that can be checked against one line rather than assumed:

| mutant | why it cannot be observed at `FILT_LANES = 4` |
| --- | --- |
| M11 `ch_pack[sel_base \| gj]` → `ch_pack[gj]` | `sel_base ≡ 0`, and `0 \| gj == gj` |
| M12 `pass_c = pass_r` instead of `PASSES−1` | `pass_r ≡ 0` and `PASSES−1 = 0` |
| M13 `pass_c << LANE_SHIFT` → `>>` | `0 << 2` and `0 >> 2` are both 0 |
| M14 the `fres_r` write index | inside `ST_FILT`, which is unreachable |
| M15 the pass counter never advances | inside `ST_FILT`, which is unreachable |
| M16 `LAST_FILT_PASS` forced to 0 | that localparam **already** evaluates to 0 |

**M16 is invisible at `FILT_LANES = 2` as well** — `LAST_FILT_PASS = PASSES − 2`
is 0 at `PASSES = 2` — so it is a real defect only at `FILT_LANES = 1`.
**No single setting reaches all six.** Scoring one would have produced six
phantom survivors and sent someone hunting a test gap that is not there.

### M27 was a real hole in the test harness, and it was FIXED rather than argued

"A bilinear request fetches one tap instead of four" (`q_en_r <= 4'b0001`)
**survived the first run**. `tests/texture/texture_tmu_dev.hpp`'s modelled cache
ignored `cac_en_o` and returned four lanes of correct data unconditionally —
**strictly more generous than the block it stands for.** `zhao_texture_cache.sv`
lines 75–76 say the opposite: lanes 1–3 of a nearest access are "not looked up,
not counted, and not filled".

The model now returns the **complement** of the texel on a disabled lane:
deterministic, so a failing vector stays reproducible, and guaranteed different,
which a fixed poison constant would not be. It is safe for a correct block for a
*proved* reason — only nearest requests disable lanes, nearest forces
`fu = fv = 0`, and there the filter is the exact identity on tap 0 (formal P3),
so lanes 1–3 cannot reach the output. All four lanes stayed green; M27 is dead.

### M18 is the one survivor, and no test can kill it

"A CLUT texel reports the filter's alpha instead of the law's 255" —
`smp_a_o = fin[3]` in place of `smp_a_o = q_clut_r ? 8'd255 : fin[3]`.

**Equivalent for every input, and the argument is a line in the same file.**
`decode16`'s `default` branch — the one CLUT8 (format 0) and CLUT4 (format 2)
fall into, because the case names only ARGB1555 and ARGB4444 — sets
`a_ = 8'd255`. So on a CLUT sample all four alpha taps are 255, and a flat
footprint filters to itself exactly (P1 with P2: `Σ 255·w = 255 << 16`). A test
that killed it would need a CLUT sample whose `fin[3] ≠ 255`; the input does not
exist.

**What the sweep found is worth more than the score:** "a CLUT texel's alpha is
255" is enforced **twice** — once by the documented mux, once accidentally by
`decode16`. The mux is defence in depth, not the sole mechanism. Recorded so
nobody later simplifies it away believing it is load-bearing, or adds a CLUT arm
to `decode16` believing it is free.

## Mutation evidence (2026-08-18, the pre-rearchitecture table)

Nine deliberate RTL defects, injected **one at a time**, each under a harness that (a) asserts the built `.exe`'s **SHA-256** changed before believing any result, and (b) asserts the reverted tree is **GREEN again** before the next mutation runs.

| # | mutation | file | cache directed | cache random | tmu directed | tmu random |
|---|---|---|---|---|---|---|
| 1 | bilinear weights swapped (`w10` ↔ `w01`) | bilerp | n/a | n/a | RED | RED |
| 2 | rounding truncated (`+32768` dropped) | bilerp | n/a | n/a | RED | RED |
| 3 | wrap MIRROR clamps instead of folding | tmu | n/a | n/a | RED | RED |
| 3b | wrap REPEAT clamps instead of repeating | tmu | n/a | n/a | RED | RED |
| 4 | mip level off by one (`lod >> 4` → `+ 1`) | tmu | n/a | n/a | RED | RED |
| 5 | CLUT index forced to 0 (the alpha-test case lost) | tmu | n/a | n/a | RED | RED |
| 6 | cache reports a hit on a tag mismatch | cache | RED | RED | n/a | n/a |
| 7 | an invalidate mid-fill no longer cancels the tag write | cache | RED | RED | n/a | n/a |
| 8 | cache ignores the valid bit | cache | RED | RED | n/a | n/a |

"n/a" means the mutated file is not compiled into that binary at all, so the lane cannot see it — the two blocks are separately verified and there is no composition (see Integration capture cases). Every mutation went RED in **both** lanes of the block that owns it.

**Mutation 7 found a real hole and it was fixed, not noted.** Both cache lanes were **GREEN** on it at first. The reason is precise: the directed case armed its invalidate on the fill's **last** beat, and on that beat the invalidate clears the valid bit in the same cycle the tag is written, so the `fill_kill_r` guard is invisible there. Every EARLIER beat is where the guard lives — that is the torn-line case. The directed suite now sweeps every early beat, and the random lane grew a fourth lane (`lane_d`) that arms a mid-fill invalidate at a random beat on a random line under random fill timings. Both go RED on the mutation now.

**Three sweeps were discarded before any of this was believed**, and the reason is worth recording because it is the exact trap the brief warned about. Touching a source file's mtime *forward* to defeat ninja's one-second granularity poisons the next comparison: ninja stores the mtime it saw, so a later edit stamped with a *smaller* future offset looks OLDER and the file is declared clean — the binary is never re-verilated and the lane reports the previous mutation's verdict. Symptom: `before` and `after` hashes identical, or a "revert" that leaves a mutated binary in place. The harness now deletes the generated `Vzhao_*.cpp/.h` for the affected targets instead of touching clocks, and refuses to report anything when the hash did not move.

## Formal properties

`tests/formal/texture_bilerp.sby` on `zhao_texture_bilerp` — the module this block instantiates **four times** per filtered sample (R, G, B, A). Two tasks, bmc and cover, `read_slang`, `smtbmc boolector`.

Free inputs are the four texel bytes and the two unit8 fractions — 48 bits, which is **total rather than sampled**: a texel channel *is* a byte (every charter §15 format decodes to charter §8's 8-bit lanes) and a fraction *is* a unit8. The harness is purely combinational, so depth 2 **is** the full state space, not a bound.

### THE HARNESS DID NOT MOVE. THE bmc TASK NO LONGER CLOSES. (2026-08-23)

Recorded in that order because the second half is the one that matters and the
first half is what made it easy to miss.

The filter was rewritten from eight products a channel to three (see THE
FACTORED FORM above) and **not one line of `texture_bilerp.sby` or
`texture_bilerp_fv.sv` changed** — the harness derives the four weights *itself*
from free `fu_free`/`fv_free`, so it stands over any DUT with these ports. It
elaborates, and the **cover task still passes in about 1 second**: every corner
it pins is still reachable on the factored form, including the exact rounding
tie and the strictly-inside-the-footprint case that stop P1 going vacuous.

**But the bmc task ran 3,300 s on boolector without an answer**, against a
recorded 741 s standalone / 1,264 s as the lane for the arithmetic it replaced.
So **P1, P2, P3 and P4 are currently UNPROVED on the shipping filter.**

**The reason is structural, and it is the same fact that makes the theorem worth
having.** `texture_bilerp_fv.sv` computes the law as `t00*w00 + t10*w10 +
t01*w01 + t11*w11`. Until today the DUT computed *that same expression*, so
`a_exact` was very nearly a **syntactic** identity — two structurally identical
circuits, which bit-blasting settles almost immediately. The DUT now computes a
chain of three narrower multiplies of three different widths, one of whose
operands is another's result. Proving them equal is a real distributive-law
identity over bit-blasted multipliers, which is the hard case for this engine.

**A run that reported "the proof did not have to move" and stopped there would
have been reporting a green harness as a green theorem.** It is not one.

### What stands in its place, and it is TOTAL rather than sampled

`runs/CLAUDE-RUNS/RUN-20260823-1736-.../total_equivalence_check.py`, in two
halves that between them settle the identity over **all 2^48 inputs**:

1. **No lane truncates.** Every intermediate is monotone in each texel, so its
   extremes over the byte domain occur at texel **corners**; enumerating all 16
   corners × all 65,536 `(fu, fv)` therefore bounds the whole domain exactly.
   **0 violations**, with the widest observed values at `|pu| ≤ 65,025`,
   `A, B ∈ [0, 65,280]`, `|dv| ≤ 65,280`, `|pv| ≤ 16,646,400`,
   `S ∈ [0, 16,711,680]` — every one comfortably inside its declared lane.
2. **Given no truncation, the pre-rounding sum is exactly linear in the four
   texels.** So `S(t) = Σ t_k·c_k(fu, fv)`, and evaluating the four basis
   vectors plus the zero vector at every `(fu, fv)` determines the map
   completely. The coefficients equal `w00, w10, w01, w11` exactly, for all
   **65,536** fraction pairs, with **0 mismatches** — which settles the identity
   for *every integer texel quadruple*, not merely every byte one.

**What that does not cover, stated plainly:** it verifies the **expression**,
transcribed from the RTL, not the RTL bytes. That gap is what the differential
lanes close — `texture_tmu_directed` (76 checks at each of three `FILT_LANES`
settings) and `texture_tmu_random` (3,749 bilinear samples, **476 exactly at a
rounding tie**) drive the real Verilator model against `zref::Tmu`, which
computes the four-weight law — together with the 13 sweep mutants aimed at this
arithmetic, **all 13 caught**.

**OPEN, and it is a regression in provability rather than in behaviour.** Worth
trying next, cheapest first: a different SMT engine (the lane pins
`smtbmc boolector`); or decomposing P1 into white-box lemmas on the DUT's own
`a_s`/`b_s` so the solver chains three easy equalities instead of one hard one.
Neither was attempted here.

`design/formal_runs.yml` now records this property as **`banked`**, not `green`
— on the precedent of `mem_sdram_refresh_bound` and `terrain_bake_delta`, whose
covers also pass while their bmc tasks do not finish. **The old `green` was not
transferable**: it was measured at `186546e` on the eight-product filter, which
no longer exists.

**A decision is left to the owner rather than taken here:** the CTest lane
`formal_texture_bilerp` is **left in place and will therefore fail the nightly
gate**. That is honest and noisy. The alternatives are to fix the proof, or to
take the lane out of the automated set as `mem_sdram_refresh_bound` was —
and removing it *silently* would hide a regression that a future fix should
restore.

**Proved (P1–P4):** the output is exactly the derived single-rounded weighted sum computed in a wide lane — which catches all three named one-LSB traps at once and, because that wide law is compared against an **8-bit** output, also proves the weighted sum can never leave the field; the weights are a **partition of unity**, `Σw = 65,536` exactly, which together with P1 makes a flat footprint filter to itself by arithmetic; `fu = fv = 0` is the exact identity on `t00`, which is what makes nearest sampling the same datapath; and the 8-bit field holds with no clamp anywhere in the module.

**Covered:** an **exact rounding tie** firing (without it the exactness theorem also holds for a truncating filter on every non-tie input, which is almost all of them), the result landing strictly **inside** its own footprint (without which P1 would also hold for a filter that never left tap 0), both far corners, the 128/128 centre, and a flat footprint at a non-trivial fraction filtering to itself.

**What was written, run, and REMOVED because it does not close on this engine — recorded so nobody re-adds it blind:**

- **Convexity / no-overshoot** (`min(t) ≤ out ≤ max(t)`). A real theorem and the one most worth having, but genuinely **nonlinear**: the solver would have to know `t_i·w_i ≤ max·w_i`, which bit-blasting does not get cheaply.
- **Monotonicity in `fu`.** Needs a second DUT instance at `fu+1`, doubling the eight bit-blasted multipliers.

Measured on this kit (boolector, depth 2, 32-bit law lane): the exactness theorem alone closes in **339 s**, the four shipped laws in **741 s** standalone and **1,264 s** as the CTest lane (`formal_texture_bilerp`, Passed, both tasks) — which is why that lane carries `TIMEOUT 3600` and not the 2100 the other formal lanes use.

With convexity added the task ran past **25 minutes twice** without an answer. What is lost is the statement that the result stays inside its own footprint; the safety half of it (the sum never leaves the 8-bit field) survives through P1, and the rest is covered by the differential lanes and the mutation table. The gap is named here and in the harness rather than assumed away.

**And a budget stated in one file was enforced in another, which cost a run.** That `TIMEOUT 3600` is a CTest property; `tests/formal/mem_formal_lane.cmake.in` hard-coded the *wrapper's* `execute_process` timeout at **1800**, so the real budget was 1800 and the comment describing 3600's headroom described headroom the lane did not have. On 2026-08-23, with a Quartus fit and a mutation sweep competing for the machine, the lane died at **1,800.88 s and was reported as a FAILED proof**. It was not a failed proof — the cover task had already passed in 2 s and the bmc task was still solving. The wrapper timeout is now a variable (`ZHAO_FORMAL_WRAPPER_TIMEOUT`, default 1800 unchanged for all nineteen other lanes) and this lane sets **3300**, inside the 3600 so the wrapper still fails with a readable sby message before CTest kills it from outside.

**Not proved here:** the address generation, the wrap folds, the mip selection and its closed form, the format decodes, the palette pass, the cache handshake, the mode-error rules and the state machine — those are the differential lanes' and the mutation table's job.

## Synthesis / resource ceiling

Budget group `tile`. Device `5CSEBA6U23I7`, Quartus Prime Lite 17.0.2, virtual
pins, `-KeepWorkspace` evidence kept for every row. **Not in `fpga/files.qip`**
— this block is characterised on its own and has never been in a composed fit.

### The DSP frontier, measured (2026-08-23, RUN-20260823-1736)

`FILT_LANES` instances of `zhao_texture_bilerp`, with the four colour channels
time-multiplexed through them in `4 / FILT_LANES` passes:

All four rows below are constrained fits under the corrected SDC (clock **and**
I/O delays — see the next section, and `reports/QUARTUS_GOTCHAS.md` §9), so they
are comparable to each other and to the "before".

| `FILT_LANES` | products | passes | direct II | ALMs | regs | **DSPs** | **Fmax** |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| *(pre-rearch)* | *32* | *1* | *4* | *1,844* | *342* | ***28*** | *36.92* |
| 4 | 12 | 1 | 4 | 1,958 | 319 | **12** | 36.38 |
| **2 (default)** | **6** | **2** | **5** | **1,921** | **350** | **6** | **36.11** |
| 1 | 3 | 4 | 7 | 1,902 | 373 | **3** | 35.62 |

**Fmax is FLAT across the entire frontier — 35.6 to 36.9 MHz, a spread of 3.6% —
while DSPs fall 28 → 12 → 6 → 3.** That is the single most useful thing the frontier says: none of
these settings touches the cone that limits the block (see below), so the DSP
axis is very nearly free in time, and the choice between points is a choice
about *cycles*, not about clock.

**QUARTUS WILL NOT FUSE TWO MULTIPLIES, HOWEVER SMALL.** Twelve products of 9×9
and 18×9 fitted at **twelve** DSP blocks, not the five a Cyclone V's "three 9×9
*or* two 18×19" modes would allow — and `zhao_texture_bilerp` fitted **on its
own**, three products, at **3 DSPs and 60 ALMs** (it was 7 DSPs and 38 ALMs
before). Four measurements, one behaviour: the same tool behaviour this contract
recorded from the other side in 2026-08-21, when the four instances' *identical*
weight products were not shared either.

**Stated carefully, because the loose version is wrong and is what people plan
cuts from.** Each nonconstant `*` creates **one physical multiplier structure**,
and the tool will not merge two of them. But the **cost** of that structure
jumps discontinuously with operand width and signedness: `reports/QUARTUS_GOTCHAS.md`
§5 has the *same* `zhao_geom_lod` source costing **28** DSPs at 72-bit operands
and **18** at 64-bit, which is impossible if width were irrelevant, and a 33×33
signed product is several blocks rather than one.

So **the operator count is a LOWER BOUND**, exact only while every operand stays
inside one block's native width. That holds here (9×9 and 18×9) and did **not**
hold for the 25×25 form this replaced — which is why the cut removed both a
count *and* a width problem.

**The default is 2, and the argument is the campaign's own principle applied
carefully in both directions.** 12 misses the 6–9 target the DSP campaign set;
6 is the bottom of it. The cycle that buys it falls **entirely on the
direct-colour path** (II 4 → 5) and not at all on CLUT, because a palette is
never filtered and the CLUT path therefore never enters `ST_FILT` — and CLUT is
the demand-critical path, since terrain is CLUT8. `FILT_LANES = 1` is measured
(3 DSPs, 35.62 MHz) and deliberately **not** the default: it would spend two
more cycles of a rate already at 0.33× to save three DSPs below a target already
met, which is the same over-provisioning error the 28 DSPs came from, pointed
the other way. **If the census ever needs those three blocks more than the
direct-colour path needs its two cycles, it is one parameter away and already
measured.**

### THE Fmax COLUMN WAS MEASURING THE SAMPLE COUNTER

**Read `reports/QUARTUS_GOTCHAS.md` §9 before quoting any per-block Fmax,
including this block's own earlier rows.**

The per-block SDC constrains the clock and nothing else — no `set_input_delay`,
no `set_output_delay` — so TimeQuest excludes every pin-to-register and
register-to-pin path. This block's arithmetic runs **from `req_*` pins** and
**to `smp_*` pins**, so almost none of it was ever timed. Verified by
re-running `quartus_sta` on three kept workspaces, no re-fit:

| row | reported Fmax | the path that produced it |
| --- | ---: | --- |
| `@pre-rearch` | 199.72 MHz | `texture_samples_o[19] → [27]`, 4.818 ns |
| `FILT_LANES = 4` | 192.46 MHz | `texture_samples_o[3] → [21]`, 4.634 ns |

Both are the 32-bit saturating counter's carry chain. The `FILT_LANES = 2` row
reported 48.45 MHz for one accidental reason — `fres_r` is a real register, so
one filter output finally terminated somewhere TimeQuest would look, at
`q_fmt_r → decode16 → ch_pack → the channel mux → the filter → fres_r`,
**20.462 ns over 8 logic levels.** That is not a regression the multiplexing
introduced; it is the first time this block's arithmetic had been timed at all.

### And with I/O delays declared, the honest Fmax is 36 MHz — before AND after

`tools/quartus/run_block_fit.ps1` now emits `set_input_delay` /
`set_output_delay 0` against `clk` — *same clock, no external budget*, which is
optimistic about inter-block routing and exact about the logic inside the block.
Two fits under it, like for like, the "before" taken by checking `8e7f974`'s RTL
back into the tree so that only the arithmetic differs:

| | `@pre-rearch-io` | **shipping default (`FILT_LANES = 2`)** |
| --- | ---: | ---: |
| ALMs | 1,844 | **1,921** (+77, +4.2%) |
| registers | 342 | **350** |
| **DSP blocks** | **28** | **6** (−22, **−79%**) |
| **Fmax** | **36.92 MHz** | **36.11 MHz** (−2.2%) |
| worst path | `q_fv_r[1] → smp_a_o[1]`, 20.913 ns | `q_fmt_r[0] → smp_a_o[4]`, 21.432 ns |

**Same RTL as the 199.72 MHz row, same tool, same device; a factor of 5.4
between them, and the only difference is whether the SDC declared I/O delays.**
So the answer to "was this block holding `gpu_clk` down, like `SURFACE.STAMP`?"
is **yes** — 36.92 MHz is 37% of the 100 MHz constraint, within noise of the 32%
`SURFACE.STAMP` was holding it to. The first re-fit said 199.72 MHz and that
reading was wrong.

**And the DSP rearchitecture does not move it, because it is not the same cone
being cut.** Before and after are limited by the *same* path in kind — a
registered mode/fraction bit, through `decode16` and the filter, to `smp_a_o` —
and the factored form is **0.5 ns slower** on it, which is exactly what the
shape predicts: the old filter was four *parallel* multiplies into one adder
tree, the new one is serial (mult → add → sub → mult → add). Fewer, narrower
multipliers; a longer chain. It costs nothing that was not already lost.

**A number that must NOT be quoted as a fitted result.** Applying the I/O
constraints *post hoc* to the earlier `FILT_LANES = 2` database — one placed
with no I/O objective at all — reports `req_mode_i[8] → q_addr_r[55]` at
**37.004 ns**, the address generator. That is an **upper bound on an
unoptimised placement**, not a critical path: once the fitter actually has the
objective, the address generator comes down and the filter-to-output cone leads
at ~21 ns. The 37 ns figure is worth keeping only because of *what* it named:

> the address generator is a 48-bit shift, a wrap fold and a `(v << log2w) + u`
> in one combinational cone from `req_valid_i` to the latched address, which is
> **the longest path in the file**

— this section's own claim, unmeasured since the day it was written, and the
first evidence in either direction. It is a **near** miss rather than the
limiter, and it contains no multiply and never did, because `LOG2W`/`LOG2H` make
every step of it a shift.

**The honest fix for the 36 MHz is a pipeline register**, which is what this
section originally proposed and what the II = 2 rearchitecture under Target
throughput needs anyway. **Not built**, and named here rather than left for a
reader to discover.

### ALMs went UP, and the prediction that they would fall was wrong

+77 at the shipping default, +107 at `FILT_LANES = 4` against the old row. The
four 25-bit adder trees did go away; what replaced them is two 9-bit subtracts,
two 17-bit adds and a 27-bit add per channel, plus the channel multiplexer. The
removals were counted and the additions were not. At 1,921 of 41,910 ALMs it
buys 22 DSP blocks of a 112-block device, which is the trade this whole campaign
exists to make.

The `{17'd0, t00_i} * {8'd0, w00}` form this replaced declared a **25×25**
multiply where the honest need was 8×17 — `reports/QUARTUS_GOTCHAS.md` §5 in the
wild, four times over, and §5 measured that same class of slack costing
`zhao_geom_lod` ten DSP blocks.

### The history this supersedes, kept because it is the same lesson twice

This section used to say the block "has not been synthesized" and to predict
that a synthesiser would share the four instances' identical weight products, so
the real cost would be "4 weight multiplies + 16 texel multiplies, not 32". A
2026-08-21 note then recorded, in capitals, **THE FIT SAID IT DID NOT HAPPEN**:
`zhao_texture_bilerp` = 7 DSP, `zhao_texture_tmu` = 28 DSP, *4 × 7 exactly, with
no discount*. Twelve of the thirty-two products were bit-identical duplicates
and every one got its own silicon.

That note also sanctioned **hoisting the weights** into the TMU as the fix, and
called it "the most structurally invasive of the available DSP cuts" because it
changes a module's ports, its directed tests, its formal harness and this
contract. **Factoring superseded it**: it removes 20 of the 32 products where
hoisting would have removed 12, it changes no interface at all, and the formal
formal HARNESS needed no edit rather than having its partition-of-unity
obligation transferred to the TMU — though the bmc TASK stopped closing, which
is recorded under Formal properties and is not the same thing.

**And the 2026-08-23 fit is the same lesson a second time.** 2026-08-21 learned
that Quartus will not share two identical products. 2026-08-23 learned that it
will not pack two *different* small ones either. Both are the one rule stated
above: on this kit, DSP blocks are counted in `*` operators.

## Integration capture cases

None on hardware. **Not composed with TEXTURE.CACHE in RTL**: the ledger registers four TEXTURE blocks and none of them is "the composition"; registering one is a validator-gated ledger edit and is not this increment's. The TMU's lane models the cache port with configurable latency and stall patterns (the same choice `raster_fragment_dev.hpp` documents for the tile store, and for the same reason — it drives timings the real block would never produce), and TEXTURE.CACHE is verified against its own oracle in its own lanes.

**Integrated with RASTER.FRAGMENT, in simulation only, in BOTH directions, and neither needed a port change:**

- `tests/texture/texture_tmu_directed.cpp:test_clut_index_zero_and_the_fragment` takes samples out of the **RTL TMU** and runs them through `zref::FragmentPipeline` under `star_disc_masked`, asserting the alpha-test kill, the seven survivors' writes and each survivor's glow-tag strength.
- `tests/raster/raster_fragment_directed.cpp:test_texels_from_the_tmu` goes the other way: `zref::Tmu::sample` fills `frag_texel_rgb_i` / `_a_i` / `_idx_i` against a real CLUT8 star face and a real bilinear direct-colour ramp, and drives the **RTL fragment** through exactly the ports it already had.

**Not one line of `zhao_raster_fragment.sv` changed, and its driver grew no port.** The interface that block's phase-4 header described as "the clean interface TEXTURE.TMU fills in when it lands" fits as written. Verilator-only; no Quartus fit, no capture, never on hardware. Simulated is not synthesized and neither is on-hardware.

## Notes

Sample modes are spec constants; secondary decal modes are cut-order 7 (§26).

Deliberately not built in this block, so the next wave knows: no trilinear and no LOD derivation; no anisotropic (§26 refuses it); no block-compressed formats; no border colour; no texture writes; no second sampler of any kind (§26); no `mosaic_pick` port (it rides `req_base_i`); no `frame_tick` shadow latch or catalog-id binding; and no pipelining — the throughput shortfall is stated under Target throughput rather than hidden, with the II = 2 design that would close it written down beside it.

**And NOT non-power-of-two textures, which is the one exclusion on this list that is a resource decision rather than a scope one.** `docs/OWNER_DOCKET.md` measures that Sacrifice's creature textures are 256 wide with *arbitrary* height up to 799 — only **81 of 637 (12.7%)** are power-of-two in both axes, because each body-part texture is a vertical atlas strip whose height is whatever that part needed (`saxs.d:90`). The fix is an **asset-pipeline repack**, already docketed, and it is a fix rather than a workaround for a specific reason: `LOG2W`/`LOG2H` are what make texel conversion, the mip level offset and the row-major index all *shifts*. Arbitrary dimensions would put a **multiply on the per-sample address path** of the block whose entire 2026-08-23 rearchitecture was removing multiplies from the sample cone. Everything else Sacrifice ships is strictly power-of-two and square, and nothing exceeds 256×256.
