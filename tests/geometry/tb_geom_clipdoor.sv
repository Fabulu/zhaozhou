// tb_geom_clipdoor.sv -- the flattening bench for `zhao_geom_clipdoor`.
//
// The DUT's client ports are flattened `NCLIENT*W` vectors because Quartus 17.0
// will not take an unpacked array port. Verilator's C++ surface for a 448-bit
// vector is an array of words, which is awkward to drive from a bench and easy
// to get subtly wrong -- so this wrapper is the one place they are unrolled and
// every signal the test touches is a flat scalar.
//
// It holds NO logic and NO state of its own beyond ONE deliberate replication:
// each client's 224-bit ruling-5 attribute packet is driven from a 32-bit
// WITNESS, repeated across all seven slots, and the door's 224-bit output is
// read back as its lowest and highest slot. That is enough to prove the mux
// selected the right CLIENT's attributes without giving the bench 448 bits to
// steer, and it is a rename, not a computation: the door never reads inside the
// packet, so no slot is more informative than any other and the two that are
// exposed bracket the whole width.
//
// N is fixed at 2 here -- GEOM.REPLAY's mesh triangles and PART.EXPAND's
// polygon particles, the two producers the console actually has. ATTRS is the
// console's own `GEOM_CLIP_ATTRS`.
`default_nettype none

module tb_geom_clipdoor #(
    parameter int unsigned ATTRS = 7
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- client 0 -----------------------------------------------------------
    input  var logic               c0_valid_i,
    output var logic               c0_ready_o,
    input  var logic signed [20:0] c0_ax_i,
    input  var logic signed [20:0] c0_ay_i,
    input  var logic signed [20:0] c0_bx_i,
    input  var logic signed [20:0] c0_by_i,
    input  var logic signed [20:0] c0_cx_i,
    input  var logic signed [20:0] c0_cy_i,
    input  var logic        [ 2:0] c0_behind_i,
    input  var logic        [15:0] c0_src_id_i,
    input  var logic               c0_untex_i,
    input  var logic        [ 1:0] c0_cull_mode_i,
    input  var logic        [31:0] c0_attr_witness_i,
    input  var logic        [31:0] c0_material_set_i,
    input  var logic        [15:0] c0_material_id_i,
    // The producer's MATERIAL-MODE declaration (owner ruling 1, 2026-09-22).
    input  var logic        [ 1:0] c0_material_mode_i,
    input  var logic        [ 7:0] c0_vertex_alpha_i,
    input  var logic        [31:0] c0_frag_state_i,
    input  var logic        [ 7:0] c0_quality_tier_i,

    // ---- client 1 -----------------------------------------------------------
    input  var logic               c1_valid_i,
    output var logic               c1_ready_o,
    input  var logic signed [20:0] c1_ax_i,
    input  var logic signed [20:0] c1_ay_i,
    input  var logic signed [20:0] c1_bx_i,
    input  var logic signed [20:0] c1_by_i,
    input  var logic signed [20:0] c1_cx_i,
    input  var logic signed [20:0] c1_cy_i,
    input  var logic        [ 2:0] c1_behind_i,
    input  var logic        [15:0] c1_src_id_i,
    input  var logic               c1_untex_i,
    input  var logic        [ 1:0] c1_cull_mode_i,
    input  var logic        [31:0] c1_attr_witness_i,
    input  var logic        [31:0] c1_material_set_i,
    input  var logic        [15:0] c1_material_id_i,
    input  var logic        [ 1:0] c1_material_mode_i,
    input  var logic        [ 7:0] c1_vertex_alpha_i,
    input  var logic        [31:0] c1_frag_state_i,
    input  var logic        [ 7:0] c1_quality_tier_i,

    // ---- the door -----------------------------------------------------------
    output var logic               o_valid_o,
    input  var logic               o_ready_i,
    output var logic signed [20:0] o_ax_o,
    output var logic signed [20:0] o_ay_o,
    output var logic signed [20:0] o_bx_o,
    output var logic signed [20:0] o_by_o,
    output var logic signed [20:0] o_cx_o,
    output var logic signed [20:0] o_cy_o,
    output var logic        [ 2:0] o_behind_o,
    output var logic        [15:0] o_src_id_o,
    output var logic               o_untex_o,
    output var logic        [ 1:0] o_cull_mode_o,
    // The first and last 32-bit slot of each of the three corner packets.
    output var logic        [31:0] o_attr_a_lo_o,
    output var logic        [31:0] o_attr_a_hi_o,
    output var logic        [31:0] o_attr_b_lo_o,
    output var logic        [31:0] o_attr_c_lo_o,
    output var logic        [31:0] o_material_set_o,
    output var logic        [15:0] o_material_id_o,
    output var logic        [ 1:0] o_material_mode_o,
    output var logic        [ 7:0] o_vertex_alpha_o,
    output var logic        [31:0] o_frag_state_o,
    output var logic        [ 7:0] o_quality_tier_o,
    output var logic        [ 1:0] o_owner_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] granted0_o,
    output var logic [31:0] granted1_o,
    output var logic [31:0] switches_o,
    output var logic [31:0] idle_offered_o,
    output var logic [31:0] err_hold_broken_o
);

  localparam int unsigned AW = ATTRS * 32;

  logic [1:0]        c_valid_c, c_ready_c, c_untex_c;
  logic [2*21-1:0]   c_ax_c, c_ay_c, c_bx_c, c_by_c, c_cx_c, c_cy_c;
  logic [2*3-1:0]    c_behind_c;
  logic [2*16-1:0]   c_src_id_c;
  logic [2*2-1:0]    c_cull_c;
  logic [2*AW-1:0]   c_attr_a_c, c_attr_b_c, c_attr_c_c;
  logic [2*32-1:0]   c_mset_c;
  logic [2*16-1:0]   c_mid_c;
  logic [2*2-1:0]    c_mmode_c;
  logic [2*8-1:0]    c_valpha_c;
  logic [2*32-1:0]   c_fstate_c;
  logic [2*8-1:0]    c_tier_c;
  logic [2*32-1:0]   granted_c;
  // The middle slots are DELIBERATELY unread: the witness is replicated across
  // every slot, so slot 0 and slot ATTRS-1 bracket the width and the five
  // between them carry no information the two ends do not. Exposing all seven
  // would give the bench 224 bits to compare and nothing more to learn.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [AW-1:0]     o_attr_a_c, o_attr_b_c, o_attr_c_c;
  /* verilator lint_on UNUSEDSIGNAL */

  // The witness, repeated across every slot of the packet. `{ATTRS{w}}` is a
  // replication of a 32-bit value, so slot k of the packet holds the witness
  // for every k -- which is what lets the bench read slot 0 and slot ATTRS-1
  // and know it has seen the whole width.
  logic [AW-1:0] c0_pack_c, c1_pack_c;
  always_comb begin
    c0_pack_c = {ATTRS{c0_attr_witness_i}};
    // B and C get the witness with its low byte complemented, so a mux that
    // forwarded corner A's packet to all three corners fails rather than
    // passing by coincidence.
    c1_pack_c = {ATTRS{c1_attr_witness_i}};

    c_valid_c  = {c1_valid_i, c0_valid_i};
    c_untex_c  = {c1_untex_i, c0_untex_i};
    c_ax_c     = {c1_ax_i, c0_ax_i};
    c_ay_c     = {c1_ay_i, c0_ay_i};
    c_bx_c     = {c1_bx_i, c0_bx_i};
    c_by_c     = {c1_by_i, c0_by_i};
    c_cx_c     = {c1_cx_i, c0_cx_i};
    c_cy_c     = {c1_cy_i, c0_cy_i};
    c_behind_c = {c1_behind_i, c0_behind_i};
    c_src_id_c = {c1_src_id_i, c0_src_id_i};
    c_cull_c   = {c1_cull_mode_i, c0_cull_mode_i};
    c_mset_c   = {c1_material_set_i, c0_material_set_i};
    c_mid_c    = {c1_material_id_i, c0_material_id_i};
    c_mmode_c  = {c1_material_mode_i, c0_material_mode_i};
    c_valpha_c = {c1_vertex_alpha_i, c0_vertex_alpha_i};
    c_fstate_c = {c1_frag_state_i, c0_frag_state_i};
    c_tier_c   = {c1_quality_tier_i, c0_quality_tier_i};

    c_attr_a_c = {c1_pack_c, c0_pack_c};
    c_attr_b_c = {{ATTRS{c1_attr_witness_i ^ 32'h0000_00FF}},
                  {ATTRS{c0_attr_witness_i ^ 32'h0000_00FF}}};
    c_attr_c_c = {{ATTRS{c1_attr_witness_i ^ 32'h0000_FF00}},
                  {ATTRS{c0_attr_witness_i ^ 32'h0000_FF00}}};
  end

  assign c0_ready_o    = c_ready_c[0];
  assign c1_ready_o    = c_ready_c[1];
  assign granted0_o    = granted_c[31:0];
  assign granted1_o    = granted_c[63:32];
  assign o_attr_a_lo_o = o_attr_a_c[31:0];
  assign o_attr_a_hi_o = o_attr_a_c[AW-1 -: 32];
  assign o_attr_b_lo_o = o_attr_b_c[31:0];
  assign o_attr_c_lo_o = o_attr_c_c[31:0];

  zhao_geom_clipdoor #(
      .NCLIENT (2),
      .ATTRS   (ATTRS),
      .IDW     (16)
  ) dut (
      .clk   (clk),
      .rst_n (rst_n),

      .c_valid_i        (c_valid_c),
      .c_ready_o        (c_ready_c),
      .c_ax_i           (c_ax_c),
      .c_ay_i           (c_ay_c),
      .c_bx_i           (c_bx_c),
      .c_by_i           (c_by_c),
      .c_cx_i           (c_cx_c),
      .c_cy_i           (c_cy_c),
      .c_behind_i       (c_behind_c),
      .c_src_id_i       (c_src_id_c),
      .c_untex_i        (c_untex_c),
      .c_cull_mode_i    (c_cull_c),
      .c_attr_a_i       (c_attr_a_c),
      .c_attr_b_i       (c_attr_b_c),
      .c_attr_c_i       (c_attr_c_c),
      // ARENAID 2026-09-25: the per-corner identity and the per-primitive
      // rider GEOM.VERTID reads. This bench does not exercise the identity
      // space, so they are tied and the outputs left open -- DECLARED here
      // rather than omitted, because a missing pin is a PINMISSING the next
      // fit finds and an explicit tie is a statement about this bench.
      .c_key_a_i        ('0),
      .c_key_b_i        ('0),
      .c_key_c_i        ('0),
      .c_rider_i        ('0),
      .c_material_set_i (c_mset_c),
      .c_material_id_i  (c_mid_c),
      .c_material_mode_i(c_mmode_c),
      .c_vertex_alpha_i (c_valpha_c),
      .c_frag_state_i   (c_fstate_c),
      .c_quality_tier_i (c_tier_c),

      .o_valid_o        (o_valid_o),
      .o_ready_i        (o_ready_i),
      .o_ax_o           (o_ax_o),
      .o_ay_o           (o_ay_o),
      .o_bx_o           (o_bx_o),
      .o_by_o           (o_by_o),
      .o_cx_o           (o_cx_o),
      .o_cy_o           (o_cy_o),
      .o_behind_o       (o_behind_o),
      .o_src_id_o       (o_src_id_o),
      .o_untex_o        (o_untex_o),
      .o_cull_mode_o    (o_cull_mode_o),
      .o_attr_a_o       (o_attr_a_c),
      .o_attr_b_o       (o_attr_b_c),
      .o_attr_c_o       (o_attr_c_c),
      /* verilator lint_off PINCONNECTEMPTY */   // ARENAID: no consumer here
      .o_key_a_o        (),
      .o_key_b_o        (),
      .o_key_c_o        (),
      .o_rider_o        (),
      /* verilator lint_on PINCONNECTEMPTY */
      .o_material_set_o (o_material_set_o),
      .o_material_id_o  (o_material_id_o),
      .o_material_mode_o(o_material_mode_o),
      .o_vertex_alpha_o (o_vertex_alpha_o),
      .o_frag_state_o   (o_frag_state_o),
      .o_quality_tier_o (o_quality_tier_o),
      .o_owner_o        (o_owner_o),

      .granted_o          (granted_c),
      .switches_o         (switches_o),
      .idle_offered_o     (idle_offered_o),
      .err_hold_broken_o  (err_hold_broken_o)
  );

endmodule

`default_nettype wire
