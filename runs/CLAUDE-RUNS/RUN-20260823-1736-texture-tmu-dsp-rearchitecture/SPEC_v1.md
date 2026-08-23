# SPEC v1: TEXTURE.TMU — 28 DSPs to 6–9, and the throughput shortfall the contract already admits

**Run ID:** RUN-20260823-1736
**Created:** 2026-08-23 17:36 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Take `zhao_texture_tmu` from a measured **28 DSPs** to the derived target of
**6–9**, and report **Fmax** and **samples/frame** against the derived demand of
**850,000 samples/frame** (`docs/OWNER_DOCKET.md`, "THE THREE DEMAND NUMBERS",
entry 2).

The demand is derived from Sacrifice, not from the ledger: terrain is layered
**tile + detail + lightmap** (`sacmap.d:136-174`), so ≥3 samples per terrain
pixel; at 92,160 pixels (Z60) with 3x overdraw that is **829,440 samples/frame
= 0.50 samples/clock**, rounded up to 850,000 for headroom. **One sample every
two clocks, not one per clock.**

No behavioural change. Every sample this block produces must stay **bit-identical**
to `zref::Tmu`, which is the same thing as saying the contract's derived bilinear
law, its half-texel bias, its wrap folds, its mip closed form, its format decodes
and its four mode-error rules are all untouched.

---

## Where the 28 DSPs are. Answer: ALL of them are in `zhao_texture_bilerp`, and this is measured, not inferred

The run brief's second lesson is "look for multipliers in the boring places",
because SURFACE.STAMP's 28 were *all* in coverage geometry and two of them were
per-stamp constants. **That lesson does not apply here, and the reason it does
not is worth stating rather than skipping.**

Checked by regex against the RTL, not by intention:

    grep -n " \* " fpga/rtl/texture/zhao_texture_tmu.sv     -> nothing
    grep -n " \* " fpga/rtl/texture/zhao_texture_bilerp.sv  -> 8 lines

The only `*` in `zhao_texture_tmu.sv` at all are `32*k` and `16*k` inside
`+:` part-selects — constant-folded loop indices, not arithmetic.

And the shipped fit agrees to the digit: `reports/synthesis/zhao_block_fit.json`
records `zhao_texture_bilerp` = **7 DSP** and `zhao_texture_tmu` = **28 DSP**.
**28 = 4 x 7 exactly, with no discount.** The contract already recorded this in
2026-08-21 as a prediction that failed:

> The **weights are identical across all four instances** ... so a synthesiser
> will share them and the real cost is **4 weight multiplies + 16 texel
> multiplies**, not 32 — a DSP question the fit will answer.
>
> **THE FIT SAID IT DID NOT HAPPEN.**

**The reason the block's addressing costs nothing is `LOG2W`/`LOG2H`.** The
contract makes texel conversion a *shift* (`u_raw << log2s`), the mip level
offset a repunit table and *one variable shift*, and the row-major index
`(v << log2w) + u` another shift. Every one of those would be a multiply if
texture dimensions were arbitrary. **This is exactly why the brief forbids
non-power-of-two support**, and the forbidding is load-bearing rather than
stylistic: it is the reason there is nothing to find in the boring places.

### The eight products, and their declared widths

Per `zhao_texture_bilerp` instance, all combinational:

| # | expression | declared operands | honest operands |
| ---: | --- | --- | --- |
| 1-4 | `w00 = iu*iv`, `w10 = fu*iv`, `w01 = iu*fv`, `w11 = fu*fv` | 9 x 9 | 9 x 9 |
| 5-8 | `p00 = {17'd0,t00_i} * {8'd0,w00}` (and 3 more) | **25 x 25** | 8 x 17 |

Four instances x 8 = **32 products**, of which **12 are literal duplicates**
(the four weights are computed four times from the same `q_fu_r`/`q_fv_r`).

