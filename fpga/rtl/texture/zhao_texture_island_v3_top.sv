// zhao_texture_island_v3_top.sv -- Packet-E E2 shared V3 texture composition.
//
// zhao_texture_v3own is the only fragment-lifecycle owner.  Every descriptor,
// material row, binding lookup, class completion and AUX transaction is keyed by
// the owner allocated on the one atomic fragment-admission edge.
`default_nettype none

`ifndef ZHAO_PACKET_B_TMU_RESULT
`define ZHAO_PACKET_B_TMU_RESULT(tuple) tuple[47:0]
`endif
// TIMING4 R1T mutation seam: which owner addresses the UVW bank.
`ifndef ZHAO_ISLAND_T4_UVW_READ_OWNER
`define ZHAO_ISLAND_T4_UVW_READ_OWNER(head_owner, live_owner) head_owner
`endif
`ifndef ZHAO_PACKET_B_COMBINE_S2
`define ZHAO_PACKET_B_COMBINE_S2(sample2, aux) sample2
`endif
`ifndef ZHAO_PACKET_B_RETIRE_CONTEXT
`define ZHAO_PACKET_B_RETIRE_CONTEXT(context) context[CTXW +: RCTXW]
`endif
`ifndef ZHAO_PACKET_B_RECOVERABLE_NEXT
`define ZHAO_PACKET_B_RECOVERABLE_NEXT(fault_set, clear_fire, current) \
    (fault_set ? 1'b1 : (clear_fire ? 1'b0 : current))
`endif
`ifndef ZHAO_PACKET_B_OWNER_MASK_GENERATION
`define ZHAO_PACKET_B_OWNER_MASK_GENERATION(generation) generation
`endif
`ifndef ZHAO_PACKET_B_CACHE_TOKEN
`define ZHAO_PACKET_B_CACHE_TOKEN(token) token
`endif
`ifndef ZHAO_PACKET_B_BILERP_OBS_TOKEN
`define ZHAO_PACKET_B_BILERP_OBS_TOKEN(token, channel) token
`endif
`ifndef ZHAO_PACKET_B_SHADOW_WRITE
`define ZHAO_PACKET_B_SHADOW_WRITE(metadata) metadata
`endif
`ifndef ZHAO_PACKET_B_RSP_OBS_DATA
`define ZHAO_PACKET_B_RSP_OBS_DATA(data, pending) data
`endif

// Packet-E refusal seams.  The production defaults preserve the original cache
// token, bypass every native arithmetic path on nonzero status, and present the
// held refusal to the fair per-class merge.  The committed mutant file may
// override exactly one expression immediately before this source.
`ifndef ZHAO_PACKET_E_CACHE_STATUS_IS_REFUSAL
`define ZHAO_PACKET_E_CACHE_STATUS_IS_REFUSAL(status) (|(status))
`endif
`ifndef ZHAO_PACKET_E_REFUSAL_CLASS
`define ZHAO_PACKET_E_REFUSAL_CLASS(token) token[17:16]
`endif
`ifndef ZHAO_PACKET_E_REFUSAL_MERGE_VALID
`define ZHAO_PACKET_E_REFUSAL_MERGE_VALID(valid) (valid)
`endif

module zhao_texture_island_v3_top #(
    parameter bit MIGRATION_SHADOWS = 1'b1,
    parameter int unsigned DEPTH = 16,
    parameter int unsigned CTXW = 64,
    parameter int unsigned RCTXW = 160,
    parameter int unsigned AUXCTXW = 224,
    parameter int unsigned BINDW = 8,
    parameter int unsigned LODW = 8,
    parameter int unsigned GENW = 8,
    parameter int unsigned LANES = 4,
    parameter int unsigned SRCW = 18,
    parameter int unsigned DATAW = 64,
    parameter int unsigned TOKW = 18,
    parameter int unsigned AUX_TOKW = 14,
    parameter int unsigned PAL_SLOTS = 4,
    parameter int unsigned PAL_ENTRIES = 256,
    parameter bit BILERP_DSP2 = 1'b0
) (
    input  var logic clk,
    input  var logic rst_n,

    // One atomic fragment admission.
    input  var logic                    frag_valid_i,
    output var logic                    frag_ready_o,
    input  var logic [23:0]             frag_invw24_i,
    input  var logic signed [31:0]      frag_u_over_w_i,
    input  var logic signed [31:0]      frag_v_over_w_i,
    input  var logic [1:0]              frag_sample_count_i,
    input  var logic [BINDW-1:0]        frag_binding_i,
    input  var logic [LODW-1:0]         frag_lod_i,
    input  var logic [2:0]              frag_recipe_i,
    input  var logic [7:0]              frag_weight_i,
    input  var logic [CTXW-1:0]         frag_ctx_i,
    input  var logic [RCTXW-1:0]        frag_retire_ctx_i,
    input  var logic [AUXCTXW-1:0]      frag_aux_ctx_i,
    input  var logic                    frag_aux_i,
    input  var logic [23:0]             frag_base_rgb_i,
    input  var logic [7:0]              frag_base_a_i,
    input  var logic [1:0]              frag_class_i,
    input  var logic [$clog2(PAL_SLOTS)-1:0] frag_pal_slot_i,
    input  var logic [GENW-1:0]         frag_pal_gen_i,
    // TERRAIN.NORMALMAP's PER-FRAGMENT DECLARATION (NORMALMAP, 2026-09-26).
    // High = this fragment's PRODUCER declared it a heightfield surface, so the
    // detail normal applies to it. It is a DECLARATION carried from the door and
    // never a property inferred here: `zref_terrain_normalmap.hpp`'s whole
    // premise is that "a heightfield's tangent frame is axis-aligned in world
    // space", which is a statement about the primitive class and not about any
    // field that happens to be zero (owner directive 2026-09-23 section 3).
    input  var logic                    frag_detail_i,

    // Canonical recoverable frame-fault handshake.
    input  var logic                    frame_fault_clear_valid_i,
    output var logic                    frame_fault_clear_ready_o,
    output var logic                    frame_fault_o,
    output var logic                    lifetime_structural_fault_o,

    // Sealed binding-page loader: BEGIN / WRITE / END / ABORT.
    input  var logic                    cfg_valid_i,
    output var logic                    cfg_ready_o,
    input  var logic [1:0]              cfg_op_i,
    input  var logic [7:0]              cfg_page_generation_i,
    input  var logic [7:0]              cfg_selector_i,
    input  var logic [74:0]             cfg_row_i,
    input  var logic [31:0]             cfg_crc32_i,
    output var logic                    cfg_rsp_valid_o,
    input  var logic                    cfg_rsp_ready_i,
    output var logic [1:0]              cfg_rsp_op_o,
    output var logic [3:0]              cfg_rsp_status_o,
    output var logic [7:0]              cfg_rsp_page_generation_o,
    output var logic [7:0]              active_page_generation_o,

    // One-line cache fill. Packet-E denial is a typed terminal response.
    output var logic                    fill_req_valid_o,
    input  var logic                    fill_req_ready_i,
    output var logic [31:0]             fill_req_addr_o,
    input  var logic                    fill_data_valid_i,
    input  var logic [15:0]             fill_data_i,
    input  var logic                    fill_refused_i,

    // Palette programming has no response channel in Packet B.
    input  var logic                    pal_load_valid_i,
    output var logic                    pal_load_ready_o,
    input  var logic [1:0]              pal_load_op_i,
    input  var logic [$clog2(PAL_SLOTS)-1:0] pal_load_slot_i,
    input  var logic [GENW-1:0]         pal_load_gen_i,
    input  var logic [$clog2(PAL_ENTRIES)-1:0] pal_load_idx_i,
    input  var logic [15:0]             pal_load_rgb565_i,
    input  var logic                    pal_load_crc_ok_i,

    // ---- TERRAIN.NORMALMAP's CONFIG AND TILE UPLOAD (NORMALMAP, 2026-09-26) --
    // ONE write port with TWO destinations and an EXPLICIT selector. The two
    // destinations have different address and data widths (a 3-bit cfg word
    // index with 32 bits of payload; a 13-bit flat pyramid word address with a
    // 16-bit {s8 dz, s8 dx} texel), so the port carries the wider of each and
    // the narrow destination takes its low bits. `dtl_sel_i` says WHICH -- it is
    // not derived from the address range, because an address-decoded selector
    // makes an upload-tool fault land silently in the other destination.
    //
    // The producer is `zhao_terrain_normalloader` in `zhao_console_core`, which
    // carries a published DETAIL_NORMAL page (spec/cartridge.md 4g, kind 16)
    // word by word, plus the sun/strength writes the core derives from
    // SetEnvironment. Five modules pass this through unchanged -- the loader
    // needs MEM.UPLOAD's publication and MEM.GUARD, which live at the core, and
    // the consumer needs the perspective-corrected fragment, which lives here.
    input  var logic                    dtl_we_i,
    input  var logic                    dtl_sel_i,   // 0 = cfg word, 1 = tile word
    input  var logic [12:0]             dtl_addr_i,
    input  var logic [31:0]             dtl_data_i,

    // Complete Surface Sheet READ request and page response.
    output var logic                    sheet_req_valid_o,
    input  var logic                    sheet_req_ready_i,
    output var logic [1:0]              sheet_req_op_o,
    output var logic [31:0]             sheet_req_handle_o,
    output var logic [11:0]             sheet_req_texel_o,
    output var logic [15:0]             sheet_req_src_id_o,
    input  var logic                    pg_valid_i,
    output var logic                    pg_ready_o,
    input  var logic [1:0]              pg_op_i,
    input  var logic [1:0]              pg_status_i,
    input  var logic [7:0]              pg_tag_i,
    input  var logic [7:0]              pg_strength_i,
    input  var logic [15:0]             pg_src_id_i,

    // Ordered owner result.  These are aliases of one immutable result48.
    output var logic                    out_valid_o,
    input  var logic                    out_ready_i,
    output var logic [23:0]             out_rgb_o,
    output var logic [7:0]              out_a_o,
    output var logic [7:0]              out_texel_idx_o,
    output var logic [7:0]              out_status_o,
    output var logic [15:0]             out_tag_o,
    output var logic [RCTXW-1:0]        out_retire_ctx_o,
    output var logic                    out_refused_o,
    // TERRAIN.NORMALMAP's shade delta for THIS fragment, s9 with value
    // raw/256, presented on the same beat as the result above and keyed by the
    // same owner. `zref::terrain::normalmap_apply` is what the consumer does
    // with it, and the consumer is the LIT COLOUR LANE, not this island's texel
    // -- see the DELTA'S CONSUMER block below.
    output var logic signed [8:0]       out_detail_delta_o,
    output var logic                    quiet_o,

    // Compatibility/evidence outputs retained where the replaced path has the
    // same meaning.  Removed migration sidecars report their honest absence.
    output var logic                    err_fragrob_wq_overflow_o,
    output var logic                    err_fragrob_id_error_o,
    output var logic                    err_aux_degenerate_o,
    output var logic                    err_rcp_q_o,
    output var logic [31:0]             cnt_reorder_held_o,
    output var logic [31:0]             cnt_live_peak_o,
    output var logic [31:0]             cnt_fragments_o,
    output var logic [31:0]             cnt_cache_hits_o,
    output var logic [31:0]             cnt_cache_misses_o,
    output var logic [31:0]             cnt_palette_lookups_o,
    output var logic [31:0]             cnt_bilerp_jobs_o,
    output var logic [31:0]             cnt_mosaic_samples_o,
    // R9's `texture_samples`: filtered TMU samples PUBLISHED into their fragment by
    // `zhao_texture_v3own` -- the one owner the ruling names. NOT the mosaic count
    // above, which counts MOSAIC picks and belongs to TEXTURE.MOSAIC.
    output var logic [31:0]             cnt_texture_samples_o,
    output var logic [31:0]             cnt_aux_accepted_o,
    output var logic [31:0]             cnt_combine_refused_o,
    output var logic [31:0]             cnt_combine_phases_o,
    output var logic [31:0]             cnt_rcp_completed_o,
    output var logic [31:0]             cnt_persp_fragments_o,
    output var logic [31:0]             cnt_dispatch_accepted_o,
    output var logic [31:0]             cnt_plan_accepted_o,
    output var logic [31:0]             cnt_fragrob_id_errors_o,
    output var logic                    shadow_present_o,
    output var logic [31:0]             meta_shadow_mismatch_o,
    output var logic [31:0]             meta_shadow_reads_o,
    output var logic [31:0]             meta_align_err_o,
    output var logic [31:0]             meta_align_chk_o,
    output var logic [31:0]             meta_bil_err_o,
    output var logic [31:0]             meta_bil_chk_o,
    output var logic [31:0]             meta_near_err_o,
    output var logic [31:0]             meta_near_chk_o,
    output var logic [20:0]             meta_bil_first_q_o,
    output var logic [20:0]             meta_bil_first_t_o,
    output var logic [17:0]             meta_bil_first_tok_o,
    output var logic [31:0]             meta_genmis_o,
    output var logic [31:0]             cnt_combine_jobs_o [0:7],
    output var logic [31:0]             cnt_palette_stale_o,
    output var logic [31:0]             cnt_palette_cold_o,
    output var logic                    err_rsp_dropped_o,
    output var logic                    err_bil_chan_o,
    output var logic [31:0]             cnt_near_refused_o,
    output var logic [31:0]             err_unknown_class_o,
    output var logic [31:0]             err_class_invalid_o,
    output var logic [31:0]             err_palette_unusable_o,
    output var logic [31:0]             err_class_mismatch_o,
    output var logic                    err_plan_mode_o,

    // ---- TERRAIN.NORMALMAP's evidence, re-exported -------------------------
    // Every one of these is the leaf's own counter carried out, except
    // `err_detail_lost_o`, which is THIS composition's and is described where
    // it is driven.
    output var logic [31:0]             cnt_detail_fragments_o,
    output var logic [31:0]             cnt_detail_zeroed_o,
    output var logic [31:0]             cnt_detail_railed_o,
    output var logic [31:0]             cnt_detail_cold_o,
    // Deltas PUBLISHED with a fragment, i.e. read back at the owner's output
    // beat with the generation matching. This is the "how many times" counter
    // this repository has a chapter about, and it is the one a test asserts
    // against the fragment count -- not `cnt_detail_fragments_o`, which counts
    // what the leaf ACCEPTED and cannot see a delta that never got home.
    output var logic [31:0]             cnt_detail_published_o,
    // A fragment reached the perspective stage while the detail pipe could not
    // accept it, so its delta was never computed. The record then still holds
    // the ZERO this composition writes at admission under the fragment's OWN
    // generation, so the fault is a lost RELIEF and never a delta belonging to
    // another fragment. Unreachable with legal stimulus while the leaf's
    // `f_ready_o` is high whenever `d_ready_i` is -- see the committed mutant.
    output var logic [31:0]             err_detail_lost_o,
    output var logic                    dtl_table_ready_o
);
  import zhao_render_texture_pkg::*;

  localparam int unsigned OWNERW = 14;
  localparam int unsigned OWNER_SLOTW = 6;
  localparam int unsigned SMPW = 16;
  localparam int unsigned OWNERS = 64;
  localparam int unsigned MATW = 46;
  localparam int unsigned TUPLEW = 66;
  localparam logic [1:0] CLS_CLUT = 2'd0;
  localparam logic [1:0] CLS_NEAR = 2'd1;
  localparam logic [1:0] CLS_BIL  = 2'd2;
  localparam logic [1:0] CLS_ERR  = 2'd3;
  localparam logic [7:0] SOURCE_REFUSED_STATUS =
      8'h01 << TEXTURE_STATUS_SOURCE_REFUSED_BIT;
  localparam logic [2:0] FMT_CLUT8    = 3'd0;
  localparam logic [2:0] FMT_RGB565   = 3'd1;
  localparam logic [2:0] FMT_CLUT4    = 3'd2;
  localparam logic [2:0] FMT_ARGB1555 = 3'd3;
  localparam logic [2:0] FMT_ARGB4444 = 3'd4;

  initial begin : p_packet_b_parameters
`ifdef ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION
    $fatal(1, "ZHAO_TEXTURE_ISLAND_V3_PACKET_E_MUTANT_SELECTOR_COLLISION: define exactly one selector");
`endif
    if ((DEPTH != 16) || (CTXW != 64) || (RCTXW != 160) ||
        (AUXCTXW != 224) || (BINDW != 8) || (LODW != 8) ||
        (GENW != 8) || (LANES != 4) || (SRCW != 18) ||
        (DATAW != 64) || (TOKW != 18) || (AUX_TOKW != 14) ||
        (PAL_SLOTS != 4) || (PAL_ENTRIES != 256))
      $fatal(1, "texture island V3 Packet-B profile changed");
    if (($bits(zhao_aux_surface_ctx_v2_t) != AUXCTXW) ||
        ($bits(zhao_texture_result_v2_t) != TEXTURE_RESULT_W) ||
        (RCTXW + CTXW != 224) || (TUPLEW != TOKW + TEXTURE_RESULT_W))
      $fatal(1, "texture island V3 Packet-B width contract changed");
  end

