// zhao_post_composite.sv -- the compositor, ONE BOUNDED LINE STREAM (ruling R6).
//
// ---------------------------------------------------------------------------
// THE ORDER IS THE CONTRACT, AND THE ORDER IS VISIBLE
// ---------------------------------------------------------------------------
// POST.COMPOSITE.md calls its stage-order paragraph "the contract's most
// load-bearing paragraph", because grading before bloom and grading after
// bloom are different pictures and no amount of reading a block list would
// reveal which was intended. The pipeline below is that list, one stage per
// line, in order:
//
//   stage 0  the raster pointer; read the compact DISPLACEMENT cell
//   stage 1  combine + CLAMP the displacement ONCE; address the line ring and
//            the glow/ink plane THROUGH THE SAME DISPLACED COORDINATE
//   stage 2  world colour, blurred glow and ink arrive together
//   stage 3  ATMOSPHERE / haze                            (visible order 4)
//   stage 4  bloom / glow                                 (visible order 5)
//   stage 5  palette curves R[32] G[64] B[32]           \ (visible order 6)
//   stage 6  the signed 3x3 Q2.14 matrix, three sums     |
//   stage 7  round-half-up, bias, saturate               /
//   stage 8  flash / tint                                 (visible order 7)
//   stage 9  exterior ink, LAST on the world image        (visible order 8)
//            >>> POST.ECHO TAP IS HERE <<<
//   stage 10 HUD                                          (visible order 9)
//
// Three details in that list are DECISIONS, not sequencing, and each is
// implemented as a decision rather than as a happy accident:
//
//   * ONE displacement field, sampled ONCE. The ruling is explicit -- "do not
//     repeatedly resample the framebuffer once per effect". Refraction,
//     shockwave and heat haze have ALREADY been summed by POST.GATHER into one
//     compact cell (its contract: "displacement accumulates as signed 8.8 per
//     axis, clamped to a declared bound ... contributions combine BEFORE
//     sampling"); this block reads that one cell, clamps once more against the
//     frame/view edge, and samples once. There is no per-effect resample
//     anywhere below and no second framebuffer read port to make one with.
//   * Glow and ink ride the SAME displaced coordinate as the world. An outline
//     that stayed put while the creature bent would read as a printing error.
//     `gg_cx_o`/`gg_cy_o` are driven from the same `xs_c`/`ys_c` that address
//     the ring -- not from the undisplaced pixel.
//   * Ink is overlaid AFTER the flash. "The default flash does not wash out the
//     line; changing that later is an explicit artistic mode, not an accidental
//     consequence of stage order."
//
// ---------------------------------------------------------------------------
// WHY THERE IS A NINE-LINE RING, AND WHAT THE LAG ACTUALLY HAS TO BE
// ---------------------------------------------------------------------------
// R5 clamps POST.GATHER's displacement to X in [-8,+8] and Y in [-4,+4]. R6
// builds this block against exactly that bound: keep NINE complete source
// lines, reach +/-8 horizontally from them, and "delay in-place writes until
// the old line can no longer be sampled".
//
// With nine slots and slot(L) = L mod 9, the write pointer and the read pointer
// walk the same raster at the same rate. Two things must hold at once and they
// pull in opposite directions:
//
//   (a) line y+4 must already be written where the output will read it, so the
//       write pointer has to be AHEAD by LAG_LINES = 4 lines;
//   (b) the write at column x_in lands in the slot that still holds line y-4,
//       so it must not yet have reached any column the output can still reach.
//       The output reaches x_out+8, so the ring READ needs x_in > x_out + 8.
//
// The number that matters for (b) is the lag measured AT THE RING READ, which
// is stage 1 -- two steps after the pointer that LAG_PX is defined against.
// So the effective column lead is LAG_PX + 2, and (b) needs LAG_PX + 2 >= 9,
// i.e. LAG_PX >= 7. LAG_PX = 9 is shipped: it is the number the +/-8 argument
// is actually about, and it leaves two columns of margin rather than none.
// This is the contract's "small output-line delay queue if needed" -- it turns
// out to be nine pixels, not a line, and nine source lines are then sufficient.
//
// `ring_hazard_o` is the detector for that whole argument, and it is
// structurally unreachable while LAG_PX >= 7. See the counter's own note.
//
// ---------------------------------------------------------------------------
// THE M10K / ALM TRADE, WITH NUMBERS -- THE CEILING MOVED, SO THE TABLE SHIPS
// ---------------------------------------------------------------------------
// The contract caps this block at 3,500 ALMs, 12 DSPs, <= 8 M10K. The M10K line
// of that cap has been superseded:
//
//   "Using some more M10K is fine, we have enough, particularly if it saves
//    ALMs. They're our only weapon against our massive ALM debt."
//                     -- Fabian, 2026-09-18
//            reports/OWNER-RULING-M10K-CEILINGS-20260918.md
//
// So the earlier arrangement -- nine real multipliers, with the conflict
// recorded rather than decided -- is withdrawn, and the owner plan's section
// 11.2 exact preparation ships.
//
//   line ring  NLINE * LINE_W * 16 b. At LINE_W = 384 (Z60's full width, and
//              >= Storm's 320 and a Duo VIEW's 256): 9 * 384 = 3,456 entries
//              x 16 b = 55,296 b. At the M10K's 512x20 shape, ceil(3456/512)
//              = 7 blocks.
//   pv table   32 + 64 + 32 = 128 entries, three signed-24 products each
//              -> 9,216 logical bits, THREE SIMULTANEOUS 72-BIT READS
//              -> at the 256x40 shape, ceil(72/40) = 2 blocks per memory,
//                 x 3 memories = SIX. "Six, not one."
//              -> 12 if double-banked for an atomic swap.
//              -> counted SEPARATELY from line / glow / displacement memory,
//                 as the plan requires.
//
//   13 M10K total, 19 double-banked, against a 553-M10K device: 3.4%.
//
// WHAT THE TABLE ACTUALLY BUYS, AND THE CONDITION THAT DECIDES IT. It removes
// nine signed 16x8 multipliers from the per-pixel path. What that is WORTH
// depends on where those multipliers would have landed, and that has not been
// measured:
//
//   * in LOGIC (multstyle "logic", which area pressure pushes toward): nine
//     16x8 signed multipliers are roughly 1,200-1,300 ALMs. Against that the
//     table costs six 24-bit adds (~145 ALMs) and widens the stage-5 registers
//     from 3x8 to 3x72 bits (+192 flops, ~96 ALMs). Net roughly -1,000 ALMs.
//   * in DSP BLOCKS: it saves about five DSPs and costs ~250 ALMs, which is a
//     BAD trade and NOT the justification. The ruling is explicit that it is
//     "not permission to spend memory to remove DSPs" -- 112 are available and
//     no campaign has been DSP-bound.
//
// So the case rests entirely on the logic-multiplier scenario, and the first
// fit should check which one happened. At the standing ~200 ALM-per-M10K
// ranking heuristic, six blocks "buy" ~1,200 ALMs and this returns ~1,000 --
// it ranks at the boundary, and the heuristic is explicitly "a ranking
// heuristic only on NET parent measurements", so it ranks and does not close.
//
// EVERY NUMBER ABOVE IS SHAPE ARITHMETIC, NOT A MEASUREMENT. Plan 14.5:
// "Physical M10K reserve must be measured, not inferred from logical bit
// occupancy." Whether Quartus infers the ring as M10K AT ALL is unverified; so
// is whether it packs the 72-bit tables two-to-a-memory. Only a fit reports
// what this cost, and no fit has been run.
//
// THE SEQUENCED WORKER IS A DIFFERENT CANDIDATE AND ITS SAVING IS NOT ADDED.
// The plan names a three-product sequenced worker as an alternative and warns
// that the two savings must not be summed, because they replace the same work.
// No combined figure appears anywhere in this file.
//
// WHERE THE KNOBS WENT, because folding a table is exactly how a value stops
// being adjustable. The curves and the matrix are no longer RTL ports -- they
// are the INPUTS TO THE GENERATOR, `zref::post::grade_product_vector`, which is
// committed in the oracle tree and not buried in a test. They are still named,
// still editable, still authored values; one level up. The bias stayed a live
// port on purpose: folding it in would have turned a per-frame knob into a
// 128-entry regeneration.
//
// STILL NOT DONE, DELIBERATELY: no generic 65,536-entry RGB565 remap (the
// contract forbids it -- 128 KiB before shape is even considered -- and the
// plan repeats the prohibition by name, adding that "full-frame lookup tables
// and ignored port replication are not free memory tricks"). No port
// replication anywhere: the ring has one write port and one read port, and the
// three table memories have one read each because they are indexed by three
// different channels -- not because a port was duplicated to fake bandwidth.
//
// ESTIMATES, NOT MEASUREMENTS. Every number in this section is arithmetic on
// shapes, not a Quartus report. This block has never been through quartus_map
// or a fit. Verilator lint-clean is NOT synthesizability -- the elaboration
// guard below is not even executed by `--lint-only`, and two SystemVerilog
// forms are on record as passing Verilator with zero diagnostics and failing
// quartus_map outright.
//
// SO: `verilator --lint-only -Wall` reports 0 warnings and 0 errors on this
// file, and that settles ONE TOOL'S OPINION and nothing else. The constructs
// here that have NOT been shown synthesizable, and that a first quartus_map
// should be expected to argue with, are named rather than hoped about:
//   * `automatic` variable declarations inside `always_ff` begin/end blocks
//     (`nslot`) -- the house pattern, borrowed from zhao_post_gather, which has
//     not been through Quartus either;
//   * indexed part-selects into a register (`k6r_q[0*PRODW +: PRODW]`);
//   * three 72-bit memories, which Quartus may or may not choose to infer as
//     M10K at all -- the whole table argument rests on it doing so;
//   * size casts applied to expressions, e.g. `(SW+2)'(dy_eff_c)` and
//     `32'((front_v_c ? 1 : 0) + ...)`;
//   * `int'(NLINE)` in a for-loop bound.
// There is no generate block and no module-scope elaboration `if` here, which
// are the two forms already known to fail; the `$fatal` guard is inside
// `initial begin ... end` as Quartus 17.0 requires.
//
// Multiplier sites, COUNTED rather than estimated (the combiner's lesson --
// `unit_mul` inside every arm of a case statement is how 2 DSPs became 8).
// There are now exactly TWO multiplying stages, and no multiply inside any case
// arm anywhere in this file:
//   stage 3  atmosphere blend      6 products of 8b x 9b   (unit_lerp x3)
//   stage 6  the colour transform  0 -- it is three table reads and three adds
//   stage 8  flash/tint blend      6 products of 8b x 9b   (unit_lerp x3)
//   TOTAL                         12 products, down from 21.
// The ADD arm of the atmosphere blend uses `unit_mul` (3 products) instead of
// `unit_lerp` (6); the two arms are mutually exclusive in the same stage, so
// synthesis shares the multipliers and the site count above is the ceiling,
// not the sum of the arms. On Cyclone V's variable-precision DSP (two 18x18 or
// three 9x9 per block) that is about 2 + 0 + 2 = 4 blocks against the ceiling's
// 12. UNMEASURED -- and per the ruling, the DSP drop is a side effect of this
// arrangement, not its justification.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS NOT, AND WHERE THE SEAMS ARE
// ---------------------------------------------------------------------------
// * PART A, the optional quarter-res glow prep, is NOT here. The contract
//   describes it as "a separable blur over the compact glow plane: one
//   horizontal and one vertical quarter-res pass. No full-resolution reread" --
//   a different pass over a different buffer, and the contract calls it
//   optional. This block consumes the glow plane through `gg_glow_i` and does
//   not care whether it has been blurred. THE SEAM IS `gg_*`: a blur module
//   sits between POST.GATHER's plane and that port, or nothing does.
// * POST.ECHO is NOT here, and must not be. Its contract is DEFERRED and its
//   behavioural sections are "Deliberately unwritten"; implementing echo
//   behaviour inside composite would make the design look decided when it is
//   not. What POST.ECHO's contract actually asks of THIS block is one sentence:
//   "POST.COMPOSITE should not make the post-ink/pre-HUD image unavailable ...
//   it is a tap point, not a buffer." THE SEAM IS `echo_valid_o`/`echo_rgb_o`:
//   the stage-9 colour, registered alongside the main output on the same beat,
//   costing two ports and one 16-bit register. No capture buffer is allocated,
//   per the ruling, and there is no echo enable, no echo address and no echo
//   write path -- adding any of those is what the deferral forbids.
// * BACKDROP is NOT here. TWOD.PLANE's contract puts role 0 BACKDROP "in world
//   setup" and role 1 ATMOSPHERE "post stage 4 -- over the displaced world,
//   before bloom/grade". So `s_rgb_i` arrives already over the backdrop, and
//   only the atmosphere sheet enters this block, at stage 3. Compositing the
//   backdrop here as well would be a second implementation of an agreement
//   that already has an owner.
// * The framebuffer WRITE is not here either; this block emits a pixel stream
//   and owns the exclusive read/write lease around it. "HUD follows post and
//   never forces another read" -- which is why HUD is a stage of this pipeline
//   and not a second pass.
// * ONE atmosphere sheet, not two. TWOD.PLANE allows both slots to carry role 1
//   ("same role in both slots: slot 0 composites first"). This block composites
//   ONE at stage 3. A second would be another six products and would put the
//   DSP estimate over the 12 ceiling, so it is NAMED AS NOT BUILT rather than
//   half-provided. The seam is the `atm_*` group, duplicated.
// * The RENDERED 3D AREA, not the displayed canvas, and in Duo ONE VIEW PER
//   PASS. The two surfaces differ only in Duo: the canvas is 512 x 240 and the
//   rendered area is two 256 x 192 views at y offset 24, leaving 48 rows that
//   are BLACK BORDER (`zhao_pkg.sv`, `spec/video_rules.md` 3.1) -- not HUD
//   scanlines, which is what an earlier draft of this header called them. The
//   distinction does not change the conclusion (border is black and HUD
//   bypasses post, so neither needs an effect-plane cell) but the reason has to
//   be the right one.
//
//   THE DUO PLANE IS 128 x 48 = 6,144 CELLS, addressed as TWO 64 x 48 views --
//   quarter of the RENDERED area, not of the displayed canvas. The contracts
//   said 128 x 60 = 7,680, which is quarter of the canvas, and the proof that
//   it is wrong is inside the contract's own throughput table: its Duo main
//   pass counts rendered pixels (98,304 = 2 x 256 x 192) while its glow pass
//   counts displayed cells (15,360 = 2 x 7,680). One row, two surfaces. Z60 and
//   Storm cannot expose it because their displayed area IS their rendered area.
//   Settled in reports/DUO-QUARTER-PLANE-GEOMETRY-20260918.md; `blocks.yml` was
//   already right ("96x60 Z60 / 2x64x48 Duo").
//
//   Corrected: Duo glow prep 12,288, Duo frame cost 98,304 + 12,288 = 110,592
//   (was 113,664, overstated by 3,072 work items). Z60 is unaffected: 92,160 +
//   11,520 = 103,680.
//
//   THE POINT IS NOT THE 1,536 SAVED CELLS. The owner has ruled memory is
//   affordable, so saving cells is not worth selling. The point is that
//   view-local addressing makes the per-view clamp STRUCTURAL: with two 64 x 48
//   planes there is no address that names the other view, so "a refraction
//   cannot reach across the split" stops being a comparator that has to be
//   right and becomes a thing that cannot happen. That is a bug class removed
//   for certain and an ALM saving that is unmeasured, in that order.
//   ENFORCED-BY: tests/compositor/post_composite_directed.cpp -- case 7, "DUO:
//   no bleed between views, STRUCTURALLY". It censuses the ADDRESS SPACE, not a
//   pixel value: no cell index may reach the other view's half, and its
//   `parked_bad` check is what makes that true for EVERY cycle rather than for
//   the cycles anyone happened to look at.
//
//   This block labels each pixel with `o_x_o`/`o_y_o` in VIEW coordinates; the
//   canvas assembly, the Duo border and the y offset belong downstream. A
//   capture or CRC taken here is of the rendered area, and saying which surface
//   a number describes is the whole point (completion plan 11.3).
//
// ---------------------------------------------------------------------------
// TWO INTEGRATION RULES THIS BLOCK CANNOT ENFORCE ALONE, STATED SO THEY ARE NOT
// ASSUMED SATISFIED
// ---------------------------------------------------------------------------
// * EVERY 1-CYCLE-LATENCY RESPONSE MUST BE HELD THROUGH A STALL. The four
//   request/response groups -- `gd_*`, `gg_*`, `atm_*` and `hud_*` -- all use
//   RASTER.RESOLVE's convention: address out in cycle N, data in cycle N+1.
//   When the pipeline stalls, the request holds, and the response MUST hold
//   with it. A synchronous memory does this for free; a plane or sprite engine
//   that free-runs does not, and the symptom is the NEXT pixel's atmosphere or
//   HUD landing on the pixel still sitting in the stage. This was found by the
//   directed bench's backpressure case, which failed on 70 pixels while every
//   other check passed -- the whole picture was right except where a stall had
//   happened. The step the response must wait for is reconstructible from the
//   handshake alone (`pipe_en = !o_valid || o_ready`;
//   `in_active = pipe_en && s_ready`; `step = pipe_en && (in_accept ||
//   !in_active)`), so no extra port is needed and none is provided.
// * NEVER POST-PROCESS THE CURRENTLY SCANNED-OUT FRONT BUFFER. This block reads
//   a source stream and emits a new one; it has no framebuffer addresses and
//   therefore cannot tell which slot it is being fed. The lease -- exclusive
//   after resolve, before publication -- is the integrator's to hold.
// * COMPLETION USES THE OWNED TERMINAL PROTOCOL, NOT AN INDEPENDENT SLOT SWAP.
//   What this block emits is `o_last_o` on the final pixel and a
//   `passes_completed_o` tick. It performs no swap, has no swap port, and must
//   not acquire one.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_post_composite #(
    // The widest view this instance can composite. 384 = Z60. Storm is 320 and
    // a Duo VIEW is 256, so 384 covers every mode -- see the M10K note: this
    // parameter is what keeps the ring inside the <= 8 M10K ceiling.
    parameter int unsigned LINE_W    = 384,
    parameter int unsigned MAX_H     = 240,
    // R6: nine complete source lines, reach +/-4 in Y from the middle one.
    parameter int unsigned NLINE     = 9,
    parameter int unsigned LAG_LINES = 4,
    // The output delay queue, in pixels. The ring read needs LAG_PX + 2 >= 9.
    // It is a PARAMETER because the ring's whole safety argument rests on this
    // number, and a number an argument rests on should be adjustable and
    // testable -- tests/mutants/ drives it to 0 to make `ring_hazard_o` fire.
    parameter int unsigned LAG_PX    = 9
) (
    input  var logic                 clk,
    input  var logic                 rst_n,

    // ---- frame geometry, per mode -------------------------------------------
    input  var logic                 frame_start_i,
    input  var logic [$clog2(LINE_W+1)-1:0] frame_w_i,
    input  var logic [$clog2(MAX_H +1)-1:0] frame_h_i,
    // WHICH VIEW THIS PASS IS. In Duo the block runs TWICE, once per 256 x 192
    // view, and `frame_w_i`/`frame_h_i` are the VIEW's size. So every x this
    // block ever forms is already view-local, and the ordinary clamp to
    // [0, frame_w-1] IS the per-view clamp. There is no split comparator, and
    // there is no address that names the other view.
    //
    // This bit is carried out on both plane address ports so the plane address
    // is COMPLETE at the interface (bank + cell) and the no-bleed property is
    // checkable at a port rather than argued about internals. It is a PASS
    // property, never computed per sample -- which is the whole reason the
    // property is structural.
    input  var logic                 view_sel_i,

    // ---- the resolved world, over the backdrop, raster order ----------------
    input  var logic                 s_valid_i,
    output var logic                 s_ready_o,
    input  var logic [15:0]          s_rgb_i,

    // ---- gather plane port A: the compact DISPLACEMENT cell -----------------
    // Addressed at the UNDISPLACED pixel. One response per request, fixed
    // 1-cycle latency, same convention as RASTER.RESOLVE's tile-read master.
    output var logic                 gd_req_v_o,
    output var logic                 gd_view_o,
    output var logic [$clog2(LINE_W+1)-3:0] gd_cx_o,
    output var logic [$clog2(MAX_H +1)-3:0] gd_cy_o,
    input  var logic                 gd_present_i,
    input  var logic signed [7:0]    gd_dx_i,
    input  var logic signed [7:0]    gd_dy_i,

    // ---- gather plane port B: blurred GLOW and INK --------------------------
    // Addressed at the DISPLACED coordinate -- the same one the ring is read
    // with. This port existing separately from port A is the whole of "glow and
    // ink are sampled through the same displaced coordinates as the world".
    output var logic                 gg_req_v_o,
    output var logic                 gg_view_o,
    output var logic [$clog2(LINE_W+1)-3:0] gg_cx_o,
    output var logic [$clog2(MAX_H +1)-3:0] gg_cy_o,
    input  var logic                 gg_present_i,
    input  var logic [15:0]          gg_glow_i,
    input  var logic                 gg_ink_i,

    // ---- atmosphere sheet (TWOD.PLANE role 1), consumed at stage 3 ----------
    output var logic                 atm_req_v_o,
    output var logic [$clog2(LINE_W+1)-1:0] atm_req_x_o,
    output var logic [$clog2(MAX_H +1)-1:0] atm_req_y_o,
    input  var logic                 atm_en_i,
    input  var logic                 atm_valid_i,
    input  var logic [15:0]          atm_rgb_i,
    input  var logic [7:0]           atm_opacity_i,  // unit8, value = raw/256
    input  var logic                 atm_add_i,      // 0 = ALPHA, 1 = ADD

    // ---- bloom --------------------------------------------------------------
    input  var logic [7:0]           bloom_gain_i,   // unit8

    // ---- the global colour transform ----------------------------------------
    // The curves and the matrix are GENERATED ASSETS, never runtime-computed,
    // and they are now PRE-MULTIPLIED. What loads here is one unrounded product
    // vector per curve entry -- see the header's table section. There is no
    // matrix port and no multiplier left in this stage.
    //
    // The BIAS stays a live port, because the plan keeps "the ORIGINAL final
    // bias/round/saturate" and because folding it into the table would make a
    // per-frame knob into a 128-entry regeneration.
    input  var logic                 grade_valid_i,
    input  var logic                 pv_we_i,
    input  var logic [1:0]           pv_sel_i,     // 0 = R curve, 1 = G, 2 = B
    input  var logic [5:0]           pv_addr_i,
    input  var logic [71:0]          pv_data_i,    // {outB, outG, outR}, s24 each
    input  var logic signed [8:0]    bias_r_i,
    input  var logic signed [8:0]    bias_g_i,
    input  var logic signed [8:0]    bias_b_i,

    // ---- flash / tint, stage 8 ----------------------------------------------
    input  var logic [15:0]          flash_rgb_i,
    input  var logic [7:0]           flash_amt_i,    // unit8

    // ---- exterior ink, stage 9 ----------------------------------------------
    input  var logic [15:0]          ink_rgb_i,

    // ---- HUD, stage 10 ------------------------------------------------------
    output var logic                 hud_req_v_o,
    output var logic [$clog2(LINE_W+1)-1:0] hud_req_x_o,
    output var logic [$clog2(MAX_H +1)-1:0] hud_req_y_o,
    input  var logic                 hud_valid_i,
    input  var logic [15:0]          hud_rgb_i,

    // ---- to the framebuffer writer ------------------------------------------
    output var logic                 o_valid_o,
    input  var logic                 o_ready_i,
    output var logic [15:0]          o_rgb_o,
    output var logic [$clog2(LINE_W+1)-1:0] o_x_o,
    output var logic [$clog2(MAX_H +1)-1:0] o_y_o,
    output var logic                 o_last_o,

    // ---- the POST.ECHO seam: post-ink, pre-HUD. A TAP, NOT A BUFFER. --------
    output var logic                 echo_valid_o,
    output var logic [15:0]          echo_rgb_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0]          displacement_edge_clamps_o,
    output var logic [31:0]          bloom_cells_contributing_o,
    output var logic [31:0]          passes_completed_o,
    output var logic [31:0]          grading_table_missing_o,
    output var logic [31:0]          plane_missing_o,

    // ---- the work census, kept in SEPARATE AXES ------------------------------
    // Completion plan section 11.1: "a single source fetch into the line system
    // is distinct from arbitrary repeated accesses within resident line RAM.
    // Count line fill, output writes, compact-plane reads and any glow-prep
    // passes. Do not equate work-item count with cycles if a pixel requires
    // several serialized operations."
    //
    // So these are three different numbers and they are NOT interchangeable:
    //   line_fill_writes_o  one per SOURCE pixel fetched into the ring. This is
    //                       the fetch the ruling says happens exactly once.
    //   output_writes_o     one per composited pixel ACCEPTED downstream.
    //   plane_reads_o       compact quarter-res plane reads: TWO per composited
    //                       pixel (port A for displacement at the undisplaced
    //                       pixel, port B for glow/ink at the displaced one).
    //                       That it is two and not one is exactly the kind of
    //                       serialized-operation fact that a single work-item
    //                       total would hide.
    // Glow-prep passes are NOT counted here because the glow prep is not in
    // this block -- see the seam note above. A zero here would be a lie about
    // work that is really happening somewhere else.
    output var logic [31:0]          line_fill_writes_o,
    output var logic [31:0]          output_writes_o,
    output var logic [31:0]          plane_reads_o,

    // Structurally unreachable while LAG_PX >= 7, which is the point: it is the
    // instrument for the line-ring safety argument, and an argument you cannot
    // fire stays an argument forever. tests/mutants/ holds a WRAPPER (not a
    // copy -- a wrapper cannot go stale) that instantiates this module with
    // LAG_PX = 0 and passes only when this counter moves.
    output var logic [31:0]          ring_hazard_o
);

  localparam int unsigned XW  = $clog2(LINE_W + 1);
  localparam int unsigned YW  = $clog2(MAX_H + 1);
  localparam int unsigned SW  = $clog2(NLINE);
  localparam int unsigned RING_DEPTH = NLINE * LINE_W;
  localparam int unsigned RAW = $clog2(RING_DEPTH);
  localparam int unsigned EW  = XW + 2;              // x plus a signed i8
  localparam int unsigned FW  = YW + 2;
  localparam int unsigned PW    = 27;                // the grading accumulator
  // One unrounded matrix product. |m| <= 32768 (Q2.14 in s16) and k <= 255, so
  // the largest is 32,768 * 255 = 8,355,840 and a signed 24-bit field holds
  // [-8,388,608, +8,388,607]. Exact with room to spare, and it would NOT fit in
  // 23 bits. Three per entry = 72; 128 entries = 9,216 logical bits.
  localparam int unsigned PRODW = 24;
  localparam int unsigned PVW   = 3 * PRODW;         // 72

  // Quartus 17.0 needs a module-scope elaboration check INSIDE an initial
  // block (a bare module-scope `if` is a syntax error there), and
  // `--lint-only` never runs it -- so a clean lint says nothing about this.
  initial begin
    if (NLINE != ((2 * LAG_LINES) + 1))
      $fatal(1, "zhao_post_composite: NLINE must be 2*LAG_LINES+1 (got %0d, %0d)",
             NLINE, LAG_LINES);
    if (LINE_W < 32)
      $fatal(1, "zhao_post_composite: LINE_W < 32 breaks the line-wrap half of the ring argument");
    if (XW < 3 || YW < 3)
      $fatal(1, "zhao_post_composite: frame too small for a quarter-resolution cell index");
    if (PVW != 72)
      $fatal(1, "zhao_post_composite: pv_data_i is declared 72 bits; PVW is %0d", PVW);
  end

  // ==========================================================================
  // The frozen unit8 arithmetic -- spec/qformats.md, transcribed not invented
  // ==========================================================================
  // unit_mul(a,b) = ((a*b) + 128) >> 8, clamp 255. The clamp is unreachable for
  // 8x8 inputs and is kept defensively exactly as the frozen pseudocode states.
  function automatic logic [7:0] unit_mul(input logic [7:0] a, input logic [7:0] b);
    /* verilator lint_off UNUSEDSIGNAL */
    logic [16:0] p;
    /* verilator lint_on UNUSEDSIGNAL */
    begin
      p = ({9'd0, a} * {9'd0, b}) + 17'd128;
      unit_mul = (p[16:8] > 9'd255) ? 8'd255 : p[15:8];
    end
  endfunction

  // The unit8 LERP in its TWO-product form, so the complement is the exact
  // 256 - opacity the frozen unit8 law states rather than an approximate
  // 255 - opacity. That is why opacity's complement needs nine bits.
  function automatic logic [7:0] unit_lerp(input logic [7:0] dst,
                                           input logic [7:0] src,
                                           input logic [7:0] w);
    /* verilator lint_off UNUSEDSIGNAL */
    logic [17:0] p;
    /* verilator lint_on UNUSEDSIGNAL */
    logic [8:0]  wc;
    begin
      wc = 9'd256 - {1'b0, w};
      p  = 18'({10'd0, src} * {10'd0, w}) + 18'({9'd0, dst} * {9'd0, wc}) + 18'd128;
      unit_lerp = (p[17:8] > 10'd255) ? 8'd255 : p[15:8];
    end
  endfunction

  function automatic logic [7:0] sat_add8(input logic [7:0] a, input logic [7:0] b);
    logic [8:0] s;
    begin
      s = {1'b0, a} + {1'b0, b};
      sat_add8 = s[8] ? 8'hFF : s[7:0];
    end
  endfunction

  // RGB565 <-> 8-bit working colour. The expansion is the frozen replication
  // law and it round-trips EXACTLY (take the top 5 of {v5,v5[4:2]} and you have
  // v5 back), which is what lets "a displaced sample cannot invent a colour
  // that was not in the frame" survive an unpack and a repack.
  function automatic logic [7:0] exp5(input logic [4:0] v);
    exp5 = {v, v[4:2]};
  endfunction
  function automatic logic [7:0] exp6(input logic [5:0] v);
    exp6 = {v, v[5:4]};
  endfunction
  // The dropped low bits ARE the RGB565 truncation -- the same shape
  // zhao_texture_combine documents for unit_mul's discarded remainder. There is
  // no dither here: this block's output is already-resolved colour, and
  // RASTER.RESOLVE owns the charter's ordered dither.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic [15:0] pack565(input logic [7:0] r,
                                          input logic [7:0] g,
                                          input logic [7:0] b);
    pack565 = {r[7:3], g[7:2], b[7:3]};
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  // ==========================================================================
  // Handshake: ONE `step` moves the whole machine by one pixel
  // ==========================================================================
  // The output back-pressures it, and an input bubble also stalls it, because
  // the write pointer and the read pointer must stay EXACTLY the designed
  // distance apart or the ring argument above is void. That is the correctness
  // rule the contract states: "a stall holds the stream, which is safe because
  // no stage holds state that expires -- but the line ring does hold state that
  // must not be overwritten while it is still reachable".
  logic pipe_en_c, step_c, in_active_c, in_accept_c, front_v_c;

  logic [XW-1:0] x_in_q;
  logic [YW-1:0] y_in_q;
  logic [SW-1:0] wr_slot_q;
  logic          wr_active_q;

  logic [XW-1:0] x_f_q;      // the front (stage-0) raster pointer
  logic [YW-1:0] y_f_q;
  logic [SW-1:0] s_f_q;
  logic          front_run_q;

  logic [9:0]    v_q;        // v_q[k] = the pixel in stage k+1 is valid

  assign in_active_c = wr_active_q;
  assign pipe_en_c   = (!o_valid_o) || o_ready_i;
  assign in_accept_c = pipe_en_c && in_active_c && s_valid_i;
  assign step_c      = pipe_en_c && (in_accept_c || !in_active_c);
  assign s_ready_o   = pipe_en_c && in_active_c;
  assign front_v_c   = front_run_q && (y_f_q < frame_h_i);

  // ==========================================================================
  // The line ring -- NLINE complete source lines, flat, one write port and one
  // read port so it infers a simple dual-port M10K rather than registers.
  // ==========================================================================
  // Read-during-write behaviour is irrelevant here and that is a claim, not an
  // assumption: the write address and the read address provably never coincide
  // (same slot implies xs < x_in on the new-line side and xs >= x_in + 3 on the
  // old-line side), and `ring_hazard_o` is the instrument for that claim.
  logic [15:0]    ring_q [0:RING_DEPTH-1];
  logic [RAW-1:0] ring_wa_c, ring_ra_c;
  logic [15:0]    ring_rd_q;

  // ==========================================================================
  // STAGE 0 -- the raster pointer, and the displacement cell address
  // ==========================================================================
  // The address is PARKED when the request is invalid. Without that, the front
  // pointer walks one row past the last line during the drain and the port
  // presents quarter-row `frame_h/4`, which is outside the view's plane -- an
  // address naming a cell that does not exist. It was harmless (the response is
  // discarded by `v_q`) and it was still wrong, because it made "no address
  // this block forms names a cell outside this view" false. The directed
  // bench's address-space census caught it; the claim is now true rather than
  // nearly true.
  assign gd_req_v_o = front_v_c;
  assign gd_view_o  = view_sel_i;
  assign gd_cx_o    = front_v_c ? x_f_q[XW-1:2] : '0;
  assign gd_cy_o    = front_v_c ? y_f_q[YW-1:2] : '0;

  logic [XW-1:0] x1_q;
  logic [YW-1:0] y1_q;
  logic [SW-1:0] s1_q;

  // ==========================================================================
  // STAGE 1 -- ONE clamp of ONE already-combined displacement field
  // ==========================================================================
  logic signed [7:0]  dx_c, dy_c;
  logic               dmiss_c;
  assign dmiss_c = !gd_present_i;
  assign dx_c    = gd_present_i ? gd_dx_i : 8'sd0;
  assign dy_c    = gd_present_i ? gd_dy_i : 8'sd0;

  // The top bits of the clamped selections are the sign/overflow headroom the
  // signed add needed; after the clamp they are provably zero, and only the
  // coordinate's own width is carried forward.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [EW-1:0] xr_c, xhi_c, xsel_c;
  logic signed [FW-1:0] yr_c, yhi_c, ysel_c;
  logic signed [FW-1:0] dy_eff_c;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [XW-1:0] xs_c;
  logic [YW-1:0] ys_c;
  logic          edge_clamp_c;

  always_comb begin
    xr_c = $signed({2'b00, x1_q}) + EW'(dx_c);
    yr_c = $signed({2'b00, y1_q}) + FW'(dy_c);

    // THE PER-VIEW CLAMP IS THIS CLAMP. There is no split comparator, because
    // the pass IS a view: `frame_w_i` is the view's width and x is view-local.
    // Clamping to [0, frame_w-1] therefore cannot produce a coordinate in
    // another view, and the cell index below cannot name another view's cell.
    // With one wide plane and a split comparator this would be a thing to test;
    // here it is a thing that cannot happen.
    // ENFORCED-BY: tests/compositor/post_composite_directed.cpp -- case 7 walks
    //   every cycle of a pass and fails if any formed index leaves this view.
    xhi_c = $signed({2'b00, frame_w_i}) - EW'(1);
    yhi_c = $signed({2'b00, frame_h_i}) - FW'(1);

    // Clamp, never wrap. Wrapping would sample the opposite side of the screen,
    // which is a spectacular and very confusing artefact.
    xsel_c = (xr_c < EW'(0)) ? EW'(0) : (xr_c > xhi_c) ? xhi_c : xr_c;
    ysel_c = (yr_c < FW'(0)) ? FW'(0) : (yr_c > yhi_c) ? yhi_c : yr_c;
    xs_c   = xsel_c[XW-1:0];
    ys_c   = ysel_c[YW-1:0];

    edge_clamp_c = (xr_c < EW'(0)) || (xr_c > xhi_c) || (yr_c < FW'(0)) || (yr_c > yhi_c);

    // The EFFECTIVE vertical step AFTER clamping -- the ring slot has to follow
    // where we actually sampled, not where we asked to.
    dy_eff_c = ysel_c - $signed({2'b00, y1_q});
  end

  // slot of ys_c, by wrapping s1_q. No divider; dy_eff is in [-4,+4].
  logic signed [SW+1:0] slot_sum_c;
  logic [SW-1:0]        slot_s_c;
  always_comb begin
    slot_sum_c = $signed({2'b00, s1_q}) + (SW+2)'(dy_eff_c);
    if (slot_sum_c < (SW+2)'(0))
      slot_s_c = SW'(slot_sum_c + (SW+2)'(NLINE));
    else if (slot_sum_c >= (SW+2)'(NLINE))
      slot_s_c = SW'(slot_sum_c - (SW+2)'(NLINE));
    else
      slot_s_c = SW'(slot_sum_c);
  end

  // slot * LINE_W: LINE_W is a constant, so this folds to shifts and one add.
  assign ring_wa_c = RAW'((32'(wr_slot_q) * 32'(LINE_W)) + 32'(x_in_q));
  assign ring_ra_c = RAW'((32'(slot_s_c)  * 32'(LINE_W)) + 32'(xs_c));

  // The SAME displaced coordinate addresses the glow/ink plane. If this ever
  // becomes x1_q/y1_q the outline stops following the creature and the frame
  // reads as a printing error.
  assign gg_req_v_o = v_q[0];
  assign gg_view_o  = view_sel_i;
  assign gg_cx_o    = v_q[0] ? xs_c[XW-1:2] : '0;
  assign gg_cy_o    = v_q[0] ? ys_c[YW-1:2] : '0;

  // ---- the ring hazard detector -------------------------------------------
  // WHAT THE TWO SIDES OF THE COMPARISON ARE CLOCKED BY, since that is the
  // question a detector has to answer about itself: the left side is the WRITE
  // pointer's record (which source line occupies each slot, and how far into
  // the current slot the write has got); the right side is the READ side's
  // computed (xs, ys), which is the output pointer PLUS the displacement. Both
  // advance on `step_c`, so this detector is structurally blind to a fault that
  // moves both pointers identically. What it CAN see is the RELATIVE lag
  // between them, which is exactly what LAG_PX sets and what a wrong LAG_PX
  // breaks -- and that is the thing the ring's safety rests on.
  //
  // The current write slot holds TWO lines at once: columns below x_in_q are
  // the new line, columns at or above it are still the line NLINE earlier. A
  // check that ignored that would false-positive at every line wrap, and a
  // detector that cries wolf at a line wrap is a detector nobody believes.
  logic [YW-1:0] line_id_q [0:NLINE-1];
  logic          hazard_c;
  always_comb begin
    if (wr_active_q && (slot_s_c == wr_slot_q)) begin
      hazard_c = (xs_c < x_in_q)
               ? (ys_c != y_in_q)
               : (((YW+1)'(ys_c) + (YW+1)'(NLINE)) != (YW+1)'(y_in_q));
    end else begin
      hazard_c = (line_id_q[slot_s_c] != ys_c);
    end
  end

  logic [XW-1:0] x2_q;
  logic [YW-1:0] y2_q;

  assign atm_req_v_o = v_q[1];
  assign atm_req_x_o = x2_q;
  assign atm_req_y_o = y2_q;

  // ==========================================================================
  // The pipeline registers, stage by stage
  // ==========================================================================
  logic [7:0] w3r_q, w3g_q, w3b_q;    // world colour at the displaced coord
  logic [7:0] g3r_q, g3g_q, g3b_q;    // blurred glow at the SAME coord
  logic       i3_q;                   // exterior ink at the SAME coord

  logic [7:0] c4r_q, c4g_q, c4b_q, g4r_q, g4g_q, g4b_q;
  logic       i4_q;

  logic [7:0] c5r_q, c5g_q, c5b_q;
  logic       i5_q, grade5_q;

  // THE PRODUCT-VECTOR TABLE. Three memories, 72 bits wide, read once each per
  // pixel -- the plan's "three simultaneous 72-bit reads". Each entry holds the
  // three UNROUNDED products of one curve entry with its own matrix column:
  //   [23:0]  contribution to output R
  //   [47:24] contribution to output G
  //   [71:48] contribution to output B
  // Nothing rounds here. Rounding a product before the sum is what would turn
  // this from an identity into an approximation, and it is the single error
  // this arrangement is most likely to acquire.
  logic [PVW-1:0] pv_r_q [0:31];
  logic [PVW-1:0] pv_g_q [0:63];
  logic [PVW-1:0] pv_b_q [0:31];

  logic [PVW-1:0] k6r_q, k6g_q, k6b_q;   // the three vectors, registered
  logic [7:0] c6r_q, c6g_q, c6b_q;    // the ungraded colour, carried for bypass
  logic       i6_q, grade6_q;

  // Registering the three SUMS rather than the nine addends keeps the stage
  // shaped like the contract's "one wide sum and one round-half-up per
  // channel". UNMEASURED for Fmax -- no fit has been run on this block, though
  // three 24-bit adds is a far shorter path than the multiply it replaces.
  logic signed [PW-1:0] a7r_q, a7g_q, a7b_q;
  logic [7:0] c7r_q, c7g_q, c7b_q;
  logic       i7_q, grade7_q;

  logic [7:0] c8r_q, c8g_q, c8b_q;
  logic       i8_q;
  logic [7:0] c9r_q, c9g_q, c9b_q;
  logic       i9_q;
  logic [15:0] c10_q;                 // post-ink, pre-HUD: the POST.ECHO tap

  // Three unrounded s24 products, sign-extended and summed. This replaced
  // `mac3` and its nine multiplies; the sum it forms is the SAME sum, which is
  // what makes the table an identity rather than a new colour convention.
  // Bound: 3 * 8,355,840 = 25,067,520, inside PW = 27 signed.
  function automatic logic signed [PW-1:0] sum3(input logic [PRODW-1:0] p0,
                                                input logic [PRODW-1:0] p1,
                                                input logic [PRODW-1:0] p2);
    sum3 = PW'($signed(p0)) + PW'($signed(p1)) + PW'($signed(p2));
  endfunction

  // ONE round-half-up per channel, then the bias, then ONE saturate.
  function automatic logic [7:0] fin(input logic signed [PW-1:0] acc,
                                     input logic signed [8:0] bias);
    logic signed [PW-1:0] r;
    begin
      r = (acc + $signed(PW'(8192))) >>> 14;
      r = r + PW'(bias);
      fin = (r < $signed(PW'(0)))   ? 8'd0
          : (r > $signed(PW'(255))) ? 8'd255
                                    : r[7:0];
    end
  endfunction

  // ==========================================================================
  // STAGE 10 -- the late raster pointer that labels the output
  // ==========================================================================
  logic [XW-1:0] x_l_q, x9_q;
  logic [YW-1:0] y_l_q, y9_q;
  logic          last9_q;
  assign hud_req_v_o = v_q[8];
  assign hud_req_x_o = x_l_q;
  assign hud_req_y_o = y_l_q;

  logic pass_missing_grade_q;

  // Every plane-absence this cycle, summed ONCE. Three separate nonblocking
  // increments of the same counter keep only the last, which is how a counter
  // silently under-reports -- POST.GATHER's header records the same trap.
  logic [1:0] pm_c;
  assign pm_c = 2'((v_q[0] && dmiss_c)                       ? 1 : 0)
              + 2'((v_q[1] && !gg_present_i)                 ? 1 : 0)
              + 2'((v_q[2] && atm_en_i && !atm_valid_i)      ? 1 : 0);

  // ==========================================================================
  // The one sequential block
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      x_in_q <= '0; y_in_q <= '0; wr_slot_q <= '0; wr_active_q <= 1'b0;
      x_f_q  <= '0; y_f_q  <= '0; s_f_q     <= '0; front_run_q <= 1'b0;
      x_l_q  <= '0; y_l_q  <= '0; x9_q <= '0; y9_q <= '0; last9_q <= 1'b0;
      v_q    <= '0;
      ring_rd_q <= '0;
      x1_q <= '0; y1_q <= '0; s1_q <= '0; x2_q <= '0; y2_q <= '0;
      w3r_q <= '0; w3g_q <= '0; w3b_q <= '0;
      g3r_q <= '0; g3g_q <= '0; g3b_q <= '0; i3_q <= 1'b0;
      c4r_q <= '0; c4g_q <= '0; c4b_q <= '0; i4_q <= 1'b0;
      g4r_q <= '0; g4g_q <= '0; g4b_q <= '0;
      c5r_q <= '0; c5g_q <= '0; c5b_q <= '0; i5_q <= 1'b0; grade5_q <= 1'b0;
      k6r_q <= '0; k6g_q <= '0; k6b_q <= '0;   // the three product vectors
      c6r_q <= '0; c6g_q <= '0; c6b_q <= '0; i6_q <= 1'b0; grade6_q <= 1'b0;
      a7r_q <= '0; a7g_q <= '0; a7b_q <= '0;
      c7r_q <= '0; c7g_q <= '0; c7b_q <= '0; i7_q <= 1'b0; grade7_q <= 1'b0;
      c8r_q <= '0; c8g_q <= '0; c8b_q <= '0; i8_q <= 1'b0;
      c9r_q <= '0; c9g_q <= '0; c9b_q <= '0; i9_q <= 1'b0;
      c10_q <= '0;
      o_valid_o <= 1'b0; o_rgb_o <= '0; o_x_o <= '0; o_y_o <= '0; o_last_o <= 1'b0;
      echo_valid_o <= 1'b0; echo_rgb_o <= '0;
      pass_missing_grade_q <= 1'b0;
      displacement_edge_clamps_o <= '0;
      bloom_cells_contributing_o <= '0;
      passes_completed_o         <= '0;
      grading_table_missing_o    <= '0;
      plane_missing_o            <= '0;
      line_fill_writes_o         <= '0;
      output_writes_o            <= '0;
      plane_reads_o              <= '0;
      ring_hazard_o              <= '0;
      for (int i = 0; i < int'(NLINE); i++) line_id_q[i] <= '1;
    end else begin
      // ---- product-vector table load, independent of the stream -----------
      // No multiply in any arm of this case, deliberately: `unit_mul` inside
      // every arm of a case statement is how a 2-DSP rule became 8 DSPs in the
      // combiner, and the habit that catches it is counting the sites.
      if (pv_we_i) begin
        case (pv_sel_i)
          2'd0:    pv_r_q[pv_addr_i[4:0]] <= pv_data_i;
          2'd1:    pv_g_q[pv_addr_i]      <= pv_data_i;
          2'd2:    pv_b_q[pv_addr_i[4:0]] <= pv_data_i;
          default: ;   // 3 is not a curve; the write is dropped
        endcase
      end

      // An OUTPUT WRITE is an accepted beat, which is a different event from a
      // step of the pipeline: a stalled beat steps nothing and writes nothing.
      if (o_valid_o && o_ready_i) output_writes_o <= output_writes_o + 32'd1;

      if (frame_start_i) begin
        x_in_q <= '0; y_in_q <= '0; wr_slot_q <= '0; wr_active_q <= 1'b1;
        x_f_q  <= '0; y_f_q  <= '0; s_f_q     <= '0; front_run_q <= 1'b0;
        x_l_q  <= '0; y_l_q  <= '0; last9_q <= 1'b0;
        v_q    <= '0;
        o_valid_o <= 1'b0;
        echo_valid_o <= 1'b0;
        pass_missing_grade_q <= 1'b0;
        for (int i = 0; i < int'(NLINE); i++) line_id_q[i] <= '1;
        line_id_q[0] <= '0;
      end else if (step_c) begin
        // ================= WRITE SIDE =====================================
        if (in_accept_c) begin
          automatic logic [SW-1:0] nslot =
              (wr_slot_q == SW'(NLINE - 1)) ? SW'(0) : (wr_slot_q + SW'(1));
          ring_q[ring_wa_c] <= s_rgb_i;
          // THE single source fetch into the line system. Everything after this
          // is a repeated access to resident line RAM, which is a different
          // axis and is not counted here.
          line_fill_writes_o <= line_fill_writes_o + 32'd1;
          if (x_in_q == (frame_w_i - XW'(1))) begin
            x_in_q <= '0;
            if (y_in_q == (frame_h_i - YW'(1))) begin
              wr_active_q <= 1'b0;
            end else begin
              y_in_q          <= y_in_q + YW'(1);
              wr_slot_q       <= nslot;
              line_id_q[nslot] <= y_in_q + YW'(1);
            end
          end else begin
            x_in_q <= x_in_q + XW'(1);
          end

          // The output is armed exactly LAG_LINES lines and LAG_PX pixels into
          // the frame. Both halves matter; see the header.
          if ((y_in_q == YW'(LAG_LINES)) && (x_in_q == XW'(LAG_PX))) front_run_q <= 1'b1;
        end

        // ================= READ SIDE, STAGE 0 =============================
        if (front_v_c) begin
          if (x_f_q == (frame_w_i - XW'(1))) begin
            x_f_q <= '0;
            y_f_q <= y_f_q + YW'(1);
            s_f_q <= (s_f_q == SW'(NLINE - 1)) ? SW'(0) : (s_f_q + SW'(1));
          end else begin
            x_f_q <= x_f_q + XW'(1);
          end
        end

        v_q  <= {v_q[8:0], front_v_c};
        x1_q <= x_f_q; y1_q <= y_f_q; s1_q <= s_f_q;

        // TWO compact-plane reads per composited pixel, counted as two: port A
        // is issued at stage 0 and port B at stage 1, and a single work-item
        // total would report one.
        if (front_v_c || v_q[0])
          plane_reads_o <= plane_reads_o + 32'((front_v_c ? 1 : 0) + (v_q[0] ? 1 : 0));

        // ================= STAGE 1 -- clamp once, address once ============
        ring_rd_q <= ring_q[ring_ra_c];
        x2_q      <= x1_q;
        y2_q      <= y1_q;
        if (v_q[0]) begin
          if (edge_clamp_c) displacement_edge_clamps_o <= displacement_edge_clamps_o + 32'd1;
          if (hazard_c)     ring_hazard_o              <= ring_hazard_o + 32'd1;
        end
        if (pm_c != 2'd0) plane_missing_o <= plane_missing_o + 32'(pm_c);

        // ================= STAGE 2 -- world, glow and ink together ========
        w3r_q <= exp5(ring_rd_q[15:11]);
        w3g_q <= exp6(ring_rd_q[10:5]);
        w3b_q <= exp5(ring_rd_q[4:0]);
        g3r_q <= gg_present_i ? exp5(gg_glow_i[15:11]) : 8'd0;
        g3g_q <= gg_present_i ? exp6(gg_glow_i[10:5])  : 8'd0;
        g3b_q <= gg_present_i ? exp5(gg_glow_i[4:0])   : 8'd0;
        i3_q  <= gg_present_i && gg_ink_i;

        // ================= STAGE 3 -- ATMOSPHERE ==========================
        // A plane that is enabled but did not arrive is treated as zero and
        // counted; the frame still composites. A wrong sheet is worse.
        if (atm_en_i && atm_valid_i) begin
          if (atm_add_i) begin
            c4r_q <= sat_add8(w3r_q, unit_mul(exp5(atm_rgb_i[15:11]), atm_opacity_i));
            c4g_q <= sat_add8(w3g_q, unit_mul(exp6(atm_rgb_i[10:5]),  atm_opacity_i));
            c4b_q <= sat_add8(w3b_q, unit_mul(exp5(atm_rgb_i[4:0]),   atm_opacity_i));
          end else begin
            c4r_q <= unit_lerp(w3r_q, exp5(atm_rgb_i[15:11]), atm_opacity_i);
            c4g_q <= unit_lerp(w3g_q, exp6(atm_rgb_i[10:5]),  atm_opacity_i);
            c4b_q <= unit_lerp(w3b_q, exp5(atm_rgb_i[4:0]),   atm_opacity_i);
          end
        end else begin
          c4r_q <= w3r_q; c4g_q <= w3g_q; c4b_q <= w3b_q;
        end
        g4r_q <= g3r_q; g4g_q <= g3g_q; g4b_q <= g3b_q;
        i4_q  <= i3_q;

        // ================= STAGE 4 -- BLOOM ===============================
        c5r_q <= sat_add8(c4r_q, unit_mul(g4r_q, bloom_gain_i));
        c5g_q <= sat_add8(c4g_q, unit_mul(g4g_q, bloom_gain_i));
        c5b_q <= sat_add8(c4b_q, unit_mul(g4b_q, bloom_gain_i));
        i5_q     <= i4_q;
        grade5_q <= grade_valid_i;
        if (v_q[3] && (bloom_gain_i != 8'd0) && ((g4r_q | g4g_q | g4b_q) != 8'd0))
          bloom_cells_contributing_o <= bloom_cells_contributing_o + 32'd1;

        // ================= STAGE 5 -- three 72-bit table reads =============
        // Indexed by the SAME 5/6/5 the curves were indexed by: the curve is
        // still the curve, it has simply been multiplied through in advance.
        k6r_q <= pv_r_q[c5r_q[7:3]];
        k6g_q <= pv_g_q[c5g_q[7:2]];
        k6b_q <= pv_b_q[c5b_q[7:3]];
        c6r_q <= c5r_q; c6g_q <= c5g_q; c6b_q <= c5b_q;
        i6_q  <= i5_q;  grade6_q <= grade5_q;
        if (v_q[4] && !grade5_q) pass_missing_grade_q <= 1'b1;

        // ================= STAGE 6 -- one wide sum per channel =============
        // Each output channel takes its own lane out of all three vectors. The
        // lane index is the OUTPUT channel and the vector is the INPUT channel;
        // transposing those two produces a plausible picture with the channels
        // cross-mixed, which is why the packing is spelled out here and in
        // zref::post::grade_product_vector rather than implied.
        a7r_q <= sum3(k6r_q[0*PRODW +: PRODW], k6g_q[0*PRODW +: PRODW], k6b_q[0*PRODW +: PRODW]);
        a7g_q <= sum3(k6r_q[1*PRODW +: PRODW], k6g_q[1*PRODW +: PRODW], k6b_q[1*PRODW +: PRODW]);
        a7b_q <= sum3(k6r_q[2*PRODW +: PRODW], k6g_q[2*PRODW +: PRODW], k6b_q[2*PRODW +: PRODW]);
        c7r_q <= c6r_q; c7g_q <= c6g_q; c7b_q <= c6b_q;
        i7_q  <= i6_q;  grade7_q <= grade6_q;

        // ================= STAGE 7 -- round, bias, saturate ================
        // A missing grading table passes through UNGRADED and is counted. A
        // wrong grade is worse than no grade.
        c8r_q <= grade7_q ? fin(a7r_q, bias_r_i) : c7r_q;
        c8g_q <= grade7_q ? fin(a7g_q, bias_g_i) : c7g_q;
        c8b_q <= grade7_q ? fin(a7b_q, bias_b_i) : c7b_q;
        i8_q  <= i7_q;

        // ================= STAGE 8 -- flash / tint =========================
        c9r_q <= unit_lerp(c8r_q, exp5(flash_rgb_i[15:11]), flash_amt_i);
        c9g_q <= unit_lerp(c8g_q, exp6(flash_rgb_i[10:5]),  flash_amt_i);
        c9b_q <= unit_lerp(c8b_q, exp5(flash_rgb_i[4:0]),   flash_amt_i);
        i9_q  <= i8_q;

        // ================= STAGE 9 -- exterior ink, LAST on the world ======
        // AFTER the flash, so a full-intensity flash cannot wash out the line.
        c10_q <= i9_q ? ink_rgb_i : pack565(c9r_q, c9g_q, c9b_q);
        if (v_q[8]) begin
          x9_q    <= x_l_q;
          y9_q    <= y_l_q;
          last9_q <= (x_l_q == (frame_w_i - XW'(1))) && (y_l_q == (frame_h_i - YW'(1)));
          if (x_l_q == (frame_w_i - XW'(1))) begin
            x_l_q <= '0;
            y_l_q <= y_l_q + YW'(1);
          end else begin
            x_l_q <= x_l_q + XW'(1);
          end
        end

        // ================= STAGE 10 -- HUD, and the output ================
        o_valid_o <= v_q[9];
        o_rgb_o   <= hud_valid_i ? hud_rgb_i : c10_q;
        o_x_o     <= x9_q;
        o_y_o     <= y9_q;
        o_last_o  <= last9_q;
        // The echo tap rides the SAME beat as the output, one stage upstream of
        // the HUD. Same valid, same pixel, no buffer.
        echo_valid_o <= v_q[9];
        echo_rgb_o   <= c10_q;

        if (v_q[9] && last9_q) begin
          passes_completed_o <= passes_completed_o + 32'd1;
          if (pass_missing_grade_q) grading_table_missing_o <= grading_table_missing_o + 32'd1;
          pass_missing_grade_q <= 1'b0;
        end
      end else if (pipe_en_c) begin
        // Accepted, and nothing new to offer: an input bubble is a hole in the
        // output, never a shift of the two pointers relative to each other.
        o_valid_o    <= 1'b0;
        echo_valid_o <= 1'b0;
      end
    end
  end

endmodule : zhao_post_composite

`default_nettype wire
