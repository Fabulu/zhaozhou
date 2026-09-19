// zhao_field_stamp_adapter.sv — the S profile's STREAM ADAPTER: the stencil
// walk that turns one SurfaceStamp into 4,096 field records.
//
// Contract: design/contracts/FIELD.SEQ.STAMP.md, which is a CONFIGURATION of
//           FIELD.SEQ.CORE and not a second engine.
// Consumer: `fpga/rtl/surface/zhao_surface_stamp.sv`, port `fld_*` (its S2).
//
// ---------------------------------------------------------------------------
// WHAT AN ADAPTER IS ALLOWED TO BE
// ---------------------------------------------------------------------------
// `design/contracts/FIELD.SEQ.CORE.md` permits exactly this file and describes
// it in advance:
//
//   > **Profile adapters are PERMITTED and are the v3 front end.** A profile is
//   > a program set plus a STREAM ADAPTER -- a small generator that produces the
//   > varying input lanes directly (Earth: lattice x,z from patch origin +
//   > pitch; ... Stamp: stencil u,v) and consumes the outputs directly from
//   > snooped export registers. Adapters generate and consume streams; they
//   > NEVER re-implement an op, and there are not five engines.
//
// So there is no arithmetic here, no op, no coverage test and no blend. There
// is a counter, a handshake, and the lane binding.
//
// ---------------------------------------------------------------------------
// THE LANE BINDING IS ASSEMBLED FROM TWO RATIFIED HALVES, NOT INVENTED
// ---------------------------------------------------------------------------
// FIELD.SEQ.STAMP.md leaves the S binding open and says where it belongs: "a
// software and shell question, and it belongs with the blocks that consume the
// output". Both halves already exist and neither is this file's opinion:
//
//   INPUT  -- FIELD.SEQ.CORE.md names the S profile's varying lanes as
//             "Stamp: stencil u,v". They go to R0 and R1, which is record
//             order, which is what every other profile's binding uses.
//   OUTPUT -- spec/form/field-ir.md 7.1's stamp record is {tag_op, strength},
//             and `zhao_surface_stamp` ALREADY unpacks it, byte for byte:
//             tag = tag_op[7:0], blend = [10:8], age_shift = [14:12], and
//             `strength` carries the ABI's u16 so that one `>> 8` serves both
//             paths. So the output vocabulary is fixed by the consumer that
//             was written first.
//
// The program's own header says which register the outputs start at, so even
// the destination is program metadata rather than a constant here.
//
// ---------------------------------------------------------------------------
// THE ORDER IS THE CONSUMER'S, AND THAT IS THE WHOLE ALIGNMENT ARGUMENT
// ---------------------------------------------------------------------------
// `zhao_surface_stamp`'s S2 is explicit, and it chose its shape precisely so
// that this file could exist without knowing anything about circles:
//
//   > S2. THE FIELD BRUSH DELIVERS ONE RESULT PER VISITED TEXEL -- all 4,096,
//   > in the same j-outer/i-inner scan order -- not one per COVERED texel. A
//   > result whose texel is not covered is consumed and DISCARDED.
//   > REJECTED ALTERNATIVE: results only for covered texels, which makes the
//   > consumption rate depend on a coverage test that lives in THIS block, so
//   > FIELD.SEQ.STAMP would have to reproduce this geometry bit-for-bit or the
//   > pair deadlocks.
//
// So this adapter walks a plain raster and never computes coverage. The two
// cursors are kept in step by TWO things, and it needs both:
//
//   1. A SHARED START. `cmd_fire_i` is the cycle the stamp's own command port
//      accepts a command -- the same edge the consumer starts on. Free-running
//      and trusting "4,096 per stamp" would misalign forever the first time a
//      stamp ABORTS, and S4 says an ACQUIRE that overflows aborts the whole
//      stamp before any write, consuming ZERO records.
//   2. THE HANDSHAKE. One record leaves per `fld_valid_o && fld_ready_i`, so
//      the consumer's cursor and this counter advance on the same cycle by
//      construction rather than by agreement.
//
// A second command arriving while a walk is still running is the fault this
// pair can actually have, and `restarts_o` counts it rather than leaving it to
// be inferred from a wrong sheet. The walk restarts, because the consumer has
// restarted: following it is the only alignment that can be right.
//
// ---------------------------------------------------------------------------
// WHAT HAPPENS WHEN THERE IS NO PROGRAM, AND WHY IT IS NOT DECIDED HERE
// ---------------------------------------------------------------------------
// `arm_ready_o` is high only when a stamp program is resident. The CONSOLE ANDs
// it into the stamp's `cmd_field_en_i`, so a stamp that asks for the brush with
// no program loaded runs as a plain ABI stamp instead of stalling forever on
// records that cannot come. That is a policy decision and it is taken in the
// composer, beside the other console policy inputs, rather than buried here.
//
// A run that FAULTS mid-walk is different and is handled here, because by then
// the consumer is already committed to 4,096 records: the record is delivered
// with the engine's zeroed lanes and `faults_o` counts it. A stalled walk would
// hang the stamp, and `zhao_field_seq`'s own header is right that a hang is the
// worse failure and the one nobody can debug from a frame capture.
//
// ENFORCED-BY: tests/field/field_stamp_adapter_directed.cpp:main

