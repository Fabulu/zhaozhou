// FIXTURE -- DELIBERATELY UNPARSEABLE. Not built, not fitted, never instantiated.
//
// The bank's width comes from a `$bits` of a type this tool does not elaborate,
// and its depth from a parameter defined in another file. The tool must EXIT 2
// and say so. It must NOT skip the array and report the file clean.
//
// That is the whole reason this fixture exists: "a parser that silently drops
// what it cannot match reports fewer problems", and nine tools in this tree
// failed in exactly that direction in one session.
module fx_unresolvable (
    input  var logic clk
);
  // V3-BANK: SAMPLE_METADATA
  (* ramstyle = "M10K" *) logic [$bits(meta_t)-1:0] sample_metadata_m [0:META_DEPTH-1];

  always_ff @(posedge clk) begin
    sample_metadata_m[0] <= '0;
  end
endmodule
