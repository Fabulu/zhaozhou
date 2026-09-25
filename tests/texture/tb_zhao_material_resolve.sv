// tb_zhao_material_resolve.sv -- TWO resolvers, ONE stimulus, TWO ceilings.
//
// DRIVEN-BY: tests/texture/material_resolve_rtl_directed.cpp:main
//
// This wrapper exists for one reason and it is not convenience. Audit R1
// (`reports/AUDIT.md`, and the header of
// `tests/texture/material_resolve_combine_bridge.cpp`) recorded a defect whose
// whole shape was a CEILING that had drifted below the combiner's:
//
//     zref_material.hpp implemented kRecipeCount = 8, including the two
//     three-sample terrain recipes ... record_legal refused `recipe >= 6`, so
//     a legal record naming either terrain recipe was rejected BEFORE it could
//     reach the combiner that exists to execute it.
//
// In `zhao_material_resolve` that ceiling is the `RECIPE_COUNT` parameter. At
// the shipping value of 8 the recipe field is three bits and the comparison is
// STRUCTURALLY UNREACHABLE -- no legal stimulus can fire the recipe arm of
// `refused_record_o`, so "it can refuse an out-of-range recipe" would stay an
// argument forever. CLAUDE.md's rule for that state is a committed mutant; the
// cheaper and better instrument, where the guard is a PARAMETER, is to
// instantiate a second copy at the HISTORICAL ceiling and drive both from the
// same wires.
//
// So `u_prod` is what ships and `u_ceiling6` is the 2026-09-05 bug preserved as
// a positive control. Every stimulus reaches both. A record naming recipe 6 or
// 7 is ACCEPTED by `u_prod` and REFUSED by `u_ceiling6`, and the difference
// between the two response streams IS the repair -- asserted, not narrated.
//
// THE LOCKSTEP IS REAL AND IT IS CONDITIONAL, and the condition was found by
// the directed suite rather than reasoned out here -- the first version of
// this comment claimed it unconditionally and was wrong.
//
// On a FIRST resolve the two instances are in lockstep by construction: a
// legality verdict changes the STATUS and never the cycle count, so both walk
// IDLE -> ADDR -> FILL -> RSP together off one handshake. That much held.
//
// What does not survive is a REVISIT of a record the two judge differently.
// `u_prod` CACHES a terrain-recipe record and hits; `u_ceiling6` refused it,
// never wrote a line, and goes back to memory. From that cycle the two are in
// different states -- and because the driver serves fetches off `u_prod`, the
// control's request is never answered and it stalls forever. Every `b_*`
// reading after that point is meaningless.
//
// So the rule, stated so the next driver does not have to rediscover it:
// **compare the two instances only across resolves they agree about, or across
// FIRST resolves on a fresh handle.** The suite's sweep therefore uses recipes
// 0..5, which both ceilings accept, and the terrain recipes get their own case
// on fresh handles where the divergence is the subject rather than a
// contaminant. The suite ASSERTS the lockstep on every resolve it makes rather
// than assuming it, because a wrapper whose two halves drift silently would
// make every comparison meaningless in the flattering direction.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply.
// This file is a TEST BENCH and is not in any production source list.
`default_nettype none

module tb_zhao_material_resolve #(
    parameter int unsigned RECW  = 256,
    parameter int unsigned BEATW = 64,
    parameter int unsigned SETS  = 4,
    parameter int unsigned LINES = 16,
    parameter int unsigned GENW  = 16,
    parameter int unsigned IDW   = 16,
    // The shipping ceiling and the historical one. Both are knobs so the
    // control can be re-pointed if `zref::material::kRecipeCount` ever moves.
    parameter int unsigned RECIPE_COUNT_PROD = 8,
    parameter int unsigned RECIPE_COUNT_OLD  = 6
) (
    input  logic clk,
    input  logic rst_n,

    // ---- shared stimulus ----------------------------------------------------
    input  logic             dir_we_i,
    input  logic [7:0]       dir_entry_i,
    input  logic             dir_valid_i,
    input  logic [23:0]      dir_set_index_i,
    input  logic [GENW-1:0]  dir_generation_i,
    input  logic [31:0]      dir_base_i,
    input  logic [IDW:0]     dir_count_i,
    input  logic             req_valid_i,
    input  logic [31:0]      req_material_set_i,
    input  logic [IDW-1:0]   req_material_id_i,
    input  logic [7:0]       req_quality_tier_i,
    input  logic             mem_req_ready_i,
    input  logic             mem_rsp_valid_i,
    input  logic [BEATW-1:0] mem_rsp_data_i,
    input  logic             mem_rsp_denied_i,   // R20: the guard refused the fetch
    input  logic             rsp_ready_i,

    // ---- A: the shipping resolver -------------------------------------------
    output logic             a_req_ready_o,
    output logic             a_mem_req_valid_o,
    output logic [31:0]      a_mem_req_addr_o,
    output logic             a_rsp_valid_o,
    output logic [2:0]       a_rsp_status_o,
    output logic             a_rsp_has_record_o,
    output logic [RECW-1:0]  a_rsp_record_o,
    output logic [7:0]       a_rsp_quality_tier_o,
    output logic [1:0]       a_rsp_sample_count_o,
    output logic [2:0]       a_rsp_material_recipe_o,
    output logic [7:0]       a_rsp_recipe_weight_o,
    output logic [7:0]       a_rsp_base_binding_o,
    output logic             a_rsp_selector_overflow_o,
    output logic [31:0]      a_rsp_palette_base_o,
    output logic [31:0]      a_rsp_raster_state_o,
    output logic [7:0]       a_rsp_flags_o,
    output logic [7:0]       a_rsp_sample0_modes_o,
    output logic [7:0]       a_rsp_sample1_modes_o,
    output logic [7:0]       a_rsp_sample2_modes_o,
    // THE FRAGMENT PROFILE's projection (FRAGSTATE, 2026-09-25). Exposed on the
    // SHIPPING instance only, because the test uses it to check that a declared
    // profile is CARRIED rather than merely accepted -- a record that passes
    // legality and then arrives with a zeroed profile would satisfy every other
    // port on this bench.
    output logic             a_rsp_frag_declared_o,
    output logic [31:0]      a_rsp_frag_state_o,
    output logic [7:0]       a_rsp_effect_tag_o,
    output logic [7:0]       a_rsp_stencil_ref_o,
    output logic [31:0]      a_material_hits_o,
    output logic [31:0]      a_material_misses_o,
    output logic [31:0]      a_material_refused_o,
    output logic [31:0]      a_refused_id_o,
    output logic [31:0]      a_refused_record_o,
    output logic [31:0]      a_not_resident_o,
    output logic [31:0]      a_selector_overflow_o,
    output logic [31:0]      a_recipe_count_mismatch_o,
    output logic [31:0]      a_fetch_denied_o,

    // ---- B: the 2026-09-05 ceiling, kept because it REFUSES -----------------
    output logic             b_req_ready_o,
    output logic             b_rsp_valid_o,
    output logic [2:0]       b_rsp_status_o,
    output logic             b_rsp_has_record_o,
    output logic [31:0]      b_refused_record_o,
    output logic [31:0]      b_material_misses_o
);

  zhao_material_resolve #(
      .RECW(RECW), .BEATW(BEATW), .SETS(SETS), .LINES(LINES),
      .RECIPE_COUNT(RECIPE_COUNT_PROD), .GENW(GENW), .IDW(IDW), .CW(32)
  ) u_prod (
      .clk(clk), .rst_n(rst_n),
      .dir_we_i(dir_we_i), .dir_entry_i(dir_entry_i), .dir_valid_i(dir_valid_i),
      .dir_set_index_i(dir_set_index_i), .dir_generation_i(dir_generation_i),
      .dir_base_i(dir_base_i), .dir_count_i(dir_count_i),
      .req_valid_i(req_valid_i), .req_ready_o(a_req_ready_o),
      .req_material_set_i(req_material_set_i),
      .req_material_id_i(req_material_id_i),
      .req_quality_tier_i(req_quality_tier_i),
      .mem_req_valid_o(a_mem_req_valid_o), .mem_req_ready_i(mem_req_ready_i),
      .mem_req_addr_o(a_mem_req_addr_o),
      .mem_rsp_valid_i(mem_rsp_valid_i), .mem_rsp_data_i(mem_rsp_data_i),
      .mem_rsp_denied_i(mem_rsp_denied_i),
      .rsp_valid_o(a_rsp_valid_o), .rsp_ready_i(rsp_ready_i),
      .rsp_status_o(a_rsp_status_o), .rsp_has_record_o(a_rsp_has_record_o),
      .rsp_record_o(a_rsp_record_o), .rsp_quality_tier_o(a_rsp_quality_tier_o),
      .rsp_sample_count_o(a_rsp_sample_count_o),
      .rsp_material_recipe_o(a_rsp_material_recipe_o),
      .rsp_recipe_weight_o(a_rsp_recipe_weight_o),
      .rsp_base_binding_o(a_rsp_base_binding_o),
      .rsp_selector_overflow_o(a_rsp_selector_overflow_o),
      .rsp_palette_base_o(a_rsp_palette_base_o),
      .rsp_raster_state_o(a_rsp_raster_state_o),
      .rsp_flags_o(a_rsp_flags_o),
      .rsp_sample0_modes_o(a_rsp_sample0_modes_o),
      .rsp_sample1_modes_o(a_rsp_sample1_modes_o),
      .rsp_sample2_modes_o(a_rsp_sample2_modes_o),
      .rsp_frag_declared_o(a_rsp_frag_declared_o),
      .rsp_frag_state_o(a_rsp_frag_state_o),
      .rsp_effect_tag_o(a_rsp_effect_tag_o),
      .rsp_stencil_ref_o(a_rsp_stencil_ref_o),
      .material_hits_o(a_material_hits_o),
      .material_misses_o(a_material_misses_o),
      .material_refused_o(a_material_refused_o),
      .refused_id_o(a_refused_id_o),
      .refused_record_o(a_refused_record_o),
      .not_resident_o(a_not_resident_o),
      .selector_overflow_o(a_selector_overflow_o),
      .recipe_count_mismatch_o(a_recipe_count_mismatch_o),
      .fetch_denied_o(a_fetch_denied_o)
  );

  // The control's unread outputs are declared and left open DELIBERATELY: what
  // this instance is evidence about is its STATUS and its refusal tally, and
  // binding the rest would suggest the two copies are compared field by field
  // when only the verdict is at issue. The waiver is scoped to this one
  // instantiation and says which pins and why, rather than silencing the class
  // for the file -- an open pin somewhere else here would be a mistake.
  /* verilator lint_off PINCONNECTEMPTY */
  zhao_material_resolve #(
      .RECW(RECW), .BEATW(BEATW), .SETS(SETS), .LINES(LINES),
      .RECIPE_COUNT(RECIPE_COUNT_OLD), .GENW(GENW), .IDW(IDW), .CW(32)
  ) u_ceiling6 (
      .clk(clk), .rst_n(rst_n),
      .dir_we_i(dir_we_i), .dir_entry_i(dir_entry_i), .dir_valid_i(dir_valid_i),
      .dir_set_index_i(dir_set_index_i), .dir_generation_i(dir_generation_i),
      .dir_base_i(dir_base_i), .dir_count_i(dir_count_i),
      .req_valid_i(req_valid_i), .req_ready_o(b_req_ready_o),
      .req_material_set_i(req_material_set_i),
      .req_material_id_i(req_material_id_i),
      .req_quality_tier_i(req_quality_tier_i),
      .mem_req_valid_o(), .mem_req_ready_i(mem_req_ready_i),
      .mem_req_addr_o(),
      .mem_rsp_valid_i(mem_rsp_valid_i), .mem_rsp_data_i(mem_rsp_data_i),
      .mem_rsp_denied_i(mem_rsp_denied_i),
      .rsp_valid_o(b_rsp_valid_o), .rsp_ready_i(rsp_ready_i),
      .rsp_status_o(b_rsp_status_o), .rsp_has_record_o(b_rsp_has_record_o),
      .rsp_record_o(), .rsp_quality_tier_o(),
      .rsp_sample_count_o(), .rsp_material_recipe_o(),
      .rsp_recipe_weight_o(), .rsp_base_binding_o(),
      .rsp_selector_overflow_o(),
      .rsp_palette_base_o(), .rsp_raster_state_o(), .rsp_flags_o(),
      .rsp_sample0_modes_o(), .rsp_sample1_modes_o(), .rsp_sample2_modes_o(),
      .rsp_frag_declared_o(), .rsp_frag_state_o(),
      .rsp_effect_tag_o(), .rsp_stencil_ref_o(),
      .material_hits_o(), .material_misses_o(b_material_misses_o),
      .material_refused_o(),
      .refused_id_o(), .refused_record_o(b_refused_record_o),
      .not_resident_o(), .selector_overflow_o(), .recipe_count_mismatch_o(),
      .fetch_denied_o()
  );
  /* verilator lint_on PINCONNECTEMPTY */

endmodule

`default_nettype wire
