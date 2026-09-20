# FINDINGS -- forge lane (gz/forge)

*The harness blocks subagents from writing report files, so this packet put its
report in a COMMIT MESSAGE. Transcribed here verbatim by the coordinator, because
files under `fpga/rtl/` cite this path and a citation to a file that does not
exist is an uncashed cheque one level down. Source commit: `7af03a10`.*

---

FORGE: FINDINGS-forge.md (harness blocked the file; it is this message), plus
the world-vertex-consumer correction and the two readings of the arena route.

THE HARNESS REFUSED THE REPORT FILE, as it did for the geomlod lane, so this
commit message IS FINDINGS-forge.md. Coordinator: transcribe it if you want it
on disk.

================= FINDINGS - FORGE lane (gz/forge) =================

Branch gz/forge, base e0484b78.
REGISTER 22 AT ENTRY AND 22 AT THIS COMMIT (10 tie-offs + 11 disconnected +
1 unbuilt), matching the brief exactly.

NO GAP CLOSED. NO GAP OPENED. All five of my blocks are refused; every refusal
was re-verified rather than inherited; two now rest on a different cause than
the one on file; and the headline is that R75's ARENA ROUTE IS THE RIGHT
ARCHITECTURE AND IS NOT REACHABLE TODAY -- one reading of it would have
composed a DEADLOCK into the console behind a closed gap, with every gate green.

--- FORGE.SHADOW -------------------------------------------------------------

A CORRECTION TO THE PREMISE R75 WAS GIVEN, and the ruling is right anyway.
The finding handed up was "nothing in fpga/rtl consumes a world-vertex fan",
from a search for `vtx_`-shaped inputs. A CONSUMER EXISTS AND IS COMPOSED:
zhao_geom_group_seq's v_valid_i/v_ready_o/v_x_i/v_y_i/v_z_i (:208-212), three
signed [31:0] -- the same type and width as zhao_forge_shadow's vtx_{x,y,z}_o.
The search missed it because the port is spelled `v_`, not `vtx_`. CLAUDE.md
section 9: READ THE STRUCTURE, NOT THE CONVENTION. So the claim is not "no
consumer" but "the consumer's BATCH owes things a shadow hull cannot pay" --
narrower, actionable, and it makes R75 load-bearing rather than incidental.

A SEAM PORT COMPARISON HIDES: v_* is "LOCAL (REBASED) coords" with the arena
origin carrying the rebase; the shadow emits ABSOLUTE world fx16. Same width,
same type, DIFFERENT FRAME. Whoever composes this owes the rebase explicitly.

