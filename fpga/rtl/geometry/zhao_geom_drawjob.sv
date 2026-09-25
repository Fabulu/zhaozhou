// zhao_geom_drawjob.sv -- ONE DrawForm becomes GEOM.MESHFETCH's jobs.
//
// Law: owner ruling R29 (2026-09-19), frozen in `zref::drawjob`
//      (reference/include/zref/zref_drawjob.hpp). Read that header first: it
//      cites, field by field, the spec sentence each job field comes from.
// Differential: tests/geometry/geom_drawjob_directed.cpp (RTL against zref).
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS
// ---------------------------------------------------------------------------
// The resolver core entries I36 and I41 both name and neither could build: the
// thing that turns a ratified `DrawForm 0x0300` -- three handle32s and some
// flags -- into the six-field job GEOM.MESHFETCH takes. It does THREE lookups
// and no arithmetic worth the name:
//
//   form      -> the MESH_STREAM residency row (spec/memory_rules.md 5f.1),
//                then that page's 64-byte HEADER, which R29 froze, which gives
//                the descriptor table's offset, count, format and generation;
//   transform -> the INSTANCE TRANSFORM PALETTE row, written by GEOM.LOOM's
//                emit stream (GEOM.LOOM.md Out: "{node_index, transform[12]}");
//   flags     -> `zref::raster_state::compose`, R28's word.
//
// ---------------------------------------------------------------------------
// WHY THE STATE TRAVELS WITH THE MESHLET INSTEAD OF BEING WIRED IN PARALLEL
// ---------------------------------------------------------------------------
// Core entry I39 refuses, by name, the join this block could have been: two
// live wires -- a draw's flags on one path and a meshlet on another -- paired
// by nothing but their timing. A draw's raster word, material set and semantic
// weight therefore leave HERE, in the job's own handshake, and ride through
// GEOM.MESHFETCH and GEOM.ASSETFETCH beside the meshlet they belong to
// (`j_side_o` -> `r_side_o` -> `s_side_o`). A meshlet cannot then be paired
// with another draw's state, because there is no second path for it to arrive
// on.
//
// `j_stream_base_o` travels the same way and for the same reason: the
// descriptor's offsets are PAGE-relative and GEOM.ASSETFETCH's are
// POOL-relative, so somebody must add the page's base, and only the job knows
// which page it is.
//
// ---------------------------------------------------------------------------
// THE GUARD'S VERDICT IS ONE CYCLE AFTER THE ACCEPT
// ---------------------------------------------------------------------------
// S_VERD exists for the reason `zhao_geom_meshfetch` spells out at length and
// `tools/rtl/check_guard_verdict.py` now enforces: `ready` and `ok` are NEVER
// high together, so a client that tests both in one cycle reads every pass as a
// denial, silently. The request drops when S_VERD is entered.
//
// THE HEADER'S CRC IS THE DESCRIPTOR'S WALKER, NOT A SECOND FOLD. The header
// has the descriptor's framing on purpose -- 64 bytes, eight beats, CRC over
// 0..59 at 60 -- so `zhao_geom_desc_crc` reads it unmodified. One CRC law.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_geom_drawjob
  import zhao_pkg::*;
