// zhao_geom_desc_crc.sv -- the CRC WALKER for GEOM.MESHFETCH's 64-byte meshlet
// descriptor. It produces `crc_ok_i`, the verdict the fetcher takes and does
// not compute (zhao_console_core entry I37).
//
// ---------------------------------------------------------------------------
// WHY THIS IS A BLOCK AND NOT A WIRE
// ---------------------------------------------------------------------------
// `zhao_geom_meshfetch` says of its `crc_ok_i`: "The CRC over bytes 0..59,
// folded by the caller's `zhao_crc32c_fold`. Wired in rather than folded here:
// that block is the one implementation and a second would be a second law."
// `zhao_crc32c_fold` is COMBINATIONAL -- {state, up to eight bytes, a count} in,
// next state out. What nothing owned was the WALKER: the thing that runs that
// fold across the descriptor's beats as they return, stops at byte 60, and
// compares the result with bytes 60..63. That is a state machine with a
// beat-counting law, so it is a file with a test, not glue in the composer.
//
// ---------------------------------------------------------------------------
// THE LAW, AND WHERE EACH HALF COMES FROM
// ---------------------------------------------------------------------------
//   reference/include/zref/zref_meshfetch.hpp:139
//       zhao_abi::zhao_crc32c(0, bytes, kCrcCovered) != d.crc32c -> kCrc
//   with kCrcCovered = 60 and kCrcOff = 60, the stored word little-endian.
//   `zhao_crc32c(0, ...)` is the reflected Castagnoli CRC with an all-ones
//   seed and a final complement -- the same convention `zhao_cmd_dma` already
//   implements around the same fold (`crc_hdr_r <= 32'hFFFF_FFFF` ...
//   `~crc_hdr_r != hget32(...)`). The test differences this block against the
//   generated `zhao_abi::zhao_crc32c` ITSELF rather than against a
//   transcription of it.
//
// THE BURST SHAPE IS ONE FACT WITH THE ALIGNMENT RULING: one len=64 request
// returns exactly eight 64-bit beats, low byte first (meshfetch's own header,
// "the alignment ruling and the burst shape are one fact"). So beats 0..6 fold
// eight bytes each (bytes 0..55) and beat 7 folds FOUR (56..59) and carries
// the stored CRC in its upper half (60..63).
//
// ---------------------------------------------------------------------------
// TIMING: THE VERDICT IS COMBINATIONAL ON THE LAST BEAT, BY THE CONSUMER'S LAW
// ---------------------------------------------------------------------------
// GEOM.MESHFETCH latches `crc_ok_q <= crc_ok_i` in the cycle `beat_last_i` is
// high -- a formal finding it records ("the fold completes with the last beat,
// so that edge is exactly when the verdict becomes meaningful"). So the eighth
// beat's four-byte fold and the compare are combinational here, and the first
// seven are registered. The verdict is FALSE on every other cycle, which is
// harmless because the consumer reads it on no other cycle.
//
// Two fold instances at CONSTANT byte counts rather than one with a runtime
// count, for the reason `zhao_cmd_dma` measured: a runtime `n_i` keeps all nine
// fold trees and puts a nine-way mux behind them.
//
// ---------------------------------------------------------------------------
// FRAMING IS PART OF THE VERDICT
// ---------------------------------------------------------------------------
// A burst whose `last` arrives on any beat but the eighth has not delivered a
// descriptor, and a CRC compared against whatever bytes happen to sit in that
// beat's upper half would be a verdict about nothing. Such a burst is REFUSED
// (verdict low) and counted on `framing_err_o`; so is a ninth beat arriving
// without `last`. The walk then restarts at the next burst: `last` always
// resets it, so one malformed burst cannot poison the next descriptor.
//
// It never learns WHICH descriptor it is walking, deliberately. The stored CRC
// comes out of the same bytes the fold consumed, and the fetcher's generation
// and format checks are independent of this block; nothing here is a detector
// wired to two operands a single enable moves together.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_geom_desc_crc (
    input  wire         clk,
    input  wire         rst_n,

    // ---- the descriptor burst, as GEOM.MESHFETCH receives it ----------------
    input  wire         beat_valid_i,
    input  wire [63:0]  beat_data_i,
    input  wire         beat_last_i,

    // ---- the verdict, meaningful in the cycle `beat_last_i` is high --------
    output wire         crc_ok_o,

    // ---- evidence -------------------------------------------------------------
    output logic [31:0] descriptors_o,   // bursts that ended (a `last` beat)
    output logic [31:0] crc_fail_o,      // well-framed bursts whose CRC mismatched
    output logic [31:0] framing_err_o    // `last` off the eighth beat, or a ninth beat
);

  localparam logic [31:0] SEED = 32'hFFFF_FFFF;

  logic [31:0] crc_q;     // the running state over the beats already folded
  logic [3:0]  idx_q;     // the beat index about to arrive, 0..8
  logic        over_q;    // a ninth beat arrived without `last`

  logic [31:0] fold8_c, fold4_c;

  zhao_crc32c_fold u_fold8 (.c_i(crc_q), .d_i(beat_data_i), .n_i(4'd8), .c_o(fold8_c));
  zhao_crc32c_fold u_fold4 (.c_i(crc_q), .d_i(beat_data_i), .n_i(4'd4), .c_o(fold4_c));

  wire last_beat_c  = beat_valid_i && beat_last_i;
  wire framed_c     = (idx_q == 4'd7) && !over_q;
  wire crc_match_c  = (~fold4_c) == beat_data_i[63:32];

  assign crc_ok_o = last_beat_c && framed_c && crc_match_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      crc_q         <= SEED;
      idx_q         <= '0;
      over_q        <= 1'b0;
      descriptors_o <= '0;
      crc_fail_o    <= '0;
      framing_err_o <= '0;
    end else if (beat_valid_i) begin
      if (beat_last_i) begin
        // The burst is over, whatever it was. Restart for the next one.
        crc_q  <= SEED;
        idx_q  <= '0;
        over_q <= 1'b0;
        if (descriptors_o != 32'hFFFF_FFFF) descriptors_o <= descriptors_o + 32'd1;
        if (!framed_c) begin
          if (framing_err_o != 32'hFFFF_FFFF) framing_err_o <= framing_err_o + 32'd1;
        end else if (!crc_match_c) begin
          if (crc_fail_o != 32'hFFFF_FFFF) crc_fail_o <= crc_fail_o + 32'd1;
        end
      end else begin
        if (idx_q < 4'd7) begin
          crc_q <= fold8_c;
          idx_q <= idx_q + 4'd1;
        end else begin
          // Beat index 7 is the one that must carry `last`. Anything at or
          // past it without `last` is an overrun; the burst will be refused
          // when its `last` finally arrives.
          over_q <= 1'b1;
        end
      end
    end
  end

endmodule : zhao_geom_desc_crc

`default_nettype wire
