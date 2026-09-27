# DECISION — I34's material triple rides a WIDENED RIDER, not a parallel FIFO

Taken 2026-09-27 by the coordinator under the standing delegation in
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0. Decision-record format per
§0: question; chosen option; reason and alternatives; constraints/cost;
code/tests/compatibility consequences.

It is recorded **before** the packet that builds it, because MATERIALPATH
refused this work for a correct reason — *"which is correct is an engineering
choice, not a fact in the tree"* — and a choice left in a refusal is a choice
nobody makes. It also frees the work to start the moment GEOM is quiet.

---

## QUESTION

MATERIALPATH composed `TERRAIN.MATJOIN` and met the owner's acceptance test: a
Field material write changes the consumer's served triple, `composepub_acceptance`
154/0, authored `{9D,EA,B4}` → served `{2A,7C,B3}`. **`I34` did not close**,
because the composed triple stops at `tcf_tri_mat_*_w` and does not reach the
mosaic as a **per-triangle** value.

**The 32 bits already have complete, live carriage.** `base_rgb[23:8]` plus
`recipe_weight` ride `tri_flat_request_c` from `zhao_console_core` into the
shell, through `zhao_geom_binner_v2`'s metadata bank, are decoded at
`zhao_raster_tile_pipe_v2.sv:770-781` and reach `zhao_texture_mosaic_v2`. **No
file under `fpga/rtl/texture/` or `fpga/rtl/raster/` needs to change.**

What is missing is a per-triangle **SOURCE**, and the obstacle MATERIALPATH
named — **which no document in this tree had named before** — is **alignment**.
`tri_flat_request_c` escapes alignment only because it is a **level held
constant for a whole span** (`zhao_console_core.sv:31256-31261` says so itself).
A per-triangle value has no such excuse: it must stay with its triangle through
GEOM.SETUP and GEOM.ATTRPACK, and the only per-triangle sideband that survives
that trip today is a **16-bit source id** (`zhao_geom_setup.sv:150`, `:179`,
`:260` — hard-coded 16 bits, **no parameter**), verified present at those sites.

So: **widen the existing riders, or run a console-local FIFO aligned beside the
pipeline?**

## DECISION

**Widen the riders. Do NOT add a parallel aligned FIFO.** The triple travels
*inside* the per-triangle record, on both legs:

* **clipdoor → clip**: a per-client field on `u_geom_clipdoor` carried to
  GEOM.CLIP's output and muxed into the 32 bits by the rider's **domain**.
  `GEOM_VID_RIDERW` is a real parameter (`zhao_console_core.sv:11714`, `16 + 32
  + 2 = 50`) and **all fifty are allocated** — `cl_o_rider[49:34]` material,
  `[33:2]` raster, `[1:0]` domain. **Widening touches three files**
  (`zhao_geom_clipdoor`, `zhao_geom_clip`, `zhao_console_core`), not eleven.
  **This is not another `METAW` widening like R234 D1's eleven-file span.**
* **setup → attrpack**: parameterise the hard-coded 16-bit `tri_src_id` /
  `out_src_id` and widen it. **It has no parameter today; give it one.**

## REASON, AND IT IS THIS SESSION'S OWN EVIDENCE

**A side queue that must stay in lockstep with a pipeline is precisely the
`u_geom_tidq` defect class, and this campaign has just paid for it.**

`u_geom_tidq` is exactly the "aligned FIFO beside the pipeline" pattern. It was
**permanently one behind** — `popped[k] == pushed[k-1]` — so **74 of 75
triangles were binned under their predecessor's arena descriptor index**. It
survived because **the ids stayed in range and decoded cleanly**, so every range
guard passed and every gate stayed green. It took a dedicated packet, three
structural changes (a door gated on occupancy, a seal-abort push, flush poisoning
in place rather than discarding), a **deadlock mode**, and an 83-check directed
test with **21 failures against the base RTL** to repair.

The asymmetry is the whole argument:

* **A value riding INSIDE the record cannot drift from it.** There is no
  invariant to maintain, so there is no invariant to break.
