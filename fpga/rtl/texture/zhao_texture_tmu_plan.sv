// zhao_texture_tmu_plan.sv — the TMU address planner as five ELASTIC stages.
//
// BESIDE `zhao_texture_tmu_pipe.sv`, which stays the golden implementation and
// the oracle. Nothing instantiates this yet.
//
// ---------------------------------------------------------------------------
// THE DEFECT THIS FIXES, NAMED BY THE BRIEF
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md:
//
//   > Every stage is a genuine elastic register. There must be no single a0_v
//   > that prevents accepting request N+1 while request N advances.
//
// The current pipe's accept line is exactly that:
//
//     assign req_ready_o = !rob_full_c && !pf_v && (!a0_v || !c_v || cac_ready_i);
//
// One request sits in `a0_*` for the whole plan, so N+1 cannot be accepted
// until N has either issued or the cache is ready. The planner is "staged"
// across registers but behaves as one occupancy: five stages of logic and one
// request in them.
//
// Here each of T0..T4 carries its own valid and its own payload, and readiness
// propagates backwards one stage at a time. Five requests are in flight. The
// accept line depends on T0's occupancy alone:
//
//     assign req_ready_o = !t0_v || t1_rdy;
//
// ---------------------------------------------------------------------------
// STAGE MAP, from the brief
// ---------------------------------------------------------------------------
//   T0  accept request, capture raw packet
//   T1  sanitize mode, select format/filter/mip, clamp level, capture binding
//   T2  selected-level dimensions, scale U/V, half-texel bias, integer+fraction
//   T3  wrap the two unique U and two unique V, calculate the two row bases
//   T4  form the four final addresses, enqueue one registered access packet
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC IS TRANSCRIBED, NOT REDERIVED
// ---------------------------------------------------------------------------
// Every expression below is copied from `zhao_texture_tmu_pipe.sv`. The paired
// test drives both and compares the four addresses, the enables and the
// per-request flags, so a transcription slip fails immediately rather than
// becoming a plausible-looking wrong address. That matters more than usual
// here: a wrong texel address produces a picture that is subtly wrong
// everywhere and obviously wrong nowhere.
//
// REP4[L] = (4^L - 1)/3 is copied verbatim WITH its two documented traps:
// REP4[0] is 0 by law (a closed form would give 1) and REP4[15] is the value
// the serial block states rather than the one the formula suggests.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_tmu_plan #(
    parameter int unsigned SRCW = 16,
    // ------------------------------------------------------------------------
    // THE LARGEST TEXTURE DIMENSION THIS PLANNER ADDRESSES, as log2.
    //
    // MEASURED, NOT ASSUMED. The first fit of this block came back at
    // 93.55 MHz -- below the shell's own 99.50 MHz and 56 MHz below the
    // brief's 150 MHz leaf target -- and the setup report named the path:
    //
    //   t2_iv0[0] -> t3_row0[13]   data delay 10.121 ns, 200 paths, 12 violated
    //
    // That is a 32-bit wrap fold (a 32-bit magnitude compare and a 32-bit
    // subtract) feeding a 32-bit barrel shift. None of those quantities is
    // 32 bits wide. A wrapped texel coordinate is bounded by the texture, and
    // TEXTURE.TMU.md records what the textures actually are:
    //
    //   > Everything else Sacrifice ships is strictly power-of-two and square,
    //   > and nothing exceeds 256x256.
    //
    // 11 is 2048 -- EIGHT DOUBLINGS above the largest asset that exists. It is
    // a parameter and not a literal because a bound chosen from today's assets
    // must stay the owner's to move; see CLAUDE.md, "never remove the owner's
    // control in the name of fidelity".
    //
    // A dimension ABOVE the bound is a DECLARED error, not a silent wrong
    // address. That is the charter's phase-5 gate ("no unsupported state
    // silently falls back") and it is the difference between a limit and a bug.
    parameter int unsigned MAXLOG2 = 11
) (
    input var logic clk,
    input var logic rst_n,

    // ---- request -------------------------------------------------------------
    input  var logic            req_valid_i,
    output var logic            req_ready_o,
    input  var logic [31:0]     req_u_i,
    input  var logic [31:0]     req_v_i,
    input  var logic [31:0]     req_base_i,
    input  var logic [31:0]     req_mode_i,
    input  var logic [ 7:0]     req_lod_i,
    input  var logic [SRCW-1:0] req_src_id_i,

    // ---- the registered cache-access packet ---------------------------------
    output var logic             acc_valid_o,
    input  var logic             acc_ready_i,
    output var logic [  3:0]     acc_en_o,
    output var logic [127:0]     acc_addr_o,
    output var logic [SRCW-1:0]  acc_src_id_o,
    output var logic             acc_filter_o,
    output var logic             acc_err_o,
    // CLUT4'S NIBBLE, one bit per lane. It exists because the address below
    // DESTROYS it: a CLUT4 address is `total >> 1`, so the texel index's low
    // bit -- which of the two nibbles in that byte -- is shifted out and cannot
    // be recovered from anything the island receives. Before this port, every
    // odd CLUT4 texel read its neighbour's palette index, and nothing counted
    // it because the lookup itself succeeded.
    //
    // Zero for every other format, so a consumer that ignores it is correct
    // everywhere except CLUT4, which is exactly where it must not be ignored.
    output var logic [  3:0]     acc_nib_o,
    output var logic [  7:0]     acc_fu_o,
    output var logic [  7:0]     acc_fv_o,
    output var logic [  2:0]     acc_fmt_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]      accepted_o,
    output var logic [ 3:0]      occupancy_o
);

  // ---- mode field positions, copied ----------------------------------------
  // READ FROM zhao_texture_tmu_pipe.sv:171, NOT ASSUMED -- the SECOND encoding
  // this file guessed wrong. The first draft had CLAMP=0, MIRROR=1, REPEAT=2,
  // which is the order the names are usually listed in and is not the order
  // shipped. Every wv=0 coordinate then took CLAMP here and REPEAT in the
  // reference, and a negative V clamped to row 0 instead of wrapping to row 8.
  //
  // Verified by hand on src=1 before changing anything: with REPEAT,
  // iv0 = -24 -> 0xFFFFFFE8 & 15 = 8, row0 = 512; iu0 = 96 & 63 = 32; total =
  // 20480 + 512 + 32 = 21024 -> 0x0010A440, which is exactly what the pipe
  // emits. Two guessed encodings, two identical mistakes, and both hid behind
  // addresses that looked plausible.
  // A coordinate inside a 2^MAXLOG2 texture needs MAXLOG2 bits; CW carries one
  // spare so the MIRROR period (2*size) is representable without truncation.
  localparam int unsigned CW = MAXLOG2 + 1;
  // A texel index inside one level, plus the mip chain's 4/3 tail: 2*MAXLOG2
  // for the level and 2 bits for the chain.
  localparam int unsigned TW = 2 * MAXLOG2 + 2;

  localparam logic [1:0] WRAP_REPEAT = 2'd0;
  localparam logic [1:0] WRAP_CLAMP  = 2'd1;
  localparam logic [1:0] WRAP_MIRROR = 2'd2;

  // READ FROM zhao_texture_tmu_pipe.sv:169, NOT ASSUMED. The first draft of
  // this file invented CLUT4=0, CLUT8=1, RGB565=2 -- a perfectly reasonable
  // ordering that is not the one shipped. Every 16bpp request then took the
  // CLUT4 address path in the reference and the 16bpp path here, and 345 of
  // 357 addresses differed while the underlying totals were IDENTICAL.
  //
  // The tell was arithmetic: for src=0x901 the reference gave
  // base + (total >> 1) of THIS planner's own total. Same maths, different
  // format lane. Guessing an encoding is the same error as guessing an
  // arithmetic law, and it hid behind a plausible-looking mismatch for an hour.
  localparam logic [2:0] FMT_CLUT8    = 3'd0;
  localparam logic [2:0] FMT_RGB565   = 3'd1;
  localparam logic [2:0] FMT_CLUT4    = 3'd2;
  localparam logic [2:0] FMT_ARGB1555 = 3'd3;
  localparam logic [2:0] FMT_ARGB4444 = 3'd4;

  // REP4[L] = (4^L - 1)/3, COPIED FROM THE SERIAL BLOCK rather than computed:
  // a closed form gives REP4[0] = 1 where the law says 0, and disagrees at 15.
  localparam logic [31:0] REP4 [0:15] = '{
      32'd0,          32'd1,          32'd5,          32'd21,
      32'd85,         32'd341,        32'd1365,       32'd5461,
      32'd21845,      32'd87381,      32'd349525,     32'd1398101,
      32'd5592405,    32'd22369621,   32'd89478485,   32'd357913941};

  // The fold, on CW bits instead of 32, with the two facts that make the
  // narrowing EXACT rather than approximately right:
  //
  //   REPEAT is `t & mask` and MIRROR is `t & ((mask<<1)|1)`. Both masks are
  //   below 2^CW, so discarding bits at or above CW changes neither -- and
  //   because the masks are powers of two minus one, two's-complement
  //   truncation is the mathematically correct floor-mod for negative t too.
  //
  //   CLAMP is the only mode that reads magnitude, so it is the only one that
  //   needs the discarded bits. It gets them as two carried flags: `neg` (t was
  //   negative) and `ovf` (any bit at or above CW was set, so t exceeds every
  //   representable mask).
  function automatic logic [CW-1:0] wrap_c(input logic [CW-1:0] lo,
                                           input logic          neg,
                                           input logic          ovf,
                                           input logic [1:0]    mode,
                                           input logic [CW-1:0] mask);
    logic [CW-1:0] per, lo_;
    begin
      case (mode)
        WRAP_CLAMP:  wrap_c = neg ? '0 : ((ovf || (lo > mask)) ? mask : lo);
        WRAP_MIRROR: begin
          per   = lo & ((mask << 1) | CW'(1));
          lo_   = per & mask;
          wrap_c = (per > mask) ? (mask - lo_) : lo_;
        end
        WRAP_REPEAT: wrap_c = lo & mask;
        default:     wrap_c = lo & mask;
      endcase
    end
  endfunction

  // ==========================================================================
  // ELASTICITY
  // ==========================================================================
  // Backwards-propagating ready. A stage may accept when it is empty or when
  // the stage after it will take what it holds. This is the whole difference
  // from the current pipe: five occupancies instead of one.
  logic t0_v, t1_v, t2_v, t3_v, t4_v;
  logic t0_rdy, t1_rdy, t2_rdy, t3_rdy, t4_rdy;

  assign t4_rdy = !t4_v || acc_ready_i;
  assign t3_rdy = !t3_v || t4_rdy;
  assign t2_rdy = !t2_v || t3_rdy;
  assign t1_rdy = !t1_v || t2_rdy;
  assign t0_rdy = !t0_v || t1_rdy;
  assign req_ready_o = t0_rdy;

  // ---- T0 payload ----------------------------------------------------------
  logic [31:0]     t0_u, t0_vc, t0_base, t0_mode;
  // Only t0_lod[7:4] selects the level. The low nibble is the FRACTIONAL LOD,
  // which nothing here consumes because this planner does not blend between
  // levels -- the serial block does not either. Suppressed with that reason
  // rather than narrowed to 4 bits: the port carries the full Q4.4 value the
  // caller sends, and silently dropping half of an input at the boundary is
  // how a trilinear path would later be built on a lie.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [7:0]      t0_lod;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [SRCW-1:0] t0_src;

  // ---- T1: mode sanitise, level clamp -------------------------------------
  logic [2:0]  m_fmt;
  logic        m_filter, m_mip_en;
  logic [1:0]  m_wrap_u, m_wrap_v;
  logic [3:0]  m_log2w, m_log2h, m_maxlvl;
  logic [10:0] m_rsvd;
  logic        is_clut, is_16bpp, fmt_bad, filter_eff;
  logic [3:0]  chain_max, lvl_cap, lvl_req, level_c;
  logic        chain_max_lt, err_c;
  always_comb begin
    m_fmt    = t0_mode[2:0];
    m_filter = t0_mode[3];
    m_wrap_u = t0_mode[5:4];
    m_wrap_v = t0_mode[7:6];
    m_log2w  = t0_mode[11:8];
    m_log2h  = t0_mode[15:12];
    m_maxlvl = t0_mode[19:16];
    m_mip_en = t0_mode[20];
    m_rsvd   = t0_mode[31:21];

    is_clut  = (m_fmt == FMT_CLUT4) || (m_fmt == FMT_CLUT8);
    is_16bpp = (m_fmt == FMT_RGB565) || (m_fmt == FMT_ARGB1555) || (m_fmt == FMT_ARGB4444);
    fmt_bad  = (m_fmt > FMT_ARGB4444);

    chain_max    = (m_log2w < m_log2h) ? m_log2w : m_log2h;
    chain_max_lt = (m_maxlvl > chain_max);
    lvl_cap      = chain_max_lt ? chain_max : m_maxlvl;
    filter_eff   = m_filter && !is_clut;   // a palette is never filtered
    err_c        = (m_filter && is_clut) || (m_rsvd != 11'd0) || chain_max_lt || fmt_bad;

    lvl_req = m_mip_en ? t0_lod[7:4] : 4'd0;
    level_c = (lvl_req > lvl_cap) ? lvl_cap : lvl_req;
  end

  // No t1_lod: the LOD is consumed in T1 to produce `level_c`, so carrying it
  // further would be dead state that looks like a dependency.
  logic [31:0]     t1_u, t1_vc, t1_base;
  logic [SRCW-1:0] t1_src;
  logic [2:0]      t1_fmt;
  logic [1:0]      t1_wu, t1_wv;
  logic [3:0]      t1_log2w, t1_log2h, t1_level;
  logic            t1_filt, t1_err, t1_clut4, t1_16bpp;

  // ---- T2: level dimensions, scale, half-texel bias ------------------------
  logic [3:0]  log2w_l, log2h_l;
  logic [CW-1:0] mask_u, mask_v;
  logic [TW-1:0] lvl_off_c;
  logic [5:0]  lvl_shift;
  // Dimensions above the declared bound. Raised into the same `err` the block
  // already carries for a malformed mode word, so an oversized texture is
  // reported rather than addressed wrongly.
  logic        dim_over_c;
  // The bottom 8 bits of the biased coordinate are DISCARDED BY LAW: the
  // filter fraction is [15:8] and anything below it is sub-fractional. The
  // serial block drops them the same way; saying so is the difference between
  // reproducing the arithmetic and truncating by accident.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [47:0] tu_q, tv_q, tu_b, tv_b;
  /* verilator lint_on UNUSEDSIGNAL */
  logic signed [31:0] iu0_c, iv0_c, iu1_c, iv1_c;
  logic [7:0]  fu_c, fv_c;
  always_comb begin
    log2w_l   = t1_log2w - t1_level;
    log2h_l   = t1_log2h - t1_level;
    mask_u    = CW'((32'd1 << log2w_l) - 32'd1);
    mask_v    = CW'((32'd1 << log2h_l) - 32'd1);
    lvl_shift = 6'({4'd0, t1_log2w} + {4'd0, t1_log2h}) - 6'({4'd0, (t1_level - 4'd1)} << 1);
    lvl_off_c = (t1_level == 4'd0) ? TW'(0) : TW'(REP4[t1_level] << lvl_shift);

    tu_q = $signed({{16{t1_u[31]}}, t1_u}) <<< log2w_l;
    tv_q = $signed({{16{t1_vc[31]}}, t1_vc}) <<< log2h_l;
    tu_b = t1_filt ? (tu_q - 48'sd32768) : tu_q;
    tv_b = t1_filt ? (tv_q - 48'sd32768) : tv_q;
    iu0_c = tu_b[47:16];
    iv0_c = tv_b[47:16];
    // The +1 tap is formed HERE, in T2, not in T3. It used to be a 32-bit add
    // sitting in front of the wrap fold on the block's worst path; here it is
    // parallel to arithmetic that is already longer than it.
    iu1_c = iu0_c + 32'sd1;
    iv1_c = iv0_c + 32'sd1;
    fu_c  = t1_filt ? tu_b[15:8] : 8'd0;
    fv_c  = t1_filt ? tv_b[15:8] : 8'd0;
    dim_over_c = (t1_log2w > 4'(MAXLOG2)) || (t1_log2h > 4'(MAXLOG2));
  end

  logic [31:0]        t2_base;
  logic [CW-1:0]      t2_masku, t2_maskv;
  logic [TW-1:0]      t2_lvloff;
  // The coordinate, carried as the CW bits the fold reads plus the two flags
  // CLAMP needs from the bits that were dropped.
  logic [CW-1:0]      t2_iu0, t2_iv0, t2_iu1, t2_iv1;
  logic               t2_iu0n, t2_iv0n, t2_iu1n, t2_iv1n;
  logic               t2_iu0o, t2_iv0o, t2_iu1o, t2_iv1o;
  logic [7:0]         t2_fu, t2_fv;
  logic [3:0]         t2_log2w_l;
  logic [1:0]         t2_wu, t2_wv;
  logic [2:0]         t2_fmt;
  logic               t2_filt, t2_err, t2_clut4, t2_16bpp;
  logic [3:0]         nib_c;
  logic [SRCW-1:0]    t2_src;

  // ---- T3: wrap and row bases ---------------------------------------------
  logic [CW-1:0] uw0_c, uw1_c, vw0_c, vw1_c;
  logic [TW-1:0] row0_c, row1_c;
  always_comb begin
    uw0_c  = wrap_c(t2_iu0, t2_iu0n, t2_iu0o, t2_wu, t2_masku);
    uw1_c  = wrap_c(t2_iu1, t2_iu1n, t2_iu1o, t2_wu, t2_masku);
    vw0_c  = wrap_c(t2_iv0, t2_iv0n, t2_iv0o, t2_wv, t2_maskv);
    vw1_c  = wrap_c(t2_iv1, t2_iv1n, t2_iv1o, t2_wv, t2_maskv);
    row0_c = TW'(vw0_c) << t2_log2w_l;
    row1_c = TW'(vw1_c) << t2_log2w_l;
  end

  logic [31:0]     t3_base;
  logic [TW-1:0]   t3_lvloff, t3_row0, t3_row1;
  logic [CW-1:0]   t3_uw0, t3_uw1;
  logic [7:0]      t3_fu, t3_fv;
  logic [2:0]      t3_fmt;
  logic            t3_filt, t3_err, t3_clut4, t3_16bpp;
  logic [SRCW-1:0] t3_src;

  // ---- T4: the four addresses ---------------------------------------------
  // The texel index adds are TW wide, not 32. Only the final base add -- which
  // genuinely is an address -- is.
  logic [TW-1:0] total_c [0:3];
  logic [31:0] addr_c  [0:3];
  always_comb begin
    total_c[0] = t3_lvloff + t3_row0 + TW'(t3_uw0);
    total_c[1] = t3_lvloff + t3_row0 + TW'(t3_uw1);
    total_c[2] = t3_lvloff + t3_row1 + TW'(t3_uw0);
    total_c[3] = t3_lvloff + t3_row1 + TW'(t3_uw1);
    for (int unsigned k = 0; k < 4; k++) begin
      addr_c[k] = t3_16bpp ? (t3_base + 32'({1'b0, total_c[k]} << 1))
                : t3_clut4 ? (t3_base + 32'(total_c[k] >> 1))
                : (t3_base + 32'(total_c[k]));
      // The bit the CLUT4 shift above throws away, kept before it is lost.
      nib_c[k] = t3_clut4 ? total_c[k][0] : 1'b0;
    end
  end

  assign acc_valid_o = t4_v;

  always_comb begin
    occupancy_o = 4'(t0_v) + 4'(t1_v) + 4'(t2_v) + 4'(t3_v) + 4'(t4_v);
  end

  // ========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      t0_v <= 1'b0; t1_v <= 1'b0; t2_v <= 1'b0; t3_v <= 1'b0; t4_v <= 1'b0;
      accepted_o <= 32'd0;
    end else begin
      // ---- T0 ----------------------------------------------------------
      if (t0_rdy) begin
        t0_v <= req_valid_i;
        if (req_valid_i) begin
          t0_u    <= req_u_i;
          t0_vc   <= req_v_i;
          t0_base <= req_base_i;
          t0_mode <= req_mode_i;
          t0_lod  <= req_lod_i;
          t0_src  <= req_src_id_i;
          accepted_o <= accepted_o + 32'd1;
        end
      end

      // ---- T1 ----------------------------------------------------------
      if (t1_rdy) begin
        t1_v <= t0_v;
        if (t0_v) begin
          t1_u     <= t0_u;
          t1_vc    <= t0_vc;
          t1_base  <= t0_base;
          t1_src   <= t0_src;
          t1_fmt   <= m_fmt;
          t1_wu    <= m_wrap_u;
          t1_wv    <= m_wrap_v;
          t1_log2w <= m_log2w;
          t1_log2h <= m_log2h;
          t1_level <= level_c;
          t1_filt  <= filter_eff;
          t1_err   <= err_c;
          t1_clut4 <= (m_fmt == FMT_CLUT4);
          t1_16bpp <= is_16bpp;
        end
      end

      // ---- T2 ----------------------------------------------------------
      if (t2_rdy) begin
        t2_v <= t1_v;
        if (t1_v) begin
          t2_base    <= t1_base;
          t2_masku   <= mask_u;
          t2_maskv   <= mask_v;
          t2_lvloff  <= lvl_off_c;
          t2_iu0     <= CW'(iu0_c);
          t2_iv0     <= CW'(iv0_c);
          t2_iu1     <= CW'(iu1_c);
          t2_iv1     <= CW'(iv1_c);
          t2_iu0n    <= iu0_c[31];
          t2_iv0n    <= iv0_c[31];
          t2_iu1n    <= iu1_c[31];
          t2_iv1n    <= iv1_c[31];
          t2_iu0o    <= |iu0_c[30:CW];
          t2_iv0o    <= |iv0_c[30:CW];
          t2_iu1o    <= |iu1_c[30:CW];
          t2_iv1o    <= |iv1_c[30:CW];
          t2_fu      <= fu_c;
          t2_fv      <= fv_c;
          t2_log2w_l <= log2w_l;
          t2_wu      <= t1_wu;
          t2_wv      <= t1_wv;
          t2_fmt     <= t1_fmt;
          t2_filt    <= t1_filt;
          t2_err     <= t1_err || dim_over_c;
          t2_clut4   <= t1_clut4;
          t2_16bpp   <= t1_16bpp;
          t2_src     <= t1_src;
        end
      end

      // ---- T3 ----------------------------------------------------------
      if (t3_rdy) begin
        t3_v <= t2_v;
        if (t2_v) begin
          t3_base   <= t2_base;
          t3_lvloff <= t2_lvloff;
          t3_uw0    <= uw0_c;
          t3_uw1    <= uw1_c;
          t3_row0   <= row0_c;
          t3_row1   <= row1_c;
          t3_fu     <= t2_fu;
          t3_fv     <= t2_fv;
          t3_fmt    <= t2_fmt;
          t3_filt   <= t2_filt;
          t3_err    <= t2_err;
          t3_clut4  <= t2_clut4;
          t3_16bpp  <= t2_16bpp;
          t3_src    <= t2_src;
        end
      end

      // ---- T4 ----------------------------------------------------------
      if (t4_rdy) begin
        t4_v <= t3_v;
        if (t3_v) begin
          acc_addr_o   <= {addr_c[3], addr_c[2], addr_c[1], addr_c[0]};
          // A non-filtered request reads one texel; a filtered one reads four.
          acc_en_o     <= t3_filt ? 4'b1111 : 4'b0001;
          acc_src_id_o <= t3_src;
          acc_filter_o <= t3_filt;
          acc_err_o    <= t3_err;
          acc_nib_o    <= nib_c;
          acc_fu_o     <= t3_fu;
          acc_fv_o     <= t3_fv;
          acc_fmt_o    <= t3_fmt;
        end
      end
    end
  end

endmodule : zhao_texture_tmu_plan

`default_nettype wire
