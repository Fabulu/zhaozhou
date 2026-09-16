# G8B — the terrain pipe's first measurement

`zhao_terrain_pipe_rpp3_matw18_fit_top@g8b`, source commit `968243b5`,
`rtlCleanAtHead: true`, `treeCleanAtHead: true`, seed 1, 9 sources, digest
`052bcdc0f898…`, `physical-top-ports`, 471.9 s.

This is the **first time** `zhao_terrain_pipe` has been fitted at
`ROWS_PER_PASS=3, MATW=18` — the configuration G8B was specified to
characterise. Before Packet I there was no wrapper, no parameter-fixed test,
and no number.

## The result

| | value |
|---|---:|
| status | `failed:structure` |
| **Fmax** | **43.94 MHz** |
| setup WNS | **−12.758 ns** |
| setup TNS | **−7,360.359 ns** |
| hold WNS / TNS | +0.251 / 0 |
| ALMs | 7,424 |
| registers | 8,070 |
| DSP | 34 |
| RAM blocks | 44 |
| memory bits | 95,610 |
| negative paths | **2,000 of 2,000 exported** |

One rule violation: *Fmax 43.94 MHz < required 100 MHz*. The resource gates all
pass — 7,424 of 41,910 ALMs, 34 of 112 DSPs, 44 of 553 RAM blocks — which is
exactly the case that rule's note warns about: **every resource gate can pass
while the clock does not.**

## This is much worse than G8A ever was

| | G8A Timing3 | G8A Timing5 | **G8B** |
|---|---:|---:|---:|
| Fmax | 90.96 MHz | 108.37 MHz | **43.94 MHz** |
| setup TNS | −131.275 ns | 0 | **−7,360.359 ns** |
| negative paths | 497 / 2,000 | 0 / 2,000 | **2,000 / 2,000** |

G8A's worst state was a design that missed its clock by 9 MHz with 497 slow
paths. G8B misses by **56 MHz**, and every single exported path is negative —
the best of the 2,000 worst is −1.47 ns. This is not a design that needs a few
cuts; it has never been timed at all.

## Where it goes, precisely

Endpoints of the negative paths: `zhao_project_core` 1,631, `zhao_terrain_tess`
160, inferred RAMs 104 and a scattering of others. Launch points:
`zhao_project_core` 716, `zhao_terrain_tess` 229, `zhao_terrain_group_seq` 104.

The six worst paths are all the same shape, and the worst is **−12.758 ns with
22.481 ns of data delay and only −0.097 ns of skew** — so this is logic depth,
not placement:

```
  lat_h_q[0]  ->  u_terrain_pipe|u_tess|vs_y[0]      -12.758 ns, data 22.481
  lat_h_q[0]  ->  u_terrain_pipe|u_tess|vo_y[0]      -12.735 ns, data 22.466
```

`lat_h_q` is the wrapper's **registered** lattice response — the memory answers
in one clock, never combinationally, exactly as the architecture requires. So
the launch is legitimate and the 22.5 ns is entirely inside the tessellator.

Reading `zhao_terrain_tess` at the morph blend shows why. From `lat_h_i` to the
vertex registers is ONE combinational chain:

```
  lat_h_i
    -> m_dab   = lat_h_i - v_ha                 (34-bit subtract)
    -> m_half  = rescale1(m_dab)
    -> m_hc    = fx_add_sat(v_ha, m_half)       (saturating add)
    -> m_d     = m_hc - vh[pend_slot]           (34-bit subtract)
    -> m_prod  = j_morph * m_d                  (17 x 34 SIGNED MULTIPLY)
    -> m_step  = rescale16(m_prod)
    -> m_y     = fx_add_sat(vh[pend_slot], m_step)
    -> last_y  -> vs_y / vo_y                   (registered here, at last)
```

Two subtracts, a multiply, two rescales and two saturating adds, launched from
a memory response and not registered until the very end. 22.5 ns for that on
this device is unsurprising; what is notable is that nothing had ever measured
it.

## What this does NOT say

* **It is not a verdict on the wrapper.** The wrapper's own activity witness
  passes 15/15 with zero matrix refusals, all three legal view masks, both
  projector clients sharing one core with contention firing 14,928 times, and
  output backpressure exercised 603 times. The traffic is legal and the machine
  was doing real work while being measured.
* **It is not a comparison against `MATW=32`.** No such fit exists. Any claim
  that narrowing to 18 bits helped or hurt would need the other fit, and the
  architecture's own rule is that a result is evidence for this wrapper only.
* **It is not the whole machine.** 7,424 ALMs is a subsystem figure.

## One observation for the ALM budget, with its caveat stated first

Sums of standalone fits are **not** a composed measurement, and this repo has
already been burned by reasoning from one: the composed island came in 2.4%
UNDER the sum of its parts, after an argument that standalone sums overstate
badly. So treat the following as an order of magnitude and nothing more.

G8A is 13,076 ALMs and 30 DSPs; G8B is 7,424 ALMs and 34 DSPs. Those two
subsystems alone are about **20,500 ALMs and 64 DSPs**, against whole-machine
targets of 30,000 and 85. If that sum is even roughly indicative, what remains
for geometry, the shell, video, audio and memory is about 9,500 ALMs and 21
DSPs. That is tight, and it is the first time the two largest measured
subsystems could be put side by side at all.

## What is owed next

1. **Pipeline the morph blend.** The chain above is the entire 43.94 MHz
   result. It is a real design change with a real contract — the tessellator's
   arithmetic is exact and its cycle laws are asserted — so it is a piece of
   work, not a tweak.

   **And it is not a one-line cut, which is worth knowing before starting.**
   `last_y` is consumed in six branches of the ModeVtx landing logic
   (`vland && vpop`, `vland` with and without `vo_valid`, `vpop` with and
   without `vs_valid`), so registering the blend delays the LANDING, not just a
   value. `pend_idx`, `pend_stride` and `pend_slot` have to travel with it, and
   the `vtx_room` credit that guarantees "a landing never finds both slots full
   without a pop" is stated against the current timing and has to be re-argued
   against the new one.

   The architecture rule permits this: latency may grow, initiation rate may
   not. So the target is a blend stage that adds one cycle of latency and no
   bubbles, with `terrain_pipe_differential`'s exact packet and cycle counts
   re-derived rather than relaxed — it currently pins 478 packets over 2,343
   cycles, and that number moving is expected while the RATE must not.

   The cheap trick that worked for RESOLVE — compute the first half a cycle
   earlier, off the arriving response — does **not** apply here. `m_hc` depends
   on `lat_h_i` itself, so there is nothing upstream of the memory response to
   move work into.
2. `zhao_project_core` holds 1,631 of the negative endpoints and 716 of the
   launches. After the tess chain is cut, it is the next question, and it is
   shared with the geometry client rather than terrain-only.
3. Re-fit as a new labelled row. `@g8b` is now taken and the runner is
   one-shot by design.
