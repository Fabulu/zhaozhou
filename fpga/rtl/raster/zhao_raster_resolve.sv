// zhao_raster_resolve.sv — RASTER.RESOLVE: the ordered-dither RGB565 resolve
// of one finished 16×16 tile, plus the tile-CRC handoff (phase 4, ZH-024).
//
// Law (in citation order):
//   design/contracts/RASTER.RESOLVE.md — the block contract (packet layouts,
//       backpressure, the CRC byte order, the latency bound).
//   reference/src/zrender/resolve.cpp — THE dither oracle. Its header states
//       the law this module reproduces bit for bit:
//         r5 = min(31, (r*31 + (B*16 + 8)) / 255)
//         g6 = min(63, (g*63 + (B*32 + 16)) / 255)
//         b5 = min(31, (b*31 + (B*16 + 8)) / 255)
//       with B the 4×4 Bayer value at (y & 3, x & 3) — the threshold
//       t = (B + 0.5)/16 of one quantization step. GREEN IS DIFFERENT: its
//       dither amplitude is 32, not 16, and its rounding term 16, not 8,
//       because RGB565 gives green SIX bits. Getting that wrong is the
//       classic subtle resolve defect and it is caught here by
//       test_green_amplitude and by every random tile.
//       The `min` clamps are the 2026-08-16 white-rail fix, also from that
//       header: green at B ≥ 8 with g ≥ 252 quantizes to 64, which WRAPS in
//       a 6-bit field, and full white resolved to a white/magenta pixel
//       checkerboard. Clamped here exactly as there.
//   plan W3.5 (quoted in resolve.cpp's header): **fixgen has NO dither
//       table.** The canonical 4×4 Bayer matrix is defined in resolve.cpp,
//       once, and this module's `bayer4` is that matrix and nothing else.
//       There is no generated table to include and none is invented here;
//       design/blocks.yml's note to the contrary is stale (see the contract).
//   spec/video_rules.md §3 — framebuffer RGB565 [15:11] R, [10:5] G, [4:0] B,
//       LITTLE-ENDIAN halfwords, row-major, no row padding. The tile CRC
//       walks the resolved bytes in that order: low byte, then high byte.
//   spec/capture_format.md §2 — CRC-32C (poly 0x82F63B78 reflected, init and
//       xorout 0xFFFFFFFF) via the GENERATED zhao_crc32c_step. One
//       polynomial machine-wide (plan A3d): this block mints no CRC variant,
//       it calls the same function zhao_debug_crc and zhao_cmd_dma call.
//       §4.2 TILE_CRC is `{u32 tile_index; u32 crc32c}` — hence the index
//       travelling beside the CRC.
//   spec/stars_and_flares.md §1 — "ordered dither applies to RGB565 only —
//       THE TAG BYTE IS NEVER DITHERED". The effect tag rides out of this
//       block untouched, on its own field, and is NOT part of the CRC
//       (the CRC covers framebuffer bytes, and the tag never reaches VRAM).
//
// WHAT THIS BLOCK IS NOT: no VRAM addressing and no framebuffer write (it
// emits a pixel stream; MEM.GUARD and the write path own the address), no
// depth/stencil resolve (charter §8: "no external full-screen depth buffer in
// the normal tile path"; capture_format's DEPTH_STENCIL_CRC is optional and
// not built), no scanout, no frame-level CRC (that is DEBUG.CRC over the
// DISPLAYED stream), no tile scheduling, no post effects.
//
// ---------------------------------------------------------------------------
// THE DIVISION
// ---------------------------------------------------------------------------
// The oracle divides by 255 with C++ integer division on a non-negative
// numerator, i.e. floor. zhao_raster_div255 computes that exactly with
// (n + (n>>8) + 1) >> 8 — an identity, not an approximation. The numerator,
// that division and the min() rail live together in zhao_raster_quant, one
// instance per channel, so tests/formal/raster_resolve_quant.sby can prove
// the SHIPPING quantizer (both parameter sets, all 4,096 (v, B) inputs)
// exactly equal to min(MAXQ, floor(num/255)) and incapable of exceeding its
// field. The white rail is a theorem here, not a comment.
//
// ---------------------------------------------------------------------------
// THE BAYER PHASE IS ABSOLUTE, NOT TILE-LOCAL
// ---------------------------------------------------------------------------
// resolve.cpp indexes the matrix by the pixel's position in the SURFACE.
// A tile whose top-left pixel is (tile_x, tile_y) therefore uses
//     B = bayer4[(tile_y + row) & 3][(tile_x + col) & 3]
// and NOT bayer4[row & 3][col & 3]. For the 16-aligned tile grid the two
// agree (16 ≡ 0 mod 4), which is exactly why a tile-local phase would pass a
// careless test and then break the moment anything resolves at an unaligned
// origin. Only the low two bits of the origin are used, so the sign of a
// negative origin is irrelevant (two's complement).
//
// ---------------------------------------------------------------------------
// TIMING
// ---------------------------------------------------------------------------
// The block masters the tile-store back-bank read port (RASTER.TILESTORE's
// `res_*`, fixed 1-cycle latency) and pushes into a 2-deep skid FIFO, so it
// sustains ONE resolved pixel per clock when the consumer is ready and never
// drops an in-flight read when it is not. The credit rule is a single
// occupancy counter: a read is issued only while `occ - pop < 2`, where occ
// counts reads issued but not yet emitted. Since a response converts an
// outstanding read into a FIFO entry (net zero), occ ≤ 2 always and the FIFO
// can never overflow.
//
// Latency is VARIABLE (ledger): 258 cycles for one tile at full readiness
// (256 pixels + the pipeline fill + the finalize cycle), unbounded above
// under backpressure on either side.
//
// Conservative SystemVerilog subset only (charter §2). Depends on
// zhao_abi_pkg (the generated CRC step) and zhao_raster_div255.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_raster_resolve).

