# TERRAIN.TESS is the terrain geometry lane's clock, and `cell_solid` is why

*2026-09-07. Written from the first path summary `zhao_pair_tess_normals` has
ever had. No RTL was changed to produce this, deliberately — the previous pass
on this lane read the RTL, guessed, edited, and moved the clock 4.2%.*

---

## How this was reached, and the mistake that preceded it

`zhao_pair_tess_normals` measured **31.10 MHz** against a 100 MHz product clock
— the worst recorded number in the tree, on the terrain geometry path, and
ungated for a fortnight because no fit target existed for it.

I read NORMALS, found a 33×33 multiply feeding a 67-bit adder in one cycle,
registered the product, and predicted the pair would move "well above 31.10".
**It moved to 32.42.** The comment written into the RTL said what that would
mean, and it means the multiply was not the limit.

The pair had **no path summary at all** — which is precisely why it had gone
unexamined — and the correct first move was to fit it and look, not to read it
and guess. It has one now.

## Where the clock is, by counting

1,803 summarised paths, classified by which block each **end** sits in:

| paths | count | worst slack | implied |
|---|---:|---:|---:|
| wrapper lattice memory → **TESS** | 129 | −20.848 | **32.42 MHz** |
| **TESS → TESS** | 625 | −14.931 | **40.11 MHz** |
| NORMALS → NORMALS | 693 | −3.752 | 72.72 MHz |
| TESS → NORMALS *(the seam)* | 66 | +2.780 | 138.50 MHz |
| NORMALS → TESS | 1 | +5.165 | 206.83 MHz |

**TESS is the limiter twice over.** The seam between the two blocks is fine in
both directions, which is worth stating plainly because it is the opposite of
what the PAGESTREAM→PATCH seam looked like: composing TESS with NORMALS costs
nothing.

## The internal cone, by census rather than by reading

Sources and destinations of the worst 200 `TESS → TESS` paths:

```
  sources                         destinations
    pend_last_OTERM632   85         vh                    69
    solid                49         subpatch_rejected_o   42
    eg                   43         f_kind                 9
    ea                    9         vz / eg / eb / ea      ~8 each
```

and the worst ten all launch from `eg[0]~DUPLICATE` into `Mux0..29~0_OTERM*`.

`solid`, `ea`, `eb` and `eg` meet in exactly one place —
`zhao_terrain_tess.sv:304-316`:

```systemverilog
logic cell_solid;
always_comb begin
  cell_solid = 1'b1;
  for (int cj = 0; cj < int'(SubCells); cj++) begin
    for (int ci = 0; ci < int'(SubCells); ci++) begin
      if (ci >= int'(ea) * int'(j_s) && ci < int'(ea) * int'(j_s) + int'(j_s) &&
          cj >= int'(eb) * int'(j_s) && cj < int'(eb) * int'(j_s) + int'(j_s)) begin
        if (!solid[cj*8+ci]) cell_solid = 1'b0;
      end
    end
  end
end
```

**64 iterations, each with four comparisons against `ea * j_s` and `eb * j_s`,
reducing to one bit.** `j_s` is a 4-bit *register* holding 1/2/4/8
(`zhao_terrain_tess.sv:242`), not a constant, so those are genuine runtime
multiplies rather than shifts by a literal.

That one bit then gates issue:

```systemverilog
wire cell_skip   = (emode != EmFan) && !cell_solid;
wire want_issue  = (st == StTri) && !done && !cell_skip;
```

which is why the destination census is `vh`, `f_kind`, `pend_slot` and
`subpatch_rejected_o` — the issue path and its reject counter. The cone runs
from all 64 bits of `solid` plus `ea`/`eb`/`j_s`, through a 64-way reduction,
into the control that decides whether a triangle is emitted, in one cycle.

## The proposal, and why it is not implemented here

The window is a rectangle `[ea·j_s, ea·j_s + j_s) × [eb·j_s, eb·j_s + j_s)` over
an 8×8 bitmap. Expressed as a mask the test is:

```systemverilog
cell_solid = ((solid & win_mask) == win_mask);
```

— a 64-bit AND and a 64-bit compare. The win comes from **registering
`win_mask`**, which moves the whole comparator array off the path and leaves
only the AND-compare.

**That requires `ea`/`eb`/`j_s` to be stable one cycle before `cell_solid` is
consumed, and whether they are is a question about the run-cell walk that this
report does not answer.** If they are not, the mask costs a cycle per run-cell,
and that is exactly the trade that just failed to pay on NORMALS.

So this is written down rather than done. The sequence that would settle it:

1. Establish from the state machine whether `ea`/`eb` advance a cycle ahead of
   the `cell_solid` consumption. Source reading is sufficient for this and it
   is cheap.
2. If yes, register the mask and refit — **prediction first**, and the honest
   prediction is that `TESS → TESS` moves from 40.11 toward the 60s, while the
   `lattice memory → TESS` path at 32.42 is **untouched**, because that one
   ends in `vy` through `Add65` carry chains and has nothing to do with
   `cell_solid`.
3. The lattice path is a separate repair: `vy[pend_slot] <= m_y` with
   `m_y = fx_add_sat(vh[pend_slot], …)` lands a saturating add on the same edge
   as the memory read that feeds it.

**Two repairs, not one, and neither alone reaches 100 MHz.** Even with both,
NORMALS sits at 72.72 MHz — 27% short — so the terrain geometry lane needs work
in all three places. That is a scope statement the previous pass did not have
and could not have had.

## What this does not claim

* Not that the NORMALS change was worthless. A 33×33 multiply feeding a 67-bit
  adder in one cycle is a real hazard and it is gone; it bought 1.3 MHz because
  something twice as bad was in front of it.
* Not that NORMALS at 72.72 MHz is an improvement on what it was. There is no
  before-measurement — the pre-change fit produced no path summary — so that is
  **unmeasured**, not assumed.
* Not that the wrapper's lattice memory is exactly the console's. It is a
  64-entry stand-in, chosen so the read is a memory read rather than a wire.
  The *shape* of the path is representative; its absolute delay is not
  necessarily.
