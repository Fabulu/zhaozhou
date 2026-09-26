// zhao_render_texture_pkg.sv
//
// Packet A of the shell/texture V3 composition architecture: types and
// instruments only.  Nothing in the selected hierarchy consumes these types.
//
// Packed-struct declarations run MSB to LSB.  Every externally relevant span
// also has a named inclusive LO/HI pair below; consumers must use the fields or
// those names rather than handwritten numeric slices.
package zhao_render_texture_pkg;

  // ---- ratified packet widths ------------------------------------------------
  localparam int unsigned RASTER_EARLYZ_KEY_W          = 80;
  localparam int unsigned RASTER_CONTINUATION_TAIL_W   = 48;
  localparam int unsigned TEXTURE_V3_REQUEST_W         = 363;
  localparam int unsigned RASTER_CONTINUATION_W        = 128;
  localparam int unsigned AUX_SURFACE_CTX_W            = 224;
  localparam int unsigned RASTER_PRETEX_W              = 491;
  localparam int unsigned RASTER_EARLYZ_PAYLOAD_W      = 411;
  localparam int unsigned RASTER_RETIRE_CTX_W          = 160;
  localparam int unsigned TEXTURE_RESULT_W             = 48;

  // ---- the 80 explicit Early-Z bits -----------------------------------------
  localparam int unsigned EARLYZ_SOURCE_ID_LO          = 0;
  localparam int unsigned EARLYZ_SOURCE_ID_HI          = 15;
  localparam int unsigned EARLYZ_FRAGMENT_STATE_LO     = 16;
  localparam int unsigned EARLYZ_FRAGMENT_STATE_HI     = 47;
  localparam int unsigned EARLYZ_INVW24_LO             = 48;
  localparam int unsigned EARLYZ_INVW24_HI             = 71;
  localparam int unsigned EARLYZ_IN_TILE_ADDR_LO       = 72;
  localparam int unsigned EARLYZ_IN_TILE_ADDR_HI       = 79;

  typedef struct packed {
    logic  [7:0] in_tile_addr;   // [79:72]
    logic [23:0] invw24;         // [71:48]
    logic [31:0] fragment_state; // [47:16]
    logic [15:0] source_id;      // [15:0]
  } zhao_raster_earlyz_key_v2_t;

  // ---- the 48 continuation bits carried opaquely through Early-Z ------------
  localparam int unsigned CONT_TAIL_STENCIL_REFERENCE_LO = 0;
  localparam int unsigned CONT_TAIL_STENCIL_REFERENCE_HI = 7;
  localparam int unsigned CONT_TAIL_EFFECT_TAG_LO         = 8;
  localparam int unsigned CONT_TAIL_EFFECT_TAG_HI         = 15;
  localparam int unsigned CONT_TAIL_VERTEX_ALPHA_LO       = 16;
  localparam int unsigned CONT_TAIL_VERTEX_ALPHA_HI       = 23;
  localparam int unsigned CONT_TAIL_VERTEX_RGB_LO         = 24;
  localparam int unsigned CONT_TAIL_VERTEX_RGB_HI         = 47;

  typedef struct packed {
    logic [23:0] vertex_rgb;        // [47:24]
    logic  [7:0] vertex_alpha;      // [23:16]
    logic  [7:0] effect_tag;        // [15:8]
    logic  [7:0] stencil_reference; // [7:0]
  } zhao_raster_continuation_tail_v2_t;

  // ---- the complete 128-bit raster continuation -----------------------------
  localparam int unsigned CONT_STENCIL_REFERENCE_LO    = 0;
  localparam int unsigned CONT_STENCIL_REFERENCE_HI    = 7;
  localparam int unsigned CONT_EFFECT_TAG_LO            = 8;
  localparam int unsigned CONT_EFFECT_TAG_HI            = 15;
  localparam int unsigned CONT_VERTEX_ALPHA_LO          = 16;
  localparam int unsigned CONT_VERTEX_ALPHA_HI          = 23;
  localparam int unsigned CONT_VERTEX_RGB_LO            = 24;
  localparam int unsigned CONT_VERTEX_RGB_HI            = 47;
  localparam int unsigned CONT_SOURCE_ID_LO             = 48;
  localparam int unsigned CONT_SOURCE_ID_HI             = 63;
  localparam int unsigned CONT_FRAGMENT_STATE_LO        = 64;
  localparam int unsigned CONT_FRAGMENT_STATE_HI        = 95;
  localparam int unsigned CONT_INVW24_LO                = 96;
  localparam int unsigned CONT_INVW24_HI                = 119;
  localparam int unsigned CONT_IN_TILE_ADDR_LO          = 120;
  localparam int unsigned CONT_IN_TILE_ADDR_HI          = 127;

  typedef struct packed {
    zhao_raster_earlyz_key_v2_t          earlyz;
    zhao_raster_continuation_tail_v2_t   post_earlyz;
  } zhao_raster_continuation_v2_t;

  // ---- owner-sealed 224-bit terrain AUX context -----------------------------
  // Exact low-word law: low 32 = world X, high 32 = world Z.  The packed
  // declaration is MSB to LSB, so the final two fields are {wz, wx}.
  localparam int unsigned AUX_WX_LO                  = 0;
  localparam int unsigned AUX_WX_HI                  = 31;
  localparam int unsigned AUX_WZ_LO                  = 32;
  localparam int unsigned AUX_WZ_HI                  = 63;
  localparam int unsigned AUX_SHEET_HANDLE_LO        = 64;
  localparam int unsigned AUX_SHEET_HANDLE_HI        = 95;
  localparam int unsigned AUX_ENV_X0_LO              = 96;
  localparam int unsigned AUX_ENV_X0_HI              = 127;
  localparam int unsigned AUX_ENV_X1_LO              = 128;
  localparam int unsigned AUX_ENV_X1_HI              = 159;
  localparam int unsigned AUX_ENV_Z0_LO              = 160;
  localparam int unsigned AUX_ENV_Z0_HI              = 191;
  localparam int unsigned AUX_ENV_Z1_LO              = 192;
  localparam int unsigned AUX_ENV_Z1_HI              = 223;

  // sheet_handle is {index[23:0], generation[7:0]}.
  localparam int unsigned AUX_SHEET_GENERATION_LO    = 64;
  localparam int unsigned AUX_SHEET_GENERATION_HI    = 71;
  localparam int unsigned AUX_SHEET_INDEX_LO         = 72;
  localparam int unsigned AUX_SHEET_INDEX_HI         = 95;

  typedef struct packed {
    logic signed [31:0] env_z1;       // [223:192]
    logic signed [31:0] env_z0;       // [191:160]
    logic signed [31:0] env_x1;       // [159:128]
    logic signed [31:0] env_x0;       // [127:96]
    logic        [31:0] sheet_handle; // [95:64]
    logic signed [31:0] wz;           // [63:32]
    logic signed [31:0] wx;           // [31:0]
  } zhao_aux_surface_ctx_v2_t;

  // ---- the 362-bit V3 request portion ---------------------------------------
  localparam int unsigned TEXREQ_PALETTE_GENERATION_LO = 0;
  localparam int unsigned TEXREQ_PALETTE_GENERATION_HI = 7;
  localparam int unsigned TEXREQ_PALETTE_SLOT_LO       = 8;
  localparam int unsigned TEXREQ_PALETTE_SLOT_HI       = 9;
  localparam int unsigned TEXREQ_RESPONSE_CLASS_LO     = 10;
  localparam int unsigned TEXREQ_RESPONSE_CLASS_HI     = 11;
  localparam int unsigned TEXREQ_BASE_ALPHA_LO         = 12;
  localparam int unsigned TEXREQ_BASE_ALPHA_HI         = 19;
  localparam int unsigned TEXREQ_BASE_RGB_LO           = 20;
  localparam int unsigned TEXREQ_BASE_RGB_HI           = 43;
  localparam int unsigned TEXREQ_AUX_SURFACE_CTX_LO    = 44;
  localparam int unsigned TEXREQ_AUX_SURFACE_CTX_HI    = 267;
  localparam int unsigned TEXREQ_AUX_REQUIRED_LO       = 268;
  localparam int unsigned TEXREQ_AUX_REQUIRED_HI       = 268;
  localparam int unsigned TEXREQ_RECIPE_WEIGHT_LO      = 269;
  localparam int unsigned TEXREQ_RECIPE_WEIGHT_HI      = 276;
  localparam int unsigned TEXREQ_MATERIAL_RECIPE_LO    = 277;
  localparam int unsigned TEXREQ_MATERIAL_RECIPE_HI    = 279;
  localparam int unsigned TEXREQ_LOD_Q4_4_LO           = 280;
  localparam int unsigned TEXREQ_LOD_Q4_4_HI           = 287;
  localparam int unsigned TEXREQ_BASE_BINDING_LO       = 288;
  localparam int unsigned TEXREQ_BASE_BINDING_HI       = 295;
  localparam int unsigned TEXREQ_SAMPLE_COUNT_LO       = 296;
  localparam int unsigned TEXREQ_SAMPLE_COUNT_HI       = 297;
  localparam int unsigned TEXREQ_V_OVER_W_LO           = 298;
  localparam int unsigned TEXREQ_V_OVER_W_HI           = 329;
  localparam int unsigned TEXREQ_U_OVER_W_LO           = 330;
  localparam int unsigned TEXREQ_U_OVER_W_HI           = 361;
  // TERRAIN.NORMALMAP's per-fragment declaration (NORMALMAP, 2026-09-26).
  // PLACED AT THE TOP SO NO EXISTING OFFSET MOVES: every `TEXREQ_*` above is
  // bit-identical to the 362-bit layout, and only the totals change. A field
  // inserted lower would have shifted eleven frozen offsets and every capture
  // that decodes them, for no gain.
  localparam int unsigned TEXREQ_DETAIL_REQUIRED_LO     = 362;
  localparam int unsigned TEXREQ_DETAIL_REQUIRED_HI     = 362;

  typedef struct packed {
    // WHY IT RIDES THE REQUEST AND NOT A SIDE WIRE. The declaration is made per
    // TRIANGLE at the door and consumed per FRAGMENT several stages later, with
    // Early-Z and a two-entry skid in between. `zhao_raster_tile_pipe_v2` builds
    // this field in the SAME `always_comb` that builds `post_earlyz`, from the
    // same per-triangle register, so it traverses Early-Z inside the payload the
    // fragment carries. A parallel one-bit pipeline beside it would separate on
    // the first stall and hand one triangle's declaration to another's fragment
    // with every counter balancing -- which is CLAUDE.md's metadata-swap
    // chapter, and is the reason `post_earlyz` is carried this way already.
    logic                        detail_required;      // [362]
    logic signed [31:0]          u_over_w;             // [361:330]
    logic signed [31:0]          v_over_w;             // [329:298]
    logic         [1:0]          sample_count;         // [297:296]
    logic         [7:0]          base_binding_selector;// [295:288]
    logic         [7:0]          lod_q4_4;             // [287:280]
    logic         [2:0]          material_recipe;      // [279:277]
    logic         [7:0]          recipe_weight;        // [276:269]
    logic                        aux_required;         // [268]
    zhao_aux_surface_ctx_v2_t    aux_surface_ctx;      // [267:44]
    logic        [23:0]          base_rgb;             // [43:20]
    logic         [7:0]          base_alpha;           // [19:12]
    logic         [1:0]          response_class;       // [11:10]
    logic         [1:0]          palette_slot;         // [9:8]
    logic         [7:0]          palette_generation;   // [7:0]
  } zhao_texture_v3_request_v2_t;

  // ---- the 410-bit opaque Early-Z payload -----------------------------------
  localparam int unsigned EZPAY_TEXTURE_REQUEST_LO       = 0;
  localparam int unsigned EZPAY_TEXTURE_REQUEST_HI       = 361;
  // THE REQUEST GREW ONE BIT AT ITS TOP (NORMALMAP, 2026-09-26), so every
  // offset ABOVE it moves by exactly one and nothing below it moves at all.
  localparam int unsigned EZPAY_DETAIL_REQUIRED_LO        = 362;
  localparam int unsigned EZPAY_DETAIL_REQUIRED_HI        = 362;
  localparam int unsigned EZPAY_STENCIL_REFERENCE_LO     = 363;
  localparam int unsigned EZPAY_STENCIL_REFERENCE_HI     = 370;
  localparam int unsigned EZPAY_EFFECT_TAG_LO             = 371;
  localparam int unsigned EZPAY_EFFECT_TAG_HI             = 378;
  localparam int unsigned EZPAY_VERTEX_ALPHA_LO           = 379;
  localparam int unsigned EZPAY_VERTEX_ALPHA_HI           = 386;
  localparam int unsigned EZPAY_VERTEX_RGB_LO             = 387;
  localparam int unsigned EZPAY_VERTEX_RGB_HI             = 410;

  typedef struct packed {
    zhao_raster_continuation_tail_v2_t raster_continuation;
    zhao_texture_v3_request_v2_t       texture_request;
  } zhao_raster_earlyz_payload_v2_t;

  // ---- the complete 490-bit post-coverage/pretexture packet -----------------
  localparam int unsigned PRETEX_PALETTE_GENERATION_LO = 0;
  localparam int unsigned PRETEX_PALETTE_GENERATION_HI = 7;
  localparam int unsigned PRETEX_PALETTE_SLOT_LO       = 8;
  localparam int unsigned PRETEX_PALETTE_SLOT_HI       = 9;
  localparam int unsigned PRETEX_RESPONSE_CLASS_LO     = 10;
  localparam int unsigned PRETEX_RESPONSE_CLASS_HI     = 11;
  localparam int unsigned PRETEX_BASE_ALPHA_LO         = 12;
  localparam int unsigned PRETEX_BASE_ALPHA_HI         = 19;
  localparam int unsigned PRETEX_BASE_RGB_LO           = 20;
  localparam int unsigned PRETEX_BASE_RGB_HI           = 43;
  localparam int unsigned PRETEX_AUX_SURFACE_CTX_LO    = 44;
  localparam int unsigned PRETEX_AUX_SURFACE_CTX_HI    = 267;
  localparam int unsigned PRETEX_AUX_REQUIRED_LO       = 268;
  localparam int unsigned PRETEX_AUX_REQUIRED_HI       = 268;
  localparam int unsigned PRETEX_RECIPE_WEIGHT_LO      = 269;
  localparam int unsigned PRETEX_RECIPE_WEIGHT_HI      = 276;
  localparam int unsigned PRETEX_MATERIAL_RECIPE_LO    = 277;
  localparam int unsigned PRETEX_MATERIAL_RECIPE_HI    = 279;
  localparam int unsigned PRETEX_LOD_Q4_4_LO           = 280;
  localparam int unsigned PRETEX_LOD_Q4_4_HI           = 287;
  localparam int unsigned PRETEX_BASE_BINDING_LO       = 288;
  localparam int unsigned PRETEX_BASE_BINDING_HI       = 295;
  localparam int unsigned PRETEX_SAMPLE_COUNT_LO       = 296;
  localparam int unsigned PRETEX_SAMPLE_COUNT_HI       = 297;
  localparam int unsigned PRETEX_V_OVER_W_LO           = 298;
  localparam int unsigned PRETEX_V_OVER_W_HI           = 329;
  localparam int unsigned PRETEX_U_OVER_W_LO           = 330;
  localparam int unsigned PRETEX_U_OVER_W_HI           = 361;
  localparam int unsigned PRETEX_STENCIL_REFERENCE_LO  = 362;
  localparam int unsigned PRETEX_STENCIL_REFERENCE_HI  = 369;
  localparam int unsigned PRETEX_EFFECT_TAG_LO          = 370;
  localparam int unsigned PRETEX_EFFECT_TAG_HI          = 377;
  localparam int unsigned PRETEX_VERTEX_ALPHA_LO        = 378;
  localparam int unsigned PRETEX_VERTEX_ALPHA_HI        = 385;
  localparam int unsigned PRETEX_VERTEX_RGB_LO          = 386;
  localparam int unsigned PRETEX_VERTEX_RGB_HI          = 409;
  localparam int unsigned PRETEX_SOURCE_ID_LO           = 410;
  localparam int unsigned PRETEX_SOURCE_ID_HI           = 425;
  localparam int unsigned PRETEX_FRAGMENT_STATE_LO      = 426;
  localparam int unsigned PRETEX_FRAGMENT_STATE_HI      = 457;
  localparam int unsigned PRETEX_INVW24_LO              = 458;
  localparam int unsigned PRETEX_INVW24_HI              = 481;
  localparam int unsigned PRETEX_IN_TILE_ADDR_LO        = 482;
  localparam int unsigned PRETEX_IN_TILE_ADDR_HI        = 489;
  localparam int unsigned PRETEX_EARLYZ_PAYLOAD_LO      = 0;
  localparam int unsigned PRETEX_EARLYZ_PAYLOAD_HI      = 409;
  localparam int unsigned PRETEX_EARLYZ_KEY_LO          = 411;
  localparam int unsigned PRETEX_EARLYZ_KEY_HI          = 490;
  localparam int unsigned PRETEX_CONTINUATION_LO        = 363;
  localparam int unsigned PRETEX_CONTINUATION_HI        = 490;
  localparam int unsigned PRETEX_TEXTURE_REQUEST_LO     = 0;
  localparam int unsigned PRETEX_TEXTURE_REQUEST_HI     = 361;

  typedef struct packed {
    zhao_raster_earlyz_key_v2_t      earlyz;
    zhao_raster_earlyz_payload_v2_t  payload;
  } zhao_raster_pretex_v2_t;

  // ---- the 160-bit owner-carried retirement context -------------------------
  // The packed declaration is exactly {raster_sequence, raster_continuation}.
  localparam int unsigned RETIRE_STENCIL_REFERENCE_LO = 0;
  localparam int unsigned RETIRE_STENCIL_REFERENCE_HI = 7;
  localparam int unsigned RETIRE_EFFECT_TAG_LO         = 8;
  localparam int unsigned RETIRE_EFFECT_TAG_HI         = 15;
  localparam int unsigned RETIRE_VERTEX_ALPHA_LO       = 16;
  localparam int unsigned RETIRE_VERTEX_ALPHA_HI       = 23;
  localparam int unsigned RETIRE_VERTEX_RGB_LO         = 24;
  localparam int unsigned RETIRE_VERTEX_RGB_HI         = 47;
  localparam int unsigned RETIRE_SOURCE_ID_LO          = 48;
  localparam int unsigned RETIRE_SOURCE_ID_HI          = 63;
  localparam int unsigned RETIRE_FRAGMENT_STATE_LO     = 64;
  localparam int unsigned RETIRE_FRAGMENT_STATE_HI     = 95;
  localparam int unsigned RETIRE_INVW24_LO             = 96;
  localparam int unsigned RETIRE_INVW24_HI             = 119;
  localparam int unsigned RETIRE_IN_TILE_ADDR_LO       = 120;
  localparam int unsigned RETIRE_IN_TILE_ADDR_HI       = 127;
  localparam int unsigned RETIRE_CONTINUATION_LO       = 0;
  localparam int unsigned RETIRE_CONTINUATION_HI       = 127;
  localparam int unsigned RETIRE_RASTER_SEQUENCE_LO    = 128;
  localparam int unsigned RETIRE_RASTER_SEQUENCE_HI    = 159;

  typedef struct packed {
    logic [31:0]                     raster_sequence;     // [159:128]
    zhao_raster_continuation_v2_t    raster_continuation; // [127:0]
  } zhao_raster_retire_ctx_v2_t;

  // ---- the complete 48-bit V3 terminal result -------------------------------
  localparam int unsigned TEXTURE_RESULT_RGB_LO          = 0;
  localparam int unsigned TEXTURE_RESULT_RGB_HI          = 23;
  localparam int unsigned TEXTURE_RESULT_ALPHA_LO        = 24;
  localparam int unsigned TEXTURE_RESULT_ALPHA_HI        = 31;
  localparam int unsigned TEXTURE_RESULT_SAMPLE0_INDEX_LO= 32;
  localparam int unsigned TEXTURE_RESULT_SAMPLE0_INDEX_HI= 39;
  localparam int unsigned TEXTURE_RESULT_STATUS_LO       = 40;
  localparam int unsigned TEXTURE_RESULT_STATUS_HI       = 47;
  localparam int unsigned TEXTURE_STATUS_SOURCE_REFUSED_BIT = 0;
  localparam int unsigned TEXTURE_STATUS_RESERVED_LO     = 1;
  localparam int unsigned TEXTURE_STATUS_RESERVED_HI     = 7;
  localparam int unsigned TEXTURE_RESULT_SOURCE_REFUSED_BIT = 40;
  localparam int unsigned TEXTURE_RESULT_RESERVED_LO     = 41;
  localparam int unsigned TEXTURE_RESULT_RESERVED_HI     = 47;

  typedef struct packed {
    logic  [7:0] status;            // [47:40], bit 0 = SOURCE_REFUSED
    logic  [7:0] sample0_raw_index; // [39:32]
    logic  [7:0] alpha;             // [31:24]
    logic [23:0] rgb;               // [23:0]
  } zhao_texture_result_v2_t;

  // Literal pins for every width and named offset.  These are deliberately not
  // derived from neighbouring constants: changing a declaration and its helper
  // constants in lockstep must still make the elaboration guard fail.
  localparam bit WIDTH_CONTRACT_OK =
      (RASTER_EARLYZ_KEY_W == 80) &&
      (RASTER_CONTINUATION_TAIL_W == 48) &&
      (TEXTURE_V3_REQUEST_W == 363) &&
      (RASTER_CONTINUATION_W == 128) &&
      (AUX_SURFACE_CTX_W == 224) &&
      (RASTER_PRETEX_W == 491) &&
      (RASTER_EARLYZ_PAYLOAD_W == 411) &&
      (RASTER_RETIRE_CTX_W == 160) &&
      (TEXTURE_RESULT_W == 48) &&
      ($bits(zhao_raster_earlyz_key_v2_t) == 80) &&
      ($bits(zhao_raster_continuation_tail_v2_t) == 48) &&
      ($bits(zhao_texture_v3_request_v2_t) == 363) &&
      ($bits(zhao_raster_continuation_v2_t) == 128) &&
      ($bits(zhao_aux_surface_ctx_v2_t) == 224) &&
      ($bits(zhao_raster_pretex_v2_t) == 491) &&
      ($bits(zhao_raster_earlyz_payload_v2_t) == 411) &&
      ($bits(zhao_raster_retire_ctx_v2_t) == 160) &&
      ($bits(zhao_texture_result_v2_t) == 48);

  localparam bit EARLYZ_OFFSET_CONTRACT_OK =
      (EARLYZ_SOURCE_ID_LO == 0) && (EARLYZ_SOURCE_ID_HI == 15) &&
      (EARLYZ_FRAGMENT_STATE_LO == 16) && (EARLYZ_FRAGMENT_STATE_HI == 47) &&
      (EARLYZ_INVW24_LO == 48) && (EARLYZ_INVW24_HI == 71) &&
      (EARLYZ_IN_TILE_ADDR_LO == 72) && (EARLYZ_IN_TILE_ADDR_HI == 79);

  localparam bit CONTINUATION_OFFSET_CONTRACT_OK =
      (CONT_TAIL_STENCIL_REFERENCE_LO == 0) && (CONT_TAIL_STENCIL_REFERENCE_HI == 7) &&
      (CONT_TAIL_EFFECT_TAG_LO == 8) && (CONT_TAIL_EFFECT_TAG_HI == 15) &&
      (CONT_TAIL_VERTEX_ALPHA_LO == 16) && (CONT_TAIL_VERTEX_ALPHA_HI == 23) &&
      (CONT_TAIL_VERTEX_RGB_LO == 24) && (CONT_TAIL_VERTEX_RGB_HI == 47) &&
      (CONT_STENCIL_REFERENCE_LO == 0) && (CONT_STENCIL_REFERENCE_HI == 7) &&
      (CONT_EFFECT_TAG_LO == 8) && (CONT_EFFECT_TAG_HI == 15) &&
      (CONT_VERTEX_ALPHA_LO == 16) && (CONT_VERTEX_ALPHA_HI == 23) &&
      (CONT_VERTEX_RGB_LO == 24) && (CONT_VERTEX_RGB_HI == 47) &&
      (CONT_SOURCE_ID_LO == 48) && (CONT_SOURCE_ID_HI == 63) &&
      (CONT_FRAGMENT_STATE_LO == 64) && (CONT_FRAGMENT_STATE_HI == 95) &&
      (CONT_INVW24_LO == 96) && (CONT_INVW24_HI == 119) &&
      (CONT_IN_TILE_ADDR_LO == 120) && (CONT_IN_TILE_ADDR_HI == 127);

  localparam bit AUX_OFFSET_CONTRACT_OK =
      (AUX_WX_LO == 0) && (AUX_WX_HI == 31) &&
      (AUX_WZ_LO == 32) && (AUX_WZ_HI == 63) &&
      (AUX_SHEET_HANDLE_LO == 64) && (AUX_SHEET_HANDLE_HI == 95) &&
      (AUX_SHEET_GENERATION_LO == 64) && (AUX_SHEET_GENERATION_HI == 71) &&
      (AUX_SHEET_INDEX_LO == 72) && (AUX_SHEET_INDEX_HI == 95) &&
      (AUX_ENV_X0_LO == 96) && (AUX_ENV_X0_HI == 127) &&
      (AUX_ENV_X1_LO == 128) && (AUX_ENV_X1_HI == 159) &&
      (AUX_ENV_Z0_LO == 160) && (AUX_ENV_Z0_HI == 191) &&
      (AUX_ENV_Z1_LO == 192) && (AUX_ENV_Z1_HI == 223);

  localparam bit TEXREQ_OFFSET_CONTRACT_OK =
      (TEXREQ_PALETTE_GENERATION_LO == 0) && (TEXREQ_PALETTE_GENERATION_HI == 7) &&
      (TEXREQ_PALETTE_SLOT_LO == 8) && (TEXREQ_PALETTE_SLOT_HI == 9) &&
      (TEXREQ_RESPONSE_CLASS_LO == 10) && (TEXREQ_RESPONSE_CLASS_HI == 11) &&
      (TEXREQ_BASE_ALPHA_LO == 12) && (TEXREQ_BASE_ALPHA_HI == 19) &&
      (TEXREQ_BASE_RGB_LO == 20) && (TEXREQ_BASE_RGB_HI == 43) &&
      (TEXREQ_AUX_SURFACE_CTX_LO == 44) && (TEXREQ_AUX_SURFACE_CTX_HI == 267) &&
      (TEXREQ_AUX_REQUIRED_LO == 268) && (TEXREQ_AUX_REQUIRED_HI == 268) &&
      (TEXREQ_RECIPE_WEIGHT_LO == 269) && (TEXREQ_RECIPE_WEIGHT_HI == 276) &&
      (TEXREQ_MATERIAL_RECIPE_LO == 277) && (TEXREQ_MATERIAL_RECIPE_HI == 279) &&
      (TEXREQ_LOD_Q4_4_LO == 280) && (TEXREQ_LOD_Q4_4_HI == 287) &&
      (TEXREQ_BASE_BINDING_LO == 288) && (TEXREQ_BASE_BINDING_HI == 295) &&
      (TEXREQ_SAMPLE_COUNT_LO == 296) && (TEXREQ_SAMPLE_COUNT_HI == 297) &&
      (TEXREQ_V_OVER_W_LO == 298) && (TEXREQ_V_OVER_W_HI == 329) &&
      (TEXREQ_U_OVER_W_LO == 330) && (TEXREQ_U_OVER_W_HI == 361);

  localparam bit EZPAY_OFFSET_CONTRACT_OK =
      (EZPAY_TEXTURE_REQUEST_LO == 0) && (EZPAY_TEXTURE_REQUEST_HI == 361) &&
      (EZPAY_DETAIL_REQUIRED_LO == 362) && (EZPAY_DETAIL_REQUIRED_HI == 362) &&
      (EZPAY_STENCIL_REFERENCE_LO == 363) && (EZPAY_STENCIL_REFERENCE_HI == 370) &&
      (EZPAY_EFFECT_TAG_LO == 371) && (EZPAY_EFFECT_TAG_HI == 378) &&
      (EZPAY_VERTEX_ALPHA_LO == 379) && (EZPAY_VERTEX_ALPHA_HI == 386) &&
      (EZPAY_VERTEX_RGB_LO == 387) && (EZPAY_VERTEX_RGB_HI == 410);

  localparam bit PRETEX_OFFSET_CONTRACT_OK =
      (PRETEX_PALETTE_GENERATION_LO == 0) && (PRETEX_PALETTE_GENERATION_HI == 7) &&
      (PRETEX_PALETTE_SLOT_LO == 8) && (PRETEX_PALETTE_SLOT_HI == 9) &&
      (PRETEX_RESPONSE_CLASS_LO == 10) && (PRETEX_RESPONSE_CLASS_HI == 11) &&
      (PRETEX_BASE_ALPHA_LO == 12) && (PRETEX_BASE_ALPHA_HI == 19) &&
      (PRETEX_BASE_RGB_LO == 20) && (PRETEX_BASE_RGB_HI == 43) &&
      (PRETEX_AUX_SURFACE_CTX_LO == 44) && (PRETEX_AUX_SURFACE_CTX_HI == 267) &&
      (PRETEX_AUX_REQUIRED_LO == 268) && (PRETEX_AUX_REQUIRED_HI == 268) &&
      (PRETEX_RECIPE_WEIGHT_LO == 269) && (PRETEX_RECIPE_WEIGHT_HI == 276) &&
      (PRETEX_MATERIAL_RECIPE_LO == 277) && (PRETEX_MATERIAL_RECIPE_HI == 279) &&
      (PRETEX_LOD_Q4_4_LO == 280) && (PRETEX_LOD_Q4_4_HI == 287) &&
      (PRETEX_BASE_BINDING_LO == 288) && (PRETEX_BASE_BINDING_HI == 295) &&
      (PRETEX_SAMPLE_COUNT_LO == 296) && (PRETEX_SAMPLE_COUNT_HI == 297) &&
      (PRETEX_V_OVER_W_LO == 298) && (PRETEX_V_OVER_W_HI == 329) &&
      (PRETEX_U_OVER_W_LO == 330) && (PRETEX_U_OVER_W_HI == 361) &&
      (PRETEX_STENCIL_REFERENCE_LO == 362) && (PRETEX_STENCIL_REFERENCE_HI == 369) &&
      (PRETEX_EFFECT_TAG_LO == 370) && (PRETEX_EFFECT_TAG_HI == 377) &&
      (PRETEX_VERTEX_ALPHA_LO == 378) && (PRETEX_VERTEX_ALPHA_HI == 385) &&
      (PRETEX_VERTEX_RGB_LO == 386) && (PRETEX_VERTEX_RGB_HI == 409) &&
      (PRETEX_SOURCE_ID_LO == 410) && (PRETEX_SOURCE_ID_HI == 425) &&
      (PRETEX_FRAGMENT_STATE_LO == 426) && (PRETEX_FRAGMENT_STATE_HI == 457) &&
      (PRETEX_INVW24_LO == 458) && (PRETEX_INVW24_HI == 481) &&
      (PRETEX_IN_TILE_ADDR_LO == 482) && (PRETEX_IN_TILE_ADDR_HI == 489) &&
      (PRETEX_EARLYZ_PAYLOAD_LO == 0) && (PRETEX_EARLYZ_PAYLOAD_HI == 409) &&
      (PRETEX_EARLYZ_KEY_LO == 411) && (PRETEX_EARLYZ_KEY_HI == 490) &&
      (PRETEX_CONTINUATION_LO == 363) && (PRETEX_CONTINUATION_HI == 490) &&
      (PRETEX_TEXTURE_REQUEST_LO == 0) && (PRETEX_TEXTURE_REQUEST_HI == 361);

  localparam bit RETIRE_OFFSET_CONTRACT_OK =
      (RETIRE_STENCIL_REFERENCE_LO == 0) && (RETIRE_STENCIL_REFERENCE_HI == 7) &&
      (RETIRE_EFFECT_TAG_LO == 8) && (RETIRE_EFFECT_TAG_HI == 15) &&
      (RETIRE_VERTEX_ALPHA_LO == 16) && (RETIRE_VERTEX_ALPHA_HI == 23) &&
      (RETIRE_VERTEX_RGB_LO == 24) && (RETIRE_VERTEX_RGB_HI == 47) &&
      (RETIRE_SOURCE_ID_LO == 48) && (RETIRE_SOURCE_ID_HI == 63) &&
      (RETIRE_FRAGMENT_STATE_LO == 64) && (RETIRE_FRAGMENT_STATE_HI == 95) &&
      (RETIRE_INVW24_LO == 96) && (RETIRE_INVW24_HI == 119) &&
      (RETIRE_IN_TILE_ADDR_LO == 120) && (RETIRE_IN_TILE_ADDR_HI == 127) &&
      (RETIRE_CONTINUATION_LO == 0) && (RETIRE_CONTINUATION_HI == 127) &&
      (RETIRE_RASTER_SEQUENCE_LO == 128) && (RETIRE_RASTER_SEQUENCE_HI == 159);

  localparam bit RESULT_OFFSET_CONTRACT_OK =
      (TEXTURE_RESULT_RGB_LO == 0) && (TEXTURE_RESULT_RGB_HI == 23) &&
      (TEXTURE_RESULT_ALPHA_LO == 24) && (TEXTURE_RESULT_ALPHA_HI == 31) &&
      (TEXTURE_RESULT_SAMPLE0_INDEX_LO == 32) && (TEXTURE_RESULT_SAMPLE0_INDEX_HI == 39) &&
      (TEXTURE_RESULT_STATUS_LO == 40) && (TEXTURE_RESULT_STATUS_HI == 47) &&
      (TEXTURE_STATUS_SOURCE_REFUSED_BIT == 0) &&
      (TEXTURE_STATUS_RESERVED_LO == 1) && (TEXTURE_STATUS_RESERVED_HI == 7) &&
      (TEXTURE_RESULT_SOURCE_REFUSED_BIT == 40) &&
      (TEXTURE_RESULT_RESERVED_LO == 41) && (TEXTURE_RESULT_RESERVED_HI == 47);

  // ---- typed pack/unpack and exact seam construction ------------------------
  function automatic logic [RASTER_CONTINUATION_W-1:0]
      pack_raster_continuation(input zhao_raster_continuation_v2_t value);
    return value;
  endfunction

  function automatic zhao_raster_continuation_v2_t
      unpack_raster_continuation(input logic [RASTER_CONTINUATION_W-1:0] bits);
    return zhao_raster_continuation_v2_t'(bits);
  endfunction

  function automatic logic [AUX_SURFACE_CTX_W-1:0]
      pack_aux_surface_ctx(input zhao_aux_surface_ctx_v2_t value);
    return value;
  endfunction

  function automatic zhao_aux_surface_ctx_v2_t
      unpack_aux_surface_ctx(input logic [AUX_SURFACE_CTX_W-1:0] bits);
    return zhao_aux_surface_ctx_v2_t'(bits);
  endfunction

  function automatic logic [RASTER_PRETEX_W-1:0]
      pack_raster_pretex(input zhao_raster_pretex_v2_t value);
    return value;
  endfunction

  function automatic zhao_raster_pretex_v2_t
      unpack_raster_pretex(input logic [RASTER_PRETEX_W-1:0] bits);
    return zhao_raster_pretex_v2_t'(bits);
  endfunction

  function automatic logic [RASTER_EARLYZ_PAYLOAD_W-1:0]
      pack_earlyz_payload(input zhao_raster_earlyz_payload_v2_t value);
    return value;
  endfunction

  function automatic zhao_raster_earlyz_payload_v2_t
      unpack_earlyz_payload(input logic [RASTER_EARLYZ_PAYLOAD_W-1:0] bits);
    return zhao_raster_earlyz_payload_v2_t'(bits);
  endfunction

  function automatic logic [RASTER_RETIRE_CTX_W-1:0]
      pack_raster_retire_ctx(input zhao_raster_retire_ctx_v2_t value);
    return value;
  endfunction

  function automatic zhao_raster_retire_ctx_v2_t
      unpack_raster_retire_ctx(input logic [RASTER_RETIRE_CTX_W-1:0] bits);
    return zhao_raster_retire_ctx_v2_t'(bits);
  endfunction

  function automatic logic [TEXTURE_RESULT_W-1:0]
      pack_texture_result(input zhao_texture_result_v2_t value);
    return value;
  endfunction

  function automatic zhao_texture_result_v2_t
      unpack_texture_result(input logic [TEXTURE_RESULT_W-1:0] bits);
    return zhao_texture_result_v2_t'(bits);
  endfunction

  function automatic zhao_raster_pretex_v2_t make_raster_pretex(
      input zhao_raster_continuation_v2_t continuation,
      input zhao_texture_v3_request_v2_t  request);
    zhao_raster_pretex_v2_t packet;
    packet.earlyz                       = continuation.earlyz;
    packet.payload.raster_continuation = continuation.post_earlyz;
    packet.payload.texture_request     = request;
    return packet;
  endfunction

  function automatic zhao_raster_continuation_v2_t pretex_continuation(
      input zhao_raster_pretex_v2_t packet);
    zhao_raster_continuation_v2_t continuation;
    continuation.earlyz      = packet.earlyz;
    continuation.post_earlyz = packet.payload.raster_continuation;
    return continuation;
  endfunction

  function automatic zhao_texture_v3_request_v2_t pretex_texture_request(
      input zhao_raster_pretex_v2_t packet);
    return packet.payload.texture_request;
  endfunction

  function automatic zhao_raster_retire_ctx_v2_t make_raster_retire_ctx(
      input logic [31:0]                    raster_sequence,
      input zhao_raster_continuation_v2_t   continuation);
    zhao_raster_retire_ctx_v2_t retire_value;
    retire_value.raster_sequence     = raster_sequence;
    retire_value.raster_continuation = continuation;
    return retire_value;
  endfunction

  // Independent layout fingerprints.  Each function writes through field names;
  // each EXPECTED value is a separately written literal concatenation.  The
  // guard compares them as constants in a synthesis-visible initial contract;
  // the committed controls execute the same checks in simulation.
  localparam logic [RASTER_CONTINUATION_W-1:0] CONTINUATION_LAYOUT_EXPECTED = {
    8'hA5, 24'h12_3456, 32'h89AB_CDEF, 16'h1357,
    24'h24_68AC, 8'h5A, 8'hC3, 8'h7E
  };
  localparam logic [AUX_SURFACE_CTX_W-1:0] AUX_LAYOUT_EXPECTED = {
    32'hFEDC_BA98, 32'h7654_3210, 32'h89AB_CDEF, 32'h0123_4567,
    32'hA1B2_C3D4, 32'hD00D_F00D, 32'h1020_3040
  };
  localparam logic [TEXTURE_V3_REQUEST_W-1:0] TEXREQ_LAYOUT_EXPECTED = {
    1'b1,
    32'h8102_0304, 32'h0506_0708, 2'b11, 8'h91, 8'hA2,
    3'b101, 8'hB3, 1'b1,
    32'hFEDC_BA98, 32'h7654_3210, 32'h89AB_CDEF, 32'h0123_4567,
    32'hA1B2_C3D4, 32'hD00D_F00D, 32'h1020_3040,
    24'hC4_D5E6, 8'hD7, 2'b10, 2'b01, 8'hE8
  };
  localparam logic [RASTER_EARLYZ_PAYLOAD_W-1:0] EZPAY_LAYOUT_EXPECTED = {
    24'h24_68AC, 8'h5A, 8'hC3, 8'h7E, TEXREQ_LAYOUT_EXPECTED
  };
  localparam logic [RASTER_PRETEX_W-1:0] PRETEX_LAYOUT_EXPECTED = {
    CONTINUATION_LAYOUT_EXPECTED, TEXREQ_LAYOUT_EXPECTED
  };
  localparam logic [RASTER_RETIRE_CTX_W-1:0] RETIRE_LAYOUT_EXPECTED = {
    32'h55AA_F00D, CONTINUATION_LAYOUT_EXPECTED
  };
  localparam logic [TEXTURE_RESULT_W-1:0] RESULT_LAYOUT_EXPECTED = {
    8'h81, 8'h42, 8'h7F, 24'h12_34AB
  };

  function automatic logic [RASTER_CONTINUATION_W-1:0]
      continuation_layout_probe();
    zhao_raster_continuation_v2_t value;
    value = '0;
    value.earlyz.in_tile_addr = 8'hA5;
    value.earlyz.invw24 = 24'h12_3456;
    value.earlyz.fragment_state = 32'h89AB_CDEF;
    value.earlyz.source_id = 16'h1357;
    value.post_earlyz.vertex_rgb = 24'h24_68AC;
    value.post_earlyz.vertex_alpha = 8'h5A;
    value.post_earlyz.effect_tag = 8'hC3;
    value.post_earlyz.stencil_reference = 8'h7E;
    return value;
  endfunction

  function automatic logic [AUX_SURFACE_CTX_W-1:0] aux_layout_probe();
    zhao_aux_surface_ctx_v2_t value;
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

  function automatic logic [TEXTURE_V3_REQUEST_W-1:0]
      texreq_layout_probe();
    zhao_texture_v3_request_v2_t value;
    value = '0;
    value.detail_required = 1'b1;
    value.u_over_w = 32'sh8102_0304;
    value.v_over_w = 32'sh0506_0708;
    value.sample_count = 2'b11;
    value.base_binding_selector = 8'h91;
    value.lod_q4_4 = 8'hA2;
    value.material_recipe = 3'b101;
    value.recipe_weight = 8'hB3;
    value.aux_required = 1'b1;
    value.aux_surface_ctx = zhao_aux_surface_ctx_v2_t'(aux_layout_probe());
    value.base_rgb = 24'hC4_D5E6;
    value.base_alpha = 8'hD7;
    value.response_class = 2'b10;
    value.palette_slot = 2'b01;
    value.palette_generation = 8'hE8;
    return value;
  endfunction

  function automatic logic [RASTER_EARLYZ_PAYLOAD_W-1:0]
      ezpay_layout_probe();
    zhao_raster_earlyz_payload_v2_t value;
    zhao_raster_continuation_v2_t continuation_value;
    continuation_value = zhao_raster_continuation_v2_t'(continuation_layout_probe());
    value.raster_continuation = continuation_value.post_earlyz;
    value.texture_request = zhao_texture_v3_request_v2_t'(texreq_layout_probe());
    return value;
  endfunction

  function automatic logic [RASTER_PRETEX_W-1:0] pretex_layout_probe();
    zhao_raster_pretex_v2_t value;
    value = make_raster_pretex(
      zhao_raster_continuation_v2_t'(continuation_layout_probe()),
      zhao_texture_v3_request_v2_t'(texreq_layout_probe()));
    return value;
  endfunction

  function automatic logic [RASTER_RETIRE_CTX_W-1:0]
      retire_layout_probe();
    zhao_raster_retire_ctx_v2_t value;
    value = make_raster_retire_ctx(
      32'h55AA_F00D,
      zhao_raster_continuation_v2_t'(continuation_layout_probe()));
    return value;
  endfunction

  function automatic logic [TEXTURE_RESULT_W-1:0] result_layout_probe();
    zhao_texture_result_v2_t value;
    value.status = 8'h81;
    value.sample0_raw_index = 8'h42;
    value.alpha = 8'h7F;
    value.rgb = 24'h12_34AB;
    return value;
  endfunction

endpackage : zhao_render_texture_pkg

// Static/runtime instrument for the package itself.  It is deliberately
// uninstantiated by production and is registered as a probe in prod_manifest.
// The first initial block is visible to synthesis so Quartus can reject a bad
// default contract during elaboration.  Packet-A controls override one operand
// at a time and execute the same checks in simulation; lint-only is not evidence.
module zhao_render_texture_layout_guard #(
  parameter bit WIDTH_CONTRACT_OK_P =
      zhao_render_texture_pkg::WIDTH_CONTRACT_OK,
  parameter bit EARLYZ_OFFSET_CONTRACT_OK_P =
      zhao_render_texture_pkg::EARLYZ_OFFSET_CONTRACT_OK,
  parameter bit CONTINUATION_OFFSET_CONTRACT_OK_P =
      zhao_render_texture_pkg::CONTINUATION_OFFSET_CONTRACT_OK,
  parameter bit AUX_OFFSET_CONTRACT_OK_P =
      zhao_render_texture_pkg::AUX_OFFSET_CONTRACT_OK,
  parameter bit TEXREQ_OFFSET_CONTRACT_OK_P =
      zhao_render_texture_pkg::TEXREQ_OFFSET_CONTRACT_OK,
  parameter bit EZPAY_OFFSET_CONTRACT_OK_P =
      zhao_render_texture_pkg::EZPAY_OFFSET_CONTRACT_OK,
  parameter bit PRETEX_OFFSET_CONTRACT_OK_P =
      zhao_render_texture_pkg::PRETEX_OFFSET_CONTRACT_OK,
  parameter bit RETIRE_OFFSET_CONTRACT_OK_P =
      zhao_render_texture_pkg::RETIRE_OFFSET_CONTRACT_OK,
  parameter bit RESULT_OFFSET_CONTRACT_OK_P =
      zhao_render_texture_pkg::RESULT_OFFSET_CONTRACT_OK,
  parameter logic [zhao_render_texture_pkg::RASTER_CONTINUATION_W-1:0]
      CONTINUATION_LAYOUT_PROBE_P = zhao_render_texture_pkg::continuation_layout_probe(),
  parameter logic [zhao_render_texture_pkg::AUX_SURFACE_CTX_W-1:0]
      AUX_LAYOUT_PROBE_P = zhao_render_texture_pkg::aux_layout_probe(),
  parameter logic [zhao_render_texture_pkg::TEXTURE_V3_REQUEST_W-1:0]
      TEXREQ_LAYOUT_PROBE_P = zhao_render_texture_pkg::texreq_layout_probe(),
  parameter logic [zhao_render_texture_pkg::RASTER_EARLYZ_PAYLOAD_W-1:0]
      EZPAY_LAYOUT_PROBE_P = zhao_render_texture_pkg::ezpay_layout_probe(),
  parameter logic [zhao_render_texture_pkg::RASTER_PRETEX_W-1:0]
      PRETEX_LAYOUT_PROBE_P = zhao_render_texture_pkg::pretex_layout_probe(),
  parameter logic [zhao_render_texture_pkg::RASTER_RETIRE_CTX_W-1:0]
      RETIRE_LAYOUT_PROBE_P = zhao_render_texture_pkg::retire_layout_probe(),
  parameter logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0]
      RESULT_LAYOUT_PROBE_P = zhao_render_texture_pkg::result_layout_probe()
);
  import zhao_render_texture_pkg::*;

  // Quartus 17 requires module-scope elaboration checks to use an explicit
  // initial block.  These conditions are constant and produce no hardware.
  // Every label is unique so a simulated positive control proves the intended
  // detector fired rather than accepting an unrelated tool failure.
  initial begin : p_static_contract
    if (!WIDTH_CONTRACT_OK_P)
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[1]: WIDTH_CONTRACT");
    if (!EARLYZ_OFFSET_CONTRACT_OK_P)
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[2]: EARLYZ_OFFSET_CONTRACT");
    if (!CONTINUATION_OFFSET_CONTRACT_OK_P)
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[3]: CONTINUATION_OFFSET_CONTRACT");
    if (!AUX_OFFSET_CONTRACT_OK_P)
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[4]: AUX_OFFSET_CONTRACT");
    if (!TEXREQ_OFFSET_CONTRACT_OK_P)
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[5]: TEXREQ_OFFSET_CONTRACT");
    if (!EZPAY_OFFSET_CONTRACT_OK_P)
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[6]: EZPAY_OFFSET_CONTRACT");
    if (!PRETEX_OFFSET_CONTRACT_OK_P)
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[7]: PRETEX_OFFSET_CONTRACT");
    if (!RETIRE_OFFSET_CONTRACT_OK_P)
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[8]: RETIRE_OFFSET_CONTRACT");
    if (!RESULT_OFFSET_CONTRACT_OK_P)
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[9]: RESULT_OFFSET_CONTRACT");

    if (($bits(zhao_raster_continuation_v2_t) != RASTER_CONTINUATION_W) ||
        (CONTINUATION_LAYOUT_PROBE_P !== CONTINUATION_LAYOUT_EXPECTED))
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[10]: CONTINUATION_LAYOUT");
    if (($bits(zhao_aux_surface_ctx_v2_t) != AUX_SURFACE_CTX_W) ||
        (AUX_LAYOUT_PROBE_P !== AUX_LAYOUT_EXPECTED))
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[16]: AUX_LAYOUT");
    if (($bits(zhao_texture_v3_request_v2_t) != TEXTURE_V3_REQUEST_W) ||
        (TEXREQ_LAYOUT_PROBE_P !== TEXREQ_LAYOUT_EXPECTED))
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[11]: TEXREQ_LAYOUT");
    if (($bits(zhao_raster_earlyz_payload_v2_t) != RASTER_EARLYZ_PAYLOAD_W) ||
        (EZPAY_LAYOUT_PROBE_P !== EZPAY_LAYOUT_EXPECTED))
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[12]: EZPAY_LAYOUT");
    if (($bits(zhao_raster_pretex_v2_t) != RASTER_PRETEX_W) ||
        (PRETEX_LAYOUT_PROBE_P !== PRETEX_LAYOUT_EXPECTED))
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[13]: PRETEX_LAYOUT");
    if (($bits(zhao_raster_retire_ctx_v2_t) != RASTER_RETIRE_CTX_W) ||
        (RETIRE_LAYOUT_PROBE_P !== RETIRE_LAYOUT_EXPECTED))
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[14]: RETIRE_LAYOUT");
    if (($bits(zhao_texture_result_v2_t) != TEXTURE_RESULT_W) ||
        (RESULT_LAYOUT_PROBE_P !== RESULT_LAYOUT_EXPECTED))
      $fatal(1, "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[15]: RESULT_LAYOUT");
  end

  // Runtime-only exhaustive field-span instrumentation.
  // synthesis translate_off
  localparam int unsigned MAX_LAYOUT_W = RASTER_PRETEX_W;

  logic [MAX_LAYOUT_W-1:0] observed;
  logic [MAX_LAYOUT_W-1:0] expected;
  integer field_span_control;
  integer field_span_call_index;
  zhao_raster_earlyz_key_v2_t earlyz;
  zhao_raster_continuation_tail_v2_t tail;
  zhao_raster_continuation_v2_t continuation;
  zhao_aux_surface_ctx_v2_t aux;
  zhao_texture_v3_request_v2_t request;
  zhao_raster_earlyz_payload_v2_t ez_payload;
  zhao_raster_pretex_v2_t pretex;
  zhao_raster_retire_ctx_v2_t retire_ctx;
  zhao_texture_result_v2_t result;

  task automatic expect_span(
      input string name,
      input logic [MAX_LAYOUT_W-1:0] actual,
      input int unsigned lo,
      input int unsigned hi);
    field_span_call_index = field_span_call_index + 1;
    expected = '0;
    for (int unsigned bit_index = lo; bit_index <= hi; bit_index++)
      expected[bit_index] = 1'b1;
    // Runtime positive control: corrupt the independently constructed expected
    // mask for exactly one named span while leaving the field-derived actual
    // untouched.  One compiled model can fire all branches via plusargs.
    if (field_span_control == field_span_call_index)
      expected[lo] = ~expected[lo];
    if (actual !== expected) begin
      if (field_span_control == field_span_call_index)
        $fatal(1,
          "ZHAO_RENDER_TEXTURE_FIELD_SPAN_FIRE[%0d]: %s",
          field_span_call_index, name);
      $fatal(1, "render-texture layout span %s expected [%0d:%0d]", name, hi, lo);
    end
  endtask

  initial begin : p_layout_contract
    field_span_control = 0;
    field_span_call_index = 0;
    if ($value$plusargs("FIELD_SPAN_CONTROL=%d", field_span_control)) begin
      if ((field_span_control < 1) || (field_span_control > 45))
        $fatal(1, "invalid FIELD_SPAN_CONTROL=%0d", field_span_control);
    end

    // Field-name probes independently prove that the declarations occupy the
    // named spans.  Nested-type span probes then prove every absolute offset.
    earlyz = '0; earlyz.source_id = '1;
    observed = '0; observed[RASTER_EARLYZ_KEY_W-1:0] = earlyz;
    expect_span("earlyz.source_id", observed, EARLYZ_SOURCE_ID_LO, EARLYZ_SOURCE_ID_HI);
    earlyz = '0; earlyz.fragment_state = '1;
    observed = '0; observed[RASTER_EARLYZ_KEY_W-1:0] = earlyz;
    expect_span("earlyz.fragment_state", observed, EARLYZ_FRAGMENT_STATE_LO, EARLYZ_FRAGMENT_STATE_HI);
    earlyz = '0; earlyz.invw24 = '1;
    observed = '0; observed[RASTER_EARLYZ_KEY_W-1:0] = earlyz;
    expect_span("earlyz.invw24", observed, EARLYZ_INVW24_LO, EARLYZ_INVW24_HI);
    earlyz = '0; earlyz.in_tile_addr = '1;
    observed = '0; observed[RASTER_EARLYZ_KEY_W-1:0] = earlyz;
    expect_span("earlyz.in_tile_addr", observed, EARLYZ_IN_TILE_ADDR_LO, EARLYZ_IN_TILE_ADDR_HI);

    tail = '0; tail.stencil_reference = '1;
    observed = '0; observed[RASTER_CONTINUATION_TAIL_W-1:0] = tail;
    expect_span("tail.stencil_reference", observed, CONT_TAIL_STENCIL_REFERENCE_LO, CONT_TAIL_STENCIL_REFERENCE_HI);
    tail = '0; tail.effect_tag = '1;
    observed = '0; observed[RASTER_CONTINUATION_TAIL_W-1:0] = tail;
    expect_span("tail.effect_tag", observed, CONT_TAIL_EFFECT_TAG_LO, CONT_TAIL_EFFECT_TAG_HI);
    tail = '0; tail.vertex_alpha = '1;
    observed = '0; observed[RASTER_CONTINUATION_TAIL_W-1:0] = tail;
    expect_span("tail.vertex_alpha", observed, CONT_TAIL_VERTEX_ALPHA_LO, CONT_TAIL_VERTEX_ALPHA_HI);
    tail = '0; tail.vertex_rgb = '1;
    observed = '0; observed[RASTER_CONTINUATION_TAIL_W-1:0] = tail;
    expect_span("tail.vertex_rgb", observed, CONT_TAIL_VERTEX_RGB_LO, CONT_TAIL_VERTEX_RGB_HI);

    continuation = '0; continuation.earlyz = '1;
    observed = '0; observed[RASTER_CONTINUATION_W-1:0] = continuation;
    expect_span("continuation.earlyz", observed, CONT_SOURCE_ID_LO, CONT_IN_TILE_ADDR_HI);
    continuation = '0; continuation.post_earlyz = '1;
    observed = '0; observed[RASTER_CONTINUATION_W-1:0] = continuation;
    expect_span("continuation.post_earlyz", observed, CONT_STENCIL_REFERENCE_LO, CONT_VERTEX_RGB_HI);

    aux = '0; aux.wx = '1;
    observed = '0; observed[AUX_SURFACE_CTX_W-1:0] = aux;
    expect_span("aux.wx", observed, AUX_WX_LO, AUX_WX_HI);
    aux = '0; aux.wz = '1;
    observed = '0; observed[AUX_SURFACE_CTX_W-1:0] = aux;
    expect_span("aux.wz", observed, AUX_WZ_LO, AUX_WZ_HI);
    aux = '0; aux.sheet_handle[7:0] = '1;
    observed = '0; observed[AUX_SURFACE_CTX_W-1:0] = aux;
    expect_span("aux.sheet_generation", observed, AUX_SHEET_GENERATION_LO, AUX_SHEET_GENERATION_HI);
    aux = '0; aux.sheet_handle[31:8] = '1;
    observed = '0; observed[AUX_SURFACE_CTX_W-1:0] = aux;
    expect_span("aux.sheet_index", observed, AUX_SHEET_INDEX_LO, AUX_SHEET_INDEX_HI);
    aux = '0; aux.env_x0 = '1;
    observed = '0; observed[AUX_SURFACE_CTX_W-1:0] = aux;
    expect_span("aux.env_x0", observed, AUX_ENV_X0_LO, AUX_ENV_X0_HI);
    aux = '0; aux.env_x1 = '1;
    observed = '0; observed[AUX_SURFACE_CTX_W-1:0] = aux;
    expect_span("aux.env_x1", observed, AUX_ENV_X1_LO, AUX_ENV_X1_HI);
    aux = '0; aux.env_z0 = '1;
    observed = '0; observed[AUX_SURFACE_CTX_W-1:0] = aux;
    expect_span("aux.env_z0", observed, AUX_ENV_Z0_LO, AUX_ENV_Z0_HI);
    aux = '0; aux.env_z1 = '1;
    observed = '0; observed[AUX_SURFACE_CTX_W-1:0] = aux;
    expect_span("aux.env_z1", observed, AUX_ENV_Z1_LO, AUX_ENV_Z1_HI);

    request = '0; request.palette_generation = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.palette_generation", observed, TEXREQ_PALETTE_GENERATION_LO, TEXREQ_PALETTE_GENERATION_HI);
    request = '0; request.palette_slot = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.palette_slot", observed, TEXREQ_PALETTE_SLOT_LO, TEXREQ_PALETTE_SLOT_HI);
    request = '0; request.response_class = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.response_class", observed, TEXREQ_RESPONSE_CLASS_LO, TEXREQ_RESPONSE_CLASS_HI);
    request = '0; request.base_alpha = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.base_alpha", observed, TEXREQ_BASE_ALPHA_LO, TEXREQ_BASE_ALPHA_HI);
    request = '0; request.base_rgb = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.base_rgb", observed, TEXREQ_BASE_RGB_LO, TEXREQ_BASE_RGB_HI);
    request = '0; request.aux_surface_ctx = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.aux_surface_ctx", observed, TEXREQ_AUX_SURFACE_CTX_LO, TEXREQ_AUX_SURFACE_CTX_HI);
    request = '0; request.aux_required = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.aux_required", observed, TEXREQ_AUX_REQUIRED_LO, TEXREQ_AUX_REQUIRED_HI);
    request = '0; request.recipe_weight = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.recipe_weight", observed, TEXREQ_RECIPE_WEIGHT_LO, TEXREQ_RECIPE_WEIGHT_HI);
    request = '0; request.material_recipe = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.material_recipe", observed, TEXREQ_MATERIAL_RECIPE_LO, TEXREQ_MATERIAL_RECIPE_HI);
    request = '0; request.lod_q4_4 = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.lod_q4_4", observed, TEXREQ_LOD_Q4_4_LO, TEXREQ_LOD_Q4_4_HI);
    request = '0; request.base_binding_selector = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.base_binding_selector", observed, TEXREQ_BASE_BINDING_LO, TEXREQ_BASE_BINDING_HI);
    request = '0; request.sample_count = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.sample_count", observed, TEXREQ_SAMPLE_COUNT_LO, TEXREQ_SAMPLE_COUNT_HI);
    request = '0; request.v_over_w = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.v_over_w", observed, TEXREQ_V_OVER_W_LO, TEXREQ_V_OVER_W_HI);
    request = '0; request.u_over_w = '1;
    observed = '0; observed[TEXTURE_V3_REQUEST_W-1:0] = request;
    expect_span("request.u_over_w", observed, TEXREQ_U_OVER_W_LO, TEXREQ_U_OVER_W_HI);

    aux = '0; aux.wx = 32'sh8000_0000;
    if (aux.wx >= 0) $fatal(1, "render-texture aux.wx lost signed type");
    aux.wz = 32'sh8000_0000;
    if (aux.wz >= 0) $fatal(1, "render-texture aux.wz lost signed type");
    aux.env_x0 = 32'sh8000_0000;
    if (aux.env_x0 >= 0) $fatal(1, "render-texture aux.env_x0 lost signed type");
    aux.env_x1 = 32'sh8000_0000;
    if (aux.env_x1 >= 0) $fatal(1, "render-texture aux.env_x1 lost signed type");
    aux.env_z0 = 32'sh8000_0000;
    if (aux.env_z0 >= 0) $fatal(1, "render-texture aux.env_z0 lost signed type");
    aux.env_z1 = 32'sh8000_0000;
    if (aux.env_z1 >= 0) $fatal(1, "render-texture aux.env_z1 lost signed type");
    request = '0; request.u_over_w = 32'sh8000_0000;
    if (request.u_over_w >= 0) $fatal(1, "render-texture request.u_over_w lost signed type");
    request.v_over_w = 32'sh8000_0000;
    if (request.v_over_w >= 0) $fatal(1, "render-texture request.v_over_w lost signed type");

    ez_payload = '0; ez_payload.texture_request = '1;
    observed = '0; observed[RASTER_EARLYZ_PAYLOAD_W-1:0] = ez_payload;
    expect_span("earlyz_payload.texture_request", observed, EZPAY_TEXTURE_REQUEST_LO, EZPAY_TEXTURE_REQUEST_HI);
    ez_payload = '0; ez_payload.raster_continuation = '1;
    observed = '0; observed[RASTER_EARLYZ_PAYLOAD_W-1:0] = ez_payload;
    expect_span("earlyz_payload.raster_continuation", observed, EZPAY_STENCIL_REFERENCE_LO, EZPAY_VERTEX_RGB_HI);

    pretex = '0; pretex.payload.texture_request = '1;
    observed = pretex;
    expect_span("pretex.texture_request", observed, PRETEX_TEXTURE_REQUEST_LO, PRETEX_TEXTURE_REQUEST_HI);
    pretex = '0; pretex.payload.raster_continuation = '1;
    observed = pretex;
    expect_span("pretex.continuation_tail", observed, PRETEX_STENCIL_REFERENCE_LO, PRETEX_VERTEX_RGB_HI);
    pretex = '0; pretex.earlyz = '1;
    observed = pretex;
    expect_span("pretex.earlyz", observed, PRETEX_EARLYZ_KEY_LO, PRETEX_EARLYZ_KEY_HI);
    pretex = '0; pretex.payload = '1;
    observed = pretex;
    expect_span("pretex.earlyz_payload", observed, PRETEX_EARLYZ_PAYLOAD_LO, PRETEX_EARLYZ_PAYLOAD_HI);
    pretex = '0; pretex.earlyz = '1; pretex.payload.raster_continuation = '1;
    observed = pretex;
    expect_span("pretex.raster_continuation", observed, PRETEX_CONTINUATION_LO, PRETEX_CONTINUATION_HI);

    retire_ctx = '0; retire_ctx.raster_continuation = '1;
    observed = '0; observed[RASTER_RETIRE_CTX_W-1:0] = retire_ctx;
    expect_span("retire_ctx.raster_continuation", observed, RETIRE_CONTINUATION_LO, RETIRE_CONTINUATION_HI);
    retire_ctx = '0; retire_ctx.raster_sequence = '1;
    observed = '0; observed[RASTER_RETIRE_CTX_W-1:0] = retire_ctx;
    expect_span("retire_ctx.raster_sequence", observed, RETIRE_RASTER_SEQUENCE_LO, RETIRE_RASTER_SEQUENCE_HI);

    result = '0; result.rgb = '1;
    observed = '0; observed[TEXTURE_RESULT_W-1:0] = result;
    expect_span("result.rgb", observed, TEXTURE_RESULT_RGB_LO, TEXTURE_RESULT_RGB_HI);
    result = '0; result.alpha = '1;
    observed = '0; observed[TEXTURE_RESULT_W-1:0] = result;
    expect_span("result.alpha", observed, TEXTURE_RESULT_ALPHA_LO, TEXTURE_RESULT_ALPHA_HI);
    result = '0; result.sample0_raw_index = '1;
    observed = '0; observed[TEXTURE_RESULT_W-1:0] = result;
    expect_span("result.sample0_raw_index", observed, TEXTURE_RESULT_SAMPLE0_INDEX_LO, TEXTURE_RESULT_SAMPLE0_INDEX_HI);
    result = '0; result.status = '1;
    observed = '0; observed[TEXTURE_RESULT_W-1:0] = result;
    expect_span("result.status", observed, TEXTURE_RESULT_STATUS_LO, TEXTURE_RESULT_STATUS_HI);

    if (field_span_call_index != 45)
      $fatal(1, "render-texture field-span census expected 45, found %0d",
             field_span_call_index);
    if (field_span_control != 0)
      $fatal(1, "ZHAO_RENDER_TEXTURE_FIELD_SPAN_ESCAPED[%0d]",
             field_span_control);
    $display("ZHAO_RENDER_TEXTURE_LAYOUT_GUARD_OK field_spans=45");
  end
  // synthesis translate_on

endmodule : zhao_render_texture_layout_guard
