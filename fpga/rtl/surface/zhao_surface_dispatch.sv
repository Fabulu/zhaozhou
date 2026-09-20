// zhao_surface_dispatch.sv -- SURFACE.STAMP's DISPATCH: the patch a stamp
// lands on, and the console policy it lands under. Core entry I30's open half.
//
// ENFORCED-BY: tests/surface/surface_dispatch_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT WAS MISSING, AND WHAT R45 RULED
// ---------------------------------------------------------------------------
// Entry I30 closed in two halves. The RATIFIED FIELDS closed when CMD.EXEC
// grew its SurfaceStamp arm: patch handle, operation, tag, strength, the
// transform's translation, radius and ring width all come off a packet
// CMD.DECODER has validated. What stayed open was
//
//   * `surf_cmd_env_*`, the patch ENVELOPE -- the world rectangle the stamp's
//     64x64 sheet is laid across -- which entry I27 recorded as "having no
//     owner anywhere in the tree"; and
//   * `cmd_blend_en` / `cmd_blend` / `cmd_age_shift`, console POLICY that no
//     opcode carries.
//
// Owner ruling R45 (2026-09-19): "The stamp's patch is resolved by the SAME
// world->patch law `zhao_terrain_heighttap` implements (powers-of-two pitch, no
// divider); the directory is keyed by the resulting patch coordinates. No
// second mapping law. blend_en=0 is the ratified policy."
//
// ---------------------------------------------------------------------------
// THE LAW, QUOTED RATHER THAN RE-DERIVED
// ---------------------------------------------------------------------------
// `zhao_terrain_place.sv` places a lattice point at
//
//     wx(i) = (patch_ix * 32 + i) <<< (16 + pitch_log2)
//
// and `zhao_terrain_heighttap.sv` inverts exactly that with an ARITHMETIC
// shift, because arithmetic right shift FLOORS and floor is the law -- its own
// header cites the trap: "a camera at x = -1 m is in patch -1, and C's `/`
// would put it in patch 0". `spec/terrain_rules.md` 1.3 froze the pitch set to
// powers of two for this reason: "no division, no rounding, anywhere in the
// addressing path".
//
// A patch is 32 CELLS across (33 lattice points), so the whole patch rectangle
// is one shift wider than a cell:
//
//     SH        = 16 + pitch_log2 + 5
//     patch_ix  = world_x >>> SH            (arithmetic: floors, including for
//                                            negative world coordinates)
//     env_x0    = patch_ix <<< SH           (= world_x with its low SH bits cleared)
//     env_x1    = (patch_ix + 1) <<< SH
//
// and the same on z. THAT IS THE WHOLE MAPPING, and it is deliberately not a
// second implementation of anything: `env_x0` is the world coordinate with its
// low bits cleared, which is the same value the tap's cell walk starts from.
// The stamp then lays its sheet across it by its own frozen rule
// (`wx = ex0 + ((ex1 - ex0) * (2i + 1)) / 128`).
//
// THE KEY THE DIRECTORY WANTS IS `{patch_ix, patch_iz}`, emitted here, so the
// mapping exists once. Nothing downstream re-derives it from the envelope.
//
// ---------------------------------------------------------------------------
// TWO FAULTS, BOTH COUNTED, NEITHER SILENT
// ---------------------------------------------------------------------------
//   * A PITCH OUTSIDE THE RATIFIED SET. `zhao_terrain_place` refuses it and
//     places nothing ("refused upstream; never placed"); the same answer is
//     given here -- a ZERO envelope, which the stamp reads as an empty
//     rectangle and covers no texel. `pitch_refused_o` counts it. A stamp that
//     lands somewhere plausible under an unratified pitch is exactly the
//     confident wrong number this tree has a chapter about.
//   * AN ENVELOPE THAT LEAVES s32. `env_x1` is `env_x0` plus one patch width,
//     so it can only overflow within one patch of the signed limit -- far
//     outside `zhao_surface_stamp`'s own stated input domain of +-4,096 world
//     metres. It is computed in 33 bits and SATURATED rather than wrapped,
//     because a wrapped envelope inverts the rectangle and the coverage test
//     then passes for the whole world. `env_clamped_o` counts it.
//
// ---------------------------------------------------------------------------
// THE POLICY IS THREE NAMED CONSTANTS, AND THEY ARE KNOBS
// ---------------------------------------------------------------------------
// R45 ratifies `blend_en = 0` -- the ABI mapping, `zhao_surface_stamp` S1's
// own default -- and says nothing about the other two, so they take the values
// that make an executor-issued stamp behave exactly as the ABI describes it:
// blend mode 0 and age shift 0. All three are PARAMETERS (CLAUDE.md rule 6):
// the owner moves them in one line, and nothing here derives them.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_surface_dispatch #(
    // Cells across one patch, as a shift. `spec/terrain_rules.md`: 33 lattice
    // points, 32 cells. It is a parameter so the elaboration check below has
    // something to check, not so it can be changed.
    parameter int unsigned PATCH_CELLS_LOG2 = 5,
    // fx16's fractional bits, the base of `zhao_terrain_place`'s shift.
    parameter int unsigned FX_SHIFT = 16,
    // ---- the console's stamp policy (R45). Named, editable, owner's. -------
    parameter logic       POLICY_BLEND_EN  = 1'b0,   // R45: the ABI mapping
    parameter logic [2:0] POLICY_BLEND     = 3'd0,
    parameter logic [2:0] POLICY_AGE_SHIFT = 3'd0
) (
    input var logic clk,
    input var logic rst_n,

    // The live pitch, from TERRAIN.HDRREAD's staged page header. Outside
    // [-1, 2] the placement is refused, exactly as `zhao_terrain_place` does.
    input  var logic signed [7:0]  pitch_log2_i,

    // The stamp's transform translation, in world fx16. Combinational: these
    // are the SAME wires the stamp itself accepts on, so the envelope and the
    // command can never be one cycle apart.
    input  var logic signed [31:0] cmd_tx_i,
    input  var logic signed [31:0] cmd_ty_i,      // world Z
    // The accept, for the counters only. Nothing here is sequenced by it.
    input  var logic               cmd_fire_i,

    // ---- the envelope, to `zhao_surface_stamp`'s `cmd_env_*` ---------------
    output var logic signed [31:0] env_x0_o,
    output var logic signed [31:0] env_z0_o,
    output var logic signed [31:0] env_x1_o,
    output var logic signed [31:0] env_z1_o,

    // ---- the directory key, for whoever keys on a patch --------------------
    output var logic signed [15:0] patch_ix_o,
    output var logic signed [15:0] patch_iz_o,
    output var logic               patch_valid_o,   // the pitch was ratified

    // ---- the policy --------------------------------------------------------
    output var logic               blend_en_o,
    output var logic [2:0]         blend_o,
    output var logic [2:0]         age_shift_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0] dispatched_o,       // stamps placed
    output var logic [31:0] pitch_refused_o,    // an unratified pitch
    output var logic [31:0] env_clamped_o       // the envelope left s32
);

  localparam int unsigned SH_MIN = FX_SHIFT + PATCH_CELLS_LOG2 - 1;  // pitch -1
  localparam int unsigned SH_MAX = FX_SHIFT + PATCH_CELLS_LOG2 + 2;  // pitch +2

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`.
  initial begin
    if (PATCH_CELLS_LOG2 != 5)
      $fatal(1, "zhao_surface_dispatch: PATCH_CELLS_LOG2=%0d; terrain_rules 1.3 says 32 cells",
             PATCH_CELLS_LOG2);
    if (SH_MAX > 30)
      $fatal(1, "zhao_surface_dispatch: SH_MAX=%0d leaves no room for a patch index", SH_MAX);
  end

  // ---- the pitch, and the shift it names ---------------------------------
  // The same test `zhao_terrain_heighttap` makes, on the same range, so the
  // two blocks cannot disagree about which pitches exist.
  wire pitch_ok_c = (pitch_log2_i >= -8'sd1) && (pitch_log2_i <= 8'sd2);
  logic [4:0] sh_c;
  always_comb begin
    sh_c = pitch_ok_c ? 5'(int'(FX_SHIFT) + int'(PATCH_CELLS_LOG2) + int'(pitch_log2_i))
                      : 5'(SH_MIN);
  end

  // ---- the mapping -------------------------------------------------------
  // 33 bits on the high side, so the upper edge is computed before it is
  // judged rather than after it has already wrapped.
  logic signed [31:0] ix_c, iz_c;
  logic signed [32:0] x0_c, z0_c, x1_c, z1_c;
  always_comb begin
    ix_c = cmd_tx_i >>> sh_c;          // arithmetic: FLOORS, which is the law
    iz_c = cmd_ty_i >>> sh_c;
    x0_c = 33'(ix_c) <<< sh_c;
    z0_c = 33'(iz_c) <<< sh_c;
    x1_c = (33'(ix_c) + 33'sd1) <<< sh_c;
    z1_c = (33'(iz_c) + 33'sd1) <<< sh_c;
  end

  // An s33 value that does not fit s32: its top two bits disagree. Written as
  // an expression per corner rather than a function, because a function taking
  // the whole word reads as thirty-one unused bits to the linter -- which is
  // true, and is the point.
  wire over_c = (x0_c[32] != x0_c[31]) || (z0_c[32] != z0_c[31]) ||
                (x1_c[32] != x1_c[31]) || (z1_c[32] != z1_c[31]);

  function automatic logic signed [31:0] sat32(input logic signed [32:0] v);
    begin
      if (v[32] != v[31]) sat32 = v[32] ? 32'sh8000_0000 : 32'sh7FFF_FFFF;
      else                sat32 = v[31:0];
    end
  endfunction

  // A refused pitch places NOTHING: the zero rectangle covers no texel, which
  // is `zhao_terrain_place`'s own answer to the same input.
  always_comb begin
    if (!pitch_ok_c || over_c) begin
      // `over_c` still SATURATES rather than zeroing: the rectangle stays
      // ordered (x0 <= x1) so the coverage test keeps its meaning, and the
      // counter says the number is not exact.
      env_x0_o = pitch_ok_c ? sat32(x0_c) : 32'sd0;
      env_z0_o = pitch_ok_c ? sat32(z0_c) : 32'sd0;
      env_x1_o = pitch_ok_c ? sat32(x1_c) : 32'sd0;
      env_z1_o = pitch_ok_c ? sat32(z1_c) : 32'sd0;
    end else begin
      env_x0_o = x0_c[31:0];
      env_z0_o = z0_c[31:0];
      env_x1_o = x1_c[31:0];
      env_z1_o = z1_c[31:0];
    end
  end

  assign patch_ix_o    = ix_c[15:0];
  assign patch_iz_o    = iz_c[15:0];
  assign patch_valid_o = pitch_ok_c;

  assign blend_en_o  = POLICY_BLEND_EN;
  assign blend_o     = POLICY_BLEND;
  assign age_shift_o = POLICY_AGE_SHIFT;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      dispatched_o    <= 32'd0;
      pitch_refused_o <= 32'd0;
      env_clamped_o   <= 32'd0;
    end else if (cmd_fire_i) begin
      dispatched_o <= dispatched_o + 32'd1;
      if (!pitch_ok_c)     pitch_refused_o <= pitch_refused_o + 32'd1;
      else if (over_c)     env_clamped_o   <= env_clamped_o + 32'd1;
    end
  end

endmodule : zhao_surface_dispatch

`default_nettype wire
