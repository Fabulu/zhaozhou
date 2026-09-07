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

---

## Step 1, answered: the mask can be registered at NO latency cost

The report above said the registered-mask proposal "requires `ea`/`eb`/`j_s` to
be stable one cycle before `cell_solid` is consumed, and whether they are is a
question about the run-cell walk that this report does not answer." Answered:

**They are not stable, and the reason looks fatal at first.** The enumerator's
advance is *gated by the very signal the mask would feed*
(`zhao_terrain_tess.sv:800`):

```systemverilog
if (cell_skip || (do_issue && iss_last)) begin
  ...
  if (ea >= cell_hi) begin ea <= cell_lo; ... eb <= eb + 4'd1; end
  else               begin ea <= ea + 4'd1; end
end
```

with `cell_skip = (emode != EmFan) && !cell_solid`. So `ea` at cycle N decides
`cell_solid` at N, which decides whether `ea` changes at N+1. A mask registered
from `ea` would lag it by a cycle, and skipping a void run-cell would take two
cycles instead of one — against a comment that states the current behaviour as
a goal: *"A void run-cell is SKIPPED here, at one cycle per skipped cell and no
lattice read at all."*

**That is the wrong way round, and the structure gives it away.** At cycle N−1
the advance decision is already made — `cell_solid(N−1)` is known, so
`ea(N)`/`eb(N)` are known, because they are *assigned* at N−1. The mask for
cycle N is therefore computable at N−1 from the same next-state expression that
already computes `ea <=` and `eb <=`.

So the shape is ordinary next-state precomputation:

```systemverilog
// alongside every existing assignment to ea/eb, assign the mask for the
// run-cell they are moving TO -- the job-start cases at lines 685-686 and
// 713-714, the EmInner->EmFan transition, and the two advance arms.
win_mask_q <= window_mask(ea_next, eb_next, j_s_next);
```

and the consumed test becomes one AND and one compare:

```systemverilog
cell_solid = ((solid & win_mask_q) == win_mask_q);
```

**No added latency, no change to the skip rate.** The 64 comparator groups move
off the consumed path into a next-state cone that has a whole cycle, and the
one-cycle void skip is preserved.

### What still has to be checked before this is built

* `j_s` is assigned at line 632 (`j_s <= s_new`) on a different edge from the
  run-cell advance, so the mask's next-state expression must take the *same*
  `j_s` the consumer will see. Getting that wrong changes which cells are
  tested, which is a correctness bug, not a timing one.
* `ea`/`eb` are assigned in **five** places (581-582 reset, 685-686 job start,
  713-714 job start, and the two advance arms). Every one needs the paired mask
  assignment or the mask goes stale — and a stale mask is a *wrong solidity
  answer*, which emits or drops triangles.
* The prediction stays as written: this addresses the **40.11 MHz** `TESS→TESS`
  family only. The `32.42 MHz` lattice-memory→`vy` family is a different cone
  and is untouched by it.

That last point is why this is still not implemented in the same pass that
found it. Five paired assignments in a state machine that emits geometry, with
41,731 checks resting on it, is not a change to make between two fits without
the before-measurement in hand.


---

## The coverage sweep this provoked, measured rather than grepped

The `cell_solid` rewrite was validated by a fire test — drop the row term from
the window's outer product, which is a wrong solidity answer that emits or
drops triangles, and see which suites notice. One did not, so every suite that
shares the fixture was put through the same mutation rather than reasoned
about:

| suite | checks | detects the mutation | why |
|---|---:|---|---|
| `terrain_tess_directed` | 6,751 | **5 failed** | punches voids |
| `terrain_tess_random` | 2,277 | **45 failed** | random `kVoidAuthored`/`kVoidBreached` |
| `terrain_tess_normals` | 41,731 → 46,709 | **0 → 636 failed** | fixture was all-solid; fixed |
| `terrain_lod_tess` | 93 | 0 failed | **correct** — non-dual by design |

`terrain_lod_tess` builds `make_lattice(false, 2)` with `job.dual = false`, and
the reference's rule is `sol = !lat.dual || substance(...) == kSolid`. On a
legacy page solidity is *unconditional*, so that suite is not blind — the
window genuinely has nothing to say to it. Recording the distinction matters:
"does not detect the mutation" and "has a coverage hole" are not the same
finding, and treating them as one would have sent someone to widen a suite that
is already correct.

**The 41,731-check suite was the only one with the hole, and it was the largest
of the four.**

---

## The mask landed, and it did not do what it was for

**32.42 → 33.10 MHz. +2.1%.** ALM 1,579 → 1,574. The prediction above said the
`TESS → TESS` family would move off 40.11; it went to **37.23 — the wrong
direction.**

| family | before | after |
|---|---:|---:|
| lattice mem → TESS | 32.42 | 33.10 |
| **TESS → TESS** | **40.11** | **37.23** |
| NORMALS → NORMALS | 72.72 | 85.50 |
| TESS → NORMALS *(seam)* | 138.50 | 156.79 |

### Why the prediction was wrong, precisely

I derived the mask from **counting operators in the source** — 256 comparisons
and 128 multiply sites — and CLAUDE.md's law is that measurement belongs on the
comparison side, never the generation side. Counting `*` in RTL is not counting
logic after synthesis, and the ALM figure says so: **five ALMs.** Quartus had
already collapsed the double loop; there was nothing there to win.

The endpoint census told me *which signals* met in `cell_solid`. It did not tell
me *where the time was*, and those are different questions. For
`zhao_project_core` I pulled the per-hop delays before proposing anything. For
this block I did not, and the per-hop data for the worst path names something
else entirely:

