// zhao_terrain_shade.sv — TERRAIN.SHADE: the terrain base light (ZH-081).
//
// *** OWNERSHIP (2026-09-09, TERRAIN.SHADE.md A7 / GEOM.LIGHT.md): THIS IS
// THE SHARED LIGHTING CORE, NOT A TERRAIN-ONLY ENGINE. Its n_x/y/z_i ports
// carry a WORLD NORMAL — there is no face-normal stage in here (that is
// zhao_terrain_normals, one of GEOM.LIGHT's THREE normal producers). It is
// the hardware of `zref::render::shade_from_world_normal_unclamped`, the
// D-1-one-level-down core, and tier 2 of the directed test checks it
// against that COMPILED function over the full port domain. GEOM.LIGHT's
// vertex-RGB block INSTANTIATES this module; building a second engine for
// the other normal producers is the mistake both contracts now forbid. ***
//
// dot(n, L) / |n|, once per TRIANGLE, sign preserved, bit-exact against
// `zref::render::shade_flat_tri_dir_unclamped` (reference/src/zrender/
// terrain.cpp) — THE ratified flat-shade law, which the golden captures pin.
// This block is the reason TERRAIN.NORMALS' output finally has a consumer:
// production terrain has had NO lighting path at all.
//
// Law, in citation order:
//   design/contracts/TERRAIN.SHADE.md              — the block contract
//     (+ 2026-09-09 amendments A1..A6 recorded there and mirrored here).
//   reference/src/zrender/terrain.cpp              — shade_flat_tri_dir_unclamped:
//     ndot (s128) over isqrt_u64(nmag2 u64), ONE div_rhu_s128 rounding,
//     INT32 clamp, nmag2 == 0 returns 0.
//   reference/include/zref/zref_terrain_shade.hpp  — the thin view: squared
//     norm in UNSIGNED 64, the dot in wide signed, the degeneracy predicate,
//     and the correction this block must not re-import: THE LIGHT IS Q16.16,
//     NOT s1.15 ("a factor of two in the relief").
//   reports/ZHAOZHOU_MEMORY_FIRST_RESOURCE_RESCUE_2026-09-09.txt §14.1/§14.3
//     — exact law, no rsqrt/Newton approximation; bounded byte tables give
//     exact products; spend M10K, refuse ALM and DSP.
//
// ---------------------------------------------------------------------------
// FORMATS (contract amendments A1/A2 — the original packet table said s1.15
// for the sun and the output, which is the exact error the oracle's history
// records catching; both are Q16.16 here because the LAW is Q16.16)
// ---------------------------------------------------------------------------
//   n_x/y/z_i  : signed 32, Q16.16, UN-normalised — exactly what
//                zhao_terrain_normals emits (its rescale(.,16) lanes).
//   sun_x/y/z_i: signed 32, Q16.16. The renderer's key light is the
//                hand-normalised unit (1,2,1)/sqrt(6) = 26758/53521/26758
//                (zref::terrain::kShadeLight*), but the LAW accepts any
//                int32 triple and so does this block. The light is a PACKET
//                FIELD, not a parameter: the owner retunes the sun by
//                changing the driver's constants, never this arithmetic.
//   base_o     : signed 32, Q16.16, SIGN PRESERVED, saturated exactly where
//                the law saturates (INT32 rails inside div_rhu_s128).
//                A unit sun keeps |base| <= ~0x10005; the INT32 clamp is
//                reachable only far outside the unit-sun domain, and the
//                block reproduces it exactly there too.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC SHAPE: 0 DSP, 1 M10K, ~147 clocks per triangle
// ---------------------------------------------------------------------------
// The demand is 2,000 terrain triangles per frame against a 1,666,666-clock
// compute frame (design/budgets/workloads.yml; zhao_terrain_normals' own
// header derives the same number) — 833 clocks available per triangle at
// matched rate. The old estimate spent 10 DSP to sustain II=1, i.e. capacity
// 833x the demand, on a console whose DSP budget stands at 171% committed.
// This block spends the abundant resource instead:
//
//   * ALL six 32x32 products (three squares for |n|^2, three sun products
//     for ndot) come from ONE quarter-square table in ONE M10K:
//         Q[s] = floor(s*s/4),  s in [0, 511],  16 bits
//         a*b  = Q[a+b] - Q[|a-b|]        (exact for all bytes a, b)
//     The identity is exact because (a+b) and (a-b) share parity: both even
//     means both squares divide by 4 exactly; both odd means both floors
//     drop exactly 1/4 each and the difference cancels. The directed test
//     proves it exhaustively over all 65,536 byte pairs rather than citing
//     this comment.
//     A 32-bit operand is four bytes, so a square is 10 byte-products
//     (4 self + 6 doubled cross) and a full multiply is 16; the walk is
//     3*10 + 3*16 = 78 table cycles on the ROM's two ports (one product per
//     cycle: port A reads Q[a+b], port B reads Q[|a-b|]).
//   * isqrt is the law's own restoring floor-sqrt digit recurrence
//     (spec/qformats.md §7.2), 32 add/sub iterations, no multiplier —
//     and it OVERLAPS the sun-product walk: |n|^2 is complete after the 30
//     square steps, so the root runs while ndot is still accumulating and
//     costs zero additional cycles.
//   * the divide is div_rhu_s128 verbatim: h = ndot + (mag >> 1), then
//     FLOOR(h / mag) — a 64-step restoring divide over the |h| magnitude
//     with the floor adjustment for negative h — then the INT32 clamp.
//     The draft normalmap block died on exactly this divider (32 steps over
//     a 64-bit numerator, quotient bits 63..32, every base silently zero);
//     this one walks all 64 quotient bits and the overhead-sun directed
//     case exists to prove it.
//
// WHY NOT AN RSQRT TABLE IN M10K: the brief prices it, and the answer is
// that no table of reciprocal roots can reproduce the law's exact quotient
// for every input — the exact law needs a real root and a real divide, and
// BOTH are add/sub iterations costing zero DSP and zero M10K. The M10K goes
// where tables are exact: the products.
//
// WIDTHS, PROVED NOT ASSUMED (the contract's overflow clause):
//   |n_c|, |l_c| <= 2^31 (|INT32_MIN| included), so a product <= 2^62 and
//   nmag2 <= 3*2^62 = 1.38e19 — over signed 64's rail, INSIDE unsigned 64:
//   macc is u64. ndot's magnitude <= 3*2^62 needs 65 signed bits: dacc is
//   s66. h = ndot + mag/2 has magnitude < 2^64: the divide numerator is the
//   u64 |h| with the sign carried beside it. The quotient can reach 2^64-1
//   (tiny |n|, huge sun), which is exactly why the law's INT32 clamp exists
//   and why the quotient register is 64 bits wide.
//
// TIMING: fixed latency, one triangle in flight. accept -> PROD (79) ->
// DSET (1) -> DIV (64) -> FIN (1) -> OUT (>=1) = 146 cycles to base_valid_o,
// II = 147 with a ready consumer. Data-independent: a degenerate triangle
// walks the same cycles and muxes 0 at FIN (the law's nmag2 == 0 arm), so
// `latency: fixed` holds. Capacity 1,666,666/147 = ~11,300 triangles/frame
// = 5.7x the 2,000 demand; the unused parallelism stays here, per the
// contract's ordering rule ("if resources are tight, lower THIS block's
// throughput point").
//
// The quarter-square ROM is FILLED at reset (512 cycles) by the s^2
// recurrence (s+1)^2 = s^2 + 2s + 1 — pure adds, no init file, no
// elaboration multiply, and the memory is never touched by reset itself so
// it can infer an M10K (rescue §4.1; zhao_terrain_normalmap.sv is the
// committed precedent for fill-at-cold). table_ready_o gates tri_ready_o.
//
// COUNTERS, all reachable with port stimulus and all SEEN TO FIRE in
// tests/terrain/terrain_shade_rtl_directed.cpp with exact-count assertions:
//   triangles_shaded_o  every completed triangle
//   degenerate_count_o  law-degenerate (nmag2 == 0) triangles
//   base_sat_o          the law's INT32 clamp engaged (needs a far-beyond-
//                       unit sun, which IS inside the law's int32 domain)
//   degen_mismatch_o    degenerate_i disagreed with the law's own nmag2==0
//                       — the seam integrity check against TERRAIN.NORMALS,
//                       whose two operands arrive by INDEPENDENT paths
//                       (flag from the producer, predicate from this walk),
//                       per the 2026-09-08 checker law.
//
// Conservative SystemVerilog subset only (charter §2). No generate blocks;
// elaboration guards live inside `initial begin` (Quartus 17 form law).

