# The attribute seam: the plane form IS the oracle, and step 6 is unblocked

> ## RESOLVED THE SAME DAY. Read this box; the analysis below is kept for its reasoning.
>
> I recommended checking, before deciding anything, whether some incremental
> form is bit-identical to the shipped oracle. **It is.**
>
>     tests/proofs/attribute_plane_equivalence.cpp
>     32,805 pixel-attributes over five triangle shapes, 0 mismatches
>
> The reasoning the analysis below missed: the oracle's edge functions step by
> CONSTANTS (`w0 += dw0_dx` in rast.cpp), so the 128-bit NUMERATOR
>
>     N(x,y) = w0*A + w1*B + w2*C
>
> is itself an exact integer plane — `N(x,y) = N0 + x*dNdx + y*dNdy`, with no
> rounding anywhere in the stepping. The division is applied to the stepped
> numerator exactly as the oracle applies it to the recomputed one, so the two
> agree bit for bit.
>
> **So `spec/qformats.md` and `rast.cpp` were never in conflict.** "Interpolate
> by plane equation" describes the NUMERATOR, which is a plane; the per-pixel
> divide stays. My reading below — that no plane form could match because
> division does not distribute over the increment — was wrong: nothing is being
> distributed, because the increment happens BEFORE the divide, not after.
>
> ### What that gives step 6
>
> `GEOM.ATTRSETUP` has a testable output format and it is exact:
>
>     per triangle, per attribute:  { N0, dNdx, dNdy }   plus the shared area
>     per pixel:                    N += dNdx (or dNdy per row), then ONE divide
>
> The plane half is **free and exact** — integer adds. The divide is the real
> cost and it is unchanged, but ruling 6's ordering is what pays for it: only
> `invw24` is needed BEFORE early-Z, so one divide per covered pixel; `u` and
> `v` are paid only by survivors; colour and alpha only when Gouraud is on.
>
> The seven-divides-per-pixel figure below is the worst case for a textured
> Gouraud triangle with every attribute live, not the common path.
>
> **Nothing in `zref` has to change and no golden capture moves.** Options 1 and
> 2 below are both withdrawn.

## BUILT AND MEASURED, same day

Both halves of the per-pixel law now exist and are exact against the oracle.

| block | what it does | gate |
|---|---|---|
| `zhao_geom_attrsetup` | emits `{N0, dNdx, dNdy}`, no divide | 16/16, 2,880 pixel-attributes reconstructed |
| `zhao_raster_attrdiv` | the divide, rounding exactly as `rast.cpp` | 7/7 |

**And the divider measures 36 clocks.** That is the number the whole textured
path now rests on:

    1,666,667 clocks / 36 = 46,296 attribute-pixels per frame, per divider

Against a terrain-primary component of 276,480 pixels needing at least `invw24`
before early-Z, **one divider is 6x short for depth alone**, before `u`, `v`,
colour or alpha, and before any other geometry in the frame.

### What that means, and what it does not

It does NOT mean the law is wrong -- the law is the reference's and it is now
reproduced bit for bit. It means the divide is the throughput wall of the
textured path, exactly where this file predicted it would be, and it is now a
measured 36 rather than an estimate.

The levers, in the order the Field engine's experience suggests trying them:

1. **Fewer divides, not faster ones.** Early-Z before texture means only
   survivors pay for `u` and `v`; a flat-shaded or untextured triangle pays for
   none; `invw24` is the only one every covered pixel needs. Ruling 6's ordering
   is worth more here than any arithmetic.
2. **A shorter divider.** 36 clocks is 33 restoring-division iterations plus
   handshake. A radix-4 divider halves the iterations for roughly double the
   per-step logic; a fully pipelined array reaches one result per clock at 33
   stages of area. Both are real options and both need a fit.
3. **More dividers**, as a tagged service with the count a parameter -- the
   shape the Field services ended up with, so throughput becomes a sweep rather
   than a rewrite.

**Do not pick one from this file.** The Field lane's lesson was that the wall is
whichever resource refuses, and nothing has yet measured what a real frame asks
of this divider once early-Z is in front of it. `busy_clocks_o` and `divides_o`
are on the block for that measurement.

## The original analysis, kept for its reasoning


Written 2026-08-30 while starting step 6 of `reports/RENDERER_ARCHITECTURE.md`
(the attribute-bearing geometry seam). It is the gate on everything textured, so
the blocker is worth stating exactly.

