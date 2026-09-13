// COMMITTED TEST MUTANT -- explicitly annotated extra lifecycle owner.
//
// The provider module and its instance are deliberately renamed, so matching a
// known module name cannot detect it. The semantic role attribute is the only
// ownership assertion. The positive-control root also reaches the selected V3
// island through a macro; exact Verilator elaboration must therefore report two
// lifecycle-owner instances and the registry-completeness check must reject the
// unregistered one.
(* zhao_ownership_role = "raster_texture_fragment_lifecycle" *)
module zhao_texture_lifecycle_extra_owner_mutant (
    output var logic witness_o
);
  assign witness_o = 1'b1;
endmodule

`define ZHAO_ROLE_SELECTED_ROOT zhao_texture_island_v3_top
module texjoin_extra_owner_control_top (
    output var logic witness_o
);
  `ZHAO_ROLE_SELECTED_ROOT macro_renamed_selected_instance();
  zhao_texture_lifecycle_extra_owner_mutant renamed_extra_instance (
      .witness_o(witness_o)
  );
endmodule
`undef ZHAO_ROLE_SELECTED_ROOT
