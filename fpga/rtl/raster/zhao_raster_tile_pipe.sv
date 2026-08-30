// zhao_raster_tile_pipe.sv — the phase-4/5 "first exact tile and triangle"
// composition: RASTER.EDGEWALK → RASTER.EARLYZ → RASTER.FRAGMENT →
// RASTER.TILESTORE → RASTER.RESOLVE, wired together into one triangle
// rasterized, shaded, depth/stencil/blend-tested and resolved to RGB565
// framebuffer words with a deterministic tile CRC.
//
// ---------------------------------------------------------------------------
// THIS BLOCK IS NOT IN design/blocks.yml, AND THAT IS DELIBERATE
// ---------------------------------------------------------------------------
// The ledger's RASTER group has exactly five entries — EDGEWALK, TILESTORE,
// EARLYZ, FRAGMENT, RESOLVE — and none of them is "the composition". There is
// no ledger block for this file and none was invented: registering a block is
// a validator-gated ledger edit (charter §4) and not this increment's call, so
// the rationale lives here instead of in a contract nobody's ledger row points
// at. What this file IS:
//
//   · the wiring the block contracts each declared as NOT built. RASTER.
//     RESOLVE.md, "Integration capture cases", says in as many words: "It has
//     also never been composed with `zhao_raster_tilestore`… no test
//     instantiates both, and the driver plays the store from a flat array
//     instead. That composition… is the next increment." This is that
//     increment.
//   · a COMPOSITION ONLY. Every law lives in the five blocks it instantiates
//     and in their contracts; this file adds exactly three things of its own,
//     each named below (the tile sequencing, the coverage→write expansion, and
//     the framebuffer pixel address). It re-derives no fill rule, no dither,
//     no CRC, no word layout.
//   · the FULL raster chain, as of the phase-4/5 raster completion. The
//     earlier revision of this file wrote ONE flat 64-bit word at every
//     covered pixel and said, in as many words: "When RASTER.FRAGMENT lands
//     it replaces the `RS_WALK` write path here, not the sequencing around
//     it." That is exactly what happened. RASTER.EARLYZ and RASTER.FRAGMENT
//     are instantiated between the coverage expansion and the tile store's
//     write port; law 1 (the sequencing and the ping-pong), law 2 (the
//     coverage→fragment expansion) and law 3 (the framebuffer address) are
//     unchanged, and the swap gate grew exactly one term — the fragment
//     pipeline must also be drained, not just the coverage mask.
//
//     THE PHASE-4 BEHAVIOUR IS PRESERVED BIT FOR BIT. `job_state_i == 0` is
//     the plain opaque write (depth test off, depth written, blend REPLACE,
//     no alpha test, stencil ALWAYS + REPLACE, tag written), so a job that
//     drives state 0 writes exactly `job_fill_word_i` at every covered pixel
//     — which is what this block did before, to the bit. The entire
//     pre-existing directed and random suite runs unchanged against the new
//     chain; that it still passes is the evidence that the replacement was a
//     write-path replacement and not a rewrite.
//
// WHAT THIS BLOCK IS NOT: no binning (it is handed one triangle × one tile,
// exactly as GEOM.BINNER would hand it), no VRAM addressing or framebuffer
// write (it emits a pixel stream and that pixel's SURFACE coordinate;
// MEM.GUARD and the write path own the address), no tile scheduling across a
// frame, no multi-triangle accumulation into one tile (one job = one clear +
// one triangle + one resolve), no attribute interpolation — the fragment
// source colour, alpha, depth, tag and texel are FLAT across the triangle,
// because interpolating them is GEOM.SETUP's job and GEOM.SETUP is not built.
//
// ---------------------------------------------------------------------------
// THE THREE LAWS THIS FILE OWNS
// ---------------------------------------------------------------------------
// 1. TILE SEQUENCING AND THE PING-PONG. Per job: clear the FRONT bank, walk
//    the triangle, write every covered pixel into the front bank, then SWAP
//    and hand the now-BACK bank to the resolve. The swap is the only
//    synchronisation point, and it is gated on BOTH sides:
//      · the raster stage must have retired its last write (a swap one cycle
//        early sends that write into the bank the resolve is about to read,
//        and the resolve then reads a hole), and
//      · the resolve must be idle (a swap one tile early pulls the bank out
//        from under a resolve that is still streaming it).
//    After the swap the raster stage is immediately free: `job_ready_o` rises
//    the next cycle, so tile N+1's clear and coverage run into the new front
//    bank WHILE tile N resolves out of the back bank. That overlap is the
//    entire reason RASTER.TILESTORE is ping-pong at all (its header: "resolve
//    streams the back bank at its own pace while the fragment pipeline already
//    renders the next tile into the front"), and it is measured, not asserted,
//    by tests/raster/raster_tile_pipe_directed.cpp:test_pingpong_overlap.
//
//    Ordering, cycle by cycle, is forced by RASTER.TILESTORE's rule 4 ("an
//    access accepted in the same cycle as a swap targets the roles as they
//    were BEFORE the swap"):
//      cycle T   — `swap` and the resolve's `start` are accepted together. The
//                  resolve cannot issue a read in its own accepting cycle
//                  (RASTER.RESOLVE.md's Latency section: `tr_valid_o` is a
//                  function of `busy_r`, which the accepting edge sets), so
//                  its first read lands at T+1, after the swap.
//      cycle T+1 — the raster stage is IDLE and may accept the next job.
//      cycle T+2 — the earliest that job's `clear` can be accepted. A clear
//                  only ever touches the FRONT bank, so it can never reach the
//                  tile now resolving out of the back one.
//
// 2. COVERAGE → FRAGMENTS. RASTER.EDGEWALK emits one 16-bit row mask per
//    non-empty row; RASTER.EARLYZ takes one fragment per cycle. This block
//    expands the mask lowest-column-first, one fragment per set bit, and
//    accepts the next coverage beat only once the current mask is drained.
//    Cost is exactly popcount(mask) fragment cycles per row plus one accept
//    cycle — no cycles are spent on uncovered columns. (The lowest-set-index
//    selector is the same idiom as EDGEWALK's own `drain_row`.) Uncovered
//    pixels never become fragments, so they are never written, keep their
//    PRESENT bit at 0 and resolve as the clear word — which is why a tile the
//    triangle misses entirely still resolves, and resolves to the cleared
//    colour, without costing 256 write cycles.
//
//    The fragment's payload is FLAT across the triangle: every covered pixel
//    gets the same source colour, alpha, depth, tag, stencil reference and
//    texel, taken from the job. That is not a simplification of the fragment
//    block — it is the absence of GEOM.SETUP, which is what would vary them
//    per pixel. RASTER.FRAGMENT itself never interpolates anything.
//
// 3. THE FRAMEBUFFER PIXEL ADDRESS. RASTER.RESOLVE emits `fb_addr_o` =
//    `{row, col}` WITHIN the tile; the surface coordinate is the resolving
//    tile's origin plus that, in the same signed 12-bit pixel space the job
//    came in on (§8's ±2048 px guard band). The resolving tile's origin is NOT
//    `job_tile_x_i` — by then the raster stage is a whole tile ahead — so it is
//    shadowed at the swap, together with the coverage count and the degenerate
//    flag, which are likewise the finishing tile's and not the walking one's.
//
// ---------------------------------------------------------------------------
// THE FLAT WORD, AND WHY IT IS A WORD AND NOT A COLOUR
// ---------------------------------------------------------------------------
// `job_fill_word_i` and `job_clear_word_i` are whole RASTER.TILESTORE words
// (that block's header, charter §8 order, MSB first: [63:40] RGB, [39:32]
// effect tag, [31:8] depth, [7:0] stencil). This block does not decode them
// and does not name their fields — RASTER.TILESTORE is field-agnostic and so
// is its composition. The layout is stated in exactly one place and this is
// not it.
//
// ---------------------------------------------------------------------------
// A KNOWN, ESCALATED ORACLE DEFECT RIDES THROUGH THIS BLOCK UNCHANGED
// ---------------------------------------------------------------------------
// reference/src/zrender/resolve.cpp resolves pure black to 0x0020 (green level
// 1) at the 8 Bayer cells with B ≥ 8, because green's dither amplitude is 32
// while red and blue get 16 (RASTER.RESOLVE.md, "FINDING — the BLACK rail is
// not clean"). A tile this block clears to black therefore resolves to a green
// speckle on black, not to 512 zero bytes. That is reproduced bit for bit and
// deliberately NOT fixed here: the oracle is the law, and changing it moves
// every golden capture's canvas CRC. The directed test pins the actual
// behaviour rather than the desired one.
//
// Conservative SystemVerilog subset only (charter §2). Depends on
// zhao_abi_pkg (through zhao_raster_resolve's generated CRC step),
// zhao_raster_fill, zhao_raster_edgewalk, zhao_raster_earlyz,
// zhao_raster_blend, zhao_raster_fragment, zhao_raster_tilestore,
// zhao_raster_div255, zhao_raster_quant, zhao_raster_resolve.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_raster_tile_pipe).