**Products 5-8 are `QUARTUS_GOTCHAS.md` §5 in the wild.** The zero-extensions
`{17'd0, t00_i}` and `{8'd0, w00}` were written to make the widths line up for
the addition, and they hand Quartus a **25x25 multiply where the honest need is
8x17**. §5 measured that exact mistake costing `zhao_geom_lod` 10 DSP blocks.
Here it is four times over.

---

## The architecture

### Move 1 — THE FACTORED FORM. 32 products become 12, and it is bit-identical by algebra

The contract's law is the four-product sum:

```
w00 = (256-fu)(256-fv)   w10 = fu(256-fv)   w01 = (256-fu)fv   w11 = fu*fv
out = (t00*w00 + t10*w10 + t01*w01 + t11*w11 + 32768) >> 16
```

Factor the **exact integer expression**, without introducing a single rounding:

```
A = t00*(256-fu) + t10*fu  =  (t00 << 8) + (t10 - t00)*fu       1 product
B = t01*(256-fu) + t11*fu  =  (t01 << 8) + (t11 - t01)*fu       1 product
S = A*(256-fv)   + B*fv    =  (A   << 8) + (B   - A  )*fv       1 product
out = (S + 32768) >> 16                                          1 rescale
```

Expanding `S` gives `t00(256-fu)(256-fv) + t10 fu(256-fv) + t01(256-fu)fv +
t11 fu fv` — **term for term the contract's law**. Three products per channel
instead of eight, and 12 across the block instead of 32.

**THIS IS NOT THE FORM `spec/qformats.md` §3 REFUSES, AND THE DISTINCTION IS THE
WHOLE ARGUMENT.** The single-rounding law rejects "two lerps then a lerp"
because the textbook writes each lerp as a *rounded unit8 blend* — three
`rescale` calls, three roundings. What is above is the same **algebraic**
factoring with **no intermediate rescale at all**: `A` and `B` are exact
integers in a 17-bit lane, `S` is the exact 25-bit weighted sum, and there is
**exactly one** `(S + 32768) >> 16`. The refused form loses bits at each stage;
this one is the identical integer.

Consequences that follow for free and are the reason to prefer this over
hoisting the weights (the fix the contract sanctioned in 2026-08-21):

- **`zhao_texture_bilerp`'s ports do not change.** So `tests/formal/texture_bilerp.sby`
  and `texture_bilerp_fv.sv` need **not one line changed**, and P1-P4 keep
  meaning exactly what they meant. P1 (`outw == law`) computes the law in the
  harness from free `fu_free`/`fv_free`; it is a statement *about the shipping
  module's output*, so it proves the factored form equals the four-product law
  over all 2^48 inputs. **The proof does not follow the weights — it never
  depended on them.**
- The contract's 2026-08-21 note said hoisting "changes a module's ports, its
  directed tests, its formal harness and this contract", and called it "the most
  structurally invasive of the available DSP cuts". **Factoring is strictly
  better: it removes more products than hoisting would (12 vs 20) and changes
  no interface at all.**
- The 12 duplicate weight products are not shared, they are **gone**. There is
  no `w00` any more.

Honest widths, stated because §5 says slack is not free:

| value | range | lane |
| --- | --- | --- |
| `t10 - t00` | [-255, 255] | **signed 9** |
| `fu`, `fv` | [0, 255] | **signed 9** (zero-extended u8) |
| `(t10-t00)*fu` | +/-65,025 | signed 17 |
| `A`, `B` = `t*(256-f) + t*f` | **[0, 65,280]** | unsigned 17 |
| `B - A` | +/-65,280 | **signed 17** |
| `(B-A)*fv` | +/-16,646,400 | signed 25 |
| `S = (A<<8) + (B-A)*fv` | [0, 16,711,680] | unsigned 25 |
| `S + 32768` | <= 16,744,448 | 25 |

So the multiplies are **two 9x9 and one 17x9 per channel** — against today's
four 9x9 and four 25x25.

### Move 2 — FILT_LANES, the frontier axis: 4 / 2 / 1 channel lanes

