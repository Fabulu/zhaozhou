// tb_partmat_acceptance.sv -- the arrangement owner ruling 1's acceptance test
// names, composed from the PRODUCTION modules.
//
// ENFORCED-BY: tests/prod/partmat_acceptance.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT IS INSTANTIATED, AND WHAT IS COPIED
// ---------------------------------------------------------------------------
// `zhao_geom_clipdoor` and `zhao_material_window` are INSTANTIATED, not copied,
// so this bench cannot drift from them: a port either binds or the elaboration
// fails.
//
// R197's UNTEXTURED GATE IS FOUR LINES AND IT IS COPIED, because it lives in
// `zhao_console_core`'s body rather than in a module of its own. **That is a
// copy and a copy goes stale** -- CLAUDE.md has a chapter about exactly this,
// and a stale copy does not report that it is stale. So the copy is stated
// here in full, with the production text beside it, and it is the ONE thing a
// reader must check when `cl_in_*` changes:
//
//     wire cl_in_untex_c  = cd_o_untex;
//     wire cl_in_refuse_c = cl_in_untex_c && (mw_pub_sample_count != 2'd0);
//     assign cl_in_valid  = mw_t_valid && !cl_in_refuse_c;
//     assign mw_t_ready   = cl_in_refuse_c || cl_in_ready;
//
// (`zhao_console_core.sv`, the UNTEXTURED DOOR block. Search for
// `cl_in_refuse_c` there; there is exactly one definition.)
//
// The `d_enter_i` / `d_reject_i` / `d_leave_i` triple is the composer's too and
// is DRIVEN FROM THE DRIVER instead, because the driver is what models the
// GEOM.CLIP..door span -- the same shape `material_window_directed.cpp` uses,
// and the only way a bench can present a reject as well as a departure.
//
// ---------------------------------------------------------------------------
// WHY THE ORDER IS DOOR -> WINDOW AND NOT THE OTHER WAY
// ---------------------------------------------------------------------------
// It is the composer's order, and the composer's own comment says why: the door
// "carries the material half and the triangle half of one beat TOGETHER and
// grants them as one ... a door that arbitrated only the triangle would let a
// resolve answer for the material of a beat that did not win." Reversing them
// here would test an arrangement this console does not have.
`default_nettype none

module tb_partmat_acceptance #(
    parameter int unsigned ATTRS = 7,
    parameter int unsigned IDW   = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- client 0: textured mesh A -----------------------------------------
    input  var logic               c0_valid_i,
    output var logic               c0_ready_o,
    input  var logic               c0_untex_i,
    input  var logic        [31:0] c0_material_set_i,
    input  var logic        [15:0] c0_material_id_i,
    input  var logic        [ 1:0] c0_material_mode_i,
    input  var logic        [IDW-1:0] c0_src_id_i,

    // ---- client 1: textured mesh B -----------------------------------------
    input  var logic               c1_valid_i,
    output var logic               c1_ready_o,
    input  var logic               c1_untex_i,
    input  var logic        [31:0] c1_material_set_i,
    input  var logic        [15:0] c1_material_id_i,
    input  var logic        [ 1:0] c1_material_mode_i,
    input  var logic        [IDW-1:0] c1_src_id_i,

    // ---- client 2: the particle batch --------------------------------------
    input  var logic               c2_valid_i,
    output var logic               c2_ready_o,
    input  var logic               c2_untex_i,
    input  var logic        [31:0] c2_material_set_i,
    input  var logic        [15:0] c2_material_id_i,
    input  var logic        [ 1:0] c2_material_mode_i,
    input  var logic        [IDW-1:0] c2_src_id_i,
    // THE PARTICLE ARM'S RASTER STATE, per client, because as of 2026-09-23
    // (PARTDEPTH) `zhao_console_core` no longer feeds this slice a constant:
    // it feeds it `zhao_part_clipfeed.o_frag_state_o`, which carries
    // `zhao_part_expand`'s pass-7 law. A bench that went on tying all three
    // clients to zero would be measuring an arrangement the composer no longer
    // builds -- and would do so silently, which is the whole failure mode this
    // file's header is about.
    input  var logic        [31:0] c2_frag_state_i,

    // ---- what reaches GEOM.CLIP's input ------------------------------------
    output var logic               cl_in_valid_o,
    input  var logic               cl_in_ready_i,
    output var logic        [IDW-1:0] cl_src_id_o,
    output var logic               cl_untex_o,
    output var logic        [ 1:0] pub_sample_count_o,
    output var logic        [ 2:0] pub_material_recipe_o,
    output var logic        [ 7:0] pub_recipe_weight_o,
    output var logic        [ 7:0] pub_base_binding_o,
    output var logic        [ 1:0] pub_response_class_o,
    output var logic        [ 1:0] pub_material_mode_o,
    // THE PUBLISHED SPAN'S RASTER STATE. It was already wired internally and
    // read by nobody; exporting it is what lets the driver check the CONSUMER
    // end of the carriage rather than only that a wire exists.
    output var logic        [31:0] pub_frag_state_o,
    output var logic               pub_valid_o,
    output var logic        [ 2:0] owner_o,

    // ---- the span's three disposal events, the driver's ---------------------
    input  var logic               d_reject_i,
    input  var logic               d_leave_i,

    // ---- MATERIAL.RESOLVE, played by the driver ----------------------------
    output var logic               req_valid_o,
    input  var logic               req_ready_i,
    output var logic        [31:0] req_material_set_o,
    output var logic        [15:0] req_material_id_o,
    input  var logic               rsp_valid_i,
    output var logic               rsp_ready_o,
    input  var logic               rsp_has_record_i,
    input  var logic        [ 1:0] rsp_sample_count_i,
    input  var logic        [ 2:0] rsp_material_recipe_i,
    input  var logic        [ 7:0] rsp_recipe_weight_i,
    input  var logic        [ 7:0] rsp_base_binding_i,
    input  var logic        [ 7:0] rsp_sample0_modes_i,

    // ---- evidence -----------------------------------------------------------
    output var logic        [31:0] geom_untex_refused_o,
    output var logic        [31:0] mw_resolves_o,
    output var logic        [31:0] mw_switches_o,
    output var logic        [31:0] mw_no_record_o,
    output var logic        [31:0] mw_no_material_spans_o,
    output var logic        [31:0] mw_mode_refused_o,
    output var logic        [31:0] mw_err_unpublished_o,
    output var logic        [31:0] mw_err_underflow_o,
    output var logic        [95:0] cd_granted_o,
    output var logic        [31:0] cd_switches_o,
    output var logic        [31:0] cd_err_hold_broken_o
);

  localparam int unsigned NC = 3;
  localparam int unsigned AW = ATTRS * 32;

  // The TRIANGLE half is not what this bench is about, so every client presents
  // the same legal fan. `src_id` is what tells the three apart downstream, and
  // it is a REAL per-client field rather than a bench invention -- the composer
  // carries `rp_o_src_id`, `fa_o_src_id` and `pcf_o_src_id` through this door.
  logic [NC-1:0]        c_valid_c, c_ready_c, c_untex_c;
  logic [NC*21-1:0]     c_ax_c, c_ay_c, c_bx_c, c_by_c, c_cx_c, c_cy_c;
  logic [NC*3-1:0]      c_behind_c;
  logic [NC*IDW-1:0]    c_src_id_c;
  logic [NC*2-1:0]      c_cull_c, c_mmode_c;
  // The per-primitive RASTER declaration (SHADOWRIDE, 2026-09-23). Every
  // client here declares the OPAQUE profile, which is what the whole
  // existing suite runs under and what it must keep running under.
  logic [NC*8-1:0]      c_valpha_c;
  logic [NC*32-1:0]     c_fstate_c;
  logic [NC*AW-1:0]     c_attr_a_c, c_attr_b_c, c_attr_c_c;
  logic [NC*32-1:0]     c_mset_c;
  logic [NC*16-1:0]     c_mid_c;
  logic [NC*8-1:0]      c_tier_c;

  always_comb begin
    c_valid_c  = {c2_valid_i, c1_valid_i, c0_valid_i};
    c_untex_c  = {c2_untex_i, c1_untex_i, c0_untex_i};
    c_src_id_c = {c2_src_id_i, c1_src_id_i, c0_src_id_i};
    c_mset_c   = {c2_material_set_i, c1_material_set_i, c0_material_set_i};
    c_mid_c    = {c2_material_id_i, c1_material_id_i, c0_material_id_i};
    c_mmode_c  = {c2_material_mode_i, c1_material_mode_i, c0_material_mode_i};
    c_valpha_c = {NC{8'hFF}};
    // Clients 0 and 1 (the two meshes) keep the opaque frame default; client 2
    // (the particles) declares its own, exactly as the composer wires it.
    c_fstate_c = {c2_frag_state_i, 32'd0, 32'd0};
    c_tier_c   = {8'h33, 8'h22, 8'h11};
    c_cull_c   = 6'd0;
    c_behind_c = 9'd0;
    c_ax_c     = {3{21'sd0}};
    c_ay_c     = {3{-21'sd64}};
    c_bx_c     = {3{-21'sd48}};
    c_by_c     = {3{21'sd32}};
    c_cx_c     = {3{21'sd48}};
    c_cy_c     = {3{21'sd32}};
    c_attr_a_c = {NC{{ATTRS{32'h0000_00AA}}}};
    c_attr_b_c = {NC{{ATTRS{32'h0000_00BB}}}};
    c_attr_c_c = {NC{{ATTRS{32'h0000_00CC}}}};
  end

  assign c0_ready_o = c_ready_c[0];
  assign c1_ready_o = c_ready_c[1];
  assign c2_ready_o = c_ready_c[2];

  logic               cd_o_valid, cd_o_ready;
  logic [IDW-1:0]     cd_o_src_id;
  logic               cd_o_untex;
  logic [31:0]        cd_o_material_set;
  logic [15:0]        cd_o_material_id;
  logic [ 1:0]        cd_o_material_mode;
  logic [ 7:0] cd_o_vertex_alpha;
  logic [31:0] cd_o_frag_state;
  /* verilator lint_off UNUSEDSIGNAL */
  // The window republishes the span's declaration; this bench asserts
  // against the MODE and does not need the other two, which the console
  // reads into entry I20's two ports.
  logic [ 7:0] pub_vertex_alpha_w;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [ 7:0]        cd_o_quality_tier;
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [20:0] cd_o_ax, cd_o_ay, cd_o_bx, cd_o_by, cd_o_cx, cd_o_cy;
  logic [ 2:0]        cd_o_behind;
  logic [ 1:0]        cd_o_cull_mode;
  logic [AW-1:0]      cd_o_attr_a, cd_o_attr_b, cd_o_attr_c;
  logic [31:0]        cd_idle_offered;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_geom_clipdoor #(
    .NCLIENT (NC),
    .ATTRS   (ATTRS),
    .IDW     (IDW)
  ) u_door (
    .clk   (clk),
    .rst_n (rst_n),
    .c_valid_i        (c_valid_c),
    .c_ready_o        (c_ready_c),
    .c_ax_i           (c_ax_c),
    .c_ay_i           (c_ay_c),
    .c_bx_i           (c_bx_c),
    .c_by_i           (c_by_c),
    .c_cx_i           (c_cx_c),
    .c_cy_i           (c_cy_c),
    .c_behind_i       (c_behind_c),
    .c_src_id_i       (c_src_id_c),
    .c_untex_i        (c_untex_c),
    .c_cull_mode_i    (c_cull_c),
    .c_attr_a_i       (c_attr_a_c),
    .c_attr_b_i       (c_attr_b_c),
    .c_attr_c_i       (c_attr_c_c),
    // ARENAID 2026-09-25: the identity half, tied -- this bench measures
    // the door's material arbitration, not the identity space.
    .c_key_a_i        ('0),
    .c_key_b_i        ('0),
    .c_key_c_i        ('0),
    .c_rider_i        ('0),
    .c_material_set_i (c_mset_c),
    .c_material_id_i  (c_mid_c),
    .c_material_mode_i(c_mmode_c),
    .c_vertex_alpha_i (c_valpha_c),
    .c_frag_state_i   (c_fstate_c),
    .c_quality_tier_i (c_tier_c),

    .o_valid_o        (cd_o_valid),
    .o_ready_i        (cd_o_ready),
    .o_ax_o           (cd_o_ax),
    .o_ay_o           (cd_o_ay),
    .o_bx_o           (cd_o_bx),
    .o_by_o           (cd_o_by),
    .o_cx_o           (cd_o_cx),
    .o_cy_o           (cd_o_cy),
    .o_behind_o       (cd_o_behind),
    .o_src_id_o       (cd_o_src_id),
    .o_untex_o        (cd_o_untex),
    .o_cull_mode_o    (cd_o_cull_mode),
    .o_attr_a_o       (cd_o_attr_a),
    .o_attr_b_o       (cd_o_attr_b),
    .o_attr_c_o       (cd_o_attr_c),
    .o_key_a_o        (),
    .o_key_b_o        (),
    .o_key_c_o        (),
    .o_rider_o        (),
    .o_material_set_o (cd_o_material_set),
    .o_material_id_o  (cd_o_material_id),
    .o_material_mode_o(cd_o_material_mode),
    .o_vertex_alpha_o (cd_o_vertex_alpha),
    .o_frag_state_o   (cd_o_frag_state),
    .o_quality_tier_o (cd_o_quality_tier),
    .o_owner_o        (owner_o),

    .granted_o        (cd_granted_o),
    .switches_o       (cd_switches_o),
    .idle_offered_o   (cd_idle_offered),
    .err_hold_broken_o(cd_err_hold_broken_o)
  );

  logic mw_t_valid, mw_t_ready;
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] mw_drain_stall, mw_answer_stall, mw_occ_max, mw_sel_overflow,
               mw_clut_unowned;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_material_window #(
    .TMU_MODE_CLASS (8'b11_10_01_00),
    .OCCW           (8)
  ) u_window (
    .clk   (clk),
    .rst_n (rst_n),
    .t_valid_i        (cd_o_valid),
    .t_ready_o        (cd_o_ready),
    .t_material_set_i (cd_o_material_set),
    .t_material_id_i  (cd_o_material_id),
    .t_material_mode_i(cd_o_material_mode),
    .t_vertex_alpha_i (cd_o_vertex_alpha),
    .t_frag_state_i   (cd_o_frag_state),
    .t_quality_tier_i (cd_o_quality_tier),
    .t_valid_o        (mw_t_valid),
    .t_ready_i        (mw_t_ready),

    // THE SPAN'S THREE DISPOSAL EVENTS. `d_enter_i` is GEOM.CLIP's OWN accept,
    // not the window's handshake -- R197's door sits between the two and
    // CONSUMES a refused triangle without entering it. That is the composer's
    // wiring and getting it wrong here would hang the drain, which is exactly
    // the fault the composer's comment warns about.
    .d_enter_i  (cl_in_valid_o && cl_in_ready_i),
    .d_reject_i (d_reject_i),
    .d_leave_i  (d_leave_i),

    .req_valid_o        (req_valid_o),
    .req_ready_i        (req_ready_i),
    .req_material_set_o (req_material_set_o),
    .req_material_id_o  (req_material_id_o),
    .req_quality_tier_o (),
    .rsp_valid_i        (rsp_valid_i),
    .rsp_ready_o        (rsp_ready_o),
    .rsp_status_i       (3'd0),
    .rsp_has_record_i   (rsp_has_record_i),
    .rsp_sample_count_i (rsp_sample_count_i),
    .rsp_material_recipe_i  (rsp_material_recipe_i),
    .rsp_recipe_weight_i    (rsp_recipe_weight_i),
    .rsp_base_binding_i     (rsp_base_binding_i),
    .rsp_selector_overflow_i(1'b0),
    .rsp_sample0_modes_i    (rsp_sample0_modes_i),

    // ---- I20's fragment-state group, CONNECTED 2026-09-25 (EDGEPREP) ------
    // NOT this packet's work, and fixed here rather than reported, because
    // `zhao_material_window` gained these eight ports at `eca5b8d3` (FRAGSTATE
    // 2/n, earlier the same day) and this bench was not updated with it -- so
    // `verilate` failed with eight PINMISSING warnings and NO target in the
    // whole tree could configure. It is the trap CLAUDE.md names for
    // `zhao_prod_top`, one level down: production gained ports and an
    // instantiation of it did not.
    //
    // `rsp_frag_declared_i` LOW is FRAGSTATE's own documented compatibility
    // default -- the material declares no fragment state, so the primitive's
    // remains authoritative and this bench's behaviour is unchanged. The other
    // three are therefore don't-care and are tied to the profile defaults the
    // directive names (opaque alpha 255 is elsewhere; effect tag 0 here).
    // Choosing HIGH instead would silently hand this acceptance bench a
    // material-declared state it never asked for.
    .rsp_frag_declared_i (1'b0),
    .rsp_frag_state_i    (32'd0),
    .rsp_effect_tag_i    (8'd0),
    .rsp_stencil_ref_i   (8'd0),

    .pub_valid_o           (pub_valid_o),
    .pub_sample_count_o    (pub_sample_count_o),
    .pub_material_recipe_o (pub_material_recipe_o),
    .pub_recipe_weight_o   (pub_recipe_weight_o),
    .pub_base_binding_o    (pub_base_binding_o),
    .pub_response_class_o  (pub_response_class_o),
    .pub_material_mode_o   (pub_material_mode_o),
    .pub_vertex_alpha_o    (pub_vertex_alpha_w),
    .pub_frag_state_o      (pub_frag_state_o),

    // Explicitly OPEN rather than absent. An empty connection is a statement
    // that this bench does not observe the port; a MISSING one is a pin the
    // next port change will hide inside a wall of warnings.
    .pub_frag_declared_o   (),
    .pub_mat_frag_state_o  (),
    .pub_effect_tag_o      (),
    .pub_stencil_ref_o     (),

    .resolves_o                (mw_resolves_o),
    .switches_o                (mw_switches_o),
    .drain_stall_cycles_o      (mw_drain_stall),
    .answer_stall_cycles_o     (mw_answer_stall),
    .occupancy_max_o           (mw_occ_max),
    .no_record_o               (mw_no_record_o),
    .selector_overflow_o       (mw_sel_overflow),
    .clut_unowned_o            (mw_clut_unowned),
    .no_material_spans_o       (mw_no_material_spans_o),
    .mode_refused_o            (mw_mode_refused_o),
    .err_unpublished_o         (mw_err_unpublished_o),
    .err_occupancy_underflow_o (mw_err_underflow_o)
  );

  // ==========================================================================
  // R197's UNTEXTURED GATE -- THE COPY. See the header.
  // ==========================================================================
  wire cl_in_untex_c  = cd_o_untex;
  wire cl_in_refuse_c = cl_in_untex_c && (pub_sample_count_o != 2'd0);
  assign cl_in_valid_o = mw_t_valid && !cl_in_refuse_c;
  assign mw_t_ready    = cl_in_refuse_c || cl_in_ready_i;
  assign cl_untex_o    = cl_in_untex_c;
  assign cl_src_id_o   = cd_o_src_id;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      geom_untex_refused_o <= 32'd0;
    end else if (mw_t_valid && cl_in_refuse_c &&
                 (geom_untex_refused_o != 32'hffff_ffff)) begin
      geom_untex_refused_o <= geom_untex_refused_o + 32'd1;
    end
  end

endmodule

`default_nettype wire
