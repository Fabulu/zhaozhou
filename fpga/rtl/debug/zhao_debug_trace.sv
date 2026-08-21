// zhao_debug_trace.sv — DEBUG.TRACE: the bounded event ring.
//
// Contract: design/contracts/DEBUG.TRACE.md
// Reference: `zref::trace::Ring` (reference/include/zref/zref_trace.hpp).
//
// ---------------------------------------------------------------------------
// THE ONE LAW THAT MATTERS MORE THAN THE OTHERS
// ---------------------------------------------------------------------------
// **THIS BLOCK DROPS. IT NEVER STALLS.**
//
// A trace ring exists to observe a machine without changing it. A ring that
// back-pressured its producer would make the act of tracing alter the timing
// being traced — the pipeline would run differently while being watched, and
// the capture would describe a machine that does not exist when the trace is
// off. So a full ring counts what it lost and the pipeline runs on untouched.
//
// There is no ready signal on the event port. That absence is the law: there is
// nothing for a producer to wait on, so no producer can be delayed by tracing.
//
// ---------------------------------------------------------------------------
// WHAT IS RATIFIED AND WHAT WAS CHOSEN
// ---------------------------------------------------------------------------
// Ratified elsewhere and NOT this block's to invent:
//   - the 32-byte record layout (capture_format.md chunk 0x000A)
//   - source_id packing, { kind:4, module:12, index:16 } (capture_format.md §5)
//   - the seven trace sources (charter §20.6)
//
// Chosen in zref_trace.hpp, mirrored here:
//   - stage numbers are the charter's list order, 0..6. A stage outside that
//     range is NOT A LEGAL EVENT: it is not stored and it is not counted as a
//     drop, because nothing was ever offered that this block understood.
//   - arming is a seven-bit MASK, not a selector, so two stages can be traced
//     in one run. Correlating two stages is the entire point of a trace ring.
//   - an unarmed stage is not an event either. It is not stored and NOT counted
//     as a drop; `dropped` means "a real event was lost to a full ring", which
//     is the only reading that lets someone trust a capture.
//
// ---------------------------------------------------------------------------
// THE RECORD, word by word
// ---------------------------------------------------------------------------
// Eight 32-bit words per event, little-endian, exactly the capture layout:
//
//     0 tile        1 primitive   2 pixel       3 { rsv[3]=0, stage }
//     4 expected_fx 5 actual_fx   6 source_id   7 command_seq
//
// Word 3 carries `stage` in its LOW byte with the three reserved bytes zero,
// which is what a little-endian read of { u8 stage; u8 rsv[3] } produces. The
// reserved bytes are zero rather than undefined so that two captures can be
// compared byte for byte; undefined padding is how a trace format rots.
//
// A WHOLE RECORD IS WRITTEN IN ONE CYCLE, into a 256-bit entry. The obvious
// alternative -- eight 32-bit writes over eight cycles -- gives the block a
// BUSY window, and an event offered during that window would be lost without
// even being counted as a drop. Silent loss is strictly worse than a counted
// drop: it makes the ring lie rather than admit. There is no ready signal to
// hold a producer off with, so the write must not take a window at all.
//
// Readout stays 32-bit word-addressed: the entry is read whole and the word is
// selected after the register, with the selector delayed to match.
//
// M10K rules: no initializer, no reset branch on the array, registered read
// inside the clocked process only.
module zhao_debug_trace #(
    parameter int DEPTH = 64
) (
    input  logic clk,
    input  logic rst_n,

    // ---- control -----------------------------------------------------------
    input  logic       arm_we_i,
    input  logic [6:0] arm_mask_i,    // bit n arms stage n
    output logic [6:0] armed_o,
    input  logic       clear_i,       // empties the ring and zeroes the drop count

    // ---- the event port: no ready, by design -------------------------------
    input  logic        ev_valid_i,
    input  logic [ 7:0] ev_stage_i,
    input  logic [31:0] ev_tile_i,
    input  logic [31:0] ev_primitive_i,
    input  logic [31:0] ev_pixel_i,
    input  logic [31:0] ev_expected_fx_i,
    input  logic [31:0] ev_actual_fx_i,
    input  logic [31:0] ev_source_id_i,
    input  logic [31:0] ev_command_seq_i,

    // ---- readout -----------------------------------------------------------
    // Word-addressed: {event, word}. The host drains the ring between frames.
    input  logic [$clog2(DEPTH*8)-1:0] rd_addr_i,
    output logic [31:0]                rd_data_o,

    output logic [31:0] count_o,      // events stored
    output logic [31:0] dropped_o     // events lost to a full ring
);

  localparam int WORDS = DEPTH * 8;
  localparam int AW = $clog2(WORDS);
  localparam int CW = $clog2(DEPTH + 1);
  localparam int EW = $clog2(DEPTH);

  // One entry per event: eight words side by side, so a record lands whole.
  logic [255:0] ring [0:DEPTH-1];
  logic [255:0] rd_entry;
  logic [2:0]   rd_word_q;

  logic [EW-1:0]  wr_idx;
  logic [255:0]   wr_data;
  logic           wr_en;

  always_ff @(posedge clk) begin
    if (wr_en) ring[wr_idx] <= wr_data;
    rd_entry <= ring[rd_addr_i[AW-1:3]];
    rd_word_q <= rd_addr_i[2:0];
  end

  // The word select sits AFTER the memory register, with the selector delayed
  // by the same cycle, so a 32-bit read of a 256-bit entry still costs one
  // cycle of latency and no extra memory ports.
  assign rd_data_o = rd_entry[{29'd0, rd_word_q} * 32 +: 32];

  logic [CW-1:0] count_q;
  logic [6:0]    mask_q;
  logic [31:0]   dropped_q;

  assign armed_o = mask_q;
  assign count_o = 32'(count_q);
  assign dropped_o = dropped_q;

  // An event this block understands: a stage inside the charter's list AND a
  // stage this run asked to see. Neither test is a drop -- see the header.
  logic legal_stage, is_armed, accept, overflow;
  always_comb begin
    legal_stage = ev_valid_i && (ev_stage_i < 8'd7);
    is_armed = legal_stage && mask_q[ev_stage_i[2:0]];
    overflow = is_armed && (count_q == CW'(DEPTH));
    accept = is_armed && !overflow;

    wr_en = accept && !clear_i;
    wr_idx = count_q[EW-1:0];
    // Capture order, little-endian. rsv[3] are zero deliberately.
    wr_data = {ev_command_seq_i, ev_source_id_i, ev_actual_fx_i, ev_expected_fx_i,
               {24'd0, ev_stage_i}, ev_pixel_i, ev_primitive_i, ev_tile_i};
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      count_q <= '0;
      mask_q <= '0;
      dropped_q <= '0;
    end else begin
      if (arm_we_i) mask_q <= arm_mask_i;

      if (clear_i) begin
        count_q <= '0;
        dropped_q <= '0;
      end else begin
        if (accept) count_q <= count_q + CW'(1);
        // Lost, counted, and the producer never knows.
        if (overflow && dropped_q != 32'hFFFF_FFFF) dropped_q <= dropped_q + 32'd1;
      end
    end
  end

endmodule : zhao_debug_trace
