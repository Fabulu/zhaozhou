// COMMITTED WRONG-LAYOUT FIRE CONTROL.
//
// One substantive mutation from zhao_aux_surface_ctx_v2_t: the final `{wz, wx}`
// declaration is reversed to `{wx, wz}`.  Width checks still pass at 224 bits;
// only an independently pinned named-offset check can catch it.  The guard's
// parameterized static contract is then executed in simulation and must issue
// the unique AUX-layout fatal label.
module zhao_render_texture_wrong_layout_mutant;
  import zhao_render_texture_pkg::*;

  typedef struct packed {
    logic signed [31:0] env_z1;
    logic signed [31:0] env_z0;
    logic signed [31:0] env_x1;
    logic signed [31:0] env_x0;
    logic        [31:0] sheet_handle;
    logic signed [31:0] wx; // MUTATION: correct declaration has wz here
    logic signed [31:0] wz; // MUTATION: correct declaration has wx here
  } wrong_aux_surface_ctx_t;

  function automatic logic [AUX_SURFACE_CTX_W-1:0] wrong_aux_layout_probe();
    wrong_aux_surface_ctx_t value;
    value = '0;
    value.env_z1 = 32'shFEDC_BA98;
    value.env_z0 = 32'sh7654_3210;
    value.env_x1 = 32'sh89AB_CDEF;
    value.env_x0 = 32'sh0123_4567;
    value.sheet_handle = 32'hA1B2_C3D4;
    value.wz = 32'shD00D_F00D;
    value.wx = 32'sh1020_3040;
    return value;
  endfunction

  localparam logic [AUX_SURFACE_CTX_W-1:0] WRONG_AUX_LAYOUT_PROBE =
      wrong_aux_layout_probe();

  zhao_render_texture_layout_guard #(
    .AUX_LAYOUT_PROBE_P(WRONG_AUX_LAYOUT_PROBE)
  ) u_control_guard();
endmodule
