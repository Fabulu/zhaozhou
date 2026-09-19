# Q007 answer — particle-antenna-occlusion

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 62d1a51ec0ec4b749b48a495367c041e
- when: 2026-09-19T17:38:25  seconds: 119  finish: stop  status: ok
- usage: {"completion_tokens": 9033, "completion_tokens_details": {"reasoning_tokens": 5949}, "prompt_tokens": 7497, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 16530}
- inputs: [{"input": "tools/reel/manafold_fx.h:1800-1870", "sha256": "1770d3018eb57e33", "chars": 3846}, {"input": "tools/reel/manafold_fx.h:2180-2210", "sha256": "ed6373731e01dd08", "chars": 1822}, {"input": "tools/reel/manafold_fx.h:3600-3647", "sha256": "dd7d74054937cef3", "chars": 3204}, {"input": "tools/reel/zhao_reel.cpp:3205-3290", "sha256": "7ef4d739e4310e0a", "chars": 4908}, {"input": "tools/reel/zhao_reel.cpp:3790-3835", "sha256": "35dee23e1f85c851", "chars": 2734}]
- kind: task

## Answer

## Q1 — `mana_push` / `ManaSplat` booleans at the fold/surge call sites

The `mana_push` signature (manafold_fx.h:1833-1836) orders its tail parameters as:

```
… int gain_pm, bool depth_test, bool pre, bool opaque=false, …
```

The `ManaSplat` struct (manafold_fx.h:1811-1831) stores them in the order `depth_test, opaque, pre, soft`, and the `push_back` at line 1839-1841 maps positionally to that struct layout.

At every fold/surge call site shown, the two booleans after the gain are `true, false`:

| Call site | Line | `depth_test` | `pre` |
|---|---|---|---|
| Surge mote halo | 2191-2192 | `true` | `false` |
| Surge mote core (opaque) | 2193-2194 | `true` | `false` |
| Surge endpoint S | 2205-2206 | `true` | `false` |
| Surge endpoint E | 2207-2208 | `true` | `false` |
| Fold mote halo | 3640-3641 | `true` | `false` |
| Fold mote core (opaque+soft) | 3642-3644 | `true` | `false` |

So: **the first boolean is `depth_test` (always `true`); the second is `pre` (always `false`).** None of the shown motes are currently routed through the pre layer.

---

## Q2 — Pre pass vs. post pass compositing

**Pre pass** (zhao_reel.cpp:3256-3281):
- Comment (3256-3258): *"PRE-compose layer — the pools the creature and its arms occlude … Each splat depth-tests at its own projected 1/w."*
- Filter: `if (!ms.pre) continue;` (line 3267) — only splats with `pre=true`.
- Each splat is projected (3268-3269) and stamped via `glow_splat(rgb, depth, …)` (3277-3279) with `ms.depth_test` forwarded.
- This block executes **before** the creature rasterise (creature instances are set up at 3287+).

**Post pass** (zhao_reel.cpp:3798-3809):
- Filter: `if (ms.pre) continue;` (line 3800) — skips splats with `pre=true`.
- Same projection + `glow_splat(rgb, depth, …)` call shape (3801-3808), also forwarding `ms.depth_test`.
- This block executes **after** the creature rasterise.

**Differences:**

| Aspect | Pre (3256-3281) | Post (3798-3809) |
|---|---|---|
| Order vs. creature | Drawn **before** creature compose | Drawn **after** creature compose |
| Depth buffer state at stamp time | Contains terrain/background only; creature not yet written | Contains terrain **+ creature** geometry |
| `ms.depth_test` effect | Tests against terrain depth; creature does not occlude because it isn't in the buffer yet | Tests against terrain **and** creature depth; a splat behind a stick is rejected |
| Blending / call shape | Identical `glow_splat(…, bloom=true, opaque, soft, opacity_pm)` | Identical |

The two passes differ **only** in *when* they run relative to the creature rasterise and *which* splats they consume. The per-splat call is the same.

---

## Q3 — Would `pre=true` preserve correct front/back ordering?

