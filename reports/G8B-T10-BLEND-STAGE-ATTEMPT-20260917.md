# G8B T10 — the fourth blend stage, attempted and withdrawn

*2026-09-17. Written so the next attempt starts from the diagnosis rather than
from scratch. The RTL is reverted; `zhao_terrain_tess.sv` is unchanged.*

## Why the stage is needed, and why nothing cheaper will do

`@g8b-t8-pins` leaves G8B at **96.45 MHz on physical pins** with 18 of 2,000
summarised paths negative and one family binding:

    ln2_prod_q[22] -> vq_y[1][30]      -0.368 ns

That is `rescale16` — a 52-bit add, a shift and a saturate — followed by
`fx_add_sat`, a 34-bit add and a saturate. **Two dependent carry chains in one
cycle.** Four cheaper things were tried or ruled out first:

| attempt | result |
|---|---|
| T7 — move the rounding add into the register ahead | **−7.9 MHz.** That register is the DSP's output register; an adder in front evicts the product into fabric |
| T9 — strength-reduce the add to a one-bit increment | **0.0.** Exact, and Quartus had already done it: byte-identical fit from a different digest |
| reassociate `rescale16(p)` + `fx_add_sat(vh, ·)` into one add | **Not equivalent.** The merged form saturates once where the original clamps the inner rescale to int32 first. At `t = 2³³`, `vh = −2³¹` the original gives **−1** and the merged form **+2³¹−1** — a sign flip mid-range, and `p >> 16` genuinely reaches 2³³ |
| seed 2 instead of seed 1 | **94.42 MHz, TNS −10.4** against seed 1's 96.45 / −1.603 — *worse*, so the gap is structural and not placement luck |

Registering the *landing* also buys nothing: the path would become
`ln2_prod_q -> rescale16 -> fx_add_sat -> ln3_y_q`, the same logic with a
different endpoint. **The stage has to fall between the two adds.**

## What was built

Stage D holding `m_step` — the rescale's 32-bit result — with the saturating add
performed at D:

    C: ln2_prod_q -> rescale16  -> ln3_step_q
    D: ln3_step_q -> fx_add_sat -> vy[] and the queues

It is cheaper in flops than carrying the product would be (32 bits, not 52).
Everything the landing reads travels with it: `kind, slot, last, idx, stride,
vh, x, z, h, ax..bz, surface, src`.

Changed with it, all of which the next attempt still needs:

* **`VQ_DEPTH` 4 → 5, `TQ_DEPTH` 2 → 3.** A fourth in-flight stage adds a fourth
  term to each credit, which would block an issue one cycle sooner at the old
  depth — and that is precisely how T1 met a timing target and lost the vertex
  rate. The law: the buffer must be *deeper* than the number of items in flight,
  because a read already issued cannot be told to wait.
* **A fourth credit term** — `vland_c` / `tland_c` — and `vland`/`tland` moved to
  `ln3_*`. `vtx_room` and `tri_room` expressed as `<= DEPTH-1` rather than a
  literal, so the depths and the credits cannot drift apart again.
* **`vland_idx` widened to 3 bits, `tland_idx` to 2**, since they now index 0..4
  and 0..2. `tnxt` widened to 4 bits.
* **`ln3_v_q` joined the StIdle drain and `idle_o`** — a stage left out of a
  drain makes the block declare a job finished while it still holds a vertex.

## Why it was withdrawn: 32 of 6,751 checks

`terrain_tess_directed` failed with the right triangle COUNT and wrong content:

    first mismatch at triangle 1 of 128 (geomorph at every factor and every stride)
      want a(1048576, 847961,1048576) b(1048576, 815925,1179648) c(1179648,740911,1179648)
      got  a(1048576, 847961,1048576) b(1048576, 740911,1179648) c(1179648,740911,1179648)

    first mismatch at triangle 1 of 128 (geomorph on the underside plane)
      want a(1048576,-3260831,1048576) b(1179648,-3162777,1179648) c(1048576,-3239712,1179648)
      got  a(1048576,-3260831,1048576) b(1179648,-3162777,1179648) c(1048576,-3162777,1179648)

**`b.y` receives `c.y`'s value in the first, and `c.y` receives `b.y`'s in the
second.** The `a` vertex is right in both. So this is not a wrong blend — it is
a vertex picking up a *neighbouring slot's* blend, which is a forwarding or
ordering fault, not an arithmetic one.

### The lead the next attempt should follow first

**`vy[]` has TWO writers, and the stage move changed their relative timing.**

    stage A:  vy[pend_slot]  <= lat_h_i     the raw lattice height (kinds 0/1)
    stage D:  vy[ln3_slot_q] <= m_y_d       the blended height (kind 2)

The three write-forwards were all moved to compare against stage D, which is
correct as far as it goes — a snapshot taken on the edge the blend lands must
see the landing:

    A->B:  lnd_ay_q <= (D lands slot 0 now) ? m_y_d : vy[0]
    B->C:  ln2_ay_q <= (D lands slot 0 now) ? m_y_d : lnd_ay_q
    C->D:  ln3_ay_q <= (D lands slot 0 now) ? m_y_d : ln2_ay_q

What that does **not** account for is the stage-A raw write. With a three-stage
blend, the raw write at A and the blended write at D were two cycles apart; with
four they are three. Any slot whose raw write and whose blended landing now fall
on different sides of a snapshot will hand the wrong value to whichever vertex
reads `vy[]` next — which is exactly the shape of the observed failure.

The next attempt should trace one job's `vy[0..2]` writes and the three
snapshots cycle by cycle, rather than reasoning about the forward conditions in
isolation as this one did. The three conditions are each individually defensible
and the interaction is what is wrong.

## Status

* RTL reverted; `zhao_terrain_tess.sv` is byte-identical to `b1dbb97d`, the
  commit `@g8b-t8-pins` measured, so that receipt still describes the tree.
* `terrain_tess_directed` 6,751, `terrain_tess_modes_directed` 33,
  `terrain_pipe_differential` 37 — all green from the reverted tree.
* **G8B stands at 96.45 MHz against its 100 MHz criterion.**

One number worth keeping from the failed build: the cycle count moved 458 → 459
for 128 triangles, i.e. the extra stage costs exactly one cycle of drain and
does **not** touch the initiation rate. The architecture rule — latency may
grow, initiation rate may not — is satisfied by this design. Only the forwarding
is wrong.