```
  2.515  IC    lat_mem_rtl_0|...|ram_block1a0|clk0
  2.463  CELL  lat_mem_rtl_0|...|ram_block1a0~POR      the memory's clock-to-out
  1.117  IC    u_tess|Add65~65|datad
  0.800  CELL  u_tess|Add65~65|cout                     a carry chain
  0.796  IC    u_tess|Add66~13|datad
  0.665  CELL  u_tess|Add66~13|cout                     a second carry chain
  0.513  CELL  u_tess|Add65~9|sumout
```

**28.080 ns of data path, and `cell_solid` is not in it.** It is a memory read
followed by adder chains — which is the *other* repair this report already
named:

> The lattice path is a separate repair: `vy[pend_slot] <= m_y` with
> `m_y = fx_add_sat(vh[pend_slot], …)` lands a saturating add on the same edge
> as the memory read that feeds it.

So the ranking in this report was right — the lattice family at 32.42 MHz was
always the bigger one — and **I implemented the smaller one anyway.**

### What is kept, and why

The mask stays. It is bit-identical, the suites prove it (48,510 checks, and the
coverage hole it exposed is now closed), the source is smaller, and it costs
nothing. But it must not be recorded as a timing improvement: **it bought 2.1%
and it was not aimed at the thing that costs 28 ns.**

### The next repair, now measured rather than reasoned

The memory's clock-to-out is 2.463 ns and cannot be removed — a synchronous
lattice read is what the console does. What follows it can be: `Add65` and
`Add66` are the `fx_add_sat` chains landing on the same edge. Registering the
lattice sample before the add splits the path at the one place with a natural
boundary, and the per-hop numbers are on disk to size it before it is written
this time.

---

## The 28 ns is the GEOMORPH BLEND, named at last

The endpoint census said `solid`, `ea`, `eg`. The per-hop walk says something
else, and this time the whole chain is legible:

```
   cum ns   incr  type   element
    8.187  2.463  CELL   lat_mem|ram_block1a0~PORT_...     the memory's clock-to-out
    8.340  0.153  CELL   lat_mem|portbdataout[2]
    9.457  1.117  IC     u_tess|Add65~65|datad
   11.230  0.513  CELL   u_tess|Add65~9|sumout             adder 1
   12.026  0.796  IC     u_tess|Add66~13|datad
   13.483  0.355  CELL   u_tess|Add66~1|sumout             adder 2
   14.576  1.093  IC     u_tess|Add67~77|datab
   16.144  0.113  CELL   u_tess|Add67~17|cout              adder 3
   ...     113 further hops below 0.09 ns each
   36.267         data path = 28.080 ns
```

Three chained adders and a long carry tail. In the source that is one
expression chain, evaluated between a memory read and a register:

```systemverilog
m_dab  = lat_h_i - v_ha                    // 34-bit subtract, straight off the RAM
m_half = rescale1(m_dab)                   // shift, round-half-up, saturate
m_hc   = fx_add_sat(v_ha, m_half)          // 34-bit add + two 34-bit compares
m_d    = m_hc - vh[pend_slot]              // 34-bit subtract
m_prod = j_morph * m_d                     // 17 x 34 MULTIPLY
m_step = rescale16(m_prod)                 // 52-bit add, shift, two compares
m_y    = fx_add_sat(vh[pend_slot], m_step) // 34-bit add + two compares
vy[pend_slot] <= m_y
```

**Seven arithmetic stages including a multiply, all combinational, all on the
same edge as the lattice read that feeds them.** That is §4.3's geomorph — the
interpolation of the coarse cell and the blend toward it — and it is the block's
clock.

## What it would take, sized rather than hoped

28.080 ns against a 10 ns period needs **at least three cuts**, not one. Two
cuts gives three stages averaging 9.4 ns with no margin for the routing a
composed design adds; three gives four stages near 7 ns.

The natural boundaries are where the expression already names its own steps:

| cut | after | what it isolates |
|---|---|---|
| 1 | `m_hc` | the parent interpolation: RAM read, subtract, rescale1, add-sat |
| 2 | `m_prod` | the multiply, in its own stage, where a DSP output register is free |
| 3 | `m_step` | rescale16's 52-bit add and saturate |

**The price is three cycles of latency per vertex**, and TESS emits geometry —
so `terrain_tess_normals`' 47,221 checks and `terrain_tess_directed`'s 6,751
are what would have to keep passing, plus the pair's throughput assumptions.
The block's contract declares `latency: variable`, which is what made the
NORMALS and PAGESTREAM pipeline changes admissible, and the same sentence
covers this.

**It is not done here for the reason the last two attempts on this lane were
wrong:** both were written from a reading, and this is the first time the chain
has been measured end to end. The next pass has the per-hop numbers to size each
cut before writing it, and the mutation sweep to prove the suites can still see
a wrong answer afterwards.

## What has been learned about this block, in order

1. **NORMALS' product register** — measured, +1.3 MHz. The multiply was not the
   limit, and the comment in the RTL said what a non-move would mean.
2. **`cell_solid` as a mask** — bit-identical, five ALMs, +0.7 MHz. Derived from
   counting operators in the source, which is the generation side.
3. **The geomorph chain** — measured per hop, 28.080 ns of the 30.2 ns period.
   This is the one.

Two wrong guesses and one measurement, in that order, is the wrong order. The
census names *which signals*; only the per-hop walk names *where the time is*.
