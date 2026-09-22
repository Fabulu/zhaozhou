# PART.CLIPFEED — the particle arm of GEOM.CLIP's door

RTL: `fpga/rtl/particles/zhao_part_clipfeed.sv`
Test: `tests/particles/part_clipfeed_directed.cpp` (54 checks)
Acceptance: `tests/prod/partmat_acceptance.cpp` (43 checks)
Ledger: `design/blocks.yml` — owner ZH-056 — phase 5

---

## THE TWO THINGS IT IS, AND NOTHING ELSE

`design/contracts/GEOM.CLIPDOOR.md`'s closing section measured what the particle
arm still owed on 2026-09-21. It was two named items, and **neither was the
seven-slot attribute packet** that five passes over the FORGE cluster believed
was the wall:

1. **A canonical depth.** `zhao_part_expand.t_d_o` is Q16.16 1/w
   (`zref::ScreenV::d`); GEOM.CLIP's attribute slot 0 is **invw24**. The two are
   different quantisations of one ordering and they **share one depth buffer
   with the mesh path**, so a Q16.16 value in slot 0 z-tests wrong against every
   mesh triangle. Owner ruling **D-4**: *"all downstream consumers receive only
   the canonical invw24. No consumer performs its own profile conversion."*
2. **The attribute packet.** Seven 32-bit slots per corner, which a fan of three
   screen vertices, one shared depth and one flat colour does not have — until
   **R197** made the u/w and v/w slots don't-care for a declared untextured
   primitive.

This block is those two and nothing else. Owner ruling 1 of 2026-09-22
authorised the work in terms: *"This authorization covers the complete
particle-to-raster connection, including the canonical depth conversion and any
carrier still required by the current producer. It is not permission to close
the task after changing only the material gate."*

---

## THE CONVERSION IS A SECOND INSTANCE OF ONE LAW

`zhao_geom_depthquant_stream` + `zhao_raster_rcp24_v4`, wired exactly as
`zhao_geom_vattr` and `zhao_forge_assemble` wire them. **There is no arithmetic
in the file at all**: no shift, no product, no rounding, no saturation. The
`zhao_field_isqrt` precedent governs — a second INSTANCE of one law is not a
second law; a second EXPRESSION of it would be, and there is none.

**The input is `w`, which is why two composed blocks grew a lane.** The
converter's own header: *"It consumes w and performs its OWN reciprocal."* `w`
existed on the particle path all along — the shared projector returns it at
`zhao_part_project.a_w_i` — and was **dropped at that block's ladder queue**.
GEOM.CLIPDOOR.md located that; this packet carried it:

* `zhao_part_project`'s `PROJ_W` goes **99 → 132**, appending `w` (31 bits) and
  the depth profile (2) at the **low** end so every field above keeps its
  offset-from-the-top. The one reader that indexed from the *bottom*
  (`lad_size_o`) is re-indexed from the top, which is where it should always
  have been — an offset-from-the-bottom is exactly the shape that goes silently
  wrong when a layout grows.
* The profile is `zhao_project_service.a_profile_o`, taken at the composer from
  the same port and for the same reason `zhao_forge_assemble.rs_profile_i` takes
  it: **valid on the same cycle as the result it describes**, so it cannot skew
  from it. It is captured into the slot store beside `a_w_i` on that one enable,
  which is what makes the pair one record rather than two live wires (entry
  I39).
* `zhao_part_expand` carries both through untouched. It converts neither.

---

## THE ANSWERS COME BACK OUT OF ORDER, AND PARTICLES MUST NOT

The stream answers *"in the reciprocal's COMPLETION order with the caller's
tag"*, and the reciprocal is multi-context, so two particles issued in order can
land in either order.

**Particles are depth-TESTED and never depth-WRITTEN** —
`draw_population`'s *"pass-7 law: test only, no write"* — so two that overlap are
resolved by **draw order and by nothing else**. Emitting them in completion
order reorders a blend and changes no number anywhere.

So there is a **ring, and the tag IS the ring slot**:

* a particle is accepted only when it holds a ring slot **and** the converter
  takes its `w` on the same clock, so the answer's home exists before the
  question is asked;
* the answer lands by tag into that slot and sets its `landed` bit;
* the **head** of the ring is emitted, and only when its bit is set.

**It is not the "independently advancing metadata queue" owner ruling 1
forbids.** The thing it holds and the thing it is keyed by are **one record at
one index, written by one enable**, and the material half does not travel
through it at all — a particle's material mode is a constant of the producer,
presented at the door beside the beat it belongs to.

**It cannot deadlock.** Every accepted particle has issued its `w`; the converter
always answers; the landing's ready is constant 1 because the landing is a write
into a slot nothing else writes.

---

## THE NARROWING 22 → 21 IS LOSSLESS, AND IT IS CHECKED ANYWAY

`tests/particles/part_expand_directed.cpp` section 7 proves exhaustively that
`max |vertex| = 524288 + 4080 = 528368 < 2^20`, so the 22nd bit is **headroom**
and no input honouring `to_screen_xy`'s clamp can set it.

The check here is **not a second opinion about that measurement**. It is the
measurement's **premise made enforceable**: the proof is conditional on a clamp
that lives three blocks upstream, and a truncation that wraps silently turns a
particle at the screen edge into one at the opposite edge. A vertex that does
not fit **refuses the fan whole** and counts `range_refused_o`.

That counter is reachable with **legal stimulus at this block's own ports** — the
port is 22 bits and a bench drives it — so it is the `t_ack_i` shape and **owes
no committed mutant**. It is fired in section 4 of the directed test, with the
negative control (528368, the largest the clamp permits) beside it.

