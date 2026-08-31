// zhao_raster_blend_fin -- one half of the shipping blend. See zhao_raster_blend.sv, which
// wires both halves together and is what the formal proof targets.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall`.

// ---------------------------------------------------------------------------
// F3 -- THE FINISH HALF. Rounding, accumulator, rail.
//
// Takes the raw product and applies exactly what the original did after the
// multiply: `mixed = (prod + 128) >>> 8`, the one accumulator, the one rail.
// The commentary that justified each of those lines lives in the original
// header above and is not repeated.
// ---------------------------------------------------------------------------
module zhao_raster_blend_fin (
  input  logic [1:0]          mode_i,
  input  logic [7:0]          dst_i,
  input  logic [7:0]          src_i,
  input  logic signed [17:0]  prod_i,
  output logic [7:0]          out_o
);

  localparam logic [1:0] BL_REPLACE = 2'd0;
  localparam logic [1:0] BL_ALPHA   = 2'd1;
  localparam logic [1:0] BL_ADD     = 2'd2;
  localparam logic [1:0] BL_ADD_MOD = 2'd3;

  logic signed [17:0] mixed;
  assign mixed = (prod_i + 18'sd128) >>> 8;

  // ADD_MOD's rescaled value, bounded [0, 254] -- (255*255 + 128) >> 8 = 254 --
  // so its low 8 bits ARE its value.
  logic [7:0] modv;
  assign modv = mixed[7:0];

  logic signed [9:0] acc;
  always_comb begin
    case (mode_i)
      BL_ALPHA:   acc = $signed({2'd0, dst_i}) + $signed(mixed[9:0]);
      BL_ADD:     acc = $signed({2'd0, dst_i}) + $signed({2'd0, src_i});
      BL_ADD_MOD: acc = $signed({2'd0, dst_i}) + $signed({2'd0, modv});
      BL_REPLACE: acc = $signed({2'd0, src_i});
      default:    acc = $signed({2'd0, src_i});  // unreachable: mode_i is 2 bits
    endcase
  end

  logic unused_ok;
  assign unused_ok = &{1'b0, mixed[17:10]};

  always_comb begin
    if (acc[9])              out_o = 8'd0;    // negative
    else if (acc > 10'sd255) out_o = 8'd255;  // the rail
    else                     out_o = acc[7:0];
  end

endmodule : zhao_raster_blend_fin