`default_nettype none

module zhao_field_stamp_adapter #(
    // The sheet is 64 x 64 (spec/terrain_rules.md 7, layer F, 4,096 texels).
    // It is a parameter so the directed test can walk a small sheet and still
    // reach the end-of-walk and restart cases without 4,096 engine runs.
    parameter int unsigned SHEET_W = 64,
    parameter int unsigned SHEET_H = 64,
    parameter int unsigned SLOTW = 3,
    parameter int unsigned IN_LANES = 12,
    parameter int unsigned OUT_LANES = 4
) (
    input  logic clk,
    input  logic rst_n,

    // ---- the consumer's start edge ------------------------------------------
    input  logic                  cmd_fire_i,      // the stamp accepted a command
    input  logic                  cmd_field_en_i,  // ... and it wants the brush

    // ---- console policy: which resident program is the brush ---------------
    input  logic [SLOTW-1:0]      slot_i,
    input  logic                  slot_valid_i,
    output logic                  arm_ready_o,

    // ---- stamp_field_results, into zhao_surface_stamp's fld_* --------------
    output logic                  fld_valid_o,
    input  logic                  fld_ready_i,
    output logic [31:0]           fld_tag_op_o,
    output logic [15:0]           fld_strength_o,

    // ---- the shared engine --------------------------------------------------
    output logic                     req_valid_o,
    input  logic                     req_ready_i,
    output logic [SLOTW-1:0]         req_slot_o,
    output logic                     req_noprog_o,
    output logic [IN_LANES*32-1:0]   req_in_o,
    input  logic                     resp_valid_i,
    output logic                     resp_ready_o,
    // ONLY 48 OF THESE BITS ARE PART OF THE S RECORD, and that is the ABI
    // rather than an oversight. The shared response bus is four lanes wide
    // because the E record is (height, velocity, material, nav_cost); the STAMP
    // record of spec/form/field-ir.md 7.1 is {tag_op u32, strength u16}, so
    // lane 1's high half and lanes 2 and 3 are not fields of it. Reading them
    // would be this block inventing two outputs the profile does not declare.
    // The engine zeroes the register file before every run, so an undeclared
    // lane is zero rather than the previous run's residue.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic [OUT_LANES*32-1:0]  resp_out_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic [7:0]               resp_status_i,

    // ---- counters ------------------------------------------------------------
    output logic [31:0] stamps_o,    // walks started
    output logic [31:0] texels_o,    // records delivered
    output logic [31:0] faults_o,    // runs that came back with a status
    output logic [31:0] restarts_o,  // a command arrived mid-walk
    output logic        busy_o
);

  localparam int unsigned TEXELS = SHEET_W * SHEET_H;
  localparam int unsigned IW = (SHEET_W > 1) ? $clog2(SHEET_W) : 1;
  localparam int unsigned JW = (SHEET_H > 1) ? $clog2(SHEET_H) : 1;
  localparam int unsigned TW = (TEXELS > 1) ? $clog2(TEXELS) : 1;

  initial begin
    if (IN_LANES < 2) begin
      $fatal(1, "zhao_field_stamp_adapter: the S binding needs at least u and v");
    end
    if (OUT_LANES < 2) begin
      $fatal(1, "zhao_field_stamp_adapter: the stamp record is {tag_op, strength}");
    end
    if (SLOTW < 1) $fatal(1, "zhao_field_stamp_adapter: SLOTW must be at least 1");
  end

  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_REQ  = 3'd1;  // offering the point to the engine
  localparam logic [2:0] S_WAIT = 3'd2;  // waiting for its answer
  localparam logic [2:0] S_OUT  = 3'd3;  // offering the record to the stamp
  // S_DROP is the restart's landing state and it is not a formality. A restart
  // taken straight back to S_REQ would do two illegal things at once:
  //
  //   * ABANDON AN IN-FLIGHT ANSWER. The engine parks in its response state
  //     until the claimant takes the result. Walking away from one leaves it
  //     holding a response nobody will ever read, and it accepts no further
  //     request -- a deadlock of the whole shared engine, caused by one stamp
  //     being aborted, with no counter anywhere that would have moved.
  //   * CHANGE AN OFFER THAT IS STILL STANDING. `req_valid_o` is high in S_REQ,
  //     so resetting the texel counter under it would move the operands of a
  //     request the engine has not yet accepted.
  //
  // So a restart always spends at least one cycle here, with the offer dropped
  // and the response port draining.
  localparam logic [2:0] S_DROP = 3'd4;

  logic [2:0]      state;
  logic            outstanding;  // a request was accepted; its answer is owed
  logic [TW:0]     tex;          // one bit wider than TEXELS so the end is a compare
  logic [31:0]     rec_tag_op;
  logic [15:0]     rec_strength;

  // j-outer, i-inner: the consumer's order, restated as arithmetic rather than
  // as an assumption. `i` is the fast axis.
  wire [IW-1:0] cur_i = tex[IW-1:0];
  wire [JW-1:0] cur_j = tex[(IW+JW)-1:IW];

  assign arm_ready_o = slot_valid_i;
  assign busy_o      = (state != S_IDLE);

  // A walk starts when the CONSUMER starts one. `cmd_field_en_i` is the
  // console's already-gated policy bit, so a stamp that did not ask for the
  // brush produces no records at all and the consumer consumes none.
  wire start_c = cmd_fire_i && cmd_field_en_i;

  always_comb begin
    req_in_o = '0;
    // R0 = u, R1 = v. Every other declared lane is zero, which is bit-exact:
    // the engine's first act is the law's `reg[0..63] = 0`, so writing a zero
    // into a lane the program never declared leaves the file where the law
    // wants it.
    req_in_o[(0*32) +: 32] = {{(32 - IW){1'b0}}, cur_i};
    req_in_o[(1*32) +: 32] = {{(32 - JW){1'b0}}, cur_j};
  end

  assign req_valid_o  = (state == S_REQ);
  assign req_slot_o   = slot_i;
  // The engine is told rather than guessed at: if the console has no resident
  // brush program the request is refused at source and comes back with a
  // status, instead of this block inventing a texel record.
  assign req_noprog_o = !slot_valid_i;
  assign resp_ready_o = (state == S_WAIT) || (state == S_DROP);

  assign fld_valid_o    = (state == S_OUT);
  assign fld_tag_op_o   = rec_tag_op;
  assign fld_strength_o = rec_strength;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state        <= S_IDLE;
      outstanding  <= 1'b0;
      tex          <= '0;
      rec_tag_op   <= 32'd0;
      rec_strength <= 16'd0;
      stamps_o     <= 32'd0;
      texels_o     <= 32'd0;
      faults_o     <= 32'd0;
      restarts_o   <= 32'd0;
    end else begin
      if (req_valid_o && req_ready_i) outstanding <= 1'b1;
      else if (resp_valid_i && resp_ready_o) outstanding <= 1'b0;

      if (start_c) begin
        // Follow the consumer, always. It has restarted its own cursor, so a
        // walk that kept its place would be delivering texel 900's record for
        // texel 0 -- well formed, plausible, and wrong everywhere.
        if ((state != S_IDLE) && (restarts_o != 32'hFFFF_FFFF)) begin
          restarts_o <= restarts_o + 32'd1;
        end
        if (stamps_o != 32'hFFFF_FFFF) stamps_o <= stamps_o + 32'd1;
        tex   <= '0;
        state <= S_DROP;
      end else begin
        case (state)
          S_IDLE: begin
            // nothing to do
          end

          // Leave only when nothing is owed. `outstanding` is cleared by the
          // drain in the same cycle the answer is taken, so this costs one
          // cycle when there was nothing in flight and two when there was.
          S_DROP: begin
            if (!outstanding || (resp_valid_i && resp_ready_o)) state <= S_REQ;
          end

          S_REQ: begin
            if (req_ready_i) state <= S_WAIT;
          end

          S_WAIT: begin
            if (resp_valid_i) begin
              rec_tag_op   <= resp_out_i[(0*32) +: 32];
              rec_strength <= resp_out_i[(1*32) +: 16];
              if (resp_status_i != 8'd0) begin
                // Delivered anyway, and counted. By now the consumer is
                // committed to exactly TEXELS records and withholding one
                // hangs the stamp.
                rec_tag_op   <= 32'd0;
                rec_strength <= 16'd0;
                if (faults_o != 32'hFFFF_FFFF) faults_o <= faults_o + 32'd1;
              end
              state <= S_OUT;
            end
          end

          S_OUT: begin
            if (fld_ready_i) begin
              if (texels_o != 32'hFFFF_FFFF) texels_o <= texels_o + 32'd1;
              if (tex == (TW+1)'(TEXELS - 1)) begin
                tex   <= '0;
                state <= S_IDLE;
              end else begin
                tex   <= tex + 1'b1;
                state <= S_REQ;
              end
            end
          end

          default: state <= S_IDLE;
        endcase
      end
    end
  end

endmodule : zhao_field_stamp_adapter

`default_nettype wire
