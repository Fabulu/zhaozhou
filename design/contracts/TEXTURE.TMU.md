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

**The proof did not have to move, and that is why this was chosen over the weight hoist this contract sanctioned in 2026-08-21.** `zhao_texture_bilerp`'s ports are unchanged, so `tests/formal/texture_bilerp.sby` and `texture_bilerp_fv.sv` needed **not one line changed**: P1 derives the four weights *in the harness* from free `fu_free`/`fv_free` and asserts `out == law`, so it now proves the **factored** form equals the four-weight law over all 2^48 inputs. Hoisting would have changed a module's ports, its directed tests, its formal harness and this contract to remove 12 products; factoring removes 20 and changes no interface.

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

## Mutation evidence (2026-08-18)

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

**Proved (P1–P4):** the output is exactly the derived single-rounded weighted sum computed in a wide lane — which catches all three named one-LSB traps at once and, because that wide law is compared against an **8-bit** output, also proves the weighted sum can never leave the field; the weights are a **partition of unity**, `Σw = 65,536` exactly, which together with P1 makes a flat footprint filter to itself by arithmetic; `fu = fv = 0` is the exact identity on `t00`, which is what makes nearest sampling the same datapath; and the 8-bit field holds with no clamp anywhere in the module.

**Covered:** an **exact rounding tie** firing (without it the exactness theorem also holds for a truncating filter on every non-tie input, which is almost all of them), the result landing strictly **inside** its own footprint (without which P1 would also hold for a filter that never left tap 0), both far corners, the 128/128 centre, and a flat footprint at a non-trivial fraction filtering to itself.

**What was written, run, and REMOVED because it does not close on this engine — recorded so nobody re-adds it blind:**

- **Convexity / no-overshoot** (`min(t) ≤ out ≤ max(t)`). A real theorem and the one most worth having, but genuinely **nonlinear**: the solver would have to know `t_i·w_i ≤ max·w_i`, which bit-blasting does not get cheaply.
- **Monotonicity in `fu`.** Needs a second DUT instance at `fu+1`, doubling the eight bit-blasted multipliers.

Measured on this kit (boolector, depth 2, 32-bit law lane): the exactness theorem alone closes in **339 s**, the four shipped laws in **741 s** standalone and **1,264 s** as the CTest lane (`formal_texture_bilerp`, Passed, both tasks) — which is why that lane carries `TIMEOUT 3600` and not the 2100 the other formal lanes use. With convexity added the task ran past **25 minutes twice** without an answer. What is lost is the statement that the result stays inside its own footprint; the safety half of it (the sum never leaves the 8-bit field) survives through P1, and the rest is covered by the differential lanes and the mutation table. The gap is named here and in the harness rather than assumed away.

**Not proved here:** the address generation, the wrap folds, the mip selection and its closed form, the format decodes, the palette pass, the cache handshake, the mode-error rules and the state machine — those are the differential lanes' and the mutation table's job.

## Synthesis / resource ceiling

Budget group `tile`. Estimate only — **this block has not been synthesized**; no Quartus fit, no timing closure, no device numbers, and it is deliberately not in `fpga/files.qip`. The shape is ~300 flops (the four latched addresses at 128 bits dominate, plus four 16-bit halfwords, the mode fields and a 32-bit counter), no RAM, and the multipliers: four `zhao_texture_bilerp` instances, each computing four 9×9 weight products and four 8×17 texel products. The **weights are identical across all four instances** (they depend only on `fu`/`fv`), so a synthesiser will share them and the real cost is **4 weight multiplies + 16 texel multiplies**, not 32 — a DSP question the fit will answer.

The weight computation is deliberately left inside `zhao_texture_bilerp` rather than hoisted into the TMU, even though hoisting would make the sharing explicit: the weights are part of what `tests/formal/texture_bilerp.sby` PROVES (`a_wsum`), and a module that took its weights as inputs would move that theorem out of the proved bytes. If the fit ever says the sharing did not happen, hoisting is the fix and the proof follows the weights.

> **THE FIT SAID IT DID NOT HAPPEN. Recorded 2026-08-21; the hoist is now sanctioned by the condition above, not yet done.**
>
> `reports/synthesis/zhao_block_fit.json`: `zhao_texture_bilerp` = **7 DSP**, `zhao_texture_tmu` = **28 DSP**. That is **4 x 7 exactly, with no discount** — the four instances did not share their weight products. The paragraph above predicted "4 weight multiplies + 16 texel multiplies, not 32"; the measurement is 32.
>
> The row is trustworthy despite the census being stale overall: **neither `zhao_texture_tmu.sv` nor `zhao_texture_bilerp.sv` has changed since the census commit `96c0394`** (checked file by file), so this measurement describes the current RTL exactly.
>
> All four instances are driven by the SAME `q_fu_r`/`q_fv_r`, so `w00..w11` are bit-identical across them: **12 of the 32 products are literal duplicates.**
>
> **The proof cost is smaller than this paragraph feared.** `a_wsum` is asserted on weights the HARNESS derives itself from free `fu_free`/`fv_free` (`tests/formal/texture_bilerp_fv.sv:130-135`), not on anything the DUT computes — it is a check on the derivation formula, and it survives a DUT that takes weights as inputs. What actually moves is the OBLIGATION that the weights are well-formed: today it is structural (the bilerp derives them and cannot be fed bad ones), and after a hoist it becomes a duty on the TMU. That is a real transfer and it is what "the proof follows the weights" has to mean in practice: the TMU would need its own partition-of-unity property.
>
> Not done yet because it is the most structurally invasive of the available DSP cuts — it changes a module's ports, its directed tests, its formal harness and this contract — while `RASTER.FRAGMENT`'s blend (landed) and `GEOM.SKIN`'s weight identity are self-contained. Sequenced accordingly, not dismissed.

Two things to watch, neither measured: the address generator is a 48-bit shift, a wrap fold and a `(v << log2w) + u` in one combinational cone from `req_valid_i` to the latched address, which is the longest path in the file; and the 32 multiplies are all in the sample cone. If either becomes critical the honest fix is a register between address generation and the cache request, which costs one cycle of the already-variable latency.

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
