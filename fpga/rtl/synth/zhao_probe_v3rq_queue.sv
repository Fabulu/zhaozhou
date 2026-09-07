// zhao_probe_v3rq_queue.sv — CHARACTERIZATION WRAPPER, not a console block.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// V3.1 control-fabric recovery architecture §5.7, "Local queue fit gate", asks
// for this by name:
//
//   "Before rebuilding the whole owner demo, fit ONE production-shaped ready
//    queue with the actual depth, width, head structure, and status outputs.
//    Use a wrapper whose outputs are observable and whose boundary is
//    registered consistently. Check empty/full transitions, read-in-flight
//    accounting, sustained pop rate, and setup/hold at the intended product
//    constraint."
//
// PRODUCTION-SHAPED means the shape `zhao_texture_v3own` actually instantiates,
// not a convenient one: WIDTH = OWNERW = SLOTW + GENW = 6 + 8 = 14, DEPTH = 64,
// and CAPACITY = 64. The third of those is the point. §5.3 warns that "a body
// of 64 plus two heads must not silently advertise 66 logical owner credits",
// so a probe that left CAPACITY at its default would be measuring the wrong
// contract while looking correct.
//
// WHY A WRAPPER AND NOT THE RAW LEAF. The 2026-08-23 budget audit asked for
// "registered characterisation wrappers -- registered stimulus -> DUT ->
// registered hash sink", because "raw leaf blocks with hundreds of virtual pins
// are poor physical models". A raw v3rq fit would hang `occ_o`, `full_o`,
// `valid_o` and `owned_empty_o` straight off pads -- and those four ARE the
// status contract under investigation, so pin-terminated paths would corrupt
// exactly the measurement §5.7 wants.
//
// §5.7 IS EXPLICIT ABOUT ITS OWN LIMIT and that limit is repeated here so no
// later reader promotes this row: "This experiment is diagnostic. It does not
// replace the owner-composed fit, because the queue-to-admission and
// queue-to-drain connections are part of the problem under investigation."
//
// NOT INSTANTIATED BY THE CONSOLE. Absent from the shell QSF, from
// ZHAO_SHELL_RTL and from the production manifest, so it cannot affect the
// shell fit or source-list parity.
`default_nettype none

module zhao_probe_v3rq_queue (
    input  var logic        clk,
    input  var logic        rst_n,
    input  var logic        stim_valid_i,
    input  var logic [31:0] stim_i,
    output var logic [31:0] hash_o
);

  // The production shape, from zhao_texture_v3own's own parameters.
  localparam int unsigned SLOTW  = 6;
  localparam int unsigned GENW   = 8;
  localparam int unsigned OWNERW = SLOTW + GENW;   // 14
  localparam int unsigned QDEPTH = 64;
  localparam int unsigned PW     = $clog2(QDEPTH);

  // ---- registered stimulus -------------------------------------------------
  // The boundary is registered on BOTH sides, so no measured path begins or
  // ends at a pad.
  logic        stim_valid_q;
  logic [31:0] stim_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      stim_valid_q <= 1'b0;
      stim_q       <= 32'd0;
    end else begin
      stim_valid_q <= stim_valid_i;
      stim_q       <= stim_i;
    end
  end

  // ---- drive ---------------------------------------------------------------
  // SUSTAINED POP RATE is one of the four things §5.7 asks to check, so pop is
  // driven from a stimulus bit rather than tied high: a queue popped every
  // cycle never exercises the head-holding logic, and one never popped never
  // exercises the read-in-flight path. Both are the accounting under test.
  logic              q_wr_en_c;
  logic [OWNERW-1:0] q_wr_data_c;
  logic              q_pop_c;

  // GATED ON full_o, because the DUT requires it and a fixture that violates
  // the contract is not measuring the design. `zhao_texture_v3rq` asserts
  //
  //     a_rq_no_write_when_full : assert (!(wr_en_i && full_o));
  //
  // and the first version of this probe drove writes regardless. A directed
  // sanity run tripped that assertion immediately. It would NOT have shown up
  // in the fit -- Quartus drops assertions -- so S5.7's gate would have
  // characterised a queue driven with illegal stimulus and reported a perfectly
  // clean number. `full_o` comes from a register (`lcnt_q >= CAPACITY`), so
  // this is a real producer's gate, not a combinational loop.
  assign q_wr_en_c   = stim_valid_q && stim_q[31] && !q_full;
  // Both halves of the stimulus feed the write data. Leaving stim_q[29:16]
  // unused was the wrapper's only lint warning, and SUPPRESSING it would have
  // been the wrong fix: unused stimulus bits give the fitter freedom to fold
  // the datapath, which makes the row report a queue cheaper than the one that
  // exists. Widening the dependency removes the warning by removing its cause.
  assign q_wr_data_c = stim_q[OWNERW-1:0] ^ stim_q[29:16];
  // The last two stimulus bits modulate the pop DUTY rather than sitting idle,
  // which is what §5.7's "sustained pop rate" asks the probe to vary: a queue
  // popped every cycle never holds a head, and one never popped never exercises
  // the read-in-flight path. Both are the accounting under test.
  assign q_pop_c     = stim_q[30] && (stim_q[15:14] != 2'b00);

  // ---- DUT: one production-shaped ready queue ------------------------------
  logic              q_full;
  logic              q_valid;
  logic [OWNERW-1:0] q_data;
  logic [PW:0]       q_occ;
  logic              q_owned_empty;

  zhao_texture_v3rq #(
      .WIDTH   (OWNERW),
      .DEPTH   (QDEPTH),
      .CAPACITY(QDEPTH)
  ) u_dut (
      .clk          (clk),
      .rst_n        (rst_n),
      .wr_en_i      (q_wr_en_c),
      .wr_data_i    (q_wr_data_c),
      .full_o       (q_full),
      .valid_o      (q_valid),
      .data_o       (q_data),
      .pop_i        (q_pop_c),
      .occ_o        (q_occ),
      .owned_empty_o(q_owned_empty)
  );

  // ---- registered hash sink ------------------------------------------------
  // EVERY status output is folded in. If any were dropped the fitter would be
  // free to delete the logic that produces it, and the row would then report a
  // queue cheaper and faster than the one that exists -- the exact direction
  // CLAUDE.md says a broken instrument always errs in.
  //
  // `full_o` and `owned_empty_o` are the empty/full transition evidence; `occ_o`
  // is the read-in-flight accounting (§5.1: total tickets held, body PLUS head
  // registers, which is a different question from `valid_o`).
  logic [31:0] hash_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      hash_q <= 32'd0;
    end else begin
      hash_q <= {hash_q[30:0], hash_q[31]}
                ^ {18'd0, q_data}
                ^ {24'd0, 1'b0, q_occ}
                ^ {28'd0, q_full, q_valid, q_owned_empty, q_pop_c};
    end
  end

  assign hash_o = hash_q;

endmodule

`default_nettype wire
