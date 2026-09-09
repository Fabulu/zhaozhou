# GEOM.WCACHE's payload predates the `w` correction by three days, and nobody widened it

2026-09-09. Found while resolving a disagreement between the existing
`zhao_geom_wcache` (75-bit payload) and the `zhao_proj_arena3` design study
(106-bit payload). The disagreement is real, it is not a matter of taste, and
**the existing production block is the wrong one.**

## The chain, traced port by port

| stage | file | exposes `w`? |
|---|---|---|
| the arithmetic | `zhao_project_core.sv:235` | **YES** -- `out_w_o [30:0]` |
| geometry wrapper | `zhao_geom_project.sv:97` | **YES** -- `out_w_o [30:0]` |
| the replay cache | `zhao_geom_wcache.sv:80` | **NO** -- `PAYLOAD_W = 75` |
| terrain wrapper | `zhao_terrain_project.sv:139-157` | **NO** -- no `w` port at all |
| the consumer | `zhao_geom_depthquant.sv:63` | **NEEDS IT** -- `v_w_i [39:0]` |

The wcache's own header spells its 75 bits out: x[21] + y[21] + invw[32] +
behind[1]. There is no `w` in it.

## Why that is a defect and not a choice

`design/contracts/GEOM.DEPTHQUANT.md` carries a **CORRECTION dated 2026-09-03**,
headed *"THE INPUT IS `w`, NOT `1/w`"*:

> The first draft of this contract said the input was the projector's Q16.16
> `1/w` and the job was a multiply and a shift. **Both were wrong** [...]
> **It consumes `w` and performs its OWN reciprocal.** It is not a rescale of a
> reciprocal somebody else computed.

and, under *"And the projector does not expose `w`"*:

> **So `w` exists one wire away from where it is needed and is discarded.**
> [...] Expose `clip.w` from `zhao_project_core` as an additional output and
> feed [it forward] [...] `1/w` back to `w` **loses precision to answer a
> question the projector could have answered exactly**, and re-expressing the
> ruled law in terms of `1/w` means [changing the ruled law].

That recommendation **was acted on.** `zhao_project_core` grew `out_w_o` and so
did `zhao_geom_project`. The RTL confirms it and the core's header even says
`w` "is a first-class signal and is exposed again on the output".

**But `zhao_geom_wcache.sv` is dated 2026-08-31 -- three days BEFORE the
correction -- and its payload was never widened.** So the fix landed at the
producer and at the wrapper, and stopped at the cache sitting between them and
the consumer.

Any consumer replaying a projected vertex through the wcache gets 75 bits with
no `w`, and DEPTHQUANT must then reconstruct `w` from the **rounded** Q16.16
reciprocal -- which is precisely the precision loss the correction was written
to prevent.

## This is the owner's pattern exactly

The projector cheque was: plan the consolidation, build the prerequisite, never
perform the final step. This one is a variant that is harder to see, because
**the cheque was PARTIALLY cashed:**

    2026-08-31  wcache written, 75-bit payload, correct for what was known
    2026-09-03  DEPTHQUANT correction: the consumer needs w, not 1/w
    2026-09-03  core and geom wrapper grow out_w_o          <- cashed
        ...     wcache payload widened to carry it          <- NEVER
        ...     terrain wrapper grows a w output            <- NEVER

A fully-forgotten follow-up leaves an obviously unfinished thing. A partially
cashed one leaves a chain where **most of the links were fixed**, which reads as
done to anyone who checks the producer or the contract, and only fails at the
one link nobody re-opened. There is no marker anywhere saying the wcache is
stale; it simply predates the ruling that invalidated it.

## Consequences

1. **`zhao_proj_arena3`'s 106-bit record is vindicated.** Its argument -- that
   `w` must never be dropped because recovering it from the rounded `1/w`
   compounds a rounding that already happened -- is the contract's own argument,
   independently rederived. My earlier note said "exactly one of these is right"
   and left it open; it is now closed, in the new module's favour.
   `reports/ARENA-I-BUILT-A-SECOND-ONE-20260909.md` should be read with this.
2. **The wcache payload needs widening**, to 75 + 31 = 106 bits, which is
   exactly arena3's record and is not a coincidence -- both are "the projector's
   full output". The header already anticipates this: *"Carried as a parameter
   rather than a literal"*, and *"When it is settled, this parameter moves and
   nothing else does."* The parameter was built to move. Nobody moved it.
3. **`zhao_terrain_project` exposes no `w` at all**, so terrain's path cannot
   feed DEPTHQUANT correctly today either. Separate gap, same root.
4. **A width to reconcile before anyone builds this:** the core emits
   `out_w_o [30:0]` (31 bits) and DEPTHQUANT accepts `v_w_i [39:0]` (40 bits).
   The nine-bit difference is presumably a zero-extend of a guarded value into a
   wider input, but it is DECLARED differently at the two ends and nothing here
   proves they mean the same thing. Check it against `spec/qformats.md` before
   sizing any storage on either number.

## What this does NOT establish

**Not that a frame is currently wrong.** `zhao_geom_wcache` is instantiated in
`zhao_prod_top.sv:2257`, but that top is the **generated area/PINMISSING
harness** -- the instance is driven from an LFSR (`u34_src`), not from
`zhao_geom_project`. So this is a defect in a block that is composed for
measurement and not yet wired into a live datapath. It is a trap laid for
whoever wires it, not damage already done. That distinction matters and I am
not going to blur it to make the find sound bigger.

**Not a measured cost.** Widening 75 to 106 bits changes the arena's memory
shape. Neither `zhao_geom_wcache` nor `zhao_vertex_arena` has any fit summary in
`reports/synthesis/blockpaths/`, so their M10K count today is UNKNOWN and the
delta from widening is UNKNOWN. It is one question for the projection-subsystem
fit that is already named.

## The generalisable check

When a correction lands, the thing to grep for is not the producer -- somebody
always fixes the producer. **Grep for every place the corrected value is STORED
or FORWARDED**, because caches, replay buffers and packet layouts are written
against yesterday's contract and carry no marker saying so. A payload width is a
frozen copy of an old agreement.
