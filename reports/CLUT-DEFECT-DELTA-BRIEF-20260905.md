# The two CLUT defects: what fixing them would change

**Date** 2026-09-05
**Status** proposal awaiting an owner decision. **No RTL has been changed.**
**Context** docket D23; `TERRAIN-MIP-TWO-LEVEL-BLEND-ARCHITECTURE-20260905.md`

The mip architecture gates these two behind *"a reviewed before/after delta"*
because they change star and sky bytes, and the owner's standing constraint is
that terrain work must not silently alter materials relying on raw
palette-index semantics. This is that review, written before touching anything.

Both defects are verified in source. Neither is fixed.

---

## Defect A — the 565 to 888 expansion zero-fills

**This one really is a one-line change.**

`reference/include/zref/zref_texture.hpp` line 15 names the expansion outright,
and `zref::sky::rgb565::to_rgb888` replicates the high bits:

```cpp
r = (r5 << 3) | (r5 >> 2);   //  31 -> 255
g = (g6 << 2) | (g6 >> 4);   //  63 -> 255
b = (b5 << 3) | (b5 >> 2);
```

`zhao_texture_island_top.sv` appends zeros instead:

```systemverilog
{pal_lu_rgb565[15:11], 3'b000, pal_lu_rgb565[10:5], 2'b00, pal_lu_rgb565[4:0], 3'b000}
```

### What changes

Every palette-sourced colour gets **brighter**, by an amount that scales with
the value: exactly 0 at the bottom of each channel's range and +7/+3/+7 at the
top. Full white goes from `248, 252, 248` to `255, 255, 255`.

| what | before | after |
|---|---|---|
| channel value 0 | 0 | 0 |
| mid (r5 = 16) | 128 | 132 |
| max (r5 = 31) | 248 | **255** |

**Who is affected:** everything that samples a palette. That includes stars and
sky, which is precisely why this is gated rather than folded into a terrain
change. Nothing gets *darker* and nothing changes hue; the whole effect is a
slight, monotonic brightening that removes a ceiling the hardware should never
have had.

**The argument for doing it:** the ABI is written down and the island disagrees
with it. Any differential against the reference oracle is currently wrong by
this amount on every palette pixel, so the oracle cannot be used as an oracle
for the CLUT path until this matches.

**The argument for waiting:** star and sky appearance changes on the same
commit, and if anything downstream was tuned by eye against the darker output,
that tuning moves.

---

## Defect B — every CLUT lookup reads the same byte

**This one is NOT a one-line change, and the earlier shorthand that called it
one was wrong.**

```systemverilog
.lu_idx_i(disp_clut_data[$clog2(PAL_ENTRIES)-1:0])   // = [7:0], always
```

`disp_clut_data` is `DATAW` = 64 bits (4 lanes x 16). A CLUT8 halfword holds
**two** texels, so which byte is wanted depends on the addressed u.

### Why it is not a one-liner

The selector exists — the planner emits `acc_fu_o`, which the island carries as
`plan_acc_fu` — but it goes **only** to `zhao_texture_bilerp_lane` (`.fu_i`).
The palette lookup happens in the RESPONSE path, keyed by the routing token,
long after the request. `plan_acc_fu` at that moment belongs to whatever request
the planner is currently emitting, not to the response being decoded.

**Reading it there would be the same defect this island was just repaired for**
— a late read of a signal belonging to a different transaction. The ingress
gate `tools/rtl/check_ingress_capture.py` exists because of exactly that
mistake.

So the byte selector has to **travel with the sample**, and the established
pattern for that is already in the file: the sample class and the palette
binding are both keyed by FRAGROB slot, written when FRAGROB reports where the
fragment landed. A per-sample byte select needs the same treatment, and it is
per SAMPLE rather than per FRAGMENT — a fragment's three samples can differ —
so it is slightly more than the class table, not less.

### What changes

Roughly **half** of all CLUT texels currently decode the wrong palette entry —
every one whose u is odd. Fixing it changes those to the entry the asset
actually names. This is not a subtle shift like defect A; it is a different
colour per affected texel, and on a paletted texture with unrelated neighbouring
entries the visible change can be arbitrary.

**Note the interaction:** defect A brightens the colour a byte names; defect B
changes *which* byte is named. Fixing A alone leaves half the terrain texels
still wrong, just brighter. Fixing B alone leaves every palette colour capped
below its intended maximum. They compound, and the mip blend inherits both.

---

## What I recommend, and what I am not deciding

1. **Fix A and B together, not separately.** Half-fixing produces a third
   behaviour that matches neither the current output nor the oracle, and makes
   any before/after comparison harder to read rather than easier.
2. **Land them on their own commit**, ahead of any blend RTL, with the star and
   sky change called out in the message — not folded into a terrain feature.
3. **Do not treat the current directed test as sufficient.** It asserts palette
   residency and non-blackness, which both defects survive. The check that would
   have caught them is a per-texel comparison against `zref`, and that check is
   worth writing before the fix so it goes red first.

The decision is whether the star and sky change is acceptable now. That is an
owner call and this document does not make it.

## What is NOT claimed

* No simulation or fit was run for either proposal.
* The pixel counts above are arithmetic from the expressions, not measurements
  from a rendered frame.
* Defect B's cost is stated as a shape — a per-sample field carried like the
  class — not as an ALM figure. On this island a small operation surrounded by
  poor scheduling has already proved able to become a large hardware problem.