---

## WHAT THE DOOR BEAT DECLARES, FIELD BY FIELD

| field | value | authority |
|---|---|---|
| `untex` | 1 | `sprites.cpp::draw_population`'s tris branch calls `raster_tri(surf, vpp, a, b, cc, p.r, p.g, p.b, tm)` — three positions, three colour bytes, **no `TextureSpan`**. R197 makes it legal to declare. |
| `material_mode` | `NO_MATERIAL` | The **sibling** of the bit above and a **different statement**. Owner ruling 1 insists on keeping them apart: *"no texture coordinates"* says the PRIMITIVE cannot support a sampling material; *"no material"* says the PRODUCER intentionally uses its own defined profile. |
| `material_set`, `material_id` | 0 | Not tie-offs: the declaration `zhao_material_window.mode_contra_c` **requires**. A producer declaring NO_MATERIAL while handing over an identity it expects resolved is **refused and counted** there. |
| `quality_tier` | 0 | A label echoed by MATERIAL.RESOLVE on a request this producer never makes. Deliberately **not** part of the window's contradiction test, so zero here is the absence of a label rather than a claim about one. |
| `behind` | 3'b000 | A **fact about the producer**: `zhao_part_expand`'s `emits = take && p_in_i` means a behind-the-eye particle is consumed and emits nothing. |
| `cull_mode` | `PART_CULL_MODE`, default CULL_NONE | `draw_population` is double-sided. The fan's 2A is negative for every size; GEOM.CLIP **normalises** winding and **rejects** zero area, which is precisely why R187 put the door at its input. |
| slots 3,4,5 | `{16'd0, c, 8'd0}` | The **exact left inverse** of `zhao_raster_tile_pipe_v2::lit_unit8(v) = v[15:8]` saturating, so it round-trips every one of the 256 bytes. **Derived from the consumer**, not invented. R234 D1's Gouraud planes are not branched on `tri_untex_i`, because *"an untextured primitive is still lit"*. |
| slot 6 | `PART_ALPHA`, default opaque | An **art value**, therefore a named editable parameter (CLAUDE.md rule 6), not a derived quantity. |
| slots 1,2 | 0 | Agreeing with `zhao_geom_attrpack`, which branches on `tri_untex_i` and substitutes the zero operand. Agreeing with the block that overwrites them is not relying on it. |

All three corners share one depth and one colour. That is what a polygon
particle is, and GEOM.ATTRPACK turns three equal slot values into a constant
plane with no special case.

---

## WHAT IT DOES NOT CARRY — declared, not overlooked

`zhao_part_expand.t_depth_test_o` (1) and `t_depth_write_o` (0) — the pass-7 law
— **do not enter here**, and the reason is **not particle-specific**.

In this console the raster state word is `zhao_console_core.render_state_i`, a
**boundary input**, sampled per tile job at `zhao_raster_tile_pipe.job_state_i`.
**No producer supplies it per primitive** — not GEOM.REPLAY, not the forge, not
terrain. GEOM.CLIP carries `cull_mode` and consumes it internally; nothing
downstream of GEOM.SETUP takes a depth mode from a triangle at all.

A port here would have nowhere to go, which is **a tie-off wearing a port's
clothes**. The gap is declared as `zhao_console_core` entry **I51** instead,
where the completion register can see it — and the register went **10 → 11**
for that reason, which is the instrument working.

---

## THE RING IS ASYNCHRONOUS-READ, AND THAT IS THE ALM QUESTION

**Corrected by review before this contract shipped.** A first draft of the
ledger row said the triangle ring *"may infer as M10K"*. It will not: both
arrays are read **asynchronously** at the head pointer — `tri_q[head_c]` and
`inv_q[head_c]` feed the output combinationally — and an async read is the one
thing that rules an M10K out.

So the block is ~3,040 bits of **flops** plus a **16-way 166-bit mux**, and the
mux is the part to look at, on the binding constraint.

**The trade is named here rather than taken.** Making the ring a *synchronous*
read would let it infer as M10K, which is the standing owner direction — ALMs
are the constraint, memory is the slack. It costs a pipeline stage between the
head select and the door beat, and **the door must never gain a skid** between
the material half and the triangle half, so the stage would have to sit
*before* the door's input rather than inside it. That is a real design question
and it belongs in a packet that can test it.

---

## WHAT THE COMPOSED PATH IS, END TO END

```
PART.POP -> PART.PROJECT (w + profile now survive the ladder queue)
         -> PART.LADDER's SHARD rung
         -> PART.EXPAND (the fan; w and profile carried, not converted)
         -> PART.CLIPFEED (invw24, the 7-slot packet, in-order)
         -> GEOM.CLIPDOOR client 2
         -> zhao_material_window (NO_MATERIAL: the defined no-sampling profile,
                                  no resolve, no fault counter)
         -> R197's untextured gate (passes: sample_count is 0)
         -> GEOM.CLIP -> GEOM.SETUP -> GEOM.ATTRPACK -> the shell's door
```

The AND-fork at PART.EXPAND keeps the boundary port group live — **a PORT is
neither a module nor a tie-off**, and `tb_zhao_console_core_smoke` and
`zhao_console_board` both carry it — with `u_part_clipfeed` as the second arm,
written exactly as GLUE 6's fork: each consumer's valid is gated by the other's
ready, so nothing is presented twice.

**The declared cost of that fork:** a boundary consumer holding
`part_exp_ready_i` low stalls particles into the door. The smoke drives it
constant 1.
