// tb_field_stamp_binding_unbound_mutant.sv -- the POSITIVE CONTROL for
// `zhao_field_stamp_adapter`'s FH26 elaboration guard.
//
// ---------------------------------------------------------------------------
// WHAT IS MUTATED, AND WHY IT HAS TO BE A COMMITTED FILE
// ---------------------------------------------------------------------------
// THE MUTATION IS AN OMISSION. This wrapper instantiates the production
// adapter and **does not pass `STAMP_BINDING`**, which is exactly the mistake
// owner directive FH26 exists to prevent: a composer that never states which
// contract it is speaking and silently gets the permissive legacy bridge.
//
// In the repaired design that omission is a hard elaboration failure. That
// state is **unreachable with legal stimulus** -- no input sequence can make a
// correctly-parameterised instance refuse -- so "the guard can fire" would
// otherwise stay an argument forever. CLAUDE.md:
//
//   > A guard you cannot reach with legal stimulus needs a COMMITTED MUTANT
//   > ... the break must not be a temporary edit to production RTL, because
//   > that is a live-tree hazard AND it leaves nothing behind: the next person
//   > inherits the same argument and no evidence.
//
// ---------------------------------------------------------------------------
// THIS IS A WRAPPER, NOT A COPY, AND THAT IS DELIBERATE
// ---------------------------------------------------------------------------
// It instantiates `zhao_field_stamp_adapter` by name rather than duplicating
// it. A copy of production RTL goes stale in the flattering direction -- the
// thirteen combiner copies and the eight AUX-pipe copies are the record -- and
// `tools/budget/mutant_copy_drift.py` deliberately does not count a wrapper as
// a copy for exactly this reason. **There is nothing here to refresh when the
// adapter changes shape**, except the port list, which the compiler checks.
//
// ---------------------------------------------------------------------------
// AND THE REASON A CLEAN LINT IS NOT THE EVIDENCE
// ---------------------------------------------------------------------------
// `verilator --lint-only` DOES NOT RUN `initial` BLOCKS. Linting this file
// returns success and says nothing whatever about the guard -- measured, on
// this exact wrapper, 2026-09-20: the only diagnostics were unused-signal
// warnings about the probe's own tie-offs. So the control is a RUN, not a
// lint: `field_stamp_binding_unbound_control.cpp` elaborates this top and the
// `$fatal` fires at time zero.
//
// THE DRIVER'S POLARITY IS INVERTED. It PASSES when the guard FIRES. It is
// evidence about the instrument, not about the design.

`default_nettype none

module tb_field_stamp_binding_unbound_mutant (
    input  var logic clk,
    input  var logic rst_n
);

  // Every output is tied off into a probe; nothing here is meant to run. The
  // elaboration check fires before a single cycle is simulated.
  /* verilator lint_off UNUSEDSIGNAL */
  logic          p_arm_ready;
  logic          p_fld_valid;
  logic [31:0]   p_fld_tag_op;
  logic [15:0]   p_fld_strength;
  logic [15:0]   p_fld_emissive;
  logic          p_req_valid;
  logic [2:0]    p_req_slot;
  logic          p_req_noprog;
  logic [415:0]  p_req_in;
  logic          p_resp_ready;
  logic [31:0]   p_stamps;
  logic [31:0]   p_texels;
  logic [31:0]   p_faults;
  logic [31:0]   p_restarts;
  logic [31:0]   p_canon_clamps;
  logic          p_busy;
  /* verilator lint_on UNUSEDSIGNAL */

  // THE MUTATION: no `#(.STAMP_BINDING(...))`. IN_LANES/OUT_LANES are left at
  // their defaults too, which are the composed 13/7 and are NOT the thing
  // under test here.
  zhao_field_stamp_adapter u_unbound (
      .clk  (clk),
      .rst_n(rst_n),

      .cmd_fire_i    (1'b0),
      .cmd_field_en_i(1'b0),

      .slot_i      (3'd0),
      .slot_valid_i(1'b0),
      .arm_ready_o (p_arm_ready),

      .fld_valid_o   (p_fld_valid),
      .fld_ready_i   (1'b0),
      .fld_tag_op_o  (p_fld_tag_op),
      .fld_strength_o(p_fld_strength),
      .fld_emissive_o(p_fld_emissive),

      .req_valid_o  (p_req_valid),
      .req_ready_i  (1'b0),
      .req_slot_o   (p_req_slot),
      .req_noprog_o (p_req_noprog),
      .req_in_o     (p_req_in),
      .resp_valid_i (1'b0),
      .resp_ready_o (p_resp_ready),
      .resp_out_i   (224'd0),
      .resp_status_i(8'd0),

      .stamps_o      (p_stamps),
      .texels_o      (p_texels),
      .faults_o      (p_faults),
      .restarts_o    (p_restarts),
      .canon_clamps_o(p_canon_clamps),
      .busy_o        (p_busy)
  );

endmodule

`default_nettype wire
