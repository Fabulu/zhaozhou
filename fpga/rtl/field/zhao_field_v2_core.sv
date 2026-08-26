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
    // The instruction's immediate. A hash SEED for RIDGE and NOISE2, an AXIS
    // SELECT for ROT3 -- and an axis quietly dropped rotates about the wrong
    // one, which is a plausible-looking wrong world rather than a visible fault.
    // v2 had no immediate at all until RIDGE needed one.
    input  logic [31:0]                ins_imm_i,

    // ---- curve table lookup, passed through to the boundary --------------
    // A REGISTERED read: the index presented this cycle is answered on the
    // NEXT one, exactly as an M10K does. The table lives outside the unit in
    // v1 and stays outside here -- v2 consumes program tables, it does not own
    // them.
    output logic [5:0]         tbl_idx_o,
    input  logic [6:0]         tbl_n_i,
    input  logic signed [31:0] tbl_x_i,
    input  logic signed [31:0] tbl_y_i,
    input  logic signed [31:0] tbl_dy_i,

    // ---- saturation ledger ------------------------------------------------
    // NOT optional bookkeeping: saturation is part of the answer in this
    // engine, so v2 exposes it as v1 does. Dangling sat_* pins would silently
    // drop half the semantics of every long operation.
    output logic        sat_add_o,
    output logic        sat_mul_o,
    output logic        sat_rescale_o,
    output logic        sat_rcp_o,     // SatLedger::rcp
    output logic        rcp_zero_o,    // SatLedger::rcp0 -- a reciprocal of zero

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
  localparam logic [7:0] OP_CURVE  = 8'h1A;   // mode 0
  localparam logic [7:0] OP_SPLINE = 8'h1B;   // mode 2
  localparam logic [7:0] OP_DCURVE = 8'h1D;   // mode 1
  localparam logic [7:0] OP_LEN2   = 8'h12;   // len mode 0
  localparam logic [7:0] OP_LEN3   = 8'h13;   // len mode 1
  localparam logic [7:0] OP_DIST2  = 8'h14;   // len mode 2
  localparam logic [7:0] OP_RING   = 8'h21;   // its own unit, no mode
  localparam logic [7:0] OP_RIDGE  = 8'h22;   // the noise unit, is_ridge = 1

  // Which long-op unit a request is for. Carried through the serialiser, which
  // stays unit-agnostic; the routing is done here.
  localparam logic [1:0] UNIT_CURVE = 2'd0;
  localparam logic [1:0] UNIT_LEN   = 2'd1;
  localparam logic [1:0] UNIT_RING  = 2'd2;
  localparam logic [1:0] UNIT_NOISE = 2'd3;

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

  // ---- THE SECOND READ PASS ---------------------------------------------
  // Three read ports per lane supply a/b/c. The length family wants up to FIVE
  // values, and a+1 is not reachable from any of them. The alternative was five
  // ports: the banked file measured 12 M10K at three (zhao_probe_banked_rf) and
  // port count is what M10K replication scales with, so that is roughly +4 M10K
  // permanently, for a minority of the opcode histogram.
  //
  // Instead ONE CLOCK is spent. With the length op held in stage 1, the read
  // addresses are driven from {s1_a+1, s1_a+2, s1_b+1} for one cycle and those
  // answers land beside the first pass. A long operation already costs about
  // LANES x II -- the measured CURVE+SPLINE mix was 2,020 clocks for 24
  // instructions -- so this is under 1% of the operation it belongs to.
  //
  // The first pass's a and b must be SAVED before the steal overwrites them,
  // because the steal reuses those same three ports.
  logic signed [31:0] s2_a0 [LANES];
  logic signed [31:0] s2_b0 [LANES];
  logic               steal_q;      // a steal cycle is in progress

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
  // The length family addresses CONSECUTIVE registers from a base -- LEN3 reads
  // a, a+1, a+2 and DIST2 also reads b, b+1 -- so unlike every short op it needs
  // the operand INDEX after the first read has already happened.
  logic [RW-1:0]  s1_a;
  logic [RW-1:0]  s1_b;
  logic [31:0]    s1_imm;

  wire pc_overrun = sel_valid && (pc[sel] >= instr_count_i);
  // issue_fire is assigned further down, once s1_is_long exists: the long-op
  // interlock needs it, and a wire must be declared before it is used.
  wire issue_fire;

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
        OP_CURVE, OP_DCURVE, OP_SPLINE: ;  // dispatched, not executed here
        OP_LEN2, OP_LEN3, OP_DIST2:     ;  // dispatched, not executed here
        OP_RING, OP_RIDGE:              ;  // dispatched, not executed here
        default:   unsupported = 1'b1;     // REFUSED, not skipped and not zero
      endcase
    end
  end

  // ---- long operations, via the tagged lane serialiser -------------------
  // A long op is not executed here. It is handed to zhao_field_v2_lanemux,
  // which serialises the LANES lanes through one scalar unit and carries the
  // {wavefront, destination} tag back. The wavefront stays IN FLIGHT until the
  // reply lands: the scoreboard does not care how long an instruction takes.
  wire s1_is_len  = s1_valid && ((s1_op == OP_LEN2) || (s1_op == OP_LEN3) ||
                                (s1_op == OP_DIST2));
  wire s1_is_curve = s1_valid && ((s1_op == OP_CURVE) || (s1_op == OP_DCURVE) ||
                                  (s1_op == OP_SPLINE));
  // RING reads a, b and c -- the natural ports -- so it needs NO steal cycle and
  // dispatches from stage 1 exactly as a curve does. It is grouped with the
  // curve family for dispatch timing and separated from it only by the unit id.
  wire s1_is_ring  = s1_valid && (s1_op == OP_RING);
  // RIDGE reads reg[a] and reg[b] -- both NATURAL ports -- and returns one
  // value, so it dispatches from stage 1 exactly as RING does. Its sibling
  // NOISE2 does not: it reads a+1 and writes TWO registers, and waits on the
  // multi-result reply that does not exist yet.
  wire s1_is_ridge = s1_valid && (s1_op == OP_RIDGE);
  // "Long" is the property that matters to the scoreboard: dispatched to a unit
  // over the request/reply seam rather than executed here. Both families are.
  wire s1_is_long  = s1_is_curve || s1_is_len || s1_is_ring || s1_is_ridge;

  // The mode is per-UNIT, so the two families have independent encodings and
  // the unit selector is what disambiguates them. zhao_field_curve reads
  // 0/1/2 = CURVE/DCURVE/SPLINE; zhao_field_len reads 0/1/2 = LEN2/LEN3/DIST2.
  logic [1:0] s1_mode;
  logic [1:0] s1_unit;
  always_comb begin
    unique case (s1_op)
      OP_DCURVE: begin s1_mode = 2'd1; s1_unit = UNIT_CURVE; end
      OP_SPLINE: begin s1_mode = 2'd2; s1_unit = UNIT_CURVE; end
      OP_LEN2:   begin s1_mode = 2'd0; s1_unit = UNIT_LEN;   end
      OP_LEN3:   begin s1_mode = 2'd1; s1_unit = UNIT_LEN;   end
      OP_DIST2:  begin s1_mode = 2'd2; s1_unit = UNIT_LEN;   end
      OP_RING:   begin s1_mode = 2'd0; s1_unit = UNIT_RING;  end
      OP_RIDGE:  begin s1_mode = 2'd1; s1_unit = UNIT_NOISE; end
      default:   begin s1_mode = 2'd0; s1_unit = UNIT_CURVE; end
    endcase
  end

  // The length's tag, held across the steal cycle. s1_wf and s1_dst happen to
  // survive -- nothing issues, so nothing overwrites them -- but depending on
  // that is depending on a stall staying exactly as it is today.
  logic [WFW-1:0]     ln_wf;
  logic [RW-1:0]      ln_dst;
  logic [1:0]         ln_mode;

  logic               lq_valid;
  logic [WFW-1:0]     lq_wf;
  logic [RW-1:0]      lq_dst;
  logic [1:0]         lq_mode;
  logic [1:0]         lq_unit;
  logic [31:0]        lq_imm;
  logic signed [31:0] lq_a  [LANES];
  logic signed [31:0] lq_a1 [LANES];
  logic signed [31:0] lq_a2 [LANES];
  logic signed [31:0] lq_b0 [LANES];
  logic signed [31:0] lq_b1 [LANES];

  logic               lm_req_ready, lm_rsp_valid;
  logic [WFW-1:0]     lm_rsp_wf;
  logic [RW-1:0]      lm_rsp_dst;
  logic signed [31:0] lm_rsp_y [LANES];

  // ---- THE LONG-OP INTERLOCK ---------------------------------------------
  // The dispatch slot holds ONE request. It is filled at stage 2 -- two cycles
  // after the instruction issued -- so a guard on lq_valid alone is two cycles
  // late: a second long op reaches stage 2 while the first request is still
  // waiting for the serialiser, and the dispatch below overwrites it. The first
  // wavefront then waits forever for a reply to a request that no longer
  // exists, and the engine hangs.
  //
  // Found by mutation sweep, not by inspection: M93/M94/M95 all survived
  // because sections 6 and 7 start ONE wavefront and never put two long ops in
  // the machine at once. Section 8 does, and hung at the 20,000-clock guard
  // with 11 of 24 instructions retired.
  //
  // Only LONG ops are held. Short ops keep issuing past a pending request --
  // they touch neither the slot nor the serialiser, and stalling the whole
  // machine behind one curve lookup would give back the throughput v2 exists
  // for. Wavefronts write disjoint register regions, so a short op retiring
  // beside a long reply cannot collide with it.
  wire ins_is_long = (ins_op_i == OP_CURVE) || (ins_op_i == OP_DCURVE) ||
                     (ins_op_i == OP_SPLINE) || (ins_op_i == OP_LEN2) ||
                     (ins_op_i == OP_LEN3) || (ins_op_i == OP_DIST2) ||
                     (ins_op_i == OP_RING) || (ins_op_i == OP_RIDGE);
  // A LENGTH IS LONG FOR THREE CYCLES, not two: stage 1, the steal, and the
  // dispatch. Between stage 1 and the dispatch there is a cycle where s1_valid
  // is already 0 and lq_valid is not yet 1, and a long op issuing into that
  // window would reach stage 1 exactly as the length filled the slot, and
  // overwrite it -- the same hang the interlock exists to prevent, reached by a
  // different door.
  wire long_slot_busy = lq_valid || (s1_valid && s1_is_long) || steal_now || steal_q;

  // ---- THE STEAL-CYCLE STALL, which is stricter than the interlock -------
  // The interlock above holds only LONG ops, deliberately: short ops touch
  // neither the slot nor the serialiser, and stalling the machine behind one
  // curve lookup gives back the throughput v2 exists for.
  //
  // The steal cycle is different and must stall EVERYTHING. It reuses the three
  // read ports, so an instruction issuing into it would have its own operand
  // reads replaced by the length's second pass and would then compute on
  // another instruction's values. That is a WRONG ANSWER rather than a hang,
  // which makes it the more dangerous of the two failures.
  wire steal_now = s1_is_len;   // stage 1 holds a length: steal the next cycle
  assign issue_fire = sel_valid && !pc_overrun && !steal_now &&
                      !(ins_is_long && long_slot_busy);

  // ---- read addresses ----------------------------------------------------
  // Pass 1 is the natural addressing every short op uses. Pass 2 -- the steal
  // -- redirects the same three ports at {a+1, a+2, b+1} of the length held in
  // stage 1. Nothing else in the machine reads the file on that cycle.
  wire [RW-1:0] addr_a = steal_now ? RW'(s1_a + RW'(1)) : ins_a_i;
  wire [RW-1:0] addr_b = steal_now ? RW'(s1_a + RW'(2)) : ins_b_i;
  wire [RW-1:0] addr_c = steal_now ? RW'(s1_b + RW'(1)) : ins_c_i;
  // The wavefront being read is the LENGTH's during a steal, not whatever the
  // round robin happens to be pointing at -- `sel` is free to move.
  wire [WFW-1:0] rd_wf = steal_now ? s1_wf : sel;

  wire s1_is_end = s1_valid && (s1_op == OP_END);
  wire s1_writes = s1_valid && !s1_is_end && !unsupported && !s1_is_long;

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
      s1_a           <= '0;
      s1_b           <= '0;
      s1_imm         <= 32'd0;
      instr_retired_o<= 32'd0;
      lq_valid       <= 1'b0;
      lq_wf          <= '0;
      lq_dst         <= '0;
      lq_mode        <= 2'd0;
      lq_unit        <= UNIT_CURVE;
      lq_imm         <= 32'd0;
      ln_wf          <= '0;
      ln_dst         <= '0;
      ln_mode        <= 2'd0;
      steal_q        <= 1'b0;
      sat_add_o      <= 1'b0;
      sat_mul_o      <= 1'b0;
      sat_rescale_o  <= 1'b0;
      sat_rcp_o      <= 1'b0;
      rcp_zero_o     <= 1'b0;
      for (i = 0; i < LANES; i++) begin
        lq_a[i]  <= '0;
        lq_a1[i] <= '0;
        lq_a2[i] <= '0;
        lq_b0[i] <= '0;
        lq_b1[i] <= '0;
        s2_a0[i] <= '0;
        s2_b0[i] <= '0;
      end
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
        s1_a     <= ins_a_i;
        s1_b     <= ins_b_i;
        s1_imm   <= ins_imm_i;
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
        rd_a[l] <= rf[l][{rd_wf, addr_a}];
        rd_b[l] <= rf[l][{rd_wf, addr_b}];
        rd_c[l] <= rf[l][{rd_wf, addr_c}];
        h_rd[l] <= rf[l][{h_rwf_i, h_rreg_i}];
      end

      // Save pass 1's a and b before the steal overwrites those ports. This
      // fires on the cycle the length is in stage 1, which is the last cycle
      // rd_a and rd_b still hold its first-pass values.
      steal_q <= steal_now;
      if (steal_now) begin
        for (l = 0; l < LANES; l++) begin
          s2_a0[l] <= rd_a[l];
          s2_b0[l] <= rd_b[l];
        end
      end

      // ---- stage 2 retire ----
      // ---- long-op dispatch, held until the serialiser accepts -----------
      if (s1_is_len) begin
        ln_wf   <= s1_wf;
        ln_dst  <= s1_dst;
        ln_mode <= s1_mode;
      end

      // The curve family dispatches straight from stage 1: one operand, already
      // read. The length family dispatches ONE CYCLE LATER, when the second
      // read pass has landed -- which is the whole cost of not adding two more
      // register-file ports.
      if (s1_is_curve || s1_is_ring || s1_is_ridge) begin
        lq_valid <= 1'b1;
        lq_wf    <= s1_wf;
        lq_dst   <= s1_dst;
        lq_mode  <= s1_mode;
        lq_unit  <= s1_unit;
        lq_imm   <= s1_imm;
        for (l = 0; l < LANES; l++) begin
          lq_a[l]  <= rd_a[l];
          // RING's r0 and r1 ride a1/a2. A curve reads neither, and zeroing
          // them there keeps a curve's request from carrying a stale radius
          // that a mis-routed unit could read as real.
          // RING's inner radius and RIDGE's second coordinate are both reg[b].
          // A curve reads neither, and zeroing them there keeps its request from
          // carrying a stale radius a mis-routed unit could read as real.
          lq_a1[l] <= (s1_is_ring || s1_is_ridge) ? rd_b[l] : 32'sd0;
          lq_a2[l] <= s1_is_ring ? rd_c[l] : 32'sd0;
          lq_b0[l] <= 32'sd0;
          lq_b1[l] <= 32'sd0;
        end
      end else if (steal_q) begin
        lq_valid <= 1'b1;
        lq_imm   <= 32'd0;   // no length op reads an immediate
        lq_wf    <= ln_wf;
        lq_dst   <= ln_dst;
        lq_mode  <= ln_mode;
        lq_unit  <= UNIT_LEN;
        for (l = 0; l < LANES; l++) begin
          lq_a[l]  <= s2_a0[l];   // reg[a],   saved before the steal
          lq_a1[l] <= rd_a[l];    // reg[a+1], from the steal
          lq_a2[l] <= rd_b[l];    // reg[a+2], from the steal
          lq_b0[l] <= s2_b0[l];   // reg[b],   saved before the steal
          lq_b1[l] <= rd_c[l];    // reg[b+1], from the steal
        end
      end else if (lq_valid && lm_req_ready) begin
        lq_valid <= 1'b0;
      end

      // Saturation is STICKY, as in v1: a run that saturated once did so.
      // Both units feed the sticky ledger. The length unit's sat_add_o is
      // DIST2's differences and its sat_rescale_o the narrow to s32; dropping
      // them would lose half the semantics of a length exactly as dangling
      // sat_* pins would have lost half of a curve's.
      if (cv_sat_add  || ln_sat_add  || rg_sat_add ||
          nz_sat_add)                                 sat_add_o     <= 1'b1;
      if (cv_sat_mul  || rg_sat_mul)                 sat_mul_o     <= 1'b1;
      if (cv_sat_resc || ln_sat_resc || rg_sat_resc ||
          nz_sat_resc)                                sat_rescale_o <= 1'b1;
      // The reciprocal's two lanes come from BOTH the ring's own accounting and
      // the reciprocal itself: the ring reports what it was told, and the unit
      // reports what it did. Taking only one of them would lose a reciprocal of
      // zero that the ring never asked about.
      if (rg_sat_rcp || rc_sat_rcp)                  sat_rcp_o     <= 1'b1;
      if (rg_rcp0    || rc_rcp0)                     rcp_zero_o    <= 1'b1;

      // ---- long-op reply: write back and release the wavefront -----------
      if (lm_rsp_valid) begin
        for (l = 0; l < LANES; l++) rf[l][{lm_rsp_wf, lm_rsp_dst}] <= lm_rsp_y[l];
        inflight[lm_rsp_wf] <= 1'b0;
        instr_retired_o     <= instr_retired_o + 32'd1;
      end

      if (s1_valid) begin
        // A LONG OP KEEPS ITS WAVEFRONT IN FLIGHT. Clearing it here would let
        // the wavefront issue again while the serialiser still owed it an
        // answer -- the hazard the one-in-flight rule exists to prevent, and
        // invisible until two wavefronts contend for the unit.
        if (!s1_is_long) inflight[s1_wf] <= 1'b0;
        if (unsupported) begin
          if (status_o == ST_OK) status_o <= ST_UNSUPPORTED_OP;
          active[s1_wf]   <= 1'b0;
          finished[s1_wf] <= 1'b1;
        end else if (s1_is_end) begin
          active[s1_wf]   <= 1'b0;
          finished[s1_wf] <= 1'b1;
          instr_retired_o <= instr_retired_o + 32'd1;
        end else if (!s1_is_long) begin
          // A long op counts when its REPLY lands, not when it dispatches.
          instr_retired_o <= instr_retired_o + 32'd1;
        end
      end

      // ---- writes: host, then write-back ----
      if (h_we_i) rf[h_lane_i][{h_wf_i, h_reg_i}] <= h_wdata_i;
      if (s1_writes)
        for (l = 0; l < LANES; l++) rf[l][{s1_wf, s1_dst}] <= alu_y[l];
    end
  end

  // ---- the serialiser, the scalar unit, and its multiplier lane ---------
  logic [5:0]         cv_seg_unused;
  logic               cv_sat_add, cv_sat_mul, cv_sat_resc;
  logic               ln_sat_add, ln_sat_resc;
  logic               rg_sat_add, rg_sat_mul, rg_sat_resc, rg_sat_rcp, rg_rcp0;
  logic               nz_sat_add, nz_sat_resc;
  logic               rc_sat_rcp, rc_rcp0;

  // The serialiser's single unit port, and the two units behind the mux.
  logic               u_valid, u_ready, u_rvalid, u_rready;
  logic [1:0]         u_mode, u_unit;
  logic [31:0]        u_imm;
  logic signed [31:0] u_a, u_a1, u_a2, u_b0, u_b1, u_result;
  logic               cv_ready, cv_rvalid, ln_ready, ln_rvalid;
  logic               rg_ready, rg_rvalid;
  logic               nz_ready, nz_rvalid;
  // o1_o is NOISE2's second lane and is zero for RIDGE by that unit's own law.
  // Nothing reads it yet: carrying it back needs the multi-result reply, which
  // is the next increment. Waived WITH the reason, because an unexplained
  // waiver is how a dropped output becomes permanent.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] nz_o1_unused;
  /* verilator lint_on UNUSEDSIGNAL */
  logic signed [31:0] cv_result, ln_result, rg_result, nz_result;

  // RING's own call into the shared reciprocal.
  logic               rg_rcp_valid, rg_rcp_rready;
  logic signed [31:0] rg_rcp_a;
  logic               rc_ready, rc_rvalid;
  logic signed [31:0] rc_result;

  // One multiplier, muxed on the captured unit id -- see the routing note.
  logic               mul_issue, mul_p_valid;
  logic signed [32:0] mul_a, mul_b;
  logic signed [65:0] mul_p;
  logic               cv_mul_issue, ln_mul_issue, rg_mul_issue, rc_mul_issue;
  logic               nz_mul_issue;
  logic signed [32:0] nz_mul_a, nz_mul_b;
  logic signed [32:0] cv_mul_a, cv_mul_b, ln_mul_a, ln_mul_b;
  logic signed [32:0] rg_mul_a, rg_mul_b, rc_mul_a, rc_mul_b;

  // v1's ring unit, UNMODIFIED. Three operands straight off the natural ports,
  // so it needs no steal cycle -- it is the cheapest long op to reach, and the
  // operand bundle built for DIST2 carries d/r0/r1 unchanged.
  zhao_field_ring u_ring (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(u_valid && to_ring), .v_ready_o(rg_ready),
      .d_i(u_a), .r0_i(u_a1), .r1_i(u_a2),
      .r_valid_o(rg_rvalid), .r_ready_i(u_rready && to_ring), .result_o(rg_result),
      .sat_add_o(rg_sat_add), .sat_mul_o(rg_sat_mul), .sat_rescale_o(rg_sat_resc),
      .sat_rcp_o(rg_sat_rcp), .rcp0_o(rg_rcp0),
      .rcp_valid_o(rg_rcp_valid), .rcp_ready_i(rc_ready), .rcp_a_o(rg_rcp_a),
      .rcp_rvalid_i(rc_rvalid), .rcp_rready_o(rg_rcp_rready),
      .rcp_result_i(rc_result), .rcp_sat_i(rc_sat_rcp), .rcp_zero_i(rc_rcp0),
      .mul_issue_o(rg_mul_issue), .mul_a_o(rg_mul_a), .mul_b_o(rg_mul_b),
      .mul_p_i(mul_p), .mul_valid_i(mul_p_valid)
  );

  // The shared reciprocal. RING is its only caller today; RCP and NORMALIZE
  // would be the others, and no committed Earth program calls NORMALIZE.
  zhao_field_rcp u_rcp (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(rg_rcp_valid), .v_ready_o(rc_ready), .a_i(rg_rcp_a),
      .r_valid_o(rc_rvalid), .r_ready_i(rg_rcp_rready), .result_o(rc_result),
      .sat_rcp_o(rc_sat_rcp), .rcp0_o(rc_rcp0),
      .mul_issue_o(rc_mul_issue), .mul_a_o(rc_mul_a), .mul_b_o(rc_mul_b),
      .mul_p_i(mul_p), .mul_valid_i(mul_p_valid)
  );

  // v1's noise unit, UNMODIFIED. RIDGE is its is_ridge_i = 1 mode. NOISE2 is
  // the other mode and is deliberately NOT wired: it writes two registers and
  // the reply carries one, so wiring it now would mean dropping o1_o silently.
  // It stays REFUSED with a status until the multi-result reply exists.
  zhao_field_noise u_noise (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(u_valid && to_noise), .v_ready_o(nz_ready),
      .is_ridge_i(u_mode[0]),
      .a0_i(u_a), .a1_i(u_a1), .seed_i(u_imm),
      .r_valid_o(nz_rvalid), .r_ready_i(u_rready && to_noise),
      .o0_o(nz_result), .o1_o(nz_o1_unused),
      .sat_add_o(nz_sat_add), .sat_rescale_o(nz_sat_resc),
      .mul_issue_o(nz_mul_issue), .mul_a_o(nz_mul_a), .mul_b_o(nz_mul_b),
      .mul_p_i(mul_p), .mul_valid_i(mul_p_valid)
  );

  // The shared integer square root, used only by the length family today.
  logic        sq_valid, sq_ready, sq_rvalid, sq_rready;
  logic [63:0] sq_n, sq_r;

  zhao_field_v2_lanemux #(.LANES(LANES), .WFS(WFS), .REGS(REGS)) u_lanemux (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(lq_valid), .req_ready_o(lm_req_ready),
      .req_wf_i(lq_wf), .req_dst_i(lq_dst), .req_mode_i(lq_mode),
      .req_unit_i(lq_unit), .req_imm_i(lq_imm),
      .req_a_i(lq_a), .req_a1_i(lq_a1), .req_a2_i(lq_a2),
      .req_b0_i(lq_b0), .req_b1_i(lq_b1),
      .u_valid_o(u_valid), .u_ready_i(u_ready),
      .u_mode_o(u_mode), .u_unit_o(u_unit), .u_imm_o(u_imm),
      .u_a_o(u_a), .u_a1_o(u_a1), .u_a2_o(u_a2), .u_b0_o(u_b0), .u_b1_o(u_b1),
      .u_rvalid_i(u_rvalid), .u_rready_o(u_rready), .u_result_i(u_result),
      .rsp_valid_o(lm_rsp_valid), .rsp_ready_i(1'b1),
      .rsp_wf_o(lm_rsp_wf), .rsp_dst_o(lm_rsp_dst), .rsp_y_o(lm_rsp_y)
  );

  // v1's curve unit, UNMODIFIED -- the same silicon the frozen engine uses and
  // the same differential covers. v2 changes how work reaches it, not what it
  // computes. It owns no multiplier: the 2026-08-23 rearchitecture took ten
  // private ones to one shared lane and 79 DSPs to 3, so v2 supplies the lane.
  // That seam is MUL_LANES in reports/FIELD_V2_MODEL.md, which prices 1/2/3/4
  // at 3/6/9/12 DSPs and finds width 4 needs at least two.
  // ---- ROUTING, and why an arbiter is not needed -------------------------
  // Two long-op units now share one serialiser port and one multiplier. The
  // interlock guarantees at most ONE long operation is in the machine at a
  // time, so at most one unit is ever active, and the selection is a MUX on the
  // captured unit id rather than an arbiter. That is v1's own structure: it
  // muxed ten units on the executing opcode for exactly this reason.
  //
  // If MUL_LANES > 1 ever lets two long ops run concurrently, this mux becomes
  // wrong and must become an arbiter. Stated here rather than discovered then.
  wire to_len  = (u_unit == UNIT_LEN);
  wire to_ring  = (u_unit == UNIT_RING);
  wire to_noise = (u_unit == UNIT_NOISE);

  always_comb begin
    if (to_noise) begin
      u_ready  = nz_ready;
      u_rvalid = nz_rvalid;
      u_result = nz_result;
    end else if (to_ring) begin
      u_ready  = rg_ready;
      u_rvalid = rg_rvalid;
      u_result = rg_result;
    end else if (to_len) begin
      u_ready  = ln_ready;
      u_rvalid = ln_rvalid;
      u_result = ln_result;
    end else begin
      u_ready  = cv_ready;
      u_rvalid = cv_rvalid;
      u_result = cv_result;
    end
  end

  // ---- THE MULTIPLIER IS NO LONGER A MUX --------------------------------
  // RING is the first operation with TWO consumers inside it: zhao_field_ring
  // drives the lane, and so does the zhao_field_rcp it calls twice. The
  // one-long-op-in-flight argument covers one UNIT being active, not one unit
  // making two demands, so selection becomes a PRIORITY CHAIN.
  //
  // The reciprocal outranks the executing unit. That is v1's own arrangement
  // (zhao_field_exec_shared), feeding the same units against the same oracle --
  // inventing a different arbitration here would mean re-proving what v1's
  // differential already covers.
  always_comb begin
    if (rc_mul_issue) begin
      mul_issue = rc_mul_issue;
      mul_a     = rc_mul_a;
      mul_b     = rc_mul_b;
    end else if (to_noise) begin
      mul_issue = nz_mul_issue;
      mul_a     = nz_mul_a;
      mul_b     = nz_mul_b;
    end else if (to_ring) begin
      mul_issue = rg_mul_issue;
      mul_a     = rg_mul_a;
      mul_b     = rg_mul_b;
    end else if (to_len) begin
      mul_issue = ln_mul_issue;
      mul_a     = ln_mul_a;
      mul_b     = ln_mul_b;
    end else begin
      mul_issue = cv_mul_issue;
      mul_a     = cv_mul_a;
      mul_b     = cv_mul_b;
    end
  end

  zhao_field_curve u_curve (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(u_valid && !to_len), .v_ready_o(cv_ready),
      .mode_i(u_mode), .a_i(u_a),
      .tbl_n_i(tbl_n_i), .tbl_idx_o(tbl_idx_o),
      .tbl_x_i(tbl_x_i), .tbl_y_i(tbl_y_i), .tbl_dy_i(tbl_dy_i),
      .r_valid_o(cv_rvalid), .r_ready_i(u_rready && !to_len), .result_o(cv_result),
      .seg_idx_o(cv_seg_unused),
      .sat_add_o(cv_sat_add), .sat_mul_o(cv_sat_mul), .sat_rescale_o(cv_sat_resc),
      .mul_issue_o(cv_mul_issue), .mul_a_o(cv_mul_a), .mul_b_o(cv_mul_b),
      .mul_p_i(mul_p), .mul_valid_i(mul_p_valid)
  );

  // v1's length unit, also UNMODIFIED. It covers LEN2, LEN3 and DIST2 in three
  // modes, exactly as the curve unit covers its three -- which is why this is
  // wiring rather than arithmetic, and why the oracle it is checked against is
  // the same zfield::interpret the frozen engine uses.
  zhao_field_len u_len (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(u_valid && to_len), .v_ready_o(ln_ready),
      .mode_i(u_mode),
      .a0_i(u_a), .a1_i(u_a1), .a2_i(u_a2), .b0_i(u_b0), .b1_i(u_b1),
      .r_valid_o(ln_rvalid), .r_ready_i(u_rready && to_len), .result_o(ln_result),
      .sat_add_o(ln_sat_add), .sat_rescale_o(ln_sat_resc),
      .mul_issue_o(ln_mul_issue), .mul_a_o(ln_mul_a), .mul_b_o(ln_mul_b),
      .mul_p_i(mul_p), .mul_valid_i(mul_p_valid),
      .sqrt_valid_o(sq_valid), .sqrt_ready_i(sq_ready), .sqrt_n_o(sq_n),
      .sqrt_rvalid_i(sq_rvalid), .sqrt_rready_o(sq_rready), .sqrt_r_i(sq_r)
  );

  // The shared integer square root. Only the length family uses it today, so it
  // is wired directly; NORMALIZE would be the second consumer, and no committed
  // Earth program calls NORMALIZE.
  zhao_field_isqrt u_isqrt (
      .clk(clk), .rst_n(rst_n),
      .n_valid_i(sq_valid), .n_ready_o(sq_ready), .n_i(sq_n),
      .r_valid_o(sq_rvalid), .r_ready_i(sq_rready), .r_o(sq_r)
  );

  zhao_field_mul u_mul (
      .clk(clk), .rst_n(rst_n),
      .issue_i(mul_issue), .a_i(mul_a), .b_i(mul_b),
      .p_o(mul_p), .p_valid_o(mul_p_valid)
  );

  assign busy_o     = active;
  assign done_o     = finished;
  assign h_rdata_o  = h_rd[h_rlane_i];

endmodule : zhao_field_v2_core
