// zhao_texture_binding_resolver_v2.sv -- Packet-B sealed binding pages.
//
// Two 256-row physical banks separate active traffic from staging writes/CRC.
// Sample jobs carry the descriptor-captured page generation; it is compared with
// the independently activated page register before the active row is read.
// Every accepted logical job pulses issue exactly once and owns reserved space
// through either a planner handshake or a next-or-later local refusal.
//
// ---------------------------------------------------------------------------
// THE TILESET ROW -- THE READER FOR TEXTURE.MOSAIC'S PICK (2026-09-26, TERRAINTEX)
// ---------------------------------------------------------------------------
// `zhao_texture_mosaic_v2` has computed a per-texel tile pick for every
// fragment since the V3 island was written, and until today NOTHING READ IT:
// `mosaic_tile_w`, `mosaic_tx_w` and `mosaic_ty_w` occurred exactly twice each
// in `zhao_texture_island_v3_top.sv` -- a declaration and a port connection --
// so the frozen `terrain_rules` 6.2 pick was computed and discarded. This block
// is that answer's reader, and the reason it lands HERE rather than on the
// selector is the oracle's own shape.
//
// `zref::Tileset` (`zref_render.hpp:166`) is `uint8_t tiles[256][64*64]` -- ONE
// memory object, 256 CLUT8 tiles of 4,096 bytes -- and `rast.cpp:370` samples it
// as `ts->tiles[tile][(ty << 6) + tx]`. So a tile index is a BYTE OFFSET inside
// one bound texture, `tile * 4096`, and NOT a different binding row. Adding it
// to the row's base is therefore the oracle's arithmetic, not an invented
// indirection; the alternative (256 binding rows per tileset, the selector
// carrying the pick) would have spent the whole 8-bit selector space on one
// material and is what the direct-colour path's `tile_base[]` array does for a
// DIFFERENT object.
//
// THE DECLARATION IS THE ROW'S, AND THAT IS THE WHOLE REASON THIS IS AFFORDABLE.
// A per-FRAGMENT "this material is a mosaic" bit has NO CARRIAGE: the 362-bit
// flat request (`zhao_render_texture_pkg::zhao_texture_v3_request_v2_t`) is
// packed solid with no reserved field, and so is the island's 287-bit logical
// descriptor -- measured, not assumed. The binding row's `mode` word, by
// contrast, has ELEVEN bits the legality law has always forced to zero
// (`row.mode[31:21] == 11'd0`), so `mode[21]` is a mandatory-zero bit whose
// zero already means "not a tileset" in every row this console has ever
// accepted. That is the same zero-keeps-its-meaning allocation the ABI uses for
// `MaterialRecord.fragment_state`, and it costs no field anywhere upstream.
//
// A TILESET ROW IS CONSTRAINED, NOT MERELY FLAGGED. `binding_row_legal()`
// refuses a tileset row that is not exactly the object 6.2 describes: CLUT8,
// 64x64 (`log2w == log2h == 6`, which is what makes the TMU's mirrored wrap
// equal to `zref::terrain::mirror_texel`), mirror on both axes, no mip chain,
// and a base whose whole 1 MiB extent fits in 32 bits. A row that declares the
// bit and is not that object is CFG_BAD_ROW -- refused at write, never stored.
//
// WHAT THIS DOES NOT DO. It does not give terrain a sampling material and it
// does not give terrain a binding key; both remain absent and both are named in
// `zhao_console_core.sv` entry I13. A terrain triangle still declares
// `MATMODE_NONE`, publishes `sample_count = 0` and asks this block for nothing.
// The pick now HAS a reader; it does not yet have a terrain fragment to read for.
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
    // TEXTURE.MOSAIC's per-texel pick for the fragment this sample belongs to
    // (`zhao_texture_mosaic_v2.pick_tile_o`). It is READ ONLY when the resolved
    // row declares `TILESET`; every other row ignores it entirely, so a client
    // that does not run a mosaic leaves it at zero and nothing changes. The
    // island holds it per owner and gates this request on the matching pick
    // having landed, so the value here is always the pick for THIS fragment.
    input  logic [7:0]  req_mosaic_tile_i,
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
    // CENSUS, NOT A FAULT: planner records whose base was displaced by a
    // TEXTURE.MOSAIC pick, i.e. samples that actually read a tileset row. It is
    // the counter that separates "the reader is composed" from "the pick
    // reached an address", and it is fired by stimulus in
    // `texture_binding_resolver_v2_directed` rather than quoted at zero.
    output logic [31:0] tileset_samples_o,
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

  // ---- the tileset declaration (terrain_rules 6.2, zref::Tileset) ----------
  // `mode[21]`, taken from the eleven bits `binding_row_legal` has always
  // forced to zero. The stride and the tile geometry are NAMED rather than
  // spelled as literals in the arithmetic below, because they are the oracle's
  // numbers and a reader must be able to check them against it: `zref::Tileset`
  // is `uint8_t tiles[256][64*64]`.
  localparam int unsigned MODE_TILESET_B      = 21;
  localparam int unsigned TILESET_TILE_LOG2W  = 6;      // 64 texels
  localparam int unsigned TILESET_TILE_LOG2H  = 6;      // 64 texels
  localparam int unsigned TILESET_TILE_LOG2B  = 12;     // 64*64 CLUT8 bytes
  localparam int unsigned TILESET_TILES       = 256;
  localparam logic [1:0]  WRAP_MIRROR         = 2'd2;

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
    logic [7:0]      mosaic_tile;
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
    logic tileset, tileset_shape_ok;
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
      tileset = row.mode[MODE_TILESET_B];
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
      // A TILESET ROW IS 256 TILES, so its live extent is the whole object and
      // not one tile's: the pick can name any of them and the row's bound has
      // to cover the furthest. `TILESET_TILES - 1` tiles of displacement sit on
      // top of the single-tile offset the dimensions already gave.
      if (tileset)
        max_byte_offset = max_byte_offset +
            (64'(TILESET_TILES - 1) << TILESET_TILE_LOG2B);
      max_line_end = ({32'd0, row.base} + max_byte_offset) | 64'd15;

      // The declared tileset must BE the object terrain_rules 6.2 and
      // `zref::Tileset` describe, or it is refused at CFG_WRITE and never
      // stored. Checking the flag alone would let a row declare a mosaic and
      // then be sampled with the wrong fold, the wrong format or a mip chain
      // the oracle has no equivalent for -- a wrong pixel past a gate.
      tileset_shape_ok = (fmt == FMT_CLUT8) && !filter && !mip_enable &&
          (max_level == 4'd0) &&
          (wrap_u == WRAP_MIRROR) && (wrap_v == WRAP_MIRROR) &&
          (log2w == 4'(TILESET_TILE_LOG2W)) &&
          (log2h == 4'(TILESET_TILE_LOG2H));

      binding_row_legal = row.valid &&
          (row.base[3:0] == 4'd0) &&
          (fmt <= FMT_ARGB4444) &&
          (wrap_u <= 2'd2) && (wrap_v <= 2'd2) &&
          (!tileset || tileset_shape_ok) &&
          (row.mode[31:22] == 10'd0) &&
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

  // --------------------------------------------------------------------------
  // THE STORED WORD CARRIES ITS OWN LEGALITY.
  //
  // `binding_row_legal()` above is a pure function of the 75-bit row: a 64-bit
  // variable shift, a 64-bit add, a second variable shift, another add, an OR
  // and a compare. It used to be evaluated combinationally on the READ side,
  // on the row that had just come out of the memory, and the 2026-09-18
  // composed-shell fit measured what that costs:
  //
  //   515 of the 2,000 worst paths launched inside altsyncram:page*_m_rtl_0,
  //   and on the worst of them the MEMORY contributed 0.192 ns while the
  //   arithmetic hanging off it contributed 16.0 -- read_row_c.mode, Add8,
  //   Add10, three ShiftLeft2 stages, a ~30-cell Add13 carry chain,
  //   max_byte_offset, Add14, into binding_fault_o.
  //
  // It is also computed a second time, at CFG_WRITE (see `cfg_status_c`), and
  // that is the copy that decides whether a row is stored at all. So the read
  // side was re-deriving a property the write side had already established.
  //
  // The word is therefore 76 bits: the row, plus the law's verdict on it,
  // written once when the row is written. The read path tests a stored bit.
  //
  // WHAT THE BIT IS WORTH TODAY, stated plainly rather than flatteringly:
  // lines below are the ONLY writers of these two arrays, and both sit past
  // the CFG_WRITE legality test, so `legal` is 1'b1 for every row that exists
  // and synthesis is free to fold it. It is not, today, a live detector, and
  // quoting it as one would be the "detector that cannot fire" mistake this
  // repository has a chapter about.
  //
  // It is kept as a stored field rather than deleted because it puts the
  // obligation in the TYPE: a future writer of `page*_m` has to say what the
  // law makes of its row, and cannot quietly install an unvalidated one the
  // way it could if the read side simply trusted `_present`. The expression is
  // shared with the CFG_WRITE test by common-subexpression elimination, so the
  // write path pays nothing extra for it.
  // A PACKED VECTOR RATHER THAN A STRUCT, deliberately.
  //
  // The obvious spelling is `struct packed { logic legal; binding_row_t row; }`
  // and it was written that way first. A packed struct emits one MEMBERDTYPE
  // node per member into Verilator's tree, and both `legal` and `row` are names
  // that already occur elsewhere in the island's closure -- so the struct moved
  // the island's duplicate-name fingerprint from 105 markers to 107 and would
  // have forced a re-derivation of the independent oracle in
  // tests/tools/test_texture_v3_interface_manifest.py, whose own comment warns
  // that fitting its remap to a target digest destroys its independence.
  //
  // That fingerprint exists to show the ISLAND's schema did not move. Spending
  // it on two leaf-internal member names makes it noisier at no benefit, and
  // this file already speaks in packed vectors with a cast for interpretation
  // (`binding_row_t'(cfg_row_i)` throughout), so a vector is the idiom here
  // rather than a concession.
  localparam int unsigned PAGE_ROW_W   = 75;             // binding_row_t
  localparam int unsigned PAGE_LEGAL_B = PAGE_ROW_W;     // [75]
  localparam int unsigned PAGE_WORD_W  = PAGE_ROW_W + 1; // 76

  // Two physical page banks.  Payload is not reset; validity masks are.
  logic [PAGE_WORD_W-1:0] page0_m [0:255];
  logic [PAGE_WORD_W-1:0] page1_m [0:255];

  // ---- ONE READ PORT PER BANK, so each infers as M10K ----------------------
  //
  // These arrays used to be read directly by both consumers, giving each array
  // two registered reads at two addresses. Quartus cannot map that onto one
  // M10K read port, so it kept the whole table in fabric:
  //
  //     Info (276007): RAM logic "...page0_m" is uninferred due to
  //     asynchronous read logic
  //
  // 2 x 256 x 75 is 38,400 flip-flops, and the first composed measurement of
  // the sibling shell put this block at 28,957 ALUT / 39,449 registers -- 44%
  // of the whole composition's logic and 49% of its registers, on a design
  // that needed 62,534 ALMs against a 41,910-ALM device.
  //
  // THE TWO READERS NEVER COLLIDE. The data plane reads the ACTIVE bank, the
  // CRC walk reads the STAGING bank, and `staging_bank_q == ~active_bank_q` is
  // maintained at every assignment to either. So each array needs ONE address
  // and ONE enable, chosen by which role that bank currently holds.
  //
  // THE ENABLE IS NOT OPTIONAL. Registering the read unconditionally is how
  // this repository's most-cited defect was built: a metadata bank whose
  // output "tracked whatever address was being OFFERED while the stage
  // downstream held the previous response", yielding one response's data with
  // another's metadata while every counter balanced. The enables below
  // reproduce the conditions the old conditional loads used, so the held-row
  // behaviour is unchanged.
  logic         crc_read_first_c, crc_read_next_c, crc_re_c, data_re_c;
  logic [7:0]   crc_ra_c;
  logic         page0_re_c, page1_re_c;
  logic [7:0]   page0_ra_c, page1_ra_c;
  logic [PAGE_WORD_W-1:0] page0_rd_q, page1_rd_q;

  // The bank each consumer read FROM, latched with the read. Muxing on the
  // live `active_bank_q` would select the wrong register if the atomic
  // activation edge landed between a read and its use. The design only swaps
  // on data quiet, so that cannot happen today -- a one-bit latch makes the
  // argument unnecessary rather than load-bearing.
  //
  // The quiet-swap half of that is a claim about behaviour, so it names its
  // check: the directed contract drives an activation with both quiet
  // witnesses low and asserts the page does NOT activate ("no quiet source
  // activated the page while both witnesses were low"), and the
  // early-activation-while-held mutant is its positive control.
  // ENFORCED-BY: tests/texture/texture_binding_resolver_v2_directed.cpp
  logic crc_bank_q, read_bank_q;
  logic [255:0] page0_valid_q, page1_valid_q;
  logic active_bank_q, staging_bank_q;
  logic [7:0] active_generation_q, staging_generation_q;

  typedef enum logic [2:0] {
    LOAD_IDLE, LOAD_LOADING, LOAD_CRC_SCAN, LOAD_CRC_CHECK, LOAD_SEAL_PENDING
  } loader_state_t;
  loader_state_t loader_state_q;

  assign active_page_generation_o = active_generation_q;
  assign cfg_loader_idle_o = loader_state_q == LOAD_IDLE;
  // LOAD_CRC_CHECK counts as busy. The seal verdict moved out of the scan's
  // last byte into its own state on 2026-09-18, and this line is what keeps
  // that invisible from outside: a consumer that waits for busy to fall still
  // sees it fall exactly once, when the page has been judged. Leaving it as
  // `== LOAD_CRC_SCAN` would drop busy for one cycle before the verdict, which
  // is a new externally-observable pulse for an internal pipelining decision.
  assign binding_crc_busy_o =
      (loader_state_q == LOAD_CRC_SCAN) || (loader_state_q == LOAD_CRC_CHECK);
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
  logic [3:0] crc_byte_q;
  logic [31:0] crc_q, crc_expected_q;
  logic crc_row_present_q, crc_have_row_q;
  wire [79:0] crc_canonical_row_c = crc_row_present_q
      ? {5'b0, crc_row_c} : 80'd0;
  wire [7:0] crc_byte_c =
      crc_canonical_row_c[crc_byte_q*8 +: 8];
  wire [31:0] crc_step_c = crc32_byte(crc_q, crc_byte_c);
  // `crc_final_c = crc_step_c ^ 32'hFFFF_FFFF` used to live here and was the
  // head of the 512-path family. The final xor now happens in LOAD_CRC_CHECK,
  // against the REGISTERED `crc_q`, so this wire has no reader and is gone
  // rather than left behind to read as live.
  wire [7:0] crc_next_selector_c = crc_selector_q + 8'd1;

  // --------------------------------------------------------------------------
  // Data plane: a synchronous read hold followed by one reserved disposition.
  sample_job_t read_job_q;
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

  // The two CRC-walk reads are mutually exclusive branches of one FSM writing
  // one destination, so they are ONE port with a muxed address, not two.
  always_comb begin
    crc_read_first_c = (loader_state_q == LOAD_CRC_SCAN) && !crc_have_row_q;
    crc_read_next_c  = (loader_state_q == LOAD_CRC_SCAN) && crc_have_row_q &&
                       (crc_byte_q == 4'd9) && (crc_selector_q != 8'hFF);
    crc_re_c = crc_read_first_c || crc_read_next_c;
    crc_ra_c = crc_read_first_c ? crc_selector_q : crc_next_selector_c;

    // Exactly the condition the data plane's own read used.
    data_re_c = req_accept_c && !req_force_refuse_i &&
                !req_selector_overflow_i &&
                (req_page_generation_i != 8'd0) &&
                (req_page_generation_i == active_generation_q);

    // page0 serves the data plane while it is ACTIVE and the CRC walk while it
    // is STAGING; page1 is the complement.
    page0_re_c = active_bank_q ? crc_re_c : data_re_c;
    page0_ra_c = active_bank_q ? crc_ra_c : req_binding_selector_i;
    page1_re_c = active_bank_q ? data_re_c : crc_re_c;
    page1_ra_c = active_bank_q ? req_binding_selector_i : crc_ra_c;
  end

  // The inferrable memories. No reset on the output registers: a reset there is
  // one of the things that costs the inference, and every consumer gates on its
  // own `_present` flag rather than on the row's contents.
  always_ff @(posedge clk) begin
    if (page0_re_c) page0_rd_q <= page0_m[page0_ra_c];
    if (page1_re_c) page1_rd_q <= page1_m[page1_ra_c];
  end

  // DECLARED THEN ASSIGNED, not `wire binding_row_t x = ...`. Verilator accepts
  // the combined form and Quartus 17.0 rejects it outright:
  //
  //   Error (10149): identifier "binding_row_t" is already declared in the
  //   present scope
  //   Error (10170): syntax error near text: "crc_row_c"; expecting ";"
  //
  // QUARTUS_GOTCHAS' standing lesson, one form further on: a clean Verilator
  // lint settles one tool's opinion and says nothing about synthesizability.
  binding_row_t crc_row_c;
  binding_row_t read_row_c;
  logic         read_row_legal_c;
  assign crc_row_c  = binding_row_t'(crc_bank_q  ? page1_rd_q[PAGE_ROW_W-1:0]
                                                  : page0_rd_q[PAGE_ROW_W-1:0]);
  assign read_row_c = binding_row_t'(read_bank_q ? page1_rd_q[PAGE_ROW_W-1:0]
                                                  : page0_rd_q[PAGE_ROW_W-1:0]);
  // The law's verdict, read from the same word as the row and by the same
  // selection, so the two cannot come from different entries.
  assign read_row_legal_c =
      read_bank_q ? page1_rd_q[PAGE_LEGAL_B] : page0_rd_q[PAGE_LEGAL_B];

  assign iss_tmu_valid_o = req_accept_c;
  assign iss_tmu_handle_o = req_sample_handle_i;

  logic [1:0] read_class_c;
  logic read_row_bad_c, read_witness_bad_c;
  logic read_generation_bad_c, read_refuse_c;
  // THE PICK'S ARRIVAL POINT. `zref::Tileset` is one object of 256 CLUT8 tiles
  // of 4,096 bytes, so the tile index is a byte displacement of the row's base
  // and the TMU's mirrored 64x64 fold then indexes inside it -- `ts->tiles
  // [tile][(ty << 6) + tx]`, the whole of `rast.cpp:370`, split across the two
  // blocks that already own its two halves.
  logic read_tileset_c;
  logic [31:0] read_base_c;
  always_comb begin
    read_tileset_c = read_row_c.mode[MODE_TILESET_B];
    read_base_c = read_tileset_c
        ? (read_row_c.base +
           ({24'd0, read_job_q.mosaic_tile} << TILESET_TILE_LOG2B))
        : read_row_c.base;
  end
  always_comb begin
    read_class_c = binding_class(read_row_c.mode);
    read_generation_bad_c =
        (read_job_q.page_generation == 8'd0) ||
        (read_job_q.page_generation != active_generation_q);
    // Was `!binding_row_legal(read_row_c)` -- the same law, re-derived here
    // from the row that had just left the memory, and the single largest
    // timing cost in the composed shell. It now reads the verdict stored
    // beside the row. Logically identical: `legal` is written by the law from
    // the same bits, in the same cycle as the row, and both are read back
    // together by the selection above.
    read_row_bad_c = !read_row_present_q || !read_row_legal_c;
    read_witness_bad_c =
        (read_job_q.handle[GENW +: 2] == 2'd0) &&
        ({read_job_q.witness_class,
          read_job_q.witness_palette_slot,
          read_job_q.witness_palette_generation} !=
         {read_class_c, read_row_c.palette_slot,
          read_row_c.palette_generation});
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
      crc_byte_q <= 4'd0;
      crc_q <= 32'd0;
      crc_expected_q <= 32'd0;
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
      tileset_samples_o <= 32'd0;
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
          // `legal` is the law applied to the bytes being stored. It is the
          // same expression the CFG_WRITE guard above already evaluated on the
          // same operand, so CSE shares one cone; and because that guard has
          // already refused every row for which it is 0, the value written
          // here is always 1 in practice. Written as the call rather than as
          // `1'b1` so the field states what it means and survives a change to
          // the guard.
          if (staging_bank_q) begin
            page1_m[cfg_selector_i] <=
                {binding_row_legal(cfg_row_i), cfg_row_i};
            page1_valid_q[cfg_selector_i] <= 1'b1;
          end else begin
            page0_m[cfg_selector_i] <=
                {binding_row_legal(cfg_row_i), cfg_row_i};
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
          crc_byte_q <= 4'd0;
          crc_have_row_q <= 1'b0;
          loader_state_q <= LOAD_CRC_SCAN;
        end
      end

      // Synchronous staging-bank scan. Invalid selectors contribute ten zero
      // bytes regardless of stale payload. One shared byte step is evaluated per
      // clock; configuration latency is off the render path and the serialized
      // CRC removes the measured ten-byte combinational chain.
      if (loader_state_q == LOAD_CRC_SCAN) begin
        if (!crc_have_row_q) begin
          crc_bank_q <= staging_bank_q;
          if (staging_bank_q)
            crc_row_present_q <= page1_valid_q[crc_selector_q];
          else
            crc_row_present_q <= page0_valid_q[crc_selector_q];
          crc_byte_q <= 4'd0;
          crc_have_row_q <= 1'b1;
        end else if (crc_byte_q != 4'd9) begin
          crc_q <= crc_step_c;
          crc_byte_q <= crc_byte_q + 4'd1;
        end else if (crc_selector_q == 8'hFF) begin
          // THE LAST BYTE IS FOLDED HERE AND JUDGED NEXT CYCLE.
          //
          // This branch used to compare `crc_final_c` -- which is
          // `crc32_byte(crc_q, <byte from the RAM>) ^ 32'hFFFF_FFFF` -- and
          // clear a 256-bit valid vector on the same edge. That put the whole
          // chain in one clock, and the 2026-09-18 composed fit measured it:
          //
          //   portbdataout    -> Mux10~4/5/6   +4.86 ns   the byte select
          //                   -> crc~3         +0.56      one crc32_byte round
          //                   -> Equal35~*     +3.49      the 32-bit compare
          //                   -> page*_valid_q +4.32      the clear fanout
          //                                    ------
          //                                    13.69 ns against a 10 ns period
          //
          // 512 of the 2,000 worst paths in the whole machine were this, one
          // per valid bit per bank -- the largest single family, and four times
          // the size of the legality cone that looked like the headline.
          //
          // Folding into `crc_q` and deciding in LOAD_CRC_CHECK splits it at
          // the register: the RAM-to-CRC half ends at a flip-flop, and the
          // compare-and-clear half starts at one. The seal VALUE is untouched
          // -- `crc_q` takes exactly the `crc_step_c` the old code compared,
          // and the next state xors the same constant -- so the bytes folded,
          // their order, and the accept/refuse outcome are all identical. Only
          // the edge on which the outcome is acted upon moves, by one cycle,
          // in a walk that already spends 256 x 10 of them off the render path.
          crc_have_row_q <= 1'b0;
          crc_byte_q <= 4'd0;
          crc_q <= crc_step_c;
          loader_state_q <= LOAD_CRC_CHECK;
        end else begin
          crc_q <= crc_step_c;
          crc_selector_q <= crc_next_selector_c;
          crc_byte_q <= 4'd0;
          crc_bank_q <= staging_bank_q;
          if (staging_bank_q)
            crc_row_present_q <= page1_valid_q[crc_next_selector_c];
          else
            crc_row_present_q <= page0_valid_q[crc_next_selector_c];
        end
      end

      // The seal verdict, one cycle after the last byte was folded. `crc_q` is
      // a register here, so this cone starts at a flip-flop rather than at the
      // page RAM: no byte select and no crc32_byte round in front of it.
      if (loader_state_q == LOAD_CRC_CHECK) begin
        if ((crc_q ^ 32'hFFFF_FFFF) == crc_expected_q) begin
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
              mosaic_tile: req_mosaic_tile_i,
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
            read_bank_q <= active_bank_q;
            if (active_bank_q)
              read_row_present_q <= page1_valid_q[req_binding_selector_i];
            else
              read_row_present_q <= page0_valid_q[req_binding_selector_i];
          end else begin
            // The row itself is no longer cleared here: a constant written into
            // a RAM read register costs the inference. Every consumer
            // short-circuits on `read_row_present_q` -- `read_row_bad_c` tests
            // it first -- and the disposition stage loads its plan and its
            // refusal on the same edge under the same condition, so a held row
            // behind a cleared present can never be published.
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
          disposition_plan_q.base <= read_base_c;
          disposition_plan_q.mode <= read_row_c.mode;
          disposition_plan_q.palette_slot <= read_row_c.palette_slot;
          disposition_plan_q.palette_generation <=
              read_row_c.palette_generation;
          disposition_plan_q.u <= read_job_q.u;
          disposition_plan_q.v <= read_job_q.v;
          disposition_plan_q.lod_q4_4 <= read_job_q.lod_q4_4;

          if (read_refuse_c)
            binding_fault_o <= 1'b1;
          // CENSUS. A sample that is going to be PLANNED against a tileset row
          // -- so the pick displaced the address that will actually be read.
          // A refused job is not counted: nothing is sampled for it.
          if (!read_refuse_c && read_tileset_c)
            tileset_samples_o <= tileset_samples_o + 32'd1;
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
    if (($bits(binding_row_t) != 75) || ($bits(sample_job_t) != 126) ||
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
