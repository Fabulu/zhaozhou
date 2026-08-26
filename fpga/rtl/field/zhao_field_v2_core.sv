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
  localparam logic [7:0] OP_NOISE2 = 8'h1C;   // the noise unit, is_ridge = 0
  localparam logic [7:0] OP_ROT2   = 8'h28;   // the rot unit, is_rot3 = 0
  localparam logic [7:0] OP_ROT3   = 8'h29;   // the rot unit, is_rot3 = 1, axis in imm
  localparam logic [7:0] OP_NRM2   = 8'h15;   // the normalize unit, is3 = 0
  localparam logic [7:0] OP_NRM3   = 8'h16;   // the normalize unit, is3 = 1

  // Which long-op unit a request is for. Carried through the serialiser, which
  // stays unit-agnostic; the routing is done here.
  // THE UNIT SELECTOR IS THREE BITS. It was two, and four units filled it
  // exactly; ROT is the fifth. Widening rather than sharing an encoding with a
  // mode: a selector that needs a second field to disambiguate it is a selector
  // that will eventually be read without one.
  localparam logic [2:0] UNIT_CURVE = 3'd0;
  localparam logic [2:0] UNIT_LEN   = 3'd1;
  localparam logic [2:0] UNIT_RING  = 3'd2;
  localparam logic [2:0] UNIT_NOISE = 3'd3;
  localparam logic [2:0] UNIT_ROT   = 3'd4;
  localparam logic [2:0] UNIT_NORM  = 3'd5;

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
  // FOUR REPLICAS, ONE WRITE PORT. Measured 2026-08-26: as a single array
  // read on four ports and written from four places in one clock, this did
  // not become memory -- it did not even reach MLAB. quartus_map reported
  // ZERO ram-conversion warnings, meaning it was never a candidate, and
  // listed rf bit by bit (rf[2][16][17], a 6:1 mux at 512 LEs). The block
  // came out at 121,292 ALMs against the device's 41,910 and the fit errored
  // 96 minutes into placement. reports/FIELD_V2_Regfile_Ports.md has the
  // numbers.
  //
  // An M10K is one write and one read. So the file is replicated once per
  // reader -- a, b, c and the host -- with every write mirrored into all
  // four. Replication buys READ ports; it does nothing for write ports, so
  // the write side is reduced to exactly one port below.
  // The arrays themselves live in zhao_field_rf_ram, one instance per lane per
  // reader, because `[LANES][0:511]` inside this always_ff is two-dimensional
  // and shares a process with the whole issue machine -- measured 2026-08-26 to
  // infer as 75,835 flops with zero ram-conversion warnings either way.
  logic signed [31:0] rd_a [LANES];
  logic signed [31:0] rd_b [LANES];
  logic signed [31:0] rd_c [LANES];
  logic signed [31:0] h_rd [LANES];

  // ---- the write-back queue ---------------------------------------------
  // The multi-result reply used to write dst, dst+1 and dst+2 in ONE clock.
  // Three writes to three addresses is three write ports, and that is the
  // half of the problem replication cannot solve. They are spent one per
  // clock instead, which costs at most three clocks on an operation that
  // already costs hundreds -- the measured CURVE+SPLINE mix was 2,020 clocks
  // for 24 instructions.
  logic [1:0]         wbq_cnt;            // results still owed, 0..3
  logic [1:0]         wbq_idx;            // which one is next, 0..2
  logic [WFW-1:0]     wbq_wf;
  logic [RW-1:0]      wbq_dst;
  logic signed [31:0] wbq_y [LANES][0:2];
  wire                wbq_busy = (wbq_cnt != 2'd0);

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
        OP_RING, OP_RIDGE, OP_NOISE2,
        OP_ROT2, OP_ROT3,
        OP_NRM2, OP_NRM3:               ;  // dispatched, not executed here
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
  // NOISE2 is RIDGE's sibling on the same unit, but it reads reg[a+1] and writes
  // TWO registers, so it takes the second read pass and the multi-result reply.
  wire s1_is_noise2 = s1_valid && (s1_op == OP_NOISE2);
  // ROT2 and ROT3 read reg[a..a+2] and the ANGLE from reg[b], all of which the
  // second read pass already fetches, and write two or three registers, which
  // the reply already carries. They cost no new front-end mechanism at all.
  wire s1_is_rot2  = s1_valid && (s1_op == OP_ROT2);
  wire s1_is_rot3  = s1_valid && (s1_op == OP_ROT3);
  wire s1_is_rot   = s1_is_rot2 || s1_is_rot3;
  // NORMALIZE2/3 read reg[a..a+2] and write two or three registers -- the
  // second read pass and the multi-result reply, both already here. The one
  // thing they add is a SECOND CONSUMER for the integer square root.
  wire s1_is_nrm2  = s1_valid && (s1_op == OP_NRM2);
  wire s1_is_nrm3  = s1_valid && (s1_op == OP_NRM3);
  wire s1_is_nrm   = s1_is_nrm2 || s1_is_nrm3;
  // "Long" is the property that matters to the scoreboard: dispatched to a unit
  // over the request/reply seam rather than executed here. Both families are.
  wire s1_is_long  = s1_is_curve || s1_is_len || s1_is_ring || s1_is_ridge ||
                     s1_is_noise2 || s1_is_rot || s1_is_nrm;

  // The mode is per-UNIT, so the two families have independent encodings and
  // the unit selector is what disambiguates them. zhao_field_curve reads
  // 0/1/2 = CURVE/DCURVE/SPLINE; zhao_field_len reads 0/1/2 = LEN2/LEN3/DIST2.
  logic [1:0] s1_mode;
  logic [2:0] s1_unit;
  // How many CONSECUTIVE registers this instruction writes. Everything built so
  // far writes one; NOISE2 is the first that does not.
  // ROT3 is the first op to write THREE. ROT2 writes two: its third lane is
  // zero by the unit's law 5 and must NOT be written, because dst+2 belongs to
  // whatever the program put there.
  wire [1:0]  s1_nres = (s1_is_rot3 || s1_is_nrm3)              ? 2'd3
                      : (s1_is_noise2 || s1_is_rot2 || s1_is_nrm2) ? 2'd2
                                                                   : 2'd1;
  always_comb begin
    unique case (s1_op)
      OP_DCURVE: begin s1_mode = 2'd1; s1_unit = UNIT_CURVE; end
      OP_SPLINE: begin s1_mode = 2'd2; s1_unit = UNIT_CURVE; end
      OP_LEN2:   begin s1_mode = 2'd0; s1_unit = UNIT_LEN;   end
      OP_LEN3:   begin s1_mode = 2'd1; s1_unit = UNIT_LEN;   end
      OP_DIST2:  begin s1_mode = 2'd2; s1_unit = UNIT_LEN;   end
      OP_RING:   begin s1_mode = 2'd0; s1_unit = UNIT_RING;  end
      OP_RIDGE:  begin s1_mode = 2'd1; s1_unit = UNIT_NOISE; end
      OP_NOISE2: begin s1_mode = 2'd0; s1_unit = UNIT_NOISE; end
      OP_ROT2:   begin s1_mode = 2'd0; s1_unit = UNIT_ROT;   end
      OP_ROT3:   begin s1_mode = 2'd1; s1_unit = UNIT_ROT;   end
      OP_NRM2:   begin s1_mode = 2'd0; s1_unit = UNIT_NORM;  end
      OP_NRM3:   begin s1_mode = 2'd1; s1_unit = UNIT_NORM;  end
      default:   begin s1_mode = 2'd0; s1_unit = UNIT_CURVE; end
    endcase
  end

  // The PASS-2 instruction's tag, held across the steal cycle. s1_wf and s1_dst
  // happen to survive -- nothing issues, so nothing overwrites them -- but
  // depending on that is depending on a stall staying exactly as it is today.
  //
  // It carries the unit and result count as well now: the steal path used to
  // serve only lengths, so it could hard-wire UNIT_LEN and one result. NOISE2
  // takes the same path to a different unit with two.
  logic [WFW-1:0]     ln_wf;
  logic [RW-1:0]      ln_dst;
  logic [1:0]         ln_mode;
  logic [2:0]         ln_unit;
  logic [1:0]         ln_nres;
  logic [31:0]        ln_imm;

  logic               lq_valid;
  logic [WFW-1:0]     lq_wf;
  logic [RW-1:0]      lq_dst;
  logic [1:0]         lq_mode;
  logic [2:0]         lq_unit;
  logic [31:0]        lq_imm;
  logic [1:0]         lq_nres;
  logic signed [31:0] lq_a  [LANES];
  logic signed [31:0] lq_a1 [LANES];
  logic signed [31:0] lq_a2 [LANES];
  logic signed [31:0] lq_b0 [LANES];
  logic signed [31:0] lq_b1 [LANES];

  logic               lm_req_ready, lm_rsp_valid;
  logic [WFW-1:0]     lm_rsp_wf;
  logic [RW-1:0]      lm_rsp_dst;
  logic [1:0]         lm_rsp_nres;
  logic signed [31:0] lm_rsp_y  [LANES];
  logic signed [31:0] lm_rsp_y1 [LANES];
  logic signed [31:0] lm_rsp_y2 [LANES];

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
                     (ins_op_i == OP_RING) || (ins_op_i == OP_RIDGE) ||
                     (ins_op_i == OP_NOISE2) || (ins_op_i == OP_ROT2) ||
                     (ins_op_i == OP_ROT3) || (ins_op_i == OP_NRM2) ||
                     (ins_op_i == OP_NRM3);
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
  // NOT "is a length" any more: NOISE2 reads reg[a+1] too, and ROT2/ROT3 and
  // NORMALIZE2/3 will. The steal fetches {a+1, a+2, b+1} and every one of them
  // wants some part of that, so the predicate is about the NEED, not the family.
  wire s1_needs_pass2 = s1_is_len || s1_is_noise2 || s1_is_rot || s1_is_nrm;
  wire steal_now = s1_needs_pass2;
  // ISSUE STOPS WHILE THE WRITE-BACK QUEUE DRAINS, and that is what makes the
  // single write port safe. The ALU write-back has priority over the queue,
  // so without this a steady stream of short ops could starve a reply
  // indefinitely. With it, at most the one instruction already in stage 1 can
  // still write, so the queue is guaranteed a free port within one clock and
  // drains in at most four. It also makes a second long reply arriving on top
  // of an undrained queue impossible: no long op can issue while it is busy.
  assign issue_fire = sel_valid && !pc_overrun && !steal_now && !wbq_busy &&
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

  // ---- THE ONE WRITE PORT ------------------------------------------------
  // Three sources, in strict priority: the ALU write-back, then the queued
  // long-op results, then the host. The host is last because it is the only
  // one that can wait for free -- it is driven exclusively while the machine
  // is being filled, never while wavefronts run (zhao_field_v2_front.sv drives
  // h_we only in F_FILL), so in practice it never contends at all. Written as
  // a priority chain rather than as an assumption, because the core cannot
  // enforce what its instantiator does.
  logic               wb_we   [LANES];
  logic [RFAW-1:0]    wb_addr;
  logic signed [31:0] wb_data [LANES];
  wire                wbq_go = wbq_busy && !s1_writes;
  wire                retire_s1   = s1_valid && !unsupported &&
                                    (s1_is_end || !s1_is_long);
  wire                retire_long = wbq_go && (wbq_cnt == 2'd1);

  always_comb begin
    integer wl;
    for (wl = 0; wl < LANES; wl++) begin
      wb_we[wl]   = 1'b0;
      wb_data[wl] = '0;
    end
    wb_addr = '0;
    if (s1_writes) begin
      wb_addr = {s1_wf, s1_dst};
      for (wl = 0; wl < LANES; wl++) begin
        wb_we[wl]   = 1'b1;
        wb_data[wl] = alu_y[wl];
      end
    end else if (wbq_busy) begin
      wb_addr = {wbq_wf, RW'(wbq_dst + RW'(wbq_idx))};
      for (wl = 0; wl < LANES; wl++) begin
        wb_we[wl]   = 1'b1;
        wb_data[wl] = wbq_y[wl][wbq_idx];
      end
    end else if (h_we_i) begin
      // ONE LANE ONLY. The host fills a point one register at a time and each
      // lane holds a different point, so a host write that broadcast would
      // overwrite three other points with this one's value.
      wb_addr           = {h_wf_i, h_reg_i};
      wb_we[h_lane_i]   = 1'b1;
      wb_data[h_lane_i] = h_wdata_i;
    end
  end

  // ---- the storage ------------------------------------------------------
  // LANES x 4 memories: one per lane per reader. Every one takes the SAME
  // write -- address, data and enable -- which is what keeps the replicas
  // identical and lets any of them answer for the file. Replication buys read
  // ports; it does nothing for write ports, which is why there is exactly one
  // write port above.
  genvar gl;
  generate
    for (gl = 0; gl < LANES; gl++) begin : g_rf
      zhao_field_rf_ram #(.AW(RFAW), .DW(32)) u_rf_a (
          .clk(clk), .we_i(wb_we[gl]), .waddr_i(wb_addr), .wdata_i(wb_data[gl]),
          .raddr_i({rd_wf, addr_a}), .rdata_o(rd_a[gl]));
      zhao_field_rf_ram #(.AW(RFAW), .DW(32)) u_rf_b (
          .clk(clk), .we_i(wb_we[gl]), .waddr_i(wb_addr), .wdata_i(wb_data[gl]),
          .raddr_i({rd_wf, addr_b}), .rdata_o(rd_b[gl]));
      zhao_field_rf_ram #(.AW(RFAW), .DW(32)) u_rf_c (
          .clk(clk), .we_i(wb_we[gl]), .waddr_i(wb_addr), .wdata_i(wb_data[gl]),
          .raddr_i({rd_wf, addr_c}), .rdata_o(rd_c[gl]));
      zhao_field_rf_ram #(.AW(RFAW), .DW(32)) u_rf_h (
          .clk(clk), .we_i(wb_we[gl]), .waddr_i(wb_addr), .wdata_i(wb_data[gl]),
          .raddr_i({h_rwf_i, h_rreg_i}), .rdata_o(h_rd[gl]));
    end
  endgenerate

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
      wbq_cnt        <= 2'd0;
      wbq_idx        <= 2'd0;
      wbq_wf         <= '0;
      wbq_dst        <= '0;
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
      ln_unit        <= UNIT_LEN;
      ln_nres        <= 2'd1;
      ln_imm         <= 32'd0;
      lq_nres        <= 2'd1;
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
      // The register reads happen in the rams, not here.

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
      if (s1_needs_pass2) begin
        ln_wf   <= s1_wf;
        ln_dst  <= s1_dst;
        ln_mode <= s1_mode;
        ln_unit <= s1_unit;
        ln_nres <= s1_nres;
        ln_imm  <= s1_imm;
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
        lq_nres  <= s1_nres;
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
        lq_imm   <= ln_imm;  // NOISE2 takes this path AND reads a seed
        lq_wf    <= ln_wf;
        lq_dst   <= ln_dst;
        lq_mode  <= ln_mode;
        lq_unit  <= ln_unit;
        lq_nres  <= ln_nres;
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
          nz_sat_add   || rt_sat_add)                 sat_add_o     <= 1'b1;
      if (cv_sat_mul  || rg_sat_mul  || rt_sat_mul)  sat_mul_o     <= 1'b1;
      if (cv_sat_resc || ln_sat_resc || rg_sat_resc ||
          nz_sat_resc  || nm_sat_resc)                sat_rescale_o <= 1'b1;
      // The reciprocal's two lanes come from BOTH the ring's own accounting and
      // the reciprocal itself: the ring reports what it was told, and the unit
      // reports what it did. Taking only one of them would lose a reciprocal of
      // zero that the ring never asked about.
      if (rg_sat_rcp || rc_sat_rcp)                  sat_rcp_o     <= 1'b1;
      if (rg_rcp0    || rc_rcp0    || nm_rcp0)       rcp_zero_o    <= 1'b1;

      // ---- long-op reply: write back and release the wavefront -----------
      // UP TO THREE CONSECUTIVE REGISTERS, ONE PER CLOCK. The count rides with
      // the reply, so this does not re-decode an opcode that left stage 1
      // several cycles ago. Only the owed results are written: writing dst+1
      // unconditionally would clobber a register a single-result op never
      // claimed, and the neighbouring register is exactly where the NEXT
      // instruction's operand is most likely to live.
      //
      // The wavefront is released when the LAST owed result lands, not when
      // the reply arrives -- releasing at arrival would let the wavefront
      // issue an instruction that reads a register the queue has not yet
      // written.
      if (lm_rsp_valid) begin
        for (l = 0; l < LANES; l++) begin
          wbq_y[l][0] <= lm_rsp_y[l];
          wbq_y[l][1] <= lm_rsp_y1[l];
          wbq_y[l][2] <= lm_rsp_y2[l];
        end
        wbq_wf  <= lm_rsp_wf;
        wbq_dst <= lm_rsp_dst;
        wbq_cnt <= lm_rsp_nres;
        wbq_idx <= 2'd0;
      end else if (wbq_go) begin
        wbq_idx <= wbq_idx + 2'd1;
        wbq_cnt <= wbq_cnt - 2'd1;
        if (wbq_cnt == 2'd1) inflight[wbq_wf] <= 1'b0;
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
        end
      end

      // ONE ACCOUNT, ADDED ONCE. These used to be two separate
      // `instr_retired_o <= instr_retired_o + 1` statements inside the same
      // always_ff. When a long op retired on the same clock as a short op, the
      // later assignment won and ONE RETIREMENT WAS SILENTLY LOST -- every
      // value in the file still correct, the count quietly short. Summing both
      // terms into a single assignment is the only form that cannot drop one.
      // A long op counts when its last result LANDS, not when it dispatches
      // and not when its reply arrives.
      instr_retired_o <= instr_retired_o + 32'(retire_s1) + 32'(retire_long);

      // The register writes happen in the rams, not here.
    end
  end

  // ---- the serialiser, the scalar unit, and its multiplier lane ---------
  logic [5:0]         cv_seg_unused;
  logic               cv_sat_add, cv_sat_mul, cv_sat_resc;
  logic               ln_sat_add, ln_sat_resc;
  logic               rg_sat_add, rg_sat_mul, rg_sat_resc, rg_sat_rcp, rg_rcp0;
  logic               nz_sat_add, nz_sat_resc;
  logic               rt_sat_add, rt_sat_mul;
  logic               nm_rcp0, nm_sat_resc;
  logic               rc_sat_rcp, rc_rcp0;

  // The serialiser's single unit port, and the two units behind the mux.
  logic               u_valid, u_ready, u_rvalid, u_rready;
  logic [1:0]         u_mode;
  logic [2:0]         u_unit;
  logic [31:0]        u_imm;
  logic signed [31:0] u_a, u_a1, u_a2, u_b0, u_b1;
  // The unit port's three result lanes. Units producing one drive only the
  // first; the request's COUNT decides how many are written, so a unit leaving
  // the others at whatever it leaves them at cannot corrupt anything.
  logic signed [31:0] u_result, u_result1, u_result2;
  logic               cv_ready, cv_rvalid, ln_ready, ln_rvalid;
  logic               rg_ready, rg_rvalid;
  logic               nz_ready, nz_rvalid;
  logic               rt_ready, rt_rvalid;
  logic signed [31:0] rt_o0, rt_o1, rt_o2;
  logic               nm_ready, nm_rvalid;
  logic signed [31:0] nm_o0, nm_o1, nm_o2;

  // The normalize unit's own call into the shared integer square root. This is
  // the SECOND consumer -- the length family was the first -- which is why the
  // isqrt's four wires become a mux below rather than a direct connection.
  logic        nm_sq_valid, nm_sq_rready;
  logic [63:0] nm_sq_n;

  // v2's FIFTH shared resource. zhao_field_rot does not own a sine table -- it
  // borrows the engine's one, and v2 had none because nothing it executed
  // needed one. Latency 2, initiation interval 1, per that module's header.
  //
  // ROT is its only consumer today, so there is no mux. OP_SIN and OP_COS would
  // be the second, and they become nearly free once the table is here: one
  // operand on a natural port, one result, no steal.
  logic        [15:0] rt_sin_angle;
  logic               rt_sin_is_cos;
  logic signed [31:0] sin_result;
  // o1_o is NOISE2's second lane, zero for RIDGE by that unit's own law. It is
  // READ now -- the multi-result reply carries it back -- so the lint waiver it
  // used to need is gone rather than merely re-justified.
  logic signed [31:0] nz_result1;
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
  logic               nz_mul_issue, rt_mul_issue, nm_mul_issue;
  logic signed [32:0] nz_mul_a, nz_mul_b, rt_mul_a, rt_mul_b, nm_mul_a, nm_mul_b;
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
      .o0_o(nz_result), .o1_o(nz_result1),
      .sat_add_o(nz_sat_add), .sat_rescale_o(nz_sat_resc),
      .mul_issue_o(nz_mul_issue), .mul_a_o(nz_mul_a), .mul_b_o(nz_mul_b),
      .mul_p_i(mul_p), .mul_valid_i(mul_p_valid)
  );

  // v1's rotation unit, UNMODIFIED. ROT2 is is_rot3_i = 0; ROT3 adds the AXIS
  // from the immediate. Its own header states the four cases and which lane
  // passes through untouched, and that pass-through lane is the cheapest proof
  // the axis arrived.
  zhao_field_rot u_rot (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(u_valid && to_rot), .v_ready_o(rt_ready),
      .is_rot3_i(u_mode[0]), .axis_i(u_imm[1:0]),
      .ang_i(u_b0),
      .a0_i(u_a), .a1_i(u_a1), .a2_i(u_a2),
      .r_valid_o(rt_rvalid), .r_ready_i(u_rready && to_rot),
      .o0_o(rt_o0), .o1_o(rt_o1), .o2_o(rt_o2),
      .sat_add_o(rt_sat_add), .sat_mul_o(rt_sat_mul),
      .sin_angle_o(rt_sin_angle), .sin_is_cos_o(rt_sin_is_cos),
      .sin_result_i(sin_result),
      .mul_issue_o(rt_mul_issue), .mul_a_o(rt_mul_a), .mul_b_o(rt_mul_b),
      .mul_p_i(mul_p), .mul_valid_i(mul_p_valid)
  );

  zhao_field_sin u_sin (
      .clk(clk), .angle_i(rt_sin_angle), .is_cos_i(rt_sin_is_cos),
      .result_o(sin_result)
  );

  // v1's normalize unit, UNMODIFIED. NORMALIZE2 is is3_i = 0 and NORMALIZE3 is
  // is3_i = 1. It does NOT use the shared reciprocal -- it carries its own
  // rcp24 ROM -- and it DOES use the shared square root, which is why that is
  // now muxed.
  //
  // rcp0_o is raised by NORMALIZE2 ONLY, by that unit's law 3. NORMALIZE3 does
  // not raise it. That asymmetry is the unit's and the oracle's; a test
  // expecting the lane on NORMALIZE3 would be testing an assumption.
  zhao_field_normalize u_norm (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(u_valid && to_norm), .v_ready_o(nm_ready),
      .is3_i(u_mode[0]),
      .a0_i(u_a), .a1_i(u_a1), .a2_i(u_a2),
      .r_valid_o(nm_rvalid), .r_ready_i(u_rready && to_norm),
      .o0_o(nm_o0), .o1_o(nm_o1), .o2_o(nm_o2),
      .rcp0_o(nm_rcp0), .sat_rescale_o(nm_sat_resc),
      .mul_issue_o(nm_mul_issue), .mul_a_o(nm_mul_a), .mul_b_o(nm_mul_b),
      .mul_p_i(mul_p), .mul_valid_i(mul_p_valid),
      .sqrt_valid_o(nm_sq_valid), .sqrt_ready_i(isq_ready), .sqrt_n_o(nm_sq_n),
      .sqrt_rvalid_i(isq_rvalid), .sqrt_rready_o(nm_sq_rready), .sqrt_r_i(isq_r)
  );

  // The shared integer square root, now serving the length family AND normalize.
  logic        sq_valid, sq_ready, sq_rvalid, sq_rready;
  logic [63:0] sq_n, sq_r;

  // ---- THE INTEGER SQUARE ROOT NOW HAS TWO CONSUMERS ---------------------
  // It was wired straight to u_len, because the length family was the only
  // caller. NORMALIZE is the second, so the four wires become a mux on the
  // captured unit id -- the same shape as the multiplier's, licensed by the
  // same fact: the interlock keeps ONE long operation in the machine, so at
  // most one unit is asking.
  //
  // It is a MUX, NOT AN ARBITER, and it becomes wrong the moment two long ops
  // can run concurrently. Stated here rather than discovered then.
  logic        isq_valid, isq_ready, isq_rvalid, isq_rready;
  logic [63:0] isq_n, isq_r;

  assign isq_valid  = to_norm ? nm_sq_valid  : sq_valid;
  assign isq_n      = to_norm ? nm_sq_n      : sq_n;
  assign isq_rready = to_norm ? nm_sq_rready : sq_rready;

  assign sq_ready   = to_norm ? 1'b0 : isq_ready;
  assign sq_rvalid  = to_norm ? 1'b0 : isq_rvalid;
  assign sq_r       = isq_r;

  zhao_field_v2_lanemux #(.LANES(LANES), .WFS(WFS), .REGS(REGS)) u_lanemux (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(lq_valid), .req_ready_o(lm_req_ready),
      .req_wf_i(lq_wf), .req_dst_i(lq_dst), .req_mode_i(lq_mode),
      .req_unit_i(lq_unit), .req_imm_i(lq_imm), .req_nres_i(lq_nres),
      .req_a_i(lq_a), .req_a1_i(lq_a1), .req_a2_i(lq_a2),
      .req_b0_i(lq_b0), .req_b1_i(lq_b1),
      .u_valid_o(u_valid), .u_ready_i(u_ready),
      .u_mode_o(u_mode), .u_unit_o(u_unit), .u_imm_o(u_imm),
      .u_a_o(u_a), .u_a1_o(u_a1), .u_a2_o(u_a2), .u_b0_o(u_b0), .u_b1_o(u_b1),
      .u_rvalid_i(u_rvalid), .u_rready_o(u_rready),
      .u_result_i(u_result), .u_result1_i(u_result1), .u_result2_i(u_result2),
      .rsp_valid_o(lm_rsp_valid), .rsp_ready_i(1'b1),
      .rsp_wf_o(lm_rsp_wf), .rsp_dst_o(lm_rsp_dst), .rsp_nres_o(lm_rsp_nres),
      .rsp_y_o(lm_rsp_y), .rsp_y1_o(lm_rsp_y1), .rsp_y2_o(lm_rsp_y2)
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
  wire to_rot   = (u_unit == UNIT_ROT);
  wire to_norm  = (u_unit == UNIT_NORM);

  always_comb begin
    u_result1 = 32'sd0;
    u_result2 = 32'sd0;
    if (to_norm) begin
      u_ready   = nm_ready;
      u_rvalid  = nm_rvalid;
      u_result  = nm_o0;
      u_result1 = nm_o1;
      u_result2 = nm_o2;
    end else if (to_rot) begin
      u_ready   = rt_ready;
      u_rvalid  = rt_rvalid;
      u_result  = rt_o0;
      u_result1 = rt_o1;
      u_result2 = rt_o2;      // zero for ROT2 by law 5, and NOT written: the
                              // result count is 2, so dst+2 is never touched
    end else if (to_noise) begin
      u_ready   = nz_ready;
      u_rvalid  = nz_rvalid;
      u_result  = nz_result;
      u_result1 = nz_result1;   // NOISE2's second lane; zero for RIDGE by law
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
    end else if (to_norm) begin
      mul_issue = nm_mul_issue;
      mul_a     = nm_mul_a;
      mul_b     = nm_mul_b;
    end else if (to_rot) begin
      mul_issue = rt_mul_issue;
      mul_a     = rt_mul_a;
      mul_b     = rt_mul_b;
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
      .n_valid_i(isq_valid), .n_ready_o(isq_ready), .n_i(isq_n),
      .r_valid_o(isq_rvalid), .r_ready_i(isq_rready), .r_o(isq_r)
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
