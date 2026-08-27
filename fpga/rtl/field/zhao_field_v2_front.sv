// zhao_field_v2_front.sv — the front end that DRIVES zhao_field_v2_core.
//
// STATUS RULING 2026-08-27 (reports/Fieldv3.md, Phase 1): FROZEN with the v2
// core — exact fallback and differential reference RTL, NOT the Earth60
// production path. This fill/run/drain front is the measured reason: its
// point transport alone is 12x1,089 + 13x1,089 = 27,225 clocks/association
// against a 10,416 allowance — 261% of the reserved frame before one
// instruction executes. The v3 production path replaces it with direct
// profile walkers that generate four x,z points per vector group and export
// results without the three-phase host-port reads. Changes here are limited
// to correctness bugs. See zhao_field_v2_core.sv for the full ruling.
//
// v2 executes programs. It does not fetch them, it does not know where points
// come from, and it does not know which registers the answers live in. Until
// now a testbench did all three, which is why "the Field engine is finished"
// was never true: the engine was finished and unusable.
//
// ---------------------------------------------------------------------------
// WHAT THE REFERENCE DOES, which is the contract this block implements
// ---------------------------------------------------------------------------
// `zref::terrain` (reference/src/zrender/terrain.cpp) walks a patch lattice and
// calls `zfield::interpret` ONCE PER POINT:
//
//     for each lattice point:
//         in[0..n_in-1]  = {cx, cz, age, progress, parameters...}
//         zfield::interpret(prog, in, n_in, out, n_out)
//         accumulate out[0] into the height lane
//
// and `zfield.hpp` states the mapping: "`in` lanes map to R0.. in the program's
// input order; `out` lanes are read from the output map at END."
//
// So this block owes exactly that: a program in, a stream of input records in,
// a stream of output records out, one out per in, in order.
//
// ---------------------------------------------------------------------------
// THE SHAPE, and why it is a BATCH rather than a stream
// ---------------------------------------------------------------------------
// v2 holds LANES x WFS points at once — 32 at the default width. This block
// fills all 32 slots, starts every wavefront, waits for them, drains the
// answers, and repeats.
//
// A rolling scheme that refilled a wavefront the moment it finished would hide
// more of the drain behind execution. It is not built here, deliberately: the
// batch is the version whose ORDERING is obviously correct, and point order is
// a correctness property — the reference accumulates `out[0]` into a lattice
// index derived from the loop counters, so a driver that returns points out of
// order silently builds the wrong terrain. Overlap can come later, against a
// differential that already pins the order.
//
// ---------------------------------------------------------------------------
// PROGRAM STORAGE
// ---------------------------------------------------------------------------
// One instruction memory, written through a host port and read combinationally
// by pc. v2 asks for the instruction of whichever wavefront it selected, on the
// same cycle it presents `pc_o` — so this read cannot be registered without
// changing the core's contract.
//
// It is small (INSTRS x ~56 bits) and is expected to infer as logic or a small
// distributed RAM rather than an M10K. That is the right trade at this size:
// QUARTUS_GOTCHAS §10 is about arrays large enough to want a block, and a
// 64-entry instruction table is not one.
//
// ENFORCED-BY: tests/differential/field_v2_front_directed.cpp:main
module zhao_field_v2_front #(
    parameter int LANES  = 4,
    parameter int WFS    = 8,
    parameter int REGS   = 64,
    parameter int INSTRS = 64,
    parameter int NIN    = 12,   // reference caps the input record at 12 lanes
    parameter int NOUT   = 4     // ... and the output record at 4
) (
    input logic clk,
    input logic rst_n,

    // ---- program load ------------------------------------------------------
    input logic                     prog_we_i,
    input logic [$clog2(INSTRS)-1:0] prog_addr_i,
    input logic [7:0]               prog_op_i,
    input logic [$clog2(REGS)-1:0]  prog_dst_i,
    input logic [$clog2(REGS)-1:0]  prog_a_i,
    input logic [$clog2(REGS)-1:0]  prog_b_i,
    input logic [$clog2(REGS)-1:0]  prog_c_i,
    input logic [31:0]              prog_imm_i,

    // ---- configuration, held for the whole run -----------------------------
    input logic [7:0]               instr_count_i,
    input logic [$clog2(NIN+1)-1:0] n_in_i,
    input logic [$clog2(NOUT+1)-1:0] n_out_i,
    // Which register each output lane is read from. The reference reads these
    // "from the output map at END" -- the map is a property of the PROGRAM, so
    // it is configuration here and not something this block can infer.
    input logic [$clog2(REGS)-1:0]  out_reg_i [NOUT],

    // ---- the curve table, passed through to the boundary --------------------
    // Tying these off would silently make every CURVE/DCURVE/SPLINE program
    // return the table's idea of index zero. The table belongs to whoever owns
    // the program, so it passes through rather than being invented here.
    output logic [5:0]               tbl_idx_o,
    input  logic [6:0]               tbl_n_i,
    input  logic signed [31:0]       tbl_x_i,
    input  logic signed [31:0]       tbl_y_i,
    input  logic signed [31:0]       tbl_dy_i,

    // ---- point stream in ---------------------------------------------------
    input  logic                     pt_valid_i,
    output logic                     pt_ready_o,
    input  logic signed [31:0]       pt_lane_i [NIN],

    // ---- result stream out, ONE PER POINT, IN ORDER ------------------------
    output logic                     res_valid_o,
    input  logic                     res_ready_i,
    output logic signed [31:0]       res_lane_o [NOUT],

    // ---- the engine's status, passed through --------------------------------
    output logic [7:0]               status_o,
    output logic                     sat_add_o,
    output logic                     sat_mul_o,
    output logic                     sat_rescale_o,
    output logic                     sat_rcp_o,
    output logic                     rcp_zero_o,
    output logic [31:0]              instr_retired_o
);

  localparam int WFW  = $clog2(WFS);
  localparam int LW   = $clog2(LANES);
  localparam int RW   = $clog2(REGS);
  localparam int IAW  = $clog2(INSTRS);
  localparam int SLOTS = LANES * WFS;
  localparam int SW   = $clog2(SLOTS + 1);
  // The lane counters must reach NIN/NOUT *inclusive* -- that is how "done" is
  // signalled -- but an ARRAY INDEX must be exactly wide enough for the array.
  // Using the counter directly is a width truncation, and Verilator is right to
  // refuse it: it would silently alias index N onto index 0.
  localparam int INW  = $clog2(NIN);
  localparam int ONW  = $clog2(NOUT);

  // ---- the instruction memory --------------------------------------------
  logic [7:0]    im_op  [INSTRS];
  logic [RW-1:0] im_dst [INSTRS];
  logic [RW-1:0] im_a   [INSTRS];
  logic [RW-1:0] im_b   [INSTRS];
  logic [RW-1:0] im_c   [INSTRS];
  logic [31:0]   im_imm [INSTRS];

  always_ff @(posedge clk) begin
    if (prog_we_i) begin
      im_op[prog_addr_i]  <= prog_op_i;
      im_dst[prog_addr_i] <= prog_dst_i;
      im_a[prog_addr_i]   <= prog_a_i;
      im_b[prog_addr_i]   <= prog_b_i;
      im_c[prog_addr_i]   <= prog_c_i;
      im_imm[prog_addr_i] <= prog_imm_i;
    end
  end

  // ---- the core ------------------------------------------------------------
  // pc_o is a byte because a program may be up to 256 instructions; this front
  // end holds INSTRS of them, so the top bits are unread by construction. A pc
  // past `instr_count_i` is refused by the core with ST_PC_OVERRUN rather than
  // truncated here, so a malformed program cannot wedge a batch.
  //
  // That claim had NO test anywhere until section 5 was written for it: the core
  // carried the RTL and nothing ever drove a program off its own end. The
  // ledger's V20 rule refused the claim, correctly.
  // ENFORCED-BY: tests/differential/field_v2_front_directed.cpp:main
  /* verilator lint_off UNUSEDSIGNAL */
  logic [7:0]     core_pc;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [WFS-1:0] core_start, core_busy, core_done;
  logic [WFS-1:0] started;   // which wavefronts this batch actually launched
  logic           launched;  // ... and whether the start pulse has been OBSERVED
  logic           h_we;
  logic [WFW-1:0] h_wf;
  logic [LW-1:0]  h_lane;
  logic [RW-1:0]  h_reg;
  logic signed [31:0] h_wdata;
  logic signed [31:0] h_rdata;

  // THE READ ADDRESS IS COMBINATIONAL, and that is a timing fact rather than a
  // preference. The core's register file is a REGISTERED read: the address
  // presented this cycle is answered on the next. Driving the address from a
  // flop as well would put it a further cycle behind, so a one-cycle wait
  // captured the PREVIOUS lane's value -- which showed up as exactly half the
  // output lanes being wrong, the most confusing possible symptom.
  wire [WFW-1:0] h_rwf   = drain_wf;
  wire [LW-1:0]  h_rlane = drain_ln;
  wire [RW-1:0]  h_rreg  = out_reg_i[rd_idx];

  // The instruction read is COMBINATIONAL on pc, because the core presents
  // pc_o and consumes ins_*_i in the same cycle.
  wire [IAW-1:0] pc_ix = IAW'(core_pc);

  zhao_field_v2_core #(.LANES(LANES), .WFS(WFS), .REGS(REGS)) u_core (
      .clk(clk), .rst_n(rst_n),
      .h_we_i(h_we), .h_wf_i(h_wf), .h_lane_i(h_lane), .h_reg_i(h_reg),
      .h_wdata_i(h_wdata),
      .h_rwf_i(h_rwf), .h_rlane_i(h_rlane), .h_rreg_i(h_rreg), .h_rdata_o(h_rdata),
      .start_i(core_start), .busy_o(core_busy), .done_o(core_done),
      .instr_count_i(instr_count_i),
      .pc_o(core_pc),
      .ins_op_i(im_op[pc_ix]), .ins_dst_i(im_dst[pc_ix]), .ins_a_i(im_a[pc_ix]),
      .ins_b_i(im_b[pc_ix]), .ins_c_i(im_c[pc_ix]), .ins_imm_i(im_imm[pc_ix]),
      .tbl_idx_o(tbl_idx_o), .tbl_n_i(tbl_n_i),
      .tbl_x_i(tbl_x_i), .tbl_y_i(tbl_y_i), .tbl_dy_i(tbl_dy_i),
      .status_o(status_o),
      .sat_add_o(sat_add_o), .sat_mul_o(sat_mul_o), .sat_rescale_o(sat_rescale_o),
      .sat_rcp_o(sat_rcp_o), .rcp_zero_o(rcp_zero_o),
      .instr_retired_o(instr_retired_o)
  );

  // ---- the batch ----------------------------------------------------------
  // F_FILL   accept points, writing each into its slot's registers
  // F_RUN    start every wavefront that holds a point, wait for all of them
  // F_DRAIN  read the output registers back, one point at a time, in order
  typedef enum logic [1:0] {F_FILL, F_RUN, F_DRAIN} state_t;
  state_t state;

  logic [SW-1:0]  filled;      // points accepted into this batch
  logic [SW-1:0]  drained;     // points emitted from it
  logic [$clog2(NIN+1)-1:0]  fill_lane;   // which input lane is being written
  logic [$clog2(NOUT+1)-1:0] drain_lane;  // which output lane is being read
  logic [ONW-1:0] rd_idx;      // which output lane the address bus is showing
  logic [1:0]     rd_phase;    // present -> settle -> capture

  // slot -> (wavefront, lane). Wavefront-major, so a wavefront's LANES points
  // are contiguous and a partial batch fills whole wavefronts first.
  wire [WFW-1:0] fill_wf   = WFW'(filled  / SW'(LANES));
  wire [LW-1:0]  fill_ln   = LW'(filled   % SW'(LANES));
  wire [WFW-1:0] drain_wf  = WFW'(drained / SW'(LANES));
  wire [LW-1:0]  drain_ln  = LW'(drained  % SW'(LANES));

  // How many wavefronts hold at least one point.
  wire [WFW:0] wf_used = (WFW+1)'((filled + SW'(LANES) - SW'(1)) / SW'(LANES));

  // READY MEANS CONSUMED, AND A POINT TAKES n_in CYCLES TO CONSUME.
  //
  // This first asserted on the FIRST lane-write cycle, which reads as "point
  // taken" to any correct producer -- so it advanced, and lanes 1..n_in-1 were
  // then written from the NEXT point's data. Every section failed, and the
  // failure looked like reordering rather than like a handshake bug.
  //
  // Ready therefore lands on the LAST lane cycle: the producer holds valid and
  // stable data across the whole write, which is exactly what ready/valid
  // already requires of it.
  assign pt_ready_o = (state == F_FILL) && (filled != SW'(SLOTS)) &&
                      (fill_lane + 1 == n_in_i);

  logic signed [31:0] res_hold [NOUT];
  always_comb begin
    for (int k = 0; k < NOUT; k++) res_lane_o[k] = res_hold[k];
  end
  assign res_valid_o = (state == F_DRAIN) && (drain_lane == n_out_i);

  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state       <= F_FILL;
      started     <= '0;
      launched    <= 1'b0;
      filled      <= '0;
      drained     <= '0;
      fill_lane   <= '0;
      drain_lane  <= '0;
      rd_idx      <= '0;
      rd_phase    <= 2'd0;
      core_start  <= '0;
      h_we        <= 1'b0;
      h_wf        <= '0;
      h_lane      <= '0;
      h_reg       <= '0;
      h_wdata     <= '0;
      for (i = 0; i < NOUT; i++) res_hold[i] <= '0;
    end else begin
      core_start <= '0;
      h_we       <= 1'b0;

      unique case (state)
        // ---- F_FILL --------------------------------------------------------
        // One register write per cycle: the core has ONE host write port, and
        // a point has up to NIN lanes, so a point costs n_in cycles. That is
        // the honest cost of this interface and it is measured, not hidden.
        F_FILL: begin
          // The write runs while the producer holds the point: it begins on
          // the cycle valid is seen and continues until the last lane, which is
          // the cycle ready fires.
          if (pt_valid_i && filled != SW'(SLOTS)) begin
            h_we    <= 1'b1;
            h_wf    <= fill_wf;
            h_lane  <= fill_ln;
            h_reg   <= RW'(fill_lane);
            h_wdata <= pt_lane_i[INW'(fill_lane)];
            if (fill_lane + 1 == n_in_i) begin
              fill_lane <= '0;
              filled    <= filled + SW'(1);
            end else begin
              fill_lane <= fill_lane + 1;
            end
          end else if (filled == SW'(SLOTS) ||
                       (filled != '0 && !pt_valid_i)) begin
            // Full, or the producer has nothing more for now: run what we have.
            // A PARTIAL BATCH IS CORRECT AND MUST STAY SO -- a patch is not a
            // multiple of 32 points, and a driver that waited for a full batch
            // would deadlock on the last few points of every patch.
            state      <= F_RUN;
            core_start <= WFS'((1 << wf_used) - 1);
            started    <= WFS'((1 << wf_used) - 1);
            launched   <= 1'b0;
          end
        end

        // ---- F_RUN ---------------------------------------------------------
        // ---- WAIT FOR THE START TO LAND, THEN FOR THE WORK TO FINISH -----
        // `done_o` is `finished`, and it is STICKY: it still holds the previous
        // batch's bits when this one is launched. `busy_o` is `active`, which
        // is also low before a wavefront starts. So immediately after the start
        // pulse -- which is REGISTERED, and therefore has not reached the core
        // yet -- both "idle" and "done" are already true, and a batch would
        // complete without executing anything.
        //
        // That is exactly what happened: the second batch drained the FIRST
        // batch's registers, so its first point returned the previous point's
        // answer. The single-cycle F_RUN is visible in a trace as state 1 for
        // one cycle and then state 2.
        //
        // `start_i` CLEARS `finished`, so the start landing is observable:
        // wait for the started wavefronts' done bits to go LOW (the pulse
        // arrived), and only then for them to come back HIGH (the work is
        // done). No delay constant, and it cannot be fooled by staleness.
        F_RUN: if (!launched) begin
          if ((core_done & started) == '0) launched <= 1'b1;
        end else if (core_busy == '0 && (core_done & started) == started) begin
          state      <= F_DRAIN;
          drained    <= '0;
          drain_lane <= '0;
          rd_idx     <= '0;
          rd_phase   <= 2'd0;
        end

        // ---- F_DRAIN -------------------------------------------------------
        // Reading one output lane is an EXPLICIT three-phase walk: present the
        // address, let it settle, capture. That is one cycle more than the
        // register file's latency strictly requires, and it is deliberate --
        // the version that tried to be exact was off by one, and the symptom
        // was answers shifted BETWEEN OUTPUT LANES, which reads like a mapping
        // bug rather than a timing one and costs far more than a cycle.
        F_DRAIN: begin
          if (drain_lane != n_out_i) begin
            unique case (rd_phase)
              2'd0:    begin rd_idx <= ONW'(drain_lane); rd_phase <= 2'd1; end
              2'd1:    rd_phase <= 2'd2;
              default: begin
                res_hold[rd_idx] <= h_rdata;
                drain_lane <= drain_lane + 1;
                rd_phase   <= 2'd0;
              end
            endcase
          end else if (res_ready_i) begin
            drain_lane <= '0;
            rd_idx     <= '0;
            rd_phase   <= 2'd0;
            if (drained + SW'(1) == filled) begin
              state   <= F_FILL;
              filled  <= '0;
              drained <= '0;
            end else begin
              drained <= drained + SW'(1);
            end
          end
        end

        default: state <= F_FILL;
      endcase
    end
  end

endmodule : zhao_field_v2_front
