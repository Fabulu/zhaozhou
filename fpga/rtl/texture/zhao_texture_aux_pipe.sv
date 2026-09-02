// zhao_texture_aux_pipe.sv — AUX as a pipeline: II=1 accept, II=1 sheet request.
//
// BESIDE `zhao_texture_aux.sv`, which stays the block of record. Nothing
// instantiates this yet.
//
// ---------------------------------------------------------------------------
// WHY
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md:
//
//   > The current AUX FSM performs three restoring quotient bits in one state,
//   > three in another, issues one Surface Sheet request, waits for the answer,
//   > presents it, and only then accepts another fragment. Its nominal
//   > one-per-six rate is about 277,778/frame, almost exactly the 276,480
//   > estimate.
//
// "Almost exactly the estimate" is the problem. A block sized at 1.005x its
// own demand has no reserve for a cache miss, a queue bubble or a frame that
// is slightly busier than the model.
//
//   A0  accept, numerator/divisor, classify neg/sat/degenerate
//   A1..A6  the six restoring bits -- zhao_texture_aux_div6, ALREADY VERIFIED
//   A7  form texel index, enqueue Surface Sheet read
//   A8  capture registered sheet response
//   A9  enqueue tokenized AUX return
//
// ---------------------------------------------------------------------------
// TWO THINGS TRANSCRIBED RATHER THAN REINVENTED
// ---------------------------------------------------------------------------
// A0's arithmetic is copied from zhao_texture_aux.sv character for character:
//
//     degen = (env_x1 <= env_x0) || (env_z1 <= env_z0)
//     du    = env_x1 - env_x0                     (unsigned)
//     nu    = (NUM_W'(signed(wx)) - NUM_W'(signed(env_x0))) <<< 6
//     neg   = nu[NUM_W-1]
//     sat   = !neg && unsigned(nu) >= {2'b00, du, 6'b0}      i.e. nu >= 64*du
//     r_in  = (neg || sat) ? 0 : nu
//     tex   = sat ? 63 : quotient
//
// The two clamps stay OUTSIDE the divider, which is what lets the divide be
// exactly six steps -- the same contract zhao_texture_aux_div6 documents and
// its test enforces.
//
// AND THE DEGENERATE CASE STILL TRAVELS. The brief is explicit: "A degenerate
// envelope travels through the ordering machinery but emits no sheet read."
// Dropping it at A0 would be simpler and would silently reorder AUX returns
// relative to the fragments that produced them.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_aux_pipe #(
    parameter int unsigned NUM_W = 40,
    parameter int unsigned DEN_W = 32,
    parameter int unsigned REM_W = 39,
    parameter int unsigned TOKW  = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- accept -------------------------------------------------------------
    input  var logic                     req_valid_i,
    output var logic                     req_ready_o,
    input  var logic signed [31:0]       req_wx_i,
    input  var logic signed [31:0]       req_wz_i,
    input  var logic signed [31:0]       req_env_x0_i,
    input  var logic signed [31:0]       req_env_x1_i,
    input  var logic signed [31:0]       req_env_z0_i,
    input  var logic signed [31:0]       req_env_z1_i,
    input  var logic [TOKW-1:0]          req_tok_i,

    // ---- Surface Sheet read -------------------------------------------------
    output var logic                     sheet_valid_o,
    input  var logic                     sheet_ready_i,
    output var logic [5:0]               sheet_u_o,
    output var logic [5:0]               sheet_v_o,
    output var logic [TOKW-1:0]          sheet_tok_o,
    input  var logic                     sheet_rvalid_i,
    input  var logic [7:0]               sheet_tag_i,
    input  var logic [7:0]               sheet_str_i,
    input  var logic [TOKW-1:0]          sheet_rtok_i,

    // ---- the tokenized AUX return -------------------------------------------
    output var logic                     out_valid_o,
    input  var logic                     out_ready_i,
    output var logic [TOKW-1:0]          out_tok_o,
    output var logic [7:0]               out_tag_o,
    output var logic [7:0]               out_str_o,
    output var logic                     out_degenerate_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]              accepted_o,
    output var logic [31:0]              sheet_reads_o,
    output var logic [31:0]              degenerate_o
);

  // ======================================================================= A0
  logic                     degen_c;
  logic [DEN_W-1:0]         du_c, dv_c;
  logic signed [NUM_W-1:0]  nu_c, nv_c;
  logic                     negu_c, negv_c, satu_c, satv_c;

  always_comb begin
    degen_c = (req_env_x1_i <= req_env_x0_i) || (req_env_z1_i <= req_env_z0_i);

    du_c = $unsigned(req_env_x1_i) - $unsigned(req_env_x0_i);
    dv_c = $unsigned(req_env_z1_i) - $unsigned(req_env_z0_i);

    nu_c = (NUM_W'($signed(req_wx_i)) - NUM_W'($signed(req_env_x0_i))) <<< 6;
    nv_c = (NUM_W'($signed(req_wz_i)) - NUM_W'($signed(req_env_z0_i))) <<< 6;

    negu_c = nu_c[NUM_W-1];
    negv_c = nv_c[NUM_W-1];
    satu_c = !negu_c && ($unsigned(nu_c) >= {2'b00, du_c, 6'b000000});
    satv_c = !negv_c && ($unsigned(nv_c) >= {2'b00, dv_c, 6'b000000});
  end

  // A side channel carries what the divider does not need. It is indexed by
  // the SAME token the divider echoes, so nothing has to be kept in step by
  // counting clocks -- that is the whole reason the divider carries a tag.
  localparam int unsigned SIDE_N = 16;
  logic            sd_degen [SIDE_N];
  logic            sd_satu  [SIDE_N];
  logic            sd_satv  [SIDE_N];
  logic [TOKW-1:0] sd_tok   [SIDE_N];

  logic [3:0] sd_wp;

  // Accept whenever the divider can take a job. The divider is a fixed-latency
  // pipeline with no stalls, so this never depends on the sheet or the output.
  assign req_ready_o = 1'b1;

  // ================================================================= A1..A6
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] nc_div_issued;
  logic [3:0]  nc_div_occ;
  /* verilator lint_on UNUSEDSIGNAL */

  logic            div_ov;
  logic [5:0]      div_qu, div_qv;
  logic [3:0]      div_tag;

  zhao_texture_aux_div6 #(
      .REM_W(REM_W),
      .DEN_W(DEN_W),
      .TAGW (4)
  ) u_div (
      .clk        (clk),
      .rst_n      (rst_n),
      .in_valid_i (req_valid_i),
      // The clamps are applied HERE, outside the divider, so the quotient is
      // in [0,63] by construction and six steps suffice.
      // ENFORCED-BY: tests/texture/texture_aux_pipe_directed.cpp
      .in_ru_i    ((negu_c || satu_c) ? {REM_W{1'b0}} : REM_W'($unsigned(nu_c))),
      .in_du_i    (du_c),
      .in_rv_i    ((negv_c || satv_c) ? {REM_W{1'b0}} : REM_W'($unsigned(nv_c))),
      .in_dv_i    (dv_c),
      .in_tag_i   (sd_wp),
      .out_valid_o(div_ov),
      .out_qu_o   (div_qu),
      .out_qv_o   (div_qv),
      .out_tag_o  (div_tag),
      .issued_o   (nc_div_issued),
      .occupancy_o(nc_div_occ)
  );

  // ======================================================================= A7
  // The texel index, and the sheet read. A degenerate envelope produces NO
  // read but still occupies a return slot.
  logic [5:0] tex_u_c, tex_v_c;
  assign tex_u_c = sd_satu[div_tag] ? 6'd63 : div_qu;
  assign tex_v_c = sd_satv[div_tag] ? 6'd63 : div_qv;

  logic            a7_v_q, a7_degen_q;
  logic [5:0]      a7_u_q, a7_v_coord_q;
  logic [TOKW-1:0] a7_tok_q;

  assign sheet_valid_o = a7_v_q && !a7_degen_q;
  assign sheet_u_o     = a7_u_q;
  assign sheet_v_o     = a7_v_coord_q;
  assign sheet_tok_o   = a7_tok_q;

  // ==================================================================== A8/A9
  // Degenerate requests bypass the sheet but NOT the ordering machinery: they
  // enter the return queue directly, in the same order.
  localparam int unsigned RQ_N = 8;
  logic [TOKW-1:0] rq_tok [RQ_N];
  logic [7:0]      rq_tag [RQ_N];
  logic [7:0]      rq_str [RQ_N];
  logic            rq_deg [RQ_N];
  logic [2:0]      rq_wp, rq_rp;
  logic [3:0]      rqcnt;

  assign out_valid_o      = (rqcnt != 4'd0);
  assign out_tok_o        = rq_tok[rq_rp];
  assign out_tag_o        = rq_tag[rq_rp];
  assign out_str_o        = rq_str[rq_rp];
  assign out_degenerate_o = rq_deg[rq_rp];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sd_wp         <= 4'd0;
      a7_v_q        <= 1'b0;
      rq_wp         <= 3'd0;
      rq_rp         <= 3'd0;
      rqcnt          <= 4'd0;
      accepted_o    <= 32'd0;
      sheet_reads_o <= 32'd0;
      degenerate_o  <= 32'd0;
    end else begin
      // ---- A0 capture ---------------------------------------------------
      if (req_valid_i && req_ready_o) begin
        sd_degen[sd_wp] <= degen_c;
        sd_satu[sd_wp]  <= satu_c;
        sd_satv[sd_wp]  <= satv_c;
        sd_tok[sd_wp]   <= req_tok_i;
        sd_wp           <= sd_wp + 4'd1;
        accepted_o      <= accepted_o + 32'd1;
        if (degen_c) degenerate_o <= degenerate_o + 32'd1;
      end

      // ---- A7 ------------------------------------------------------------
      a7_v_q <= div_ov;
      if (div_ov) begin
        a7_u_q       <= tex_u_c;
        a7_v_coord_q <= tex_v_c;
        a7_tok_q     <= sd_tok[div_tag];
        a7_degen_q   <= sd_degen[div_tag];
      end
      if (sheet_valid_o && sheet_ready_i) sheet_reads_o <= sheet_reads_o + 32'd1;

      // ---- A8/A9: the return queue's count moves ONCE --------------------
      // Two producers (a sheet response, and a degenerate bypass) and one
      // consumer. Counting each separately is the fault the perspective lane
      // had; the net is computed here instead.
      begin
        automatic logic p_sheet = sheet_rvalid_i;
        automatic logic p_degen = a7_v_q && a7_degen_q;
        automatic logic pop     = out_valid_o && out_ready_i;
        automatic logic [1:0] pushes = 2'(p_sheet) + 2'(p_degen);
        rqcnt <= rqcnt + 4'(pushes) - 4'(pop);

        if (p_sheet) begin
          rq_tok[rq_wp] <= sheet_rtok_i;
          rq_tag[rq_wp] <= sheet_tag_i;
          rq_str[rq_wp] <= sheet_str_i;
          rq_deg[rq_wp] <= 1'b0;
        end
        if (p_degen) begin
          automatic logic [2:0] w = p_sheet ? (rq_wp + 3'd1) : rq_wp;
          rq_tok[w] <= a7_tok_q;
          rq_tag[w] <= 8'd0;
          rq_str[w] <= 8'd0;
          rq_deg[w] <= 1'b1;
        end
        rq_wp <= rq_wp + 3'(pushes);
        if (pop) rq_rp <= rq_rp + 3'd1;
      end
    end
  end

endmodule : zhao_texture_aux_pipe

`default_nettype wire
