# FINDINGS — SETUPDOOR (the GEOM.SETUP triangle-arm arbiter)

**Branch `gz/setupdoor`, commit `64feccd7`. Register 21 → 21. REFUSED.**
**No port changed, no tie-off created; the core diff is comments only.**

> **TRANSCRIBED BY THE COORDINATOR** — sixth lane the harness has refused a
> report `.md` (R183). It did not route around the refusal.

**The headline: the arbiter is the SMALLEST of four blockers.** R180's *shape*
half is confirmed; its conclusion is not — and the correction is worth more
than the arbiter would have been.

---

setupdoor: GEOM.SETUP's arm needs an ATTRIBUTE LAW, not an arbiter. I24 refused, measured

Commissioned to build the arbiter on zhao_geom_setup's triangle arm and take
zhao_part_expand through it to close boundary I24. REFUSED. Register 21 -> 21:
nothing closed, nothing opened, NO tie-off created, NO port changed, no RTL
behaviour changed. The core diff is comments only.

(The harness refused this lane a FINDINGS-setupdoor.md, as it has the five
before it. Not routed around; the findings are here instead.)

THE HEADLINE: THE ARBITER IS THE SMALLEST OF FOUR BLOCKERS

R180 / FINDINGS-forge4 read "one missing arbiter is holding a composed block at
the boundary". The SHAPE half is confirmed. The conclusion is not.

1. The arm is not a stream endpoint. It is ONE TINE OF A THREE-WAY ORDERED
   JOIN -- zhao_geom_setup (edge functions), zhao_geom_attrpack (three 240-bit
   planes), u_material_window (resolved material AND an occupancy accounting)
   -- pairing by ARRIVAL ORDER with no tag. The window's own comment states the
   invariant a particle breaks: "There is no fourth outcome for a triangle in
   that span". A particle at the door IS that fourth outcome.

2. IT DEADLOCKS COMBINATIONALLY, and the proof is three assigns in the core:
       cl_o_ready       = st_tri_ready_w && ap_tri_ready_w
       st_o_ready       = door_tri_ready_w && ap_o_valid_w
       door_tri_valid_w = st_o_valid && ap_o_valid_w
   Particle into SETUP alone -> ATTRPACK never saw it -> ap_o_valid_w low ->
   st_o_ready low -> SETUP cannot drain -> cl_o_ready falls -> GEOM.CLIP
   stalls -- and ATTRPACK can only be fed through GEOM.CLIP. Closed cycle.
   The other branch is WORSE: if a mesh triangle is in ATTRPACK, the particle's
   edge functions join THAT triangle's planes and material, skewed by one for
   the rest of the frame. R133's "composing it wrong costs a deadlock", at a
   different block.

3. zhao_material_window's err_occupancy_underflow_o and err_unpublished_o would
   BOTH fire on every particle. The interlock is already instrumented.

4. THE BINDING BLOCKER IS THE SEVEN-SLOT ATTRIBUTE PACKET -- THE SAME WALL
   ENTRY (b) ALREADY RECORDS FOR TERRAIN. GEOM_CLIP_ATTRS = 7 (invw24, u/w,
   v/w, lit r,g,b, alpha) PER CORNER. A polygon particle has a flat colour, ONE
   shared 1/w, and NO TEXTURE COORDINATES BY LAW -- draw_population's tris
   branch rasterises with a flat colour and a TriMode carrying only
   depth_test/depth_write. Zeroing u/w and v/w would sample texel (0,0) on
   every particle: closing a gap by narrowing function.

TERRAIN AND PARTICLES REACHED THE IDENTICAL WALL INDEPENDENTLY. So:
GEOM.SETUP's arm is not short of an ARBITER, it is short of an ATTRIBUTE LAW
FOR EVERY NON-MESH PRODUCER. The arbiter is the LAST step, not the first.

THE WIDTH ANSWER: THE 22ND BIT IS HEADROOM, NEVER RANGE