* **A FIFO beside the record CAN drift, and its failure is invisible** — wrong
  material on the right triangle renders a plausible picture. There is no range
  check that catches it, and this tree has just demonstrated that a
  structural startup skew of **one beat** is enough.

Widening costs **bits**, which are countable and which a fit will price.
A FIFO costs an **alignment invariant that nothing in the tree can check
cheaply** — and the campaign's law is that the instrument which would have caught
it goes blind in the flattering direction.

## THE OBJECTION THAT MADE WIDENING LOOK UNAFFORDABLE IS DEAD, AND SO IS THE ONE BEHIND IT

**MATERIALPATH measured both.**

1. **The drain price is NOT owed.** `zhao_material_window.sv:486-492`'s `match_c`
   has five terms — mode, vertex alpha, detail, frag state, material set,
   material id — and **neither `base_rgb` nor `recipe_weight` is one of them.**
   A per-triangle triple muxed into those bits costs **ZERO drains.**

   **This answers a question entry `I34` explicitly left open** at
   `zhao_console_core.sv:4065`, which asked whether a constant terrain material
   plus a per-cell `{tile_a, tile_b, weight}` on a widened `GEOM_VID_RIDERW`
   satisfies the window's *"a span is a run of primitives that AGREE"*, and
   correctly declined to answer without measuring. **It is now measured: the
   triple does not participate in span agreement, so varying it per triangle
   does not break spans.**

2. **The blocker quoted immediately above that paragraph is FALSE and has been
   inherited through FOUR refusals.** The entry says `material_set`/`material_id`
   under `fpga/rtl/terrain/` *"return ZERO hits"*, with a positive control
   offered to show the zero was not a broken grep. **There are four code hits.**
   The zero was real when taken and is not real now; **a document cannot go
   stale loudly.** That text must be struck in place when this is built, not
   quietly overwritten.

## ALTERNATIVES CONSIDERED

* **A console-local aligned FIFO.** Rejected above. Cheaper in bits, and it buys
  the exact failure mode that cost this campaign a packet last night.
* **Reusing the 30 zero bits on three of the four client arms.** Rejected: they
  are **R28's `raster_state`, allocated and not spare** (`:11339`, `:18564`,
  `:18576-18579`, `:20189`, `:20206-20207`). Directive §4: *"never silently
  overload a field, truncate a handle, or substitute a convenient zero."* **Bits
  that happen to be zero today are not free bits.**
* **Widening `METAW`.** Not required and explicitly not the shape here — the
  shell-side carriage already exists.

## CONSTRAINTS AND COST

* **Unpriced in area, and that is declared.** No `-MapOnly` row is attached to
  this decision. The console stands at **293,352 ALUTs against 227,120 present**,
  and two packets have been refused this week on area (**+11,979 ALUTs** for
  I34's front, **+40 DSP** for I55's second back end). **A rider widening is
  bits, not datapath, so it is expected to be small — but expected is not
  measured.** The building packet owes a leaf row on the shipping part with
  `rtlCleanAtHead` true, and may refuse on it.
* **`GEOM_VID_RIDERW` is a parameter; `tri_src_id` is not.** Creating that
  parameter is part of the work, not a side effect.
* **Sequencing: this is GEOM work and GEOM is `PVSCHEMA`'s lane.** It must not
  start while that packet is live. This record exists so the decision is not on
  the critical path when it clears.

## CONSEQUENCES

* **Code:** `zhao_geom_clipdoor`, `zhao_geom_clip`, `zhao_geom_setup`,
  `zhao_console_core`. Both `.*` wrapper mutants if a core port moves.
* **Tests:** a directed test asserting the triple arrives **with its own
  triangle** under interleaving — not merely that it arrives. The `tidq` lesson
  is that arrival proves nothing; **the discriminator is an equality against the
  producer**, per triangle, not a variety check on the consumer.
* **Compatibility:** no texture or raster file changes; no `METAW` change; no
  memory region changes.
* **It closes `I13`'s seam, not `I34`'s** — MATERIALPATH was right about that.
  Expect the register to move on `I13`'s terms and `I34` to follow.