`zhao_texture_bilerp` stays **one channel, combinational, unparameterised** —
the module the formal harness proves. The TMU instantiates **`FILT_LANES`** of
them and time-multiplexes the four channels (R, G, B, A) through them in
`4 / FILT_LANES` passes:

| `FILT_LANES` | instances | products | filter passes |
| ---: | ---: | ---: | ---: |
| 4 | 4 | 12 | 1 |
| **2** | 2 | **6** | 2 |
| 1 | 1 | 3 | 4 |

`FILT_LANES = 2` is the docket's own prediction — "about 5-6 DSPs at half rate"
— reached by the route the docket named. The default is chosen **after** the
frontier is measured, per the standing ruling that a met demand is not to be
exceeded.

The frontier is also **coverage**, per the ruling: a mutation that damages the
pass counter or the channel mux is invisible at `FILT_LANES = 4` (one pass, the
mux degenerates) and visible at 2 and 1. SURFACE.STAMP's S03/S04 were exactly
this shape.

### What is NOT done, and why — the ruled-out branches

- **NOT hoisting the weights out of `zhao_texture_bilerp`.** Sanctioned by the
  contract, superseded by Move 1: factoring removes 20 of the 32 products that
  hoisting would have kept, and costs no interface change and no transfer of
  the partition-of-unity obligation.
- **NOT a parallel nearest path.** Forbidden by the contract and by charter §26.
  `FILTER_NEAREST` forces `fu = fv = 0`; in the factored form that gives
  `A = t00<<8`, `B = t01<<8`, `S = t00<<16`, `out = t00` **exactly** — the
  identity survives the refactor, and it is the same datapath and the same
  cycle count as bilinear.
- **NOT non-power-of-two textures.** Forbidden by the brief; the fix is an
  asset-pipeline repack, already docketed. Adding it would put a multiply on
  the per-sample address path of the block being cut *from* 28 DSPs.
- **NOT `(* multstyle = "logic" *)`.** `QUARTUS_GOTCHAS.md` §3: accepted and
  silently ignored, symptom is a DSP count that will not fall.
- **NOT shift-add multipliers.** `fu`/`fv` are 8 bits, so each product would be
  an 8-term shift-add; 12 of them is a large ALM bill to save DSPs the target
  already permits. `zhao_field_sin`'s §3 fix worked because its operand was
  **six** bits and there was **one** product.

---

## The throughput problem, which is a SEPARATE problem in a SEPARATE part of the file

**This must be reported whether or not it is fixed, because the run brief asks
for samples/frame against the 850,000 demand and the answer today is not close.**

