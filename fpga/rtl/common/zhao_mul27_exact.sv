// zhao_mul27_exact.sv -- one exact signed 27x27 multiplication boundary.
//
// The caller owns every register, valid, tag, and hold rule.  This stateless
// leaf exists so the selected Cyclone-V mapping can be counted independently;
// `multstyle` is a preference and the mapped physical DSP remains the authority.
`default_nettype none

(* preserve_hierarchy *)
module zhao_mul27_exact (
    input  var logic signed [26:0] a_i,
    input  var logic signed [26:0] b_i,
    output var logic signed [53:0] p_o
);

  (* multstyle = "dsp" *) logic signed [53:0] product_c;
  assign product_c = a_i * b_i;
  assign p_o = product_c;

endmodule : zhao_mul27_exact

`default_nettype wire