`ifdef ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION
  ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE_SELECTOR
      u_packet_e_top_selector_collision_compile_fail();
`endif

  function automatic logic [3:0] required_mask_of(
      input logic [1:0] count, input logic aux_required);
    logic [2:0] samples;
    begin
      unique case (count)
        2'd0: samples = 3'b000;
        2'd1: samples = 3'b001;
        2'd2: samples = 3'b011;
        default: samples = 3'b111;
      endcase
      required_mask_of = {aux_required, samples};
    end
  endfunction

  function automatic logic [1:0] sample_count_from_mask(input logic [2:0] mask);
    unique case (mask)
      3'b000: sample_count_from_mask = 2'd0;
      3'b001: sample_count_from_mask = 2'd1;
      3'b011: sample_count_from_mask = 2'd2;
      default: sample_count_from_mask = 2'd3;
    endcase
  endfunction

  function automatic logic [MATW-1:0] canonical_refusal_material(
      input logic [3:0] owner_mask);
    logic [1:0] count;
    logic [2:0] illegal_recipe;
    begin
      count = sample_count_from_mask(owner_mask[2:0]);
      // PASSTHRU is illegal at count 2; MODULATE is illegal at 0/1/3.
      illegal_recipe = (count == 2'd2) ? 3'd0 : 3'd1;
      canonical_refusal_material = {
          24'd0, 8'd0, 8'd0, illegal_recipe, count, owner_mask[3]};
    end
  endfunction

  function automatic logic material_count_legal(
      input logic [2:0] recipe, input logic [1:0] count);
    unique case (recipe)
      3'd0: material_count_legal = (count == 2'd0) || (count == 2'd1);
      3'd6, 3'd7: material_count_legal = (count == 2'd3);
      default: material_count_legal = (count == 2'd2);
    endcase
  endfunction

  function automatic logic format_is_direct(input logic [2:0] format);
    unique case (format)
      FMT_RGB565, FMT_ARGB1555, FMT_ARGB4444: format_is_direct = 1'b1;
      default: format_is_direct = 1'b0;
    endcase
  endfunction

  function automatic logic [31:0] decode16(
      input logic [15:0] halfword, input logic [2:0] format);
    logic [7:0] alpha, red, green, blue;
    begin
      unique case (format)
        FMT_ARGB1555: begin
          alpha = halfword[15] ? 8'hff : 8'h00;
          red   = {halfword[14:10], halfword[14:12]};
          green = {halfword[9:5], halfword[9:7]};
          blue  = {halfword[4:0], halfword[4:2]};
        end
        FMT_ARGB4444: begin
          alpha = {halfword[15:12], halfword[15:12]};
          red   = {halfword[11:8], halfword[11:8]};
          green = {halfword[7:4], halfword[7:4]};
          blue  = {halfword[3:0], halfword[3:0]};
        end
        default: begin
          alpha = 8'hff;
          red   = {halfword[15:11], halfword[15:13]};
          green = {halfword[10:5], halfword[10:9]};
          blue  = {halfword[4:0], halfword[4:2]};
        end
      endcase
      decode16 = {alpha, red, green, blue};
    end
  endfunction

  function automatic logic [7:0] decoded_channel(
      input logic [15:0] halfword,
      input logic [2:0] format,
      input logic [1:0] channel);
    logic [31:0] decoded;
    begin
      decoded = decode16(halfword, format);
      unique case (channel)
        2'd0: decoded_channel = decoded[23:16];
        2'd1: decoded_channel = decoded[15:8];
        2'd2: decoded_channel = decoded[7:0];
        default: decoded_channel = decoded[31:24];
      endcase
    end
  endfunction

  function automatic logic [20:0] metadata21(input logic [39:0] metadata);
    metadata21 = {metadata[1], metadata[21:19], metadata[18:11],
                  metadata[10:3], metadata[2]};
  endfunction

  // ---------------------------------------------------------------------------
  logic own_ev_quiet_w;
  logic rcp_idle_w;
  logic persp_idle_w;
  logic metajoin_idle_w;
  logic desc_idle_w;
  logic uvjoin_idle_w;
  logic expand_idle_w;
  logic binding_data_idle_w;
  logic plan_idle_w;
  logic cache_idle_w;
  logic dispatch_idle_w;
  logic mosaic_idle_w;
  logic [3:0] bilerp_lane_idle_w;
  logic palette_idle_w;
  logic palette_cfg_idle_w;
  logic aux_idle_w;
  logic owner_claim_valid_w;
  logic owner_ready_valid_w;
  logic owner_combine_valid_w;
  logic owner_final_valid_w;
  logic rcp_req_valid_w;
  logic rcp_rsp_valid_w;
  logic persp_req_valid_w;
  logic persp_rsp_valid_w;
  logic metajoin_a_valid_w;
  logic metajoin_b_valid_w;
  logic metajoin_rsp_valid_w;
  logic desc_req_valid_w;
  logic desc_rsp_valid_w;
  logic uvjoin_desc_valid_w;
  logic uvjoin_uv_valid_w;
  logic uvjoin_rsp_valid_w;
  logic expand_frag_valid_w;
  logic expand_sample_valid_w;
  logic expand_aux_valid_w;
  logic binding_req_valid_w;
  logic binding_plan_valid_w;
  logic binding_refuse_valid_w;
  logic plan_req_valid_w;
  logic plan_cache_valid_w;
  logic cache_req_valid_w;
  logic cache_rsp_valid_w;
  logic mosaic_req_valid_w;
  logic mosaic_rsp_valid_w;
  logic [3:0] bilerp_req_valid_w;
  logic [3:0] bilerp_rsp_valid_w;
  logic palette_req_valid_w;
  logic palette_rsp_valid_w;
  logic [3:0] class_terminal_offer_valid_w;
  logic [3:0] dispatch_pending_w;
  logic dispatch_return_valid_w;
  logic aux_job_valid_w;
  logic sheet_req_valid_w;
  logic aux_sheet_rsp_owed_w;
  logic aux_refuse_valid_w;
  logic aux_return_valid_w;
  logic combine_req_valid_w;
  logic combine_rsp_valid_w;
  logic binding_cfg_loader_idle_w;
  logic binding_crc_busy_w;
  logic binding_seal_pending_w;

  logic q_owner_idle;
  logic q_rcp_idle;
  logic q_persp_idle;
  logic q_metajoin_idle;
  logic q_desc_idle;
  logic q_uvjoin_idle;
  logic q_expand_idle;
  logic q_bind_idle;
  logic q_plan_idle;
  logic q_cache_idle;
  logic q_dispatch_idle;
  logic q_mosaic_idle;
  logic q_bilerp_idle;
  logic q_palette_idle;
  logic q_palette_cfg_idle;
  logic q_aux_idle;
  logic q_combine_idle;
  logic q_frag_offer_valid;
  logic q_owner_claim_valid;
  logic q_owner_ready_valid;
  logic q_owner_combine_valid;
  logic q_owner_final_valid;
  logic q_rcp_req_valid;
  logic q_rcp_rsp_valid;
  logic q_persp_req_valid;
  logic q_persp_rsp_valid;
  logic q_metajoin_a_valid;
  logic q_metajoin_b_valid;
  logic q_metajoin_rsp_valid;
  logic q_desc_req_valid;
  logic q_desc_rsp_valid;
  logic q_uvjoin_desc_valid;
  logic q_uvjoin_uv_valid;
  logic q_uvjoin_rsp_valid;
  logic q_expand_frag_valid;
  logic q_expand_sample_valid;
  logic q_expand_aux_valid;
  logic q_bind_req_valid;
  logic q_bind_plan_valid;
  logic q_bind_refuse_valid;
  logic q_plan_req_valid;
  logic q_plan_cache_valid;
  logic q_cache_req_valid;
  logic q_fill_req_valid;
  logic q_fill_rsp_valid;
  logic q_cache_rsp_valid;
  logic q_mosaic_req_valid;
  logic q_mosaic_rsp_valid;
  logic [3:0] q_bilerp_req_valid;
  logic [3:0] q_bilerp_rsp_valid;
  logic q_palette_req_valid;
  logic q_palette_rsp_valid;
  logic q_palette_cfg_valid;
  logic q_palette_cfg_rsp_valid;
  logic q_dispatch_req_valid;
  logic [3:0] q_class_rsp_valid;
  logic q_tmu_return_valid;
  logic q_aux_req_valid;
  logic q_sheet_req_valid;
  logic q_sheet_rsp_owed;
  logic q_sheet_rsp_valid;
  logic q_aux_refuse_valid;
  logic q_aux_return_valid;
  logic q_combine_req_valid;
  logic q_combine_rsp_valid;
  logic q_retire_valid;
  logic q_cfg_cmd_valid;
  logic q_cfg_rsp_valid;
  logic cfg_loader_idle;
  logic binding_crc_busy;
  logic binding_seal_pending;
  logic data_quiet;

  assign q_owner_idle = own_ev_quiet_w;
  assign q_rcp_idle = rcp_idle_w;
  assign q_persp_idle = persp_idle_w;
  assign q_metajoin_idle = metajoin_idle_w;
  assign q_desc_idle = desc_idle_w;
  assign q_uvjoin_idle = uvjoin_idle_w;
  assign q_expand_idle = expand_idle_w;
  assign q_bind_idle = binding_data_idle_w;
  assign q_plan_idle = plan_idle_w;
  assign q_cache_idle = cache_idle_w;
  assign q_dispatch_idle = dispatch_idle_w;
  assign q_mosaic_idle = mosaic_idle_w;
  assign q_bilerp_idle = &bilerp_lane_idle_w[3:0];
  assign q_palette_idle = palette_idle_w;
  assign q_palette_cfg_idle = palette_cfg_idle_w;
  assign q_aux_idle = aux_idle_w;
  assign q_combine_idle = material_read_idle_w && combine_leaf_idle_w;
  assign q_frag_offer_valid = frag_valid_i;
  assign q_owner_claim_valid = owner_claim_valid_w;
  assign q_owner_ready_valid = owner_ready_valid_w;
  assign q_owner_combine_valid = owner_combine_valid_w;
  assign q_owner_final_valid = owner_final_valid_w;
  assign q_rcp_req_valid = rcp_req_valid_w;
  // The Timing4 reciprocal head holds an accepted RCP response that has not yet
  // reached perspective prep, so it is outstanding reciprocal-response work and
  // belongs in this existing quiet term. Folding it here rather than adding a
  // new operand keeps data_quiet's operand inventory -- and the committed quiet
  // mutation fixture built from it -- exactly as it was, while making quiet
  // strictly stronger: it now stays false until the head is empty too.
  assign q_rcp_rsp_valid = rcp_rsp_valid_w || rcp_head_valid_q;
  assign q_persp_req_valid = persp_req_valid_w;
  assign q_persp_rsp_valid = persp_rsp_valid_w;
  assign q_metajoin_a_valid = metajoin_a_valid_w;
  assign q_metajoin_b_valid = metajoin_b_valid_w;
  assign q_metajoin_rsp_valid = metajoin_rsp_valid_w;
  assign q_desc_req_valid = desc_req_valid_w;
  assign q_desc_rsp_valid = desc_rsp_valid_w;
  assign q_uvjoin_desc_valid = uvjoin_desc_valid_w;
  assign q_uvjoin_uv_valid = uvjoin_uv_valid_w;
  assign q_uvjoin_rsp_valid = uvjoin_rsp_valid_w;
  assign q_expand_frag_valid = expand_frag_valid_w;
  assign q_expand_sample_valid = expand_sample_valid_w;
  assign q_expand_aux_valid = expand_aux_valid_w;
  assign q_bind_req_valid = binding_req_valid_w;
  assign q_bind_plan_valid = binding_plan_valid_w;
  assign q_bind_refuse_valid = binding_refuse_valid_w;
  assign q_plan_req_valid = plan_req_valid_w;
  assign q_plan_cache_valid = plan_cache_valid_w;
  assign q_cache_req_valid = cache_req_valid_w;
  assign q_fill_req_valid = fill_req_valid_o;
  assign q_fill_rsp_valid = fill_data_valid_i || fill_refused_i;
  assign q_cache_rsp_valid = cache_rsp_valid_w;
  assign q_mosaic_req_valid = mosaic_req_valid_w;
  assign q_mosaic_rsp_valid = mosaic_rsp_valid_w;
  assign q_bilerp_req_valid = bilerp_req_valid_w[3:0];
  assign q_bilerp_rsp_valid = bilerp_rsp_valid_w[3:0];
  assign q_palette_req_valid = palette_req_valid_w;
  assign q_palette_rsp_valid = palette_rsp_valid_w;
  assign q_palette_cfg_valid = pal_load_valid_i;
  assign q_palette_cfg_rsp_valid = 1'b0;
  assign q_dispatch_req_valid = |class_terminal_offer_valid_w[3:0];
  assign q_class_rsp_valid = dispatch_pending_w[3:0];
  assign q_tmu_return_valid = dispatch_return_valid_w;
  assign q_aux_req_valid = aux_job_valid_w;
  assign q_sheet_req_valid = sheet_req_valid_w;
  assign q_sheet_rsp_owed = aux_sheet_rsp_owed_w;
  assign q_sheet_rsp_valid = pg_valid_i;
  assign q_aux_refuse_valid = aux_refuse_valid_w;
  assign q_aux_return_valid = aux_return_valid_w;
  assign q_combine_req_valid = combine_req_valid_w;
  assign q_combine_rsp_valid = combine_rsp_valid_w;
  assign q_retire_valid = out_valid_o;
  assign q_cfg_cmd_valid = cfg_valid_i;
  assign q_cfg_rsp_valid = cfg_rsp_valid_o;
  assign cfg_loader_idle = binding_cfg_loader_idle_w;
  assign binding_crc_busy = binding_crc_busy_w;
  assign binding_seal_pending = binding_seal_pending_w;

  assign data_quiet =
      q_owner_idle
   && q_rcp_idle
   && q_persp_idle
   && q_metajoin_idle
   && q_desc_idle
   && q_uvjoin_idle
   && q_expand_idle
   && q_bind_idle
   && q_plan_idle
   && q_cache_idle
   && q_dispatch_idle
   && q_mosaic_idle
   && q_bilerp_idle
   && q_palette_idle
   && q_aux_idle
   && q_combine_idle
   && !q_owner_claim_valid
   && !q_owner_ready_valid
   && !q_owner_combine_valid
   && !q_owner_final_valid
   && !q_rcp_req_valid
   && !q_rcp_rsp_valid
   && !q_persp_req_valid
   && !q_persp_rsp_valid
   && !q_metajoin_a_valid
   && !q_metajoin_b_valid
   && !q_metajoin_rsp_valid
   && !q_desc_req_valid
   && !q_desc_rsp_valid
   && !q_uvjoin_desc_valid
   && !q_uvjoin_uv_valid
   && !q_uvjoin_rsp_valid
   && !q_expand_frag_valid
   && !q_expand_sample_valid
   && !q_expand_aux_valid
   && !q_bind_req_valid
   && !q_bind_plan_valid
   && !q_bind_refuse_valid
   && !q_plan_req_valid
   && !q_plan_cache_valid
   && !q_cache_req_valid
   && !q_fill_req_valid
   && !q_cache_rsp_valid
   && !q_mosaic_req_valid
   && !q_mosaic_rsp_valid
   && !(|q_bilerp_req_valid[3:0])
   && !(|q_bilerp_rsp_valid[3:0])
   && !q_palette_req_valid
   && !q_palette_rsp_valid
   && !q_dispatch_req_valid
   && !(|q_class_rsp_valid[3:0])
   && !q_tmu_return_valid
   && !q_aux_req_valid
   && !q_sheet_req_valid
   && !q_sheet_rsp_owed
   && !q_aux_refuse_valid
   && !q_aux_return_valid
   && !q_combine_req_valid
   && !q_combine_rsp_valid
   && !q_retire_valid;

  assign quiet_o =
      data_quiet
   && cfg_loader_idle
   && !binding_crc_busy
   && !binding_seal_pending
   && q_palette_cfg_idle
   && !q_frag_offer_valid
   && !q_fill_rsp_valid
   && !q_sheet_rsp_valid
   && !q_palette_cfg_valid
   && !q_palette_cfg_rsp_valid
   && !q_cfg_cmd_valid
   && !q_cfg_rsp_valid;

  logic frame_fault_clear_w;
  logic owner_mask_lifetime_fault_q;
  logic cache_sidx3_lifetime_fault_q;
  logic lifetime_admission_block_w;
  logic sim_frame_fault_inject_w;
  logic sim_rcp_qerr_overlay_w;
  logic sim_expand_overflow_overlay_w;
  logic sim_uv_mismatch_overlay_w;
  logic sim_owner_mask_overlay_w;
  logic sim_cache_sidx3_overlay_w;
  logic sim_metajoin_illegal_overlay_w;
  logic [2:0] sim_material_fault_mode_w;
  logic sim_combine_hold_w;
  logic [3:0] sim_packet_e_merge_hold_w;
  logic sim_packet_e_cache_class_override_en_w;
  logic [1:0] sim_packet_e_cache_class_override_w;
`ifndef SYNTHESIS
  logic sim_frame_fault_inject_q = 1'b0;
  logic [5:0] sim_lifetime_fault_pulse_q = 6'd0;
  logic sim_rcp_qerr_latched_q;
  logic sim_expand_overflow_latched_q;
  logic sim_uv_mismatch_latched_q;
  logic sim_owner_mask_latched_q;
  logic sim_cache_sidx3_latched_q;
  logic sim_metajoin_illegal_latched_q;
  logic [2:0] sim_material_fault_mode_q = 3'd0;
  logic sim_combine_hold_q = 1'b0;
  logic [3:0] sim_packet_e_merge_hold_q = 4'd0;
  logic sim_packet_e_cache_class_override_en_q = 1'b0;
  logic [1:0] sim_packet_e_cache_class_override_q = 2'd0;
  export "DPI-C" task zhao_texture_packet_b_set_frame_fault_inject;
  export "DPI-C" task zhao_texture_packet_b_set_lifetime_fault_inject;
  export "DPI-C" task zhao_texture_packet_b_set_material_fault_mode;
  export "DPI-C" task zhao_texture_packet_b_set_combine_hold;
  export "DPI-C" task zhao_texture_packet_b_get_owner_context;
  export "DPI-C" task zhao_texture_packet_b_get_owner_admitted;
  export "DPI-C" task zhao_texture_packet_b_get_combine_idle_components;
  export "DPI-C" task zhao_texture_packet_b_get_leaf_idle_vector;
  export "DPI-C" task zhao_texture_packet_b_get_combine_counters;
  export "DPI-C" task zhao_texture_packet_b_get_hostile_counters;
  export "DPI-C" task zhao_texture_packet_e_set_merge_hold;
  export "DPI-C" task zhao_texture_packet_e_set_cache_class_override;
  export "DPI-C" task zhao_texture_packet_e_get_merge_class_state;
  export "DPI-C" task zhao_texture_packet_e_get_cache_observation;
  export "DPI-C" task zhao_texture_packet_e_get_cache_counters;
  export "DPI-C" task zhao_texture_packet_e_get_protocol_counters;
  export "DPI-C" task zhao_texture_packet_e_get_quiet_observation;
  task zhao_texture_packet_b_set_frame_fault_inject(input bit enable);
    sim_frame_fault_inject_q = enable;
  endtask
  task zhao_texture_packet_b_set_lifetime_fault_inject(input int unsigned mask);
    sim_lifetime_fault_pulse_q = mask[5:0];
  endtask
  task zhao_texture_packet_b_set_material_fault_mode(input int unsigned mode);
    sim_material_fault_mode_q = mode[2:0];
  endtask
  task zhao_texture_packet_b_set_combine_hold(input bit enable);
    sim_combine_hold_q = enable;
  endtask
  task zhao_texture_packet_b_get_owner_context(
      output bit [RCTXW+CTXW-1:0] context_o);
    context_o = owner_out_context_w;
  endtask
  task zhao_texture_packet_b_get_owner_admitted(output int unsigned admitted_o);
    admitted_o = owner_admitted_w;
  endtask
  task zhao_texture_packet_b_get_combine_idle_components(
      output bit material_read_idle_o, output bit combine_leaf_idle_o);
    material_read_idle_o = material_read_idle_w;
    combine_leaf_idle_o = combine_leaf_idle_w;
  endtask
  task zhao_texture_packet_b_get_leaf_idle_vector(output bit [15:0] idle_o);
    idle_o = {q_combine_idle, q_aux_idle, q_palette_idle, q_bilerp_idle,
              q_mosaic_idle, q_dispatch_idle, q_cache_idle, q_plan_idle,
              q_bind_idle, q_expand_idle, q_uvjoin_idle, q_desc_idle,
              q_metajoin_idle, q_persp_idle, q_rcp_idle, q_owner_idle};
  endtask
  task zhao_texture_packet_b_get_combine_counters(
      output int unsigned recipe0_o,
      output int unsigned jobs_accepted_o,
      output int unsigned jobs_completed_o,
      output int unsigned phases_issued_o,
      output int unsigned phases_completed_o,
      output int unsigned material_fifo_count_o);
    recipe0_o = combine_recipe0_w;
    jobs_accepted_o = combine_jobs_accepted_w;
    jobs_completed_o = combine_jobs_completed_w;
    phases_issued_o = cnt_combine_phases_o;
    phases_completed_o = combine_phases_completed_w;
    material_fifo_count_o = 32'(material_fifo_count_q);
  endtask
  task zhao_texture_packet_b_get_hostile_counters(
      output int unsigned binding_issues_o,
      output int unsigned binding_refusals_o,
      output int unsigned owner_tmu_commits_o,
      output int unsigned material_mismatches_o);
    binding_issues_o = binding_jobs_w;
    binding_refusals_o = binding_local_refused_w;
    owner_tmu_commits_o = owner_tmu_commits_w;
    material_mismatches_o = material_read_mismatch_count_q;
  endtask
  task zhao_texture_packet_e_set_merge_hold(input int unsigned mask);
    sim_packet_e_merge_hold_q = mask[3:0];
  endtask
  task zhao_texture_packet_e_set_cache_class_override(
      input bit enable, input int unsigned response_class);
    sim_packet_e_cache_class_override_en_q = enable;
    sim_packet_e_cache_class_override_q = response_class[1:0];
  endtask
  task zhao_texture_packet_e_get_merge_class_state(
      input int unsigned response_class,
      output bit native_valid_o, output bit refusal_valid_o,
      output bit merged_valid_o, output bit merged_ready_o,
      output bit selected_refusal_o, output bit rr_refusal_o,
      output bit [TUPLEW-1:0] native_tuple_o,
      output bit [TUPLEW-1:0] refusal_tuple_o,
      output bit [TUPLEW-1:0] merged_tuple_o);
    native_valid_o = class_native_valid_w[response_class[1:0]];
    refusal_valid_o = refusal_valid_q[response_class[1:0]];
    merged_valid_o = class_merge_valid_w[response_class[1:0]];
    merged_ready_o = class_dispatch_ready_w[response_class[1:0]] &&
                     !sim_packet_e_merge_hold_w[response_class[1:0]];
    selected_refusal_o = class_merge_select_refusal_w[response_class[1:0]];
    rr_refusal_o = class_merge_rr_refusal_q[response_class[1:0]];
    native_tuple_o = class_native_tuple_w[response_class[1:0]];
    refusal_tuple_o = refusal_tuple_q[response_class[1:0]];
    merged_tuple_o = class_merge_tuple_w[response_class[1:0]];
  endtask
  task zhao_texture_packet_e_get_cache_observation(
      output bit valid_o, output bit ready_o, output bit [7:0] status_o,
      output bit [TOKW-1:0] token_o, output bit [DATAW-1:0] data_o);
    valid_o = cache_rsp_valid_w;
    ready_o = cache_rsp_ready_w;
    status_o = cache_rsp_status_w;
    token_o = cache_checked_token_w;
    data_o = cache_rsp_data_w;
  endtask
  task zhao_texture_packet_e_get_cache_counters(
      output int unsigned cache_accepted_o,
      output int unsigned cache_completed_o,
      output int unsigned fill_accepted_o,
      output int unsigned fill_completed_o,
      output int unsigned fill_refused_o,
      output int unsigned fill_beats_o,
      output bit protocol_fault_o,
      output int unsigned reservation_count_o,
      output bit [6:0] reservation_owner_o,
      output bit [8:0] work_state_o);
    cache_accepted_o = cache_jobs_accepted_w;
    cache_completed_o = cache_jobs_completed_w;
    fill_accepted_o = cache_fill_jobs_accepted_w;
    fill_completed_o = cache_fill_jobs_completed_w;
    fill_refused_o = cache_fill_jobs_refused_w;
    fill_beats_o = cache_fill_data_beats_w;
    protocol_fault_o = cache_fill_protocol_fault_w;
    reservation_count_o = cache_reservation_count_w;
    reservation_owner_o = cache_reservation_owner_state_w;
    work_state_o = cache_work_state_w;
  endtask
  task zhao_texture_packet_e_get_protocol_counters(
      output int unsigned dispatch_class_mismatch_o,
      output int unsigned metadata_generation_mismatch_o,
      output int unsigned owner_identity_error_o);
    dispatch_class_mismatch_o = dispatch_class_mismatch_w;
    metadata_generation_mismatch_o = metadata_generation_mismatch_w;
    owner_identity_error_o = cnt_fragrob_id_errors_o;
  endtask
  task zhao_texture_packet_e_get_quiet_observation(
      output bit q_dispatch_valid_o,
      output bit data_quiet_o,
      output bit public_quiet_o);
    q_dispatch_valid_o = q_dispatch_req_valid;
    data_quiet_o = data_quiet;
    public_quiet_o = quiet_o;
  endtask

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sim_rcp_qerr_latched_q <= 1'b0;
      sim_expand_overflow_latched_q <= 1'b0;
      sim_uv_mismatch_latched_q <= 1'b0;
      sim_owner_mask_latched_q <= 1'b0;
      sim_cache_sidx3_latched_q <= 1'b0;
      sim_metajoin_illegal_latched_q <= 1'b0;
    end else begin
      if (sim_lifetime_fault_pulse_q[0]) sim_rcp_qerr_latched_q <= 1'b1;
      if (sim_lifetime_fault_pulse_q[1]) sim_expand_overflow_latched_q <= 1'b1;
      if (sim_lifetime_fault_pulse_q[2]) sim_uv_mismatch_latched_q <= 1'b1;
      if (sim_lifetime_fault_pulse_q[3]) sim_owner_mask_latched_q <= 1'b1;
      if (sim_lifetime_fault_pulse_q[4]) sim_cache_sidx3_latched_q <= 1'b1;
      if (sim_lifetime_fault_pulse_q[5]) sim_metajoin_illegal_latched_q <= 1'b1;
    end
  end

  assign sim_frame_fault_inject_w = sim_frame_fault_inject_q;
  assign sim_rcp_qerr_overlay_w = sim_rcp_qerr_latched_q;
  assign sim_expand_overflow_overlay_w = sim_expand_overflow_latched_q;
  assign sim_uv_mismatch_overlay_w = sim_uv_mismatch_latched_q;
  assign sim_owner_mask_overlay_w = sim_owner_mask_latched_q;
  assign sim_cache_sidx3_overlay_w = sim_cache_sidx3_latched_q;
  assign sim_metajoin_illegal_overlay_w = sim_metajoin_illegal_latched_q;
  assign sim_material_fault_mode_w = sim_material_fault_mode_q;
  assign sim_combine_hold_w = sim_combine_hold_q;
  assign sim_packet_e_merge_hold_w = sim_packet_e_merge_hold_q;
  assign sim_packet_e_cache_class_override_en_w =
      sim_packet_e_cache_class_override_en_q;
  assign sim_packet_e_cache_class_override_w = sim_packet_e_cache_class_override_q;
`else
  assign sim_frame_fault_inject_w = 1'b0;
  assign sim_rcp_qerr_overlay_w = 1'b0;
  assign sim_expand_overflow_overlay_w = 1'b0;
  assign sim_uv_mismatch_overlay_w = 1'b0;
  assign sim_owner_mask_overlay_w = 1'b0;
  assign sim_cache_sidx3_overlay_w = 1'b0;
  assign sim_metajoin_illegal_overlay_w = 1'b0;
  assign sim_material_fault_mode_w = 3'd0;
  assign sim_combine_hold_w = 1'b0;
  assign sim_packet_e_merge_hold_w = 4'd0;
  assign sim_packet_e_cache_class_override_en_w = 1'b0;
  assign sim_packet_e_cache_class_override_w = 2'd0;
`endif

`ifdef ZHAO_PACKET_E_MUTANT_PRE_E_FILL_LIFETIME
  // Historical Packet-B behavior, compiled only by its committed inverse control:
  // denial never reaches the cache and poisons admission until reset.
  logic sim_pre_e_fill_refusal_lifetime_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) sim_pre_e_fill_refusal_lifetime_q <= 1'b0;
    else if (fill_refused_i) sim_pre_e_fill_refusal_lifetime_q <= 1'b1;
  end
`endif

  assign frame_fault_clear_ready_o = quiet_o;
  assign frame_fault_clear_w = frame_fault_clear_valid_i && frame_fault_clear_ready_o;
  assign lifetime_admission_block_w =
      (err_rcp_q_o || sim_rcp_qerr_overlay_w) ||
      (uvjoin_lifetime_fault_w || sim_uv_mismatch_overlay_w) ||
      ((expand_overflow_w != 32'd0) || sim_expand_overflow_overlay_w) ||
      (owner_mask_lifetime_fault_q || sim_owner_mask_overlay_w) ||
      (cache_sidx3_lifetime_fault_q || sim_cache_sidx3_overlay_w) ||
      err_rsp_dropped_o ||
      ((metadata_illegal_w != 32'd0) || sim_metajoin_illegal_overlay_w)
`ifdef ZHAO_PACKET_E_MUTANT_PRE_E_FILL_LIFETIME
      || sim_pre_e_fill_refusal_lifetime_q
`endif
      ;
  assign lifetime_structural_fault_o = lifetime_admission_block_w;

  // ---------------------------------------------------------------------------
  // Atomic admission and reciprocal/perspective path.
  logic binding_admission_enable_w;
  logic own_adm_valid_w;
  logic own_adm_ready_w;
  logic own_adm_accept_w;
  logic [OWNERW-1:0] own_adm_owner_w;
  logic [3:0] admission_required_mask_w;
  logic rcp_req_ready_w;
  logic rcp_rsp_ready_w;
  logic [23:0] rcp_result_w;
  logic [5:0] rcp_shift_w;
  logic rcp_zero_w;
  logic [OWNERW-1:0] rcp_owner_w;
  logic [5:0] rcp_occupancy_w;
  logic [31:0] rcp_accepted_w;

  assign admission_required_mask_w =
      required_mask_of(frag_sample_count_i, frag_aux_i);
  assign frag_ready_o = binding_admission_enable_w && !lifetime_admission_block_w &&
      own_adm_ready_w && rcp_req_ready_w;
  assign own_adm_valid_w = frag_valid_i && binding_admission_enable_w &&
      !lifetime_admission_block_w && rcp_req_ready_w;
  assign rcp_req_valid_w = frag_valid_i && binding_admission_enable_w &&
      !lifetime_admission_block_w && own_adm_ready_w;

  logic [63:0] uvw_m [0:OWNERS-1];
  always_ff @(posedge clk) begin
    if (own_adm_accept_w)
      uvw_m[own_adm_owner_w[13:8]] <= {frag_u_over_w_i, frag_v_over_w_i};
  end

  zhao_raster_rcp24_v4 #(.NCTX(12), .TOKW(OWNERW)) u_rcp (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(rcp_req_valid_w), .v_ready_o(rcp_req_ready_w),
      .d_i(frag_invw24_i), .v_tok_i(own_adm_owner_w),
      .r_valid_o(rcp_rsp_valid_w), .r_ready_i(rcp_rsp_ready_w),
      .r_o(rcp_result_w), .k_o(rcp_shift_w), .d_zero_o(rcp_zero_w),
      .r_tok_o(rcp_owner_w), .accepted_o(rcp_accepted_w),
      .completed_o(cnt_rcp_completed_o), .mul_jobs_o(), .zero_jobs_o(),
      .phase_jobs_o(), .negcorr_jobs_o(), .occupancy_o(rcp_occupancy_w),
      .qerr_o(err_rcp_q_o), .idle_o(rcp_idle_w));

  // TIMING4 R1T: A HELD RECIPROCAL HEAD IN FRONT OF THE UVW LOOKUP.
  //
  // The reported path started at the reciprocal pipeline's token bypass and
  // ended at persp_prep_uow_q, combining token selection, owner-slot addressing,
  // the 64-entry stored UVW value and destination acceptance in one cone. The
  // live RCP token was the read address.
  //
  // Now the RCP output handshake first captures the complete result record --
  // owner14 + reciprocal24 + shift6 + zero1 + valid, 46 bits -- and its
  // REGISTERED owner slot drives the uvw_m read on a later accepted transfer.
  // The reciprocal, shift, zero and owner that travel with that read all come
  // from the head, never from the next live RCP result, so a stalled prep can
  // never pair one owner's UVW row with another owner's reciprocal.
  //
  // This is a narrow identity boundary, not a second owner UVW table: uvw_m
  // keeps its sole writer on owner admission. Reading it a cycle later is safe
  // because the owner stays reserved until its ordinary retirement, which is
  // far downstream of here, so the slot cannot have been reallocated.
  //
  // Both stages accept one transfer per clock and the head permits simultaneous
  // consume/refill, so II=1 is preserved; only latency grows by one edge.
  logic rcp_head_valid_q;
  logic [23:0] rcp_head_recip_q;
  logic [5:0] rcp_head_shift_q;
  logic rcp_head_zero_q;
  logic [OWNERW-1:0] rcp_head_owner_q;

  logic persp_prep_valid_q;
  logic signed [31:0] persp_prep_uow_q, persp_prep_vow_q;
  logic [23:0] persp_prep_recip_q;
  logic [5:0] persp_prep_shift_q;
  logic persp_prep_zero_q;
  logic [OWNERW-1:0] persp_prep_owner_q;
  logic persp_req_ready_w;

  wire persp_prep_room_c = !persp_prep_valid_q || persp_req_ready_w;
  wire rcp_head_room_c   = !rcp_head_valid_q || persp_prep_room_c;

  // The UVW read address. Production uses the REGISTERED head owner; the
  // committed control substitutes the live RCP token, which is the pairing
  // failure this boundary removes.
  wire [OWNERW-1:0] uvw_read_owner_c =
      `ZHAO_ISLAND_T4_UVW_READ_OWNER(rcp_head_owner_q, rcp_owner_w);

  assign rcp_rsp_ready_w = rcp_head_room_c;
  assign persp_req_valid_w = persp_prep_valid_q;

  // THE uvw_m READ, MOVED OUT OF THE RESET BLOCK SO THE ARRAY CAN INFER.
  //
  // This assignment used to sit in the `always_ff @(posedge clk or negedge
  // rst_n)` below, under exactly the enable it still has. `uvw_m` is
  // 64 x 64 = 4,096 bits, it is written once and read once -- already the shape
  // that infers -- and Quartus still reported it as the ONE uninferred array in
  // the whole composed shell, because an M10K output register cannot carry an
  // asynchronous reset and this one was declared inside a block that has one.
  //
  // The 2026-09-18 composed fit measured what that costs in time as well as
  // area: ALL 206 paths in the `zhao_geom_binner_v2 -> zhao_texture_island_v3
  // _top` family end at `uvw_m~*`, worst -4.475 ns. It was docketed as an area
  // question; it was both.
  //
  // Nothing else moves. `persp_prep_uow_q` and `persp_prep_vow_q` are assigned
  // here and nowhere else, are consumed by the perspective stage below, and had
  // NO assignment in that reset branch -- so they lose no reset that existed,
  // gain no latency, and see the same enable on the same edge. The block below
  // keeps its asynchronous reset for the two valid bits that actually use it.
  always_ff @(posedge clk) begin
    if (persp_prep_room_c && rcp_head_valid_q)
      {persp_prep_uow_q, persp_prep_vow_q} <= uvw_m[uvw_read_owner_c[13:8]];
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rcp_head_valid_q <= 1'b0;
      persp_prep_valid_q <= 1'b0;
    end else begin
      // Consume-and-refill on one edge: when prep has room the head both
      // launches its record and accepts the next reciprocal result.
      if (persp_prep_room_c) begin
        persp_prep_valid_q <= rcp_head_valid_q;
        if (rcp_head_valid_q) begin
          persp_prep_recip_q <= rcp_head_recip_q;
          persp_prep_shift_q <= rcp_head_shift_q;
          persp_prep_zero_q  <= rcp_head_zero_q;
          persp_prep_owner_q <= rcp_head_owner_q;
        end
      end

      if (rcp_head_room_c) begin
        rcp_head_valid_q <= rcp_rsp_valid_w;
        if (rcp_rsp_valid_w) begin
          rcp_head_recip_q <= rcp_result_w;
          rcp_head_shift_q <= rcp_shift_w;
          rcp_head_zero_q  <= rcp_zero_w;
          rcp_head_owner_q <= rcp_owner_w;
        end
      end
    end
  end

  logic persp_rsp_ready_w;
  logic signed [31:0] persp_u_w, persp_v_w;
  logic [OWNERW-1:0] persp_owner_w;
  logic persp_sat_w, persp_zero_out_w;
  logic [31:0] persp_products_w, persp_zero_products_w;
  logic [4:0] persp_occupancy_w;

  zhao_raster_perspuv_pairpipe_v2 #(.NTOK(16), .TAGW(OWNERW)) u_persp (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(persp_req_valid_w), .v_ready_o(persp_req_ready_w),
      .u_over_w_i(persp_prep_uow_q), .v_over_w_i(persp_prep_vow_q),
      .r_mant_i(persp_prep_recip_q), .r_k_i(persp_prep_shift_q),
      .depth_zero_i(persp_prep_zero_q), .tag_i(persp_prep_owner_q),
      .r_valid_o(persp_rsp_valid_w), .r_ready_i(persp_rsp_ready_w),
      .u_o(persp_u_w), .v_o(persp_v_w), .tag_o(persp_owner_w),
      .sat_o(persp_sat_w), .depth_zero_o(persp_zero_out_w),
      .fragments_o(cnt_persp_fragments_o), .products_o(persp_products_w),
      .zero_products_o(persp_zero_products_w), .occupancy_o(persp_occupancy_w),
      .idle_o(persp_idle_w));

  // ===========================================================================
  // TERRAIN.NORMALMAP -- THE PER-FRAGMENT DETAIL TERM (NORMALMAP, 2026-09-26)
  // ===========================================================================
  // `zhao_terrain_normalmap` was built to contract on 2026-09-09 and
  // instantiated NOWHERE for seventeen days. Six packets refused to compose it
  // and every refusal named the same thing: its input is "a perspective-correct
  // terrain (u, v) with an integer mip level, tapped from the stream that feeds
  // the texture path", and no such stream existed.
  //
  // IT EXISTS HERE AND IT IS THE LINE ABOVE. `u_persp` runs for EVERY admitted
  // fragment -- the island's own assertion says so in as many words,
  // `a_atomic_admission: own_adm_accept_w == (rcp_req_valid_w && rcp_req_ready_w)`
  // -- so a fragment that takes NO texture sample still has its reciprocal
  // issued and its u/w, v/w multiplied through. `persp_u_w`/`persp_v_w` are
  // `signed [31:0]` perspective-correct coordinates, which is `f_u_i`/`f_v_i`'s
  // declared shape and format exactly. That is why the block goes HERE and not
  // one level out: this is the only place in the machine where a terrain
  // fragment's real (u, v) exists.
  //
  // THE TAP DOES NOT PARTICIPATE IN THE HANDSHAKE, DELIBERATELY. `f_valid_i` is
  // the persp transfer BEAT and `persp_rsp_ready_w` is left exactly as it was.
  // Adding a term to it would put this block inside the island's descriptor/uv
  // join arbitration, whose correctness argument is structural and does not
  // include a fourth party. The price is that a detail pipe which could not
  // accept would LOSE a delta rather than stall one, which is what
  // `err_detail_lost_o` counts and what `tests/mutants/
  // zhao_texture_island_v3_top_detail_stall_mutant.sv` fires -- the state is
  // unreachable with legal stimulus, because `d_ready_i` below is tied high, so
  // the leaf's II = 1 skid never fills.
  //
  // AND A LOST DELTA IS A LOST RELIEF, NEVER SOMEBODY ELSE'S. The record is
  // keyed by OWNER SLOT and sealed with the owner's GENERATION, read back with
  // the generation `zhao_texture_v3own` publishes at the OUTPUT -- so a row that
  // was never written for this fragment fails the compare and the delta reads
  // as exactly zero. The two sides of that compare are loaded by two different
  // enables in two different modules (the leaf's response register here, v3own's
  // output register there), which is the property CLAUDE.md's metadata-swap
  // chapter says to check first.
  logic nm_f_ready_w, nm_d_valid_w, nm_idle_w;
  logic signed [8:0] nm_d_delta_w;
  logic [15:0] nm_d_src_id_w;

  // THE DECLARATION AND THE LEVEL, held per owner slot from admission. They
  // arrive with the fragment and are needed several clocks later at the persp
  // beat, which is the same reason `sheet_m` and `material_m` below are records
  // and not wires: the island interleaves NCTX contexts, so a wire read at the
  // persp stage would hand fragment A's declaration to fragment B.
  localparam int unsigned DTL_DECLW = GENW + 5;   // {gen, declared, lod[3:0]}
  logic [DTL_DECLW-1:0] dtl_decl_m [0:OWNERS-1];
  always_ff @(posedge clk) begin
    if (own_adm_accept_w)
      dtl_decl_m[own_adm_owner_w[13:8]] <=
          {own_adm_owner_w[7:0], frag_detail_i, frag_lod_i[LODW-1 -: 4]};
  end

  wire [DTL_DECLW-1:0] dtl_decl_row_c = dtl_decl_m[persp_owner_w[13:8]];
  wire dtl_decl_gen_ok_c = (dtl_decl_row_c[DTL_DECLW-1 -: GENW] == persp_owner_w[7:0]);
  // Fail-safe in the direction that removes relief rather than inventing it: a
  // row whose generation does not match cannot be this fragment's declaration,
  // so the fragment is declared UNDETAILED and the leaf forces delta 0 with the
  // tile read enable held low.
  wire dtl_declared_c = dtl_decl_row_c[4] && dtl_decl_gen_ok_c;
  wire persp_xfer_c = persp_rsp_valid_w && persp_rsp_ready_w;

  // ONE WRITE PORT, TWO DESTINATIONS, EXPLICIT SELECTOR. See the port comment.
  wire nm_cfg_we_c = dtl_we_i && !dtl_sel_i;
  wire nm_tw_we_c  = dtl_we_i &&  dtl_sel_i;

  zhao_terrain_normalmap #(
      .SUNS(1), .LEVELS(7), .DELTA_SHIFT(22), .LODW(4)
  ) u_terrain_normalmap (
      .clk(clk), .rst_n(rst_n),
      .cfg_we_i  (nm_cfg_we_c),
      .cfg_addr_i(dtl_addr_i[2:0]),
      .cfg_data_i(dtl_data_i),
      .f_valid_i (persp_xfer_c),
      .f_ready_o (nm_f_ready_w),
      .f_u_i     (persp_u_w),
      .f_v_i     (persp_v_w),
      .f_detail_i(dtl_declared_c),
      .f_lod_i   (dtl_decl_row_c[3:0]),
      // The OWNER is the tag. It is 14 bits into a 16-bit port, and the two
      // pad bits are zero rather than don't-care so the returned tag compares
      // equal to the owner it was issued with.
      .f_src_id_i({2'd0, persp_owner_w}),
      .d_valid_o (nm_d_valid_w),
      // TIED HIGH, AND IT IS THE REASON THE TAP CANNOT STALL. The response
      // goes to a record and not to a stream, so there is nothing for it to
      // wait on. A block whose consumer never refuses cannot fill its own skid.
      .d_ready_i (1'b1),
      .d_delta_o (nm_d_delta_w),
      .d_src_id_o(nm_d_src_id_w),
      .tw_we_i   (nm_tw_we_c),
      .tw_addr_i (dtl_addr_i),
      .tw_data_i (dtl_data_i[15:0]),
      .fragments_o  (cnt_detail_fragments_o),
      .zeroed_o     (cnt_detail_zeroed_o),
      .railed_o     (cnt_detail_railed_o),
      .cold_o       (cnt_detail_cold_o),
      .table_ready_o(dtl_table_ready_o),
      .idle_o       (nm_idle_w));

  /* verilator lint_off UNUSEDSIGNAL */
  wire unused_nm_idle_w = nm_idle_w;
  /* verilator lint_on UNUSEDSIGNAL */

  // THE DELTA RECORD. Written only by the leaf's response, keyed by the owner
  // the request carried. NOT also written at admission: one array cannot take
  // two writers on one clock, and an admission-priority write would starve the
  // response on a busy frame while every counter balanced -- which is the exact
  // shape this repository has a chapter about. The generation compare at the
  // read does the work instead, and it does it for free.
  logic [GENW+8:0] dtl_delta_m [0:OWNERS-1];
  always_ff @(posedge clk) begin
    if (nm_d_valid_w)
      dtl_delta_m[nm_d_src_id_w[13:8]] <= {nm_d_src_id_w[7:0], nm_d_delta_w};
  end

  // The record is READ at the owner's output beat, which is declared far below
  // beside the sheet record -- see THE DELTA LEAVES WITH ITS OWNER there.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) err_detail_lost_o <= 32'd0;
    else if (persp_xfer_c && !nm_f_ready_w)
      err_detail_lost_o <= err_detail_lost_o + 32'd1;
  end

  // ---------------------------------------------------------------------------
  // Exact descriptor physical image and 301/78/365 owner join.
  logic desc_req_ready_w;
  logic [OWNERW-1:0] desc_owner_w;
  // The public MASKED descriptor output. Since Timing4 E1 the join is fed from
  // the raw row plus its verdict instead, so nothing in the synthesized island
  // consumes this any more -- it is retained because it is the leaf's public
  // contract for every other client, and because it gives the assertion below
  // an independently derived oracle for what the join must publish.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [286:0] desc_logical_w;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [286:0] desc_logical_raw_w;
  logic desc_rsp_ready_w;
  logic desc_owner_generation_ok_w, desc_pad_ok_w, desc_usable_w;
  logic [31:0] desc_pad_fault_w, desc_generation_mismatch_w;
  logic desc_frame_fault_w;
  logic [31:0] desc_writes_w, desc_reads_w;

  logic uvjoin_desc_ready_w, uvjoin_uv_ready_w, uvjoin_rsp_ready_w;
  logic [300:0] uvjoin_desc_data_w;
  logic [77:0] uvjoin_uv_data_w;
  logic [364:0] uvjoin_data_w;
  logic [31:0] uvjoin_owner_mismatch_w;
  logic uvjoin_lifetime_fault_w;

  assign desc_req_valid_w = persp_rsp_valid_w && uvjoin_uv_ready_w;
  assign uvjoin_uv_valid_w = persp_rsp_valid_w && desc_req_ready_w;
  assign persp_rsp_ready_w = desc_req_ready_w && uvjoin_uv_ready_w;
  assign uvjoin_uv_data_w = {persp_owner_w, persp_u_w, persp_v_w};

  zhao_texture_early_desc_v2 #(.SLOTW(6), .GENW(GENW)) u_early_desc (
      .clk(clk), .rst_n(rst_n), .frame_fault_clear_i(frame_fault_clear_w),
      .wr_valid_i(own_adm_accept_w), .wr_slot_i(own_adm_owner_w[13:8]),
      .wr_owner_generation_i(own_adm_owner_w[7:0]),
      .wr_aux_context_i(frag_aux_ctx_i), .wr_lod_q4_4_i(frag_lod_i),
      .wr_response_class_i(frag_class_i), .wr_aux_required_i(frag_aux_i),
      .wr_sample_count_i(frag_sample_count_i),
      .wr_palette_slot_i(frag_pal_slot_i),
      .wr_palette_generation_i(frag_pal_gen_i),
      .wr_mosaic_material_a_i(frag_base_rgb_i[23:16]),
      .wr_mosaic_material_b_i(frag_base_rgb_i[15:8]),
      .wr_mosaic_weight_i(frag_weight_i),
      .wr_binding_selector_i(frag_binding_i),
      .wr_active_page_generation_i(active_page_generation_o),
      .rd_valid_i(desc_req_valid_w), .rd_ready_o(desc_req_ready_w),
      .rd_owner_i(persp_owner_w), .rd_result_valid_o(desc_rsp_valid_w),
      .rd_result_ready_i(desc_rsp_ready_w), .rd_owner_o(desc_owner_w),
      .rd_logical_o(desc_logical_w),
      .rd_logical_raw_o(desc_logical_raw_w),
      .rd_owner_generation_ok_o(desc_owner_generation_ok_w),
      .rd_descriptor_pad_ok_o(desc_pad_ok_w),
      .rd_descriptor_usable_o(desc_usable_w),
      .desc_pad_fault_o(desc_pad_fault_w),
      .rd_generation_mismatch_o(desc_generation_mismatch_w),
      .writes_o(desc_writes_w), .reads_o(desc_reads_w),
      .frame_fault_o(desc_frame_fault_w), .idle_o(desc_idle_w));

  assign uvjoin_desc_valid_w = desc_rsp_valid_w;
  assign desc_rsp_ready_w = uvjoin_desc_ready_w;
  // TIMING4 E1: the RAW held row crosses into the join, and its usability
  // verdict crosses beside it as one bit on the same transfer. The join
  // registers both and masks from its own registered copy, which removes
  // generation-compare -> 287-bit mask -> join register from this cone.
  // desc_logical_w (the masked public output) keeps its own consumers and is
  // still what the trust witnesses below are recorded against.
  assign uvjoin_desc_data_w = {desc_owner_w, desc_logical_raw_w};

  zhao_texture_uv_join_v2 u_uv_join (
      .clk(clk), .rst_n(rst_n),
      .desc_valid_i(uvjoin_desc_valid_w), .desc_ready_o(uvjoin_desc_ready_w),
      .desc_usable_i(desc_usable_w),
      .desc_data_i(uvjoin_desc_data_w),
      .uv_valid_i(uvjoin_uv_valid_w), .uv_ready_o(uvjoin_uv_ready_w),
      .uv_data_i(uvjoin_uv_data_w),
      .out_valid_o(uvjoin_rsp_valid_w), .out_ready_i(uvjoin_rsp_ready_w),
      .out_data_o(uvjoin_data_w),
      .uvjoin_owner_mismatch_o(uvjoin_owner_mismatch_w),
      .lifetime_fault_o(uvjoin_lifetime_fault_w), .idle_o(uvjoin_idle_w));

  // The exact row46, separate admission-frozen malformed bit, independently
  // captured owner generation, and authoritative owner request mask are written
  // together on the one owner-admission edge.  The mask copy is the exact value
  // presented to u_own, not a second count/AUX derivation.
  logic [MATW-1:0] material_m [0:OWNERS-1];
  logic material_refused_m [0:OWNERS-1];
  logic [GENW-1:0] material_generation_m [0:OWNERS-1];
  logic [3:0] owner_required_mask_m [0:OWNERS-1];
  logic [GENW-1:0] owner_required_mask_generation_m [0:OWNERS-1];
  always_ff @(posedge clk) begin
    if (own_adm_accept_w) begin
      material_m[own_adm_owner_w[13:8]] <= {
          frag_base_rgb_i, frag_base_a_i, frag_weight_i,
          frag_recipe_i, frag_sample_count_i, frag_aux_i};
      material_refused_m[own_adm_owner_w[13:8]] <=
          !material_count_legal(frag_recipe_i, frag_sample_count_i);
      material_generation_m[own_adm_owner_w[13:8]] <= own_adm_owner_w[7:0];
      owner_required_mask_m[own_adm_owner_w[13:8]] <= admission_required_mask_w;
      owner_required_mask_generation_m[own_adm_owner_w[13:8]] <=
          `ZHAO_PACKET_B_OWNER_MASK_GENERATION(own_adm_owner_w[7:0]);
    end
  end

  // Timing4 admission event boundary. Outer lifetime levels continue to block
  // frag_ready/owner/RCP admission combinationally above; only a real accepted
  // full owner identity reaches the wide validation-pending table one edge later.
  logic owner_admission_event_valid_q;
  logic [OWNERW-1:0] owner_admission_event_owner_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      owner_admission_event_valid_q <= 1'b0;
      owner_admission_event_owner_q <= '0;
    end else begin
      owner_admission_event_valid_q <= own_adm_accept_w;
      if (own_adm_accept_w)
        owner_admission_event_owner_q <= own_adm_owner_w;
    end
  end

  // Descriptor trust is written only when the held descriptor response is
  // accepted by the UV join.  Generation seals the unreset per-slot payload.
  logic descriptor_usable_m [0:OWNERS-1];
  logic [GENW-1:0] descriptor_owner_generation_m [0:OWNERS-1];
  logic join_validation_pending_q [0:OWNERS-1];
  logic join_force_refuse_m [0:OWNERS-1];
  logic [GENW-1:0] join_validation_generation_m [0:OWNERS-1];
  wire descriptor_response_accept_c = desc_rsp_valid_w && desc_rsp_ready_w;
  always_ff @(posedge clk) begin
    if (descriptor_response_accept_c) begin
      descriptor_usable_m[desc_owner_w[13:8]] <= desc_usable_w;
      descriptor_owner_generation_m[desc_owner_w[13:8]] <= desc_owner_w[7:0];
    end
  end

  // A held synchronous read transports joined365 beside three independent
  // witnesses: owner-mask authority, material copy, and descriptor trust.
  logic material_join_valid_q;
  logic [364:0] material_join_payload_q;
  logic [MATW-1:0] material_join_row_q;
  logic material_join_refused_q;
  logic [GENW-1:0] material_join_generation_q;
  logic [3:0] material_join_owner_mask_q;
  logic [GENW-1:0] material_join_owner_mask_generation_q;
  logic material_join_descriptor_usable_q;
  logic [GENW-1:0] material_join_descriptor_generation_q;
  logic expand_frag_ready_w;

  wire [OWNERW-1:0] joined_owner_c = material_join_payload_q[364:351];
  wire [286:0] joined_descriptor_c = material_join_payload_q[350:64];
  wire signed [31:0] joined_u_c = material_join_payload_q[63:32];
  wire signed [31:0] joined_v_c = material_join_payload_q[31:0];
  wire joined_owner_mask_valid_c =
      material_join_owner_mask_generation_q == joined_owner_c[7:0];
  wire joined_material_valid_c =
      (material_join_generation_q == joined_owner_c[7:0]) &&
      !sim_material_fault_mode_w[1];
  wire joined_descriptor_trusted_c = material_join_descriptor_usable_q &&
      (material_join_descriptor_generation_q == joined_owner_c[7:0]) &&
      !sim_material_fault_mode_w[0];
  wire [3:0] joined_material_mask_c =
      required_mask_of(material_join_row_q[2:1], material_join_row_q[0]) ^
      (sim_material_fault_mode_w[2] ? 4'b0001 : 4'b0000);
  wire [3:0] joined_descriptor_mask_c =
      required_mask_of(joined_descriptor_c[236:235], joined_descriptor_c[234]);
  wire joined_copies_match_owner_c = joined_material_valid_c &&
      joined_descriptor_trusted_c &&
      (joined_material_mask_c == material_join_owner_mask_q) &&
      (joined_descriptor_mask_c == material_join_owner_mask_q);
  wire joined_force_refuse_c = material_join_refused_q ||
      !joined_copies_match_owner_c;
  wire material_join_consume_c = material_join_valid_q &&
      (!joined_owner_mask_valid_c || expand_frag_ready_w);
  wire material_join_room_c = !material_join_valid_q || material_join_consume_c;
  wire material_join_accept_c = uvjoin_rsp_valid_w && material_join_room_c;
  assign uvjoin_rsp_ready_w = material_join_room_c;

  // Count-zero owners can become owner-ready before their descriptor returns.
  // Hold every combine ticket behind this per-owner validation fence so a bad
  // descriptor/mask cannot escape as a clean base-colour PASSTHRU. The pending
  // bit is reset state; generation still seals the unreset refusal payload.
  wire join_validation_accept_c = material_join_valid_q &&
      joined_owner_mask_valid_c && expand_frag_ready_w;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int unsigned i = 0; i < OWNERS; i++)
        join_validation_pending_q[i] <= 1'b0;
    end else begin
      if (owner_admission_event_valid_q)
        join_validation_pending_q[
            owner_admission_event_owner_q[OWNERW-1 -: OWNER_SLOTW]] <= 1'b1;
      if (join_validation_accept_c) begin
        join_validation_pending_q[joined_owner_c[13:8]] <= 1'b0;
        join_force_refuse_m[joined_owner_c[13:8]] <= joined_force_refuse_c;
        join_validation_generation_m[joined_owner_c[13:8]] <=
            joined_owner_c[7:0];
      end
    end
  end

  always_ff @(posedge clk) begin
    if (material_join_accept_c) begin
      material_join_row_q <= material_m[uvjoin_data_w[364:359]];
      material_join_refused_q <= material_refused_m[uvjoin_data_w[364:359]];
      material_join_generation_q <=
          material_generation_m[uvjoin_data_w[364:359]];
      material_join_owner_mask_q <=
          owner_required_mask_m[uvjoin_data_w[364:359]];
      material_join_owner_mask_generation_q <=
          owner_required_mask_generation_m[uvjoin_data_w[364:359]];
      material_join_descriptor_usable_q <=
          descriptor_usable_m[uvjoin_data_w[364:359]];
      material_join_descriptor_generation_q <=
          descriptor_owner_generation_m[uvjoin_data_w[364:359]];
    end
  end
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      material_join_valid_q <= 1'b0;
      material_join_payload_q <= '0;
    end else if (material_join_room_c) begin
      material_join_valid_q <= uvjoin_rsp_valid_w;
      if (uvjoin_rsp_valid_w)
        material_join_payload_q <= uvjoin_data_w;
    end
  end

  logic combine_owner_mask_invalid_event_w;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)
      owner_mask_lifetime_fault_q <= 1'b0;
    else if ((material_join_valid_q && !joined_owner_mask_valid_c) ||
             combine_owner_mask_invalid_event_w)
      owner_mask_lifetime_fault_q <= 1'b1;
  end

  // Expander owns one held Mosaic side record for every accepted fragment.
  logic mosaic_req_ready_w;
  logic [OWNERW-1:0] mosaic_owner_w;
  logic signed [31:0] mosaic_u_w, mosaic_v_w;
  logic [7:0] mosaic_material_a_w, mosaic_material_b_w, mosaic_weight_w;
  logic [7:0] mosaic_tile_w;
  logic [5:0] mosaic_tx_w, mosaic_ty_w;
  logic [15:0] mosaic_src_w;

  assign expand_frag_valid_w = material_join_valid_q && joined_owner_mask_valid_c;

  zhao_texture_mosaic_v2 u_mosaic (
      .clk(clk), .rst_n(rst_n), .req_valid_i(mosaic_req_valid_w),
      .req_ready_o(mosaic_req_ready_w), .req_u_i(mosaic_u_w),
      .req_v_i(mosaic_v_w), .req_mat_a_i(mosaic_material_a_w),
      .req_mat_b_i(mosaic_material_b_w),
      .req_weight_i(mosaic_weight_w), .req_mosaic_i(1'b1),
      .req_src_id_i({2'b00, mosaic_owner_w}),
      .pick_valid_o(mosaic_rsp_valid_w), .pick_ready_i(1'b1),
      .pick_tile_o(mosaic_tile_w), .pick_tx_o(mosaic_tx_w),
      .pick_ty_o(mosaic_ty_w), .pick_src_id_o(mosaic_src_w),
      .idle_o(mosaic_idle_w), .texture_samples_o(cnt_mosaic_samples_o));

  // ---------------------------------------------------------------------------
  // THE PICK'S LANDING PLACE (2026-09-26, TERRAINTEX).
  //
  // Until today `pick_tile_o` had NO READER: `mosaic_tile_w`, `mosaic_tx_w` and
  // `mosaic_ty_w` occurred exactly twice each in this file -- declared above,
  // connected above -- and the frozen terrain_rules 6.2 pick was computed for
  // every fragment and thrown away. It is held here, per owner, and consumed by
  // `u_binding`, whose TILESET row turns it into the byte displacement
  // `zref::Tileset` gives a tile (`tiles[256][64*64]`). `pick_tx_o`/`pick_ty_o`
  // remain unread ON PURPOSE and are not a second half of this gap: they are the
  // mirrored texel pair, which the TMU recomputes from u/v under the row's own
  // mirror wrap at log2w = log2h = 6 -- the SAME fold, by the same law. Reading
  // them here would be a second implementation of `zref::terrain::mirror_texel`
  // sitting beside the one that ships, which is the duplication this tree has a
  // chapter about. They are left as the mosaic's public contract.
  //
  // WHY A PER-OWNER TABLE AND NOT A FIFO. It is the shape this island already
  // uses four times over for exactly this problem -- `material_m`,
  // `owner_required_mask_m`, `descriptor_usable_m`, `join_force_refuse_m` are
  // all per-owner rows sealed by the owner's generation -- and it needs no
  // argument about the order two independent handshakes retire in.
  //
  // AND THE SEAL IS NOT DECORATION. The mosaic answers TWO CYCLES after the
  // expander offers the fragment, while the SAMPLE job for the same fragment is
  // offered on the first of those cycles. So the pick is genuinely late, and an
  // ungated read would hand `u_binding` the PREVIOUS occupant of the slot --
  // which is CLAUDE.md's metadata-swap defect, with the detector wired to
  // operands that move together. `mosaic_pick_ready_c` therefore HOLDS the
  // request until this owner's own generation is the one stored. It cannot
  // deadlock: the expander offers the mosaic job on the same beat as the sample,
  // `u_mosaic` takes it whenever it can advance, and its `pick_ready_i` is a
  // constant one, so the pick always lands whether or not the sample moves.
  // DECLARED NOT EXPORTED -- see the note on `binding_tileset_samples_w`
  // below. Both are fired and asserted by exact amount at their own leaves
  // (`texture_mosaic_hold_directed`), which is where a leaf counter's
  // evidence belongs, and neither is named silently into the blanket
  // UNUSEDSIGNAL waiver this packet's own finding is about.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] mosaic_picks_held_w, mosaic_stale_slot_holds_w;
  /* verilator lint_on UNUSEDSIGNAL */
  logic        mosaic_pick_ready_c;
  logic [7:0]  mosaic_pick_tile_c;

  zhao_texture_mosaic_hold #(.SLOTW(OWNER_SLOTW), .GENW(GENW)) u_mosaic_hold (
      .clk(clk), .rst_n(rst_n),
      .pick_valid_i(mosaic_rsp_valid_w),
      .pick_src_id_i(mosaic_src_w[OWNERW-1:0]),
      .pick_tile_i(mosaic_tile_w),
      .smp_handle_i(expand_sample_handle_w),
      .pick_ready_o(mosaic_pick_ready_c),
      .pick_tile_o(mosaic_pick_tile_c),
      .picks_held_o(mosaic_picks_held_w),
      .stale_slot_holds_o(mosaic_stale_slot_holds_w));

  // ---------------------------------------------------------------------------
  // Fragment expansion and sealed binding resolution.
  logic expand_sample_ready_w;
  logic [SMPW-1:0] expand_sample_handle_w;
  logic [7:0] expand_sample_page_generation_w;
  logic expand_sample_selector_overflow_w, expand_sample_force_refuse_w;
  logic [7:0] expand_sample_binding_selector_w;
  logic signed [31:0] expand_sample_u_w, expand_sample_v_w;
  logic [7:0] expand_sample_lod_w;
  logic [1:0] expand_sample_class_witness_w, expand_sample_pal_slot_witness_w;
  logic [7:0] expand_sample_pal_gen_w;
  logic expand_aux_ready_w;
  logic [OWNERW-1:0] expand_aux_owner_w;
  logic [AUXCTXW-1:0] expand_aux_context_w;
  logic expand_aux_force_refuse_w;
  logic expand_issue_aux_valid_w;
  logic [OWNERW-1:0] expand_issue_aux_owner_w;
  logic [31:0] expand_fragments_w, expand_samples_w, expand_mosaic_jobs_w,
      expand_aux_jobs_w;
  logic [31:0] expand_zero_w, expand_malformed_w, expand_overflow_w;
  logic expand_frame_fault_w;

  zhao_texture_frag_expand_v2 #(.FQD(4), .SLOTW(6), .GENW(GENW)) u_expand (
      .clk(clk), .rst_n(rst_n), .frame_fault_clear_i(frame_fault_clear_w),
      .frag_valid_i(expand_frag_valid_w),
      .frag_ready_o(expand_frag_ready_w), .frag_owner_i(joined_owner_c),
      .frag_logical_descriptor_i(joined_descriptor_c),
      .frag_u_i(joined_u_c), .frag_v_i(joined_v_c),
      .frag_required_mask_i(material_join_owner_mask_q),
      .frag_material_refused_i(joined_force_refuse_c),
      .sample_valid_o(expand_sample_valid_w),
      .sample_ready_i(expand_sample_ready_w),
      .sample_handle_o(expand_sample_handle_w),
      .sample_page_generation_o(expand_sample_page_generation_w),
      .sample_selector_overflow_o(expand_sample_selector_overflow_w),
      .sample_force_refuse_o(expand_sample_force_refuse_w),
      .sample_binding_selector_o(expand_sample_binding_selector_w),
      .sample_u_o(expand_sample_u_w), .sample_v_o(expand_sample_v_w),
      .sample_lod_q4_4_o(expand_sample_lod_w),
      .sample0_class_witness_o(expand_sample_class_witness_w),
      .sample0_palette_slot_witness_o(expand_sample_pal_slot_witness_w),
      .sample0_palette_generation_witness_o(expand_sample_pal_gen_w),
      .mosaic_valid_o(mosaic_req_valid_w), .mosaic_ready_i(mosaic_req_ready_w),
      .mosaic_owner_o(mosaic_owner_w), .mosaic_u_o(mosaic_u_w),
      .mosaic_v_o(mosaic_v_w), .mosaic_material_a_o(mosaic_material_a_w),
      .mosaic_material_b_o(mosaic_material_b_w),
      .mosaic_weight_o(mosaic_weight_w),
      .aux_valid_o(expand_aux_valid_w), .aux_ready_i(expand_aux_ready_w),
      .aux_owner_o(expand_aux_owner_w), .aux_context_o(expand_aux_context_w),
      .aux_force_refuse_o(expand_aux_force_refuse_w),
      .iss_aux_valid_o(expand_issue_aux_valid_w),
      .iss_aux_owner_o(expand_issue_aux_owner_w),
      .fragments_accepted_o(expand_fragments_w),
      .sample_jobs_accepted_o(expand_samples_w),
      .mosaic_jobs_accepted_o(expand_mosaic_jobs_w),
      .aux_jobs_accepted_o(expand_aux_jobs_w),
      .zero_sample_fragments_o(expand_zero_w),
      .malformed_descriptors_o(expand_malformed_w),
      .wq_overflow_o(expand_overflow_w), .frame_fault_o(expand_frame_fault_w),
      .idle_o(expand_idle_w));

  logic binding_req_ready_w;
  logic binding_cfg_valid_w, binding_cfg_ready_w;
  logic binding_issue_tmu_valid_w;
  logic [SMPW-1:0] binding_issue_tmu_handle_w;
  logic binding_plan_ready_w;
  logic [TOKW-1:0] binding_plan_token_w;
  logic [31:0] binding_plan_base_w, binding_plan_mode_w;
  logic [1:0] binding_plan_pal_slot_w;
  logic [7:0] binding_plan_pal_gen_w;
  logic signed [31:0] binding_plan_u_w, binding_plan_v_w;
  logic [7:0] binding_plan_lod_w;
  logic binding_refuse_ready_w;
  logic [SMPW-1:0] binding_refuse_handle_w;
  logic [TEXTURE_RESULT_W-1:0] binding_refuse_result_w;
  logic [31:0] binding_jobs_w, binding_planner_jobs_w, binding_local_refused_w;
  logic [31:0] binding_selector_overflow_w, binding_generation_mismatch_w;
  logic [31:0] binding_invalid_row_w, binding_witness_mismatch_w;
  logic [31:0] binding_forced_refused_w, binding_cfg_errors_w;
  // DECLARED NOT EXPORTED, and said out loud rather than left for the
  // blanket UNUSEDSIGNAL waiver in `tests/shell/v3_closure_inherited.vlt` to
  // swallow -- that waiver is how the mosaic pick went unread for weeks and
  // this packet will not add to its pile silently. `tileset_samples_o` is the
  // RESOLVER's census of samples planned against a tileset row; it is fired by
  // stimulus and asserted by exact amount in
  // `tests/texture/texture_binding_resolver_v2_directed.cpp`, which is where a
  // leaf counter's evidence belongs. Exporting it would add an island output
  // port and a shell/tile-pipe/binner lane for a number that is ZERO in the
  // composed console today and will stay zero until terrain presents a
  // sampling material -- an uncashed cheque, not observability.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [31:0] binding_tileset_samples_w;
  /* verilator lint_on UNUSEDSIGNAL */
  logic binding_frame_fault_w;

  assign binding_req_valid_w = expand_sample_valid_w && mosaic_pick_ready_c;
  assign expand_sample_ready_w = binding_req_ready_w && mosaic_pick_ready_c;
  assign binding_plan_valid_w = plan_req_valid_w;
  assign binding_cfg_valid_w = cfg_valid_i && !lifetime_admission_block_w;
  assign cfg_ready_o = binding_cfg_ready_w && !lifetime_admission_block_w;

  zhao_texture_binding_resolver_v2 #(.MAXLOG2(11), .SLOTW(6), .GENW(GENW)) u_binding (
      .clk(clk), .rst_n(rst_n), .frame_fault_clear_i(frame_fault_clear_w),
      .cfg_valid_i(binding_cfg_valid_w), .cfg_ready_o(binding_cfg_ready_w),
      .cfg_op_i(cfg_op_i),
      .cfg_page_generation_i(cfg_page_generation_i),
      .cfg_selector_i(cfg_selector_i), .cfg_row_i(cfg_row_i),
      .cfg_crc32_i(cfg_crc32_i), .cfg_rsp_valid_o(cfg_rsp_valid_o),
      .cfg_rsp_ready_i(cfg_rsp_ready_i), .cfg_rsp_op_o(cfg_rsp_op_o),
      .cfg_rsp_status_o(cfg_rsp_status_o),
      .cfg_rsp_page_generation_o(cfg_rsp_page_generation_o),
      .data_quiet_i(data_quiet), .admission_enable_o(binding_admission_enable_w),
      .active_page_generation_o(active_page_generation_o),
      .cfg_loader_idle_o(binding_cfg_loader_idle_w),
      .binding_crc_busy_o(binding_crc_busy_w),
      .binding_seal_pending_o(binding_seal_pending_w),
      .req_valid_i(binding_req_valid_w), .req_ready_o(binding_req_ready_w),
      .req_sample_handle_i(expand_sample_handle_w),
      .req_page_generation_i(expand_sample_page_generation_w),
      .req_selector_overflow_i(expand_sample_selector_overflow_w),
      .req_force_refuse_i(expand_sample_force_refuse_w),
      .req_binding_selector_i(expand_sample_binding_selector_w),
      .req_mosaic_tile_i(mosaic_pick_tile_c),
      .req_u_i(expand_sample_u_w), .req_v_i(expand_sample_v_w),
      .req_lod_q4_4_i(expand_sample_lod_w),
      .req_sample0_class_witness_i(expand_sample_class_witness_w),
      .req_sample0_palette_slot_witness_i(expand_sample_pal_slot_witness_w),
      .req_sample0_palette_generation_witness_i(expand_sample_pal_gen_w),
      .iss_tmu_valid_o(binding_issue_tmu_valid_w),
      .iss_tmu_handle_o(binding_issue_tmu_handle_w),
      .plan_valid_o(plan_req_valid_w), .plan_ready_i(binding_plan_ready_w),
      .plan_route_token_o(binding_plan_token_w),
      .plan_base_o(binding_plan_base_w), .plan_mode_o(binding_plan_mode_w),
      .plan_palette_slot_o(binding_plan_pal_slot_w),
      .plan_palette_generation_o(binding_plan_pal_gen_w),
      .plan_u_o(binding_plan_u_w), .plan_v_o(binding_plan_v_w),
      .plan_lod_q4_4_o(binding_plan_lod_w),
      .refuse_valid_o(binding_refuse_valid_w),
      .refuse_ready_i(binding_refuse_ready_w),
      .refuse_sample_handle_o(binding_refuse_handle_w),
      .refuse_result_o(binding_refuse_result_w),
      .sample_jobs_accepted_o(binding_jobs_w),
      .planner_jobs_accepted_o(binding_planner_jobs_w),
      .local_refused_o(binding_local_refused_w),
      .selector_overflow_count_o(binding_selector_overflow_w),
      .page_generation_mismatch_o(binding_generation_mismatch_w),
      .invalid_row_o(binding_invalid_row_w),
      .witness_mismatch_o(binding_witness_mismatch_w),
      .forced_refused_o(binding_forced_refused_w),
      .tileset_samples_o(binding_tileset_samples_w),
      .cfg_errors_o(binding_cfg_errors_w), .binding_fault_o(binding_frame_fault_w),
      .data_idle_o(binding_data_idle_w));

  // ---------------------------------------------------------------------------
  // Planner, cache and synchronous per-sample metadata.
  logic plan_req_ready_w;
  logic plan_cache_ready_w;
  logic [3:0] plan_cache_enable_w;
  logic [127:0] plan_cache_address_w;
  logic [TOKW-1:0] plan_cache_token_w;
  logic [1:0] plan_cache_pal_slot_w;
  logic [7:0] plan_cache_pal_gen_w;
  logic plan_cache_filter_w, plan_cache_error_w;
  logic [3:0] plan_cache_nibble_w;
  logic [7:0] plan_cache_fu_w, plan_cache_fv_w;
  logic [2:0] plan_cache_format_w;
  logic [3:0] plan_occupancy_w;

  assign binding_plan_ready_w = plan_req_ready_w;
  zhao_texture_tmu_plan_v2 #(.PAL_CARRY(1'b1), .SRCW(SRCW), .MAXLOG2(11)) u_plan (
      .clk(clk), .rst_n(rst_n), .req_valid_i(plan_req_valid_w),
      .req_ready_o(plan_req_ready_w), .req_u_i(binding_plan_u_w),
      .req_v_i(binding_plan_v_w), .req_base_i(binding_plan_base_w),
      .req_mode_i(binding_plan_mode_w), .req_lod_i(binding_plan_lod_w),
      .req_src_id_i(binding_plan_token_w),
      .req_pal_slot_i(binding_plan_pal_slot_w),
      .req_pal_gen_i(binding_plan_pal_gen_w),
      .acc_valid_o(plan_cache_valid_w), .acc_ready_i(plan_cache_ready_w),
      .acc_en_o(plan_cache_enable_w), .acc_addr_o(plan_cache_address_w),
      .acc_src_id_o(plan_cache_token_w),
      .acc_pal_slot_o(plan_cache_pal_slot_w),
      .acc_pal_gen_o(plan_cache_pal_gen_w),
      .acc_filter_o(plan_cache_filter_w), .acc_err_o(plan_cache_error_w),
      .acc_nib_o(plan_cache_nibble_w), .acc_fu_o(plan_cache_fu_w),
      .acc_fv_o(plan_cache_fv_w), .acc_fmt_o(plan_cache_format_w),
      .accepted_o(cnt_plan_accepted_o), .occupancy_o(plan_occupancy_w),
      .idle_o(plan_idle_w));

  logic cache_req_ready_w;
  logic cache_rsp_ready_w;
  logic [DATAW-1:0] cache_rsp_data_w;
  logic [7:0] cache_rsp_status_w;
  logic [TOKW-1:0] cache_rsp_token_w;
  logic [TOKW-1:0] cache_request_token_w;
  wire [TOKW-1:0] cache_checked_token_w =
      `ZHAO_PACKET_B_CACHE_TOKEN(cache_rsp_token_w);
  logic [31:0] cache_fills_w, cache_multicast_w, cache_replays_w;
  logic cache_fill_protocol_fault_w;
  logic [31:0] cache_jobs_accepted_w, cache_jobs_completed_w;
  logic [31:0] cache_fill_jobs_accepted_w, cache_fill_jobs_completed_w;
  logic [31:0] cache_fill_jobs_refused_w, cache_fill_data_beats_w;
  logic [31:0] cache_reservation_count_w;
  logic [6:0] cache_reservation_owner_state_w;
  logic [8:0] cache_work_state_w;
  assign cache_req_valid_w = plan_cache_valid_w;
  assign plan_cache_ready_w = cache_req_ready_w;
  assign cache_request_token_w = sim_packet_e_cache_class_override_en_w
      ? {sim_packet_e_cache_class_override_w, plan_cache_token_w[15:0]}
      : plan_cache_token_w;

  zhao_texture_cache_pipe_v2 #(
      .LANES(LANES), .LINES(16), .LINE_BYTES(16), .REQN(4), .SRCW(SRCW)
  ) u_cache (
      .clk(clk), .rst_n(rst_n), .acc_valid_i(cache_req_valid_w),
      .acc_ready_o(cache_req_ready_w), .acc_en_i(plan_cache_enable_w),
      .acc_addr_i(plan_cache_address_w), .acc_src_id_i(cache_request_token_w),
      .smp_valid_o(cache_rsp_valid_w), .smp_ready_i(cache_rsp_ready_w),
      .smp_data_o(cache_rsp_data_w), .smp_status_o(cache_rsp_status_w),
      .smp_src_id_o(cache_rsp_token_w),
      .fill_valid_o(fill_req_valid_o), .fill_ready_i(fill_req_ready_i),
      .fill_addr_o(fill_req_addr_o), .fill_data_valid_i(fill_data_valid_i),
      .fill_data_i(fill_data_i),
`ifdef ZHAO_PACKET_E_MUTANT_PRE_E_FILL_LIFETIME
      .fill_refused_i(1'b0),
`else
      .fill_refused_i(fill_refused_i),
`endif
      .frame_fault_clear_i(frame_fault_clear_w),
      .cache_hits_o(cnt_cache_hits_o), .cache_misses_o(cnt_cache_misses_o),
      .fills_o(cache_fills_w), .multicast_o(cache_multicast_w),
      .replays_o(cache_replays_w),
      .fill_protocol_fault_o(cache_fill_protocol_fault_w),
      .cache_jobs_accepted_o(cache_jobs_accepted_w),
      .cache_jobs_completed_o(cache_jobs_completed_w),
      .fill_jobs_accepted_o(cache_fill_jobs_accepted_w),
      .fill_jobs_completed_o(cache_fill_jobs_completed_w),
      .fill_jobs_refused_o(cache_fill_jobs_refused_w),
      .fill_data_beats_o(cache_fill_data_beats_w),
      .reservation_count_o(cache_reservation_count_w),
      .reservation_owner_state_o(cache_reservation_owner_state_w),
      .cache_work_state_o(cache_work_state_w), .idle_o(cache_idle_w));

  logic metadata_read_launch_w;
  logic metadata_read_pending_q;
  logic metadata_hold_valid_q;
  logic [39:0] metadata_hold_q;
  logic [TOKW-1:0] metadata_expected_token_q;
  logic metadata_rd_result_valid_w;
  logic [1:0] metadata_pal_slot_w;
  logic [GENW-1:0] metadata_pal_gen_w;
  logic [2:0] metadata_format_w;
  logic [7:0] metadata_fu_w, metadata_fv_w;
  logic metadata_byte_sel_w, metadata_nibble_w;
  logic [GENW-1:0] metadata_owner_gen_w;
  logic [31:0] metadata_writes_w, metadata_reads_w, metadata_illegal_w;
  logic [31:0] metadata_generation_mismatch_w;
  logic cache_raw_accept_w;

  // A correct cache holds its complete response until acceptance. This shipped
  // detector observes that boundary independently, so a future drop/overwrite
  // cannot leave the compatibility alarm reassuringly tied low.
  wire [DATAW-1:0] cache_rsp_observed_data_c =
      `ZHAO_PACKET_B_RSP_OBS_DATA(cache_rsp_data_w, metadata_read_pending_q);
  logic cache_rsp_stalled_q;
  logic [DATAW-1:0] cache_rsp_stalled_data_q;
  logic [7:0] cache_rsp_stalled_status_q;
  logic [TOKW-1:0] cache_rsp_stalled_token_q;
  wire cache_rsp_hold_violation_c = cache_rsp_stalled_q &&
      (!cache_rsp_valid_w ||
       (cache_rsp_observed_data_c != cache_rsp_stalled_data_q) ||
       (cache_rsp_status_w != cache_rsp_stalled_status_q) ||
       (cache_checked_token_w != cache_rsp_stalled_token_q));
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      cache_rsp_stalled_q <= 1'b0;
      cache_rsp_stalled_data_q <= '0;
      cache_rsp_stalled_status_q <= '0;
      cache_rsp_stalled_token_q <= '0;
      err_rsp_dropped_o <= 1'b0;
    end else begin
      if (cache_rsp_hold_violation_c)
        err_rsp_dropped_o <= 1'b1;
      cache_rsp_stalled_q <= cache_rsp_valid_w && !cache_rsp_ready_w;
      if (cache_rsp_valid_w && !cache_rsp_ready_w && !cache_rsp_stalled_q) begin
        cache_rsp_stalled_data_q <= cache_rsp_observed_data_c;
        cache_rsp_stalled_status_q <= cache_rsp_status_w;
        cache_rsp_stalled_token_q <= cache_checked_token_w;
      end
    end
  end

  wire cache_status_refusal_c =
      `ZHAO_PACKET_E_CACHE_STATUS_IS_REFUSAL(cache_rsp_status_w);
  wire cache_sidx3_c = cache_rsp_valid_w &&
      (cache_checked_token_w[9:8] == 2'd3);
  wire cache_sidx3_drop_c = cache_sidx3_c && cache_rsp_ready_w;

  assign metadata_read_launch_w = cache_rsp_valid_w && !cache_sidx3_c &&
      !cache_status_refusal_c && !metadata_read_pending_q &&
      !metadata_hold_valid_q;
  assign metajoin_a_valid_w = metadata_read_launch_w;
  assign metajoin_b_valid_w = metadata_rd_result_valid_w;
  assign metajoin_rsp_valid_w = metadata_hold_valid_q ||
      metadata_rd_result_valid_w;

  zhao_texture_metajoin_v2 #(
      .SLOTW(6), .SIDXW(2), .GENW(GENW),
      .PALSW($clog2(PAL_SLOTS)), .METAW(40)
  ) u_metajoin (
      .clk(clk), .rst_n(rst_n),
      .wr_valid_i(plan_cache_valid_w && plan_cache_ready_w),
      .wr_slot_i(plan_cache_token_w[15:10]),
      .wr_sidx_i(plan_cache_token_w[9:8]),
      .wr_owner_gen_i(plan_cache_token_w[7:0]),
      .wr_pal_slot_i(plan_cache_pal_slot_w),
      .wr_pal_gen_i(plan_cache_pal_gen_w), .wr_format_i(plan_cache_format_w),
      .wr_frac_u_i(plan_cache_fu_w), .wr_frac_v_i(plan_cache_fv_w),
      .wr_byte_sel_i(plan_cache_address_w[0]),
      .wr_nibble_i(plan_cache_nibble_w[0]),
      .rd_valid_i(metadata_read_launch_w),
      .rd_slot_i(cache_checked_token_w[15:10]),
      .rd_sidx_i(cache_checked_token_w[9:8]),
      .rd_owner_gen_i(cache_checked_token_w[7:0]),
      .rd_result_valid_o(metadata_rd_result_valid_w),
      .rd_pal_slot_o(metadata_pal_slot_w), .rd_pal_gen_o(metadata_pal_gen_w),
      .rd_format_o(metadata_format_w), .rd_frac_u_o(metadata_fu_w),
      .rd_frac_v_o(metadata_fv_w), .rd_byte_sel_o(metadata_byte_sel_w),
      .rd_nibble_o(metadata_nibble_w), .rd_owner_gen_o(metadata_owner_gen_w),
      .writes_o(metadata_writes_w), .reads_o(metadata_reads_w),
      .rd_illegal_sidx_o(metadata_illegal_w),
      .rd_gen_mismatch_o(metadata_generation_mismatch_w),
      .idle_o(metajoin_idle_w));

  wire [39:0] metadata_result_packed_c = {
      metadata_owner_gen_w, metadata_pal_slot_w, metadata_pal_gen_w,
      metadata_format_w, metadata_fv_w, metadata_fu_w,
      metadata_byte_sel_w, metadata_nibble_w, 1'b0};
  wire metadata_active_valid_c = metadata_hold_valid_q ||
      metadata_rd_result_valid_w;
  wire [39:0] metadata_active_c = metadata_hold_valid_q
      ? metadata_hold_q : metadata_result_packed_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      metadata_read_pending_q <= 1'b0;
      metadata_hold_valid_q <= 1'b0;
      metadata_hold_q <= '0;
      metadata_expected_token_q <= '0;
    end else begin
      if (metadata_read_launch_w) begin
        metadata_read_pending_q <= 1'b1;
        metadata_expected_token_q <= cache_checked_token_w;
      end
      if (metadata_rd_result_valid_w) begin
        metadata_read_pending_q <= 1'b0;
        metadata_hold_valid_q <= 1'b1;
        metadata_hold_q <= metadata_result_packed_c;
      end
      if (cache_raw_accept_w)
        metadata_hold_valid_q <= 1'b0;
    end
  end

  // The migration laboratory keeps an independent metadata image keyed by the
  // accepted planner token.  Functional routing never reads it.  With shadows
  // disabled neither the table nor comparison cones have a live writer/reader.
  logic [39:0] shadow_metadata_m [0:OWNERS-1][0:2];
  wire shadow_metadata_write_c = plan_cache_valid_w && plan_cache_ready_w;
  wire [39:0] shadow_write_metadata_c = {
      plan_cache_token_w[7:0], plan_cache_pal_slot_w, plan_cache_pal_gen_w,
      plan_cache_format_w, plan_cache_fv_w, plan_cache_fu_w,
      plan_cache_address_w[0], plan_cache_nibble_w[0], 1'b0};
  wire [39:0] shadow_expected_metadata_c =
      shadow_metadata_m[metadata_expected_token_q[15:10]]
                       [metadata_expected_token_q[9:8]];
  wire shadow_metadata_mismatch_c = metadata_rd_result_valid_w &&
      (metadata_result_packed_c != shadow_expected_metadata_c);
  assign shadow_present_o = MIGRATION_SHADOWS;

  generate
    if (MIGRATION_SHADOWS) begin : g_migration_shadows
      always_ff @(posedge clk) begin
        if (shadow_metadata_write_c)
          shadow_metadata_m[plan_cache_token_w[15:10]]
                           [plan_cache_token_w[9:8]] <=
              `ZHAO_PACKET_B_SHADOW_WRITE(shadow_write_metadata_c);
      end
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          meta_shadow_mismatch_o <= 32'd0;
          meta_shadow_reads_o <= 32'd0;
          meta_align_err_o <= 32'd0;
          meta_align_chk_o <= 32'd0;
          meta_bil_err_o <= 32'd0;
          meta_bil_chk_o <= 32'd0;
          meta_near_err_o <= 32'd0;
          meta_near_chk_o <= 32'd0;
          meta_bil_first_q_o <= 21'd0;
          meta_bil_first_t_o <= 21'd0;
          meta_bil_first_tok_o <= 18'd0;
        end else if (metadata_rd_result_valid_w) begin
          meta_shadow_reads_o <= meta_shadow_reads_o + 32'd1;
          if (shadow_metadata_mismatch_c)
            meta_shadow_mismatch_o <= meta_shadow_mismatch_o + 32'd1;
          unique case (metadata_expected_token_q[17:16])
            CLS_CLUT: begin
              meta_align_chk_o <= meta_align_chk_o + 32'd1;
              if (shadow_metadata_mismatch_c)
                meta_align_err_o <= meta_align_err_o + 32'd1;
            end
            CLS_NEAR: begin
              meta_near_chk_o <= meta_near_chk_o + 32'd1;
              if (shadow_metadata_mismatch_c)
                meta_near_err_o <= meta_near_err_o + 32'd1;
            end
            CLS_BIL: begin
              meta_bil_chk_o <= meta_bil_chk_o + 32'd1;
              if (shadow_metadata_mismatch_c) begin
                meta_bil_err_o <= meta_bil_err_o + 32'd1;
                if (meta_bil_err_o == 32'd0) begin
                  meta_bil_first_q_o <= metadata21(metadata_result_packed_c);
                  meta_bil_first_t_o <= metadata21(shadow_expected_metadata_c);
                  meta_bil_first_tok_o <= metadata_expected_token_q;
                end
              end
            end
            default: begin end
          endcase
        end
      end
    end else begin : g_no_migration_shadows
      assign meta_shadow_mismatch_o = 32'd0;
      assign meta_shadow_reads_o = 32'd0;
      assign meta_align_err_o = 32'd0;
      assign meta_align_chk_o = 32'd0;
      assign meta_bil_err_o = 32'd0;
      assign meta_bil_chk_o = 32'd0;
      assign meta_near_err_o = 32'd0;
      assign meta_near_chk_o = 32'd0;
      assign meta_bil_first_q_o = 21'd0;
      assign meta_bil_first_t_o = 21'd0;
      assign meta_bil_first_tok_o = 18'd0;
    end
  endgenerate

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)
      cache_sidx3_lifetime_fault_q <= 1'b0;
    else if (cache_sidx3_drop_c)
      cache_sidx3_lifetime_fault_q <= 1'b1;
  end

  // ---------------------------------------------------------------------------
  // Direct held-cache class steering and immutable final class tuples.
  wire [1:0] raw_class_c = cache_checked_token_w[17:16];
  wire [7:0] raw_byte_c = metadata_active_c[2]
      ? cache_rsp_data_w[15:8] : cache_rsp_data_w[7:0];
  wire [7:0] raw_index_c = (metadata_active_c[21:19] == FMT_CLUT4)
      ? (metadata_active_c[1] ? {4'd0, raw_byte_c[7:4]}
                            : {4'd0, raw_byte_c[3:0]})
      : raw_byte_c;

  logic palette_req_ready_w;
  logic palette_load_valid_w, palette_load_ready_w;
  logic [TUPLEW-1:0] palette_seed_tuple_w;
  logic [TUPLEW-1:0] palette_rsp_tuple_w;
  logic palette_rsp_ready_w;
  logic [31:0] palette_err_write_w, palette_err_same_gen_w;
  logic [31:0] palette_err_incomplete_w, palette_err_crc_w, palette_loads_ok_w;

  assign palette_req_valid_w = cache_rsp_valid_w && metadata_active_valid_c &&
      (raw_class_c == CLS_CLUT);
  assign palette_load_valid_w = pal_load_valid_i && !lifetime_admission_block_w;
  assign pal_load_ready_o = palette_load_ready_w && !lifetime_admission_block_w;
  assign palette_seed_tuple_w = {
      cache_checked_token_w, 8'h00, raw_index_c, 8'hff, 24'h000000};

  zhao_texture_palette_res_v2 #(
      .SLOTS(PAL_SLOTS), .ENTRIES(PAL_ENTRIES), .GENW(GENW), .TOKW(TOKW)
  ) u_palette (
      .clk(clk), .rst_n(rst_n), .ld_valid_i(palette_load_valid_w),
      .ld_ready_o(palette_load_ready_w), .ld_op_i(pal_load_op_i),
      .ld_slot_i(pal_load_slot_i), .ld_gen_i(pal_load_gen_i),
      .ld_idx_i(pal_load_idx_i), .ld_rgb565_i(pal_load_rgb565_i),
      .ld_crc_ok_i(pal_load_crc_ok_i), .req_valid_i(palette_req_valid_w),
      .req_ready_o(palette_req_ready_w), .req_tuple_i(palette_seed_tuple_w),
      .req_slot_i(metadata_active_c[31:30]), .req_gen_i(metadata_active_c[29:22]),
      .rsp_valid_o(palette_rsp_valid_w), .rsp_ready_i(palette_rsp_ready_w),
      .rsp_tuple_o(palette_rsp_tuple_w), .idle_o(palette_idle_w),
      .cfg_idle_o(palette_cfg_idle_w), .lookups_o(cnt_palette_lookups_o),
      .stale_o(cnt_palette_stale_o), .cold_o(cnt_palette_cold_o),
      .err_write_outside_o(palette_err_write_w),
      .err_same_gen_o(palette_err_same_gen_w),
      .err_incomplete_o(palette_err_incomplete_w),
      .err_crc_o(palette_err_crc_w), .loads_ok_o(palette_loads_ok_w));

  logic near_terminal_valid_q;
  logic [TUPLEW-1:0] near_terminal_tuple_q;
  logic near_dispatch_ready_w;
  wire near_terminal_room_c = !near_terminal_valid_q || near_dispatch_ready_w;
  wire [31:0] near_decoded_c = decode16(
      cache_rsp_data_w[15:0], metadata_active_c[21:19]);
  wire near_bad_format_c = !format_is_direct(metadata_active_c[21:19]);
  wire near_capture_c = cache_rsp_valid_w && metadata_active_valid_c &&
      (raw_class_c == CLS_NEAR) && near_terminal_room_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      near_terminal_valid_q <= 1'b0;
      near_terminal_tuple_q <= '0;
      cnt_near_refused_o <= 32'd0;
    end else if (near_terminal_room_c) begin
      near_terminal_valid_q <= near_capture_c;
      if (near_capture_c) begin
        near_terminal_tuple_q <= near_bad_format_c
            ? {cache_checked_token_w, 8'h01, 8'h00, 8'hff, 24'hff00ff}
            : {cache_checked_token_w, 8'h00, 8'h00,
               near_decoded_c[31:24], near_decoded_c[23:0]};
        if (near_bad_format_c)
          cnt_near_refused_o <= cnt_near_refused_o + 32'd1;
      end
    end
  end

  // Four physical bilerp lanes, one each for R/G/B/A.  A small typed collector
  // accepts their independently stalled outputs and creates one held tuple.
  logic bil_busy_q;
  logic [DATAW-1:0] bil_data_q;
  logic [TOKW-1:0] bil_token_q;
  logic [39:0] bil_metadata_q;
  logic [3:0] bil_request_pending_q;
  logic [3:0] bil_seen_q;
  logic bil_identity_bad_q;
  logic [7:0] bil_channel_q [0:3];
  logic [3:0] bilerp_req_ready_w;
  logic [7:0] bilerp_out_w [0:3];
  logic [TOKW-1:0] bilerp_out_token_w [0:3];
  logic [1:0] bilerp_out_channel_w [0:3];
  logic [31:0] bilerp_jobs_w [0:3];
  logic [1:0] bilerp_occupancy_w [0:3];
  logic bil_terminal_valid_q;
  logic [TUPLEW-1:0] bil_terminal_tuple_q;
  logic bil_dispatch_ready_w;
  wire bil_terminal_room_c = !bil_terminal_valid_q || bil_dispatch_ready_w;
  wire bil_capture_c = cache_rsp_valid_w && metadata_active_valid_c &&
      (raw_class_c == CLS_BIL) && !bil_busy_q;

  assign bilerp_req_valid_w = bil_request_pending_q;
  genvar bil_channel;
  generate
    for (bil_channel = 0; bil_channel < 4; bil_channel = bil_channel + 1) begin : g_bilerp
      if (BILERP_DSP2) begin : g_dsp2
        zhao_texture_bilerp_lane_dsp2 #(.TOKW(TOKW)) u_bilerp (
            .clk(clk), .rst_n(rst_n),
            .job_valid_i(bilerp_req_valid_w[bil_channel]),
            .job_ready_o(bilerp_req_ready_w[bil_channel]),
            .t00_i(decoded_channel(bil_data_q[15:0], bil_metadata_q[21:19], 2'(bil_channel))),
            .t10_i(decoded_channel(bil_data_q[31:16], bil_metadata_q[21:19], 2'(bil_channel))),
            .t01_i(decoded_channel(bil_data_q[47:32], bil_metadata_q[21:19], 2'(bil_channel))),
            .t11_i(decoded_channel(bil_data_q[63:48], bil_metadata_q[21:19], 2'(bil_channel))),
            .fu_i(bil_metadata_q[10:3]), .fv_i(bil_metadata_q[18:11]),
            .tok_i(bil_token_q), .chan_i(2'(bil_channel)),
            .out_valid_o(bilerp_rsp_valid_w[bil_channel]),
            .out_ready_i(bil_busy_q && !bil_seen_q[bil_channel] && bil_terminal_room_c),
            .out_o(bilerp_out_w[bil_channel]),
            .out_tok_o(bilerp_out_token_w[bil_channel]),
            .out_chan_o(bilerp_out_channel_w[bil_channel]),
            .idle_o(bilerp_lane_idle_w[bil_channel]),
            .jobs_o(bilerp_jobs_w[bil_channel]),
            .occupancy_o(bilerp_occupancy_w[bil_channel]));
      end else begin : g_v2
        zhao_texture_bilerp_lane_v2 #(.TOKW(TOKW)) u_bilerp (
            .clk(clk), .rst_n(rst_n),
            .job_valid_i(bilerp_req_valid_w[bil_channel]),
            .job_ready_o(bilerp_req_ready_w[bil_channel]),
            .t00_i(decoded_channel(bil_data_q[15:0], bil_metadata_q[21:19], 2'(bil_channel))),
            .t10_i(decoded_channel(bil_data_q[31:16], bil_metadata_q[21:19], 2'(bil_channel))),
            .t01_i(decoded_channel(bil_data_q[47:32], bil_metadata_q[21:19], 2'(bil_channel))),
            .t11_i(decoded_channel(bil_data_q[63:48], bil_metadata_q[21:19], 2'(bil_channel))),
            .fu_i(bil_metadata_q[10:3]), .fv_i(bil_metadata_q[18:11]),
            .tok_i(bil_token_q), .chan_i(2'(bil_channel)),
            .out_valid_o(bilerp_rsp_valid_w[bil_channel]),
            .out_ready_i(bil_busy_q && !bil_seen_q[bil_channel] && bil_terminal_room_c),
            .out_o(bilerp_out_w[bil_channel]),
            .out_tok_o(bilerp_out_token_w[bil_channel]),
            .out_chan_o(bilerp_out_channel_w[bil_channel]),
            .idle_o(bilerp_lane_idle_w[bil_channel]),
            .jobs_o(bilerp_jobs_w[bil_channel]),
            .occupancy_o(bilerp_occupancy_w[bil_channel]));
      end
    end
  endgenerate

  logic [3:0] bil_seen_next_c;
  logic [7:0] bil_channel_next_c [0:3];
  logic bil_identity_bad_next_c;
  logic bil_identity_mismatch_event_w;
  logic bil_complete_c;
  always_comb begin
    bil_seen_next_c = bil_seen_q;
    bil_identity_bad_next_c = bil_identity_bad_q;
    bil_identity_mismatch_event_w = 1'b0;
    for (int unsigned channel = 0; channel < 4; channel++) begin
      bil_channel_next_c[channel] = bil_channel_q[channel];
      if (bilerp_rsp_valid_w[channel] && bil_busy_q &&
          !bil_seen_q[channel] && bil_terminal_room_c) begin
        bil_seen_next_c[channel] = 1'b1;
        bil_channel_next_c[channel] = bilerp_out_w[channel];
        if ((`ZHAO_PACKET_B_BILERP_OBS_TOKEN(
                 bilerp_out_token_w[channel], channel) != bil_token_q) ||
            (bilerp_out_channel_w[channel] != 2'(channel))) begin
          bil_identity_bad_next_c = 1'b1;
          bil_identity_mismatch_event_w = 1'b1;
        end
      end
    end
    bil_complete_c = bil_busy_q && bil_terminal_room_c && (&bil_seen_next_c);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      bil_busy_q <= 1'b0;
      bil_data_q <= '0;
      bil_token_q <= '0;
      bil_metadata_q <= '0;
      bil_request_pending_q <= 4'b0000;
      bil_seen_q <= 4'b0000;
      bil_identity_bad_q <= 1'b0;
      bil_terminal_valid_q <= 1'b0;
      bil_terminal_tuple_q <= '0;
      err_bil_chan_o <= 1'b0;
      for (int unsigned channel = 0; channel < 4; channel++)
        bil_channel_q[channel] <= 8'd0;
    end else begin
      if (bil_capture_c) begin
        bil_busy_q <= 1'b1;
        bil_data_q <= cache_rsp_data_w;
        bil_token_q <= cache_checked_token_w;
        bil_metadata_q <= metadata_active_c;
        bil_request_pending_q <= 4'b1111;
        bil_seen_q <= 4'b0000;
        bil_identity_bad_q <= 1'b0;
      end else begin
        bil_seen_q <= bil_seen_next_c;
        bil_identity_bad_q <= bil_identity_bad_next_c;
        if (bil_identity_mismatch_event_w)
          err_bil_chan_o <= 1'b1;
        for (int unsigned channel = 0; channel < 4; channel++) begin
          bil_channel_q[channel] <= bil_channel_next_c[channel];
          if (bilerp_req_valid_w[channel] && bilerp_req_ready_w[channel])
            bil_request_pending_q[channel] <= 1'b0;
        end
      end

      if (bil_terminal_room_c) begin
        if (bil_complete_c) begin
          bil_terminal_valid_q <= 1'b1;
          bil_terminal_tuple_q <= bil_identity_bad_next_c
              ? {bil_token_q, 8'h01, 8'h00, 8'hff, 24'hff00ff}
              : {bil_token_q, 8'h00, 8'h00,
                 bil_channel_next_c[3], bil_channel_next_c[0],
                 bil_channel_next_c[1], bil_channel_next_c[2]};
          bil_busy_q <= 1'b0;
          bil_request_pending_q <= 4'b0000;
          bil_seen_q <= 4'b0000;
          bil_identity_bad_q <= 1'b0;
        end else if (bil_terminal_valid_q && bil_dispatch_ready_w) begin
          bil_terminal_valid_q <= 1'b0;
        end
      end
    end
  end
  assign cnt_bilerp_jobs_o = bilerp_jobs_w[0] + bilerp_jobs_w[1] +
      bilerp_jobs_w[2] + bilerp_jobs_w[3];

  // One held ERR completion accepts either a resolver-local refusal or a raw
  // class-3 response.  The resolver wins; raw cache data remains held.
  logic err_terminal_valid_q;
  logic [TUPLEW-1:0] err_terminal_tuple_q;
  logic err_dispatch_ready_w;
  wire err_terminal_room_c = !err_terminal_valid_q || err_dispatch_ready_w;
  wire binding_refuse_capture_c = binding_refuse_valid_w && err_terminal_room_c;
  wire raw_err_capture_c = cache_rsp_valid_w && metadata_active_valid_c &&
      (raw_class_c == CLS_ERR) && err_terminal_room_c && !binding_refuse_valid_w;
  assign binding_refuse_ready_w = err_terminal_room_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      err_terminal_valid_q <= 1'b0;
      err_terminal_tuple_q <= '0;
      err_unknown_class_o <= 32'd0;
    end else if (err_terminal_room_c) begin
      err_terminal_valid_q <= binding_refuse_capture_c || raw_err_capture_c;
      if (binding_refuse_capture_c)
        err_terminal_tuple_q <= {CLS_ERR, binding_refuse_handle_w,
                                 binding_refuse_result_w};
      else if (raw_err_capture_c) begin
        err_terminal_tuple_q <= {cache_checked_token_w, 8'h01, 8'h00,
                                 8'hff, 24'hff00ff};
        err_unknown_class_o <= err_unknown_class_o + 32'd1;
      end
    end
  end

  // ---------------------------------------------------------------------------
  // Packet-E typed cache refusal bank and fair physical-class merges.
  //
  // Status-zero cache responses retain the Packet-B metadata/native path above.
  // A nonzero response is accepted only when the one-entry slot named by the
  // ORIGINAL token class can capture it.  Every slot holds the complete 66-bit
  // tuple while stalled.  Its per-class arbiter changes preference only after a
  // genuinely contended acceptance, so neither native nor refusal traffic can
  // starve the other and no class is relabelled to ERR.
  logic [3:0] refusal_valid_q;
  logic [TUPLEW-1:0] refusal_tuple_q [0:3];
  logic [3:0] refusal_room_w;
  logic [3:0] refusal_capture_w;
  logic [3:0] refusal_merge_ready_w;
  logic [3:0] class_native_valid_w;
  logic [TUPLEW-1:0] class_native_tuple_w [0:3];
  logic [3:0] class_native_ready_w;
  logic [3:0] class_merge_valid_w;
  logic [TUPLEW-1:0] class_merge_tuple_w [0:3];
  logic [3:0] class_merge_select_refusal_w;
  logic [3:0] class_merge_accept_w;
  logic [3:0] class_dispatch_ready_w;
  logic [3:0] class_dispatch_offer_valid_w;
  logic [3:0] class_merge_rr_refusal_q;
  logic [3:0] refusal_offer_valid_c;
  wire [1:0] refusal_route_class_c =
      `ZHAO_PACKET_E_REFUSAL_CLASS(cache_checked_token_w);

  assign class_native_valid_w = {
      err_terminal_valid_q, bil_terminal_valid_q,
      near_terminal_valid_q, palette_rsp_valid_w};
  assign class_native_tuple_w[CLS_CLUT] = palette_rsp_tuple_w;
  assign class_native_tuple_w[CLS_NEAR] = near_terminal_tuple_q;
  assign class_native_tuple_w[CLS_BIL] = bil_terminal_tuple_q;
  assign class_native_tuple_w[CLS_ERR] = err_terminal_tuple_q;

  genvar merge_class;
  generate
    for (merge_class = 0; merge_class < 4; merge_class++) begin : g_packet_e_merge
      assign refusal_offer_valid_c[merge_class] =
          `ZHAO_PACKET_E_REFUSAL_MERGE_VALID(refusal_valid_q[merge_class]);
      assign class_merge_select_refusal_w[merge_class] =
          refusal_offer_valid_c[merge_class] &&
          (!class_native_valid_w[merge_class] ||
           class_merge_rr_refusal_q[merge_class]);
      assign class_merge_valid_w[merge_class] =
          class_native_valid_w[merge_class] ||
          refusal_offer_valid_c[merge_class];
      assign class_merge_tuple_w[merge_class] =
          class_merge_select_refusal_w[merge_class]
              ? refusal_tuple_q[merge_class]
              : class_native_tuple_w[merge_class];
      assign class_merge_accept_w[merge_class] =
          class_merge_valid_w[merge_class] &&
          class_dispatch_ready_w[merge_class] &&
          !sim_packet_e_merge_hold_w[merge_class];
      assign class_native_ready_w[merge_class] =
          class_dispatch_ready_w[merge_class] &&
          !sim_packet_e_merge_hold_w[merge_class] &&
          !class_merge_select_refusal_w[merge_class];
      assign refusal_merge_ready_w[merge_class] =
          class_dispatch_ready_w[merge_class] &&
          !sim_packet_e_merge_hold_w[merge_class] &&
          class_merge_select_refusal_w[merge_class];
      assign refusal_room_w[merge_class] =
          !refusal_valid_q[merge_class] ||
          (refusal_valid_q[merge_class] &&
           refusal_merge_ready_w[merge_class]);
    end
  endgenerate

  always_comb begin
    refusal_capture_w = '0;
    if (cache_rsp_valid_w && cache_status_refusal_c && !cache_sidx3_c &&
        refusal_room_w[refusal_route_class_c])
      refusal_capture_w[refusal_route_class_c] = 1'b1;
  end

  assign palette_rsp_ready_w = class_native_ready_w[CLS_CLUT];
  assign near_dispatch_ready_w = class_native_ready_w[CLS_NEAR];
  assign bil_dispatch_ready_w = class_native_ready_w[CLS_BIL];
  assign err_dispatch_ready_w = class_native_ready_w[CLS_ERR];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      refusal_valid_q <= 4'b0000;
      class_merge_rr_refusal_q <= 4'b0000;
      for (int unsigned response_class = 0; response_class < 4;
           response_class++)
        refusal_tuple_q[response_class] <= '0;
    end else begin
      for (int unsigned response_class = 0; response_class < 4;
           response_class++) begin
        if (refusal_room_w[response_class]) begin
          refusal_valid_q[response_class] <= refusal_capture_w[response_class];
          if (refusal_capture_w[response_class])
            refusal_tuple_q[response_class] <= {
                cache_checked_token_w, SOURCE_REFUSED_STATUS,
                8'h00, 8'hff, 24'hff00ff};
        end
        if (class_native_valid_w[response_class] &&
            refusal_offer_valid_c[response_class] &&
            class_merge_accept_w[response_class])
          class_merge_rr_refusal_q[response_class] <=
              !class_merge_select_refusal_w[response_class];
      end
    end
  end

  always_comb begin
    if (cache_sidx3_c) begin
      // Sample index 3 remains higher priority than status: it is outside every
      // legal owner request and can never be converted into a refusal completion.
      cache_rsp_ready_w = 1'b1;
    end else if (cache_status_refusal_c) begin
      // Ready means the routed held slot captures on this exact edge.
      cache_rsp_ready_w = refusal_room_w[refusal_route_class_c];
    end else begin
      unique case (raw_class_c)
        CLS_CLUT: cache_rsp_ready_w = metadata_active_valid_c && palette_req_ready_w;
        CLS_NEAR: cache_rsp_ready_w = metadata_active_valid_c && near_terminal_room_c;
        CLS_BIL:  cache_rsp_ready_w = metadata_active_valid_c && !bil_busy_q;
        default:  cache_rsp_ready_w = metadata_active_valid_c && err_terminal_room_c &&
                                             !binding_refuse_valid_w;
      endcase
    end
  end
  assign cache_raw_accept_w = cache_rsp_valid_w && cache_rsp_ready_w;

  // ---------------------------------------------------------------------------
  // Post-class four-input terminal collector, then direct route/result split.
  logic [TUPLEW-1:0] dispatch_return_tuple_w;
  logic dispatch_return_ready_w;
  logic [31:0] dispatch_emitted_w, dispatch_class_mismatch_w;

  // Structural occupancy remains visible to the unchanged quiet-source alias even
  // when a simulation-only hold gates actual dispatcher acceptance.
  assign class_terminal_offer_valid_w = class_merge_valid_w;
  assign class_dispatch_offer_valid_w = class_merge_valid_w &
      ~sim_packet_e_merge_hold_w;

  zhao_texture_rsp_dispatch_v2 #(.ROUTEW(TOKW)) u_dispatch (
      .clk(clk), .rst_n(rst_n),
      .clut_valid_i(class_dispatch_offer_valid_w[CLS_CLUT]),
      .clut_ready_o(class_dispatch_ready_w[CLS_CLUT]),
      .clut_tuple_i(class_merge_tuple_w[CLS_CLUT]),
      .near_valid_i(class_dispatch_offer_valid_w[CLS_NEAR]),
      .near_ready_o(class_dispatch_ready_w[CLS_NEAR]),
      .near_tuple_i(class_merge_tuple_w[CLS_NEAR]),
      .bil_valid_i(class_dispatch_offer_valid_w[CLS_BIL]),
      .bil_ready_o(class_dispatch_ready_w[CLS_BIL]),
      .bil_tuple_i(class_merge_tuple_w[CLS_BIL]),
      .err_valid_i(class_dispatch_offer_valid_w[CLS_ERR]),
      .err_ready_o(class_dispatch_ready_w[CLS_ERR]),
      .err_tuple_i(class_merge_tuple_w[CLS_ERR]),
      .out_valid_o(dispatch_return_valid_w),
      .out_ready_i(dispatch_return_ready_w),
      .out_tuple_o(dispatch_return_tuple_w),
      .pending_valid_o(dispatch_pending_w), .idle_o(dispatch_idle_w),
      .accepted_o(cnt_dispatch_accepted_o), .emitted_o(dispatch_emitted_w),
      .class_mismatch_o(dispatch_class_mismatch_w));

  // ---------------------------------------------------------------------------
  // Typed AUX: complete Sheet packet and AUX status-only owner plane.
  zhao_aux_surface_ctx_v2_t aux_context_t;
  assign aux_context_t = zhao_aux_surface_ctx_v2_t'(expand_aux_context_w);
  logic aux_issue_valid_w;
  logic [OWNERW-1:0] aux_issue_owner_w;
  logic aux_return_ready_w;
  logic [OWNERW-1:0] aux_return_owner_w;
  logic [TEXTURE_RESULT_W-1:0] aux_return_result_w;
  logic [31:0] aux_sheet_reads_w, aux_local_refused_w, aux_completed_w;
  logic [31:0] aux_degenerate_w, aux_hits_w, aux_misses_w;
  logic [31:0] aux_wrong_op_w, aux_wrong_status_w, aux_wrong_src_w;
  logic [31:0] aux_unsolicited_w, aux_credit_fault_w;
  logic aux_frame_fault_w;
  logic [$clog2(17)-1:0] aux_credit_w;

  assign aux_job_valid_w = expand_aux_valid_w;
  zhao_texture_aux_pipe_v2 #(.OWNERW(OWNERW), .CREDIT(16)) u_aux (
      .clk(clk), .rst_n(rst_n), .frame_fault_clear_i(frame_fault_clear_w),
      .job_valid_i(aux_job_valid_w), .job_ready_o(expand_aux_ready_w),
      .job_wx_i(aux_context_t.wx), .job_wz_i(aux_context_t.wz),
      .job_env_x0_i(aux_context_t.env_x0), .job_env_x1_i(aux_context_t.env_x1),
      .job_env_z0_i(aux_context_t.env_z0), .job_env_z1_i(aux_context_t.env_z1),
      .job_sheet_handle_i(aux_context_t.sheet_handle),
      .job_owner_i(expand_aux_owner_w),
      .job_force_refuse_i(expand_aux_force_refuse_w),
      .issue_valid_o(aux_issue_valid_w), .issue_owner_o(aux_issue_owner_w),
      .req_valid_o(sheet_req_valid_w), .req_ready_i(sheet_req_ready_i),
      .req_op_o(sheet_req_op_o), .req_handle_o(sheet_req_handle_o),
      .req_texel_o(sheet_req_texel_o), .req_src_id_o(sheet_req_src_id_o),
      .pg_valid_i(pg_valid_i), .pg_ready_o(pg_ready_o), .pg_op_i(pg_op_i),
      .pg_status_i(pg_status_i), .pg_tag_i(pg_tag_i),
      .pg_strength_i(pg_strength_i), .pg_src_id_i(pg_src_id_i),
      .out_valid_o(aux_return_valid_w), .out_ready_i(aux_return_ready_w),
      .out_owner_o(aux_return_owner_w), .out_result_o(aux_return_result_w),
      .refuse_valid_o(aux_refuse_valid_w),
      .sheet_rsp_owed_o(aux_sheet_rsp_owed_w), .idle_o(aux_idle_w),
      .accepted_o(cnt_aux_accepted_o), .sheet_reads_o(aux_sheet_reads_w),
      .local_refused_o(aux_local_refused_w), .completed_o(aux_completed_w),
      .degenerate_o(aux_degenerate_w), .sheet_hits_o(aux_hits_w),
      .sheet_misses_o(aux_misses_w), .sheet_rsp_wrong_op_o(aux_wrong_op_w),
      .sheet_rsp_wrong_status_o(aux_wrong_status_w),
      .sheet_rsp_wrong_src_o(aux_wrong_src_w),
      .sheet_rsp_unsolicited_o(aux_unsolicited_w),
      .credit_fault_o(aux_credit_fault_w),
      .frame_fault_o(aux_frame_fault_w), .credit_in_use_o(aux_credit_w));
  assign sheet_req_valid_o = sheet_req_valid_w;

  // ---------------------------------------------------------------------------
  // Sole lifecycle owner and exact 46-bit owner-keyed material row.
  logic [CTXW+RCTXW-1:0] owner_out_context_w;
  logic [OWNERW-1:0] owner_out_owner_w;
  logic [TEXTURE_RESULT_W-1:0] owner_out_result_w;
  logic owner_out_valid_w;
  logic owner_combine_valid_raw_w;
  logic owner_combine_ready_w;
  logic [OWNERW-1:0] owner_combine_owner_w;
  // THE combine-queue depth: `u_own` below is now parameterised from this
  // constant rather than from a literal 4, so the fence logic that indexes
  // `owner_combine_owner_all_w` cannot drift out of step with the queue it
  // reads. One name, one value, no guard needed to keep two copies honest.
  localparam int unsigned OWNER_CMBQD = 4;
  logic [OWNER_CMBQD*OWNERW-1:0]   owner_combine_owner_all_w;
  logic [$clog2(OWNER_CMBQD)-1:0]  owner_combine_rp_w;
  logic [TEXTURE_RESULT_W-1:0] owner_combine_s0_w;
  logic [TEXTURE_RESULT_W-1:0] owner_combine_s1_w;
  logic [TEXTURE_RESULT_W-1:0] owner_combine_s2_w;
  logic [TEXTURE_RESULT_W-1:0] owner_combine_aux_w;
  logic combine_source_read_valid_w;
  logic [5:0] combine_source_read_slot_w;
  logic [TEXTURE_RESULT_W-1:0] owner_source_s0_w;
  logic [TEXTURE_RESULT_W-1:0] owner_source_s1_w;
  logic [TEXTURE_RESULT_W-1:0] owner_source_s2_w;
  logic [TEXTURE_RESULT_W-1:0] owner_source_aux_w;
  logic owner_final_ready_w;
  logic [OWNERW-1:0] owner_final_owner_w;
  logic [TEXTURE_RESULT_W-1:0] owner_final_result_w;
  logic owner_tmu_return_ready_w, owner_aux_return_ready_w;
  logic [31:0] owner_admitted_w, owner_emitted_w, owner_commits_w;
  logic [31:0] owner_tmu_commits_w, owner_aux_commits_w, owner_tickets_w;
  logic [31:0] owner_err_range_w, owner_err_stale_w, owner_err_unsol_w;
  logic [31:0] owner_err_dup_w, owner_err_final_w, owner_err_issue_w;
  logic [31:0] owner_wrap_drains_w, owner_src_unpublished_w;
  logic [6:0] owner_live_w, owner_live_peak_w;

  assign dispatch_return_ready_w = owner_tmu_return_ready_w;
  assign aux_return_ready_w = owner_aux_return_ready_w;
  assign owner_combine_valid_w = owner_combine_valid_raw_w;
  assign owner_final_valid_w = combine_rsp_valid_w;

  zhao_texture_v3own #(
      .OWNERS(OWNERS), .SLOTW(6), .GENW(GENW), .RESW(TEXTURE_RESULT_W),
      .CTXW(RCTXW+CTXW), .OUTQD(4), .CMBQD(OWNER_CMBQD), .READ_LATE(1)
  ) u_own (
      .clk(clk), .rst_n(rst_n), .adm_valid_i(own_adm_valid_w),
      .adm_ready_o(own_adm_ready_w),
      .adm_ctx_i({frag_retire_ctx_i, frag_ctx_i}),
      .adm_req_i(admission_required_mask_w), .adm_owner_o(own_adm_owner_w),
      .adm_accept_o(own_adm_accept_w),
      .iss_tmu_valid_i(binding_issue_tmu_valid_w),
      .iss_tmu_handle_i(binding_issue_tmu_handle_w),
      // Exactly one AUX issue pulse reaches the owner: u_aux's accepted-job pulse.
      .iss_aux_valid_i(aux_issue_valid_w), .iss_aux_owner_i(aux_issue_owner_w),
      .tmu_rvalid_i(dispatch_return_valid_w),
      .tmu_rready_o(owner_tmu_return_ready_w),
      .tmu_rhandle_i(dispatch_return_tuple_w[63:48]),
      .tmu_rresult_i(`ZHAO_PACKET_B_TMU_RESULT(dispatch_return_tuple_w)),
      .aux_rvalid_i(aux_return_valid_w), .aux_rready_o(owner_aux_return_ready_w),
      .aux_rowner_i(aux_return_owner_w), .aux_rresult_i(aux_return_result_w),
      .cmb_valid_o(owner_combine_valid_raw_w),
      .cmb_ready_i(owner_combine_ready_w), .cmb_owner_o(owner_combine_owner_w),
      .cmb_owner_all_o(owner_combine_owner_all_w),
      .cmb_rp_o(owner_combine_rp_w),
      .cmb_s0_o(owner_combine_s0_w), .cmb_s1_o(owner_combine_s1_w),
      .cmb_s2_o(owner_combine_s2_w), .cmb_aux_o(owner_combine_aux_w),
      .src_rd_valid_i(combine_source_read_valid_w),
      .src_rd_slot_i(combine_source_read_slot_w),
      .src_s0_o(owner_source_s0_w), .src_s1_o(owner_source_s1_w),
      .src_s2_o(owner_source_s2_w), .src_aux_o(owner_source_aux_w),
      .fin_valid_i(combine_rsp_valid_w), .fin_ready_o(owner_final_ready_w),
      .fin_owner_i(owner_final_owner_w), .fin_result_i(owner_final_result_w),
      .out_valid_o(owner_out_valid_w), .out_ready_i(out_ready_i),
      .out_owner_o(owner_out_owner_w), .out_result_o(owner_out_result_w),
      .out_ctx_o(owner_out_context_w), .ev_admitted_o(owner_admitted_w),
      .ev_emitted_o(owner_emitted_w), .ev_commits_o(owner_commits_w),
      .ev_tmu_commits_o(owner_tmu_commits_w),
      .ev_texture_samples_o(cnt_texture_samples_o),
      .ev_aux_commits_o(owner_aux_commits_w), .ev_tickets_o(owner_tickets_w),
      .ev_reorder_held_o(cnt_reorder_held_o),
      .ev_err_range_o(owner_err_range_w), .ev_err_stale_o(owner_err_stale_w),
      .ev_err_unsol_o(owner_err_unsol_w), .ev_err_dup_o(owner_err_dup_w),
      .ev_err_final_o(owner_err_final_w), .ev_err_issue_o(owner_err_issue_w),
      .ev_wrap_drains_o(owner_wrap_drains_w),
      .ev_src_unpub_o(owner_src_unpublished_w), .ev_live_o(owner_live_w),
      .ev_live_peak_o(owner_live_peak_w), .ev_quiet_o(own_ev_quiet_w),
      .obs_claim_valid_o(owner_claim_valid_w),
      .obs_ready_valid_o(owner_ready_valid_w));

  // Tagged, credit-reserved synchronous material read.  A ticket is accepted
  // only with a reservation spanning the RAM result and eight-entry FIFO.
  localparam int unsigned MATERIAL_FIFO_DEPTH = 8;
  localparam int unsigned MATERIAL_FIFO_PTRW = 3;
  localparam int unsigned MATERIAL_FIFO_COUNTW = 4;

  logic material_read_valid_q;
  logic [OWNERW-1:0] material_read_owner_q;
  logic [MATW-1:0] material_read_row_q;
  logic material_read_refused_q;
  logic [GENW-1:0] material_read_generation_q;
  logic [3:0] material_read_owner_mask_q;
  logic [GENW-1:0] material_read_owner_mask_generation_q;
  logic material_read_join_refused_q;
  logic [GENW-1:0] material_read_join_generation_q;

  logic [OWNERW-1:0] material_fifo_owner_m [0:MATERIAL_FIFO_DEPTH-1];
  logic [MATW-1:0] material_fifo_row_m [0:MATERIAL_FIFO_DEPTH-1];
  logic material_fifo_refused_m [0:MATERIAL_FIFO_DEPTH-1];
  logic [GENW-1:0] material_fifo_generation_m [0:MATERIAL_FIFO_DEPTH-1];
  logic [MATERIAL_FIFO_PTRW-1:0] material_fifo_write_q;
  logic [MATERIAL_FIFO_PTRW-1:0] material_fifo_read_q;
  logic [MATERIAL_FIFO_COUNTW-1:0] material_fifo_count_q;
  wire [MATERIAL_FIFO_COUNTW-1:0] material_reservation_count_c =
      material_fifo_count_q + MATERIAL_FIFO_COUNTW'(material_read_valid_q);

  wire material_fifo_valid_c = material_fifo_count_q != 4'd0;
  wire [OWNERW-1:0] material_fifo_owner_c =
      material_fifo_owner_m[material_fifo_read_q];
  wire [MATW-1:0] material_fifo_row_c =
      material_fifo_row_m[material_fifo_read_q];
  wire material_fifo_refused_c =
      material_fifo_refused_m[material_fifo_read_q];
  wire [GENW-1:0] material_fifo_generation_c =
      material_fifo_generation_m[material_fifo_read_q];

  wire material_read_owner_mask_valid_c =
      material_read_owner_mask_generation_q == material_read_owner_q[7:0];
  wire material_read_generation_valid_c =
      (material_read_generation_q == material_read_owner_q[7:0]) &&
      !sim_material_fault_mode_w[1];
  wire [3:0] material_read_row_mask_c =
      required_mask_of(material_read_row_q[2:1], material_read_row_q[0]) ^
      (sim_material_fault_mode_w[2] ? 4'b0001 : 4'b0000);
  wire material_read_refused_consistent_c =
      material_read_refused_q ==
      !material_count_legal(material_read_row_q[5:3], material_read_row_q[2:1]);
  wire material_read_join_valid_c =
      (material_read_join_generation_q == material_read_owner_q[7:0]) &&
      !material_read_join_refused_q;
  wire material_read_usable_c = material_read_generation_valid_c &&
      material_read_join_valid_c &&
      (material_read_row_mask_c == material_read_owner_mask_q) &&
      material_read_refused_consistent_c;

  logic combine_ready_w;
  wire material_fifo_pop_c = material_fifo_valid_c &&
      !sim_combine_hold_w && combine_ready_w;
  wire material_read_drop_c = material_read_valid_q &&
      !material_read_owner_mask_valid_c;
  wire material_fifo_push_c = material_read_valid_q &&
      material_read_owner_mask_valid_c;
  wire owner_combine_fire_c = owner_combine_valid_raw_w && owner_combine_ready_w;
  // Admission sets this fence before any owner can become ready; only the
  // generation-sealed joined validation above clears it.  The wide generation
  // table remains a downstream registered validation witness, but is
  // deliberately absent from this COMBINE ready feedback path.
  //
  // EVALUATE THE FENCE FOR EVERY QUEUE ENTRY, THEN SELECT -- 2026-09-18.
  //
  // This read used to be `join_validation_pending_q[owner_combine_owner_w[13:8]]`,
  // a 64:1 mux whose INDEX was itself the output of `u_own`'s CMBQD-deep queue
  // read. `@packet-h-satstage` measured the pair, node by node, as 5.70 ns of a
  // 12.277 ns path -- 1.43 ns for the queue read (`cq_own_q~56`) and 4.27 ns
  // for the fence (`Mux4~1` -> `Mux4~4` -> `Mux4~20`, all at this scope) --
  // and that path gated 110 of the 200 worst paths in the machine through
  // `h_d_q`, `s_d_q`, `lcnt_q` and `rp_q`.
  //
  // `join_validation_pending_q` does not depend on WHICH entry is being read,
  // so the two reads commute. Now the four 64:1 lookups start directly at
  // `u_own`'s queue registers and run beside the read pointer instead of behind
  // it, and what remains in series is a 4:1 select.
  //
  // Identical value, nothing registered, no cycle moved: `owner_combine_rp_w`
  // is the same pointer `cmb_owner_o` is read with, so entry
  // `owner_combine_rp_w` IS `owner_combine_owner_w`.
  logic [OWNER_CMBQD-1:0] owner_combine_fence_ok_c;
  always_comb
    for (int unsigned e = 0; e < OWNER_CMBQD; e++)
      owner_combine_fence_ok_c[e] =
          !join_validation_pending_q[owner_combine_owner_all_w[e*OWNERW + 8 +: 6]];
  wire owner_combine_validation_ready_c =
      owner_combine_fence_ok_c[owner_combine_rp_w];
  assign owner_combine_ready_w = !owner_mask_lifetime_fault_q &&
      owner_combine_validation_ready_c &&
      ((material_reservation_count_c < MATERIAL_FIFO_COUNTW'(MATERIAL_FIFO_DEPTH)) ||
       material_fifo_pop_c);
  assign combine_owner_mask_invalid_event_w = material_read_drop_c;
  logic combine_material_recoverable_fault_w;
  logic [31:0] material_read_mismatch_count_q;
  assign combine_material_recoverable_fault_w = material_fifo_push_c &&
      !material_read_usable_c;

  always_ff @(posedge clk) begin
    if (owner_combine_fire_c) begin
      material_read_row_q <= material_m[owner_combine_owner_w[13:8]];
      material_read_refused_q <= material_refused_m[owner_combine_owner_w[13:8]];
      material_read_generation_q <=
          material_generation_m[owner_combine_owner_w[13:8]];
      material_read_owner_mask_q <=
          owner_required_mask_m[owner_combine_owner_w[13:8]];
      material_read_owner_mask_generation_q <=
          owner_required_mask_generation_m[owner_combine_owner_w[13:8]];
      material_read_join_refused_q <=
          join_force_refuse_m[owner_combine_owner_w[13:8]];
      material_read_join_generation_q <=
          join_validation_generation_m[owner_combine_owner_w[13:8]];
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      material_read_valid_q <= 1'b0;
      material_read_owner_q <= '0;
      material_fifo_write_q <= '0;
      material_fifo_read_q <= '0;
      material_fifo_count_q <= '0;
      material_read_mismatch_count_q <= 32'd0;
    end else begin
      material_read_valid_q <= owner_combine_fire_c;
      if (owner_combine_fire_c)
        material_read_owner_q <= owner_combine_owner_w;

      if (combine_material_recoverable_fault_w)
        material_read_mismatch_count_q <= material_read_mismatch_count_q + 32'd1;

      if (material_fifo_push_c) begin
        material_fifo_owner_m[material_fifo_write_q] <= material_read_owner_q;
        material_fifo_row_m[material_fifo_write_q] <= material_read_usable_c
            ? material_read_row_q
            : canonical_refusal_material(material_read_owner_mask_q);
        material_fifo_refused_m[material_fifo_write_q] <= material_read_usable_c
            ? material_read_refused_q : 1'b1;
        material_fifo_generation_m[material_fifo_write_q] <=
            material_read_usable_c ? material_read_generation_q
                                   : material_read_owner_q[7:0];
        material_fifo_write_q <= material_fifo_write_q + 3'd1;
      end
      if (material_fifo_pop_c)
        material_fifo_read_q <= material_fifo_read_q + 3'd1;
      material_fifo_count_q <= material_fifo_count_q +
          MATERIAL_FIFO_COUNTW'(material_fifo_push_c) -
          MATERIAL_FIFO_COUNTW'(material_fifo_pop_c);
    end
  end

  wire material_aux_c = material_fifo_row_c[0];
  wire [1:0] material_count_c = material_fifo_row_c[2:1];
  wire [2:0] material_recipe_c = material_fifo_row_c[5:3];
  wire [7:0] material_weight_c = material_fifo_row_c[13:6];
  wire [7:0] material_base_a_c = material_fifo_row_c[21:14];
  wire [23:0] material_base_rgb_c = material_fifo_row_c[45:22];

  logic material_read_idle_w;
  logic combine_leaf_idle_w;
  assign material_read_idle_w = (material_reservation_count_c == 4'd0);

  logic [TEXTURE_RESULT_W-1:0] combine_result_w;
  logic [23:0] combine_rgb_w;
  logic [7:0] combine_alpha_w, combine_index_w, combine_status_w;
  logic [OWNERW-1:0] combine_owner_w;
  logic combine_refused_w;
  logic [31:0] combine_refused_material_w, combine_saturated_add_w;
  logic [31:0] combine_saturated_mul2x_w;
  logic [31:0] combine_jobs_accepted_w, combine_jobs_completed_w;
  logic [31:0] combine_phases_completed_w;
  logic [31:0] combine_jobs_w [8];
  logic [31:0] combine_recipe0_w, combine_recipe1_w;
  logic [31:0] combine_recipe2_w, combine_recipe3_w;
  logic [31:0] combine_recipe4_w, combine_recipe5_w;
  logic [31:0] combine_recipe6_w, combine_recipe7_w;

  assign combine_req_valid_w = material_fifo_valid_c && !sim_combine_hold_w;
  assign cnt_combine_jobs_o[0] = combine_recipe0_w;
  assign cnt_combine_jobs_o[1] = combine_recipe1_w;
  assign cnt_combine_jobs_o[2] = combine_recipe2_w;
  assign cnt_combine_jobs_o[3] = combine_recipe3_w;
  assign cnt_combine_jobs_o[4] = combine_recipe4_w;
  assign cnt_combine_jobs_o[5] = combine_recipe5_w;
  assign cnt_combine_jobs_o[6] = combine_recipe6_w;
  assign cnt_combine_jobs_o[7] = combine_recipe7_w;

  zhao_texture_material_combine_v3 #(
      .NCTX(8), .TAGW(OWNERW), .READ_LATE(1), .SLOTW(6)
  ) u_combine (
      .clk(clk), .rst_n(rst_n), .f_valid_i(combine_req_valid_w),
      .f_ready_o(combine_ready_w), .f_sample_count_i(material_count_c),
      .f_recipe_i(material_recipe_c), .f_weight_i(material_weight_c),
      .f_aux_required_i(material_aux_c), .f_base_rgb_i(material_base_rgb_c),
      .f_base_a_i(material_base_a_c), .f_tag_i(material_fifo_owner_c),
      .f_s0_i('0), .f_s1_i('0), .f_s2_i('0), .f_aux_i('0),
      .f_slot_i(material_fifo_owner_c[13:8]),
      .src_s0_i(owner_source_s0_w), .src_s1_i(owner_source_s1_w),
      .src_s2_i(`ZHAO_PACKET_B_COMBINE_S2(owner_source_s2_w, owner_source_aux_w)),
      .src_aux_i(owner_source_aux_w),
      .src_rd_valid_o(combine_source_read_valid_w),
      .src_rd_slot_o(combine_source_read_slot_w),
      .o_valid_o(combine_rsp_valid_w), .o_ready_i(owner_final_ready_w),
      .o_result_o(combine_result_w), .o_rgb_o(combine_rgb_w),
      .o_a_o(combine_alpha_w), .o_raw_index_o(combine_index_w),
      .o_status_o(combine_status_w), .o_tag_o(combine_owner_w),
      .o_refused_o(combine_refused_w), .idle_o(combine_leaf_idle_w),
      .refused_material_o(combine_refused_material_w),
      .saturated_add_o(combine_saturated_add_w),
      .saturated_mul2x_o(combine_saturated_mul2x_w),
      .jobs_by_recipe_o(combine_jobs_w),
      .jobs_recipe_0_o(combine_recipe0_w),
      .jobs_recipe_1_o(combine_recipe1_w),
      .jobs_recipe_2_o(combine_recipe2_w),
      .jobs_recipe_3_o(combine_recipe3_w),
      .jobs_recipe_4_o(combine_recipe4_w),
      .jobs_recipe_5_o(combine_recipe5_w),
      .jobs_recipe_6_o(combine_recipe6_w),
      .jobs_recipe_7_o(combine_recipe7_w),
      .jobs_accepted_o(combine_jobs_accepted_w),
      .jobs_completed_o(combine_jobs_completed_w),
      .phases_issued_o(cnt_combine_phases_o),
      .phases_completed_o(combine_phases_completed_w));

  // ===========================================================================
  // THE SURFACE SHEET'S VISIBLE EFFECT (TERRAINAUX, 2026-09-25)
  // ===========================================================================
  // `design/contracts/TEXTURE.AUX.V2.md` and `design/contracts/TEXTURE.COMBINE.md`
  // both end their AUX section with the same sentence: *"tag and strength are
  // reserved for A LATER VISIBLE TERRAIN-EFFECT COMPOSITION and Packet B does
  // not claim that effect is connected."* This is that composition, and the
  // decision record -- what each contract amendment supersedes and why -- is in
  // `design/contracts/TEXTURE.SHEETMOD.md`.
  //
  // WHAT IS NOT WEAKENED. AUX still never occupies, aliases or substitutes for a
  // TMU sample, and NO RECIPE consumes tag or strength as an operand: the
  // combiner below is untouched, runs exactly as it did, and its 13 committed
  // mutants still describe it. The tint is applied to the colour the recipe
  // PRODUCED. That is the ORACLE'S OWN SHAPE -- `span.mod_r/g/b` multiplies the
  // texel the recipe made; it is not one of the texels.
  //
  // WHY THE STRENGTH IS TAKEN FROM A PER-SLOT RECORD AND NOT FROM A WIRE.
  // `u_aux` returns its result some clocks before `u_combine` produces the
  // colour for the same fragment, and the combiner interleaves NCTX contexts --
  // so a wire read at the combiner's output would apply fragment A's scar to
  // fragment B's colour, with every counter in this file balancing. That is
  // CLAUDE.md's lockstep fault exactly. The record is therefore keyed by the
  // OWNER SLOT and read back with the OWNER THE COMBINER RETURNS, which is the
  // record's own key -- the same discipline `material_m` above already uses and
  // the same one `zhao_terrain_lightlane` uses for its world vertices.
  //
  // AND NO GENERATION COUNTER IS ADDED, DELIBERATELY (a counter declined at
  // design time beats a counter explained at review time). `zhao_texture_v3own`
  // is the sole lifecycle owner and does not free a slot until its OUTPUT
  // handshake, which is downstream of `fin_result_i` -- so the slot cannot be
  // re-admitted while its own result is in the combiner. The generation is
  // stored and CHECKED anyway, because the check is free: a mismatch suppresses
  // the tint (fail-safe: an untinted colour, never a wrong one) and trips a
  // simulation assertion -- and `synthesis translate_off` does NOT hide such an
  // assertion from the simulator (CLAUDE.md, proven by planting a syntax error
  // inside one).
  logic [GENW+8:0] sheet_m [0:OWNERS-1];   // {generation, aux_required, strength}
  always_ff @(posedge clk) begin
    // ADMISSION writes the DECLARATION and a zero strength. Writing it here and
    // not only on the AUX return is what makes a fragment that requires no AUX
    // read its OWN record instead of the previous tenant's.
    if (own_adm_accept_w)
      sheet_m[own_adm_owner_w[13:8]] <= {own_adm_owner_w[7:0], frag_aux_i, 8'd0};
    // THE AUX RETURN overwrites the strength, keeping the declaration and the
    // generation the admission wrote. The two writers cannot collide on one
    // slot: a slot's AUX return is inside its own lifetime, and admission is
    // the start of a lifetime.
    else if (aux_return_valid_w && aux_return_ready_w)
      sheet_m[aux_return_owner_w[13:8]] <= {
          sheet_m[aux_return_owner_w[13:8]][GENW+8 -: GENW],
          sheet_m[aux_return_owner_w[13:8]][8],
          aux_return_result_w[TEXTURE_RESULT_ALPHA_HI:TEXTURE_RESULT_ALPHA_LO]};
  end

  wire [GENW+8:0] sheet_row_c = sheet_m[combine_owner_w[13:8]];
  wire sheet_gen_ok_c = (sheet_row_c[GENW+8 -: GENW] == combine_owner_w[7:0]);
  // THE THREE GUARDS, each for a different reason:
  //   `sheet_row_c[8]`  the MATERIAL's own `aux_required` declaration, not a
  //                     non-zero strength. Owner directive 2026-09-23 section 3:
  //                     "Profile selection is explicit, not inferred from
  //                     whether a port happens to be zero."
  //   `sheet_gen_ok_c`  see above; fail-safe, not fail-quiet.
  //   status == 0       TEXTURE.COMBINE.md fixes the terminal value of a faulted
  //                     result at EXACTLY 24'hFF00FF. A tinted magenta is a
  //                     different and quieter colour, and a fault must stay loud.
  wire sheet_apply_c =
      sheet_row_c[8] && sheet_gen_ok_c &&
      (combine_result_w[TEXTURE_RESULT_STATUS_HI:TEXTURE_RESULT_STATUS_LO] == 8'd0);

  logic [23:0] sheet_rgb_w;
  logic sheet_applied_w;
  zhao_texture_sheetmod u_sheetmod (
      .en_i      (sheet_apply_c),
      .strength_i(sheet_row_c[7:0]),
      .rgb_i     (combine_result_w[TEXTURE_RESULT_RGB_HI:TEXTURE_RESULT_RGB_LO]),
      .rgb_o     (sheet_rgb_w),
      .applied_o (sheet_applied_w)
  );
  // `applied_o` is `en_i` by construction and is therefore not a second opinion
  // about anything; it is consumed so the port is not dangling, and the leaf's
  // own directed test owns that equality.
  /* verilator lint_off UNUSEDSIGNAL */
  wire unused_sheet_applied_w = sheet_applied_w;
  /* verilator lint_on UNUSEDSIGNAL */

  // synthesis translate_off
  always_ff @(posedge clk) begin
    if (rst_n && combine_rsp_valid_w && sheet_row_c[8] && !sheet_gen_ok_c)
      $fatal(1, "zhao_texture_island_v3_top: the sheet record's generation %0d does not match the combiner's returned owner generation %0d -- a slot was reused while its result was in flight",
             sheet_row_c[GENW+8 -: GENW], combine_owner_w[7:0]);
  end
  // synthesis translate_on

  assign owner_final_owner_w = combine_owner_w;
  assign owner_final_result_w = {
      combine_result_w[TEXTURE_RESULT_W-1:TEXTURE_RESULT_RGB_HI+1], sheet_rgb_w};

  // ===========================================================================
  // THE DELTA LEAVES WITH ITS OWNER (NORMALMAP, 2026-09-26)
  // ===========================================================================
  // Read at the OUTPUT beat rather than at the combine beat, and that is not
  // arbitrary: the delta does NOT modulate this island's texel. Its consumer is
  // the LIT COLOUR LANE -- `zref::terrain::normalmap_apply(uint8_t v, delta)`,
  // whose oracle comment reads "the delta lands on the flat lit colour lanes,
  // saturating unsigned 8-bit" -- and the lit colour lane is
  // `zhao_raster_texture_stage_v3`'s `frag_vert_rgb_o`. So the delta is carried
  // OUT beside the result on the same beat and applied there.
  //
  // WHY NOT `u_sheetmod`, WHICH IS THE BLOCK THE COMMISSIONING BRIEF NAMED. Two
  // independent measurements say it is the wrong consumer for THIS value and
  // both are structural rather than arguable:
  //   * a terrain fragment's colour is NOT this island's texel.
  //     `zhao_raster_fragment.sv:717-721` selects `s1_src_rgb_r` as
  //     `unit_mul(texel, vertex)` only when the state word's SHADE_MOD bit is
  //     set, and `TERR_FRAG_STATE` is the opaque profile with SHADE_MOD = 0 --
  //     so for terrain the texel is computed and discarded, and a delta applied
  //     to it would change no pixel at all;
  //   * and the sheet arm is itself shut in the composed console, because
  //     `frag_aux_i` traces to `mat_flat_request_c[268]`, which
  //     `zhao_console_core.sv` drives with a literal `1'b0`. `sheet_apply_c`
  //     therefore cannot be true there. That is a fact about the composer, not
  //     a defect in this island or in `zhao_texture_sheetmod`, and it is
  //     recorded here because a brief cited that block as a live consumer.
  //
  // The arithmetic is also different in kind: the sheet is a MULTIPLICATIVE
  // unit8 tint on a texel; this is an ADDITIVE s9 term on a light. Folding one
  // into the other would be a second home for neither law.
  wire [GENW+8:0] dtl_delta_row_c = dtl_delta_m[owner_out_owner_w[13:8]];
  wire dtl_delta_gen_ok_c =
      (dtl_delta_row_c[GENW+8 -: GENW] == owner_out_owner_w[7:0]);
  assign out_detail_delta_o =
      dtl_delta_gen_ok_c ? $signed(dtl_delta_row_c[8:0]) : 9'sd0;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) cnt_detail_published_o <= 32'd0;
    else if (owner_out_valid_w && out_ready_i && dtl_delta_gen_ok_c)
      cnt_detail_published_o <= cnt_detail_published_o + 32'd1;
  end

  assign out_valid_o = owner_out_valid_w;
  assign out_status_o = owner_out_result_w[47:40];
  assign out_texel_idx_o = owner_out_result_w[39:32];
  assign out_a_o = owner_out_result_w[31:24];
  assign out_rgb_o = owner_out_result_w[23:0];
  assign out_refused_o = owner_out_result_w[40];
  assign out_retire_ctx_o = `ZHAO_PACKET_B_RETIRE_CONTEXT(owner_out_context_w);
  assign out_tag_o = owner_out_context_w[15:0];

  // ---------------------------------------------------------------------------
  // Recoverable frame state, independent owner-counter baselines, and lifetime
  // structural faults.  Same-edge live-counter increments beat baseline capture.
  logic [31:0] owner_err_range_base_q, owner_err_stale_base_q;
  logic [31:0] owner_err_unsol_base_q, owner_err_dup_base_q;
  logic [31:0] owner_err_final_base_q, owner_err_issue_base_q;
  logic [31:0] owner_src_unpublished_base_q;
  logic [31:0] dispatch_mismatch_base_q, metadata_genmis_base_q;
  logic [31:0] aux_credit_fault_base_q;
  logic recoverable_frame_fault_q;
  logic local_fault_event_w;
  logic child_fault_level_w;
  logic owner_counter_delta_w;
  logic protocol_counter_delta_w;

  assign owner_counter_delta_w =
      (owner_err_range_w != owner_err_range_base_q) ||
      (owner_err_stale_w != owner_err_stale_base_q) ||
      (owner_err_unsol_w != owner_err_unsol_base_q) ||
      (owner_err_dup_w != owner_err_dup_base_q) ||
      (owner_err_final_w != owner_err_final_base_q) ||
      (owner_err_issue_w != owner_err_issue_base_q) ||
      (owner_src_unpublished_w != owner_src_unpublished_base_q);
  assign protocol_counter_delta_w =
      (dispatch_class_mismatch_w != dispatch_mismatch_base_q) ||
      (metadata_generation_mismatch_w != metadata_genmis_base_q) ||
      (aux_credit_fault_w != aux_credit_fault_base_q);
  assign child_fault_level_w = desc_frame_fault_w || expand_frame_fault_w ||
      binding_frame_fault_w || aux_frame_fault_w || cache_fill_protocol_fault_w;

  assign local_fault_event_w =
      sim_frame_fault_inject_w ||
      (own_adm_accept_w &&
       !material_count_legal(frag_recipe_i, frag_sample_count_i)) ||
      (expand_frag_valid_w && expand_frag_ready_w && joined_force_refuse_c) ||
      (plan_cache_valid_w && plan_cache_ready_w && plan_cache_error_w) ||
      (cfg_rsp_valid_o && (cfg_rsp_status_o != 4'd0)) ||
      (dispatch_return_valid_w && dispatch_return_ready_w &&
       (dispatch_return_tuple_w[47:40] != 8'd0)) ||
      (aux_return_valid_w && owner_aux_return_ready_w &&
       (aux_return_result_w[47:40] != 8'd0)) ||
      (combine_rsp_valid_w && owner_final_ready_w &&
       (combine_status_w != 8'd0)) ||
      combine_material_recoverable_fault_w ||
      bil_identity_mismatch_event_w ||
      ((!frame_fault_clear_w) && child_fault_level_w);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      owner_err_range_base_q <= 32'd0;
      owner_err_stale_base_q <= 32'd0;
      owner_err_unsol_base_q <= 32'd0;
      owner_err_dup_base_q <= 32'd0;
      owner_err_final_base_q <= 32'd0;
      owner_err_issue_base_q <= 32'd0;
      owner_src_unpublished_base_q <= 32'd0;
      dispatch_mismatch_base_q <= 32'd0;
      metadata_genmis_base_q <= 32'd0;
      aux_credit_fault_base_q <= 32'd0;
      recoverable_frame_fault_q <= 1'b0;
    end else begin
      if (frame_fault_clear_w) begin
        owner_err_range_base_q <= owner_err_range_w;
        owner_err_stale_base_q <= owner_err_stale_w;
        owner_err_unsol_base_q <= owner_err_unsol_w;
        owner_err_dup_base_q <= owner_err_dup_w;
        owner_err_final_base_q <= owner_err_final_w;
        owner_err_issue_base_q <= owner_err_issue_w;
        owner_src_unpublished_base_q <= owner_src_unpublished_w;
        dispatch_mismatch_base_q <= dispatch_class_mismatch_w;
        metadata_genmis_base_q <= metadata_generation_mismatch_w;
        aux_credit_fault_base_q <= aux_credit_fault_w;
      end
      recoverable_frame_fault_q <= `ZHAO_PACKET_B_RECOVERABLE_NEXT(
          local_fault_event_w, frame_fault_clear_w, recoverable_frame_fault_q);
    end
  end

  assign frame_fault_o = recoverable_frame_fault_q || owner_counter_delta_w ||
      protocol_counter_delta_w || child_fault_level_w ||
      (err_rcp_q_o || sim_rcp_qerr_overlay_w) ||
      (uvjoin_lifetime_fault_w || sim_uv_mismatch_overlay_w) ||
      ((expand_overflow_w != 32'd0) || sim_expand_overflow_overlay_w) ||
      (owner_mask_lifetime_fault_q || sim_owner_mask_overlay_w) ||
      (cache_sidx3_lifetime_fault_q || sim_cache_sidx3_overlay_w) ||
      err_rsp_dropped_o ||
      ((metadata_illegal_w != 32'd0) || sim_metajoin_illegal_overlay_w)
`ifdef ZHAO_PACKET_E_MUTANT_PRE_E_FILL_LIFETIME
      || sim_pre_e_fill_refusal_lifetime_q
`endif
      ;

  // Compatibility/evidence aliases.
  assign cnt_fragments_o = expand_fragments_w;
  assign cnt_live_peak_o = {25'd0, owner_live_peak_w};
  assign cnt_combine_refused_o = combine_refused_material_w;
  assign cnt_fragrob_id_errors_o = owner_err_range_w + owner_err_stale_w +
      owner_err_unsol_w + owner_err_dup_w + owner_err_final_w +
      owner_err_issue_w + owner_src_unpublished_w;
  assign err_fragrob_wq_overflow_o = expand_overflow_w != 32'd0;
  assign err_fragrob_id_error_o = cnt_fragrob_id_errors_o != 32'd0;
  assign err_aux_degenerate_o = aux_degenerate_w != 32'd0;
  assign err_palette_unusable_o = cnt_palette_stale_o + cnt_palette_cold_o;
  assign err_class_mismatch_o = binding_witness_mismatch_w;
  assign meta_genmis_o = metadata_generation_mismatch_w;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      err_class_invalid_o <= 32'd0;
      err_plan_mode_o <= 1'b0;
    end else begin
      if (own_adm_accept_w && (frag_class_i == CLS_ERR))
        err_class_invalid_o <= err_class_invalid_o + 32'd1;
      if (plan_cache_valid_w && plan_cache_ready_w && plan_cache_error_w)
        err_plan_mode_o <= 1'b1;
    end
  end

`ifndef SYNTHESIS
  logic assert_armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) assert_armed_q <= 1'b0;
    else        assert_armed_q <= 1'b1;
  end

  // TIMING4 R1T shadow. This latches the owner the UVW row was ACTUALLY
  // ADDRESSED BY on the launching edge, so it can be compared against the
  // identity the record went on to carry.
  //
  // Its first version latched rcp_head_owner_q instead, under a comment saying
  // it was "deliberately NOT written by the same enable expression as the
  // payload it checks". It was written by exactly that enable, from exactly
  // that source, so persp_prep_shadow_owner_q and persp_prep_owner_q were
  // provably equal at all times and a_persp_prep_owner_is_head was a
  // tautology -- a detector wired to two operands that move together, with a
  // comment asserting the opposite. It could not fire on correct hardware and
  // it could not fire under the R1T mutant either, because the mutant changes
  // only the read ADDRESS and never touches persp_prep_owner_q.
  //
  // Shadowing uvw_read_owner_c fixes that. In production the macro makes it
  // rcp_head_owner_q and the comparison holds; under the mutant it is the live
  // rcp_owner_w, which differs from the head exactly while a second result is
  // in flight, and the assertion has something real to catch.
  logic persp_prep_shadow_v_q;
  logic [OWNERW-1:0] persp_prep_shadow_owner_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      persp_prep_shadow_v_q <= 1'b0;
      persp_prep_shadow_owner_q <= '0;
    end else if (persp_prep_room_c) begin
      persp_prep_shadow_v_q <= rcp_head_valid_q;
      if (rcp_head_valid_q)
        persp_prep_shadow_owner_q <= uvw_read_owner_c;
    end
  end

  // The head holds while it is stalled -- but "stalled" must be sampled on the
  // edge where a write COULD have happened, not on the edge where the change is
  // observed. The first version guarded $stable with THIS cycle's
  // persp_prep_room_c while $stable reports a change made on the PREVIOUS edge,
  // so an ordinary accept-then-stall sequence fired it on correct hardware.
  // geom_bin_pipe_v2_omit_v3_quiet_mutant caught that at cycle 4500.
  logic rcp_head_write_allowed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) rcp_head_write_allowed_q <= 1'b1;
    else        rcp_head_write_allowed_q <= rcp_head_room_c;
  end
  always_ff @(posedge clk) begin
    if (assert_armed_q) begin
      a_atomic_admission: assert (own_adm_accept_w ==
          (rcp_req_valid_w && rcp_req_ready_w));
      // TIMING4 R1T. The UVW row must be addressed by the identity of the
      // record that is actually launching, never by whatever the reciprocal
      // pipeline happens to be presenting. These two differ only while a
      // second result is in flight behind a held head -- which is precisely
      // when a live address pairs one owner's row with another's reciprocal.
      if (persp_prep_room_c && rcp_head_valid_q) begin
        a_uvw_read_uses_head_identity: assert (
            uvw_read_owner_c == rcp_head_owner_q);
      end
      // The record handed to perspective is entirely the head's. The expected
      // owner is latched independently on the launching edge below, so this
      // compares two separately clocked quantities rather than one signal
      // against itself.
      if (persp_prep_valid_q && persp_prep_shadow_v_q) begin
        a_persp_prep_owner_is_head: assert (
            persp_prep_owner_q == persp_prep_shadow_owner_q);
      end
      // TIMING4 E1 cross-check. The leaf's public masked output and the join's
      // raw-plus-verdict route are computed from the same row by two different
      // paths; on the offered edge they must agree exactly. If they ever do
      // not, the descriptor the join is about to publish is not the descriptor
      // the rest of the island believes it validated.
      if (desc_rsp_valid_w) begin
        a_desc_raw_and_masked_agree: assert (
            desc_logical_w ==
            (desc_usable_w ? desc_logical_raw_w : 287'd0));
      end
      // A held head must not change while it waits for perspective prep.
      // Guarded by the REGISTERED write permission, so the condition describes
      // the edge on which a change could have been made rather than the edge on
      // which it is observed.
      if (!rcp_head_write_allowed_q) begin
        a_rcp_head_holds: assert ($stable(rcp_head_owner_q) &&
                                  $stable(rcp_head_recip_q) &&
                                  $stable(rcp_head_shift_q) &&
                                  $stable(rcp_head_zero_q));
      end
      a_aux_issue_pulses_equal: assert (expand_issue_aux_valid_w == aux_issue_valid_w);
      a_aux_issue_owner_equal: if (aux_issue_valid_w)
          assert (expand_issue_aux_owner_w == aux_issue_owner_w);
      a_cache_token_holds_for_metadata: if (metadata_read_pending_q || metadata_hold_valid_q)
          assert (cache_rsp_valid_w &&
                  (cache_checked_token_w == metadata_expected_token_q));
      a_cache_one_class: assert ($onehot0({
          palette_req_valid_w, near_capture_c, bil_capture_c, raw_err_capture_c}));
      a_no_metadata_for_sample3: assert (!(cache_sidx3_c && metadata_read_launch_w));
      a_no_metadata_for_cache_refusal: assert (
          !(cache_rsp_valid_w && cache_status_refusal_c &&
            metadata_read_launch_w));
      a_refusal_capture_one_class: assert ($onehot0(refusal_capture_w));
      a_dispatch_quiet_alias_is_structural: assert (
          q_dispatch_req_valid == (|class_merge_valid_w));
      if (q_dispatch_req_valid)
        a_dispatch_occupancy_blocks_data_quiet: assert (!data_quiet);
      if (cache_rsp_valid_w && cache_rsp_ready_w &&
          cache_status_refusal_c && !cache_sidx3_c) begin
        a_refusal_accept_is_held_capture: assert (
            refusal_capture_w[refusal_route_class_c]);
      end
      a_metadata_fallthrough_exclusive: assert (
          !(metadata_hold_valid_q && metadata_rd_result_valid_w));
      a_response_drop_enters_barrier: assert (
          !err_rsp_dropped_o || lifetime_admission_block_w);
      if (owner_combine_fire_c) begin
        a_combine_after_join_validation: assert (
            !join_validation_pending_q[owner_combine_owner_w[13:8]] &&
            (join_validation_generation_m[owner_combine_owner_w[13:8]] ==
             owner_combine_owner_w[7:0]));
      end
      a_validation_not_reused_on_admission: assert (
          !(own_adm_accept_w && join_validation_accept_c &&
            (own_adm_owner_w[13:8] == joined_owner_c[13:8])));
      a_material_reservation_bound: assert (
          material_reservation_count_c <= MATERIAL_FIFO_COUNTW'(MATERIAL_FIFO_DEPTH));
      a_material_fifo_bound: assert (
          material_fifo_count_q <= MATERIAL_FIFO_COUNTW'(MATERIAL_FIFO_DEPTH));
      if (material_fifo_valid_c) begin
        a_material_fifo_generation: assert (
            material_fifo_generation_c == material_fifo_owner_c[7:0]);
        a_material_fifo_refusal_consistency: assert (
            material_fifo_refused_c ==
            !material_count_legal(material_fifo_row_c[5:3],
                                  material_fifo_row_c[2:1]));
      end
    end
  end
`endif

endmodule : zhao_texture_island_v3_top

`undef ZHAO_PACKET_B_TMU_RESULT
`undef ZHAO_ISLAND_T4_UVW_READ_OWNER
`undef ZHAO_PACKET_B_COMBINE_S2
`undef ZHAO_PACKET_B_RETIRE_CONTEXT
`undef ZHAO_PACKET_B_RECOVERABLE_NEXT
`undef ZHAO_PACKET_B_OWNER_MASK_GENERATION
`undef ZHAO_PACKET_B_CACHE_TOKEN
`undef ZHAO_PACKET_B_BILERP_OBS_TOKEN
`undef ZHAO_PACKET_B_SHADOW_WRITE
`undef ZHAO_PACKET_B_RSP_OBS_DATA
`undef ZHAO_PACKET_E_CACHE_STATUS_IS_REFUSAL
`undef ZHAO_PACKET_E_REFUSAL_CLASS
`undef ZHAO_PACKET_E_REFUSAL_MERGE_VALID
`ifdef ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION
  `undef ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION
`endif

`default_nettype wire
