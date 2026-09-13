// texjoin_duplicate_owner_top.sv -- positive control for role-aware ownership.
//
// This test-only composition unconditionally elaborates BOTH explicitly
// registered implementations: V3OWN through the selected island and TEXJOIN
// directly. Ports are intentionally left unconnected because this fixture is
// inspected as a Verilator V3Param AST and is neither simulated nor synthesized.
module texjoin_duplicate_owner_top;
  zhao_texture_island_v3_top u_v3();
  zhao_raster_texjoin_v2 u_texjoin();
endmodule