module zhao_terrain_shade #(
    parameter int unsigned CNTW = 32   // counter width, editable
) (
    input logic clk,
    input logic rst_n,

    // terrain_normals: one face normal per triangle, plus the sun riding
    // the packet (a moving sun is per-triangle data, not configuration).
    input  logic               tri_valid_i,
    output logic               tri_ready_o,
    input  logic signed [31:0] n_x_i,
    input  logic signed [31:0] n_y_i,
    input  logic signed [31:0] n_z_i,
    input  logic               degenerate_i,
    input  logic signed [31:0] sun_x_i,
    input  logic signed [31:0] sun_y_i,
    input  logic signed [31:0] sun_z_i,
    input  logic        [15:0] src_id_i,

    // terrain_shade: the signed, unclamped base light, Q16.16.
    output logic               base_valid_o,
    input  logic               base_ready_i,
    output logic signed [31:0] base_o,
    output logic               degenerate_o,
    output logic        [15:0] src_id_o,

    output logic [CNTW-1:0] triangles_shaded_o,
    output logic [CNTW-1:0] degenerate_count_o,
    output logic [CNTW-1:0] base_sat_o,
    output logic [CNTW-1:0] degen_mismatch_o,
    output logic            table_ready_o,
    output logic            idle_o
);

  // ---- named shapes (localparams: structural, but named so the next
  // reader can retune the walk without re-deriving it) ----------------------
  localparam int unsigned QROM_DEPTH = 512;  // Q[s], s in [0, 511]
  localparam int unsigned QW         = 16;   // floor(510^2/4) = 65025 < 2^16
  localparam int unsigned SQ_STEPS   = 10;   // byte-products per square
  localparam int unsigned MUL_STEPS  = 16;   // byte-products per full multiply
  localparam int unsigned SQRT_STEPS = 32;   // qformats §7.2 digit recurrence
  localparam int unsigned DIV_STEPS  = 64;   // full u64 quotient — see header

  // ---- elaboration guards (Quartus 17: MUST be inside initial begin;
  // and note --lint-only does not run these, so a clean lint proves nothing
  // about them — the ctest/standalone run is what executes them) ------------
  initial begin
    if ((510 * 510) / 4 > (1 << QW) - 1)
      $fatal(1, "zhao_terrain_shade: QW too narrow for the quarter-square table");
    if (3 * SQ_STEPS + 3 * MUL_STEPS != 78)
      $fatal(1, "zhao_terrain_shade: product walk is not the 78 steps the timing law states");
    if (CNTW < 8)
      $fatal(1, "zhao_terrain_shade: CNTW below 8 makes the counters decorative");
  end

  // ---- state --------------------------------------------------------------
  typedef enum logic [2:0] {
    ST_FILL = 3'd0,  // cold: write the quarter-square table, 512 cycles
    ST_IDLE = 3'd1,
    ST_PROD = 3'd2,  // the 78-step byte-product walk (+1 drain)
    ST_DSET = 3'd3,  // h = ndot + (mag>>1); sign/magnitude split
    ST_DIV  = 3'd4,  // 64-step restoring divide
    ST_FIN  = 3'd5,  // floor adjust, INT32 clamp, degenerate mux, counters
    ST_OUT  = 3'd6   // hold base_valid_o until base_ready_i
  } state_e;

  state_e st_q;

  // ---- the ONE memory: quarter-square table, true dual port ---------------
  // Written only during ST_FILL (port A), read on both ports during ST_PROD.
  // Clock-only process, untouched by reset, so it can infer an M10K.
  logic [QW-1:0] qmem [0:QROM_DEPTH-1];
  logic [QW-1:0] qa_q, qb_q;

  logic [8:0]  fill_s_q;   // s, 0..511
  logic [17:0] fill_sq_q;  // s^2 by recurrence; 511^2 = 261121 < 2^18
  logic        fill_we_c;
  logic [8:0]  qaddr_a_c;
  logic [7:0]  qaddr_b_c;

  assign fill_we_c = (st_q == ST_FILL);

  always_ff @(posedge clk) begin
    if (fill_we_c) qmem[fill_s_q] <= fill_sq_q[17:2];  // floor(s^2/4)
    else           qa_q           <= qmem[qaddr_a_c];
  end
  always_ff @(posedge clk) begin
    qb_q <= qmem[{1'b0, qaddr_b_c}];
  end

  // ---- captured packet ----------------------------------------------------
  // Magnitude/sign split at accept: |INT32_MIN| = 2^31 fits unsigned 32.
  function automatic logic [31:0] abs32(input logic signed [31:0] x);
    abs32 = x[31] ? (~$unsigned(x) + 32'd1) : $unsigned(x);
  endfunction

  logic [31:0] un_q [0:2];   // |n_x|, |n_y|, |n_z|
  logic [31:0] ul_q [0:2];   // |sun_x|, |sun_y|, |sun_z|
  logic [2:0]  nsgn_q, lsgn_q;
  logic        degen_in_q;
  logic [15:0] src_q;

  // ---- product walk issue side --------------------------------------------
  logic [2:0] job_q;   // 0..2 squares of n, 3..5 n*sun per axis
  logic [4:0] k_q;     // step within job
  logic       issue_done_q;

  // squares: the 10 byte pairs of a 4-byte operand, cross pairs doubled
  function automatic logic [4:0] sq_pair(input logic [4:0] k);
    // {ia[1:0], ib[1:0], dbl}
    unique case (k)
      5'd0:    sq_pair = {2'd0, 2'd0, 1'b0};
      5'd1:    sq_pair = {2'd1, 2'd1, 1'b0};
      5'd2:    sq_pair = {2'd2, 2'd2, 1'b0};
      5'd3:    sq_pair = {2'd3, 2'd3, 1'b0};
      5'd4:    sq_pair = {2'd0, 2'd1, 1'b1};
      5'd5:    sq_pair = {2'd0, 2'd2, 1'b1};
      5'd6:    sq_pair = {2'd0, 2'd3, 1'b1};
      5'd7:    sq_pair = {2'd1, 2'd2, 1'b1};
      5'd8:    sq_pair = {2'd1, 2'd3, 1'b1};
      default: sq_pair = {2'd2, 2'd3, 1'b1};
    endcase
  endfunction

  logic       is_sq_c;
  logic [1:0] axis_c, ia_c, ib_c;
  logic       dbl_c, sgn_c;
  logic [7:0] abyte_c, bbyte_c;
  logic [4:0] pair_c;
  logic       issue_active_c, k_last_c;

  always_comb begin
    is_sq_c        = (job_q < 3'd3);
    axis_c         = is_sq_c ? job_q[1:0] : 2'(job_q - 3'd3);
    pair_c         = sq_pair(k_q);
    ia_c           = is_sq_c ? pair_c[4:3] : k_q[3:2];
    ib_c           = is_sq_c ? pair_c[2:1] : k_q[1:0];
    dbl_c          = is_sq_c ? pair_c[0] : 1'b0;
    sgn_c          = is_sq_c ? 1'b0 : (nsgn_q[axis_c] ^ lsgn_q[axis_c]);
    abyte_c        = un_q[axis_c][8*ia_c +: 8];
    bbyte_c        = is_sq_c ? un_q[axis_c][8*ib_c +: 8] : ul_q[axis_c][8*ib_c +: 8];
    qaddr_a_c      = {1'b0, abyte_c} + {1'b0, bbyte_c};
    qaddr_b_c      = (abyte_c >= bbyte_c) ? (abyte_c - bbyte_c) : (bbyte_c - abyte_c);
    issue_active_c = (st_q == ST_PROD) && !issue_done_q;
    k_last_c       = is_sq_c ? (k_q == 5'(SQ_STEPS - 1)) : (k_q == 5'(MUL_STEPS - 1));
  end

  // ---- product walk accumulate side (one cycle behind issue) --------------
  logic        pv_q, psq_q, pdbl_q, psgn_q, plsq_q, plast_q;
  logic [2:0]  psh_q;  // ia + ib, 0..6 -> shift by 8*psh_q

  logic [15:0]        p16_c;
  logic [65:0]        term_c, term_sh_c;
  logic [63:0]        macc_q, macc_next_c;
  logic signed [65:0] dacc_q;

  always_comb begin
    p16_c = qa_q - qb_q;  // Q[a+b] - Q[|a-b|] = a*b, exact
    unique case (psh_q)
      3'd0:    term_sh_c = {50'd0, p16_c};
      3'd1:    term_sh_c = {42'd0, p16_c, 8'd0};
      3'd2:    term_sh_c = {34'd0, p16_c, 16'd0};
      3'd3:    term_sh_c = {26'd0, p16_c, 24'd0};
      3'd4:    term_sh_c = {18'd0, p16_c, 32'd0};
      3'd5:    term_sh_c = {10'd0, p16_c, 40'd0};
      default: term_sh_c = {2'd0, p16_c, 48'd0};
    endcase
    term_c      = pdbl_q ? {term_sh_c[64:0], 1'b0} : term_sh_c;
    macc_next_c = macc_q + term_c[63:0];  // square terms: proved < 2^64 total
  end

  // ---- overlapped restoring floor-sqrt (qformats §7.2, the law's own) -----
  logic        is_run_q;
  logic [5:0]  is_cnt_q;
  logic [63:0] is_num_q, is_res_q, is_bit_q;
  logic        degen_law_q;  // nmag2 == 0, THE law's own predicate

  // ---- divide -------------------------------------------------------------
  logic        dneg_q;
  logic [63:0] da_q, dq_q;
  logic [31:0] dr_q;
  logic [6:0]  dv_cnt_q;

  logic [32:0] div_acc_c;
  logic        div_ge_c;
  always_comb begin
    div_acc_c = {dr_q, da_q[63]};
    div_ge_c  = (div_acc_c >= {1'b0, is_res_q[31:0]});
  end

  // ---- finalisation (comb, consumed in ST_FIN) ----------------------------
  logic               rem_nz_c;
  logic [64:0]        mneg_c;
  logic               sat_pos_c, sat_neg_c, sat_c;
  logic signed [31:0] base_c;

  always_comb begin
    rem_nz_c  = |dr_q;
    sat_pos_c = (dq_q > 64'd2147483647);
    mneg_c    = {1'b0, dq_q} + {64'd0, rem_nz_c};       // ceil for floor(neg/pos)
    sat_neg_c = (mneg_c > 65'd2147483648);
    sat_c     = !degen_law_q && (dneg_q ? sat_neg_c : sat_pos_c);
    if (degen_law_q)     base_c = 32'sd0;               // the law's nmag2==0 arm
    else if (!dneg_q)    base_c = sat_pos_c ? 32'sh7FFF_FFFF : $signed(dq_q[31:0]);
    else                 base_c = sat_neg_c ? 32'sh8000_0000
                                            : -$signed(mneg_c[31:0]);
  end

  // ---- output registers ---------------------------------------------------
  logic signed [31:0] base_q;
  logic               out_degen_q;
  logic        [15:0] out_src_q;

  // |h| < 2^64 (header proof), so h fits signed 65 and the truncation from
  // the s66 sum is exact — bit 64 of the wide sum only ever repeats the sign.
  wire signed [64:0] h_c = 65'(dacc_q + $signed({35'd0, is_res_q[31:1]}));

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q          <= ST_FILL;
      fill_s_q      <= 9'd0;
      fill_sq_q     <= 18'd0;
      table_ready_o <= 1'b0;
      un_q[0] <= '0; un_q[1] <= '0; un_q[2] <= '0;
      ul_q[0] <= '0; ul_q[1] <= '0; ul_q[2] <= '0;
      nsgn_q <= '0;  lsgn_q <= '0;
      degen_in_q <= 1'b0;
      src_q      <= '0;
      job_q <= '0;  k_q <= '0;  issue_done_q <= 1'b0;
      pv_q <= 1'b0; psq_q <= 1'b0; pdbl_q <= 1'b0; psgn_q <= 1'b0;
      plsq_q <= 1'b0; plast_q <= 1'b0; psh_q <= '0;
      macc_q <= '0; dacc_q <= '0;
      is_run_q <= 1'b0; is_cnt_q <= '0;
      is_num_q <= '0; is_res_q <= '0; is_bit_q <= '0;
      degen_law_q <= 1'b0;
      dneg_q <= 1'b0; da_q <= '0; dq_q <= '0; dr_q <= '0; dv_cnt_q <= '0;
      base_q <= '0; out_degen_q <= 1'b0; out_src_q <= '0;
      triangles_shaded_o <= '0;
      degenerate_count_o <= '0;
      base_sat_o         <= '0;
      degen_mismatch_o   <= '0;
    end else begin
      // ---- cold fill: Q[s] = floor(s^2/4) by the (s+1)^2 recurrence ------
      if (st_q == ST_FILL) begin
        fill_sq_q <= fill_sq_q + {8'd0, fill_s_q, 1'b0} + 18'd1;
        fill_s_q  <= fill_s_q + 9'd1;
        if (fill_s_q == 9'd511) begin
          st_q          <= ST_IDLE;
          table_ready_o <= 1'b1;
        end
      end

      // ---- accept ---------------------------------------------------------
      if (st_q == ST_IDLE && tri_valid_i) begin
        un_q[0] <= abs32(n_x_i);
        un_q[1] <= abs32(n_y_i);
        un_q[2] <= abs32(n_z_i);
        ul_q[0] <= abs32(sun_x_i);
        ul_q[1] <= abs32(sun_y_i);
        ul_q[2] <= abs32(sun_z_i);
        nsgn_q  <= {n_z_i[31], n_y_i[31], n_x_i[31]};
        lsgn_q  <= {sun_z_i[31], sun_y_i[31], sun_x_i[31]};
        degen_in_q   <= degenerate_i;
        src_q        <= src_id_i;
        job_q        <= 3'd0;
        k_q          <= 5'd0;
        issue_done_q <= 1'b0;
        macc_q       <= 64'd0;
        dacc_q       <= 66'sd0;
        st_q         <= ST_PROD;
      end

      // ---- issue counters -------------------------------------------------
      if (issue_active_c) begin
        if (k_last_c) begin
          k_q <= 5'd0;
          if (job_q == 3'd5) issue_done_q <= 1'b1;
          else               job_q        <= job_q + 3'd1;
        end else begin
          k_q <= k_q + 5'd1;
        end
      end

      // ---- issue -> accumulate pipeline registers ------------------------
      pv_q    <= issue_active_c;
      psq_q   <= is_sq_c;
      psh_q   <= {1'b0, ia_c} + {1'b0, ib_c};
      pdbl_q  <= dbl_c;
      psgn_q  <= sgn_c;
      plsq_q  <= issue_active_c && (job_q == 3'd2) && k_last_c;
      plast_q <= issue_active_c && (job_q == 3'd5) && k_last_c;

      // ---- accumulate -----------------------------------------------------
      if (pv_q) begin
        if (psq_q) begin
          macc_q <= macc_next_c;
          if (plsq_q) begin
            // |n|^2 is complete THIS edge: judge degeneracy (the law's own
            // predicate) and launch the root under the remaining sun walk.
            degen_law_q <= (macc_next_c == 64'd0);
            is_num_q    <= macc_next_c;
            is_res_q    <= 64'd0;
            is_bit_q    <= 64'h4000_0000_0000_0000;  // 1 << 62
            is_cnt_q    <= 6'd0;
            is_run_q    <= 1'b1;
          end
        end else begin
          dacc_q <= psgn_q ? (dacc_q - $signed(term_c)) : (dacc_q + $signed(term_c));
        end
        if (plast_q) st_q <= ST_DSET;
      end

      // ---- the root, overlapped (finishes at walk cycle ~63 of 79) -------
      if (is_run_q) begin
        if (is_num_q >= is_res_q + is_bit_q) begin
          is_num_q <= is_num_q - (is_res_q + is_bit_q);
          is_res_q <= (is_res_q >> 1) + is_bit_q;
        end else begin
          is_res_q <= is_res_q >> 1;
        end
        is_bit_q <= is_bit_q >> 2;
        is_cnt_q <= is_cnt_q + 6'd1;
        if (is_cnt_q == 6'(SQRT_STEPS - 1)) is_run_q <= 1'b0;
      end

      // ---- divide setup: h = ndot + (mag >> 1), split sign/magnitude -----
      if (st_q == ST_DSET && !is_run_q) begin
        dneg_q   <= h_c[64];
        da_q     <= h_c[64] ? (~h_c[63:0] + 64'd1) : h_c[63:0];  // |h| < 2^64, proved
        dq_q     <= 64'd0;
        dr_q     <= 32'd0;
        dv_cnt_q <= 7'd0;
        st_q     <= ST_DIV;
      end

      // ---- 64-step restoring divide: floor(|h| / mag) --------------------
      if (st_q == ST_DIV) begin
        dr_q     <= div_ge_c ? (div_acc_c[31:0] - is_res_q[31:0]) : div_acc_c[31:0];
        dq_q     <= {dq_q[62:0], div_ge_c};
        da_q     <= {da_q[62:0], 1'b0};
        dv_cnt_q <= dv_cnt_q + 7'd1;
        if (dv_cnt_q == 7'(DIV_STEPS - 1)) st_q <= ST_FIN;
      end

      // ---- finalise: floor adjust, INT32 clamp, degenerate mux, count ----
      if (st_q == ST_FIN) begin
        base_q      <= base_c;
        out_degen_q <= degen_law_q;
        out_src_q   <= src_q;
        triangles_shaded_o <= triangles_shaded_o + CNTW'(1);
        if (degen_law_q)              degenerate_count_o <= degenerate_count_o + CNTW'(1);
        if (sat_c)                    base_sat_o         <= base_sat_o + CNTW'(1);
        if (degen_in_q != degen_law_q) degen_mismatch_o  <= degen_mismatch_o + CNTW'(1);
        st_q <= ST_OUT;
      end

      // ---- hand over ------------------------------------------------------
      if (st_q == ST_OUT && base_ready_i) st_q <= ST_IDLE;
    end
  end

  assign tri_ready_o  = (st_q == ST_IDLE);
  assign base_valid_o = (st_q == ST_OUT);
  assign base_o       = base_q;
  assign degenerate_o = out_degen_q;
  assign src_id_o     = out_src_q;
  assign idle_o       = (st_q == ST_IDLE);

endmodule : zhao_terrain_shade