The contract does not hide it — under "Target throughput" it says the ledger's
"1 sample per clock" is "**NOT met by this increment**" and measures the block at
**one sample per 4 clocks (direct colour) and one per 6 (CLUT)**. At the
compute budget of **1,666,667 clocks/frame** (`design/budgets/latency.md` §63 —
*not* the 251,520 raster period, which is 6.6x smaller and also called "gpu
cycles"):

| path | II today | samples/frame at 100 MHz | vs. 850,000 |
| --- | ---: | ---: | ---: |
| direct colour | 4 | 416,667 | **0.49x** |
| CLUT (the terrain path) | 6 | 277,778 | **0.33x** |

**And terrain — the source of the whole demand — is CLUT8.** So the demand-critical
number is the 0.33x one.

**The DSP problem and the throughput problem do not touch.** Every DSP is in the
filter; the filter is bypassed entirely for CLUT (`smp_rgb_o = q_clut_r ?
pal_dec[23:0] : {bl_r,bl_g,bl_b}`), which is the path that misses demand worst.
Cutting DSPs cannot help throughput and pipelining cannot help DSPs.

### The plan: two commits, in this order

**Commit A — the arithmetic.** Moves 1 and 2. Meets the DSP target. Throughput
unchanged. Fitted and scored on its own.

**Commit B — the pipeline.** The II=2 restructure below. Meets the demand.
Fitted and scored on its own.

**A is committed and pushed before B is started.** If B cannot be closed, the
assigned target still lands, and the gap is reported with a measured number and
a named design rather than a shrug. This ordering is deliberate and is recorded
here so that the choice is legible either way.

### The II=2 design, as far as it is designed before any RTL

The floor is the **cache access port**, not the arithmetic. `zhao_texture_cache`
accepts one access per clock (`acc_ready_o = ... && (!s1_v_r || smp_ready_i)`,
line 331 — a 1-deep response pipeline). A CLUT sample needs **two** accesses,
texel then palette, and they are unavoidably serial because the palette address
is a function of the returned index. **Two accesses per sample through a
one-access-per-clock port is II = 2 exactly** — the same number the demand
derivation asks for, arrived at from the other end.

Schedule, with palette accesses taking priority so order is preserved
(T = texel access, P = palette access, subscript = sample):

```
c0  T0
c1  T1     (T0 returns; index0 registered)
c2  P0     (T1 returns; index1 registered)
c3  P1     (P0 returns; sample 0 complete)
c4  T2     (P1 returns; sample 1 complete)
...        period 4 for 2 samples  ->  II = 2
```

Two requests in flight, retiring **in order**. The three things this needs that
today's four-state machine does not have:

1. a **2-entry in-flight record** (fmt, clut, fu, fv, bytesel, nib, palbase,
   src_id, and the four returned halfwords — ~134 bits an entry; the 128-bit
   address bank is *dead* once the access is issued and is not duplicated);
2. an **issue arbiter** over one cache port, palette-before-texel;
3. an **in-order completion**, because a direct-colour request behind a CLUT one
   would otherwise finish first and the sample channel would go out of order.

`mode_error_o` keeps pulsing in the cycle **after acceptance**, which is what
`texture_tmu_dev.hpp` attributes it by; at II=2 acceptances are 2 cycles apart,
so attribution is unambiguous.

**The test harness's modelled cache has to change and that is a faithfulness
improvement, not a concession.** `texture_tmu_dev.hpp` today models a **strictly
one-outstanding** cache (`if (cac_busy) add(err, "a cache access while one was
outstanding")`) — which the **real** `zhao_texture_cache` is not. The model gets
the real block's 1-deep pipeline. Recorded here in advance so the change is not
mistaken later for a test being relaxed to fit an RTL bug.

---

## Predictions, written down before the measurements, so they can be wrong in public

1. **The pristine fit will report an Fmax well below 100 MHz.** The contract
   names the suspect: "the address generator is a 48-bit shift, a wrap fold and
   a `(v << log2w) + u` in one combinational cone from `req_valid_i` to the
   latched address, which is the longest path in the file", and separately "the
   32 multiplies are all in the sample cone". The shipped row has **no `fmaxMhz`
   field at all**, so like SURFACE.STAMP's it was fitted with no timing
   objective (`QUARTUS_GOTCHAS.md` §7). **Guess: 45-70 MHz.**
2. **`FILT_LANES = 4` lands at 4-8 DSPs.** 8 products of 9x9 and 4 of 17x9. A
   Cyclone V variable-precision block does three 9x9 *or* two 18x19, not a mix,
   so the plausible packing is 3 blocks of 9x9 plus 2 of 18x19 = **5**.
3. **`FILT_LANES = 2` lands at 2-4, `FILT_LANES = 1` at 1-2.** If two settings
   report the **same** DSP count, the parameter was ignored (`QUARTUS_GOTCHAS.md`
   §3) and the row is not to be believed.
4. **Fmax will IMPROVE at every setting**, because the deepest arithmetic in the
   sample cone goes from a 25x25 multiply feeding a four-input 25-bit adder tree
   to a 9x9 multiply feeding a 17-bit add feeding a 17x9 multiply. The mux that
   `FILT_LANES < 4` adds is 4:1 on 8-bit lanes and should not be the path.
5. **ALMs will fall**, because the four 25-bit adder trees go away and the
   replacement is two 8-bit subtracts and two adds per channel.

---

## Scope

**In Scope:**

- `fpga/rtl/texture/zhao_texture_bilerp.sv` — the factored arithmetic.
- `fpga/rtl/texture/zhao_texture_tmu.sv` — `FILT_LANES`, the channel mux, and
  (commit B) the II=2 restructure.
- `tests/texture/texture_tmu_dev.hpp` — the cache model, commit B only.
- `tests/texture/texture_tmu_directed.cpp` — the throughput assertion, which
  today measures cycles; it must measure the **derived demand**.
- `tests/CMakeLists.txt` — the `FILT_LANES` frontier targets.
- A mutation sweep, in the existing worktree, at all three `FILT_LANES`.
- `design/contracts/TEXTURE.TMU.md`, `design/blocks.yml`,
  `design/budgets/dsp.md`, `docs/OWNER_DOCKET.md`, `reports/REMAINING_BLOCKERS.md`.

**Out of Scope:**

- Non-power-of-two textures (brief; asset-pipeline repack is docketed).
- Trilinear, anisotropic, block compression, border colour, LOD derivation, a
  second sampler — all refused by charter §26 / the contract.
- `TEXTURE.CACHE`, `TEXTURE.MOSAIC`, `TEXTURE.AUX` — separate blocks, separate
  contracts. The cache is *read* here, not changed.
- Any game behaviour for particle simulation, compositor or 2D blocks
  (owner-reserved). Questions get docketed.
- `terrain_normals`, `terrain_project` — queued behind this run.
- Advancing `design/blocks.yml` maturity — ledger rule V16's business, not this
  run's, exactly as SURFACE.STAMP left it.

---

## Constraints

- **The weights sum to exactly 65,536.** In the factored form there are no
  weights, so the property becomes: `S` at `fu = fv = f` on a flat footprint
  `t` is exactly `t << 16`. Preserved by algebra; still proved by the formal
  harness's `a_wsum` (which derives its own weights) and `a_exact`.
- **`FILTER_NEAREST` takes the SAME datapath.** No parallel path, and the same
  number of passes.
- **The one-LSB traps stay aimed at**: truncate-instead-of-round, swapped
  weights (asymmetric fractions), `/255` instead of `/256`.
- Share arithmetic **within this subsystem only**.
- **Never hand-edit** a measured number into `reports/synthesis/zhao_block_fit.json`.
- Verify every fit was constrained: `Info (332111): 10.000 clk`, captured to
  `fit-evidence/` before the harness deletes the workspace.
- One Quartus at a time; **no RTL edits in the main tree while a fit runs**.
- Sweep runs in the worktree `C:\Programmieren\zencrifice\zhaozhou-sweep-stamp`,
  detached, with a verified **non-zero** mutant count.
- Report **both** numbers: Fmax and samples/frame.

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

- Do **not** try `(* multstyle = "logic" *)` — §3, silently ignored.
- Do **not** hoist the weights into the TMU — superseded by factoring, and it
  would move the partition-of-unity obligation out of the proved module.
- Do **not** widen operands "for safety" — §5, slack changes DSP decomposition.
- Do **not** put a module-scope `if` generate without `generate`/`endgenerate`
  — §8, three frontends accept it and Quartus does not.

---

## Open Questions

1. Does Quartus 17.0.2 pack two 9x9 products into one Cyclone V DSP block here,
   or does each product take one? Decides whether `FILT_LANES = 4` is 5 or 12.
   **Answered only by the fit.**
2. Is the 850,000 demand really all through *this* block, or does
   `TEXTURE.MOSAIC` / `TEXTURE.AUX` carry part of the terrain layering? The
   docket assigns it here. **Not resolvable in this run; docket it if the II=2
   design turns out to cost more than it is worth.**
3. `texture_samples` is claimed by four blocks in `design/blocks.yml`. Not this
   run's to reconcile (DEBUG.COUNTERS integration wave), same as SURFACE.STAMP.
