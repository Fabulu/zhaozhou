# FINDINGS — UNTEX (owner ruling R197, the untextured attribute law)

**Branch `gz/untex`, commits `addc0be7` and `55451c42`. Register 21 → 21.**
**No tie-off created. The seven-slot packet did NOT change shape.**

> **TRANSCRIBED BY THE COORDINATOR** — the harness refused this lane a report
> `.md` (*"Subagents should return findings as text"*), the eighth in a row.
> It did not route around the refusal.

---

## `addc0be7`

untex: R197's untextured attribute law -- DECLARED by a per-primitive bit, refused at GEOM.CLIP's door, never encoded

Owner ruling R197 (2026-09-20): a primitive MAY enter GEOM.CLIP with u/w and
v/w undefined PROVIDED IT DECLARES that it has none. The absence is DECLARED,
never ENCODED: u/w = v/w = 0 is texel (0,0), not "no texture" (spec W10;
R168/R181 one seam over). Register 21 -> 21. No tie-off created, no port
narrowed, no function removed, nothing older composed.

(The harness refused this lane a FINDINGS-untex.md, as it has the seven
before it -- "Subagents should return findings as text". Not routed around;
the findings are here and in the lane's final message.)

THE LAW, AS AUTHORED (design/contracts/GEOM.CLIP.md "The untextured
declaration"; spec/qformats.md 8; GEOM.SETUP.md; RENDERER_ARCHITECTURE.md
RULING 5; design/blocks.yml GEOM.CLIP; the core header's I13 entry (b))

1. THE FLAG: one bit, `untex`, PER PRIMITIVE, presented at GEOM.CLIP's input
   beside `tri_src_id_i` on the same handshake and carried through the three
   stages exactly as src_id is (`tri_untex_i -> s1/s2/s3_untex -> out_untex_o`).
   Per primitive and not per corner, justified: the oracle's own law is per
   CALL (`ScreenV::u, v` are "read only when raster_tri carries a
   TextureSpan"); three per-corner copies could disagree and would need a
   corner-agreement checker whose zero is one more silent instrument; the B/C
   winding swap has nothing to swap in it; GEOM.VATTR would store a constant
   per row.

   THE SEVEN-SLOT PACKET DOES NOT CHANGE SHAPE. GEOM_CLIP_ATTRS stays 7,
   GEOM_CLIP_ATTRW 224, GEOM_ATTR_STORE_W 192; no [ATTRS*32-1:0] port moved
   anywhere; gen_shell_paired_diff.py --check is fresh because the shell's
   port list is untouched.

2. EVERY CONSUMER, BY INSTANTIATION in zhao_console_core.sv (R180: the
   ledger's `upstream:` is intent, not wiring):
   * THE DOOR at `u_geom_clip`'s input (`cl_in_*`, new): law 3.
   * `u_geom_clip`: carries the bit, interprets nothing. Verdict, packet,
     box and counters identical with the bit set or clear.
   * `u_geom_attrpack` -- THE ONLY READER OF SLOTS u_over_w AND v_over_w IN
     THE TREE. With the bit set it does not latch them: lanes 1 and 2 of the
     shared attrsetup core are fed the ZERO operand, so the slot content never
     enters the plane arithmetic and the packed u/w, v/w planes are the null
     plane {n0=0, dndx=0, dndy=0}. The lane schedule is unchanged, so
     `planes == 3 * triangles` still holds.
   * `u_geom_setup`: unchanged; it reads no attribute.
   * the shell: unchanged, honestly. Downstream of the door `untex =>
     sample_count == 0` holds by construction, and the island's existing
     zero-sample branch (required_mask_of -> no sample; combine's
     `untextured_c`) keeps the slot from a sampler. The tile pipe still
     interpolates the null planes and divides them; skipping that needs the
     bit inside Packet-D's frozen 1,157-bit metadata word -- an ABI change,
     not taken.

   A FACT THAT DECIDED THE ATTRPACK BRANCH: zhao_raster_tile_pipe_v2's
   `incoming_range_bad_c` ORs every lane's `q_error_o`, and attrgrad_v2 raises
   it when a lane's gradient divide is refused. Arbitrary don't-care content
   in slots 1/2 COULD therefore terminate the frame through lanes 1 or 2 --
   "don't-care" would have been a lie. The null plane (0 / 2A never errors)
   makes the content genuinely irrelevant instead of documenting a constraint
   producers would have to remember.

3. REFUSED AND COUNTED: a primitive offered at the door DECLARING untextured
   while the material window's published `sample_count` is non-zero is
   consumed at the door (`mw_t_ready` high), never entered (`cl_in_valid`
   low), and counted on the core's new `geom_untex_refused_o`. The refusal
   sits BEFORE the window's accounted span -- `d_enter_i` is now GEOM.CLIP's
   own accept, not the window's handshake -- so "there is no fourth outcome"
   still holds and the drain law is untouched. `cl_in_refuse_c` is a function
   of the offered declaration and the REGISTERED publication only, never of a
   valid or of GEOM.CLIP's ready, so no ready depends on its own valid. It is
   also the door R187 names for every non-mesh producer ("the honest door is
   at GEOM.CLIP's input"): the arbiter that admits particles and shadow hulls
   presents its `untex` bit to THIS gate.

4. R48's ALPHA_C IS THE PRECEDENT, and the producer's declaration takes its
   shape: `GEOM_REPLAY_UNTEX_DECL = 0` (TEXTURED) in the core's parameter
   block -- true, because every REPLAY triangle is a format-0 record and
   format 0 carries u/v (VDECODE refuses every other format). A named seam,
   not a stub; a parameter rather than a localparam so a WRAPPER mutant can
   flip it.

THE PRODUCER CARRIED THROUGH, AND THE ONES REFUSED
Carried through end to end: GEOM.REPLAY, the only composed producer; its
declaration rides door -> CLIP -> ATTRPACK and the plain smoke asserts the
refusal counter ZERO on it. Every producer that would declare 1 is REFUSED
with measurements, because each is a subsystem and not a wire: particles
(zhao_part_expand, I24) need the arbiter, a per-tine flat request (the window
publishes ONE material per span), the window's occupancy for a second tine, a
DEPTHQUANT reciprocal for `t_d_o`, and a producer for `tri_fragment_state_i`
(SETUPDOOR's four blockers, R187); FORGE.SHADOW is refused by R133; FORGE.PRIM
emits index triples and needs the whole projection front end (R180); terrain
still needs lit r/g/b, which is art content R197 does not take (decision 5).
So the refusal counter's state is unreachable by legal stimulus in the
console -- which is exactly why the positive control is a committed mutant.

THE INSTRUMENT, PROVEN TO FIRE
* tests/mutants/zhao_console_core_untex_decl_mutant.sv -- a WRAPPER (.*,
  every parameter by name, no copied body; the slot-overflow wrapper's shape),
  one substantive line GEOM_REPLAY_UNTEX_DECL 0 -> 1. Driven by
  run_console_core_smoke.ps1 -UntexMutant (own TAG, own build dir), INVERTED
  polarity: passes only when geom_untex_refused_o == SGF_EXP_REPLAYED,
  geom_clip_submitted_o == 0, geom_setup_triangles_submitted_o == 0 and the
  window's guards stay silent. The plain run is the negative control and
  asserts the counter zero.
* geom_clip_directed case 12 (carriage, both windings, toggled between
  consecutive triangles, under backpressure): 3,438 -> 3,484 checks, RC 0.
* geom_attrpack_directed case 5 (loud values in slots 1/2 with the bit set ->
  null planes, unchanged invw24 plane, full read on the next triangle):
  4 -> 6 cases, planes == 3 * triangles, 0 failures, RC 0.
* geom_clip_random (differential over the default iterations) RC 0;
  geom_clip_attrswap_directed 13 checks RC 0; terrain_downstream_rate 17
  checks RC 0 (its bench gained the port). All BUILT AND RUN (R60).

BENCHES AND TOPS TOUCHED BY THE PORT
tests/shell/tb_zhao_shell.sv and tests/terrain/tb_terrain_downstream.sv
present `tri_untex_i = 1'b0` with the reason beside it and sink out_untex_o;
zhao_prod_top.sv and zhao_console_board.sv regenerated and READ (the leaf's
new input is LFSR-driven at u14, the board carries the parameter and the
counter); the existing slot-overflow wrapper gained the parameter and the
port (a wrapper must match production's port list or -Mutant stops
elaborating).

ONE BENCH DEFECT FOUND ON THE WAY: tb_zhao_console_core_smoke's PC_CORE and
PC_SHELL macros picked the wrapper path only under ZHAO_MUT_SLOT_OVERFLOW, so
any second wrapper mutant fails 47 hierarchical probes at elaboration and
reads exactly like a broken core. Both macros now list every wrapper define,
and say so.

TIE-OFFS: none. GEOM_REPLAY_UNTEX_DECL is a named-constant declaration at a
seam (R48's shape) and the TRUE value for the producer it describes; it is
recorded at the parameter, at the port map (// REAL:), in the contract and
here so the coordinator can disagree with the classification in the open.
packet_h_tieoff_audit on the core: 0 SILENT.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## `55451c42`

smoke: a mutant arm's $finish is not a stop -- `disable run` after it, both wrapper arms

Found running -UntexMutant at addc0be7. The bench's mutant verdicts sit in
an `ifdef` chain whose `else` arm ends at the TERRAIN verdict's `endif`, so
every production check after that point (GEOM.CLIP submitted/clipped/culled,
GEOM.SETUP's count, the untextured door's own production zero, ...) still
runs against the mutant after its `$finish` -- `$finish` runs the named
block to the end of the time step. The slot-overflow arm has always
survived this because production's geometry counters happen to agree on its
design. The R197 arm does not: nothing enters GEOM.CLIP BY DESIGN, so the
run printed

    SMOKE: MUTANT PASS -- geom_untex_refused_o fired 16 time(s) ...
    %Fatal: ... GEOM.CLIP submitted=0 clipped=0 culled=0 -- the reference wants 16 / 2 / 0
    (nine %Fatal lines in all)

under a PASSING exit code -- the broken-instrument shape exactly: the
script's RC said green, the log said red, and only reading the log caught it
(ruling R82, capture the full output). The slot-overflow arm's own comment
already describes this trap from its first writing and chose `ifdef/else`
to dodge it; the `else` was never wide enough.

The stop is `disable run;` after each arm's `$finish;` -- the verdict block
is `initial begin : run`, a named block, so a mutant verdict is now the last
thing the bench says. Both wrapper arms get it, because the latent form in
the slot-overflow arm would go red the first time a production counter
disagrees with that mutant, and would say something alarming about the
design rather than "your arm kept running".

Re-measured on this commit: -UntexMutant and -Mutant both PASS with ZERO
%Fatal lines in their logs; the plain, -LintOnly, -BadVertex, -NoEchoArm
and -BadTraceArm forms are unaffected by the change and were green at
addc0be7.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

