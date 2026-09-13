// Elaboration-semantics control for lifecycle ownership.
//
// The active V3 island is instantiated through a macro. TEXJOIN sits in a
// constant-false generate branch, FRAGROB sits behind an undefined `ifdef, and a
// string contains instance-shaped text. Only the macro-expanded island and its
// nested zhao_texture_v3own are concrete elaborated cells.
`define ZHAO_ROLE_SELECTED_ROOT zhao_texture_island_v3_top

module texjoin_elaboration_semantics_top;
  localparam string INSTANCE_SHAPED_TEXT =
      "zhao_raster_texjoin_v2 string_shaped_instance(";

  `ZHAO_ROLE_SELECTED_ROOT macro_renamed_selected_instance();

  if (1'b0) begin : g_dead_generate
    zhao_raster_texjoin_v2 dead_generate_instance();
  end

`ifdef ZHAO_OWNERSHIP_DISABLED_CONTROL
  zhao_texture_fragrob disabled_ifdef_instance();
`endif
endmodule

// Merely sharing a source file with the selected top is not reachability.
module texjoin_unselected_container;
  zhao_raster_texjoin_v2 unselected_module_instance();
endmodule

`undef ZHAO_ROLE_SELECTED_ROOT
