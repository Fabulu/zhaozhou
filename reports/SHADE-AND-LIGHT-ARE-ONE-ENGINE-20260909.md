# The contract of the next block predicted the mistake I had just made

2026-09-09. I built `TERRAIN.SHADE` an hour ago — 0 DSP, 4,142/4,142 bit-exact,
verified myself. Then I went to build `GEOM.LIGHT`, read its contract first, and
found this at `design/contracts/GEOM.LIGHT.md:118-122`:

> "**So there is nothing for this block's oracle to call, and the contract's
> instruction cannot currently be obeyed.** Writing the oracle as `normal ->
> ndot -> isqrt -> divide` would be a second implementation of the ratified
> arithmetic — **the exact failure this contract was written to prevent, and the
> one that shipped in September's terrain shade header.**"

`normal -> ndot -> root -> divide` is precisely what `zhao_terrain_shade.sv`
implements. The contract of the very next block warned against building it as a
separate engine, named the terrain shade header as where that failure had
*already* happened once, and I dispatched an architect to build exactly it.

## Why the two blocks are one engine

`GEOM.LIGHT.md`'s own diagram makes terrain a *client*, not a peer:

    ... normal producers ...
    '- TERRAIN.NORMALS face normal   (terrain)
                |
          world normal
                |
            GEOM.LIGHT   <-- this block
                |
          vertex RGB

> "One block, one law, **three different normal producers**." And its exclusions:
> "**No normal production.** Three different producers feed it; that is the point
> of the split."

So the intended architecture is one lighting engine with three feeds, and terrain
is one of them. `TERRAIN.SHADE` occupies that slot for terrain alone.

## The two contracts were written the same day and do not reference each other

Both are dated **2026-09-03** and both come from the same audit
(`BORING_3D_FUNDAMENTALS_AUDIT.md` R2). `GEOM.LIGHT.md` mentions terrain shade
exactly once — in the warning quoted above. **`TERRAIN.SHADE.md` does not mention
`GEOM.LIGHT` at all** (grep: zero occurrences).

Two contracts, same day, same finding, same ratified law, and **neither says which
one owns the arithmetic.** That is how the duplication got authored, and it is the
same shape as the projector: two blocks legitimately needing the same maths, with
nothing in either document assigning ownership.

## The fix is written down, and it is a refactor rather than a rebuild

`GEOM.LIGHT.md:124-132` gives it, calling it "the D-1 refactor again, one level
down":

    shade_from_world_normal_unclamped(nx,ny,nz, lx,ly,lz, SatLedger*)   <-- NEW core
    shade_flat_tri_dir_unclamped(a,b,c, l, L)
        = shade_from_world_normal_unclamped(face_normal(a,b,c), l, L)   <-- wrapper

and it names the acceptance criterion, which is the good part:

> "D-1 made `shade_flat_tri_dir` a bit-identical wrapper around an unclamped
> primitive, and **the goldens not moving is how it was known to be correct.**"

In RTL the same move applies: `zhao_terrain_shade` already contains the ndot,
root and divide. It needs a **named parameter or a second input mode** so the
`face_normal` stage can be bypassed and a world normal supplied directly. Then
one engine serves terrain's face normals, the skinned-creature normal producer,
and the third producer, exactly as `GEOM.LIGHT` specifies.

## What is NOT wrong

**`TERRAIN.SHADE`'s arithmetic is not wasted and should not be reverted.** It is
bit-exact against the ratified law over 4,142 checks including degeneracies, it
costs 0 DSP and 1 M10K, and its quarter-square product ROM and hidden-root
scheduling are the expensive thinking. The engine is right. What is wrong is that
it is **reachable only from terrain**, and that a second copy would have been
authored for the other two producers if I had carried on.

Nor is this a case of a block that should not exist: terrain genuinely needs
lighting, `GEOM.LIGHT` is `SPECIFIED` rather than `REFERENCE_COMPLETE` and had
no oracle to build against, and TERRAIN.SHADE was the buildable half. **The
sequencing was defensible; not reading the sibling contract was not.**

## Consequences, ranked

1. **Do not build `GEOM.LIGHT` as a new engine.** Extend `zhao_terrain_shade`
   with a world-normal input mode and rename or wrap it as the shared lighting
   core. One block, three producers.
2. **The reference needs the D-1 split first**, because the RTL's oracle is the
   C++ function: add `shade_from_world_normal_unclamped` and make
   `shade_flat_tri_dir_unclamped` a bit-identical wrapper. **The goldens not
   moving is the proof**, which makes this a cheap, checkable refactor rather
   than a risky one.
3. **Both contracts need a cross-reference**, so the next reader cannot repeat
   this. `TERRAIN.SHADE.md` in particular must say that its engine is the shared
   one and point at `GEOM.LIGHT`.
4. `GEOM.LIGHT`'s maturity stays `SPECIFIED` until the reference split lands —
   the ledger's V6 check refuses an unwritten reference from
   `REFERENCE_COMPLETE` up, and that gate is doing its job here.

## The transferable part

I have spent today finding places where somebody built a thing that already
existed, and wrote a detector for it. **The detector cannot catch this one**, and
it is worth being precise about why: `uncashed_cheques.py` finds modules that are
*built and uninstantiated*, and it finds fit rows that are *stale*. It has no
notion of two blocks that will each need the same arithmetic, because neither is
uninstantiated and neither row is stale — the duplication would be **authored**,
not inherited.

What catches it is one habit, and the habit is cheaper than the tool: **before
building a block, read the contract of every block that consumes or produces the
same quantity.** Here that was a single file, and its line 118 had the answer
written out with the previous instance of the same mistake already named in it.