Reasoning strictly from the pipeline order shown:

1. **Pre pass (3265-3280):** the mote is projected and stamped into `rgb` (and `depth`, per the `glow_splat` signature at 3277). At this moment the creature/antenna is **not yet in the depth buffer**, so the depth test at `ms.depth_test` (true) only rejects the mote against terrain/background.

2. **Creature rasterise (3287+):** the creature (including antenna sticks) is drawn. Its fragments write to `rgb` and `depth`. Wherever a creature fragment lands, it **overpaints** the pre-pass splat pixels.

3. **Post pass (3798-3809):** irrelevant here because the mote was consumed in step 1.

Consequences:

- **Mote in front of an antenna stick (closer to camera):** the pre pass stamps the mote. The creature then rasterises the antenna over the same pixels, overpainting the mote. The mote is **hidden** even though it is nearer. **This violates the binding plan's requirement that front-facing motes still draw in front.**
- **Mote behind a stick:** same result — the antenna overpaints it. Correctly hidden.

"Draws before the creature" is **not** equivalent to a depth test between the mote and the creature. It is a **paint-order** relationship: the creature unconditionally overpaints whatever the pre pass left in the buffer (at the pixels the creature covers). Whether the pre-splat also *writes* its 1/w into the depth buffer (which would cause the creature's fragments to be depth-rejected where the mote is nearer) is **not shown** in the provided code — the `glow_splat` implementation is not in the input. If it does **not** write depth (the creature-glow comment at 3210-3214 suggests the body *does* overpaint its own pre-drawn glow), then depth is fully flattened for all pre splats and the front-mote case fails. If it *does* write depth, a front-mote could survive because the antenna fragment would be rejected. **This is the critical unknown for the experiment.**

---

## Q4 — Why do motes visibly "go through" the antenna today (post pass, depth_test=true)?

The post pass (3800-3808) does depth-test at the splat's **centre** 1/w. The most likely mechanisms visible in the code:

1. **Single-point depth test at the centre; the stamp is a 2-D circle of radius `r_px`.** The fold-mote halo radius varies from `kMoteHaloRPxMin` to `kMoteHaloRPxMax` (3609-3610) and the core is a further fraction of that (3642: `halo * kMoteCoreOfHaloPm / 1000`). The comment at 3616 notes the halo blobs are "∼7 px across." The antenna stick is thin. If the mote centre is just to the side of the stick and passes the depth test, the halo circle overlaps the stick's pixels and paints additive light over them. The depth test was only at the centre; the halo's outer pixels are not individually re-tested.

2. **Additive blending over an opaque stick.** The halo is additive (the default when `opaque=false` at 3640-3641: no `opaque` or `soft` flags). Adding the cyan halo colour on top of the antenna's rendered colour lightens it, making the mote look like it is *in front of* or *inside* the stick rather than behind it.

3. **`bloom=true` on `glow_splat`.** Both the pre (3278) and post (3807) passes pass `/*bloom=*/true`. If bloom extends the effective painted radius beyond `r_px` (not shown, but the parameter implies a wider faint pass), the halo's reach is even larger, increasing the chance of overlapping a thin stick.

4. **Mote depth is taken at the centre only; the stick is a 3-D volume.** The antenna has a near edge and a far edge in screen space. A mote whose centre is in front of the antenna's near edge passes the test, but the halo's pixels that fall over the antenna's far edge (which is actually *behind* the mote centre) still paint. Conversely, a mote whose centre is just behind the antenna's far edge fails the test entirely, even though part of its halo is in front of the near edge.

Mechanism 1 is the dominant one: the halo radius exceeds the stick width, and the depth test is a single boolean at the centre.

---

## Q5 — Narrowest A/B toggle to try first

**Named toggle:** `ZHAO_U02_MOTE_PRE` (env var, default off), checked at the fold/surge `mana_push` call sites. When set, the `pre` argument flips from `false` to `true` for:

- Surge motes: lines 2191, 2193, 2205, 2208
- Fold motes: lines 3640, 3642

This is the exact reuse of `ManaSplat::pre` the binding plan calls for, with a single gate so the A and B builds differ only in this one flag.

**What to look at in the render to accept or reject:**

- **Accept path:** orbit the camera so a fold/surge mote passes *behind* an antenna stick. In the B build the mote should disappear behind the stick and re-appear when it passes in front. If it does, and motes that are clearly in front of the creature body still draw in front, the pre layer is working without flattening depth → ship.
- **Reject path:** if *all* motes near the antenna are hidden regardless of whether they are in front or behind (i.e. the creature unconditionally overpaints every pre-splat), the depth is flattened → reject, and the single-point-centre depth test in the post pass (mechanism 1 in Q4) becomes the target for a follow-up (e.g. per-pixel depth re-test within the halo, or reducing halo radius below the stick width).

The key discriminator is the **front-mote case**: does a mote that is genuinely closer to the camera than the antenna stick still appear in front of it? If yes, `glow_splat` must be writing depth in the pre pass and the creature's depth test is rejecting its own fragments where the mote is nearer. If no, the pre pass does not write depth and the paint-order model flattens everything.

---

## CONTINUATION
- Done:
  - Q1 established: all six shown `mana_push` call sites (2191, 2193, 2205, 2207, 3640, 3642) pass `depth_test=true, pre=false`. The first boolean is depth_test, the second is pre (manafold_fx.h:1833-1836, 2191-2194, 2205-2208, 3640-3644).
  - Q2 established: pre pass (zhao_reel.cpp:3256-3281, filter `!ms.pre` at 3267) runs before creature rasterise; post pass (3798-3809, filter `ms.pre` at 3800) runs after. Both call the identical `glow_splat` with the same arguments; the only difference is pipeline position and which splats are consumed.
  - Q3 established: pre=true means paint-order, not depth-order, *unless* `glow_splat` writes depth. Whether it writes depth is not shown in the provided input. If it does not, front-facing motes are hidden (violates binding plan).
  - Q4 established: primary mechanism is single-centre depth test + halo radius (up to `kMoteHaloRPxMax`, ∼7 px per 3616) exceeding the antenna stick width, with additive blending painting over the stick's pixels.
  - Q5 recommended: env-var toggle `ZHAO_U02_MOTE_PRE` flipping `pre` to `true` at the six call sites.

- Open:
  - **Critical unknown:** does `glow_splat` write to the depth buffer (the `depth` parameter is passed at 3277/3807 but the implementation is not in the input)? This determines whether pre=true flattens depth or preserves it.
  - The actual pixel widths of `kMoteHaloRPxMin`, `kMoteHaloRPxMax`, `kSurgeRPx`, and the antenna stick are not shown.
  - Whether `bloom=true` in `glow_splat` extends the painted radius beyond `r_px` is not shown.
  - Whether there are additional `mana_push` call sites for fold/surge motes outside the provided line ranges is not shown.

- Next chunk: **Read the `glow_splat` implementation** (search in the same file or its includes) and determine (a) whether it writes `pm.s.d` into `depth[]` and (b) whether `bloom=true` widens the stamp beyond `r_px`. This single finding resolves the Q3 "does pre=true flatten depth?" question and tells whether the toggle in Q5 can be accepted or must be rejected. Inputs needed: the `glow_splat` function body, the `kMoteHaloRPxMin/Max` and `kSurgeRPx` constant values, and the antenna stick's rendered pixel width (or its geometry radius in world units).

## Coordinator verdict

**partial** — spot 4: booleans (depth_test,pre) OK; pre/post order OK; pre=true flattens depth - CONFIRMED by coordinator: glow_splat never writes depth (fx.h:3933-3990), so creature overpaints every pre mote incl. front ones -> plan's pre-layer A/B predicted to fail; Q4 mechanism 1 REFUTED: depth test is PER PIXEL (fx.h:3952) at the centre depth, not centre-only. Real cause likely flat disc at centre depth sliced by the round stick surface; narrow fix = fade mote opacity near antenna centreline.
