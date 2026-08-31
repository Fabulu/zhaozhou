# Depth profiles: proved, not yet wired

`tests/proofs/depth_profile_law.cpp` derives and proves the three profiles from
owner ruling 2026-08-31 #1. **The derivation is done. Nothing consumes it yet.**
This file says exactly what is left, so the gap between "proved" and "shipped"
is visible rather than assumed closed.

## What is proved

| profile | wmin | wmax | generated scale | d(wmax) |
|---|---|---|---|---|
| 0 `WORLD_LONG` | 1.0 m | 16,384 m | 1,099,511,627,776 = **2⁴⁰** | 1024 |
| 1 `WORLD_STANDARD` | 0.5 m | 8,192 m | 549,755,813,888 = **2³⁹** | 1024 |
| 2 `CLOSE` | 0.25 m | 2,048 m | 274,877,906,944 = **2³⁸** | 2048 |

For each: `d(wmin) = 0xFFFFFF` exactly, monotonic non-increasing over ~39,000
geometric samples and 200,000 consecutive raw units at the near plane, non-zero
floor, no intermediate wrap.

**The scale is a power of two in all three**, so the "multiply by scale" is a
shift. That is a property of these three near planes being powers of two in raw
units — a fourth profile with `wmin = 0.7 m` would not have it, and the
generator must not assume it.

    s  = smallest shift with (W >> s) < 2^24
    {r, k} = rcp_u24(W >> s)
    d  = rescale(SCALE * r, 48 + s - k), round-half-up, saturate 0xFFFFFF

`SCALE` is solved from the reciprocal's **actual output** at `wmin`, not from
the ideal `0xFFFFFF * Wmin` — that form gives `0xFFFFFE`, one short of the pin,
because `rcp_u24` carries up to 1 LSB and the pinned input is exactly where it
saturates.

## What is left, in order

1. **`tools/fixgen` emits the table.** Per profile: `wmin_raw`, `wmax_raw`,
   `scale`, and the vectors the proof already checks (pin, floor, monotonicity
   samples). The constants belong in `zref/generated/` beside the other frozen
   tables, not hand-written into RTL.
2. **`spec/qformats.md` §8 gains the profile table** and states that `scale` is
   generated. Today the section names `wmin`, `wmax` and `scale` and defines
   none of them, which is what blocked this for the whole wave.
3. **The ABI decision:** two reserved bits of `SetView.flags`, or a small
   additive view-depth command. Ruling 1 permits either and makes the audit the
   deciding factor. **This is the only part that is not mechanical** — it is a
   permanent command ABI, so Class C.
4. **`zref` gains `depth_of(w, profile)`** as the oracle, so RTL can be
   differentially tested rather than checked against a restatement.
5. **GEOM.PROJECT carries the attribute packet** and emits `invw24` through the
   above. This is the last piece of the renderer's step 6, and the only thing
   that was ever actually blocked.
6. **The profile is captured**, so a replay reproduces the depth mapping it was
   recorded under.

## What must not happen

* **Do not hand-write the scale into RTL.** It is generated; a hand-copied
  constant is how a wrong number becomes an unadjustable one.
* **Do not treat `wmax` as a far-clip plane.** It is a depth clamp. GEOM.PROJECT
  row 2 stays inert and the culler stays at five planes, so distant islands keep
  rendering and The Measure, fog and representation control what is visible.
* **Do not add a fourth profile without re-proving.** The proof is cheap and the
  power-of-two scale is a coincidence of the first three.