#(
    // Residency rows for MESH_STREAM pages. The directory is keyed by the
    // handle INDEX and entered by the publication's SLOT, exactly as
    // `zhao_material_resolve` keys its own (5f.1 is one rule for both).
    parameter int unsigned DIR_SETS = 4,
    // Draw-addressable instance transforms. The ruling's content tier is 256
    // creatures; a transform id at or above this is REFUSED, never wrapped.
    parameter int unsigned XFORMS = 256,
    // GEOM.LOOM's node-index width on its emit port.
    parameter int unsigned LOOM_IDXW = 10,
    // {semantic_weight[7:0], material_set[31:0], raster_state[31:0]}
    parameter int unsigned SIDEW = 72,
    // DrawWarpedForm 0x0304's descriptor cookie -- {en, stamp[4:0]}, directive
    // 7.1's "compact descriptor cookie". See `d_warp_cookie_i` below.
    parameter int unsigned WCKW = 6
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the ratified draw, whole (CMD.EXEC's DrawForm arm) ----------------
    input  var logic         d_valid_i,
    output var logic         d_ready_o,
    input  var logic [31:0]  d_form_i,
    input  var logic [31:0]  d_material_set_i,
    /* verilator lint_off UNUSEDSIGNAL */
    // [7:0] is the transform handle's GENERATION, and nothing in this console
    // produces one for an instance transform: GEOM.LOOM emits {node_index,
    // transform} with no generation, and `spec/commands.zidl` says
    // ZH_ABI_STALE_HANDLE has no v1 user. Checking it against an invented
    // value would manufacture its own mismatch. Recorded rather than waived
    // silently: when the Loom gains REPARENT's epoch/generation this is where
    // it is compared.
    input  var logic [31:0]  d_transform_i,
    // [7:2] name views this console does not have -- SetPresentationContract
    // carries at most two. A draw that sets them draws in the views it also
    // named; the bits are not a refusal because nothing ratifies them as one.
    input  var logic [7:0]   d_viewport_mask_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic [7:0]   d_semantic_weight_i,
    input  var logic [15:0]  d_flags_i,
    input  var logic [15:0]  d_src_id_i,

    // ---- DrawWarpedForm 0x0304's DESCRIPTOR COOKIE -------------------------
    // Directive 7.1: "the common draw item holds `warp_enabled` plus a compact
    // descriptor cookie, not an 80-byte Warp record copied through every
    // geometry stage." This is that cookie. `zhao_geom_warpbook` allocates it
    // ON THIS SAME HANDSHAKE -- its `w_fire_i` is `d_valid_i && d_ready_o` --
    // so the cookie and the draw are latched by one edge and there is no
    // second path for either to arrive on. That is composer entry I39's
    // argument for the raster sideband, one field along.
    //
    // IT IS NOT KEYED ON `d_src_id_i`, and that was measured rather than
    // assumed. `src_id` is capture_format.md 5's `index` alone -- a compiler
    // source-registry ordinal naming an emit SITE, shared by every draw from
    // that statement -- and decision W08 says the same from the other side:
    // "source_id remains attribution, not identity."
    input  var logic [WCKW-1:0] d_warp_cookie_i,

    // ---- the MESH_STREAM residency directory (MEM.UPLOAD's publication) ----
    input  var logic         dir_we_i,
    input  var logic [7:0]   dir_entry_i,       // the publication's slot
    input  var logic [23:0]  dir_index_i,       // the resource's handle index
    input  var logic [15:0]  dir_generation_i,
    input  var logic [31:0]  dir_base_i,
    input  var logic [31:0]  dir_extent_i,

    // ---- the instance transform palette (GEOM.LOOM's emit stream) ----------
    input  var logic                  px_valid_i,
    input  var logic [LOOM_IDXW-1:0]  px_index_i,
    input  var logic signed [31:0]    px_m_i [12],

    // ---- the header read: one 64-byte line through MEM.GUARD ---------------
    input  var zhao_client_e    client_i,
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    input  var logic            beat_valid_i,
    input  var logic [63:0]     beat_data_i,
    input  var logic            beat_last_i,
    // The CRC over bytes 0..59, folded by the CALLER'S `zhao_geom_desc_crc`
    // over these same beats -- wired in rather than folded here, exactly as
    // GEOM.MESHFETCH takes it and for the same reason: that walker is the one
    // implementation and a second instance inside this block would also make it
    // a census top inside a census top.
    input  var logic            crc_ok_i,

    // ---- the job, into GEOM.MESHFETCH --------------------------------------
    output var logic                  j_valid_o,
    input  var logic                  j_ready_i,
    output var logic [15:0]           j_instance_id_o,
    output var logic [26:0]           j_desc_addr_o,
    output var logic [7:0]            j_format_o,
    output var logic [15:0]           j_generation_o,
    output var logic [1:0]            j_active_mask_o,
    output var logic signed [31:0]    j_xform_o [12],
    output var logic [31:0]           j_stream_base_o,
    output var logic [SIDEW-1:0]      j_side_o,
    // THE DRAW'S FORM INDEX, all 24 bits, on the job that was ACCEPTED.
    //
    // This is `form_idx_q` -- `d_form_i[31:8]`, `spec/memory_rules.md` 5f.1's
    // residency key and the number `zref_creature_page.hpp` names as THE
    // per-creature-type key at the hardware boundary ("THE KEY IS THE
    // MESH_STREAM HANDLE INDEX, NOT AN INVENTED TYPE ID"). The owner ruling of
    // 2026-09-21 section 3 requires it to be EXPOSED here rather than
    // re-derived, and requires its CONSUMER to land with it:
    // `zhao_geom_clipread.p_form_idx_i` is that consumer and lands in the same
    // commit.
    //
    // IT IS GATED BY STATE AND NOT OFFERED BARE. `d_ready_o` is
    // `(st_q == S_IDLE)`, so in S_IDLE the register still holds the PREVIOUS
    // draw; a consumer reading it there would associate this draw's pages with
    // the last draw's form. It is therefore driven from the register ONLY
    // while `j_valid_o` is high -- the job's own lifetime and handshake -- and
    // zero otherwise. Zero is not a sentinel (index 0 is a legal form); the
    // QUALIFIER is `j_valid_o`, exactly as it is for every other `j_*` field.
    output var logic [23:0]           j_form_idx_o,

    // THE COOKIE, ON THE JOB THAT WAS ACCEPTED. Gated by state exactly as
    // `j_form_idx_o` above is, and for the identical reason: `d_ready_o` is
    // `(st_q == S_IDLE)`, so in S_IDLE the register still holds the PREVIOUS
    // draw's cookie and a consumer reading it there would hand this draw's
    // vertices the last draw's deformation. It is driven from the register
    // ONLY while `j_valid_o` is high, and zero otherwise -- and zero here is a
    // cookie with its `en` bit CLEAR, which `zhao_geom_warpbook` answers as
    // "no warp" rather than as entry 0. Unlike `j_form_idx_o`'s zero, that is
    // a sentinel and is safe to be one.
    output var logic [WCKW-1:0]       j_warp_cookie_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] draws_o,          // DrawForms accepted
    output var logic [31:0] jobs_o,           // jobs emitted
    output var logic [31:0] masked_o,         // viewport_mask[1:0] == 0
    output var logic [31:0] empty_o,          // a legal stream of zero meshlets
    output var logic [31:0] pal_writes_o,     // palette rows written by GEOM.LOOM
    output var logic [31:0] pal_dropped_o,    // a node index past the palette
    output var logic [31:0] refused_o [9]     // zref::drawjob::Refusal order
);

  // The descriptor format this reader speaks, and the frozen strides.
  localparam logic [7:0]  SUPPORTED_FORMAT_C = 8'd1;
  localparam int unsigned HEADER_BYTES_C     = 64;
  localparam int unsigned DESC_BYTES_C       = 64;
  // R28's material half, R48's precedent: every v1 material writes 0 in
  // raster_state[31:2], so the value is NAMED here rather than fetched. When a
  // material format gains a raster bit, MATERIAL.RESOLVE's record replaces this
  // constant at this seam and nothing else moves.
  localparam logic [29:0] V1_MATERIAL_RASTER_C = 30'd0;
  // The palette's address width. Held apart from XFORMS so XFORMS = 1 is legal
  // (a zero-width index is not).
  localparam int unsigned XIDXW = (XFORMS <= 1) ? 1 : $clog2(XFORMS);

  initial begin
    // Quartus 17.0 needs an elaboration guard inside `initial` (a module-scope
    // `if` is a syntax error there, and `--lint-only` never runs this block).
    if (SIDEW != 72)
      $fatal(1, "zhao_geom_drawjob: SIDEW is the packing {weight,material_set,raster}");
    if (XFORMS == 0)
      $fatal(1, "zhao_geom_drawjob: XFORMS must be at least one");
    if (DIR_SETS == 0)
      $fatal(1, "zhao_geom_drawjob: DIR_SETS must be at least one");
  end

  typedef enum logic [2:0] {
    S_IDLE, S_CHECK, S_REQ, S_VERD, S_FILL, S_VALID, S_EMIT
  } state_e;
  state_e st_q;

  // ---- the directory -------------------------------------------------------
  logic            dir_v_q     [DIR_SETS];
  logic [23:0]     dir_index_q [DIR_SETS];
  logic [15:0]     dir_gen_q   [DIR_SETS];
  logic [31:0]     dir_base_q  [DIR_SETS];
  logic [31:0]     dir_ext_q   [DIR_SETS];

  // ---- the palette. A REGISTERED READ WITH AN ENABLE, so the row holds for
  // the whole draw and Quartus infers M10K rather than 98,304 flops. The valid
  // bits are separate flops because they must be CLEARED by reset and an
  // asynchronous clear on the array would destroy that inference (the argument
  // `zhao_geom_loom` makes about its own store).
  //
  // THAT SENTENCE WAS AN ASSERTION ABOUT A SYNTHESIS OUTCOME THAT HAD NEVER
  // BEEN CHECKED AGAINST A SYNTHESIS RESULT, AND IT WAS FALSE FOR SIX DAYS.
  // Measured 2026-09-26, `zhao_geom_drawjob@palram-base`, quartus_map 17.0.2:
  // 100,561 registers and ZERO block memory bits, with a 256:1 x 384-bit read
  // multiplexer costing 65,280 LEs. Every precaution the paragraph above
  // describes was present and correct, and the array was still 98,304 flops.
  //
  // THE CAUSE WAS IN THE WRITE, AND IT IS A CONJUNCTION OF TWO THINGS.
  // Neither one alone does any harm, which is exactly why it survived review:
  // every individual property here reads as correct. The write used to be
  // twelve 32-bit PARTIAL-SELECT assignments in an unrolled for loop, indexed
  // by a 32-bit expression on a 256-deep array:
  //
  //     for (int unsigned k = 0; k < 12; k++)
  //       pal_q[int'(px_index_i)][32 * k +: 32] <= px_m_i[k];
  //
  //   (i)  the element is written through a PART-SELECT rather than whole, and
  //   (ii) the index expression is WIDER THAN THE ARRAY'S ADDRESS -- `int'()`
  //        of a LOOM_IDXW (10-bit) port, cast to 32 bits, on 256 rows, so the
  //        value can leave the array's range and Quartus must reason about it.
  //
  // Isolated one change at a time by `tests/probes/zhao_palram_probe.sv`, whose
  // arm 0 is this description verbatim and reproduces the defect exactly
  // (99,008 registers = 98,304 + 256 + 384 + 64, 0 memory bits):
  //
  //   arm 0  both (i) and (ii)                  99,008 reg        0 mem bits
  //   arm 1  whole-element write, (ii) kept        320 reg   98,304 mem bits
  //   arm 2  part-select kept, index narrowed      320 reg   98,304 mem bits
  //   arm 3  read split to its own always_ff    99,008 reg        0 mem bits
  //   arm 4  arms 1 and 2 together                 320 reg   98,304 mem bits
  //
  // So REMOVING EITHER ONE restores inference, and the shared always_ff -- the
  // property most likely to be blamed -- is innocent (arm 3 is the control that
  // says so). This block takes the arm 1 repair: assemble the word
  // combinationally and write the ELEMENT whole. The wide index is retained
  // because it is harmless on its own and because the enable wants the full
  // width; narrowing it is the independent second lever if the write ever has
  // to go back to slices.
  //
  // WHY NOTHING WARNED. Quartus 17.0's RAM recognition matches a whole-element
  // assignment `mem[addr] <= expr`. Under the conjunction above `pal_q` was
  // never a RAM CANDIDATE -- which is why it appears in no `Info (276004)`
  // uninferred line with a reason attached, while a dozen sibling arrays do.
  // The array was not rejected; it was never considered. Silence from that
  // list means "not a candidate", NOT "fine".
  //
  // This is the third killer in reports/QUARTUS_GOTCHAS.md section 10, whose
  // 102-bench calibration grid already ruled that a byte-enabled (per-slice)
  // write infers NOTHING on this device -- and `zhao_surface_sheet.sv:164`
  // paid 131,258 registers for the same construct in August. That knowledge
  // was on disk the whole time and no instrument read it back;
  // `tools/quartus/check_ram_inference.py` rule 5 now does.
  //
  // Keep it whole. Reintroduce a partial-select write here and 98,304
  // flip-flops come back silently, with every gate in this tree still green.
  logic [383:0] pal_q [XFORMS];
  logic         pal_v_q [XFORMS];
  logic [383:0] pal_rd_q;

  // ---- the latched draw ----------------------------------------------------
  logic [23:0]  form_idx_q;
  logic [7:0]   form_gen_q;
  logic [31:0]  mset_q;
  logic [23:0]  xf_idx_q;
  logic [1:0]   mask_q;
  logic [7:0]   weight_q;
  /* verilator lint_off UNUSEDSIGNAL */
  // Only flags[3:2] (R28's cull mode) is hardware's. Bits 0-1 are the marker
  // law's (billboard, screen-space size), which the software renderer owns,
  // and 4-15 are reserved 0.
  logic [15:0]  flags_q;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [15:0]  src_q;
  logic [WCKW-1:0] wck_q;   // 0x0304's descriptor cookie, latched with the draw
  logic [31:0]  base_q, ext_q;

  // ---- the header, as eight beats -----------------------------------------
  logic [63:0] h_q [8];
  logic [2:0]  beat_q;
  logic        crc_ok_q;

  // ---- the emit walk -------------------------------------------------------
  logic [26:0] desc_addr_q;
  logic [16:0] rem_q;            // meshlets left, 0..65536
  logic [7:0]  fmt_q;
  logic [15:0] gen_q;
  logic [31:0] sbase_q;
  logic [31:0] raster_q;

  // ---- header field views (little-endian, as the oracle reads them) --------
  function automatic logic [7:0] hb(input int unsigned i);
    hb = h_q[i / 8][8 * (i % 8) +: 8];
  endfunction
  function automatic logic [15:0] hh(input int unsigned i);
    hh = {hb(i + 1), hb(i)};
  endfunction
  function automatic logic [31:0] hw(input int unsigned i);
    hw = {hb(i + 3), hb(i + 2), hb(i + 1), hb(i)};
  endfunction

  // ---- the draw's cull mode, R28 ------------------------------------------
  wire [1:0] cull_c     = flags_q[3:2];
  wire       cull_bad_c = (cull_c == 2'd3);     // RESERVED: refused, never aliased
  wire [1:0] cull_in_c  = d_flags_i[3:2];

  // ---- the directory lookup ------------------------------------------------
  logic        dir_hit_c;
  /* verilator lint_off UNUSEDSIGNAL */
  // Only [7:0] is compared: a handle32 carries eight generation bits and the
  // directory sixteen, and `zhao_material_resolve` settles the same question
  // the same way for the same handle shape. The full sixteen stay in the row
  // because 5f.1 keeps them ("a directory that kept only the eight the handle
  // carries would alias every 256th publication") -- the CACHE tag's business,
  // not this comparison's.
  logic [15:0] dir_hit_gen_c;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [31:0] dir_hit_base_c, dir_hit_ext_c;
  always_comb begin
    dir_hit_c      = 1'b0;
    dir_hit_gen_c  = '0;
    dir_hit_base_c = '0;
    dir_hit_ext_c  = '0;
    for (int unsigned s = 0; s < DIR_SETS; s++) begin
      if (!dir_hit_c && dir_v_q[s] && (dir_index_q[s] == form_idx_q)) begin
        dir_hit_c      = 1'b1;
        dir_hit_gen_c  = dir_gen_q[s];
        dir_hit_base_c = dir_base_q[s];
        dir_hit_ext_c  = dir_ext_q[s];
      end
    end
  end

  wire xf_in_range_c = (xf_idx_q < 24'(XFORMS));
  wire xf_valid_c    = xf_in_range_c && pal_v_q[int'(xf_idx_q[XIDXW-1:0])];

  // ---- stage 1: everything decidable before a byte is read ----------------
  // Priority IS the oracle's: cull, residency, staleness, transform.
  logic [3:0] pre_ref_c;     // 4'hF = none, else the Refusal ordinal
  always_comb begin
    if      (cull_bad_c)                                   pre_ref_c = 4'd0;
    else if (!dir_hit_c)                                   pre_ref_c = 4'd1;
    else if (dir_hit_gen_c[7:0] != form_gen_q)             pre_ref_c = 4'd2;
    else if (!xf_valid_c)                                  pre_ref_c = 4'd3;
    else                                                   pre_ref_c = 4'hF;
  end

  // ---- stage 2: the header, once its last beat has landed -----------------
  // The four header fields the checks read, named once. A part-select of a
  // function call is not legal SystemVerilog, and naming them also keeps the
  // decode in ONE place for the oracle to be compared against.
  wire [7:0]  hdr_format_c = hb(0);
  wire [15:0] hdr_count_c  = hh(2);
  wire [15:0] hdr_gen_c    = hh(4);
  wire [31:0] hdr_dofs_c   = hw(8);

  logic        hdr_resv_nz_c;
  logic [33:0] table_end_c;
  logic [3:0]  hdr_ref_c;
  always_comb begin
    hdr_resv_nz_c = (hb(1) != 8'd0) || (hh(6) != 16'd0);
    for (int unsigned i = 12; i < 60; i++) if (hb(i) != 8'd0) hdr_resv_nz_c = 1'b1;
    // desc_offset + 64 * meshlet_count, at full width: a 32-bit sum could wrap
    // and report a table INSIDE a page it ends far outside.
    table_end_c = {2'b00, hdr_dofs_c} + ({18'd0, hdr_count_c} << 6);

    if      (hdr_format_c != SUPPORTED_FORMAT_C)           hdr_ref_c = 4'd5;
    else if (!crc_ok_q)                                    hdr_ref_c = 4'd6;
    else if (hdr_resv_nz_c)                                hdr_ref_c = 4'd7;
    else if ((base_q[5:0] != 6'd0) || (hdr_dofs_c[5:0] != 6'd0) ||
             (hdr_dofs_c < 32'(HEADER_BYTES_C)) ||
             (table_end_c > {2'b00, ext_q}))               hdr_ref_c = 4'd8;
    else                                                   hdr_ref_c = 4'hF;
  end

  // ---- ports ---------------------------------------------------------------
  assign d_ready_o = (st_q == S_IDLE);

  assign guard_req_o.valid  = (st_q == S_REQ);
  assign guard_req_o.write  = 1'b0;
  assign guard_req_o.client = client_i;
  assign guard_req_o.addr   = base_q[ZHAO_VRAM_ADDR_BITS-1:0];
  assign guard_req_o.len    = 7'd64;
  assign guard_req_o.be     = {64{1'b1}};

  assign j_valid_o       = (st_q == S_EMIT);
  assign j_instance_id_o = src_q;
  assign j_desc_addr_o   = desc_addr_q;
  assign j_format_o      = fmt_q;
  assign j_generation_o  = gen_q;
  assign j_active_mask_o = mask_q;
  assign j_stream_base_o = sbase_q;
  assign j_side_o        = {weight_q, mset_q, raster_q};
  assign j_form_idx_o    = (st_q == S_EMIT) ? form_idx_q : 24'd0;
  assign j_warp_cookie_o = (st_q == S_EMIT) ? wck_q : '0;

  always_comb begin
    for (int unsigned k = 0; k < 12; k++)
      j_xform_o[k] = $signed(pal_rd_q[32 * k +: 32]);
  end


  // ---- the palette write port (GEOM.LOOM never waits) ---------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int unsigned r = 0; r < XFORMS; r++) pal_v_q[r] <= 1'b0;
      pal_writes_o  <= '0;
      pal_dropped_o <= '0;
    end else begin
      if (px_valid_i) begin
        if (32'(px_index_i) < 32'(XFORMS)) begin
          pal_v_q[int'(px_index_i)] <= 1'b1;
          pal_writes_o <= pal_writes_o + 32'd1;
        end else begin
          // A node GEOM.LOOM composed that no draw can name. Counted rather
          // than wrapped: a wrapped index would overwrite another instance's
          // transform, which is silently wrong geometry.
          pal_dropped_o <= pal_dropped_o + 32'd1;
        end
      end
    end
  end

  // The row, assembled COMBINATIONALLY so the array write below can name the
  // whole element. This loop is the one that used to live inside the always_ff
  // writing `pal_q[...][32*k +: 32]` a slice at a time; moving it out here is
  // the entire repair, and it is bit-for-bit the same word. Every one of the
  // 384 bits is covered exactly once by k = 0..11, so `pal_wr_word_c` is fully
  // driven and there is no latch and no undriven bit. The unpacking at the
  // read end (`j_xform_o[k] = pal_rd_q[32*k +: 32]`, above) uses the identical
  // lane mapping, so the round trip is unchanged.
  logic [383:0] pal_wr_word_c;
  always_comb begin
    for (int unsigned k = 0; k < 12; k++)
      pal_wr_word_c[32 * k +: 32] = px_m_i[k];
  end

  // The array itself has NO reset, which is what lets it infer M10K -- and
  // the WHOLE-ELEMENT write is the other half of what lets it. See :229.
  // The enable is unchanged: the full-width `px_index_i < XFORMS` comparison
  // still guards the write, so a node index at or past the tier is still
  // dropped and counted by `pal_dropped_o` and is NEVER wrapped into another
  // instance's row.
  always_ff @(posedge clk) begin
    if (px_valid_i && (32'(px_index_i) < 32'(XFORMS))) begin
      pal_q[int'(px_index_i)] <= pal_wr_word_c;
    end
    if (st_q == S_CHECK) pal_rd_q <= pal_q[int'(xf_idx_q[XIDXW-1:0])];
  end

  // ---- the machine ---------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q     <= S_IDLE;
      beat_q   <= '0;
      crc_ok_q <= 1'b0;
      draws_o  <= '0;
      jobs_o   <= '0;
      masked_o <= '0;
      empty_o  <= '0;
      for (int unsigned i = 0; i < 9; i++) refused_o[i] <= '0;
      for (int unsigned s = 0; s < DIR_SETS; s++) dir_v_q[s] <= 1'b0;
    end else begin
      if (dir_we_i && (32'(dir_entry_i) < 32'(DIR_SETS))) begin
        dir_v_q[int'(dir_entry_i)]     <= 1'b1;
        dir_index_q[int'(dir_entry_i)] <= dir_index_i;
        dir_gen_q[int'(dir_entry_i)]   <= dir_generation_i;
        dir_base_q[int'(dir_entry_i)]  <= dir_base_i;
        dir_ext_q[int'(dir_entry_i)]   <= dir_extent_i;
      end

      case (st_q)
        S_IDLE: begin
          if (d_valid_i) begin
            form_idx_q <= d_form_i[31:8];
            form_gen_q <= d_form_i[7:0];
            mset_q     <= d_material_set_i;
            xf_idx_q   <= d_transform_i[31:8];
            mask_q     <= d_viewport_mask_i[1:0];
            weight_q   <= d_semantic_weight_i;
            flags_q    <= d_flags_i;
            src_q      <= d_src_id_i;
            wck_q      <= d_warp_cookie_i;
            draws_o    <= draws_o + 32'd1;
            // A zero mask draws nothing and reads nothing -- but a RESERVED
            // cull mode is malformed either way, so it is still refused.
            if (d_viewport_mask_i[1:0] == 2'd0) begin
              if (cull_in_c == 2'd3) refused_o[0] <= refused_o[0] + 32'd1;
              else                   masked_o     <= masked_o + 32'd1;
            end else begin
              st_q <= S_CHECK;
            end
          end
        end

        S_CHECK: begin
          if (pre_ref_c != 4'hF) begin
            refused_o[int'(pre_ref_c)] <= refused_o[int'(pre_ref_c)] + 32'd1;
            st_q <= S_IDLE;
          end else begin
            base_q <= dir_hit_base_c;
            ext_q  <= dir_hit_ext_c;
            beat_q <= '0;
            st_q   <= S_REQ;
          end
        end

        S_REQ: if (guard_rsp_i.ready) st_q <= S_VERD;

        S_VERD: begin
          if (guard_rsp_i.ok) begin
            st_q <= S_FILL;
          end else if (guard_rsp_i.violation) begin
            refused_o[4] <= refused_o[4] + 32'd1;
            st_q <= S_IDLE;
          end
        end

        S_FILL: if (beat_valid_i) begin
          h_q[beat_q] <= beat_data_i;
          beat_q      <= beat_q + 3'd1;
          if (beat_last_i) begin
            crc_ok_q <= crc_ok_i;
            st_q     <= S_VALID;
          end
        end

        S_VALID: begin
          if (hdr_ref_c != 4'hF) begin
            refused_o[int'(hdr_ref_c)] <= refused_o[int'(hdr_ref_c)] + 32'd1;
            st_q <= S_IDLE;
          end else if (hh(2) == 16'd0) begin
            // A legal stream of no meshlets. Not a refusal: an empty mesh
            // draws nothing, and calling that corruption would report a fault
            // every time a form is authored empty.
            empty_o <= empty_o + 32'd1;
            st_q    <= S_IDLE;
          end else begin
            desc_addr_q <= 27'(base_q + hw(8));
            rem_q       <= {1'b0, hh(2)};
            fmt_q       <= hb(0);
            gen_q       <= hdr_gen_c;
            sbase_q     <= base_q - ZHAO_RENDER_ASSET_BASE;
            raster_q    <= {V1_MATERIAL_RASTER_C, cull_c};
            st_q        <= S_EMIT;
          end
        end

        S_EMIT: if (j_ready_i) begin
          jobs_o      <= jobs_o + 32'd1;
          desc_addr_q <= desc_addr_q + 27'(DESC_BYTES_C);
          rem_q       <= rem_q - 17'd1;
          if (rem_q == 17'd1) st_q <= S_IDLE;
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire
