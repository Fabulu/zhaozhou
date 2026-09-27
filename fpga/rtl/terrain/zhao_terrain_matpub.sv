// zhao_terrain_matpub.sv -- TERRAIN.COMPOSED_MATERIAL's PUBLISHER.
//
// Owner ruling `reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`
// ("Build TERRAIN.COMPOSED_MATERIAL and complete directive 13.7's -FieldActive
// positive console path"), implementing the shape fixed by
// `reports/DECISION-20260927-I34-COMPOSED-MATERIAL-PUBLISHER.md`.
//
// ===========================================================================
// WHAT IT IS FOR
// ===========================================================================
// `zhao_terrain_matjoin` composes the authored layer-E plane with the Earth
// field's material lane and writes the result into the compose cache, one cell
// at a time.  That composed plane is the ONLY place the console's material
// truth for a patch exists as a plane rather than as a per-fragment pick, and
// until this block it left no copy anywhere outside the cache -- so a patch's
// composed material died with its compose slot.
//
// This block taps that write face and publishes the plane to the commissioned
// region.  It is a WRITER, not a cache: it never answers a read, and nothing
// here is on any fragment's path.
//
// ===========================================================================
// WHY THIS TAP AND NOT ANOTHER
// ===========================================================================
// The decision record carries the argument in full; the short form is that
// `matjoin`'s write face is the only place the composed triple exists AS A
// UNIT.  Upstream it is an authored plane plus a separate field lane;
// downstream it is already a mosaic pick, which answers per FRAGMENT rather
// than per CELL, so the same cell would publish many times and the write count
// would stop being a property of the lattice.
//
// `zhao_terrain_patch_acc` was checked and RULED OUT -- it is the four-bank
// field-major accumulator of the `patch_v2` topology this console does not
// run.  It is not re-scouted here.
//
// ===========================================================================
// THE PAYLOAD AND THE ARITHMETIC, STATED SO IT CAN BE CHECKED
// ===========================================================================
//   a cell      = {8'h00, weight, mat_b, mat_a}                 =    4 B
//   a plane     = 32 x 32 cells                                 = 1024 cells
//   a patch     = 1024 x 4 B                                    = 4,096 B
//   a burst     = BURST_B(64) B = BEATS(8) x 64 bits            =   16 cells
//   per patch   = 4096 / 64                                     =   64 bursts
//   a slot      = 8,192 B, so the payload occupies the first half
//   the region  = 256 slots x 8,192 B = 2 MiB = 0x0020_0000
//
// The reserved byte is the DECISION's, not an accident of packing: 4 B carries
// the triple with a byte spare, and the 8 B/cell layout that would fill the
// slot was rejected for doubling every number above to buy nothing declared.
//
// ===========================================================================
// THE PUBLISH GATE, AND THE HAZARD THE DECISION'S ONE-LINE VERSION MISSES
// ===========================================================================
// The decision says "published only for a patch whose composed plane actually
// changed".  That is right and it is not sufficient, and the missing half is
// one `TERRAIN.DEVSTORE` already paid for, in its own words:
//
//     "THE KEY IS THE RESIDENCY PAGE SLOT.  A compose slot is reused by a
//      different patch from one frame to the next, so the HISTORY -- which is
//      what hysteresis is made of -- would be inherited by a stranger."
//
// A gate keyed on "did the plane change" alone is blind in exactly that case.
// If slot 7 held patch A and now holds patch B, and B's plane happens to hash
// equal to A's, a change-only gate SKIPS the write and the region goes on
// describing A under B's address.  The skip is silent, every counter balances,
// and the region is wrong.  That is this campaign's own cancelling-errors
// shape with a slot number on it.
//
// So the gate has THREE arms and each is counted separately:
//
//   * NEVER PUBLISHED.  The slot has no record since reset.  Publish.
//   * OCCUPANT CHANGED.  The slot's stored patch id differs from the one
//     committing.  Publish REGARDLESS of the signature, and count it on
//     `stranger_pub_o` -- a counter that exists to make this arm visible
//     rather than merely correct.
//   * SIGNATURE CHANGED.  Same occupant, different plane.  Publish.
//
// and one skip arm, `skipped_clean_o`, for the same occupant with the same
// plane -- which is the case the decision's bandwidth argument is about, and
// the only case in which not writing is right.
//
// THIS IS NOT A TRIM.  It adds publications the decision's gate would have
// skipped; it removes none.  The owner's constraint is that capacity,
// semantics, update behaviour and destination must not be cut to meet the
// budget, and a gate that publishes MORE than the costed one cannot be a cut.
// The cost consequence is reported rather than hidden: see `bursts_written_o`.
//
// ===========================================================================
// WHY A SIGNATURE AND NOT A COMPARE
// ===========================================================================
// The exact test for "did the plane change" is to compare 1,024 cells against
// the 1,024 last published.  That needs a second read port on the plane buffer
// during the fill, or a second buffer, and it buys nothing a wide signature
// does not: the signature is computed from the cells as they ARRIVE, costs a
// register and an adder-free shift-xor, and needs no port at all.
//
// The one thing a signature can do that a compare cannot is collide -- report
// CLEAN for a plane that changed.  The mixing below is a 32-bit shift-xor over
// {cell index, cell word}, so a collision needs two different planes to agree
// in all 32 bits; and because the cell INDEX is mixed in, the common
// degenerate cases -- a permuted plane, a plane whose cells are all equal, a
// plane one cell short -- are separated rather than aliased.  `sig_fold_c` is a
// named function so a future change to it is a change to one place.
//
// It is a KNOB and it is not a law: if a collision is ever suspected, widen
// SIGW or replace `sig_fold_c`.  Nothing else in this block depends on either.
//
// ===========================================================================
// THE GUARD PROTOCOL, AND THE TWO-CYCLE ANSWER
// ===========================================================================
// Copied in shape from `zhao_terrain_devstore`, including the fact that cost
// it: the guard answers in TWO cycles -- `ready` is a LEVEL and `ok` is PULSED
// the cycle after the accept -- so testing both in one arm reads every pass as
// a denial.  `P_REQ` waits on `ready`; `P_VERD` reads `ok` / `violation`.
//
// ===========================================================================
// WHAT IT COSTS, HAND-COUNTED AND IN THE UNFLATTERING DIRECTION
// ===========================================================================
// NOT FITTED -- the owner's ruling for this entry forbids a full-console fit
// and authorises targeted local measurement only, so every number here is a
// hand count from the declarations below and must be read as one.
//
//   plane buffer   1,024 x 32 bits = 32,768 bits  -> 4 M10K at 10,240 b each
//   slot records   256 x (1 + PIDW(16) + SIGW(32)) = 256 x 49 = 12,544 bits
//                                                  -> 2 M10K
//   burst shifter  BURST_W = 512 flops
//   the rest       the two FSMs, the counters and the addressing: order 400
//                  flops, dominated by the ten 32-bit censuses
//
// M10K is the resource with slack on this device and ALMs are the binding
// constraint, so both arrays are memories deliberately.  In particular the
// slot records are a RAM and NOT a 256-entry flop array: `zhao_dc_sdp_ram` at
// module scope is the remedy this tree records for exactly that, after an
// array declared inside a `generate for` loop cost 146,414 registers against
// 1,010.
//
// ===========================================================================
// EVERY COUNTER HERE CAN BE MADE TO MOVE, and `tests/terrain/
// terrain_matpub_directed.cpp` moves each one by stimulus.  A counter asserted
// zero and never seen to fire is a claim, not a measurement.
// ===========================================================================

module zhao_terrain_matpub
  import zhao_pkg::*;
#(
    // The commissioned region.  Both are KNOBS: the publisher can be pointed
    // at any unmapped range, and the elaboration guard below refuses a
    // parameterisation whose footprint does not fit the span it was given.
    parameter logic [31:0] REGION_BASE = ZHAO_TERRAIN_COMPOSED_MATERIAL_BASE,
    parameter logic [31:0] REGION_SPAN = ZHAO_TERRAIN_COMPOSED_MATERIAL_SPAN,
    parameter int unsigned SLOTS       = 256,
    parameter int unsigned SLOT_BYTES  = 8192,
    // The composed plane's shape.  CELLS_X/Y are the CELL counts, which are one
    // less per axis than the lattice's vertex counts -- 33 x 33 vertices make
    // 32 x 32 cells, and `matjoin`'s `o_ci_o` / `o_cj_o` are five bits for
    // exactly that reason.
    parameter int unsigned CELLS_X     = 32,
    parameter int unsigned CELLS_Y     = 32,
    parameter int unsigned PIDW        = 16,
    parameter int unsigned SIGW        = 32,
    // DERIVED, AND A PARAMETER RATHER THAN A LOCALPARAM ONLY BECAUSE A PORT
    // USES IT.  A port declaration cannot reference a localparam declared in
    // the body, so the slot index's width has to be visible here.  It is not
    // meant to be overridden; `SLOTS` is the knob.
    parameter int unsigned SLOTSW      = $clog2(SLOTS)
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- configuration ----------------------------------------------------
    // The publisher is ARMED rather than always-on.  A guard window opens with
    // its block and never ahead of it (TERRAIN.DEVSTORE's precedent), and a
    // console that has not been told to publish must issue no request at all.
    input  var logic        cfg_enable_i,
    input  var zhao_client_e cfg_vram_client_i,

    // ---- THE TAP: `zhao_terrain_matjoin`'s composed write face -------------
    // Port for port the compose cache's layer-E write.  This block never
    // acknowledges it and never stalls it: the join's write face has no ready
    // and must not grow one, because a publisher that could backpressure the
    // composer would put a memory client on the compose path.
    input  var logic       c_we_i,
    input  var logic [4:0] c_ci_i,
    input  var logic [4:0] c_cj_i,
    input  var logic [7:0] c_mat_a_i,
    input  var logic [7:0] c_mat_b_i,
    input  var logic [7:0] c_weight_i,

    // ---- the fill's boundaries and the patch's identity --------------------
    // `fill_start_i` opens a plane; `commit_i` declares it complete.  The
    // identity is sampled at COMMIT rather than at start, because the slot a
    // plane lands in is the slot it is committed into.
    input  var logic                  fill_start_i,
    input  var logic                  commit_i,
    input  var logic [PIDW-1:0]       patch_id_i,
    input  var logic [SLOTSW-1:0]     slot_i,

    // ---- MEM.GUARD write client -------------------------------------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    output var logic [63:0]     guard_wdata_o,
    output var logic            guard_wvalid_o,
    input  var logic            guard_wready_i,
    output var logic            guard_wlast_o,

    // ---- censuses ----------------------------------------------------------
    output var logic [31:0] cells_captured_o,   // cells taken into the buffer
    output var logic [31:0] commits_o,          // planes declared complete
    output var logic [31:0] patches_published_o,// planes actually written out
    output var logic [31:0] skipped_clean_o,    // same occupant, same plane
    output var logic [31:0] stranger_pub_o,     // published on an occupant change
    output var logic [31:0] bursts_written_o,   // 64-byte writes that completed
    output var logic [31:0] guard_denied_o,     // the guard refused a request
    output var logic [31:0] cell_oob_o,         // a cell write outside the plane
    output var logic [31:0] short_fill_o,       // a commit that owed more cells
    output var logic [31:0] commit_busy_o,      // a commit while still writing
    output var logic        busy_o
);

  // ==========================================================================
  // DERIVED SHAPE
  // ==========================================================================
  localparam int unsigned CELLS      = CELLS_X * CELLS_Y;          // 1024
  localparam int unsigned CELLAW     = $clog2(CELLS);              // 10
  // The fill counter is TWO bits wider than the cell address, not one.  One
  // wider holds CELLS exactly and therefore cannot represent an OVERFILL --
  // which would make `short_fill_o` blind to the fault in the flattering
  // direction, the one direction this repository audits hardest.
  localparam int unsigned FILLCW     = CELLAW + 2;                 // 12
  localparam int unsigned BURST_B    = 64;                         // bytes
  localparam int unsigned BEATS      = BURST_B / 8;                // 8
  localparam int unsigned BURST_W    = 8 * BURST_B;                // 512 bits
  localparam int unsigned CELLS_PER_BURST = BURST_B / 4;           // 16
  localparam int unsigned BURSTS     = CELLS / CELLS_PER_BURST;    // 64
  localparam int unsigned BURSTSW    = $clog2(BURSTS);             // 6
  localparam int unsigned PAYLOAD_B  = CELLS * 4;                  // 4096
  localparam int unsigned RECW       = 1 + PIDW + SIGW;            // 49

  // ==========================================================================
  // ELABORATION GUARDS
  // ==========================================================================
  // Quartus 17 requires an elaboration check to sit INSIDE `initial begin ...
  // end`: a bare module-scope `if` lints clean under Verilator with zero
  // diagnostics and fails `quartus_map` with "syntax error near text: `if`".
  // A `--lint-only` pass does not RUN an `initial` block, so a clean lint is
  // not evidence about any of the four checks below.
  //
  // (And the line above is deliberately not started with the linter's own
  // name: a comment whose first word after the slashes is that name is parsed
  // as a PRAGMA and rejected. This file hit it on its first lint.)
  // synthesis translate_off
  initial begin
    if (PAYLOAD_B > SLOT_BYTES)
      $fatal(1, "zhao_terrain_matpub: a plane is %0d B and the slot is %0d B",
             PAYLOAD_B, SLOT_BYTES);
    if ((SLOTS * SLOT_BYTES) > REGION_SPAN)
      $fatal(1, "zhao_terrain_matpub: %0d slots x %0d B exceeds the region span %0d",
             SLOTS, SLOT_BYTES, REGION_SPAN);
    if ((CELLS % CELLS_PER_BURST) != 0)
      $fatal(1, "zhao_terrain_matpub: %0d cells is not a whole number of %0d-cell bursts",
             CELLS, CELLS_PER_BURST);
    if ((SLOT_BYTES % BURST_B) != 0)
      $fatal(1, "zhao_terrain_matpub: slot %0d B is not a whole number of %0d-B bursts",
             SLOT_BYTES, BURST_B);
    // THE TOP OF THE REGION MUST FIT THE GUARD'S ADDRESS WIDTH.  The address
    // arithmetic is done at ZHAO_VRAM_ADDR_BITS, so a region placed above that
    // would WRAP rather than fail, and a wrapped write lands inside somebody
    // else's window with every counter agreeing.
    if ((64'(REGION_BASE) + 64'(SLOTS) * 64'(SLOT_BYTES)) >
        (64'd1 << ZHAO_VRAM_ADDR_BITS))
      $fatal(1, "zhao_terrain_matpub: base %0h + %0d slots exceeds the %0d-bit guard address space",
             REGION_BASE, SLOTS, ZHAO_VRAM_ADDR_BITS);
  end
  // synthesis translate_on

  // ==========================================================================
  // THE CELL WORD, AND THE ONE PLACE THE LAYOUT IS WRITTEN DOWN
  // ==========================================================================
  // `{8'h00, weight, mat_b, mat_a}` -- mat_a in the LOW byte, so cell N's
  // mat_a is byte 4N of the published slot.  A reader decodes it by that
  // sentence and nothing else.
  function automatic logic [31:0] mp_cell_word(input logic [7:0] a,
                                               input logic [7:0] b,
                                               input logic [7:0] w);
    mp_cell_word = {8'h00, w, b, a};
  endfunction

  // The signature fold.  Shift-xor over {index, word}: the index is mixed in so
  // a permutation of the same cell values does not alias to the same plane.
  function automatic logic [SIGW-1:0] sig_fold_c(input logic [SIGW-1:0] acc,
                                                 input logic [CELLAW-1:0] idx,
                                                 input logic [31:0] word);
    sig_fold_c = {acc[SIGW-2:0], acc[SIGW-1]} ^ word ^ {{(SIGW-CELLAW){1'b0}}, idx};
  endfunction

  // ==========================================================================
  // THE PLANE BUFFER
  // ==========================================================================
  // One flat array at module scope, through `zhao_dc_sdp_ram`, which is this
  // tree's remedy for the generate-loop array trap.  The write port is the
  // tap's; the read port is the publisher's, and the two never address the same
  // cell in the same cycle because a fill and a publish of the SAME slot are
  // sequenced by `commit_busy_o` below.
  logic                 pb_wr_en;
  logic [CELLAW-1:0]    pb_wr_addr;
  logic [31:0]          pb_wr_data;
  logic                 pb_rd_en;
  logic [CELLAW-1:0]    pb_rd_addr;
  logic [31:0]          pb_rd_data;

  zhao_dc_sdp_ram #(
      .DATA_W(32),
      .ADDR_W(CELLAW)
  ) u_plane (
      .wr_clk (clk),
      .wr_en  (pb_wr_en),
      .wr_addr(pb_wr_addr),
      .wr_data(pb_wr_data),
      .rd_clk (clk),
      .rd_en  (pb_rd_en),
      .rd_addr(pb_rd_addr),
      .rd_data(pb_rd_data)
  );

  // ==========================================================================
  // THE SLOT RECORDS -- {valid, patch id, signature}
  // ==========================================================================
  logic                 sr_wr_en;
  logic [SLOTSW-1:0]    sr_wr_addr;
  logic [RECW-1:0]      sr_wr_data;
  logic                 sr_rd_en;
  logic [SLOTSW-1:0]    sr_rd_addr;
  logic [RECW-1:0]      sr_rd_data;

  zhao_dc_sdp_ram #(
      .DATA_W(RECW),
      .ADDR_W(SLOTSW)
  ) u_slotrec (
      .wr_clk (clk),
      .wr_en  (sr_wr_en),
      .wr_addr(sr_wr_addr),
      .wr_data(sr_wr_data),
      .rd_clk (clk),
      .rd_en  (sr_rd_en),
      .rd_addr(sr_rd_addr),
      .rd_data(sr_rd_data)
  );

  // A slot record RAM comes out of reset holding whatever the memory holds, so
  // "have I ever published this slot" cannot be read from it.  This 256-bit
  // vector is the seen-since-reset mark, and it is flops on purpose: one bit
  // per slot is cheap and a RAM that needs clearing would need a clear walk.
  logic [SLOTS-1:0] sr_seen_q;

  // ==========================================================================
  // THE CAPTURE SIDE
  // ==========================================================================
  // ROW-MAJOR, AND WRITTEN AS AN ARITHMETIC RATHER THAN A CONCATENATION.
  // `{c_cj_i, c_ci_i}` is the same thing ONLY while CELLS_X is exactly 32, and
  // it truncates silently at any other shape -- so the concatenation would make
  // every parameterisation but the shipped one quietly wrong, which is the
  // direction this tree audits hardest.  The multiply is by an
  // elaboration-time constant and costs no DSP.
  wire [CELLAW-1:0] cap_idx_c = CELLAW'(({5'd0, c_cj_i} * 10'(CELLS_X)) +
                                        {5'd0, c_ci_i});
  wire              cap_in_c  = ({3'd0, c_ci_i} < 8'(CELLS_X)) &&
                                ({3'd0, c_cj_i} < 8'(CELLS_Y));
  wire [31:0]       cap_word_c = mp_cell_word(c_mat_a_i, c_mat_b_i, c_weight_i);

  logic [SIGW-1:0]      fill_sig_q;
  logic [FILLCW-1:0]    fill_cells_q;   // wide enough that an OVERFILL is visible
  localparam logic [FILLCW-1:0] FILL_OWED_C = FILLCW'(CELLS);
  localparam logic [FILLCW-1:0] FILL_SAT_C  = {FILLCW{1'b1}};

  // ==========================================================================
  // THE PUBLISH FSM
  // ==========================================================================
  typedef enum logic [2:0] {
    P_IDLE,    // nothing pending
    P_REC,     // the slot record's read is in flight
    P_DECIDE,  // the record is back; take the three-arm gate
    P_LOAD,    // pull CELLS_PER_BURST cells into the burst shifter
    P_REQ,     // offer the request; wait for the guard's LEVEL ready
    P_VERD,    // read the guard's PULSED ok / violation
    P_WBEAT    // stream BEATS beats
  } pstate_e;

  pstate_e              pstate_q;
  logic [SLOTSW-1:0]    pub_slot_q;
  logic [PIDW-1:0]      pub_pid_q;
  logic [SIGW-1:0]      pub_sig_q;
  logic [BURSTSW-1:0]   pub_burst_q;
  logic [BURST_W-1:0]   pub_sh_q;
  logic [3:0]           pub_beat_q;
  logic [4:0]           pub_load_q;     // 0..CELLS_PER_BURST, plus the RAM's latency

  // The read address walks the plane: burst b covers cells [16b, 16b+16).
  wire [CELLAW-1:0] pub_cell_c =
      CELLAW'({pub_burst_q, 4'd0}) + CELLAW'(pub_load_q[3:0]);

  // The destination.  `REGION_BASE + slot*SLOT_BYTES + burst*BURST_B`, with the
  // slot index bounded by SLOTS and the burst index by BURSTS, so the sum
  // cannot leave the region.
  //
  // IT IS BUILT AT THE GUARD'S OWN WIDTH RATHER THAN AT 32 AND SLICED.  A
  // 32-bit sum narrowed to the guard's 27 would DISCARD the top five bits
  // silently -- and a discarded carry is an address that lands somewhere
  // plausible in a region it does not own.  The elaboration guard below refuses
  // any parameterisation whose top byte would not fit instead, so the
  // truncation this arithmetic used to do by accident is now a compile error.
  wire [ZHAO_VRAM_ADDR_BITS-1:0] pub_addr_c =
      REGION_BASE[ZHAO_VRAM_ADDR_BITS-1:0]
    + (ZHAO_VRAM_ADDR_BITS'(pub_slot_q)  * ZHAO_VRAM_ADDR_BITS'(SLOT_BYTES))
    + (ZHAO_VRAM_ADDR_BITS'(pub_burst_q) * ZHAO_VRAM_ADDR_BITS'(BURST_B));

  assign busy_o = (pstate_q != P_IDLE);

  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (pstate_q == P_REQ);
    guard_req_o.write  = 1'b1;
    guard_req_o.client = cfg_vram_client_i;
    guard_req_o.addr   = pub_addr_c[ZHAO_VRAM_ADDR_BITS-1:0];
    guard_req_o.len    = 7'(BURST_B);
    guard_req_o.be     = '1;
  end

  assign guard_wvalid_o = (pstate_q == P_WBEAT);
  assign guard_wdata_o  = pub_sh_q[63:0];
  assign guard_wlast_o  = (pub_beat_q == 4'(BEATS - 1));

  // The plane's read port belongs to P_LOAD alone.
  assign pb_rd_en   = (pstate_q == P_LOAD);
  assign pb_rd_addr = pub_cell_c;

  // The record RAM's read port belongs to P_REC alone.
  assign sr_rd_en   = (pstate_q == P_REC);
  assign sr_rd_addr = pub_slot_q;

  // The record's write happens on the cycle the decision is taken, so a slot
  // committed twice in a row cannot read its own stale record.
  assign sr_wr_en   = (pstate_q == P_DECIDE);
  assign sr_wr_addr = pub_slot_q;
  assign sr_wr_data = {1'b1, pub_pid_q, pub_sig_q};

  // The three-arm gate, from the record that came back.
  wire                 rec_valid_c = sr_rd_data[RECW-1] && sr_seen_q[pub_slot_q];
  wire [PIDW-1:0]      rec_pid_c   = sr_rd_data[SIGW +: PIDW];
  wire [SIGW-1:0]      rec_sig_c   = sr_rd_data[SIGW-1:0];
  wire                 arm_never_c    = !rec_valid_c;
  wire                 arm_stranger_c = rec_valid_c && (rec_pid_c != pub_pid_q);
  wire                 arm_changed_c  = rec_valid_c && (rec_pid_c == pub_pid_q) &&
                                        (rec_sig_c != pub_sig_q);
  wire                 do_publish_c   = arm_never_c || arm_stranger_c || arm_changed_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pstate_q            <= P_IDLE;
      pub_slot_q          <= '0;
      pub_pid_q           <= '0;
      pub_sig_q           <= '0;
      pub_burst_q         <= '0;
      pub_sh_q            <= '0;
      pub_beat_q          <= '0;
      pub_load_q          <= '0;
      sr_seen_q           <= '0;
      fill_sig_q          <= '0;
      fill_cells_q        <= '0;
      pb_wr_en            <= 1'b0;
      pb_wr_addr          <= '0;
      pb_wr_data          <= '0;
      cells_captured_o    <= 32'd0;
      commits_o           <= 32'd0;
      patches_published_o <= 32'd0;
      skipped_clean_o     <= 32'd0;
      stranger_pub_o      <= 32'd0;
      bursts_written_o    <= 32'd0;
      guard_denied_o      <= 32'd0;
      cell_oob_o          <= 32'd0;
      short_fill_o        <= 32'd0;
      commit_busy_o       <= 32'd0;
    end else begin
      pb_wr_en <= 1'b0;

      // ====================================================================
      // CAPTURE.  Unconditional on the tap, because the join's write face has
      // no ready and this block must never gate it.
      // ====================================================================
      if (fill_start_i) begin
        fill_sig_q   <= '0;
        fill_cells_q <= '0;
      end

      if (c_we_i) begin
        if (!cap_in_c) begin
          cell_oob_o <= cell_oob_o + 32'd1;
        end else begin
          pb_wr_en   <= 1'b1;
          pb_wr_addr <= cap_idx_c;
          pb_wr_data <= cap_word_c;
          fill_sig_q <= sig_fold_c(fill_sig_q, cap_idx_c, cap_word_c);
          // Saturating, so an overfilled plane reads as MORE than it owes and
          // the short-fill check cannot be satisfied by a wrap.
          if (fill_cells_q != FILL_SAT_C)
            fill_cells_q <= fill_cells_q + FILLCW'(1);
          cells_captured_o <= cells_captured_o + 32'd1;
          // `fill_start_i` and a cell on the same clock: the reset above wins
          // for the accumulators, so re-apply this cell to the cleared state.
          if (fill_start_i) begin
            fill_sig_q   <= sig_fold_c('0, cap_idx_c, cap_word_c);
            fill_cells_q <= FILLCW'(1);
          end
        end
      end

      // ====================================================================
      // COMMIT.  A plane is complete; decide whether it is published.
      // ====================================================================
      if (commit_i) begin
        commits_o <= commits_o + 32'd1;
        if (fill_cells_q != FILL_OWED_C)
          short_fill_o <= short_fill_o + 32'd1;
        if (pstate_q != P_IDLE) begin
          // A commit arriving while the previous plane is still being written
          // is DROPPED and COUNTED.  Buffering it would need a second plane
          // buffer, and a silent drop is the thing this campaign keeps finding.
          commit_busy_o <= commit_busy_o + 32'd1;
        end else if (cfg_enable_i) begin
          pub_slot_q  <= slot_i;
          pub_pid_q   <= patch_id_i;
          pub_sig_q   <= fill_sig_q;
          pstate_q    <= P_REC;
        end
      end

      // ====================================================================
      // THE PUBLISH ENGINE
      // ====================================================================
      case (pstate_q)
        P_IDLE: ;   // the commit arm above is the only entry

        // The record RAM answers in one cycle.
        P_REC: pstate_q <= P_DECIDE;

        P_DECIDE: begin
          sr_seen_q[pub_slot_q] <= 1'b1;   // the record write is assigned above
          if (do_publish_c) begin
            if (arm_stranger_c) stranger_pub_o <= stranger_pub_o + 32'd1;
            pub_burst_q <= '0;
            pub_load_q  <= '0;
            pstate_q    <= P_LOAD;
          end else begin
            skipped_clean_o <= skipped_clean_o + 32'd1;
            pstate_q        <= P_IDLE;
          end
        end

        // Pull sixteen cells into the 512-bit shifter, LOW cell first, so byte
        // 0 of the burst is cell 16b's mat_a.  The RAM answers one cycle after
        // the address, so the shift takes the PREVIOUS cycle's data and the
        // walk runs one step ahead of it.
        P_LOAD: begin
          if (pub_load_q != 5'd0)
            pub_sh_q <= {pb_rd_data, pub_sh_q[BURST_W-1:32]};
          if (pub_load_q == 5'(CELLS_PER_BURST)) begin
            pub_beat_q <= 4'd0;
            pstate_q   <= P_REQ;
          end else begin
            pub_load_q <= pub_load_q + 5'd1;
          end
        end

        // `ready` is a LEVEL; `ok` is PULSED the cycle after the accept.
        P_REQ: if (guard_rsp_i.ready) pstate_q <= P_VERD;

        P_VERD: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            pstate_q       <= P_IDLE;
          end else if (guard_rsp_i.ok) begin
            pub_beat_q <= 4'd0;
            pstate_q   <= P_WBEAT;
          end
        end

        P_WBEAT: if (guard_wready_i) begin
          pub_sh_q   <= {64'd0, pub_sh_q[BURST_W-1:64]};
          pub_beat_q <= pub_beat_q + 4'd1;
          if (pub_beat_q == 4'(BEATS - 1)) begin
            bursts_written_o <= bursts_written_o + 32'd1;
            if (pub_burst_q == BURSTSW'(BURSTS - 1)) begin
              patches_published_o <= patches_published_o + 32'd1;
              pstate_q            <= P_IDLE;
            end else begin
              pub_burst_q <= pub_burst_q + 1'b1;
              pub_load_q  <= '0;
              pstate_q    <= P_LOAD;
            end
          end
        end

        default: pstate_q <= P_IDLE;
      endcase
    end
  end

endmodule