TWO READINGS OF "THE ARENA ROUTE", and only one hits the deadlock:
 A. THE BATCH ROUTE -- the hull becomes a meshlet in the existing front end.
    It inherits zhao_geom_vattr.sv:490's done_o, a six-term AND including
    lit_ord_q == uv_ord_q: every vertex needs a colour from GEOM.LIGHT and a
    u/v from GEOM.VDECODE. A shadow hull has NEITHER. va_done gates BOTH sides
    of the GROUP_SEQ -> REPLAY handshake (:7354, :13722), so the batch never
    releases its arena, GEOM_ARENAS exhausts, GEOM.PROJ_LANE backpressures and
    the whole front end wedges. NO timeout, NO abort, NO counter. The tell
    would be colours_written_o frozen while uv_staged_o and landings_o climb,
    and nothing differences them. CLAUDE.md's ALIVE-AT-ZERO-CPU shape in
    silicon. REFUSED.
 B. THE PRIVATE-ARENA ROUTE (RECOMMENDED) -- a second, small zhao_vertex_arena
    (16 deep against GEOM_DEPTH's 1089, which is exactly what R75's
    parenthetical prices), filled from the SAME client A and the SAME
    zhao_project_core, walked by a small fan replay whose triangles are
    ARBITRATED into GEOM.CLIP's door beside GEOM.REPLAY's. It does NOT
    duplicate the ratified projection arithmetic -- the duplication R75 exists
    to forbid -- and it touches GEOM.VATTR not at all, because the hull carries
    its OWN attribute packet (invw24 from its own w; u/v unused; rgb and alpha
    AUTHORED, which is this block's whole design). Reason A does not apply.
    Still needs: the client-A widening, an arbiter at GEOM.CLIP's door plus an
    authored material constant for u_material_window, the rebase above -- and
    it is STILL GATED ON THE ALPHA DECISION below.

REASON 2, WHICH GATES BOTH READINGS: THE CONSOLE HAS NO VERTEX ALPHA.
The blend ALU is NOT the missing piece -- zhao_raster_blend is real and
composed, six instances at zhao_raster_fragment.sv:490-510. Missing is every
path to it: zhao_geom_vattr.sv:474 injects ALPHA_C = fx16 1.0 (opaque) and its
header :69-74 says "ALPHA HAS NO PRODUCER"; zhao_geom_attrpack emits exactly
three planes (:225-227) and the rasteriser has exactly three lanes
(zhao_raster_tile_pipe_v2.sv:601), so slots 3..6 -- lit r/g/b AND alpha -- have
no interpolator and no carriage; tri_continuation_tail_i and
tri_fragment_state_i are OPEN BOUNDARIES by this file's own port table; and the
only alpha the console supplies is MAT_BASE_ALPHA_C = 8'hFF (:14164) by R48.
Every one of those I read first-hand.

CLIENT-A WIDENING, CONFIRMED INDEPENDENTLY AND WITH A DETAIL NOBODY WROTE DOWN.
GEOM_PAY_A_W is 16 and FULL: ARENA_W 3 + INDEX_W 12 = 15 with the owner tag at
bit 15 (zhao_part_project.sv:356, TAG_BIT = PAY_W - 1). WITH A THIRD OWNER THE
SINGLE TAG BIT STOPS BEING ENOUGH -- it becomes a two-bit owner field, so the
payload goes to 17 AT LEAST and geom_tag_collision_o's one-bit two-owner law
(:53) must be RE-AUTHORED with it. Sizing the widening as "16 -> 17" without
that is how a collision counter silently stops meaning anything.

CHECKED AND REJECTED, so nobody re-derives it: the PARTICLE route.
zhao_part_expand emits screen triangles with colour and depth bits, but
part_exp_* is ALREADY A DANGLING BOUNDARY of this core (:11951-11967). Routing
the shadow there moves a dangling producer from one port to another and puts
the register UP. Refused for the reason geomlod refused to compose lodstate.

--- FORGE.PRIM / FORGE.PRIM_EVAL -- cause SURVIVES, narrower ------------------

Searched: every page kind in spec/cartridge.md 4 (0 field programs, 1
sourceids, 2 generated-code manifest, 3 sky set, 4 terrain patch, 5 tone bank,
6 island patch, 7 island table, 8 creature form, 9 clip bank, plus 4b
SPECIES_TABLE and 4c the ladder table) -- NONE is a forge program page. And
j_family|j_segments|j_sides across all of fpga/rtl including synth/ and every
probe-named file: only the two blocks' own inputs and the LFSR noise source in
the generated zhao_prod_top.sv.
 * THE ABI CANNOT NAME ONE OF THE SIX FAMILIES. spec/commands.zidl:133-136
   declares enum forge_kind : u8 with EXACTLY ONE member,
   FORGE_HEIGHTFIELD_PATCH = 0, and :131 calls it "the one implemented kind".
   So the gap is a missing page kind AND a missing enum member set.
 * PRIM AND PRIM_EVAL DO NOT MEET BY A WIRE AND MUST NOT BE WIRED. They are the
   two HALVES of a meshlet -- indices and positions -- joined only by an
   ordering convention at zhao_forge_prim_eval.sv:24-32. And PRIM_EVAL is the
   LIGHTNING evaluator (reports/ADDLIGHTNING.md), RIBBON family only, so even
   with a page and an enum four of the six families have no evaluator.

--- FORGE.CLIFF -- cause SURVIVES; two things it is NOT -----------------------

 * `solid\w*_o` as an output port across every subdirectory of fpga/rtl
   including synth/ and the probes: ZERO HITS.
 * `vdist` across fpga/rtl: four files, all the wrong end (two consumers, the
   refusal prose, the LFSR harness). Real only in the C++ oracle.
 * zhao_probe_walk_earth.sv IS a real lattice walker and is the WRONG one --
   four-wide world (x,z) for the FIELD v3 executor, no page ci/cj, no cell
   extents, no solid bits, no vdist. Recorded as a near-miss.
 * zhao_forge_cliff_ram.sv IS NOT A CHILD of zhao_forge_cliff -- its own first
   lines call it "the bitmap-RAM CANDIDATE beside the golden". A RIVAL. So
   "compose FORGE.CLIFF" is ALSO a latest-version question with two candidates
   and no ruling. zhao_forge_jitter_rom belongs to PRIM_EVAL (:314).
 * design/blocks.yml declares inputs:[terrain_mesh] upstream:[TERRAIN.TESS],
   which does not fit the port shape at all. A ledger edge read as a port.

--- GEOM.PARAMBUF -- cause SURVIVES, but ALL THREE CITATIONS WERE STALE -------

The cause is NOT "blocked on the behavioural SDRAM model" (that was a
paraphrase); it is that nothing writes the three record types into memory. It
got STRONGER when checked. But every line number had drifted and one named a
mechanism that does not exist:
   meshfetch :319 -> :340 ; assetfetch :341 -> :360 ;
   mem_adapter :190 -> NO `write = 1'b0` EXISTS IN THAT FILE. It is read-only
   by a stronger mechanism: .FORCE_READ(1'b1) at :175.
Two more clients found agreeing (drawjob:308, ladderbank:222). The five writers
are now NAMED individually instead of "two terrain paths": fbwrite:218,
frameblit:391, mem_upload:394, pageloader:381, writeback:583. All re-counted.

--- THE SIXTEENTH FALSE-ABSENCE CLAIM: I29, refuted in its own file -----------

I29's cause was "there is no behavioural SDRAM model in this tree".
sim/models/zhao_sdram_model.sv exists. The I23 record ~1400 lines ABOVE I29 IN
THE SAME FILE is the correction of that exact sentence, written the same day.
I29's REAL blocker is a RULING: cartridge.md 4 freezes kind 8's BONE HIERARCHY
and kind 9 until Phase-12; 4c lifted it only for the header and ladder table.

--- OWNER DECISIONS FOUND -- two, each with a recommendation ------------------

D-D. THE CONSOLE HAS NO VERTEX ALPHA AND TWO RATIFIED STATEMENTS DISAGREE.
R48 fixes material alpha at 8'hFF reasoning "no ratified vertex format carries
alpha"; FORGE.SHADOW's contract requires "ordinary TRANSPARENT geometry". Three
blocks wrote the same gap into their own headers and none says who closes it --
the projector's-two-cores shape.
RECOMMENDATION: take the FLAT-ALPHA route and do NOT widen the vertex packet
for v1. zhao_forge_shadow.sv:295 is `vtx_alpha_o = strength_q` -- ALPHA IS
CONSTANT OVER THE HULL. So what is needed is a PER-PRIMITIVE alpha, and the
console already has carriage for exactly that: tri_continuation_tail_i's flat
per-triangle vertex_alpha feeding the composed blend ALU. Giving that open
boundary a producer is far smaller than a fourth interpolated plane, buys
transparency for PART.EXPAND and the compositor too, and leaves R48 true.
Interpolated per-vertex alpha is a real feature (a 4th attrpack lane AND a 4th
rasteriser lane) and should be commissioned as one, not smuggled in as part of
closing a shadow gap.
THIS DECISION GATES THE REST: until it is ruled, building more of R68's
sub-build 4 is an uncashed cheque -- a prerequisite for a consumer that cannot
legally exist yet.

D-E. I29 NEEDS A SECOND PARTIAL LIFT OF THE KIND-8 FREEZE, R26's SHAPE EXACTLY.
The pose decoder and its palette store are composed and GEOM.SKIN reads them --
"the missing thing is the BYTES, not the path" is the entry's own right
sentence, and the bytes are frozen by ruling.
RECOMMENDATION: grant it; the format already anticipates it. 4c's body_off
"names the byte offset at which that body will begin, so the unfrozen half can
be appended later without moving a byte of the frozen half". A second partial
lift -- the bone hierarchy as a kind-8 body section and one 30 Hz clip frame as
a minimal kind-9 -- costs nothing already frozen and unblocks a composed
decoder that is otherwise dead weight in the fit. If the owner prefers to hold
the freeze, SAY SO, because I29 is then a Phase-12 item rather than a gap this
campaign can close and the register should be told.

--- INSTRUMENT NOTES ----------------------------------------------------------

 * uncashed_cheques.py ALREADY handles the zhao_prod_top.sv trap. All five of
   my blocks ARE instantiated -- in the generated LFSR pricing harness (:631,
   :687, :742, :1683) and as Verilator test tops. Check 1 says so itself:
   "Check 1 cannot see these -- every one of them IS instantiated ... Rooted at
   every step, adopted at none." Not a defect; recorded because "nothing
   instantiates them" would be a FALSE sentence if anyone wrote it.
 * The zhao_geom_vattr starvation in route A HAS NO DETECTOR. I did not add
   one: a counter there is an output port on a composed leaf, costing its whole
   instantiation chain plus a gen_console_board regeneration, and it closes no
   gap. Recorded as that block's next owner's first job. I built no new guard,
   so none of mine is owed a firing.

--- GATES AT THIS COMMIT ------------------------------------------------------

register 22 (RC1, normal) . inventory OK . prod_manifest OK . gen_prod_top
fresh . gen_console_board FRESH . mutant_copy_drift OK . quartus17_syntax RC0 .
shell_paired_diff fresh . check_case_labels RC0 . smoke -LintOnly RC0.
R60 directed tests BUILT AND RUN: forge_shadow_directed 39 checks 0 failed .
forge_prim_directed 11 . forge_prim_eval_directed 968 . forge_cliff_directed
all green (worst page 11,946 clocks) . geom_parambuf_directed 16 .
cmd_exec_directed 677. All RC0.
Five smoke forms run sequentially at this commit; results reported separately.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>

