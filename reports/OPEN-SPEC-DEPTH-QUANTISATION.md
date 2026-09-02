> # SUPERSEDED — 2026-09-02
>
> **This question is closed, and asking it was itself the error.** Three depth
> profiles were already ruled, generated and proved. Owner ruling R1, saved in
> `reports/OWNER-RULINGS-BUILDABILITY-20260902.md`:
>
> ```
> profile 0 WORLD_LONG      wmin 1.0 m     wmax 16,384 m  SCALE 2^40  d(wmax) 1024
> profile 1 WORLD_STANDARD  wmin 0.5 m     wmax  8,192 m  SCALE 2^39  d(wmax) 1024
> profile 2 CLOSE           wmin 0.25 m    wmax  2,048 m  SCALE 2^38  d(wmax) 2048
> profile 3                 reserved and refused until re-proved
> ```
>
> For all three: `d(wmin) = 0xFFFFFF` exactly; depth monotonic non-increasing;
> far value non-zero; **`wmax` is a depth clamp, NOT a far clipping plane**;
> scale generated from the reciprocal law, never hand-entered.
> `SetView.flags[1:0]` selects. Zero remains `WORLD_LONG`.
>
> **What is left is mechanical, not a decision:** emit the constants from
> fixgen; use `zref::depth_of` as the oracle; carry the profile through
> capture/replay; wire GEOM.PROJECT; run the endpoint and monotonic proofs.
>
> The document is kept because the derivation below is still the reasoning that
> the ruled numbers satisfy — but **nothing here is an open question**, and a
> reader who treats it as one will re-ask something already answered. That is
> the specific failure this banner exists to prevent: the question was carried
> into a published spec-questions page and put in front of the owner a second
> time.

# Open spec question: `wmin`, `wmax` and `scale` for `invw24`

**This is a decision request, not a proposal.** It blocks GEOM.PROJECT's
attribute carry, which is the last piece of step 6 of
`reports/RENDERER_ARCHITECTURE.md`. Everything either side of it is built.

---

## The hole, exactly

`spec/qformats.md` §8 defines depth as:

```
w: fx16 clamped to [wmin, wmax]; r = rcp_u24(normalize(w))
d = rescale(r · scale) round-half-up, saturate 0xFFFFFF   // w == wmin => d == 0xFFFFFF exactly
test: pass <=> d_new > d_old (strict; ties fail); clear value = 0
```

**`wmin`, `wmax` and `scale` have no numeric definition anywhere in the repo.**
They appear only in this paragraph and in one fog cross-reference at §8's linear
law. `grep` over `spec/`, `reference/include/zref/` and `fpga/rtl/` finds no
constant, no table entry, and no generated value.

Everything else in the chain is closed:

* `rcp_u24` is frozen, exhaustively hashed, and now exists in RTL
  (`zhao_raster_rcp24`, verified against `RCP24_FULL_HASH`).
* `invw24`'s format is U 0.0.24, larger is closer, clear value 0
  (`zref_earlyz.hpp`, `zref_fragment.hpp`).
* The plane interpolation of `invw24` is built and exact
  (`zhao_geom_attrsetup`, `zhao_raster_attrinterp`, `zhao_raster_attrdiv`).
* The per-pixel recovery that CONSUMES `invw24` is built and exact
  (`zhao_raster_perspuv`).

The one missing link is how a projected `w` becomes the `invw24` all of that
operates on.

## Why I am not choosing it

These three numbers are **the depth range of the console**. Together they decide:

* the **near plane** — `wmin` is where geometry starts being drawn, and it is
  also the value that pins `d == 0xFFFFFF` exactly;
* the **far plane** — `wmax` is where everything collapses to the same depth,
  and on 8 km maps that is a gameplay-visible distance, not a technicality;
* the **depth resolution profile** — `scale` decides how the 24 bits are spent
  across that range, which is what determines whether two terrain sheets a metre
  apart at 3 km still resolve, or z-fight.

A wrong choice here does not fail a gate. It produces a picture that is subtly
wrong at distance, and because `invw24` is baked into every golden capture CRC,
changing it later moves every one of them.

There is also a specific reason this project should not let me pick it: the near
and far planes interact with the **8 km map**, the **deep keel and dropoff**, the
**two skyboxes** and the **god beams**, none of which I have measured and all of
which the owner has named as requirements. This is a "measurement can remove a
bias, it cannot choose a value" case, and there is not even a bias to remove yet.

## What is needed

Three numbers, or a rule that produces them:

1. **`wmin`** — fx16, view-space forward distance. The near plane.
2. **`wmax`** — fx16. The far plane. Should be at least the map diagonal if
   distant terrain is meant to depth-test correctly.
3. **`scale`** — and the shift `rescale` uses with it, so that
   `w == wmin` gives exactly `0xFFFFFF` and `w == wmax` gives a sensible
   non-zero floor.

The third is constrained by the other two — the pin at `wmin` fixes one end —
so in practice this may be two numbers and an arithmetic check rather than
three free choices.

## What happens meanwhile

Nothing else in step 6 is blocked. GEOM.PROJECT's attribute carry is the only
piece that needs it, and the pieces around it are done, so this lands as one
small block once the numbers exist. Work continues on TEXJOIN and the
integration path.

If a decision is slow, the fallback that does **not** guess is to make all three
**configuration registers** rather than constants — the console would then ship
with the choice deferred to the runtime, at the cost of the depth pipeline no
longer being frame-constant-foldable. That is a real cost and it should be a
deliberate choice too, not a default I take because nobody answered.
