// zhao_probe_dist_svc.sv — Field v3 decisive probe 3 (reports/Fieldv3.md
// Phase 3): the two-bank exact distance service.
//
// TARGET: four-point DIST2 initiation interval <= 20 clocks, timing-clean at
// the gpu-domain constraint. A probe that misses kills or changes this
// topology BEFORE it contaminates the engine — that is the probe's whole job.
//
// WHAT IT IS: eight copies of the exact restoring-root unit the engine
// already ships (zhao_field_isqrt, ~251 mapped ALMs each, 32 fixed
// iterations), arranged as two banks of four. Bank A runs one four-point
// request while bank B accepts the next; a 1-bit order FIFO drains replies
// in ACCEPT ORDER, so "every reply returns to its issuing requester" is true
// by construction rather than by tag arithmetic.
// ENFORCED-BY: tests/differential/field_dist_svc_directed.cpp:main
//
// THE SERVICE BOUNDARY IS n2, NOT (dx,dz). The vector multiplier bank
// supplies the squares (Fieldv3.md section 6: "vector multiplier bank
// supplies the squares"); this service owes exactly what zref::len_of owes
// AFTER its u64 sum exists: len = isqrt_u64(n2), saturated to s32 with the
// rescale lane bumped on clamp. Keeping the multipliers out of the probe
// keeps the measured ALM/DSP numbers describing the SERVICE, not a private
// copy of the bank the engine already owns.
//
// EXACT, NOT APPROXIMATE: the root is the engine's own floor-exact unit; the
// saturation is zref::len_of's (len > INT32_MAX -> INT32_MAX + sat flag).
// ENFORCED-BY: tests/differential/field_dist_svc_directed.cpp:main
module zhao_probe_dist_svc (
    input logic clk,
    input logic rst_n,

    // ---- request: one four-point group, n2 per lane ------------------------
    input  logic        req_valid_i,
    output logic        req_ready_o,
    input  logic [63:0] req_n2_0_i,
    input  logic [63:0] req_n2_1_i,
    input  logic [63:0] req_n2_2_i,
    input  logic [63:0] req_n2_3_i,
    input  logic [7:0]  req_tag_i,

    // ---- reply: lengths + per-lane saturation, in accept order -------------
    output logic        rsp_valid_o,
    input  logic        rsp_ready_i,
    output logic [31:0] rsp_len_0_o,
    output logic [31:0] rsp_len_1_o,
    output logic [31:0] rsp_len_2_o,
    output logic [31:0] rsp_len_3_o,
    output logic [3:0]  rsp_sat_o,
    output logic [7:0]  rsp_tag_o
);

  localparam int LANES = 4;

  logic [LANES-1:0][63:0] req_n2;
  assign req_n2[0] = req_n2_0_i;
  assign req_n2[1] = req_n2_1_i;
  assign req_n2[2] = req_n2_2_i;
  assign req_n2[3] = req_n2_3_i;

  // ---- two banks of four roots ---------------------------------------------
  logic [1:0]                  bank_n_valid;
  logic [1:0][LANES-1:0]       bank_n_ready;
  logic [1:0][LANES-1:0]       bank_r_valid;
  logic [1:0]                  bank_r_ready;
  logic [1:0][LANES-1:0][63:0] bank_r;

  generate
    for (genvar b = 0; b < 2; b++) begin : g_bank
      for (genvar l = 0; l < LANES; l++) begin : g_lane
        zhao_field_isqrt u_root (
            .clk      (clk),
            .rst_n    (rst_n),
            .n_valid_i(bank_n_valid[b]),
            .n_ready_o(bank_n_ready[b][l]),
            .n_i      (req_n2[l]),
            .r_valid_o(bank_r_valid[b][l]),
            .r_ready_i(bank_r_ready[b]),
            .r_o      (bank_r[b][l])
        );
      end
    end
  endgenerate

  // The four units of a bank share one FSM schedule, so a bank is ready when
  // all four are, and done when all four are.
  logic [1:0] bank_free;
  logic [1:0] bank_done;
  assign bank_free[0] = &bank_n_ready[0];
  assign bank_free[1] = &bank_n_ready[1];
  assign bank_done[0] = &bank_r_valid[0];
  assign bank_done[1] = &bank_r_valid[1];

  // Per-bank tag store.
  logic [1:0][7:0] bank_tag;

  // ---- accept: bank 0 preferred, order recorded ---------------------------
  // The order FIFO holds WHICH bank owns request k; replies drain in that
  // order. At most one request is in flight per bank, so depth 4 is generous.
  logic [3:0] ord_bank;  // circular, indexed by 2-bit pointers
  logic [2:0] ord_wr, ord_rd;
  logic [2:0] ord_count;

  logic accept_b;  // which bank takes this request
  assign accept_b   = bank_free[0] ? 1'b0 : 1'b1;
  assign req_ready_o = (bank_free[0] | bank_free[1]) && (ord_count != 3'd4);

  assign bank_n_valid[0] = req_valid_i && req_ready_o && (accept_b == 1'b0);
  assign bank_n_valid[1] = req_valid_i && req_ready_o && (accept_b == 1'b1);

  // ---- drain: the bank at the FIFO head, when its four roots are done -----
  logic head_valid;
  logic head_bank;
  assign head_valid = (ord_count != 3'd0);
  assign head_bank  = ord_bank[ord_rd[1:0]];

  logic drain_fire;
  assign drain_fire = head_valid && bank_done[head_bank] && (!rsp_valid_o || rsp_ready_i);
  assign bank_r_ready[0] = drain_fire && (head_bank == 1'b0);
  assign bank_r_ready[1] = drain_fire && (head_bank == 1'b1);

  // zref::len_of saturation: len > INT32_MAX -> INT32_MAX + rescale-lane flag.
  function automatic logic [31:0] sat_len(input logic [63:0] len);
    return (len > 64'h7FFF_FFFF) ? 32'h7FFF_FFFF : len[31:0];
  endfunction

  logic [LANES-1:0][31:0] rsp_len_q;
  assign rsp_len_0_o = rsp_len_q[0];
  assign rsp_len_1_o = rsp_len_q[1];
  assign rsp_len_2_o = rsp_len_q[2];
  assign rsp_len_3_o = rsp_len_q[3];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      ord_bank    <= '0;
      ord_wr      <= '0;
      ord_rd      <= '0;
      ord_count   <= '0;
      bank_tag    <= '0;
      rsp_valid_o <= 1'b0;
      rsp_len_q   <= '0;
      rsp_sat_o   <= '0;
      rsp_tag_o   <= '0;
    end else begin
      if (rsp_valid_o && rsp_ready_i) rsp_valid_o <= 1'b0;

      // accept
      if (req_valid_i && req_ready_o) begin
        ord_bank[ord_wr[1:0]] <= accept_b;
        ord_wr <= ord_wr + 3'd1;
        bank_tag[accept_b] <= req_tag_i;
      end

      // drain
      if (drain_fire) begin
        for (int l = 0; l < LANES; l++) begin
          rsp_len_q[l] <= sat_len(bank_r[head_bank][l]);
          rsp_sat_o[l] <= (bank_r[head_bank][l] > 64'h7FFF_FFFF);
        end
        rsp_tag_o   <= bank_tag[head_bank];
        rsp_valid_o <= 1'b1;
        ord_rd <= ord_rd + 3'd1;
      end

      // count (accept and drain can coincide)
      unique case ({req_valid_i && req_ready_o, drain_fire})
        2'b10:   ord_count <= ord_count + 3'd1;
        2'b01:   ord_count <= ord_count - 3'd1;
        default: ;
      endcase
    end
  end

endmodule : zhao_probe_dist_svc
