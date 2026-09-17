// tb_fb_tuple.sv -- scalar-port harness for the 84-bit READY/swap tuple layout.
//
// Exposes the package's pack and its six accessors at a module boundary so a
// directed test can compare them against INDEPENDENTLY WRITTEN bit positions.
// A round trip through pack and unpack would prove nothing here: both read the
// same constants, so they agree even when a constant is wrong. That is the
// two-operands-moving-together shape, and in a layout definition it is the only
// shape available -- which is why the test carries literals.
`default_nettype none

module tb_fb_tuple
  import zhao_fb_tuple_pkg::*;
(
    input  logic        writer_i,
    input  logic        slot_i,
    input  logic [15:0] generation_i,
    input  logic [1:0]  mode_i,
    input  logic [31:0] base_i,
    input  logic [31:0] span_i,
    output logic [83:0] packed_o,

    input  logic [83:0] tuple_i,
    output logic        writer_o,
    output logic        slot_o,
    output logic [15:0] generation_o,
    output logic [1:0]  mode_o,
    output logic [31:0] base_o,
    output logic [31:0] span_o,

    // The layout itself, so a test can assert the positions rather than infer
    // them from behaviour.
    output logic [7:0]  writer_lo_o,
    output logic [7:0]  slot_lo_o,
    output logic [7:0]  gen_lo_o,
    output logic [7:0]  mode_lo_o,
    output logic [7:0]  base_lo_o,
    output logic [7:0]  span_lo_o
);

  assign packed_o = zhao_fb_tuple_pack(writer_i, slot_i, generation_i, mode_i,
                                       base_i, span_i);

  assign writer_o     = zhao_fb_tuple_writer(tuple_i);
  assign slot_o       = zhao_fb_tuple_slot(tuple_i);
  assign generation_o = zhao_fb_tuple_generation(tuple_i);
  assign mode_o       = zhao_fb_tuple_mode(tuple_i);
  assign base_o       = zhao_fb_tuple_base(tuple_i);
  assign span_o       = zhao_fb_tuple_span(tuple_i);

  assign writer_lo_o = 8'(ZHAO_FB_WRITER_LO);
  assign slot_lo_o   = 8'(ZHAO_FB_SLOT_LO);
  assign gen_lo_o    = 8'(ZHAO_FB_GEN_LO);
  assign mode_lo_o   = 8'(ZHAO_FB_MODE_LO);
  assign base_lo_o   = 8'(ZHAO_FB_BASE_LO);
  assign span_lo_o   = 8'(ZHAO_FB_SPAN_LO);

  zhao_fb_tuple_contract u_contract ();

endmodule : tb_fb_tuple

`default_nettype wire