**No RTL changed.** This is a reading of the shipped oracle against the shipped
spec, and they do not say the same thing.

---

## What the spec says

`spec/qformats.md` §8:

> "**Perspective UV:** interpolate `u_over_w`, `v_over_w` (S 8.24) and `invw24`
> by plane equation; per pixel
> `u = rescale((s64)u_over_w · rcp_u24(invw24_interp))`."

and for depth, "1/W is linear in screen space; it interpolates with the same
plane-equation machinery".

Read on its own, that describes what the reviewer's ruling 6 asks for: a setup
stage producing `origin`, `d/dx` and `d/dy`, and a raster stage that evaluates
the plane at the tile's first pixel and steps it.

## What the shipped oracle actually does

`reference/src/zrender/rast.cpp`, the per-pixel body:

    u = div_rhu_s128(w0*A.u + w1*B.u + w2*C.u, area);
    v = div_rhu_s128(w0*A.v + w1*B.v + w2*C.v, area);
    d = div_rhu_s128(w0*A.d + w1*B.d + w2*C.d, area);
    a = div_rhu_s128(...);  cr = ...;  cg = ...;  cb = ...;

That is **not** a plane equation. It is a normalised barycentric combination
with a **128-bit numerator divided by the triangle's area, rounded half-up,
once per attribute per pixel**. The edge functions `w0/w1/w2` are stepped
incrementally and exactly (`w0 += dw0_dx`), so the NUMERATOR is cheap — the
division is the law.

`zref` is REFERENCE_COMPLETE for this path, so that division is not an
implementation detail of a software bootstrap. It is the definition of every
interpolated value the console will ever produce.

## Why that matters, in one line

**No incremental or plane form is bit-identical to it, because integer division
does not distribute over the increment.** `div(N, area)` and
`div(N + dN, area)` differ from `div(N, area) + div(dN, area)` in general, and
the rounding is half-up on the quotient, not on the numerator. Precomputing
`d/dx = dN/area` and stepping it accumulates a different value.

So a `GEOM.ATTRSETUP` that emits `origin, d/dx, d/dy` and a `RASTER.INTERP` that
steps them **cannot** be differentially tested against `zref::` as it stands.
Building them and then discovering that is the expensive order to find out.

## What it costs to implement the law as written

Per pixel, per attribute: one 128 / 64 divide with round-half-up. A textured
Gouraud triangle interpolates `u, v, d, a, cr, cg, cb` — **seven divides per
pixel**. At one pixel per clock that is seven divider pipelines, or one shared
divider at an initiation interval that caps fragment throughput at 1/7.

For scale: the Field engine's `zhao_field_isqrt` is 32 fixed iterations and
could not be pipelined without being rewritten, and its cost drove that whole
lane's architecture. A 128-bit divide is not cheaper.

## The three ways out, and none of them is free

1. **Change the law to something implementable, and accept that every golden
   capture moves.** An exact plane form in higher intermediate precision is the
   obvious candidate. The cost is that `zref` is the law: every capture CRC in
   `captures/golden/` shifts, and the reference has to change first, with the
   RTL following it rather than the other way round.
2. **Keep the law and pay for it.** A tagged divide service, exactly like the
   Field engine's long-op services, with a measured initiation interval. Then
   the fragment rate is bounded by divides per pixel, and the number of
   interpolated attributes becomes a throughput decision rather than a free
   one. Ruling 6's "only survivors pay the reciprocal" helps a great deal here,
   because early-Z rejects before the divides.
3. **Prove some incremental form IS identical over the legal input range.** I do
   not believe it is, but it is checkable rather than arguable, and a bounded
   exhaustive check over realistic areas and attribute ranges would settle it
   for the cost of an afternoon. That is the cheapest thing to try FIRST,
   because if it holds, options 1 and 2 both disappear.

## Recommendation

**Try 3, then decide between 1 and 2 with the answer in hand.** Do not build
`GEOM.ATTRSETUP` before that: its output format IS the choice, and writing it
down in RTL freezes the decision by accident.

This is also the reviewer's own instruction from ruling 6, arriving at the
opposite conclusion to the one it expected. The warning was that the software
scanline form must not "silently become the silicon law" because it is an
approximation. It is not an approximation — it is *more* exact than the hardware
can afford, and it is the plane form that would be the change.
