// zhao_surface_blend.sv — the surface-sheet blend arithmetic, on its own.
//
// Factored out of zhao_surface_stamp so it can be PROVED rather than only
// sampled: it is purely combinational over 22 free input bits (mode 3, dst 8,
// src 8, age_shift 3), so `tests/formal/surface_blend.sby` is a TOTAL proof
// over all 4,194,304 inputs the block can ever be handed, not a bounded one.
// (Exactly the shape zhao_raster_blend / raster_fragment_blend.sby set for the
// fragment blend, and for the same reason.)
//
// Law:
//   reference/src/zrender/terrain.cpp `stamp_surface` — the two RATIFIED
//     blends: ABI operation 1 is `sat8((dst >> 1) + src)`, and every other
//     operation byte is `max(dst, src)` (the reference's `else`).
//   design/ops.yml FIELD.STAMP.{MAX,ADD,SUB,REPLACE,AGE} — the five
//     `stamp_mode` blends. Their `zref::fieldir::stamp_*` reference functions
//     do not exist anywhere in this tree (checked 2026-08-19); this is their
//     first implementation.
//   reference/include/zref/zref_surface.hpp `blend_apply` — the oracle.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md 12 — layer F strength is ONE
//     BYTE, which is why ops.yml's `sat16` wording saturates at 8 bits here.
//
// AGE's rate is CHOSEN (ops.yml says only "decays toward zero by a
// per-material age rate", which is not arithmetic): a right shift of 0..7, so
// decay is exact, monotone and TERMINATES. REJECTED ALTERNATIVE: the
// qformats 2 `unit_mul` form `(dst*rate + 128) >> 8` — more expressive, the
// right shape once a per-material table exists, but it costs a multiplier and
// a rate of 255/256 never reaches zero, so an aged scar lingers at strength 1
// for hundreds of frames.
//
// Conservative SystemVerilog subset only (charter 2).

module zhao_surface_blend (
    input  logic [2:0] mode_i,       // zref::surface::Blend
    input  logic [7:0] dst_i,        // layer F strength before
    input  logic [7:0] src_i,        // the stamp's source byte (strength >> 8)
    input  logic [2:0] age_shift_i,  // AGE only
    output logic [7:0] out_o
);

  // Codes, mirrored in zref::surface::Blend and in SURFACE.STAMP.
  localparam logic [2:0] BlStamp = 3'd0;  // ABI operation 0 (and every value != 1)
  localparam logic [2:0] BlDecayAcc = 3'd1;  // ABI operation 1
  localparam logic [2:0] BlMax = 3'd2;  // ops.yml FIELD.STAMP.MAX
  localparam logic [2:0] BlAdd = 3'd3;  // ops.yml FIELD.STAMP.ADD
  localparam logic [2:0] BlSub = 3'd4;  // ops.yml FIELD.STAMP.SUB
  localparam logic [2:0] BlReplace = 3'd5;  // ops.yml FIELD.STAMP.REPLACE
  localparam logic [2:0] BlAge = 3'd6;  // ops.yml FIELD.STAMP.AGE

  // One 9-bit lane per additive/subtractive form: the carry-out IS the
  // saturation flag, so there is no wide accumulate to narrow later and no
  // second place for a rail to be decided.
  logic [8:0] acc;

  always_comb begin
    acc = 9'd0;
    case (mode_i)
      BlDecayAcc: begin
        acc   = {2'b0, dst_i[7:1]} + {1'b0, src_i};
        out_o = acc[8] ? 8'hFF : acc[7:0];
      end
      BlAdd: begin
        acc   = {1'b0, dst_i} + {1'b0, src_i};
        out_o = acc[8] ? 8'hFF : acc[7:0];
      end
      BlSub: begin
        acc   = {1'b0, dst_i} - {1'b0, src_i};
        out_o = acc[8] ? 8'h00 : acc[7:0];  // acc[8] is the borrow
      end
      BlReplace: out_o = src_i;
      BlAge: out_o = dst_i >> age_shift_i;
      // BlStamp and BlMax are the same arithmetic; they are separate codes
      // only so the two vocabularies stay distinguishable in a trace.
      BlStamp, BlMax: out_o = (src_i > dst_i) ? src_i : dst_i;
      // Code 3'd7 is unassigned. It falls to MAX, matching the shape of the
      // reference's own `else` branch on the ABI operation byte: an
      // unrecognised blend keeps the peak rather than producing a value the
      // sheet has never held.
      default: out_o = (src_i > dst_i) ? src_i : dst_i;
    endcase
  end

endmodule