module zhao_raster_tile_pipe (
  input  logic clk,
  input  logic rst_n,

  // ---- job in: one triangle × one 16×16 tile × one flat word ------------
  // Vertices are S 12.8 screen subpixels (spec/qformats.md §8), guard band
  // ±2048 px; the tile origin is the tile's top-left PIXEL.
  input  logic               job_valid_i,
  output logic               job_ready_o,
  input  logic signed [20:0] job_ax_i,
  input  logic signed [20:0] job_ay_i,
  input  logic signed [20:0] job_bx_i,
  input  logic signed [20:0] job_by_i,
  input  logic signed [20:0] job_cx_i,
  input  logic signed [20:0] job_cy_i,
  input  logic signed [11:0] job_tile_x_i,
  input  logic signed [11:0] job_tile_y_i,
  // `job_fill_word_i` is the FRAGMENT SOURCE, in RASTER.TILESTORE's own word
  // layout: [63:40] source RGB, [39:32] the constant effect tag, [31:8] the
  // fragment's depth, [7:0] the stencil reference (and the REPLACE value).
  // With `job_state_i == 0` — the plain opaque write — that word lands at
  // every covered pixel unaltered, which is exactly what this port meant
  // before RASTER.FRAGMENT existed.
  input  logic        [63:0] job_fill_word_i,
  input  logic        [63:0] job_clear_word_i,  // the tile's clear word
  input  logic        [15:0] job_tile_index_i,  // .zcap TILE_CRC tile_index
  // ---- ONE LIFECYCLE PER TILE, NOT PER TRIANGLE --------------------------
  // This block used to be "one job = one clear + one triangle + one resolve",
  // which is correct in isolation and wrong in a frame: a tile two triangles
  // both reference was rendered TWICE and the second job's clear erased the
  // first triangle. tests/render/render_pipe_directed.cpp measured exactly
  // that, and reports/RENDER_SEAM_FINDINGS.md recorded it as the owner's
  // decision.
  //
  // The decision was option 1 -- accumulate a tile's triangles in the store
  // before resolving -- and this pair of bits is the whole protocol:
  //
  //   job_first_i  clear the front bank before walking this triangle
  //   job_last_i   swap and resolve after it
  //
  // A caller that ties BOTH high gets exactly the old behaviour, which is why
  // every existing directed and random test still passes unchanged. GEOM.BINNER
  // drives them from its own drain cursor, which already knows a tile list's
  // first and last reference.
  //
  // ENFORCED-BY: tests/render/render_pipe_directed.cpp:main
  input  logic               job_first_i,
  input  logic               job_last_i,
  input  logic        [15:0] job_src_id_i,      // source_id passthrough

  // ---- the recipe, flat across the triangle ------------------------------
  // The 32-bit fragment state word (layout: zhao_raster_fragment.sv), the
  // vertex alpha, and the sampled texel TEXTURE.TMU would have produced.
  // There is no sampler in this composition and none is imitated: the texel
  // is a job field, exactly as it is a packet field in RASTER.FRAGMENT.
  input  logic        [31:0] job_state_i,
  input  logic        [7:0]  job_src_a_i,       // vertex alpha, unit8
  input  logic        [23:0] job_texel_rgb_i,
  input  logic        [7:0]  job_texel_a_i,
  input  logic        [7:0]  job_texel_idx_i,   // CLUT8 index; 0 is the masked one

  // ---- framebuffer words out: one beat per pixel, tile raster order ------
  output logic               fb_valid_o,
  input  logic               fb_ready_i,
  output logic        [15:0] fb_rgb565_o,       // video_rules.md §3 [15:11] R
  output logic        [7:0]  fb_tag_o,          // effect tag — never dithered
  output logic        [7:0]  fb_addr_o,         // {row[3:0], col[3:0]} in-tile
  output logic signed [11:0] fb_x_o,            // SURFACE pixel x of this beat
  output logic signed [11:0] fb_y_o,            // SURFACE pixel y of this beat
  output logic               fb_last_o,         // the 256th pixel of this tile
  output logic        [15:0] fb_src_id_o,

  // ---- per-tile completion (one pulse per resolved tile) ----------------
  output logic        [31:0] tile_crc_o,
  output logic        [15:0] tile_crc_index_o,
  output logic               tile_done_o,
  output logic        [8:0]  tile_cov_count_o,   // covered pixels, 0..256
  output logic               tile_degenerate_o,  // area == 0: culled

  // ---- observability ----------------------------------------------------
  output logic               front_bank_o,
  output logic        [31:0] tile_references_o,   // RASTER.TILESTORE's counter
  output logic        [31:0] resolved_tiles_o,    // RASTER.RESOLVE's counter
  output logic        [31:0] early_z_rejects_o,   // RASTER.EARLYZ's counter
  output logic        [31:0] ez_covered_o,        // RASTER.EARLYZ's covered_fragments
  output logic        [31:0] fr_covered_o,        // RASTER.FRAGMENT's covered_fragments
  output logic        [31:0] blended_fragments_o, // RASTER.FRAGMENT's counter
  output logic        [7:0]  bin_mask_o,          // the coarse depth-bin occupancy
  output logic        [23:0] z_floor_o,           // the conservative depth floor
  output logic               fragment_error_o     // RASTER.FRAGMENT's error pulse
);

  // ======================================================= raster stage ====
  // RS_IDLE  — accept a job.
  // RS_CLEAR — clear the FRONT bank, begin the tile in RASTER.EARLYZ, and
  //            launch the edge walk (all in the same cycle).
  // RS_WALK  — consume coverage beats, one FRAGMENT per set column.
  // RS_SWAP  — the last write has retired AND the fragment pipeline is empty:
  //            swap the banks and start the resolve, as soon as the resolve
  //            stage is free.
  localparam logic [1:0] RS_IDLE  = 2'd0;
  localparam logic [1:0] RS_CLEAR = 2'd1;
  localparam logic [1:0] RS_WALK  = 2'd2;
  localparam logic [1:0] RS_SWAP  = 2'd3;

  logic [1:0] rs_state;
  logic        job_first_r;  // this triangle begins its tile
  logic        job_last_r;   // this triangle ends its tile
  logic [8:0]  cov_acc_r;    // pixels covered by the TILE so far

  // the job, latched at acceptance
  logic signed [20:0] ax_r, ay_r, bx_r, by_r, cx_r, cy_r;
  logic signed [11:0] tile_x_r, tile_y_r;
  logic        [63:0] fill_r, clear_r;
  logic        [15:0] index_r, src_r;
  logic        [31:0] state_r;
  logic        [7:0]  src_a_r;
  logic        [23:0] texel_rgb_r;
  logic        [7:0]  texel_a_r, texel_idx_r;

  // the coverage beat being expanded into writes
  logic [3:0]  pend_row_r;
  logic [15:0] pend_mask_r;

  // the edge walk's per-job status, latched at its done pulse
  logic       ew_done_r;
  logic [8:0] ew_count_r;
  logic       ew_degen_r;

  // ------------------------------------------------- the resolve shadow ----
  // The finishing tile's origin and status. The raster stage is a whole tile
  // ahead by the time these are needed, so they are captured at the swap.
  logic [11:0] rz_tile_x_r, rz_tile_y_r;
  logic [8:0]  rz_count_r;
  logic        rz_degen_r;

  // ------------------------------------------------------- block wiring ----
  logic        ew_start, ew_ready;
  logic        cov_valid, cov_ready, cov_last;
  logic [3:0]  cov_row;
  logic [15:0] cov_mask, cov_src;
  logic        ew_done, ew_degen;
  logic [8:0]  ew_count;

  logic        ez_frag_valid, ez_frag_ready;
  logic        ez_cand_valid, ez_cand_ready;
  logic [7:0]  ez_cand_addr;
  logic [23:0] ez_cand_depth;
  logic [31:0] ez_cand_state;
  logic [15:0] ez_cand_src;
  logic [87:0] ez_cand_payload;
  logic [2:0]  ez_cand_bin;
  logic        ez_reject;
  logic [7:0]  ez_reject_addr;

  logic        fr_idle;

  logic        ts_clear, ts_clear_ready;
  logic        ts_wr, ts_wr_ready;
  logic [7:0]  ts_wr_addr;
  logic [63:0] ts_wr_data;
  logic        ts_rd, ts_rd_ready, ts_rd_valid;
  logic [7:0]  ts_rd_addr;
  logic [15:0] ts_rd_src_in;
  logic [63:0] ts_rd_data;
  logic [15:0] ts_rd_src;
  logic        ts_swap, ts_swap_ready;

  logic        tr_valid, tr_ready, tr_data_valid;
  logic [7:0]  tr_addr;
  logic [63:0] tr_data;

  logic        rz_start, rz_ready;

  // ------------------------------------------- lowest set coverage column --
  // Same idiom as zhao_raster_edgewalk's `drain_row`: the descending loop
  // leaves the SMALLEST set index in place, so columns are written left to
  // right and `ts_wr_addr` is a plain {row, col} concatenation.
  logic [3:0]  wr_col;
  logic [15:0] wr_hot;
  always_comb begin
    wr_col = 4'd0;
    wr_hot = 16'd0;
    for (int i = 15; i >= 0; i--) begin
      if (pend_mask_r[i]) begin
        wr_col = i[3:0];
        wr_hot = 16'd1 << i;
      end
    end
  end

  // ---------------------------------------------------------- handshakes ---
  // Hygiene: no `valid` here is a function of its own channel's `ready`.
  // `ts_swap` does depend on the RESOLVE's `start_ready_o` — a DIFFERENT
  // channel's ready, the permitted direction — because the two must be
  // accepted on the same edge (law 1 above); RASTER.TILESTORE's `swap_ready_o`
  // is a constant 1, so that composition is loop-free.
  assign job_ready_o   = (rs_state == RS_IDLE);
  // RS_CLEAR DOES THREE THINGS AND ONLY ONE OF THEM BELONGS TO THE TILE:
  // it launches the EDGE WALK, it begins the tile in RASTER.EARLYZ, and it
  // clears the front bank. The walk is per TRIANGLE and must happen for every
  // job; the clear and the early-Z floor are per TILE and must happen once.
  //
  // Skipping the whole state for a non-first triangle skipped the WALK too, so
  // the pipe accepted nine jobs and then stalled forever waiting for coverage
  // that was never going to arrive. The state always runs; the two tile-scoped
  // actions are what `job_first_r` gates.
  assign ew_start      = (rs_state == RS_CLEAR);
  assign ts_clear      = (rs_state == RS_CLEAR) && job_first_r;
  assign cov_ready     = (rs_state == RS_WALK) && (pend_mask_r == 16'd0);
  assign ez_frag_valid = (rs_state == RS_WALK) && (pend_mask_r != 16'd0);
  assign rz_start      = (rs_state == RS_SWAP);
  assign ts_swap       = (rs_state == RS_SWAP) && rz_ready;

  logic cov_acc, frag_acc, swap_acc;
  assign cov_acc  = cov_valid && cov_ready;
  assign frag_acc = ez_frag_valid && ez_frag_ready;
  assign swap_acc = ts_swap && ts_swap_ready;

  // ------------------------------------------------- the fragment packet ---
  // Law 2: the job's flat source, replicated at every covered pixel. The
  // `fill_r` word is decoded HERE and nowhere else in this file, in
  // RASTER.TILESTORE's layout: [63:40] RGB, [39:32] tag, [31:8] depth,
  // [7:0] stencil. The payload RASTER.EARLYZ carries opaquely is packed in
  // the order zhao_raster_fragment's ports take it back out.
  logic [7:0]  frag_addr;
  logic [23:0] frag_depth;
  logic [87:0] frag_payload;
  assign frag_addr  = {pend_row_r, wr_col};
  assign frag_depth = fill_r[31:8];
  assign frag_payload = {fill_r[63:40],  // vertex RGB
                         src_a_r,        // vertex alpha
                         fill_r[39:32],  // the constant effect tag
                         fill_r[7:0],    // the stencil reference
                         texel_rgb_r, texel_a_r, texel_idx_r};

  // The pipeline is EMPTY when nothing stands in either block. The swap gate
  // needs this: `pend_mask_r == 0` now only means the last FRAGMENT was
  // handed over, not that its write has retired.
  logic pipe_empty;
  assign pipe_empty = !ez_cand_valid && fr_idle;

  // ------------------------------------------------- the surface address ---
  // Law 3: the resolving tile's origin plus the in-tile {row, col}. Signed
  // 12-bit pixel space, wrapping exactly as the job's own coordinates do.
  logic [11:0] fb_x_raw, fb_y_raw;
  always_comb begin
    fb_x_raw = rz_tile_x_r + {8'd0, fb_addr_o[3:0]};
    fb_y_raw = rz_tile_y_r + {8'd0, fb_addr_o[7:4]};
  end
  assign fb_x_o = $signed(fb_x_raw);
  assign fb_y_o = $signed(fb_y_raw);

  assign tile_cov_count_o  = rz_count_r;
  assign tile_degenerate_o = rz_degen_r;

  // -------------------------------------------------- unused block ports ---
  // RASTER.TILESTORE's read port A is now RASTER.FRAGMENT's working view and
  // is wired; what remains unused here is the store's echo of the read
  // source_id (RASTER.FRAGMENT drives it and ignores the echo, which is the
  // passthrough working as specified), the store's always-1 readies, the edge
  // walk's `cov_last` and its source-id echo (this block carries the job's own
  // `src_r`, which is the same value), and three of RASTER.EARLYZ's outputs —
  // the per-reject PULSE and ADDRESS and the per-fragment coarse BIN. The
  // pulse's durable form, `early_z_rejects_o`, IS wired out and is what the
  // tests count; the address is observability for a consumer this composition
  // does not have; and the bin belongs to the tile scheduler, which is
  // likewise not built (the per-tile `bin_mask_o` it would actually schedule
  // from is wired). All are sunk explicitly rather than deleted from the
  // instantiation: publishing a port and then dropping it is honest, and
  // quietly not wiring a contract channel is not.
  logic unused_ok;
  assign unused_ok = &{1'b0, ts_clear_ready, ts_rd_ready, ts_rd_src,
                       ts_swap_ready, cov_last, cov_src, ew_ready,
                       ez_reject, ez_reject_addr, ez_cand_bin};

  // ============================================================ EDGEWALK ===
  zhao_raster_edgewalk u_edgewalk (
    .clk              (clk),
    .rst_n            (rst_n),
    .job_valid_i      (ew_start),
    .job_ready_o      (ew_ready),
    .job_ax_i         (ax_r),
    .job_ay_i         (ay_r),
    .job_bx_i         (bx_r),
    .job_by_i         (by_r),
    .job_cx_i         (cx_r),
    .job_cy_i         (cy_r),
    .job_tile_x_i     (tile_x_r),
    .job_tile_y_i     (tile_y_r),
    .job_src_id_i     (src_r),
    .cov_valid_o      (cov_valid),
    .cov_ready_i      (cov_ready),
    .cov_row_o        (cov_row),
    .cov_mask_o       (cov_mask),
    .cov_last_o       (cov_last),
    .cov_src_id_o     (cov_src),
    .job_done_o       (ew_done),
    .job_degenerate_o (ew_degen),
    .cov_count_o      (ew_count)
  );

  // ============================================================== EARLYZ ===
  // The conservative reject, and the only block here that sees a fragment
  // before the tile store does. Its `tile_begin` rides the SAME cycle as the
  // store's clear: both describe the same event — "this tile now holds the
  // clear word everywhere" — and the clear word's depth field is what makes
  // the initial floor exact.
  zhao_raster_earlyz #(.PAYLOAD_W(88)) u_earlyz (
    .clk                 (clk),
    .rst_n               (rst_n),
    .tile_begin_i        (ts_clear),
    .tile_clear_depth_i  (clear_r[31:8]),
    .frag_valid_i        (ez_frag_valid),
    .frag_ready_o        (ez_frag_ready),
    .frag_addr_i         (frag_addr),
    .frag_depth_i        (frag_depth),
    .frag_state_i        (state_r),
    .frag_src_id_i       (src_r),
    .frag_payload_i      (frag_payload),
    .cand_valid_o        (ez_cand_valid),
    .cand_ready_i        (ez_cand_ready),
    .cand_addr_o         (ez_cand_addr),
    .cand_depth_o        (ez_cand_depth),
    .cand_state_o        (ez_cand_state),
    .cand_src_id_o       (ez_cand_src),
    .cand_payload_o      (ez_cand_payload),
    .cand_bin_o          (ez_cand_bin),
    .z_reject_o          (ez_reject),
    .z_reject_addr_o     (ez_reject_addr),
    .bin_mask_o          (bin_mask_o),
    .z_floor_o           (z_floor_o),
    .early_z_rejects_o   (early_z_rejects_o),
    .covered_fragments_o (ez_covered_o)
  );

  // ============================================================ FRAGMENT ===
  // The read-modify-write on the tile store's port A. The payload is unpacked
  // here in exactly the order it was packed above.
  zhao_raster_fragment u_fragment (
    .clk                 (clk),
    .rst_n               (rst_n),
    .frag_valid_i        (ez_cand_valid),
    .frag_ready_o        (ez_cand_ready),
    .frag_addr_i         (ez_cand_addr),
    .frag_depth_i        (ez_cand_depth),
    .frag_state_i        (ez_cand_state),
    .frag_src_id_i       (ez_cand_src),
    .frag_vert_rgb_i     (ez_cand_payload[87:64]),
    .frag_vert_a_i       (ez_cand_payload[63:56]),
    .frag_tag_i          (ez_cand_payload[55:48]),
    .frag_sten_ref_i     (ez_cand_payload[47:40]),
    .frag_texel_rgb_i    (ez_cand_payload[39:16]),
    .frag_texel_a_i      (ez_cand_payload[15:8]),
    .frag_texel_idx_i    (ez_cand_payload[7:0]),
    .rd_valid_o          (ts_rd),
    .rd_ready_i          (ts_rd_ready),
    .rd_addr_o           (ts_rd_addr),
    .rd_src_id_o         (ts_rd_src_in),
    .rd_valid_i          (ts_rd_valid),
    .rd_data_i           (ts_rd_data),
    .wr_valid_o          (ts_wr),
    .wr_ready_i          (ts_wr_ready),
    .wr_addr_o           (ts_wr_addr),
    .wr_data_o           (ts_wr_data),
    .fragment_error_o    (fragment_error_o),
    .idle_o              (fr_idle),
    .covered_fragments_o (fr_covered_o),
    .blended_fragments_o (blended_fragments_o)
  );

  // =========================================================== TILESTORE ===
  zhao_raster_tilestore u_tilestore (
    .clk               (clk),
    .rst_n             (rst_n),
    .clear_valid_i     (ts_clear),
    .clear_ready_o     (ts_clear_ready),
    .clear_data_i      (clear_r),
    .wr_valid_i        (ts_wr),
    .wr_ready_o        (ts_wr_ready),
    .wr_addr_i         (ts_wr_addr),
    .wr_data_i         (ts_wr_data),
    // read port A — RASTER.FRAGMENT's working view, port for port
    .rd_valid_i        (ts_rd),
    .rd_ready_o        (ts_rd_ready),
    .rd_addr_i         (ts_rd_addr),
    .rd_src_id_i       (ts_rd_src_in),
    .rd_valid_o        (ts_rd_valid),
    .rd_data_o         (ts_rd_data),
    .rd_src_id_o       (ts_rd_src),
    // read port B — RASTER.RESOLVE's `tr_*` master, port for port
    .res_valid_i       (tr_valid),
    .res_ready_o       (tr_ready),
    .res_addr_i        (tr_addr),
    .res_valid_o       (tr_data_valid),
    .res_data_o        (tr_data),
    .swap_valid_i      (ts_swap),
    .swap_ready_o      (ts_swap_ready),
    .front_bank_o      (front_bank_o),
    .tile_references_o (tile_references_o)
  );

  // ============================================================= RESOLVE ===
  // The `tr_*` / `res_*` pairing RASTER.RESOLVE.md documents: this master's
  // request channel is the store's back-bank read port, and `tr_data_valid_i`
  // is that port's fixed 1-cycle response — exactly one per accepted request.
  zhao_raster_resolve u_resolve (
    .clk                (clk),
    .rst_n              (rst_n),
    .start_valid_i      (rz_start),
    .start_ready_o      (rz_ready),
    .start_tile_x_i     (tile_x_r),
    .start_tile_y_i     (tile_y_r),
    .start_tile_index_i (index_r),
    .start_src_id_i     (src_r),
    .tr_valid_o         (tr_valid),
    .tr_ready_i         (tr_ready),
    .tr_addr_o          (tr_addr),
    .tr_data_valid_i    (tr_data_valid),
    .tr_data_i          (tr_data),
    .fb_valid_o         (fb_valid_o),
    .fb_ready_i         (fb_ready_i),
    .fb_rgb565_o        (fb_rgb565_o),
    .fb_tag_o           (fb_tag_o),
    .fb_addr_o          (fb_addr_o),
    .fb_last_o          (fb_last_o),
    .fb_src_id_o        (fb_src_id_o),
    .tile_crc_o         (tile_crc_o),
    .tile_crc_index_o   (tile_crc_index_o),
    .tile_crc_valid_o   (tile_done_o),
    .tile_references_o  (resolved_tiles_o)
  );

  // ============================================================ sequential =
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rs_state    <= RS_IDLE;
      ax_r        <= 21'sd0;
      ay_r        <= 21'sd0;
      bx_r        <= 21'sd0;
      by_r        <= 21'sd0;
      cx_r        <= 21'sd0;
      cy_r        <= 21'sd0;
      tile_x_r    <= 12'sd0;
      tile_y_r    <= 12'sd0;
      fill_r      <= 64'd0;
      clear_r     <= 64'd0;
      index_r     <= 16'd0;
      src_r       <= 16'd0;
      state_r     <= 32'd0;
      src_a_r     <= 8'd0;
      texel_rgb_r <= 24'd0;
      texel_a_r   <= 8'd0;
      texel_idx_r <= 8'd0;
      pend_row_r  <= 4'd0;
      pend_mask_r <= 16'd0;
      ew_done_r   <= 1'b0;
      ew_count_r  <= 9'd0;
      job_first_r <= 1'b1;
      job_last_r  <= 1'b1;
      cov_acc_r   <= 9'd0;
      ew_degen_r  <= 1'b0;
      rz_tile_x_r <= 12'd0;
      rz_tile_y_r <= 12'd0;
      rz_count_r  <= 9'd0;
      rz_degen_r  <= 1'b0;
    end else begin
      // The edge walk's status pulse can land in any RS_WALK cycle (or, for a
      // degenerate triangle, before the first coverage beat would have been);
      // it is latched here and consumed at the swap.
      if (ew_done) begin
        ew_done_r  <= 1'b1;
        ew_count_r <= ew_count;
        ew_degen_r <= ew_degen;
      end

      case (rs_state)
        RS_IDLE: begin
          if (job_valid_i) begin
            ax_r        <= job_ax_i;
            ay_r        <= job_ay_i;
            bx_r        <= job_bx_i;
            by_r        <= job_by_i;
            cx_r        <= job_cx_i;
            cy_r        <= job_cy_i;
            tile_x_r    <= job_tile_x_i;
            tile_y_r    <= job_tile_y_i;
            fill_r      <= job_fill_word_i;
            clear_r     <= job_clear_word_i;
            index_r     <= job_tile_index_i;
            src_r       <= job_src_id_i;
            state_r     <= job_state_i;
            src_a_r     <= job_src_a_i;
            texel_rgb_r <= job_texel_rgb_i;
            texel_a_r   <= job_texel_a_i;
            texel_idx_r <= job_texel_idx_i;
            pend_mask_r <= 16'd0;
            ew_done_r   <= 1'b0;
            ew_count_r  <= 9'd0;
            ew_degen_r  <= 1'b0;
            job_first_r <= job_first_i;
            job_last_r  <= job_last_i;
            // The clear belongs to the TILE, so only the first triangle of a
            // tile pays for it. A later triangle walks straight into the bank
            // its predecessors already wrote, which is what makes them
            // compose instead of overwrite.
            rs_state    <= RS_CLEAR;
            // Coverage accumulates across the tile: the resolve reports how
            // many pixels the TILE covered, not the last triangle.
            if (job_first_i) cov_acc_r <= 9'd0;
          end
        end

        // The clear and the edge-walk job are accepted on the same edge. The
        // walk needs 5 setup cycles before its first coverage beat, so the
        // one-cycle clear is always long retired before the first write.
        RS_CLEAR: begin
          if (ew_ready) rs_state <= RS_WALK;
        end

        RS_WALK: begin
          // Law 2: one FRAGMENT per set column, lowest first; the next
          // coverage beat is accepted only once the current mask has drained.
          if (frag_acc) pend_mask_r <= pend_mask_r & ~wr_hot;
          if (cov_acc) begin
            pend_row_r  <= cov_row;
            pend_mask_r <= cov_mask;
          end
          // Law 1, the raster half of the swap gate — and the ONE term the
          // arrival of RASTER.EARLYZ and RASTER.FRAGMENT added. The walk has
          // reported done, the last fragment has been handed over, AND the
          // fragment pipeline has drained: `pend_mask_r == 0` now only means
          // the last fragment ENTERED the chain, and swapping while a write
          // is still two stages away would send that write into the bank the
          // resolve is about to read. `pipe_empty` closes exactly that gap.
          // ENFORCED-BY: tests/raster/raster_tile_pipe_directed.cpp:
          //   test_pipeline_drain_before_swap
          // ...and now the LAST triangle of the tile swaps and resolves; an
          // earlier one returns to IDLE so the next reference for the same
          // tile can be accepted into the same bank.
          if (ew_done_r && (pend_mask_r == 16'd0) && !cov_acc && pipe_empty) begin
            cov_acc_r <= cov_acc_r + ew_count_r;
            rs_state  <= job_last_r ? RS_SWAP : RS_IDLE;
          end
        end

        RS_SWAP: begin
          // Law 1, the resolve half: the bank cannot be handed over until the
          // previous tile has finished streaming out of it.
          if (swap_acc) begin
            rz_tile_x_r <= tile_x_r;
            rz_tile_y_r <= tile_y_r;
            rz_count_r  <= cov_acc_r;
            rz_degen_r  <= ew_degen_r;
            rs_state    <= RS_IDLE;
          end
        end

        default: rs_state <= RS_IDLE;
      endcase
    end
  end

endmodule : zhao_raster_tile_pipe