module zhao_raster_resolve (
  input  logic clk,
  input  logic rst_n,

  // ---- start: one finished tile (the `resolved_tile_trigger`) -----------
  input  logic               start_valid_i,
  output logic               start_ready_o,
  input  logic signed [11:0] start_tile_x_i,      // tile origin, PIXELS
  input  logic signed [11:0] start_tile_y_i,
  input  logic        [15:0] start_tile_index_i,  // TILE_CRC tile_index
  input  logic        [15:0] start_src_id_i,      // source_id passthrough

  // ---- tile_read master: RASTER.TILESTORE's back-bank port --------------
  output logic        tr_valid_o,
  input  logic        tr_ready_i,
  output logic [7:0]  tr_addr_o,        // {row[3:0], col[3:0]}
  input  logic        tr_data_valid_i,  // one response per accepted request
  input  logic [63:0] tr_data_i,        // the tile word (TILESTORE layout)

  // ---- fb_tiles: the resolved pixel stream, tile raster order -----------
  output logic        fb_valid_o,
  input  logic        fb_ready_i,
  output logic [15:0] fb_rgb565_o,
  output logic [7:0]  fb_tag_o,         // effect tag — NEVER dithered
  output logic [7:0]  fb_addr_o,        // {row[3:0], col[3:0]} within the tile
  output logic        fb_last_o,        // the 256th pixel of this tile
  output logic [15:0] fb_src_id_o,

  // ---- tile_crc: the DEBUG.CRC / .zcap TILE_CRC handoff ------------------
  output logic [31:0] tile_crc_o,
  output logic [15:0] tile_crc_index_o,
  output logic        tile_crc_valid_o, // one-cycle pulse, tile complete

  // ---- counters ---------------------------------------------------------
  output logic [31:0] tile_references_o
);

  localparam logic [31:0] REF_MAX = 32'hFFFF_FFFF;
  localparam logic [31:0] CRC_INIT = 32'hFFFF_FFFF;

  // ------------------------------------------------- the tile-store word ---
  // Layout owned by zhao_raster_tilestore (charter §8, MSB first):
  //   [63:40] RGB colour   [39:32] effect tag   [31:8] depth   [7:0] stencil
  // Only colour and tag are resolved; depth and stencil are dropped here by
  // design (charter §8: no external full-screen depth buffer).
  logic [7:0] px_r, px_g, px_b, px_tag;
  // Depth [31:8] and stencil [7:0] are deliberately NOT resolved (charter §8:
  // no external full-screen depth buffer in the normal tile path), and only
  // the low two bits of the tile origin can affect the Bayer phase. Both are
  // sunk explicitly rather than left to a lint waiver.
  logic unused_ok;
  assign unused_ok = &{1'b0, tr_data_i[31:0], start_tile_x_i[11:2], start_tile_y_i[11:2]};

  always_comb begin
    px_r   = tr_data_i[63:56];
    px_g   = tr_data_i[55:48];
    px_b   = tr_data_i[47:40];
    px_tag = tr_data_i[39:32];
  end

  // ------------------------------------------------------------- job state --
  logic        busy_r;
  logic [1:0]  tile_xp_r, tile_yp_r;   // low 2 bits of the origin = the phase
  logic [15:0] index_r, src_r;
  logic [8:0]  iss_n;                  // reads issued,  0..256
  logic [7:0]  ret_addr;               // address of the NEXT response
  logic [8:0]  emit_n;                 // pixels accepted downstream, 0..256
  logic [1:0]  occ;                    // issued-but-not-emitted, 0..2
  logic [31:0] crc_r;
  logic        crc_v_r;
  logic [31:0] crc_out_r;
  logic [15:0] crc_idx_r;

  assign start_ready_o    = !busy_r;
  assign tile_crc_o       = crc_out_r;
  assign tile_crc_index_o = crc_idx_r;
  assign tile_crc_valid_o = crc_v_r;

  // -------------------------------------------------------- the 4×4 Bayer --
  // reference/src/zrender/resolve.cpp `kBayer4`, transcribed once. NOT a
  // generated table — fixgen has none (plan W3.5, quoted in that header).
  function automatic logic [3:0] bayer4(input logic [1:0] by, input logic [1:0] bx);
    case ({by, bx})
      4'b00_00: bayer4 = 4'd0;   4'b00_01: bayer4 = 4'd8;
      4'b00_10: bayer4 = 4'd2;   4'b00_11: bayer4 = 4'd10;
      4'b01_00: bayer4 = 4'd12;  4'b01_01: bayer4 = 4'd4;
      4'b01_10: bayer4 = 4'd14;  4'b01_11: bayer4 = 4'd6;
      4'b10_00: bayer4 = 4'd3;   4'b10_01: bayer4 = 4'd11;
      4'b10_10: bayer4 = 4'd1;   4'b10_11: bayer4 = 4'd9;
      4'b11_00: bayer4 = 4'd15;  4'b11_01: bayer4 = 4'd7;
      4'b11_10: bayer4 = 4'd13;  4'b11_11: bayer4 = 4'd5;
      default:  bayer4 = 4'd0;
    endcase
  endfunction

  // ------------------------------------------------------- the quantizer ---
  // The absolute Bayer phase of the pixel this response belongs to.
  logic [1:0] ph_y, ph_x;
  logic [3:0] bay;
  always_comb begin
    ph_y = tile_yp_r + ret_addr[5:4];   // (tile_y + row) & 3
    ph_x = tile_xp_r + ret_addr[1:0];   // (tile_x + col) & 3
    bay  = bayer4(ph_y, ph_x);
  end

  // One zhao_raster_quant per channel, with resolve.cpp's constants NAMED at
  // the instantiation: the 5-bit channels take (31, 16, 8) and green takes
  // (63, 32, 16). Nothing is derived here — green's doubled amplitude is the
  // oracle's stated constant, not a formula (see zhao_raster_quant.sv).
  logic [4:0] c_r5, c_b5;
  logic [5:0] c_g6;
  zhao_raster_quant #(.MAXQ(31), .QW(5), .AMP(16), .RND(8))
    u_qr (.v_i(px_r), .bayer_i(bay), .q_o(c_r5));
  zhao_raster_quant #(.MAXQ(63), .QW(6), .AMP(32), .RND(16))
    u_qg (.v_i(px_g), .bayer_i(bay), .q_o(c_g6));
  zhao_raster_quant #(.MAXQ(31), .QW(5), .AMP(16), .RND(8))
    u_qb (.v_i(px_b), .bayer_i(bay), .q_o(c_b5));

  // video_rules.md §3: [15:11] R, [10:5] G, [4:0] B.
  logic [15:0] px565;
  assign px565 = {c_r5, c_g6, c_b5};

  // ------------------------------------------------------ the skid FIFO ----
  // 2 entries × {last, addr[7:0], tag[7:0], rgb565[15:0]}.
  localparam int unsigned FW = 1 + 8 + 8 + 16;
  logic [FW-1:0] fifo_q [0:1];
  logic          wptr, rptr;
  logic [1:0]    fcount;

  logic push, pop;
  logic [FW-1:0] push_d;
  assign push   = tr_data_valid_i;
  assign push_d = {(ret_addr == 8'd255), ret_addr, px_tag, px565};

  assign fb_valid_o  = (fcount != 2'd0);
  assign fb_rgb565_o = fifo_q[rptr][15:0];
  assign fb_tag_o    = fifo_q[rptr][23:16];
  assign fb_addr_o   = fifo_q[rptr][31:24];
  assign fb_last_o   = fifo_q[rptr][32];
  assign fb_src_id_o = src_r;
  assign pop         = fb_valid_o && fb_ready_i;

  // --------------------------------------------------------- issue credit --
  // occ counts reads issued but not yet emitted (outstanding + FIFO). A
  // response converts one into the other, so occ moves only on issue (+1) and
  // on emit (−1). Issuing while `occ − pop < 2` therefore keeps occ ≤ 2 and
  // the 2-entry FIFO can never overflow. `tr_valid_o` is a function of
  // registers and `fb_ready_i` only — it never depends on `tr_ready_i`.
  logic issue_acc;
  logic [1:0] occ_free;
  always_comb begin
    occ_free   = occ - {1'b0, pop};
    tr_valid_o = busy_r && (iss_n != 9'd256) && (occ_free < 2'd2);
    tr_addr_o  = iss_n[7:0];
  end
  assign issue_acc = tr_valid_o && tr_ready_i;

  // ------------------------------------------------------------ the CRC ----
  // spec/video_rules.md §3 stores RGB565 as LITTLE-ENDIAN halfwords, so the
  // framebuffer byte order is low then high — two zhao_crc32c_step calls per
  // pixel, chained, on the beat the consumer ACCEPTS (exactly 256 pixels per
  // tile, in raster order, whatever the stall pattern). The generated step is
  // the one polynomial machine (plan A3d); nothing is re-derived here.
  logic [31:0] crc_next;
  always_comb begin
    crc_next = zhao_abi_pkg::zhao_crc32c_step(
                 zhao_abi_pkg::zhao_crc32c_step(crc_r, fb_rgb565_o[7:0]),
                 fb_rgb565_o[15:8]);
  end

  // --------------------------------------------------------- the counter ---
  // One `tile_references` increment per RESOLVED TILE (this block's share of
  // the catalog entry; see the contract — the catalog id and the frame_tick
  // shadow latch are not wired in this wave). Saturating, spec/counters.md §4.
  logic [31:0] refs_r;
  assign tile_references_o = refs_r;

  // ---------------------------------------------------------- sequential ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy_r    <= 1'b0;
      tile_xp_r <= 2'd0;
      tile_yp_r <= 2'd0;
      index_r   <= 16'd0;
      src_r     <= 16'd0;
      iss_n     <= 9'd0;
      ret_addr  <= 8'd0;
      emit_n    <= 9'd0;
      occ       <= 2'd0;
      fcount    <= 2'd0;
      wptr      <= 1'b0;
      rptr      <= 1'b0;
      fifo_q[0] <= {FW{1'b0}};
      fifo_q[1] <= {FW{1'b0}};
      crc_r     <= CRC_INIT;
      crc_v_r   <= 1'b0;
      crc_out_r <= 32'd0;
      crc_idx_r <= 16'd0;
      refs_r    <= 32'd0;
    end else begin
      crc_v_r <= 1'b0;

      // ---- start of a tile ------------------------------------------------
      if (!busy_r) begin
        if (start_valid_i) begin
          busy_r    <= 1'b1;
          tile_xp_r <= start_tile_x_i[1:0];
          tile_yp_r <= start_tile_y_i[1:0];
          index_r   <= start_tile_index_i;
          src_r     <= start_src_id_i;
          iss_n     <= 9'd0;
          ret_addr  <= 8'd0;
          emit_n    <= 9'd0;
          occ       <= 2'd0;
          fcount    <= 2'd0;
          wptr      <= 1'b0;
          rptr      <= 1'b0;
          crc_r     <= CRC_INIT;
        end
      end

      // ---- issue ----------------------------------------------------------
      if (issue_acc) iss_n <= iss_n + 9'd1;

      // ---- response: dither and push --------------------------------------
      if (push) begin
        fifo_q[wptr] <= push_d;
        wptr         <= !wptr;
        ret_addr     <= ret_addr + 8'd1;
      end

      // ---- emit: CRC and completion ----------------------------------------
      if (pop) begin
        rptr   <= !rptr;
        crc_r  <= crc_next;
        emit_n <= emit_n + 9'd1;
        if (emit_n == 9'd255) begin
          // the 256th pixel: finalize (xorout) and hand the tile off
          crc_out_r <= ~crc_next;
          crc_idx_r <= index_r;
          crc_v_r   <= 1'b1;
          busy_r    <= 1'b0;
          if (refs_r != REF_MAX) refs_r <= refs_r + 32'd1;
        end
      end

      // ---- occupancy / FIFO bookkeeping ------------------------------------
      case ({issue_acc, pop})
        2'b10:   occ <= occ + 2'd1;
        2'b01:   occ <= occ - 2'd1;
        default: ;
      endcase
      case ({push, pop})
        2'b10:   fcount <= fcount + 2'd1;
        2'b01:   fcount <= fcount - 2'd1;
        default: ;
      endcase
    end
  end

endmodule : zhao_raster_resolve
