// zhao_field_v2_core.sv — FIELD v2: a small SIMD barrel engine.
//
// Authorised by owner ruling 2026-08-25. FIELD v1 (`zhao_field_seq.sv`) is
// FROZEN as the exact serial reference, the differential oracle and the
// fallback; this is built ALONGSIDE it, not out of it.
//
// ---------------------------------------------------------------------------
// WHY A NEW CORE RATHER THAN ANOTHER TIMING WAVE
// ---------------------------------------------------------------------------
// v1's defining assumption is ONE INSTRUCTION IN FLIGHT. Every multi-cycle op
// drains before the next is fetched, and that is what lets it have no arbiter,
// one executing opcode, one accumulator owner, one write-back owner and
// untagged unit replies. All of those become false the moment two samples
// coexist, so mutating v1 into a vector machine would falsify its invariants
// one at a time with the suite passing throughout.
//
// Waves 3-10 took v1 from 8.59 to 58.99 MHz -- 6.9x, all fitter-measured -- and
// it is still ~7x short on THROUGHPUT, because the terrain cost model assumes
// one instruction per clock and v1 retires one every seven.
//
// ---------------------------------------------------------------------------
// THE SHAPE, AND WHY IT FITS THIS WORKLOAD SPECIFICALLY
// ---------------------------------------------------------------------------
// Field evaluates the SAME straight-line program over thousands of independent
// points; one terrain patch is 1,089 vertices. So:
//
//   * LANES vertices execute one instruction together (SIMD). They share PC,
//     decode and program memory. Field programs are straight-line, so there is
//     no branch divergence to handle -- that is a property of the ISA, not an
//     assumption being made here.
//   * WFS such groups (wavefronts) stay resident. At most ONE instruction is in
//     flight per wavefront, so when one waits, another issues.
//
// That is fine-grained multithreading: hide latency with independent work
// rather than try to make latency disappear. The measured Earth histogram
// (reports/FIELD_V2_MODEL.md) says width 4 with 8 wavefronts is the point that
// closes Earth60.
//
// ONE INSTRUCTION IN FLIGHT PER WAVEFRONT IS DELIBERATE AND IS THE WHOLE TRICK.
// It means no intra-wavefront RAW forwarding, no renaming and no reorder
// buffer: a wavefront simply is not revisited until its previous instruction
// retired. Dependencies within a vertex stop mattering. The cost is that a
// single wavefront runs no faster than v1 -- throughput comes from having
// several.
//
// ---------------------------------------------------------------------------
// THIS INCREMENT
// ---------------------------------------------------------------------------
// Simple single-cycle ALU ops only: MOV ADD SUB MUL MAD MIN MAX ABS CLAMP
// SELECT CMP. Long operations (LEN, NORMALIZE, RCP, CURVE, RING, NOISE, ROT,
// SIN/COS) are NOT executed here yet and raise `ST_UNSUPPORTED_OP`, exactly as
// v1 refuses an unknown opcode -- refused, never skipped and never zero, since
// a sequencer that quietly ignores an opcode produces a plausible field and a
// wrong world.
//
// The tagged long-operation interface, the banked register file (already
// probed at 12 M10K / 375 ALM / 96.54 MHz) and the multi-lane write-back come
// next, in the order the measured histogram gives: CURVE/DCURVE, DIST2/isqrt,
// RING, then multiplier lanes.
//
// ENFORCED-BY: tests/differential/field_v2_core_directed.cpp:main
module zhao_field_v2_core #(
    parameter int LANES = 4,    // vertices per wavefront
    parameter int WFS   = 8,    // resident wavefronts
    parameter int REGS  = 64
) (
    input  logic clk,
    input  logic rst_n,

    // ---- host port: load a wavefront's registers, read them back ----------
    input  logic                      h_we_i,
    input  logic [$clog2(WFS)-1:0]    h_wf_i,
    input  logic [$clog2(LANES)-1:0]  h_lane_i,
    input  logic [$clog2(REGS)-1:0]   h_reg_i,
    input  logic signed [31:0]        h_wdata_i,
    input  logic [$clog2(WFS)-1:0]    h_rwf_i,
    input  logic [$clog2(LANES)-1:0]  h_rlane_i,
    input  logic [$clog2(REGS)-1:0]   h_rreg_i,
    output logic signed [31:0]        h_rdata_o,

    // ---- run control, per wavefront --------------------------------------
    input  logic [WFS-1:0]  start_i,      // pulse: begin this wavefront at pc 0
    output logic [WFS-1:0]  busy_o,
    output logic [WFS-1:0]  done_o,
    output logic [7:0]      status_o,     // first non-OK status seen
    input  logic [7:0]      instr_count_i,

    // ---- instruction fetch: one shared program ---------------------------
    output logic [7:0]                 pc_o,        // pc of the issuing wavefront
    input  logic [7:0]                 ins_op_i,
    input  logic [$clog2(REGS)-1:0]    ins_dst_i,
    input  logic [$clog2(REGS)-1:0]    ins_a_i,
    input  logic [$clog2(REGS)-1:0]    ins_b_i,
    input  logic [$clog2(REGS)-1:0]    ins_c_i,

    // ---- observability ---------------------------------------------------
    output logic [31:0] instr_retired_o   // vector instructions retired
);

  localparam logic [7:0] OP_END    = 8'hFF;
  localparam logic [7:0] OP_MOV    = 8'h00;
  localparam logic [7:0] OP_ADD    = 8'h01;
  localparam logic [7:0] OP_SUB    = 8'h02;
  localparam logic [7:0] OP_MUL    = 8'h03;
  localparam logic [7:0] OP_MAD    = 8'h04;
  localparam logic [7:0] OP_MIN    = 8'h05;
  localparam logic [7:0] OP_MAX    = 8'h06;
  localparam logic [7:0] OP_ABS    = 8'h07;
  localparam logic [7:0] OP_CLAMP  = 8'h08;
  localparam logic [7:0] OP_SELECT = 8'h09;
  localparam logic [7:0] OP_CMP    = 8'h0A;

  localparam logic [7:0] ST_OK             = 8'd0;
  localparam logic [7:0] ST_PC_OVERRUN     = 8'd2;
  localparam logic [7:0] ST_UNSUPPORTED_OP = 8'd3;

  localparam int WFW = $clog2(WFS);
  localparam int RW  = $clog2(REGS);

  // ---- register storage, one memory per lane ----------------------------
  // Synchronous read, no reset touching the array, no byte enables -- the shape
  // QUARTUS_GOTCHAS §10 says infers. Address is {wavefront, register}: a
  // wavefront's registers are contiguous, so a lane's file is one memory.
  localparam int RFAW = WFW + RW;
  logic signed [31:0] rf   [LANES][0:(1<<RFAW)-1];
  logic signed [31:0] rd_a [LANES];
  logic signed [31:0] rd_b [LANES];
  logic signed [31:0] rd_c [LANES];
  logic signed [31:0] h_rd [LANES];

  // ---- wavefront state --------------------------------------------------
  logic [7:0]     pc      [WFS];
  logic [WFS-1:0] active;      // started and not finished
  logic [WFS-1:0] inflight;    // has an instruction in the pipe
  logic [WFS-1:0] finished;

  // A wavefront may issue when it is active and has nothing in flight. THAT
  // ONE BIT is what removes the need for forwarding: the previous instruction
  // has already written back before the next is even fetched.
  logic [WFS-1:0] ready;
  assign ready = active & ~inflight & ~finished;

  // ---- round-robin selection -------------------------------------------
  // Round robin rather than fixed priority so no wavefront starves behind a
  // busier neighbour; with one instruction in flight each, fairness here is
  // what keeps the lanes fed.
  logic [WFW-1:0] rr_ptr;
  logic [WFW-1:0] sel;
  logic           sel_valid;

  always_comb begin
    sel       = '0;
    sel_valid = 1'b0;
    for (int k = WFS - 1; k >= 0; k--) begin
      automatic logic [WFW-1:0] idx = WFW'((int'(rr_ptr) + k) % WFS);
      if (ready[idx]) begin
        sel       = idx;
        sel_valid = 1'b1;
      end
    end
  end

  assign pc_o = sel_valid ? pc[sel] : 8'd0;

  // ---- stage 1: issue ---------------------------------------------------
  // The instruction word and the operand addresses are presented here; the
  // register reads land on the next edge.
  logic           s1_valid;
  logic [WFW-1:0] s1_wf;
  logic [7:0]     s1_op;
  logic [RW-1:0]  s1_dst;

  wire pc_overrun = sel_valid && (pc[sel] >= instr_count_i);
  wire issue_fire = sel_valid && !pc_overrun;

  // ---- stage 2: execute and write back ---------------------------------
  logic signed [31:0] alu_y [LANES];
  logic               unsupported;

  function automatic logic signed [31:0] sat_add(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] s;
    begin
      s = {a[31], a} + {b[31], b};
      if (s > 33'sh0_7FFFFFFF)       sat_add = 32'sh7FFF_FFFF;
      else if (s < -33'sh0_80000000) sat_add = 32'sh8000_0000;
      else                           sat_add = s[31:0];
    end
  endfunction

  function automatic logic signed [31:0] sat_sub(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [32:0] s;
    begin
      s = {a[31], a} - {b[31], b};
      if (s > 33'sh0_7FFFFFFF)       sat_sub = 32'sh7FFF_FFFF;
      else if (s < -33'sh0_80000000) sat_sub = 32'sh8000_0000;
      else                           sat_sub = s[31:0];
    end
  endfunction

  // Q16.16 multiply with round-half-up, matching the reference's rescale.
  function automatic logic signed [31:0] q16_mul(input logic signed [31:0] a,
                                                 input logic signed [31:0] b);
    logic signed [63:0] p;
    logic signed [63:0] r;
    begin
      p = 64'(a) * 64'(b);
      r = (p + 64'sd32768) >>> 16;
      if (r > 64'sd2147483647)       q16_mul = 32'sh7FFF_FFFF;
      else if (r < -64'sd2147483648) q16_mul = 32'sh8000_0000;
      else                           q16_mul = r[31:0];
    end
  endfunction

  always_comb begin
    unsupported = 1'b0;
    for (int l = 0; l < LANES; l++) alu_y[l] = 32'sd0;
    if (s1_valid) begin
      unique case (s1_op)
        OP_MOV:    for (int l = 0; l < LANES; l++) alu_y[l] = rd_a[l];
        OP_ADD:    for (int l = 0; l < LANES; l++) alu_y[l] = sat_add(rd_a[l], rd_b[l]);
        OP_SUB:    for (int l = 0; l < LANES; l++) alu_y[l] = sat_sub(rd_a[l], rd_b[l]);
        OP_MUL:    for (int l = 0; l < LANES; l++) alu_y[l] = q16_mul(rd_a[l], rd_b[l]);
        OP_MAD:    for (int l = 0; l < LANES; l++)
                     alu_y[l] = sat_add(q16_mul(rd_a[l], rd_b[l]), rd_c[l]);
        OP_MIN:    for (int l = 0; l < LANES; l++)
                     alu_y[l] = (rd_a[l] < rd_b[l]) ? rd_a[l] : rd_b[l];
        OP_MAX:    for (int l = 0; l < LANES; l++)
                     alu_y[l] = (rd_a[l] > rd_b[l]) ? rd_a[l] : rd_b[l];
        OP_ABS:    for (int l = 0; l < LANES; l++)
                     alu_y[l] = (rd_a[l] < 0) ? sat_sub(32'sd0, rd_a[l]) : rd_a[l];
        OP_CLAMP:  for (int l = 0; l < LANES; l++)
                     alu_y[l] = (rd_a[l] < rd_b[l]) ? rd_b[l]
                              : (rd_a[l] > rd_c[l]) ? rd_c[l] : rd_a[l];
        OP_SELECT: for (int l = 0; l < LANES; l++)
                     alu_y[l] = (rd_c[l] != 0) ? rd_a[l] : rd_b[l];
        OP_CMP:    for (int l = 0; l < LANES; l++)
                     alu_y[l] = (rd_a[l] < rd_b[l]) ? 32'sh0001_0000 : 32'sd0;
        OP_END:    ;                       // retires the wavefront, writes nothing
        default:   unsupported = 1'b1;     // REFUSED, not skipped and not zero
      endcase
    end
  end

  wire s1_is_end = s1_valid && (s1_op == OP_END);
  wire s1_writes = s1_valid && !s1_is_end && !unsupported;

  // ---- sequential ------------------------------------------------------
  integer i, l;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (i = 0; i < WFS; i++) pc[i] <= 8'd0;
      active         <= '0;
      inflight       <= '0;
      finished       <= '0;
      status_o       <= ST_OK;
      rr_ptr         <= '0;
      s1_valid       <= 1'b0;
      s1_wf          <= '0;
      s1_op          <= 8'd0;
      s1_dst         <= '0;
      instr_retired_o<= 32'd0;
    end else begin
      // start pulses
      for (i = 0; i < WFS; i++) begin
        if (start_i[i]) begin
          pc[i]       <= 8'd0;
          active[i]   <= 1'b1;
          finished[i] <= 1'b0;
        end
      end

      // ---- stage 1 capture ----
      s1_valid <= issue_fire;
      if (issue_fire) begin
        s1_wf    <= sel;
        s1_op    <= ins_op_i;
        s1_dst   <= ins_dst_i;
        inflight[sel] <= 1'b1;
        pc[sel]  <= pc[sel] + 8'd1;
        rr_ptr   <= (sel == WFW'(WFS-1)) ? '0 : (sel + WFW'(1));
      end

      // A pc that ran past the program's end without an END is a STATUS, not a
      // hang -- the same law v1 enforces.
      if (pc_overrun) begin
        if (status_o == ST_OK) status_o <= ST_PC_OVERRUN;
        active[sel]   <= 1'b0;
        finished[sel] <= 1'b1;
      end

      // ---- register reads land here ----
      for (l = 0; l < LANES; l++) begin
        rd_a[l] <= rf[l][{sel, ins_a_i}];
        rd_b[l] <= rf[l][{sel, ins_b_i}];
        rd_c[l] <= rf[l][{sel, ins_c_i}];
        h_rd[l] <= rf[l][{h_rwf_i, h_rreg_i}];
      end

      // ---- stage 2 retire ----
      if (s1_valid) begin
        inflight[s1_wf] <= 1'b0;
        if (unsupported) begin
          if (status_o == ST_OK) status_o <= ST_UNSUPPORTED_OP;
          active[s1_wf]   <= 1'b0;
          finished[s1_wf] <= 1'b1;
        end else if (s1_is_end) begin
          active[s1_wf]   <= 1'b0;
          finished[s1_wf] <= 1'b1;
          instr_retired_o <= instr_retired_o + 32'd1;
        end else begin
          instr_retired_o <= instr_retired_o + 32'd1;
        end
      end

      // ---- writes: host, then write-back ----
      if (h_we_i) rf[h_lane_i][{h_wf_i, h_reg_i}] <= h_wdata_i;
      if (s1_writes)
        for (l = 0; l < LANES; l++) rf[l][{s1_wf, s1_dst}] <= alu_y[l];
    end
  end

  assign busy_o     = active;
  assign done_o     = finished;
  assign h_rdata_o  = h_rd[h_rlane_i];

endmodule : zhao_field_v2_core
