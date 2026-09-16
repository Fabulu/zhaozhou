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

## Traced, 2026-09-17 — the mechanism, and why a fourth forward level is not it

The first hypothesis was that a snapshot can now fall one cycle *before* the
blend lands, so `vy[k]` still holds the raw height and no forward fires. A
two-level forward was built to test it — forward from stage C (lands next
edge, value is the old three-stage `m_y`) as well as from stage D. **Result:
the same 32 failures, unchanged.** Hypothesis refuted, cheaply.

So the pipeline was traced instead of argued about: a `$display` of every
stage's `(valid, kind, slot, last)`, `vy[0..2]`, both blend outputs and all
six snapshot registers, on every edge with traffic.

### What the trace settles

**1. Non-morphing traffic is correct through the four-stage pipe.** 400 cycles
of kind-0 jobs were checked by hand against the emit edges: every `a` and `b`
matched the `vy[]` value written for that group, including the shared-vertex
repeats between adjacent triangles. The stage, the credits, the widened
indices and the drain all work. The fault is *only* in morphing vertices.

**2. THE READ WALK IS NOT (s0, s1, s2).** This is the fact the whole attempt
was built on a wrong picture of. A morphing vertex takes THREE reads — kind 0,
then coarse parent A, then parent B — and the walk is over (slot, kind) pairs:

```
TR 329863  A(k1 s1)     slot 1, parent A
TR 329864  A(k2 s1)     slot 1, parent B  -> its blend completes at D on 329867
TR 329865  A(k0 s2)     slot 2, raw
TR 329866  A(k0 s0)     ... the next group has already started
```

So a triangle takes between three and nine reads, and **the `last` read can
itself be the kind-2 read of the last slot** — `TR 329872  C(k2 s2 L1)`.

**3. Which gives the real mechanism.** When the last read is slot 2's kind-2
read, the emit needs slot 0's and slot 1's *blends*, and slot 1's kind-2 read
may have issued only one or two cycles earlier. With a three-stage blend the
two snapshot levels (A→B, B→C) were exactly enough to catch both landings;
with four stages the blend lands one cycle later than the snapshot chain can
reach, and **no number of forward levels fixes it**, because forwarding from A
or B means forwarding a value whose multiply has not happened yet.

### Which leaves two real options, neither of them a bug fix

**(a) Hold the last read until pending blends have landed.** Correct and
simple, and it is a STALL — it would cost initiation rate on exactly the
morphing path, which `terrain_tess_modes_directed` measures at 169 cycles for
81 vertices at morph 0.5. The architecture rule is that latency may grow and
the initiation rate may not, so this needs the rate measured before it is
accepted, not after.

**(b) Select at the EMIT rather than snapshotting early.** For each of `a` and
`b`, choose between `vy[k]` and whichever pipeline stage currently holds slot
k's unlanded blend — a four-way match on `(kind == 2, slot == k)` across A, B,
C, D. That removes the snapshot registers rather than adding to them, and it
puts the mux on the `a`/`b` path, which is **not** the critical one: the
binding path is `prod -> rescale -> add -> vq_y`, and `a`/`b` reach the
triangle queue through a different cone entirely.

**(b) is the one to try**, and it is a different change from the one attempted
here — it deletes the three write-forwards instead of extending them.
## Both named options tested, 2026-09-17 — and both are now CLOSED by evidence

### Option (a): hold the last read until pending blends land — CORRECT, REJECTED

One extra term on `last_blocked`: the last read does not issue while any
earlier kind-2 read is still in the blend pipe.

```systemverilog
wire blend_pending = (pend_v  && pend_kind  == 2'd2)
                  || (lnd_v_q && lnd_kind_q == 2'd2)
                  || (ln2_v_q && ln2_kind_q == 2'd2);
wire last_blocked  = blend_pending || (j_vtx ? !vtx_room : !tri_room);
```

**The geometry is right: `terrain_tess_directed` 6,751/6,751, and
`terrain_pipe_differential` 37/37 bit-exact against the oracle.** So the
mechanism diagnosed above is confirmed — waiting for the blends *is* the
missing condition, and nothing else about the four-stage design is wrong.

**And the rate law rejects it, in the words of the block's own test:**

```
FAIL: VTX level 0 with morph: one lattice read per clock:
      expected 0xAD, got 0xF2
```

173 cycles for 161 reads became **242**. Triangle morph-0.5 went 939 → 1,113
cycles for 128 triangles, 7.34 → 8.70 per triangle, an 18.5% loss on exactly
the morphing path. Non-morphing was untouched at 459 (the one extra drain
cycle). *Latency may grow; the initiation rate may not* — and this is the same
test that caught T1 doing the same thing, firing on the same clause.

### Option (b): read `vy[]` at the emit — BLOCKED, and the reason is in the file

At the emit, slot 0's and slot 1's blends have necessarily landed: their
kind-2 reads issued *before* the last read, so they are ahead in the pipe and
past the landing stage. `vy[0]`/`vy[1]` should therefore be correct with no
forwarding at all — which would delete all three write-forwards.

They are not, and `zhao_terrain_tess.sv` already says why:

> *"reading `vx[0]`/`vy[1]` a cycle later would read the NEXT job's captures,
> because the enumerator advances at issue and the next job's first read can be
> issued on the same edge this one lands."*

The trace confirms it directly — at `TR 76` the emit needed `vy[1] = 562253`
while `vy[1]` already held `811090`, the following group's value. **That is why
the snapshots exist at all**, and it is why they are taken as early as
possible. Option (b) as stated cannot work.

### What is left, stated precisely

The four-stage design is correct except that a snapshot taken at A→B may
predate a blend it needs, and no forward can reach back to a stage whose
multiply has not happened. So the value has to be *patched in later* rather
than forwarded earlier:

* snapshot at A→B as now, and record per slot whether that snapshot was taken
  before the slot's kind-2 read had landed;
* keep the landing's value in a small per-slot holding register — one that the
  next group cannot overwrite, which is exactly what `vy[]` fails to be;
* at the emit, take the holding register for any slot whose snapshot was
  stale, and the snapshot otherwise.

That is three flags and three registers, it adds nothing to the binding
`prod -> rescale -> add -> vq_y` cone, and it does not stall. It is a design
change rather than a repair, and it should be built and measured on its own
rather than bolted onto this attempt.
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