zhao_project_core::to_screen_xy clamps to +-524288 ("the clamp is the law; it
is not a clip and it is not optional") and is the ONLY producer of p_x_i/p_y_i
(-> zhao_project_service.a_x_o -> zhao_part_project.h_x_o/q_x_o -> the rung
demux). Largest fan offset is 255<<4 = 4080, so

    max |vertex| = 524288 + 4080 = 528368  <  2^20 = 1048576

Proved exhaustively (all 256 size bytes x all four rail corners -- exhaustive,
not sampled, because every output is monotone in one coordinate plus a
size-dependent offset), asserted on EVERY vector of both lanes, and the
invariant was SEEN TO FIRE: a positive control with the clamp premise withdrawn
(one vector at the 21-bit PORT maximum) failed 1 of 757 checks, and only that
one.

AND A CORRECTION TO A SHIPPED HEADER, which is this repo's own law. The WIDTHS
section reasoned "21 bits plus a twelve-bit offset -> 22 bits" -- from the PORT
WIDTH, a PROJECTION of the value rather than its law. The clamp pins the centre
to TWENTY bits of magnitude and the offset fits in the spare one. Corrected in
place.

The consequence runs the useful way: a future door may narrow 22 -> 21
LOSSLESSLY AND PROVABLY, instead of truncating on faith or widening the arm,
the core's render_ax_i, BOTH shell tops and the raster for a bit that cannot be
set. The port stays 22 bits -- it costs nothing and keeps a clamp violation
looking wrong rather than wrapping silently.

A SECOND FACT A FUTURE DOOR NEEDS: THE FAN IS WOUND THE WRONG WAY

zhao_geom_setup requires "2A > 0" (GEOM.CLIP normalised it). The fan's
2A = -2*half_w*(half_drop + side_sub) is NEGATIVE for every size; size 0 gives
2A = 0, the degenerate GEOM.CLIP rejects and GEOM.SETUP does not. Neither is a
defect -- they are the normalisation nobody yet does for particles -- and both
are now pinned by committed checks so the door's author inherits the
measurement, not the bug.

RECOMMENDATION (recorded, not acted on -- owner-shaped)

The honest door is at GEOM.CLIP's INPUT, not GEOM.SETUP's: entering there gives
winding normalisation, 2A, the bbox, the zero-area reject and all three tines
in step, for free, from blocks already composed. (Note the bbox MUST be
scissored: a size-255 particle at the rail needs max_x = 2059 px against a
signed [11:0] port maximum of 2047 -- which zhao_part_expand's header already
endorses as correct.) What it still needs is a particle's material identity and
an attribute law for the untextured case -- the latter shared with TERRAIN,
which argues for deciding it once.

CORRECTIONS TO THE BRIEF (verified in my own tree)

* "three .tri_ax_i connections" is true, but only TWO are GEOM.SETUP-family
  arms; the rp_o_ax one is zhao_geom_clip's OWN INPUT. The third connection is
  u_geom_attrpack -- which is what revealed the fork.
* zhao_geom_setup.sv is in fpga/rtl/geometry/, not fpga/rtl/prod/.
* zhao_material_window.sv is in fpga/rtl/texture/.
Everything else verified as written.

TESTS -- BUILT AND RUN (R60), NOT MERELY LINTED

  part_expand_directed     565 -> 745 checks passed
  part_expand_random 2000  19470 -> 21217 checks passed

Baseline measured by stashing the change and rebuilding, so the delta is not a
stale binary. verilator_bin --lint-only -Wall on zhao_part_expand.sv: RC 0.
Built by direct compilation, STATIC linked -- the mixed winlibs/mingw
toolchains here give STATUS_ENTRYPOINT_NOT_FOUND on a dynamically linked test
binary, which looks exactly like a test crash and is not.

NOT DONE, DELIBERATELY: no arbiter (it would be deleted by whoever solves the
attribute law -- a "built, installed nowhere" cheque); no 22->21 narrowing
anywhere (lossless, but nothing consumes it yet); no FORGE.PRIM/PRIM_EVAL/CLIFF.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>

