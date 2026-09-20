// zhao_view_eye.sv -- the two views' CAMERA POSITIONS, ruling R63.
//
// WHAT THIS IS FOR
//
// `SetView 0x0010` carries `mat4fx view_projection`, which is FUSED. TERRAIN.LOD
// measures screen-space error as a deviation divided by the distance from the
// EYE to a subpatch centre, in world metres, so it needs the camera position and
// not a matrix built from it. Recovering an eye from a fused view-projection is
// a 4x4 inverse, and nothing in this console ratifies one. R63's answer is to
// put the eye on the wire beside the matrix (`fx16 eye[3]`, +16 B on the record)
// and to lower it, like the depth profile before it, onto the projector
// configuration bus. This block is where those three words come to rest.
//
// WHY IT IS ITS OWN MODULE AND NOT THREE MORE REGISTERS IN `zhao_project_core`
//
// The projector's configuration bus already carries addresses 0..18 into
// `zhao_project_core` (matrix 0..15, viewport rect 16/17, depth profile 18), and
// the obvious home for 19..21 is beside them. It is the wrong home, for a reason
// that is about COST and not about tidiness:
//
//   * `zhao_project_core` is a LEAF with seven instantiation sites -- three in
//     production (`zhao_project_service`, `zhao_geom_project`,
//     `zhao_terrain_project`) and four benches -- plus a committed mutant copy.
//     CLAUDE.md: "A port on a leaf costs its WHOLE instantiation chain plus
//     every bench."
//   * NOT ONE of those readers wants the eye. The projector's arithmetic never
//     touches it; the matrix it belongs to was built from it upstream, by
//     software. Six output ports would thread a value through seven modules so
//     that one module elsewhere can read it.
//
// So this block SNOOPS the same bus. `zhao_project_core` ignores addresses it
// does not decode, and `zhao_geom_cull` (the bus's other reader) ignores
// everything from 16 up on purpose -- which is the same property that let the
// viewport rect and then the depth profile be added without a port change. The
// address map lives in ONE place, `zhao_project_core`'s port-list comment, and
// that comment names this file as the owner of 19..21 so the hole cannot be
// filled twice.
//
// WHAT IT IS NOT
//
// It is not a second writer of anything. It holds no matrix, performs no
// arithmetic (0 DSP), infers no memory (0 M10K) and back-pressures nothing:
// `cfg_we_i` is a pulse the executor owns, and a write lands the cycle it is
// offered. The refusal path on that bus (`proj_cfg_ready_i`) belongs to
// `zhao_proj_subsystem`'s two-writer arbitration and is settled before a write
// ever reaches here.
//
// ZERO IS THE ORIGIN, AND THAT IS A DECISION. Reset puts both eyes at
// (0, 0, 0) -- see `spec/commands.zidl`'s SetView comment. A console that never
// issues an eye therefore measures LOD distance from the world origin, which is
// a defined picture rather than an undefined one. Whether a view participates at
// all is the consumer's `cam*_en_i`, not a presence flag here, which is why no
// bit of `SetView.flags` was spent on one.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_view_eye #(
    // The three configuration addresses this block owns. NAMED AND EDITABLE
    // (CLAUDE.md rule 6): the projector bus has 32 addresses and spends 19, so
    // moving the eye is a parameter and not a rewrite. They must sit ABOVE
    // `zhao_project_core`'s highest decoded address (18) or two blocks answer
    // one write; the elaboration guard below says so.
    parameter int unsigned EYE_ADDR_X = 19,
    parameter int unsigned EYE_ADDR_Y = 20,
    parameter int unsigned EYE_ADDR_Z = 21,
    // The highest address `zhao_project_core` decodes today. Kept as a
    // parameter so the guard reads as a relationship between two blocks rather
    // than as a magic number.
    parameter int unsigned PROJ_TOP_ADDR = 18
) (
    input  wire clk,
    input  wire rst_n,

    // ---- the projector configuration bus, SNOOPED -------------------------
    // Identical wires to `zhao_project_core`'s `cfg_*`. This block never drives
    // them and never stalls them.
    input  wire        cfg_we_i,
    input  wire        cfg_view_i,
    input  wire [ 4:0] cfg_addr_i,
    input  wire [31:0] cfg_data_i,

    // ---- the two eyes, fx16 Q16.16 world metres ---------------------------
    // Registered configuration, not pipeline data: each output is whatever the
    // last SetView for that view wrote, and it changes only when one does.
    output logic signed [31:0] eye0_x_o,
    output logic signed [31:0] eye0_y_o,
    output logic signed [31:0] eye0_z_o,
    output logic signed [31:0] eye1_x_o,
    output logic signed [31:0] eye1_y_o,
    output logic signed [31:0] eye1_z_o
);

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there however clean the lint
  // (CLAUDE.md, "Verilator lint-clean is not Quartus-synthesizable"). And
  // `--lint-only` does not run this block, so it is a build-time guard and is
  // NOT claimed as a tested one by linting -- it is fired by parameter
  // override, and the override and its output are recorded in the findings.
  initial begin
    if (EYE_ADDR_X <= PROJ_TOP_ADDR || EYE_ADDR_Y <= PROJ_TOP_ADDR
        || EYE_ADDR_Z <= PROJ_TOP_ADDR)
      $fatal(1, "zhao_view_eye: an eye address collides with zhao_project_core's map");
    if (EYE_ADDR_X > 31 || EYE_ADDR_Y > 31 || EYE_ADDR_Z > 31)
      $fatal(1, "zhao_view_eye: cfg_addr_i is five bits; an eye address is out of range");
    if (EYE_ADDR_X == EYE_ADDR_Y || EYE_ADDR_Y == EYE_ADDR_Z
        || EYE_ADDR_X == EYE_ADDR_Z)
      $fatal(1, "zhao_view_eye: the three eye addresses must be distinct");
  end

  logic signed [31:0] ex [0:1];
  logic signed [31:0] ey [0:1];
  logic signed [31:0] ez [0:1];

  integer v;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (v = 0; v < 2; v = v + 1) begin
        ex[v] <= 32'sd0;
        ey[v] <= 32'sd0;
        ez[v] <= 32'sd0;
      end
    end else if (cfg_we_i) begin
      // One address, one register. A write to any other address is not this
      // block's business and is silently ignored -- exactly as
      // `zhao_project_core` ignores 19..21.
      if (cfg_addr_i == 5'(EYE_ADDR_X)) ex[cfg_view_i] <= $signed(cfg_data_i);
      else if (cfg_addr_i == 5'(EYE_ADDR_Y)) ey[cfg_view_i] <= $signed(cfg_data_i);
      else if (cfg_addr_i == 5'(EYE_ADDR_Z)) ez[cfg_view_i] <= $signed(cfg_data_i);
    end
  end

  assign eye0_x_o = ex[0];
  assign eye0_y_o = ey[0];
  assign eye0_z_o = ez[0];
  assign eye1_x_o = ex[1];
  assign eye1_y_o = ey[1];
  assign eye1_z_o = ez[1];

endmodule

`default_nettype wire
