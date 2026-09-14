// zhao_texture_mosaic_v2.sv — Packet-B observation-preserving Mosaic lane.
//
// This is the versioned successor of zhao_texture_mosaic.  The frozen terrain
// laws, two-stage latency, ready path and retirement counter are unchanged.
// The separate name lets the Packet-B island observe this lane without changing
// the unversioned executable oracle underneath zhao_texture_island_top.
//
// Frozen laws (spec/terrain_rules.md 6.2):
//   m     = floor(q16.16 * 64) = raw >>> 10
//   texel = m[6] ? 127-m[6:0] : m[5:0]
//   h     = u32(m_u)*73856093 XOR u32(m_v)*19349663, products wrap at 32 bits
//   p     = h mod 255
//   pick  = p < weight ? mat_a : mat_b
//
// req_mosaic_i disables only the material pick.  Wall and underside spans still
// use the mirrored fold.  idle_o covers both valid stages; stale payload bits in
// empty registers are deliberately not work.
`default_nettype none

module zhao_texture_mosaic_v2 (
    input  var logic               clk,
    input  var logic               rst_n,

    input  var logic               req_valid_i,
    output var logic               req_ready_o,
    input  var logic signed [31:0] req_u_i,
    input  var logic signed [31:0] req_v_i,
    input  var logic        [ 7:0] req_mat_a_i,
    input  var logic        [ 7:0] req_mat_b_i,
    input  var logic        [ 7:0] req_weight_i,
    input  var logic               req_mosaic_i,
    input  var logic        [15:0] req_src_id_i,

    output var logic               pick_valid_o,
    input  var logic               pick_ready_i,
    output var logic        [ 7:0] pick_tile_o,
    output var logic        [ 5:0] pick_tx_o,
    output var logic        [ 5:0] pick_ty_o,
    output var logic        [15:0] pick_src_id_o,

    output var logic               idle_o,
    output var logic        [31:0] texture_samples_o
);

  localparam logic [31:0] MOSAIC_CX = 32'd73856093;
  localparam logic [31:0] MOSAIC_CY = 32'd19349663;
  localparam logic [31:0] CNT_MAX   = 32'hFFFF_FFFF;

  // The constant products are explicit CSD shift/add trees.  A generic `*`
  // infers DSPs on Quartus 17 even with a logic-style attribute.
  function automatic logic [31:0] mul_cx(input logic [31:0] m);
    mul_cx = (m << 0) + (m << 7) + (m << 10) + (m << 19) + (m << 23) + (m << 26)
           - (m << 2) - (m << 5) - (m << 12) - (m << 16) - (m << 21);
  endfunction

  function automatic logic [31:0] mul_cy(input logic [31:0] m);
    mul_cy = (m << 5) + (m << 7) + (m << 14) + (m << 19) + (m << 21) + (m << 24)
           - (m << 0) - (m << 16);
  endfunction

  // The named constants remain independently visible controls.  Because each
  // tree is linear over the 32-bit ring, checking operand one proves every word.
  initial begin : p_constant_contract
    if (mul_cx(32'd1) !== MOSAIC_CX)
      $fatal(1, "mosaic-v2 CX shift/add tree does not implement MOSAIC_CX");
    if (mul_cy(32'd1) !== MOSAIC_CY)
      $fatal(1, "mosaic-v2 CY shift/add tree does not implement MOSAIC_CY");
  end

  logic signed [31:0] m_u_c;
  logic signed [31:0] m_v_c;
  logic        [ 6:0] per_u_c;
  logic        [ 6:0] per_v_c;
  logic        [ 5:0] tx_c;
  logic        [ 5:0] ty_c;
  logic        [31:0] hash_c;

  always_comb begin
    m_u_c = req_u_i >>> 10;
    m_v_c = req_v_i >>> 10;
    per_u_c = req_u_i[16:10];
    per_v_c = req_v_i[16:10];
    tx_c = per_u_c[5:0] ^ {6{per_u_c[6]}};
    ty_c = per_v_c[5:0] ^ {6{per_v_c[6]}};
    hash_c = mul_cx($unsigned(m_u_c)) ^ mul_cy($unsigned(m_v_c));
  end

  logic        a_valid_q;
  logic [31:0] a_hash_q;
  logic [ 7:0] a_mat_a_q;
  logic [ 7:0] a_mat_b_q;
  logic [ 7:0] a_weight_q;
  logic        a_mosaic_q;
  logic [ 5:0] a_tx_q;
  logic [ 5:0] a_ty_q;
  logic [15:0] a_src_q;

  logic        b_valid_q;
  logic [ 7:0] b_tile_q;
  logic [ 5:0] b_tx_q;
  logic [ 5:0] b_ty_q;
  logic [15:0] b_src_q;

  logic [7:0] residue_c;
  logic       advance_c;
  logic       pick_a_c;
  logic [7:0] tile_c;

  zhao_texture_mod255 u_mod255 (
      .h_i(a_hash_q),
      .p_o(residue_c)
  );

  always_comb begin
    advance_c = !b_valid_q || pick_ready_i;
    pick_a_c  = (residue_c < a_weight_q);
    tile_c    = (!a_mosaic_q || pick_a_c) ? a_mat_a_q : a_mat_b_q;

    req_ready_o   = advance_c;
    pick_valid_o  = b_valid_q;
    pick_tile_o   = b_tile_q;
    pick_tx_o     = b_tx_q;
    pick_ty_o     = b_ty_q;
    pick_src_id_o = b_src_q;
    idle_o        = !a_valid_q && !b_valid_q;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      a_valid_q          <= 1'b0;
      a_hash_q           <= 32'd0;
      a_mat_a_q          <= 8'd0;
      a_mat_b_q          <= 8'd0;
      a_weight_q         <= 8'd0;
      a_mosaic_q         <= 1'b0;
      a_tx_q             <= 6'd0;
      a_ty_q             <= 6'd0;
      a_src_q            <= 16'd0;
      b_valid_q          <= 1'b0;
      b_tile_q           <= 8'd0;
      b_tx_q             <= 6'd0;
      b_ty_q             <= 6'd0;
      b_src_q            <= 16'd0;
      texture_samples_o  <= 32'd0;
    end else begin
      if (pick_valid_o && pick_ready_i && (texture_samples_o != CNT_MAX))
        texture_samples_o <= texture_samples_o + 32'd1;

      if (advance_c) begin
        a_valid_q  <= req_valid_i;
        if (req_valid_i) begin
          a_hash_q    <= hash_c;
          a_mat_a_q   <= req_mat_a_i;
          a_mat_b_q   <= req_mat_b_i;
          a_weight_q  <= req_weight_i;
          a_mosaic_q  <= req_mosaic_i;
          a_tx_q      <= tx_c;
          a_ty_q      <= ty_c;
          a_src_q     <= req_src_id_i;
        end

        b_valid_q <= a_valid_q;
        if (a_valid_q) begin
          b_tile_q <= tile_c;
          b_tx_q   <= a_tx_q;
          b_ty_q   <= a_ty_q;
          b_src_q  <= a_src_q;
        end
      end
    end
  end

endmodule : zhao_texture_mosaic_v2

`default_nettype wire
