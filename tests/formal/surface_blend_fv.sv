// surface_blend_fv.sv — formal harness for the surface-sheet blend
// (SURFACE.STAMP / ZH-052; property surface_blend.sby).
//
// WHAT IS PROVED, and why it is not vacuous.
//
// The DUT is `zhao_surface_blend`, the EXACT module `zhao_surface_stamp`
// instantiates — there is one instance and this is it, so nothing here is a
// copy of the datapath. The free inputs are mode (3 bits), dst (8), src (8)
// and age_shift (3): 22 bits, which is TOTAL rather than sampled, because
// those ARE the port widths. Layer F's strength is one byte (charter 12,
// spec/terrain_rules.md 2 layer table), the source byte is one byte (it is
// `strength >> 8` of the ABI's u16), the blend vocabulary is seven codes in a
// 3-bit field and the shift is 3 bits. Free (mode, dst, src, age_shift) ranges
// over every input the block can ever be handed — all 4,194,304 of them.
// There is no reachability gap for the solver to hide in, and depth 2 is the
// whole state space rather than a bound: the module is one `always_comb`.
//
// The assertions are written against the ARITHMETIC — wide lanes, min/max,
// the reference's own expressions — never against a restatement of the RTL's
// case arms.
//
//   P1  a_replace     REPLACE is exactly the source. Trivial, and asserted
//                     because a datapath that quietly kept the destination in
//                     "replace" would leave every healing/erase stamp inert
//                     and nothing else here would notice.
//
//   P2  a_max_exact   MAX (and the ABI's operation-0 STAMP, which shares the
//                     arithmetic) is exactly max(dst, src). Two-sided, so it
//                     catches a swapped comparison as well as a wrong branch.
//                     This is the blend the software console runs for every
//                     stamp whose operation byte is not 1 — the one the
//                     committed render goldens pin.
//
//   P3  a_decay_exact DECAY_ACC is exactly min(255, (dst >> 1) + src) — ABI
//                     operation 1, `stamp_surface`'s own expression. The
//                     equality is two-sided: it catches a blend that WRAPS
//                     (a scar at 255 becoming a clean patch of ground on the
//                     next strike) and one that clamps too early.
//
//   P4  a_add_exact   ADD is exactly min(255, dst + src) — ops.yml
//                     FIELD.STAMP.ADD, with the saturation at 8 bits because
//                     that is the width layer F has.
//
//   P5  a_sub_exact   SUB is exactly max(0, dst - src) — ops.yml
//                     FIELD.STAMP.SUB. Two-sided, so it catches the borrow
//                     wrapping to 255, which would turn an erase into the
//                     brightest scar on the sheet.
//
//   P6  a_age_exact   AGE is exactly dst >> age_shift.
//
//   P7  a_sub_darkens SUB and AGE can never BRIGHTEN: the result is always
//                     <= dst. Erasure only ever erases. This is a real
//                     theorem and not a restatement — it is FALSE for the
//                     obvious near-miss (a subtract that wraps), which is the
//                     mutation the differential lanes also catch.
//
//   P8  a_add_brightens ADD, MAX and REPLACE-with-src>=dst can never DARKEN:
//                     additive residue only ever accumulates. Stated for ADD
//                     and MAX, which are the two unconditional cases.
//                     DECAY_ACC is DELIBERATELY EXCLUDED and that exclusion is
//                     the interesting part: `(dst >> 1) + src` with src = 0
//                     HALVES the destination, so decay-accumulate is NOT
//                     monotone, and asserting that it were would be asserting
//                     something false about the ratified blend.
//
//   P9  a_age_terminates AGE with a non-zero shift strictly decreases any
//                     non-zero destination. This is the property that
//                     justifies choosing a shift over a unit8 multiply: the
//                     rejected `(dst*rate + 128) >> 8` form with rate 255/256
//                     has a FIXED POINT at 1, so an aged scar would never
//                     reach zero. A shift always terminates, and here is the
//                     proof.
//
// NOT ASSERTED, and why: "the result never leaves the 8-bit field". `out_o` IS
// eight bits wide, so that statement is a tautology about the port
// declaration rather than a theorem about the arithmetic — unlike
// zhao_raster_blend, whose internal lane is wider than its output. Saying it
// here would inflate the property count without adding a bit of assurance.
//
// The cover task is load-bearing. Every assertion above is unconditional or
// guarded by a mode equality that is trivially reachable, so none can go
// vacuous through an unreachable antecedent — but the covers pin the corners
// anyway: BOTH saturation rails ACTUALLY FIRING (without them P3/P4/P5 also
// hold for a blend that never reaches a rail), MAX taking each of its two
// branches, DECAY_ACC actually halving (the non-monotone case P8 excludes),
// and AGE actually driving a non-zero destination to zero.
//
// WHAT THIS DOES NOT PROVE, stated plainly: the coverage geometry (the texel
// centres, the annulus, the truncating divide), the ABI operation -> blend
// mapping, the residency handshake, the pipeline, the counters and the
// `stamp_results` stream are NOT proved here. They are covered by the
// differential lanes against `zref::render::stamp_surface` and by the mutation
// evidence in design/contracts/SURFACE.STAMP.md. What IS proved is the
// arithmetic every stamped texel in the machine flows through.

