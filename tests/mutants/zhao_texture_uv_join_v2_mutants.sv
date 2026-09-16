// zhao_texture_uv_join_v2_mutants.sv -- DELIBERATELY BROKEN test-only controls.
//
// These renamed modules can never replace the production join through a source-
// list accident.  Each wraps the real transport and changes exactly the sole
// page-generation slice in the outward view.  The ordinary driver asserts the
// correct production hold; the inverse-polarity controls require these copies to
// violate it while owner, all other logical bits, U, and V remain those of A.
//
// Mutant 1, generation-slice swap:
//   held A joined record takes logical287[286:279] from the currently offered B
//   descriptor while retaining A's owner/U/V and all other logical bits.
//
// Mutant 2, late-global overwrite:
//   held A joined record takes that same slice from the current active binding-
//   page generation.  Production RTL has no such input and cannot perform this
//   late read.
//
// Mutant 3, no same-edge reload:
//   a retiring output does not reserve its register for the matching pair held
//   behind it.  Correct payloads still emerge in order, but an all-ready stream
//   acquires an observable empty cycle between every two outputs.
`default_nettype none
/* verilator lint_off DECLFILENAME */

module zhao_texture_uv_join_v2_generation_swap_mutant (
    input var logic clk,
    input var logic rst_n,

    input  var logic         desc_valid_i,
    output var logic         desc_ready_o,
    input  var logic [300:0] desc_data_i,
    // TIMING4 E1: carried so this control keeps the production shape.
    input  var logic         desc_usable_i,

    input  var logic        uv_valid_i,
    output var logic        uv_ready_o,
    input  var logic [77:0] uv_data_i,

    output var logic         out_valid_o,
    input  var logic         out_ready_i,
    output var logic [364:0] out_data_o,

    // Present only so both test mutants have one bench-facing shape.  This
    // mutant deliberately ignores the global generation.
    input  var logic [7:0]  active_page_generation_i,

    output var logic [31:0] uvjoin_owner_mismatch_o,
    output var logic        lifetime_fault_o,
    output var logic        idle_o
);

  localparam int unsigned DESC_PAGE_GEN_LO = 279;
  localparam int unsigned OUT_PAGE_GEN_LO  = 343;

  logic [364:0] held_data_w;

  zhao_texture_uv_join_v2 u_reference_transport (
      .clk                         (clk),
      .rst_n                       (rst_n),
      .desc_valid_i                (desc_valid_i),
      .desc_ready_o                (desc_ready_o),
      .desc_data_i                 (desc_data_i),
      .desc_usable_i               (desc_usable_i),
      .uv_valid_i                  (uv_valid_i),
      .uv_ready_o                  (uv_ready_o),
      .uv_data_i                   (uv_data_i),
      .out_valid_o                 (out_valid_o),
      .out_ready_i                 (out_ready_i),
      .out_data_o                  (held_data_w),
      .uvjoin_owner_mismatch_o     (uvjoin_owner_mismatch_o),
      .lifetime_fault_o               (lifetime_fault_o),
      .idle_o                      (idle_o)
  );

  // MUTANT: a held A packet borrows B's live generation slice.
  always_comb begin
    out_data_o = held_data_w;
    out_data_o[OUT_PAGE_GEN_LO +: 8] =
        desc_data_i[DESC_PAGE_GEN_LO +: 8];
  end

  wire unused_active_generation = &{1'b0, active_page_generation_i};

endmodule : zhao_texture_uv_join_v2_generation_swap_mutant


module zhao_texture_uv_join_v2_late_overwrite_mutant (
    input var logic clk,
    input var logic rst_n,

    input  var logic         desc_valid_i,
    output var logic         desc_ready_o,
    input  var logic [300:0] desc_data_i,
    // TIMING4 E1: carried so this control keeps the production shape.
    input  var logic         desc_usable_i,

    input  var logic        uv_valid_i,
    output var logic        uv_ready_o,
    input  var logic [77:0] uv_data_i,

    output var logic         out_valid_o,
    input  var logic         out_ready_i,
    output var logic [364:0] out_data_o,

    input  var logic [7:0]  active_page_generation_i,

    output var logic [31:0] uvjoin_owner_mismatch_o,
    output var logic        lifetime_fault_o,
    output var logic        idle_o
);

  localparam int unsigned OUT_PAGE_GEN_LO = 343;

  logic [364:0] held_data_w;

  zhao_texture_uv_join_v2 u_reference_transport (
      .clk                         (clk),
      .rst_n                       (rst_n),
      .desc_valid_i                (desc_valid_i),
      .desc_ready_o                (desc_ready_o),
      .desc_data_i                 (desc_data_i),
      .desc_usable_i               (desc_usable_i),
      .uv_valid_i                  (uv_valid_i),
      .uv_ready_o                  (uv_ready_o),
      .uv_data_i                   (uv_data_i),
      .out_valid_o                 (out_valid_o),
      .out_ready_i                 (out_ready_i),
      .out_data_o                  (held_data_w),
      .uvjoin_owner_mismatch_o     (uvjoin_owner_mismatch_o),
      .lifetime_fault_o               (lifetime_fault_o),
      .idle_o                      (idle_o)
  );

  // MUTANT: current configuration overwrites admission-sealed carriage.
  always_comb begin
    out_data_o = held_data_w;
    out_data_o[OUT_PAGE_GEN_LO +: 8] = active_page_generation_i;
  end

  wire unused_desc_offer = &{1'b0, desc_data_i};

endmodule : zhao_texture_uv_join_v2_late_overwrite_mutant


module zhao_texture_uv_join_v2_no_same_edge_reload_mutant (
    input var logic clk,
    input var logic rst_n,

    input  var logic         desc_valid_i,
    output var logic         desc_ready_o,
    input  var logic [300:0] desc_data_i,
    // TIMING4 E1: carried so this control keeps the production shape.
    input  var logic         desc_usable_i,

    input  var logic        uv_valid_i,
    output var logic        uv_ready_o,
    input  var logic [77:0] uv_data_i,

    output var logic         out_valid_o,
    input  var logic         out_ready_i,
    output var logic [364:0] out_data_o,

    output var logic [31:0] uvjoin_owner_mismatch_o,
    output var logic        lifetime_fault_o,
    output var logic        idle_o
);

  localparam int unsigned OWNER_W       = 14;
  localparam int unsigned LOGICAL_W     = 287;
  localparam int unsigned UV_COMPONENT_W= 32;
  localparam int unsigned DESC_OWNER_LO = LOGICAL_W;
  localparam int unsigned UV_OWNER_LO   = 2*UV_COMPONENT_W;
  localparam int unsigned UV_U_LO       = UV_COMPONENT_W;
  localparam int unsigned UV_V_LO       = 0;

  logic         desc_v_q;
  logic [300:0] desc_q;
  logic         desc_usable_q;
  logic         uv_v_q;
  logic [77:0]  uv_q;
  logic         out_v_q;
  logic [364:0] out_q;

  wire [OWNER_W-1:0] desc_owner_c =
      desc_q[DESC_OWNER_LO +: OWNER_W];
  wire [OWNER_W-1:0] uv_owner_c =
      uv_q[UV_OWNER_LO +: OWNER_W];

  wire pair_present_c = desc_v_q && uv_v_q;
  wire owners_match_c = (desc_owner_c == uv_owner_c);
  wire out_pop_c       = out_v_q && out_ready_i;

  // MUTANT: production uses `!out_v_q || out_ready_i`, allowing a retiring
  // output to reserve the register for the next pair on the same edge.  This
  // copy requires the output to have been empty at the START of the cycle.
  wire fault_free_c = !lifetime_fault_o;
  wire join_c = fault_free_c && pair_present_c && owners_match_c && !out_v_q;
  wire mismatch_c = fault_free_c && pair_present_c && !owners_match_c;

  assign desc_ready_o = fault_free_c && (!desc_v_q || join_c);
  assign uv_ready_o   = fault_free_c && (!uv_v_q   || join_c);
  assign out_valid_o  = out_v_q;
  assign out_data_o   = out_q;
  assign idle_o       = !desc_v_q && !uv_v_q && !out_v_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      desc_v_q                <= 1'b0;
      uv_v_q                  <= 1'b0;
      out_v_q                 <= 1'b0;
      uvjoin_owner_mismatch_o <= 32'd0;
      lifetime_fault_o           <= 1'b0;
    end else begin
      if (mismatch_c) begin
        desc_v_q <= 1'b0;
        uv_v_q   <= 1'b0;
      end else begin
        if (desc_ready_o) begin
          desc_v_q <= desc_valid_i;
          if (desc_valid_i) begin
            desc_q        <= desc_data_i;
            desc_usable_q <= desc_usable_i;
          end
        end
        if (uv_ready_o) begin
          uv_v_q <= uv_valid_i;
          if (uv_valid_i) uv_q <= uv_data_i;
        end
      end

      if (out_pop_c) out_v_q <= 1'b0;
      if (join_c) begin
        out_v_q <= 1'b1;
        out_q <= {
          desc_owner_c,
          desc_usable_q ? desc_q[LOGICAL_W-1:0] : {LOGICAL_W{1'b0}},
          uv_q[UV_U_LO +: UV_COMPONENT_W],
          uv_q[UV_V_LO +: UV_COMPONENT_W]
        };
      end

      if (mismatch_c) begin
        uvjoin_owner_mismatch_o <= uvjoin_owner_mismatch_o + 32'd1;
        lifetime_fault_o        <= 1'b1;
      end
    end
  end

endmodule : zhao_texture_uv_join_v2_no_same_edge_reload_mutant

/* verilator lint_on DECLFILENAME */
`default_nettype wire
