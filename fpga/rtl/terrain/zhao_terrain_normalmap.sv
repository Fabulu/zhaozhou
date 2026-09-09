// zhao_terrain_normalmap.sv — TERRAIN.NORMALMAP: the per-fragment detail term.
//
// Rebuilt to contract 2026-09-09 (design/contracts/TERRAIN.NORMALMAP.md plus
// the dated amendment appended to it the same day). The DRAFT that lived in
// this file — serial divider, timing-matched fragments, ambient floor — is
// superseded exactly as reports/NORMALMAP-ARCHITECTURE.md required; its
// per-triangle engine's job belongs to TERRAIN.SHADE and is NOT here.
//
// ENFORCED-BY: tests/texture/terrain_normalmap_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHAT IT COMPUTES
// ---------------------------------------------------------------------------
//     dot(n/|n| + s*d, L)  =  dot(n, L)/|n|  +  s*dot(d, L)
//                             \___________/     \__________/
//                             TERRAIN.SHADE      THIS BLOCK
//
// One signed shade delta per fragment:
//
//     delta = sat_s9( rescale_s( strength * SUM_suns(dx*sun_x + dz*sun_z),
//                                DELTA_SHIFT ) )
//
// dx,dz are s8 (value raw/128) from the block's own resident detail tile,
// sun_x/sun_z are s1.15 (value raw/32768), strength is u8 (value raw/256),
// and the delta's value is raw/256. The scale algebra: 7+15+8 fraction bits
// in, 8 out, so DELTA_SHIFT = 22. The contract's original text said 23 —
// that is the s1.15-assumption factor-of-two the ratified oracle header
// already documents ("getting that wrong is a factor of two in the relief",
// reference/include/zref/zref_terrain_normalmap.hpp); the oracle side was
// corrected on 2026-09-03 and the contract number is corrected by the
// 2026-09-09 amendment. zref::terrain::normalmap_delta_s9 is the law.
//
// ---------------------------------------------------------------------------
// ZERO DSP, BY THE RESCUE'S OWN RULE
// ---------------------------------------------------------------------------
// ZHAOZHOU_MEMORY_FIRST_RESOURCE_RESCUE_2026-09-09.txt §14.3: "For a fixed
// per-epoch light coefficient, byte tables offer exact products." The sun and
// strength change per EPOCH (a cfg write, at most once a frame); dx and dz
// are BYTES. So the per-fragment products live in two 256-entry tables:
//
//     tblx[dx] = dx * Kx      Kx = strength * SUM_suns(sun_x)   (exact, s33)
//     tblz[dz] = dz * Kz      Kz = strength * SUM_suns(sun_z)
//
// and the fragment path is two RAM reads, one add, ONE rounding, one clamp.
// No multiplier exists anywhere in this module — not per fragment, and not
// per epoch either: Kx/Kz are built by an 8-step shift-add over strength's
// bits, and the tables are filled by pure accumulation (tbl[d+1] = tbl[d]+K),
// 256 adds. The sum-over-suns folds into K by distributivity BEFORE any
// rounding, so SUNS=2 is bit-exact against the per-sun-accumulate law and
// costs nothing per fragment.
//
// Every product is exact; the single rescale_s at the end is the block's ONE
// rounding (spec/qformats.md §3).
//
// ---------------------------------------------------------------------------
// THE DETAIL TILE, WITH ITS MIP TAIL
// ---------------------------------------------------------------------------
// One always-resident tile in M10K, 64x64 {s8 dz, s8 dx} texels at level 0,
// plus the seven-level pyramid tail the mipmapping addendum requires
// (reports/zhaozhou-terrain-mipmapping-architecture-2026-09-05.txt §4):
// levels 64,32,16,8,4,2,1 — 5,461 words. Detail bump that never coarsens
// aliases into shimmer exactly where the owner will look, so the level is
// selected per fragment from f_lod_i (an unsigned INTEGER level, the
// addendum's preferred contract) plus a signed authored bias, clamped to an
// authored max_level. Reset state is max_level = 0: the block behaves
// bit-identically to the un-mipped contract until the knob is opened.
//
// The pyramid is built OFFLINE by averaging SIGNED dx/dz (never normalising —
// addendum §4); this block only addresses it:
//
//     level base words: 0, 4096, 5120, 5376, 5440, 5456, 5460
//     addr = base[L] + ((v6 >> L) << (6 - L)) + (u6 >> L)
//     u6   = (u_raw >> uv_shift)[5:0]        (wraps at the 64-texel seam)
//
// zref::terrain::normalmap_pyramid_addr is the same law in the oracle.
//
// ---------------------------------------------------------------------------
// STREAM DISCIPLINE
// ---------------------------------------------------------------------------
// Bump-in-wire on the fragment stream: ready/valid both sides, skid-buffered
// so f_ready_o is REGISTERED, fixed latency (LATENCY below), II = 1, strictly
// in input order. A stalled consumer stalls the whole pipe; nothing drops,
// nothing reorders. f_detail_i = 0 forces delta 0 with the tile read enable
// held low (counted in zeroed_o).
//
// COLD is a state, not an error: after reset, and from any cfg write to sun
// or strength until the table refill completes (~270 cycles), table_ready_o
// is low and every live fragment emits delta 0, counted in cold_o. That is
// what makes "strength register = 0 at reset -> bit-exact off" hold without
// resetting a RAM (an async-reset payload array cannot be an M10K —
// rescue §4.1, and the texture cache's scar). Fragments already in flight
// when a cfg write lands are forced cold too: they must not read a table
// that is being rewritten under them.
//
// Mixed-port read-during-write on the K tables is declared DON'T-CARE: it can
// only occur while table_ready_o is low, and every such fragment is forced to
// delta 0 before the read result is used.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK REFUSES (contract, unchanged)
// ---------------------------------------------------------------------------
// No TMU, no texture cache, no TEXJOIN/fragrob sample slots. No per-triangle
// base term. No application of the delta (the seam adds it to the colour
// lanes downstream). No Y component. And it is NOT precedent for general
// tangent-space normal maps — D-8, ruled 2026-09-03, stands.
`default_nettype none

module zhao_terrain_normalmap #(
    // Suns summed into K before the single rounding. The environment record
    // carries ONE sun today; 2 is a parameter plus a spec amendment.
    parameter int unsigned SUNS = 1,
    // Pyramid depth: 1 = the bare 64x64 contract tile (4,096 words, no mip),
    // 7 = the full tail down to 1x1 (5,461 words). A knob, not a derivation.
    parameter int unsigned LEVELS = 7,
    // The one rounding: delta_raw = (strength*d*sun) >> DELTA_SHIFT,
    // round-half-up. 22 is the law (see header); a knob so a deliberate
    // global relief rescale stays an edit, not a rebuild.
    parameter int unsigned DELTA_SHIFT = 22,
    // Width of the fragment's integer mip level input.
    parameter int unsigned LODW = 4
) (
    input var logic clk,
    input var logic rst_n,

    // ---- config (zhao_terrain_project cfg idiom, one word at a time) ------
    //   0: {sun_z0[31:16], sun_x0[15:0]}  s1.15, FROM surface TOWARD light
    //   1: {sun_z1[31:16], sun_x1[15:0]}  read only when SUNS >= 2
    //   2: strength[7:0]                  u0.8; 0 = path is bit-exact off
    //   3: uv_shift[3:0]                  texel index = u_raw[uv_shift+5:uv_shift]
    //   4: {lod_bias[8:4] (s5), max_level[2:0]}   reset 0 = mip disabled
    // A write to 0..2 drops table_ready_o and starts the ~270-cycle refill.
    input var logic        cfg_we_i,
    input var logic [2:0]  cfg_addr_i,
    input var logic [31:0] cfg_data_i,

    // ---- fragment in: tapped from the stream that feeds the texture path --
    input  var logic               f_valid_i,
    output var logic               f_ready_o,
    input  var logic signed [31:0] f_u_i,       // perspective-correct terrain U, S15.16
    input  var logic signed [31:0] f_v_i,       // perspective-correct terrain V, S15.16
    input  var logic               f_detail_i,  // 0 = force delta 0, no tile read
    input  var logic [LODW-1:0]    f_lod_i,     // unsigned INTEGER mip level
    input  var logic [15:0]        f_src_id_i,

    // ---- delta out: in input order, fixed latency, II = 1 -----------------
    output var logic               d_valid_o,
    input  var logic               d_ready_i,
    output var logic signed [8:0]  d_delta_o,   // value = raw/256
    output var logic [15:0]        d_src_id_o,

    // ---- tile upload (write-only, CPU/CMD path) ---------------------------
    // Flat word address into the pyramid layout above. Writes at or beyond
    // PYR_WORDS are ignored (an upload-tool fault, not a machine state).
    input var logic        tw_we_i,
    input var logic [12:0] tw_addr_i,
    input var logic [15:0] tw_data_i,   // {s8 dz, s8 dx}

    // ---- evidence ---------------------------------------------------------
    output var logic [31:0] fragments_o,   // accepted fragments
    output var logic [31:0] zeroed_o,      // f_detail_i was low
    output var logic [31:0] railed_o,      // delta saturated at s9
    output var logic [31:0] cold_o,        // live fragment met unready tables
    output var logic        table_ready_o,
    output var logic        idle_o
);

  // -------------------------------------------------------------------------
  // Elaboration guards. Quartus 17.0 requires these inside `initial begin`
  // (a bare module-scope `if` is a syntax error there), and `--lint-only`
  // does NOT run them — the directed test elaborates the module, which does.
  // -------------------------------------------------------------------------
  initial begin
    if (SUNS < 1 || SUNS > 2)
      $fatal(1, "zhao_terrain_normalmap: SUNS must be 1 or 2, got %0d", SUNS);
    if (LEVELS < 1 || LEVELS > 7)
      $fatal(1, "zhao_terrain_normalmap: LEVELS must be 1..7, got %0d", LEVELS);
    if (DELTA_SHIFT < 8 || DELTA_SHIFT > 30)
      $fatal(1, "zhao_terrain_normalmap: DELTA_SHIFT out of range: %0d", DELTA_SHIFT);
    if (LODW < 3 || LODW > 8)
      $fatal(1, "zhao_terrain_normalmap: LODW must be 3..8, got %0d", LODW);
  end

  // Pyramid size for the configured depth. Ternary chain rather than a
  // constant function: Quartus 17.0's elaborator is the reason.
  localparam int unsigned PYR_WORDS =
      (LEVELS == 1) ? 4096 :
      (LEVELS == 2) ? 5120 :
      (LEVELS == 3) ? 5376 :
      (LEVELS == 4) ? 5440 :
      (LEVELS == 5) ? 5456 :
      (LEVELS == 6) ? 5460 : 5461;

  // Fixed latency, acceptance to d_valid_o: P0 in-regs, P1 level/wrap,
  // P2 address, P3 tile read, P4 K-table reads, P5 sum/round/clamp out-regs.
  localparam int unsigned LATENCY = 6;
  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned LATENCY_DECLARED = LATENCY;  // for the contract row
  /* verilator lint_on UNUSEDPARAM */

  // =========================================================================
  // Config registers. Reset is the bit-exact-off state: strength 0, tables
  // not ready, mip closed (max_level 0).
  // =========================================================================
  logic signed [15:0] sun_x_q [2];
  logic signed [15:0] sun_z_q [2];
  logic        [7:0]  strength_q;
  logic        [3:0]  uv_shift_q;
  logic signed [4:0]  lod_bias_q;
  logic        [2:0]  max_level_q;

  logic cfg_epoch_we_c;
  assign cfg_epoch_we_c = cfg_we_i && (cfg_addr_i <= 3'd2);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sun_x_q[0]  <= '0;
      sun_z_q[0]  <= '0;
      sun_x_q[1]  <= '0;
      sun_z_q[1]  <= '0;
      strength_q  <= '0;
      uv_shift_q  <= '0;
      lod_bias_q  <= '0;
      max_level_q <= '0;
    end else if (cfg_we_i) begin
      case (cfg_addr_i)
        3'd0: begin
          sun_x_q[0] <= signed'(cfg_data_i[15:0]);
          sun_z_q[0] <= signed'(cfg_data_i[31:16]);
        end
        3'd1: begin
          sun_x_q[1] <= signed'(cfg_data_i[15:0]);
          sun_z_q[1] <= signed'(cfg_data_i[31:16]);
        end
        3'd2: strength_q <= cfg_data_i[7:0];
        3'd3: uv_shift_q <= cfg_data_i[3:0];
        3'd4: begin
          max_level_q <= cfg_data_i[2:0];
          lod_bias_q  <= signed'(cfg_data_i[8:4]);
        end
        default: ;
      endcase
    end
  end

  // =========================================================================
  // Epoch sequencer: K = strength * SUM_suns(sun), then 256 accumulate-writes
  // per table. No multiplier: K by shift-add over strength's 8 bits, the fill
  // by repeated addition from -128*K (a shift). All arithmetic exact.
  //
  // Widths, stated: sun sum s17 (two s16), K = |sum|*255 < 2^24 -> s25,
  // table entry = d*K with |d| <= 128 -> |entry| < 2^32 -> s33.
  // =========================================================================
  typedef enum logic [1:0] { E_IDLE, E_KMUL, E_KPREP, E_FILL } eseq_e;
  eseq_e eq_st_q;

  logic               dirty_q;
  logic               tbl_valid_q;
  logic signed [16:0] ssx_q, ssz_q;    // per-epoch sun sums
  logic        [2:0]  kbit_q;
  logic signed [24:0] kx_q, kz_q;
  logic        [7:0]  fd_q;            // fill address, starts at -128
  logic        [7:0]  fcnt_q;
  logic signed [32:0] vx_q, vz_q;      // running table values

  logic fill_we_c;
  assign fill_we_c    = (eq_st_q == E_FILL);
  assign table_ready_o = tbl_valid_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      eq_st_q     <= E_IDLE;
      dirty_q     <= 1'b0;
      tbl_valid_q <= 1'b0;
      ssx_q       <= '0;
      ssz_q       <= '0;
      kbit_q      <= '0;
      kx_q        <= '0;
      kz_q        <= '0;
      fd_q        <= '0;
      fcnt_q      <= '0;
      vx_q        <= '0;
      vz_q        <= '0;
    end else begin
      if (cfg_epoch_we_c) begin
        dirty_q     <= 1'b1;
        tbl_valid_q <= 1'b0;
      end
      case (eq_st_q)
        E_IDLE: begin
          // The dirty flag is consumed one cycle AFTER the cfg write, so the
          // sums below read the just-written register values, not the bus.
          if (dirty_q && !cfg_epoch_we_c) begin
            dirty_q <= 1'b0;
            ssx_q   <= 17'(sun_x_q[0]) + ((SUNS > 1) ? 17'(sun_x_q[1]) : 17'sd0);
            ssz_q   <= 17'(sun_z_q[0]) + ((SUNS > 1) ? 17'(sun_z_q[1]) : 17'sd0);
            kx_q    <= '0;
            kz_q    <= '0;
            kbit_q  <= '0;
            eq_st_q <= E_KMUL;
          end
        end
        E_KMUL: begin
          if (strength_q[kbit_q]) begin
            kx_q <= kx_q + (25'(ssx_q) <<< kbit_q);
            kz_q <= kz_q + (25'(ssz_q) <<< kbit_q);
          end
          if (kbit_q == 3'd7) eq_st_q <= E_KPREP;
          kbit_q <= kbit_q + 3'd1;
        end
        E_KPREP: begin
          vx_q    <= -(33'(kx_q) <<< 7);   // -128 * Kx, a shift, exact
          vz_q    <= -(33'(kz_q) <<< 7);
          fd_q    <= 8'h80;                // two's-complement -128
          fcnt_q  <= '0;
          eq_st_q <= E_FILL;
        end
        E_FILL: begin
          // The write itself happens in the clock-only RAM process below.
          vx_q   <= vx_q + 33'(kx_q);
          vz_q   <= vz_q + 33'(kz_q);
          fd_q   <= fd_q + 8'd1;
          fcnt_q <= fcnt_q + 8'd1;
          if (fcnt_q == 8'd255) begin
            eq_st_q <= E_IDLE;
            // A cfg write that landed mid-fill re-dirties; only a clean fill
            // publishes the tables.
            if (!dirty_q && !cfg_epoch_we_c) tbl_valid_q <= 1'b1;
          end
        end
        default: eq_st_q <= E_IDLE;
      endcase
    end
  end

  // =========================================================================
  // Memories. All three are written and read ONLY inside clock-only
  // processes with registered addresses, and none is touched by reset —
  // an async-reset payload array cannot infer an M10K (rescue §4.1).
  // =========================================================================
  logic        [15:0] tile_m [0:PYR_WORDS-1];  // {s8 dz, s8 dx} pyramid
  logic signed [32:0] tblx_m [0:255];          // dx * Kx, exact
  logic signed [32:0] tblz_m [0:255];          // dz * Kz, exact

  always_ff @(posedge clk) begin
    if (tw_we_i && ({1'b0, tw_addr_i} < 14'(PYR_WORDS))) tile_m[tw_addr_i] <= tw_data_i;
  end

  always_ff @(posedge clk) begin
    if (fill_we_c) begin
      tblx_m[fd_q] <= vx_q;
      tblz_m[fd_q] <= vz_q;
    end
  end

  // =========================================================================
  // The fragment pipe. One shared advance: when the output register is
  // occupied and the consumer stalls, every stage holds. Fixed latency,
  // in order, II = 1 by construction.
  // =========================================================================
  logic adv_c;
  assign adv_c = !d_valid_o || d_ready_i;

  // ---- skid, so f_ready_o is registered -----------------------------------
  logic               sk_valid_q;
  logic signed [31:0] sk_u_q, sk_v_q;
  logic               sk_det_q;
  logic [LODW-1:0]    sk_lod_q;
  logic [15:0]        sk_src_q;

  logic push_c;
  assign push_c = f_valid_i && f_ready_o;

  // ---- P0: input registers ------------------------------------------------
  logic               p0_valid_q;
  logic signed [31:0] p0_u_q, p0_v_q;
  logic               p0_det_q;
  logic [LODW-1:0]    p0_lod_q;
  logic [15:0]        p0_src_q;

  // ---- P1: level select + world wrap --------------------------------------
  logic        p1_valid_q;
  logic        p1_det_q;
  logic        p1_cold_q;
  logic [2:0]  p1_lvl_q;
  logic [5:0]  p1_u6_q, p1_v6_q;
  logic [15:0] p1_src_q;

  // ---- P2: pyramid address ------------------------------------------------
  logic        p2_valid_q;
  logic        p2_det_q;
  logic        p2_cold_q;
  logic [12:0] p2_addr_q;
  logic [15:0] p2_src_q;

  // ---- P3: tile read result -----------------------------------------------
  logic        p3_valid_q;
  logic        p3_det_q;
  logic        p3_cold_q;
  logic [15:0] p3_src_q;
  logic [15:0] tile_q;      // read-enable gated: no read for a zeroed fragment

  // ---- P4: K-table read results -------------------------------------------
  logic               p4_valid_q;
  logic               p4_det_q;
  logic               p4_cold_q;
  logic [15:0]        p4_src_q;
  logic signed [32:0] tblx_q, tblz_q;

  // P1 combinational: authored level and wrapped texel coordinates.
  logic [2:0] max_eff_c;
  assign max_eff_c = (max_level_q > 3'(LEVELS - 1)) ? 3'(LEVELS - 1) : max_level_q;

  logic signed [5:0] lvl_raw_c;
  assign lvl_raw_c = 6'({1'b0, p0_lod_q}) + 6'(lod_bias_q);

  logic [2:0] lvl_c;
  always_comb begin
    if (lvl_raw_c < 6'sd0)                  lvl_c = 3'd0;
    else if (lvl_raw_c > 6'(max_eff_c))     lvl_c = max_eff_c;
    else                                    lvl_c = lvl_raw_c[2:0];
  end

  // Bit slice of the raw UV: two's-complement wrap IS the 64-texel tiling
  // seam law, negatives included.
  logic [5:0] u6_c, v6_c;
  assign u6_c = 6'(unsigned'(p0_u_q) >> uv_shift_q);
  assign v6_c = 6'(unsigned'(p0_v_q) >> uv_shift_q);

  // P2 combinational: base[L] + ((v6>>L) << (6-L)) + (u6>>L).
  logic [12:0] lvl_base_c;
  always_comb begin
    case (p1_lvl_q)
      3'd0:    lvl_base_c = 13'd0;
      3'd1:    lvl_base_c = 13'd4096;
      3'd2:    lvl_base_c = 13'd5120;
      3'd3:    lvl_base_c = 13'd5376;
      3'd4:    lvl_base_c = 13'd5440;
      3'd5:    lvl_base_c = 13'd5456;
      3'd6:    lvl_base_c = 13'd5460;
      default: lvl_base_c = 13'd0;
    endcase
  end

  logic [12:0] addr_c;
  assign addr_c = lvl_base_c
                + (13'(p1_v6_q >> p1_lvl_q) << (4'd6 - 4'(p1_lvl_q)))
                + 13'(p1_u6_q >> p1_lvl_q);

  // P5 combinational: sum, the ONE rounding, the s9 clamp.
  logic signed [33:0] sum_c;
  assign sum_c = 34'(tblx_q) + 34'(tblz_q);

  logic signed [33:0] rnd_c;
  assign rnd_c = (sum_c + (34'sd1 <<< (DELTA_SHIFT - 1))) >>> DELTA_SHIFT;

  logic               live_c;    // this fragment's delta is real
  logic               rail_c;
  logic signed [8:0]  delta_c;
  always_comb begin
    live_c = p4_det_q && !p4_cold_q && tbl_valid_q;
    rail_c = 1'b0;
    delta_c = 9'sd0;
    if (live_c) begin
      if (rnd_c > 34'sd255) begin
        delta_c = 9'sd255;
        rail_c  = 1'b1;
      end else if (rnd_c < -34'sd256) begin
        delta_c = -9'sd256;
        rail_c  = 1'b1;
      end else begin
        delta_c = rnd_c[8:0];
      end
    end
  end

  // The tile read, enable-gated: a zeroed fragment performs no read.
  always_ff @(posedge clk) begin
    if (adv_c && p2_valid_q && p2_det_q) tile_q <= tile_m[p2_addr_q];
  end

  // The K-table reads. Reading a table mid-fill can only reach a fragment
  // that live_c already forces to zero (tbl_valid_q is low for the whole
  // window), so the collision value is never used.
  always_ff @(posedge clk) begin
    if (adv_c) begin
      tblx_q <= tblx_m[tile_q[7:0]];
      tblz_q <= tblz_m[tile_q[15:8]];
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sk_valid_q <= 1'b0;
      sk_u_q     <= '0;
      sk_v_q     <= '0;
      sk_det_q   <= 1'b0;
      sk_lod_q   <= '0;
      sk_src_q   <= '0;
      f_ready_o  <= 1'b0;
      p0_valid_q <= 1'b0;
      p0_u_q     <= '0;
      p0_v_q     <= '0;
      p0_det_q   <= 1'b0;
      p0_lod_q   <= '0;
      p0_src_q   <= '0;
      p1_valid_q <= 1'b0;
      p1_det_q   <= 1'b0;
      p1_cold_q  <= 1'b0;
      p1_lvl_q   <= '0;
      p1_u6_q    <= '0;
      p1_v6_q    <= '0;
      p1_src_q   <= '0;
      p2_valid_q <= 1'b0;
      p2_det_q   <= 1'b0;
      p2_cold_q  <= 1'b0;
      p2_addr_q  <= '0;
      p2_src_q   <= '0;
      p3_valid_q <= 1'b0;
      p3_det_q   <= 1'b0;
      p3_cold_q  <= 1'b0;
      p3_src_q   <= '0;
      p4_valid_q <= 1'b0;
      p4_det_q   <= 1'b0;
      p4_cold_q  <= 1'b0;
      p4_src_q   <= '0;
      d_valid_o  <= 1'b0;
      d_delta_o  <= '0;
      d_src_id_o <= '0;
      fragments_o <= '0;
      zeroed_o    <= '0;
      railed_o    <= '0;
      cold_o      <= '0;
    end else begin
      // ---- skid + P0 ------------------------------------------------------
      if (adv_c) begin
        if (sk_valid_q) begin
          p0_valid_q <= 1'b1;
          p0_u_q     <= sk_u_q;
          p0_v_q     <= sk_v_q;
          p0_det_q   <= sk_det_q;
          p0_lod_q   <= sk_lod_q;
          p0_src_q   <= sk_src_q;
        end else begin
          p0_valid_q <= push_c;
          p0_u_q     <= f_u_i;
          p0_v_q     <= f_v_i;
          p0_det_q   <= f_detail_i;
          p0_lod_q   <= f_lod_i;
          p0_src_q   <= f_src_id_i;
        end
        sk_valid_q <= 1'b0;
      end else if (push_c) begin
        sk_valid_q <= 1'b1;
        sk_u_q     <= f_u_i;
        sk_v_q     <= f_v_i;
        sk_det_q   <= f_detail_i;
        sk_lod_q   <= f_lod_i;
        sk_src_q   <= f_src_id_i;
      end
      // Registered ready: exactly "the skid will be empty next cycle".
      f_ready_o <= adv_c ? 1'b1 : !(sk_valid_q || push_c);

      if (push_c) fragments_o <= fragments_o + 32'd1;

      // ---- P1..P4 ---------------------------------------------------------
      if (adv_c) begin
        p1_valid_q <= p0_valid_q;
        p1_det_q   <= p0_det_q;
        p1_cold_q  <= !tbl_valid_q;
        p1_lvl_q   <= lvl_c;
        p1_u6_q    <= u6_c;
        p1_v6_q    <= v6_c;
        p1_src_q   <= p0_src_q;

        p2_valid_q <= p1_valid_q;
        p2_det_q   <= p1_det_q;
        p2_cold_q  <= p1_cold_q;
        p2_addr_q  <= addr_c;
        p2_src_q   <= p1_src_q;

        p3_valid_q <= p2_valid_q;
        p3_det_q   <= p2_det_q;
        p3_cold_q  <= p2_cold_q;
        p3_src_q   <= p2_src_q;

        p4_valid_q <= p3_valid_q;
        p4_det_q   <= p3_det_q;
        p4_cold_q  <= p3_cold_q;
        p4_src_q   <= p3_src_q;

        // ---- P5: emit -----------------------------------------------------
        d_valid_o  <= p4_valid_q;
        d_delta_o  <= delta_c;
        d_src_id_o <= p4_src_q;
        if (p4_valid_q) begin
          if (!p4_det_q)                                   zeroed_o <= zeroed_o + 32'd1;
          else if (p4_cold_q || !tbl_valid_q)              cold_o   <= cold_o + 32'd1;
          else if (rail_c)                                 railed_o <= railed_o + 32'd1;
        end
      end
    end
  end

  assign idle_o = !(sk_valid_q || p0_valid_q || p1_valid_q || p2_valid_q ||
                    p3_valid_q || p4_valid_q || d_valid_o) &&
                  (eq_st_q == E_IDLE) && !dirty_q;

endmodule : zhao_terrain_normalmap

`default_nettype wire