`default_nettype none

module surface_blend_fv (
    input wire clk,
    input wire [2:0] mode,
    input wire [7:0] dst,
    input wire [7:0] src,
    input wire [2:0] age_shift
);

  localparam logic [2:0] BlStamp = 3'd0;
  localparam logic [2:0] BlDecayAcc = 3'd1;
  localparam logic [2:0] BlMax = 3'd2;
  localparam logic [2:0] BlAdd = 3'd3;
  localparam logic [2:0] BlSub = 3'd4;
  localparam logic [2:0] BlReplace = 3'd5;
  localparam logic [2:0] BlAge = 3'd6;

  wire [7:0] out;

  zhao_surface_blend dut (
      .mode_i(mode),
      .dst_i(dst),
      .src_i(src),
      .age_shift_i(age_shift),
      .out_o(out)
  );

  // Independent wide-lane arithmetic — the spec's expressions, not the RTL's.
  wire [31:0] w_dst = {24'd0, dst};
  wire [31:0] w_src = {24'd0, src};
  wire [31:0] w_add = w_dst + w_src;
  wire [31:0] w_decay = (w_dst >> 1) + w_src;
  wire signed [31:0] w_sub = $signed(w_dst) - $signed(w_src);
  wire [ 7:0] e_add = (w_add > 32'd255) ? 8'd255 : w_add[7:0];
  wire [ 7:0] e_decay = (w_decay > 32'd255) ? 8'd255 : w_decay[7:0];
  wire [ 7:0] e_sub = (w_sub < 32'sd0) ? 8'd0 : w_sub[7:0];
  wire [ 7:0] e_max = (w_src > w_dst) ? src : dst;
  wire [ 7:0] e_age = dst >> age_shift;

  always_ff @(posedge clk) begin
    a_replace : assert (mode != BlReplace || out == src);
    a_max_exact : assert ((mode != BlMax && mode != BlStamp) || out == e_max);
    a_decay_exact : assert (mode != BlDecayAcc || out == e_decay);
    a_add_exact : assert (mode != BlAdd || out == e_add);
    a_sub_exact : assert (mode != BlSub || out == e_sub);
    a_age_exact : assert (mode != BlAge || out == e_age);

    // P7 — erasure only ever erases.
    a_sub_darkens : assert ((mode != BlSub && mode != BlAge) || out <= dst);

    // P8 — additive residue only ever accumulates. DECAY_ACC is deliberately
    // NOT in this list: `(dst >> 1) + src` halves when src is 0.
    a_add_brightens : assert ((mode != BlAdd && mode != BlMax && mode != BlStamp) || out >= dst);

    // P9 — a shift decay always terminates.
    a_age_terminates :
    assert (mode != BlAge || age_shift == 3'd0 || dst == 8'd0 || out < dst);

    // ---- covers: the rails and the branches must be REACHABLE ------------
    c_add_rail : cover (mode == BlAdd && out == 8'd255 && dst != 8'd255);
    c_sub_rail : cover (mode == BlSub && out == 8'd0 && dst != 8'd0);
    c_decay_rail : cover (mode == BlDecayAcc && out == 8'd255 && dst != 8'd255);
    c_decay_halves : cover (mode == BlDecayAcc && dst > 8'd1 && out < dst);
    c_max_takes_src : cover (mode == BlMax && out == src && src > dst);
    c_max_takes_dst : cover (mode == BlMax && out == dst && dst > src);
    c_stamp_takes_src : cover (mode == BlStamp && out == src && src > dst);
    c_age_to_zero : cover (mode == BlAge && dst != 8'd0 && out == 8'd0);
    c_replace_lowers : cover (mode == BlReplace && out < dst);
  end

endmodule

`default_nettype wire
