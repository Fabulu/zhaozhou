// zhao_texture_binding_resolver_v2.sv -- Packet-B sealed binding pages.
//
// Two 256-row physical banks separate active traffic from staging writes/CRC.
// Sample jobs carry the descriptor-captured page generation; it is compared with
// the independently activated page register before the active row is read.
// Every accepted logical job pulses issue exactly once and owns reserved space
// through either a planner handshake or a next-or-later local refusal.
`default_nettype none

module zhao_texture_binding_resolver_v2 #(
    parameter int unsigned MAXLOG2 = 11,
    parameter int unsigned SLOTW   = 6,
    parameter int unsigned GENW    = 8
) (
    input  logic clk,
    input  logic rst_n,
    input  logic frame_fault_clear_i,

    // ---------------- binding-page configuration -----------------------------
    input  logic        cfg_valid_i,
    output logic        cfg_ready_o,
    input  logic [1:0]  cfg_op_i, // 0 BEGIN, 1 WRITE, 2 END, 3 ABORT
    input  logic [7:0]  cfg_page_generation_i,
    input  logic [7:0]  cfg_selector_i,
    input  logic [74:0] cfg_row_i,
    input  logic [31:0] cfg_crc32_i,

    output logic        cfg_rsp_valid_o,
    input  logic        cfg_rsp_ready_i,
    output logic [1:0]  cfg_rsp_op_o,
    output logic [3:0]  cfg_rsp_status_o,
    output logic [7:0]  cfg_rsp_page_generation_o,

    // The activator uses structural data quiet, never owner quiet or public
    // quiet.  Existing accepted work continues to use the old page during CRC.
    input  logic        data_quiet_i,
    output logic        admission_enable_o,
    output logic [7:0]  active_page_generation_o,
    output logic        cfg_loader_idle_o,
    output logic        binding_crc_busy_o,
    output logic        binding_seal_pending_o,

    // ---------------- one typed logical sample job ---------------------------
    input  logic        req_valid_i,
    output logic        req_ready_o,
    input  logic [SLOTW+2+GENW-1:0] req_sample_handle_i,
    input  logic [7:0]  req_page_generation_i,
    input  logic        req_selector_overflow_i,
    input  logic        req_force_refuse_i,
    input  logic [7:0]  req_binding_selector_i,
    input  logic signed [31:0] req_u_i,
    input  logic signed [31:0] req_v_i,
    input  logic [7:0]  req_lod_q4_4_i,
    input  logic [1:0]  req_sample0_class_witness_i,
    input  logic [1:0]  req_sample0_palette_slot_witness_i,
    input  logic [7:0]  req_sample0_palette_generation_witness_i,

    // Atomic notification to zhao_texture_v3own on logical job acceptance.
    output logic        iss_tmu_valid_o,
    output logic [SLOTW+2+GENW-1:0] iss_tmu_handle_o,

    // ---------------- accepted planner record -------------------------------
    output logic        plan_valid_o,
    input  logic        plan_ready_i,
    output logic [2+SLOTW+2+GENW-1:0] plan_route_token_o,
    output logic [31:0] plan_base_o,
    output logic [31:0] plan_mode_o,
    output logic [1:0]  plan_palette_slot_o,
    output logic [7:0]  plan_palette_generation_o,
    output logic signed [31:0] plan_u_o,
    output logic signed [31:0] plan_v_o,
    output logic [7:0]  plan_lod_q4_4_o,

    // ---------------- reserved local terminal refusal -----------------------
    output logic        refuse_valid_o,
    input  logic        refuse_ready_i,
    output logic [SLOTW+2+GENW-1:0] refuse_sample_handle_o,
    output logic [zhao_render_texture_pkg::TEXTURE_RESULT_W-1:0]
                                      refuse_result_o,

    // ---------------- evidence and structural observation -------------------
    output logic [31:0] sample_jobs_accepted_o,
    output logic [31:0] planner_jobs_accepted_o,
    output logic [31:0] local_refused_o,
    output logic [31:0] selector_overflow_count_o,
    output logic [31:0] page_generation_mismatch_o,
    output logic [31:0] invalid_row_o,
    output logic [31:0] witness_mismatch_o,
    output logic [31:0] forced_refused_o,
    output logic [31:0] cfg_errors_o,
    output logic        binding_fault_o,
    output logic        data_idle_o
);
  import zhao_render_texture_pkg::*;

  localparam int unsigned OWNERW = SLOTW + GENW;
  localparam int unsigned SMPW   = SLOTW + 2 + GENW;
  localparam int unsigned ROUTEW = 2 + SMPW;

  localparam logic [1:0] CFG_BEGIN = 2'd0;
  localparam logic [1:0] CFG_WRITE = 2'd1;
  localparam logic [1:0] CFG_END   = 2'd2;
  localparam logic [1:0] CFG_ABORT = 2'd3;

  localparam logic [3:0] CFG_OK                = 4'd0;
  localparam logic [3:0] CFG_BAD_STATE         = 4'd1;
  localparam logic [3:0] CFG_BAD_GENERATION    = 4'd2;
  localparam logic [3:0] CFG_BAD_ROW           = 4'd3;
  localparam logic [3:0] CFG_DUP_SELECTOR      = 4'd4;
  localparam logic [3:0] CFG_BAD_CRC           = 4'd5;
  localparam logic [3:0] CFG_INTERNAL_PROTOCOL = 4'd6;

  localparam logic [1:0] CLS_CLUT = 2'd0;
  localparam logic [1:0] CLS_NEAR = 2'd1;
  localparam logic [1:0] CLS_BIL  = 2'd2;

  localparam logic [2:0] FMT_CLUT8    = 3'd0;
  localparam logic [2:0] FMT_RGB565   = 3'd1;
  localparam logic [2:0] FMT_CLUT4    = 3'd2;
  localparam logic [2:0] FMT_ARGB1555 = 3'd3;
  localparam logic [2:0] FMT_ARGB4444 = 3'd4;

  typedef struct packed {
    logic        valid;               // [74]
    logic [7:0]  palette_generation;  // [73:66]
    logic [1:0]  palette_slot;        // [65:64]
    logic [31:0] mode;                // [63:32]
    logic [31:0] base;                // [31:0]
  } binding_row_t;

  typedef struct packed {
    logic [SMPW-1:0] handle;
    logic [7:0]      page_generation;
    logic            selector_overflow;
    logic            force_refuse;
    logic [7:0]      binding_selector;
    logic signed [31:0] u;
    logic signed [31:0] v;
    logic [7:0]      lod_q4_4;
    logic [1:0]      witness_class;
    logic [1:0]      witness_palette_slot;
    logic [7:0]      witness_palette_generation;
  } sample_job_t;

  typedef struct packed {
    logic [ROUTEW-1:0] route_token;
    logic [31:0]       base;
    logic [31:0]       mode;
    logic [1:0]        palette_slot;
    logic [7:0]        palette_generation;
    logic signed [31:0] u;
    logic signed [31:0] v;
    logic [7:0]        lod_q4_4;
  } planner_job_t;

  function automatic logic [31:0] rep4(input logic [3:0] level);
    unique case (level)
      4'd0:  rep4 = 32'd0;
      4'd1:  rep4 = 32'd1;
      4'd2:  rep4 = 32'd5;
      4'd3:  rep4 = 32'd21;
      4'd4:  rep4 = 32'd85;
      4'd5:  rep4 = 32'd341;
      4'd6:  rep4 = 32'd1365;
      4'd7:  rep4 = 32'd5461;
      4'd8:  rep4 = 32'd21845;
      4'd9:  rep4 = 32'd87381;
      4'd10: rep4 = 32'd349525;
      default: rep4 = 32'd1398101; // level 11, MAXLOG2 default
    endcase
  endfunction

  // Canonical row law, including the planner's full packed-chain address bound.
  function automatic logic binding_row_legal(input logic [74:0] row_bits);
    binding_row_t row;
    logic [2:0] fmt;
    logic filter, mip_enable, clut, direct;
    logic [1:0] wrap_u, wrap_v;
    logic [3:0] log2w, log2h, max_level, min_dimension, final_level;
    logic [4:0] area_exp;
    logic [63:0] level_offset_texel, level_texels, max_total_texel;
    logic [63:0] max_byte_offset, max_line_end;
    integer unsigned offset_shift;
    begin
      row = binding_row_t'(row_bits);
      fmt = row.mode[2:0];
      filter = row.mode[3];
      wrap_u = row.mode[5:4];
      wrap_v = row.mode[7:6];
      log2w = row.mode[11:8];
      log2h = row.mode[15:12];
      max_level = row.mode[19:16];
      mip_enable = row.mode[20];
      clut = (fmt == FMT_CLUT8) || (fmt == FMT_CLUT4);
      direct = (fmt == FMT_RGB565) || (fmt == FMT_ARGB1555) ||
               (fmt == FMT_ARGB4444);
      min_dimension = (log2w < log2h) ? log2w : log2h;
      final_level = mip_enable ? max_level : 4'd0;
      area_exp = {1'b0, log2w} + {1'b0, log2h};

      level_offset_texel = 64'd0;
      if (final_level != 4'd0) begin
        offset_shift = 32'(area_exp) -
                       ((32'(final_level) - 32'd1) << 1);
        level_offset_texel = {32'd0, rep4(final_level)} << offset_shift;
      end
      level_texels = 64'd1 <<
                      (32'(area_exp) - (32'(final_level) << 1));
      max_total_texel = level_offset_texel + level_texels - 64'd1;
      if (direct)
        max_byte_offset = (max_total_texel << 1) + 64'd1;
      else if (fmt == FMT_CLUT4)
        max_byte_offset = max_total_texel >> 1;
      else
        max_byte_offset = max_total_texel;
      max_line_end = ({32'd0, row.base} + max_byte_offset) | 64'd15;

      binding_row_legal = row.valid &&
          (row.base[3:0] == 4'd0) &&
          (fmt <= FMT_ARGB4444) &&
          (wrap_u <= 2'd2) && (wrap_v <= 2'd2) &&
          (row.mode[31:21] == 11'd0) &&
          (log2w <= 4'(MAXLOG2)) && (log2h <= 4'(MAXLOG2)) &&
          !(clut && filter) &&
          (max_level <= min_dimension) &&
          (mip_enable || (max_level == 4'd0)) &&
          (!direct || ({row.palette_generation, row.palette_slot} == 10'd0)) &&
          (max_line_end[63:32] == 32'd0);
    end
  endfunction

  function automatic logic [1:0] binding_class(input logic [31:0] mode);
    unique case (mode[2:0])
      FMT_CLUT8, FMT_CLUT4: binding_class = CLS_CLUT;
      FMT_RGB565, FMT_ARGB1555, FMT_ARGB4444:
        binding_class = mode[3] ? CLS_BIL : CLS_NEAR;
      default: binding_class = 2'd3;
    endcase
  endfunction

  function automatic logic [31:0] crc32_byte(
      input logic [31:0] crc_in, input logic [7:0] data);
    logic [31:0] crc;
    integer bit_index;
    begin
      crc = crc_in;
      for (bit_index = 0; bit_index < 8; bit_index = bit_index + 1) begin
        if (crc[0] ^ data[bit_index])
          crc = (crc >> 1) ^ 32'hEDB88320;
        else
          crc = crc >> 1;
      end
      crc32_byte = crc;
    end
  endfunction

  function automatic logic [31:0] crc32_row10(
      input logic [31:0] crc_in,
      input logic [74:0] row,
      input logic row_present);
    logic [31:0] crc;
    logic [79:0] canonical;
    integer byte_index;
    begin
      crc = crc_in;
      canonical = row_present ? {5'b0, row} : 80'd0;
      for (byte_index = 0; byte_index < 10; byte_index = byte_index + 1)
        crc = crc32_byte(crc, canonical[byte_index*8 +: 8]);
      crc32_row10 = crc;
    end
  endfunction

  // --------------------------------------------------------------------------
  // Two physical page banks.  Payload is not reset; validity masks are.
  binding_row_t page0_m [0:255];
  binding_row_t page1_m [0:255];
  logic [255:0] page0_valid_q, page1_valid_q;
  logic active_bank_q, staging_bank_q;
  logic [7:0] active_generation_q, staging_generation_q;

  typedef enum logic [2:0] {
    LOAD_IDLE, LOAD_LOADING, LOAD_CRC_SCAN, LOAD_SEAL_PENDING
  } loader_state_t;
  loader_state_t loader_state_q;

  assign active_page_generation_o = active_generation_q;
  assign cfg_loader_idle_o = loader_state_q == LOAD_IDLE;
  assign binding_crc_busy_o = loader_state_q == LOAD_CRC_SCAN;
  assign binding_seal_pending_o = loader_state_q == LOAD_SEAL_PENDING;
  assign admission_enable_o = loader_state_q != LOAD_SEAL_PENDING;

  logic cfg_rsp_v_q;
  logic [1:0] cfg_rsp_op_q;
  logic [3:0] cfg_rsp_status_q;
  logic [7:0] cfg_rsp_generation_q;
  assign cfg_rsp_valid_o = cfg_rsp_v_q;
  assign cfg_rsp_op_o = cfg_rsp_op_q;
  assign cfg_rsp_status_o = cfg_rsp_status_q;
  assign cfg_rsp_page_generation_o = cfg_rsp_generation_q;

  wire loader_accepts_commands_c =
      (loader_state_q == LOAD_IDLE) || (loader_state_q == LOAD_LOADING);
  assign cfg_ready_o = !cfg_rsp_v_q && loader_accepts_commands_c;
  wire cfg_accept_c = cfg_valid_i && cfg_ready_o;
  wire staging_selector_present_c = staging_bank_q
      ? page1_valid_q[cfg_selector_i] : page0_valid_q[cfg_selector_i];

  logic [3:0] cfg_status_c;
  logic cfg_begin_c, cfg_write_c, cfg_end_c, cfg_abort_c;
  always_comb begin
    cfg_status_c = CFG_BAD_STATE;
    cfg_begin_c = 1'b0;
    cfg_write_c = 1'b0;
    cfg_end_c = 1'b0;
    cfg_abort_c = 1'b0;

    if (loader_state_q == LOAD_IDLE) begin
      if (cfg_op_i != CFG_BEGIN) begin
        cfg_status_c = CFG_BAD_STATE;
      end else if ((cfg_selector_i != 8'd0) || (cfg_row_i != 75'd0) ||
                   (cfg_crc32_i != 32'd0)) begin
        cfg_status_c = CFG_BAD_ROW;
      end else if ((cfg_page_generation_i == 8'd0) ||
                   (cfg_page_generation_i == active_generation_q)) begin
        cfg_status_c = CFG_BAD_GENERATION;
      end else begin
        cfg_status_c = CFG_OK;
        cfg_begin_c = 1'b1;
      end
    end else if (loader_state_q == LOAD_LOADING) begin
      unique case (cfg_op_i)
        CFG_WRITE: begin
          if (cfg_crc32_i != 32'd0)
            cfg_status_c = CFG_BAD_ROW;
          else if (cfg_page_generation_i != staging_generation_q)
            cfg_status_c = CFG_BAD_GENERATION;
          else if (!cfg_row_i[74] || !binding_row_legal(cfg_row_i))
            cfg_status_c = CFG_BAD_ROW;
          else if (staging_selector_present_c)
            cfg_status_c = CFG_DUP_SELECTOR;
          else begin
            cfg_status_c = CFG_OK;
            cfg_write_c = 1'b1;
          end
        end
        CFG_END: begin
          if ((cfg_selector_i != 8'd0) || (cfg_row_i != 75'd0))
            cfg_status_c = CFG_BAD_ROW;
          else if (cfg_page_generation_i != staging_generation_q)
            cfg_status_c = CFG_BAD_GENERATION;
          else begin
            cfg_status_c = CFG_OK;
            cfg_end_c = 1'b1;
          end
        end
        CFG_ABORT: begin
          if ((cfg_selector_i != 8'd0) || (cfg_row_i != 75'd0) ||
              (cfg_crc32_i != 32'd0))
            cfg_status_c = CFG_BAD_ROW;
          else if (cfg_page_generation_i != staging_generation_q)
            cfg_status_c = CFG_BAD_GENERATION;
          else begin
            cfg_status_c = CFG_OK;
            cfg_abort_c = 1'b1;
          end
        end
        default: cfg_status_c = CFG_BAD_STATE;
      endcase
    end
  end

  logic [7:0] crc_selector_q;
  logic [31:0] crc_q, crc_expected_q;
  logic [74:0] crc_row_q;
  logic crc_row_present_q, crc_have_row_q;
  wire [31:0] crc_row_next_c =
      crc32_row10(crc_q, crc_row_q, crc_row_present_q);
  wire [31:0] crc_final_c = crc_row_next_c ^ 32'hFFFF_FFFF;
  wire [7:0] crc_next_selector_c = crc_selector_q + 8'd1;

  // --------------------------------------------------------------------------
  // Data plane: a synchronous read hold followed by one reserved disposition.
  sample_job_t read_job_q;
  binding_row_t read_row_q;
  logic read_v_q, read_row_present_q;

  planner_job_t disposition_plan_q;
  logic [SMPW-1:0] disposition_handle_q;
  logic disposition_v_q, disposition_refuse_q;

  wire plan_accept_c = plan_valid_o && plan_ready_i;
  wire refuse_accept_c = refuse_valid_o && refuse_ready_i;
  wire disposition_pop_c = plan_accept_c || refuse_accept_c;
  wire disposition_ready_c = !disposition_v_q || disposition_pop_c;
  wire read_ready_c = !read_v_q || disposition_ready_c;
  assign req_ready_o = read_ready_c;
  wire req_accept_c = req_valid_i && req_ready_o;

  assign iss_tmu_valid_o = req_accept_c;
  assign iss_tmu_handle_o = req_sample_handle_i;

  logic [1:0] read_class_c;
  logic read_row_bad_c, read_witness_bad_c;
  logic read_generation_bad_c, read_refuse_c;
  always_comb begin
    read_class_c = binding_class(read_row_q.mode);
    read_generation_bad_c =
        (read_job_q.page_generation == 8'd0) ||
        (read_job_q.page_generation != active_generation_q);
    read_row_bad_c = !read_row_present_q ||
                     !binding_row_legal(read_row_q);
    read_witness_bad_c =
        (read_job_q.handle[GENW +: 2] == 2'd0) &&
        ({read_job_q.witness_class,
          read_job_q.witness_palette_slot,
          read_job_q.witness_palette_generation} !=
         {read_class_c, read_row_q.palette_slot,
          read_row_q.palette_generation});
    read_refuse_c = read_job_q.force_refuse ||
                    read_job_q.selector_overflow ||
                    read_generation_bad_c || read_row_bad_c ||
                    read_witness_bad_c;
  end

  assign plan_valid_o = disposition_v_q && !disposition_refuse_q;
  assign plan_route_token_o = disposition_plan_q.route_token;
  assign plan_base_o = disposition_plan_q.base;
  assign plan_mode_o = disposition_plan_q.mode;
  assign plan_palette_slot_o = disposition_plan_q.palette_slot;
  assign plan_palette_generation_o = disposition_plan_q.palette_generation;
  assign plan_u_o = disposition_plan_q.u;
  assign plan_v_o = disposition_plan_q.v;
  assign plan_lod_q4_4_o = disposition_plan_q.lod_q4_4;

  zhao_texture_result_v2_t refusal_result_c;
  always_comb begin
    refusal_result_c.status = 8'b0000_0001;
    refusal_result_c.sample0_raw_index = 8'd0;
    refusal_result_c.alpha = 8'hFF;
    refusal_result_c.rgb = 24'hFF00FF;
  end
  assign refuse_valid_o = disposition_v_q && disposition_refuse_q;
  assign refuse_sample_handle_o = disposition_handle_q;
  assign refuse_result_o = refusal_result_c;

  assign data_idle_o = !read_v_q && !disposition_v_q;

  // --------------------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      page0_valid_q <= '0;
      page1_valid_q <= '0;
      active_bank_q <= 1'b0;
      staging_bank_q <= 1'b1;
      active_generation_q <= 8'd0;
      staging_generation_q <= 8'd0;
      loader_state_q <= LOAD_IDLE;
      cfg_rsp_v_q <= 1'b0;
      cfg_rsp_op_q <= CFG_BEGIN;
      cfg_rsp_status_q <= CFG_OK;
      cfg_rsp_generation_q <= 8'd0;
      crc_selector_q <= 8'd0;
      crc_q <= 32'd0;
      crc_expected_q <= 32'd0;
      crc_row_q <= '0;
      crc_row_present_q <= 1'b0;
      crc_have_row_q <= 1'b0;

      read_v_q <= 1'b0;
      disposition_v_q <= 1'b0;
      disposition_refuse_q <= 1'b0;
      sample_jobs_accepted_o <= 32'd0;
      planner_jobs_accepted_o <= 32'd0;
      local_refused_o <= 32'd0;
      selector_overflow_count_o <= 32'd0;
      page_generation_mismatch_o <= 32'd0;
      invalid_row_o <= 32'd0;
      witness_mismatch_o <= 32'd0;
      forced_refused_o <= 32'd0;
      cfg_errors_o <= 32'd0;
      binding_fault_o <= 1'b0;
    end else begin
      if (frame_fault_clear_i)
        binding_fault_o <= 1'b0;

      if (cfg_rsp_v_q && cfg_rsp_ready_i)
        cfg_rsp_v_q <= 1'b0;

      // Every non-END command and every rejected END gets one immediate held
      // response.  A legal END owns the loader until CRC and activation finish.
      if (cfg_accept_c) begin
        if (!cfg_end_c) begin
          cfg_rsp_v_q <= 1'b1;
          cfg_rsp_op_q <= cfg_op_i;
          cfg_rsp_status_q <= cfg_status_c;
          cfg_rsp_generation_q <= cfg_page_generation_i;
        end
        if (cfg_status_c != CFG_OK) begin
          cfg_errors_o <= cfg_errors_o + 32'd1;
          // Configuration fault creation follows the held response event and
          // has priority over a same-edge frame clear.
          binding_fault_o <= 1'b1;
        end

        if (cfg_begin_c) begin
          staging_bank_q <= ~active_bank_q;
          staging_generation_q <= cfg_page_generation_i;
          if (~active_bank_q) page1_valid_q <= '0;
          else                page0_valid_q <= '0;
          loader_state_q <= LOAD_LOADING;
        end

        if (cfg_write_c) begin
          if (staging_bank_q) begin
            page1_m[cfg_selector_i] <= binding_row_t'(cfg_row_i);
            page1_valid_q[cfg_selector_i] <= 1'b1;
          end else begin
            page0_m[cfg_selector_i] <= binding_row_t'(cfg_row_i);
            page0_valid_q[cfg_selector_i] <= 1'b1;
          end
        end

        if (cfg_abort_c) begin
          if (staging_bank_q) page1_valid_q <= '0;
          else                page0_valid_q <= '0;
          loader_state_q <= LOAD_IDLE;
        end

        if (cfg_end_c) begin
          crc_q <= crc32_byte(32'hFFFF_FFFF, staging_generation_q);
          crc_expected_q <= cfg_crc32_i;
          crc_selector_q <= 8'd0;
          crc_have_row_q <= 1'b0;
          loader_state_q <= LOAD_CRC_SCAN;
        end
      end

      // Synchronous staging-bank scan. Invalid selectors contribute ten zero
      // bytes regardless of stale payload.
      if (loader_state_q == LOAD_CRC_SCAN) begin
        if (!crc_have_row_q) begin
          if (staging_bank_q) begin
            crc_row_q <= page1_m[crc_selector_q];
            crc_row_present_q <= page1_valid_q[crc_selector_q];
          end else begin
            crc_row_q <= page0_m[crc_selector_q];
            crc_row_present_q <= page0_valid_q[crc_selector_q];
          end
          crc_have_row_q <= 1'b1;
        end else if (crc_selector_q == 8'hFF) begin
          crc_have_row_q <= 1'b0;
          if (crc_final_c == crc_expected_q) begin
            loader_state_q <= LOAD_SEAL_PENDING;
          end else begin
            if (staging_bank_q) page1_valid_q <= '0;
            else                page0_valid_q <= '0;
            loader_state_q <= LOAD_IDLE;
            cfg_rsp_v_q <= 1'b1;
            cfg_rsp_op_q <= CFG_END;
            cfg_rsp_status_q <= CFG_BAD_CRC;
            cfg_rsp_generation_q <= staging_generation_q;
            cfg_errors_o <= cfg_errors_o + 32'd1;
            binding_fault_o <= 1'b1;
          end
        end else begin
          crc_q <= crc_row_next_c;
          crc_selector_q <= crc_next_selector_c;
          if (staging_bank_q) begin
            crc_row_q <= page1_m[crc_next_selector_c];
            crc_row_present_q <= page1_valid_q[crc_next_selector_c];
          end else begin
            crc_row_q <= page0_m[crc_next_selector_c];
            crc_row_present_q <= page0_valid_q[crc_next_selector_c];
          end
        end
      end

      // The single atomic activation edge.  No active bank/generation register
      // changes at END acceptance, CRC success, or owner-only quiet.
      if ((loader_state_q == LOAD_SEAL_PENDING) && data_quiet_i) begin
        active_bank_q <= staging_bank_q;
        active_generation_q <= staging_generation_q;
        loader_state_q <= LOAD_IDLE;
        cfg_rsp_v_q <= 1'b1;
        cfg_rsp_op_q <= CFG_END;
        cfg_rsp_status_q <= CFG_OK;
        cfg_rsp_generation_q <= staging_generation_q;
      end

      // ---------------- data read stage --------------------------------------
      if (read_ready_c) begin
        read_v_q <= req_valid_i;
        if (req_valid_i) begin
          read_job_q <= '{
              handle: req_sample_handle_i,
              page_generation: req_page_generation_i,
              selector_overflow: req_selector_overflow_i,
              force_refuse: req_force_refuse_i,
              binding_selector: req_binding_selector_i,
              u: req_u_i,
              v: req_v_i,
              lod_q4_4: req_lod_q4_4_i,
              witness_class: req_sample0_class_witness_i,
              witness_palette_slot: req_sample0_palette_slot_witness_i,
              witness_palette_generation:
                  req_sample0_palette_generation_witness_i};

          // Suppress even the table read when an earlier refusal condition is
          // already known. Selector low bits are ignored on overflow/force.
          if (!req_force_refuse_i && !req_selector_overflow_i &&
              (req_page_generation_i != 8'd0) &&
              (req_page_generation_i == active_generation_q)) begin
            if (active_bank_q) begin
              read_row_q <= page1_m[req_binding_selector_i];
              read_row_present_q <= page1_valid_q[req_binding_selector_i];
            end else begin
              read_row_q <= page0_m[req_binding_selector_i];
              read_row_present_q <= page0_valid_q[req_binding_selector_i];
            end
          end else begin
            read_row_q <= '0;
            read_row_present_q <= 1'b0;
          end
          sample_jobs_accepted_o <= sample_jobs_accepted_o + 32'd1;
        end
      end

      // ---------------- reserved disposition stage ---------------------------
      if (disposition_ready_c) begin
        disposition_v_q <= read_v_q;
        if (read_v_q) begin
          disposition_refuse_q <= read_refuse_c;
          disposition_handle_q <= read_job_q.handle;
          disposition_plan_q.route_token <=
              {read_class_c, read_job_q.handle};
          disposition_plan_q.base <= read_row_q.base;
          disposition_plan_q.mode <= read_row_q.mode;
          disposition_plan_q.palette_slot <= read_row_q.palette_slot;
          disposition_plan_q.palette_generation <=
              read_row_q.palette_generation;
          disposition_plan_q.u <= read_job_q.u;
          disposition_plan_q.v <= read_job_q.v;
          disposition_plan_q.lod_q4_4 <= read_job_q.lod_q4_4;

          if (read_refuse_c)
            binding_fault_o <= 1'b1;
          // The page-generation detector is independent of the functional
          // refusal priority: it compares the admission-carried byte with the
          // live activation register exactly once at terminal disposition.
          if (read_generation_bad_c)
            page_generation_mismatch_o <=
                page_generation_mismatch_o + 32'd1;
          if (read_job_q.force_refuse)
            forced_refused_o <= forced_refused_o + 32'd1;
          else if (read_job_q.selector_overflow)
            selector_overflow_count_o <= selector_overflow_count_o + 32'd1;
          else if (!read_generation_bad_c && read_row_bad_c)
            invalid_row_o <= invalid_row_o + 32'd1;
          else if (!read_generation_bad_c && read_witness_bad_c)
            witness_mismatch_o <= witness_mismatch_o + 32'd1;
        end
      end

      if (plan_accept_c)
        planner_jobs_accepted_o <= planner_jobs_accepted_o + 32'd1;
      if (refuse_accept_c)
        local_refused_o <= local_refused_o + 32'd1;
    end
  end

  initial begin : p_layout_contract
    if ((SLOTW != 6) || (GENW != 8) || (MAXLOG2 != 11))
      $fatal(1, "binding_resolver_v2 requires Packet-B 6/8/11 profile");
    if (($bits(binding_row_t) != 75) || ($bits(sample_job_t) != 118) ||
        ($bits(planner_job_t) != 164) || (ROUTEW != 18))
      $fatal(1, "binding_resolver_v2 typed record width changed");
  end

`ifndef SYNTHESIS
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q) begin
      a_one_disposition: assert (!(plan_valid_o && refuse_valid_o));
      a_issue_is_accept: assert (iss_tmu_valid_o ==
                                (req_valid_i && req_ready_o));
      a_no_active_write: if (cfg_accept_c && cfg_write_c)
        assert (staging_bank_q != active_bank_q);
    end
  end
`endif

endmodule : zhao_texture_binding_resolver_v2

`default_nettype wire
