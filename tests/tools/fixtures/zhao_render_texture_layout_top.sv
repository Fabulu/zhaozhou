// Exact Packet-A layout and round-trip fixture.  This is simulated, not merely
// linted, so the production guard's initial-block $fatal checks are live.
module zhao_render_texture_layout_top;
  import zhao_render_texture_pkg::*;
  import zhao_fragment_state_pkg::*;

  // ---- THE FRAGMENT STATE WORD's SECOND OPINION -----------------------------
  // The package declares this word as named LO/HI spans and builds every
  // profile through them; it deliberately carries NO packed struct, because
  // `tools/rtl/texture_v3_interface_parser.py` pins the island's duplicate-name
  // marker fingerprint and "a packed struct emits one MEMBERDTYPE per member".
  // A 14-member struct in the package moved the island's count 105 -> 119, and
  // that file's own 2026-09-18 note refused the identical trade at TWO markers.
  //
  // So the struct lives in the FIXTURE, which is where a layout probe belongs:
  // it is an INDEPENDENTLY WRITTEN statement of the same layout, and the checks
  // below set each field BY NAME and compare against the package's own spans.
  // If the two ever disagree, one of them is wrong and this fails -- which is
  // the entire value the struct was ever providing, obtained without making the
  // production closure's schema fingerprint noisier.
  typedef struct packed {
    logic [7:0] sten_mask;      // [31:24] masks EQUAL / NOTEQUAL COMPARES only
    logic [1:0] tag_channel;    // [23:22]
    logic       tag_from_texel; // [21]
    logic       tag_write_dis;  // [20]
    logic [1:0] sten_op;        // [19:18]
    logic [1:0] sten_func;      // [17:16]
    logic [7:0] atest_ref;      // [15:8]  an INDEX reference, not an alpha one
    logic       atest_en;       // [7]
    logic       alpha_mod;      // [6]
    logic       shade_mod;      // [5]
    logic [1:0] blend;          // [4:3]
    logic       z_force_far;    // [2]
    logic       z_write_dis;    // [1]
    logic       z_test_en;      // [0]
  } fixture_fragment_state_t;

  integer frag_span_control;
  integer frag_span_index;
  logic [FRAG_STATE_W-1:0] fs_probe;
  logic [FRAG_STATE_W-1:0] fs_expect;

  // The fixture's own span checker, with the same runtime positive control the
  // package's `expect_span` carries: corrupt the independently built expected
  // mask for exactly one named span and watch that one fail.
  task automatic expect_frag_span(input string name,
                                  input logic [FRAG_STATE_W-1:0] actual,
                                  input int unsigned lo,
                                  input int unsigned hi);
    begin
      frag_span_index = frag_span_index + 1;
      fs_expect = '0;
      for (int unsigned b = lo; b <= hi; b++) fs_expect[b] = 1'b1;
      if (frag_span_control == frag_span_index)
        fs_expect[lo] = ~fs_expect[lo];
      if (actual !== fs_expect) begin
        if (frag_span_control == frag_span_index)
          $fatal(1, "ZHAO_FRAG_STATE_SPAN_FIRE[%0d]: %s", frag_span_index, name);
        $fatal(1, "fragment-state span %s expected [%0d:%0d]", name, hi, lo);
      end
    end
  endtask

  zhao_render_texture_layout_guard u_layout_guard();
  // The fragment-state offset contract, executed in the plain run.
  zhao_fragment_state_guard u_frag_state_guard();

  zhao_raster_continuation_v2_t continuation;
  zhao_aux_surface_ctx_v2_t aux;
  zhao_texture_v3_request_v2_t request;
  zhao_raster_earlyz_payload_v2_t earlyz_payload;
  zhao_raster_pretex_v2_t pretex;
  zhao_raster_retire_ctx_v2_t retire_ctx;
  zhao_texture_result_v2_t result;

  logic [RASTER_CONTINUATION_W-1:0] continuation_bits;
  logic [AUX_SURFACE_CTX_W-1:0] aux_bits;
  logic [TEXTURE_V3_REQUEST_W-1:0] request_bits;
  logic [RASTER_EARLYZ_PAYLOAD_W-1:0] earlyz_payload_bits;
  logic [RASTER_PRETEX_W-1:0] pretex_bits;
  logic [RASTER_RETIRE_CTX_W-1:0] retire_ctx_bits;
  logic [TEXTURE_RESULT_W-1:0] result_bits;

  logic [RASTER_CONTINUATION_W-1:0] continuation_roundtrip_bits;
  logic [AUX_SURFACE_CTX_W-1:0] aux_roundtrip_bits;
  logic [RASTER_EARLYZ_PAYLOAD_W-1:0] earlyz_payload_roundtrip_bits;
  logic [RASTER_PRETEX_W-1:0] pretex_roundtrip_bits;
  logic [RASTER_RETIRE_CTX_W-1:0] retire_ctx_roundtrip_bits;
  logic [TEXTURE_RESULT_W-1:0] result_roundtrip_bits;
  fixture_fragment_state_t frag_state;
  logic [FRAG_STATE_W-1:0] frag_state_bits;
  logic [FRAG_STATE_W-1:0] frag_state_roundtrip_bits;
  integer roundtrip_control;

  initial begin
    roundtrip_control = 0;
    if ($value$plusargs("ROUNDTRIP_CONTROL=%d", roundtrip_control)) begin
      if ((roundtrip_control < 1) || (roundtrip_control > 7))
        $fatal(1, "invalid ROUNDTRIP_CONTROL=%0d", roundtrip_control);
    end
    continuation = '0;
    continuation.earlyz.in_tile_addr        = 8'hA5;
    continuation.earlyz.invw24              = 24'h12_3456;
    continuation.earlyz.fragment_state      = 32'h89AB_CDEF;
    continuation.earlyz.source_id           = 16'h1357;
    continuation.post_earlyz.vertex_rgb     = 24'h24_68AC;
    continuation.post_earlyz.vertex_alpha   = 8'h5A;
    continuation.post_earlyz.effect_tag     = 8'hC3;
    continuation.post_earlyz.stencil_reference = 8'h7E;

    aux = '0;
    aux.env_z1       = 32'shFEDC_BA98;
    aux.env_z0       = 32'sh7654_3210;
    aux.env_x1       = 32'sh89AB_CDEF;
    aux.env_x0       = 32'sh0123_4567;
    aux.sheet_handle = 32'hA1B2_C3D4;
    aux.wz           = 32'shD00D_F00D;
    aux.wx           = 32'sh1020_3040;

    request = '0;
    request.u_over_w              = 32'sh8102_0304;
    request.v_over_w              = 32'sh0506_0708;
    request.sample_count          = 2'b11;
    request.base_binding_selector = 8'h91;
    request.lod_q4_4              = 8'hA2;
    request.material_recipe       = 3'b101;
    request.recipe_weight         = 8'hB3;
    request.aux_required          = 1'b1;
    request.aux_surface_ctx       = aux;
    request.base_rgb              = 24'hC4_D5E6;
    request.base_alpha            = 8'hD7;
    request.response_class        = 2'b10;
    request.palette_slot          = 2'b01;
    request.palette_generation    = 8'hE8;

    earlyz_payload.raster_continuation = continuation.post_earlyz;
    earlyz_payload.texture_request     = request;
    pretex = make_raster_pretex(continuation, request);
    retire_ctx = make_raster_retire_ctx(32'h55AA_F00D, continuation);

    result.status            = 8'h81;
    result.sample0_raw_index = 8'h42;
    result.alpha             = 8'h7F;
    result.rgb               = 24'h12_34AB;

    // FOURTEEN FIELD-NAME PROBES against the package's own spans. Each sets
    // ONE field of the fixture's struct and asserts the bits that light up are
    // exactly the span the package names -- two independently written
    // statements of one layout, compared.
    frag_span_control = 0;
    frag_span_index = 0;
    if ($value$plusargs("FRAG_SPAN_CONTROL=%d", frag_span_control)) begin
      if ((frag_span_control < 1) || (frag_span_control > 14))
        $fatal(1, "invalid FRAG_SPAN_CONTROL=%0d", frag_span_control);
    end
    frag_state = '0; frag_state.z_test_en = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("z_test_en", fs_probe, FRAG_Z_TEST_EN_BIT, FRAG_Z_TEST_EN_BIT);
    frag_state = '0; frag_state.z_write_dis = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("z_write_dis", fs_probe, FRAG_Z_WRITE_DIS_BIT, FRAG_Z_WRITE_DIS_BIT);
    frag_state = '0; frag_state.z_force_far = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("z_force_far", fs_probe, FRAG_Z_FORCE_FAR_BIT, FRAG_Z_FORCE_FAR_BIT);
    frag_state = '0; frag_state.blend = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("blend", fs_probe, FRAG_BLEND_LO, FRAG_BLEND_HI);
    frag_state = '0; frag_state.shade_mod = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("shade_mod", fs_probe, FRAG_SHADE_MOD_BIT, FRAG_SHADE_MOD_BIT);
    frag_state = '0; frag_state.alpha_mod = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("alpha_mod", fs_probe, FRAG_ALPHA_MOD_BIT, FRAG_ALPHA_MOD_BIT);
    frag_state = '0; frag_state.atest_en = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("atest_en", fs_probe, FRAG_ATEST_EN_BIT, FRAG_ATEST_EN_BIT);
    frag_state = '0; frag_state.atest_ref = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("atest_ref", fs_probe, FRAG_ATEST_REF_LO, FRAG_ATEST_REF_HI);
    frag_state = '0; frag_state.sten_func = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("sten_func", fs_probe, FRAG_STEN_FUNC_LO, FRAG_STEN_FUNC_HI);
    frag_state = '0; frag_state.sten_op = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("sten_op", fs_probe, FRAG_STEN_OP_LO, FRAG_STEN_OP_HI);
    frag_state = '0; frag_state.tag_write_dis = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("tag_write_dis", fs_probe, FRAG_TAG_WRITE_DIS_BIT, FRAG_TAG_WRITE_DIS_BIT);
    frag_state = '0; frag_state.tag_from_texel = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("tag_from_texel", fs_probe, FRAG_TAG_FROM_TEXEL_BIT, FRAG_TAG_FROM_TEXEL_BIT);
    frag_state = '0; frag_state.tag_channel = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("tag_channel", fs_probe, FRAG_TAG_CHANNEL_LO, FRAG_TAG_CHANNEL_HI);
    frag_state = '0; frag_state.sten_mask = '1; fs_probe = FRAG_STATE_W'(frag_state);
    expect_frag_span("sten_mask", fs_probe, FRAG_STEN_MASK_LO, FRAG_STEN_MASK_HI);
    if (frag_span_index != 14)
      $fatal(1, "fragment-state span census expected 14, found %0d", frag_span_index);
    if (frag_span_control != 0)
      $fatal(1, "ZHAO_FRAG_STATE_SPAN_ESCAPED[%0d]", frag_span_control);

    // The fragment state word. The vector below is written as an independent
    // literal concatenation, so a struct that reordered two same-width
    // neighbours fails here as well as against the spans above.
    frag_state                = '0;
    frag_state.sten_mask      = 8'h3C;
    frag_state.tag_channel    = FRAG_TAG_CHANNEL_GLOW;
    frag_state.tag_from_texel = 1'b1;
    frag_state.tag_write_dis  = 1'b0;
    frag_state.sten_op        = FRAG_STEN_OP_INCR_SAT;
    frag_state.sten_func      = FRAG_STEN_FUNC_NOTEQUAL;
    frag_state.atest_ref      = 8'hE1;
    frag_state.atest_en       = 1'b1;
    frag_state.alpha_mod      = 1'b0;
    frag_state.shade_mod      = 1'b1;
    frag_state.blend          = FRAG_BLEND_ADD_MOD;
    frag_state.z_force_far    = 1'b0;
    frag_state.z_write_dis    = 1'b1;
    frag_state.z_test_en      = 1'b1;
    frag_state_bits = FRAG_STATE_W'(frag_state);

    continuation_bits = pack_raster_continuation(continuation);
    aux_bits = pack_aux_surface_ctx(aux);
    request_bits = request;
    earlyz_payload_bits = pack_earlyz_payload(earlyz_payload);
    pretex_bits = pack_raster_pretex(pretex);
    retire_ctx_bits = pack_raster_retire_ctx(retire_ctx);
    result_bits = pack_texture_result(result);

    // Independent runtime fire controls.  Corrupt only the packed operand fed
    // back to one unpacker; the expected typed value remains admission-time
    // state, so the two sides cannot move together and cancel the fault.
    continuation_roundtrip_bits = continuation_bits;
    aux_roundtrip_bits = aux_bits;
    earlyz_payload_roundtrip_bits = earlyz_payload_bits;
    pretex_roundtrip_bits = pretex_bits;
    retire_ctx_roundtrip_bits = retire_ctx_bits;
    result_roundtrip_bits = result_bits;
    if (roundtrip_control == 1)
      continuation_roundtrip_bits[CONT_SOURCE_ID_LO] =
          ~continuation_roundtrip_bits[CONT_SOURCE_ID_LO];
    if (roundtrip_control == 2)
      aux_roundtrip_bits[AUX_WX_LO] = ~aux_roundtrip_bits[AUX_WX_LO];
    if (roundtrip_control == 3)
      earlyz_payload_roundtrip_bits[EZPAY_VERTEX_RGB_LO] =
          ~earlyz_payload_roundtrip_bits[EZPAY_VERTEX_RGB_LO];
    if (roundtrip_control == 4)
      pretex_roundtrip_bits[PRETEX_SOURCE_ID_LO] =
          ~pretex_roundtrip_bits[PRETEX_SOURCE_ID_LO];
    if (roundtrip_control == 5)
      retire_ctx_roundtrip_bits[RETIRE_RASTER_SEQUENCE_LO] =
          ~retire_ctx_roundtrip_bits[RETIRE_RASTER_SEQUENCE_LO];
    if (roundtrip_control == 6)
      result_roundtrip_bits[TEXTURE_RESULT_STATUS_LO] =
          ~result_roundtrip_bits[TEXTURE_RESULT_STATUS_LO];
    frag_state_roundtrip_bits = frag_state_bits;
    if (roundtrip_control == 7)
      frag_state_roundtrip_bits[FRAG_STEN_FUNC_LO] =
          ~frag_state_roundtrip_bits[FRAG_STEN_FUNC_LO];

    if (continuation_bits !== {
          8'hA5, 24'h12_3456, 32'h89AB_CDEF, 16'h1357,
          24'h24_68AC, 8'h5A, 8'hC3, 8'h7E})
      $fatal(1, "continuation exact vector mismatch");
    if (aux_bits !== {
          32'hFEDC_BA98, 32'h7654_3210, 32'h89AB_CDEF, 32'h0123_4567,
          32'hA1B2_C3D4, 32'hD00D_F00D, 32'h1020_3040})
      $fatal(1, "AUX exact vector mismatch (low word must be wx, high word wz)");
    if (request_bits !== {
          32'h8102_0304, 32'h0506_0708, 2'b11, 8'h91, 8'hA2,
          3'b101, 8'hB3, 1'b1,
          32'hFEDC_BA98, 32'h7654_3210, 32'h89AB_CDEF, 32'h0123_4567,
          32'hA1B2_C3D4, 32'hD00D_F00D, 32'h1020_3040,
          24'hC4_D5E6, 8'hD7, 2'b10, 2'b01, 8'hE8})
      $fatal(1, "V3 request exact vector mismatch");
    if (earlyz_payload_bits !== {
          24'h24_68AC, 8'h5A, 8'hC3, 8'h7E, request_bits})
      $fatal(1, "Early-Z payload exact vector mismatch");
    if (pretex_bits !== {continuation_bits, request_bits})
      $fatal(1, "pretexture exact vector mismatch");
    if (pretex_bits[PRETEX_EARLYZ_PAYLOAD_HI:PRETEX_EARLYZ_PAYLOAD_LO]
          !== earlyz_payload_bits)
      $fatal(1, "pretexture/Early-Z payload splice mismatch");
    if (pack_raster_continuation(pretex_continuation(pretex))
          !== continuation_bits)
      $fatal(1, "pretexture continuation reconstruction mismatch");
    if (pretex_texture_request(pretex) !== request)
      $fatal(1, "pretexture request reconstruction mismatch");
    if (retire_ctx_bits !== {32'h55AA_F00D, continuation_bits})
      $fatal(1, "retirement context exact vector mismatch");
    if (result_bits !== {8'h81, 8'h42, 8'h7F, 24'h12_34AB})
      $fatal(1, "texture result exact vector mismatch");
    if (frag_state_bits !== {
          8'h3C, 2'b01, 1'b1, 1'b0, 2'b10, 2'b10, 8'hE1,
          1'b1, 1'b0, 1'b1, 2'b11, 1'b0, 1'b1, 1'b1})
      $fatal(1, "fragment state exact vector mismatch");
    // THE SIX RATIFIED RECIPES AND THE TWO PROFILES, as exact words. These are
    // the values `zref::FragmentPipeline`'s constructors pack, and a producer
    // declaring a profile gets exactly this. Written as literals so a recipe
    // whose field assignments drifted fails here rather than in a picture.
    if (frag_profile_opaque_geometry() !== 32'h0000_0000)
      $fatal(1, "profile opaque_geometry is not the all-zero opaque word");
    if (frag_profile_sky_backdrop() !== 32'h0004_0004)
      $fatal(1, "profile sky_backdrop mismatch");
    if (frag_profile_sky_cloud_fade() !== 32'h0014_004B)
      $fatal(1, "profile sky_cloud_fade mismatch");
    if (frag_profile_sun_additive() !== 32'h0044_001B)
      $fatal(1, "profile sun_additive mismatch");
    if (frag_profile_beam_additive_fade() !== 32'h0014_0033)
      $fatal(1, "profile beam_additive_fade mismatch");
    if (frag_profile_star_disc_masked() !== 32'h0064_0083)
      $fatal(1, "profile star_disc_masked mismatch");
    if (frag_profile_star_halo_additive() !== 32'h0064_0013)
      $fatal(1, "profile star_halo_additive mismatch");
    if (frag_profile_shadow_alpha() !== 32'h0014_000B)
      $fatal(1, "profile shadow_alpha mismatch");
    // EVERY PROFILE THAT READS AN ALPHA MUST NOT BE BLEND=REPLACE. This is the
    // structural form of the R89 defect: a flat alpha delivered under REPLACE
    // has its product thrown away by `zhao_raster_blend_fin`, so a profile that
    // means to consume an alpha and selects REPLACE is a silent no-op.
    if (frag_profile_shadow_alpha()[FRAG_BLEND_LO +: 2] == FRAG_BLEND_REPLACE)
      $fatal(1, "shadow_alpha selects BLEND=REPLACE: the alpha would be computed and thrown away");
    if (frag_profile_sky_cloud_fade()[FRAG_BLEND_LO +: 2] == FRAG_BLEND_REPLACE)
      $fatal(1, "sky_cloud_fade selects BLEND=REPLACE: the alpha would be computed and thrown away");

    if (unpack_raster_continuation(continuation_roundtrip_bits) !== continuation) begin
      if (roundtrip_control == 1)
        $fatal(1, "ZHAO_RENDER_TEXTURE_ROUNDTRIP_FIRE[1]: continuation");
      $fatal(1, "continuation pack/unpack round trip failed");
    end
    if (unpack_aux_surface_ctx(aux_roundtrip_bits) !== aux) begin
      if (roundtrip_control == 2)
        $fatal(1, "ZHAO_RENDER_TEXTURE_ROUNDTRIP_FIRE[2]: aux");
      $fatal(1, "AUX pack/unpack round trip failed");
    end
    if (unpack_earlyz_payload(earlyz_payload_roundtrip_bits) !== earlyz_payload) begin
      if (roundtrip_control == 3)
        $fatal(1, "ZHAO_RENDER_TEXTURE_ROUNDTRIP_FIRE[3]: earlyz_payload");
      $fatal(1, "Early-Z payload pack/unpack round trip failed");
    end
    if (unpack_raster_pretex(pretex_roundtrip_bits) !== pretex) begin
      if (roundtrip_control == 4)
        $fatal(1, "ZHAO_RENDER_TEXTURE_ROUNDTRIP_FIRE[4]: pretexture");
      $fatal(1, "pretexture pack/unpack round trip failed");
    end
    if (unpack_raster_retire_ctx(retire_ctx_roundtrip_bits) !== retire_ctx) begin
      if (roundtrip_control == 5)
        $fatal(1, "ZHAO_RENDER_TEXTURE_ROUNDTRIP_FIRE[5]: retirement");
      $fatal(1, "retirement context pack/unpack round trip failed");
    end
    if (unpack_texture_result(result_roundtrip_bits) !== result) begin
      if (roundtrip_control == 6)
        $fatal(1, "ZHAO_RENDER_TEXTURE_ROUNDTRIP_FIRE[6]: result");
      $fatal(1, "result pack/unpack round trip failed");
    end
    if (fixture_fragment_state_t'(frag_state_roundtrip_bits) !== frag_state) begin
      if (roundtrip_control == 7)
        $fatal(1, "ZHAO_RENDER_TEXTURE_ROUNDTRIP_FIRE[7]: frag_state");
      $fatal(1, "fragment state pack/unpack round trip failed");
    end
    if (roundtrip_control != 0)
      $fatal(1, "ZHAO_RENDER_TEXTURE_ROUNDTRIP_ESCAPED[%0d]", roundtrip_control);

    $display("ZHAO_RENDER_TEXTURE_LAYOUT_ROUNDTRIP_OK controls=7 frag_spans=14");
  end
endmodule

// Parameterized simulation fire control.  CONTROL=1..9 forces each literal
// width/offset family false independently.  CONTROL=10..15 flips one bit in
// every non-AUX layout fingerprint; the committed reversed-wx/wz mutant remains
// the independent AUX fingerprint control.  Each selected model must compile
// normally and then stop at its unique guard $fatal during execution.
module zhao_render_texture_elab_control_top #(
  parameter int unsigned CONTROL = 0
);
  import zhao_render_texture_pkg::*;
  import zhao_fragment_state_pkg::*;

  localparam logic [RASTER_CONTINUATION_W-1:0] CONT_ONE =
      {{(RASTER_CONTINUATION_W-1){1'b0}}, 1'b1};
  localparam logic [TEXTURE_V3_REQUEST_W-1:0] TEXREQ_ONE =
      {{(TEXTURE_V3_REQUEST_W-1){1'b0}}, 1'b1};
  localparam logic [RASTER_EARLYZ_PAYLOAD_W-1:0] EZPAY_ONE =
      {{(RASTER_EARLYZ_PAYLOAD_W-1){1'b0}}, 1'b1};
  localparam logic [RASTER_PRETEX_W-1:0] PRETEX_ONE =
      {{(RASTER_PRETEX_W-1){1'b0}}, 1'b1};
  localparam logic [RASTER_RETIRE_CTX_W-1:0] RETIRE_ONE =
      {{(RASTER_RETIRE_CTX_W-1){1'b0}}, 1'b1};
  localparam logic [TEXTURE_RESULT_W-1:0] RESULT_ONE =
      {{(TEXTURE_RESULT_W-1){1'b0}}, 1'b1};

  localparam logic [RASTER_CONTINUATION_W-1:0] CONT_PROBE =
      continuation_layout_probe() ^ ((CONTROL == 10) ? CONT_ONE : '0);
  localparam logic [TEXTURE_V3_REQUEST_W-1:0] TEXREQ_PROBE =
      texreq_layout_probe() ^ ((CONTROL == 11) ? TEXREQ_ONE : '0);
  localparam logic [RASTER_EARLYZ_PAYLOAD_W-1:0] EZPAY_PROBE =
      ezpay_layout_probe() ^ ((CONTROL == 12) ? EZPAY_ONE : '0);
  localparam logic [RASTER_PRETEX_W-1:0] PRETEX_PROBE =
      pretex_layout_probe() ^ ((CONTROL == 13) ? PRETEX_ONE : '0);
  localparam logic [RASTER_RETIRE_CTX_W-1:0] RETIRE_PROBE =
      retire_layout_probe() ^ ((CONTROL == 14) ? RETIRE_ONE : '0);
  localparam logic [TEXTURE_RESULT_W-1:0] RESULT_PROBE =
      result_layout_probe() ^ ((CONTROL == 15) ? RESULT_ONE : '0);

  zhao_render_texture_layout_guard #(
    .WIDTH_CONTRACT_OK_P(WIDTH_CONTRACT_OK && (CONTROL != 1)),
    .EARLYZ_OFFSET_CONTRACT_OK_P(EARLYZ_OFFSET_CONTRACT_OK && (CONTROL != 2)),
    .CONTINUATION_OFFSET_CONTRACT_OK_P(
      CONTINUATION_OFFSET_CONTRACT_OK && (CONTROL != 3)),
    .AUX_OFFSET_CONTRACT_OK_P(AUX_OFFSET_CONTRACT_OK && (CONTROL != 4)),
    .TEXREQ_OFFSET_CONTRACT_OK_P(TEXREQ_OFFSET_CONTRACT_OK && (CONTROL != 5)),
    .EZPAY_OFFSET_CONTRACT_OK_P(EZPAY_OFFSET_CONTRACT_OK && (CONTROL != 6)),
    .PRETEX_OFFSET_CONTRACT_OK_P(PRETEX_OFFSET_CONTRACT_OK && (CONTROL != 7)),
    .RETIRE_OFFSET_CONTRACT_OK_P(RETIRE_OFFSET_CONTRACT_OK && (CONTROL != 8)),
    .RESULT_OFFSET_CONTRACT_OK_P(RESULT_OFFSET_CONTRACT_OK && (CONTROL != 9)),
    .CONTINUATION_LAYOUT_PROBE_P(CONT_PROBE),
    .TEXREQ_LAYOUT_PROBE_P(TEXREQ_PROBE),
    .EZPAY_LAYOUT_PROBE_P(EZPAY_PROBE),
    .PRETEX_LAYOUT_PROBE_P(PRETEX_PROBE),
    .RETIRE_LAYOUT_PROBE_P(RETIRE_PROBE),
    .RESULT_LAYOUT_PROBE_P(RESULT_PROBE)
  ) u_control();

  // THE FRAGMENT STATE WORD's OWN GUARD, in its own module because its schema is
  // its own package -- `zhao_render_texture_pkg` is inside the texture island's
  // FROZEN interface manifest and had to stay byte-identical. CONTROL 17 forces
  // its offset contract false, exactly as 1..9 do for the render-texture ones.
  zhao_fragment_state_guard #(
    .FRAG_STATE_OFFSET_CONTRACT_OK_P(
      zhao_fragment_state_pkg::FRAG_STATE_OFFSET_CONTRACT_OK && (CONTROL != 17))
  ) u_frag_control();
endmodule
