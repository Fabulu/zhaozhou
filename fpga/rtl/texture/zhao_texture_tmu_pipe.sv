// zhao_texture_tmu_pipe.sv — TEXTURE.TMU v2: the production sampler.
//
// ============================================================================
// INCOMPLETE. NOT INSTANTIATED ANYWHERE. DO NOT WIRE THIS IN.
// ============================================================================
// This is the front half of the v2 sampler and it is committed in that state
// deliberately, because the part that exists is the part the respec turns on --
// and because a block that silently half-works is worse than one that says so.
//
// WHAT IS HERE AND IS BELIEVED RIGHT:
//   * A0 request capture, so no request pin feeds deep arithmetic. This is the
//     36.11 MHz story: the serial block runs pins through decode, channel
//     select and the whole factored bilinear to the output in one path.
//   * The planner, staged: level select, closed-form level offset, the UV
//     shift, half-texel bias, and `wrap_coord` called on each UNIQUE
//     coordinate ONCE (the serial block wraps eight times and shifts four rows
//     for a footprint with two distinct U, two distinct V and two rows).
//   * A registered cache issue bundle, held bit-stable until accepted.
//   * `req_ready_o` decoupled from completion: a request is accepted every
//     clock there is a free record. That is the II=1 claim.
//   * A ROB claimed at ACCEPT, so retirement is acceptance-ordered with no
//     sorting, and an in-flight tag FIFO carrying INTERNAL record ids because
//     `src_id` repeats.
//   * Resident palette storage, PAL_SLOTS=16.
//
// WHAT IS MISSING, AND UNTIL IT LANDS NOTHING RETIRES:
//   1. Response routing. The cache response must be steered by the RECORD's
//      plan, so the ROB needs the per-record fields (`fmt`, `clut`, `filt`,
//      `bytesel`, `nib`, `pal_base`, `fu`, `fv`, `err`) written at issue. They
//      are currently held only in the single `c_*` issue register, which has
//      moved on by the time the response arrives. THIS IS THE NEXT EDIT.
//   2. CLUT8: extract the raw index, read the resident palette page, decode
//      RGB565, alpha 255. Plus the cold fallback for a non-resident page,
//      which needs a second cache access and is what `pal_fallback_o` counts.
//   3. Direct nearest: decode the returned halfword straight into the record.
//   4. Direct bilinear: decode four taps, enqueue a footprint, and run the
//      F0..F3 channel lane (two 9x9 and one 18x9 product, ONE rounding at F3).
//   5. `smp_a_o`/`smp_idx_o` semantics per path, and `mode_error_o` proven to
//      track the accepted request under a full pipeline rather than by
//      inspection.
//
// It lints and it is referenced by no CMake target, so it cannot affect a gate.
// The shipping sampler is still `zhao_texture_tmu.sv`.
//
// ---------------------------------------------------------------------------
// WHY A SECOND IMPLEMENTATION RATHER THAN AN EDIT
// ---------------------------------------------------------------------------
// `zhao_texture_tmu.sv` is a one-request-at-a-time FSM: `req_ready_o` rises
// only in ST_IDLE, so a sample must finish completely before the next may
// exist. That shape gives CLUT 5 clocks a sample after the resident palette
// landed, and it is why the block fits at 36.11 MHz — the output sits
// combinationally behind format decode, channel selection and the whole
// factored bilinear expression. Reducing DSPs from 28 to 6 barely moved Fmax,
// which proved multiplier count was never the timing wall.
//
// That FSM is now an excellent EXECUTABLE SPECIFICATION and it is left exactly
// where it is, still differentially tested against an untouched `zref::Tmu`.
// This file is the machine, built beside it, with the SAME PORTS so that the
// entire existing directed and random suite can be pointed at either one. A
// second implementation that shares its predecessor's tests is the cheapest
// verification this project can buy.
//
// ---------------------------------------------------------------------------
// THE WORKLOAD THIS IS SHAPED FOR, WHICH IS NOT THE OLD ONE
// ---------------------------------------------------------------------------
// The old target was "850,000 samples a frame, so every mode must reach II 1".
// That number was 92,160 px x 3.0 overdraw x THREE TEXTURE LAYERS, and the
// third multiplier came from Sacrifice's `tile + detail + lightmap` map format.
// Zhaozhou samples ONCE per textured fragment (charter 26, and
// `zhao_raster_fragment` has exactly one texel port).
//
// The real known Z60 work is a MODE VECTOR, not a scalar:
//
//     terrain      CLUT8 nearest      276,480
//     sky backdrop CLUT8 nearest       92,160
//     stars        CLUT8 nearest      128,000
//     clouds       ARGB4444 bilinear   45,000
//     -------------------------------------------
//     known subtotal                  541,640   before creatures and beams
//
// So the dominant path by an order of magnitude is NEAREST, and every CLUT
// recipe is nearest-mandatory ("bilinear must never touch a palette"). The
// filtered work is one cloud sheet and some beams:
// 45,000 x 4 = 180,000 channel jobs against 541,640 cache accesses.
//
// The architecture follows from that and from nothing else:
//
//     request -> planner -> cache -+-> CLUT8 nearest -> palette RAM --+
//                                  +-> direct nearest -> decode ------+-> ROB
//                                  +-> direct bilinear -> filter -----+
//
// The nearest paths take one request per clock because the cache port already
// sustains it. The filter is ONE channel lane, because the measured floor is
// max(cache 541,640, palette 496,640, filter 180,000, output 541,640) — the
// cache and output ports, and NOT the filter. Building four filter lanes for a
// workload the filter does not bind would be twelve DSPs spent on the smallest
// term in the maximum.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT CHANGE
// ---------------------------------------------------------------------------
// No texture semantics. The planner arithmetic below — level selection, the
// closed-form level offset, the UV shift, the half-texel bias, `wrap_coord`'s
// three folds, row-major addressing and `decode16` — is TAKEN VERBATIM from
// the serial block. It is staged across registers, not rewritten. Any
// difference in a sampled value is a defect in this file, and the shared test
// suite is what says so.
`default_nettype none

module zhao_texture_tmu_pipe #(
    // Resident palette pages. 256 RGB565 entries is 4,096 bits a slot, so 16
    // slots is 64 Kbit. Sixteen rather than the serial block's two because the
    // working set is real: four resident terrain tilesets already imply four
    // palettes, the sky needs one, up to two near stars carry live palettes,
    // and creatures swap pages. Two slots prove a mechanism, not a working set.
    parameter int unsigned PAL_SLOTS = 16,
    // Records in flight. Retirement is strictly in acceptance order, so this
    // is the amount of latency the block can hide, not a reordering window.
    parameter int unsigned ROB_N = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the request port, identical to the serial block -------------------
    input  var logic        req_valid_i,
    input  var logic        pal_inv_valid_i,
    output var logic        req_ready_o,
    input  var logic [31:0] req_u_i,
    input  var logic [31:0] req_v_i,
    input  var logic [31:0] req_base_i,
    input  var logic [31:0] req_pal_base_i,
    input  var logic [31:0] req_mode_i,
    input  var logic [ 7:0] req_lod_i,
    input  var logic [15:0] req_src_id_i,

    // ---- TEXTURE.CACHE -----------------------------------------------------
    output var logic         cac_valid_o,
    input  var logic         cac_ready_i,
    output var logic [  3:0] cac_en_o,
    output var logic [127:0] cac_addr_o,
    output var logic [ 15:0] cac_src_id_o,
    input  var logic         cac_valid_i,
    output var logic         cac_ready_o,
    input  var logic [ 63:0] cac_data_i,

    // ---- the sample --------------------------------------------------------
    output var logic        smp_valid_o,
    input  var logic        smp_ready_i,
    output var logic [23:0] smp_rgb_o,
    output var logic [ 7:0] smp_a_o,
    output var logic [ 7:0] smp_idx_o,
    output var logic [15:0] smp_src_id_o,

    output var logic        mode_error_o,
    output var logic        idle_o,
    output var logic [31:0] texture_samples_o,

    // ---- evidence: every non-progress clock has exactly one reason ---------
    output var logic [31:0] rob_full_clocks_o,
    output var logic [31:0] cache_wait_clocks_o,
    output var logic [31:0] filter_busy_clocks_o,
    output var logic [31:0] out_stall_clocks_o,
    output var logic [31:0] pal_fallback_o
);

  // ==========================================================================
  // the mode word and the planner, verbatim from the serial block
  // ==========================================================================
  localparam logic [2:0] FMT_CLUT8 = 3'd0, FMT_RGB565 = 3'd1, FMT_CLUT4 = 3'd2;
  localparam logic [2:0] FMT_ARGB1555 = 3'd3, FMT_ARGB4444 = 3'd4;
  localparam logic [1:0] WRAP_REPEAT = 2'd0, WRAP_CLAMP = 2'd1, WRAP_MIRROR = 2'd2;

  // REP4[L] = (4^L - 1)/3, the base-4 repunits, COPIED FROM THE SERIAL BLOCK
  // RATHER THAN GENERATED. The first version of this file computed it as
  // `32'h5555_5555 >> (2 * (15 - k))`, which is off by one at every entry: it
  // gives REP4[0] = 1 where the law says 0 and REP4[15] = 1,431,655,765 where
  // the law says 357,913,941. Every mip level above zero then addressed the
  // wrong level offset, the texel came back as a zero byte, and the record
  // retired with a CLUT index of 0.
  //
  // This block's whole claim is that its arithmetic is the serial block's,
  // staged rather than rewritten. A cleverer expression for a table that is
  // already written down is not staging, it is rewriting.
  localparam logic [31:0] REP4 [0:15] = '{
    32'd0,       32'd1,        32'd5,        32'd21,
    32'd85,      32'd341,      32'd1365,     32'd5461,
    32'd21845,   32'd87381,    32'd349525,   32'd1398101,
    32'd5592405, 32'd22369621, 32'd89478485, 32'd357913941
  };

  // ---- A0: capture ---------------------------------------------------------
  // Nothing downstream reads a request pin. This is the register that turns a
  // pin-to-output continent into a pipeline, and it is the whole timing story.
  logic        a0_v;
  logic [31:0] a0_u, a0_v_coord, a0_base, a0_pal, a0_mode;
  logic [ 7:0] a0_lod;
  logic [15:0] a0_src;
  logic [$clog2(ROB_N)-1:0] a0_rec;

  logic [2:0] m_fmt;
  logic       m_filter, m_mip_en;
  logic [1:0] m_wrap_u, m_wrap_v;
  logic [3:0] m_log2w, m_log2h, m_maxlvl;
  logic [10:0] m_rsvd;
  always_comb begin
    m_fmt    = a0_mode[2:0];
    m_filter = a0_mode[3];
    m_wrap_u = a0_mode[5:4];
    m_wrap_v = a0_mode[7:6];
    m_log2w  = a0_mode[11:8];
    m_log2h  = a0_mode[15:12];
    m_maxlvl = a0_mode[19:16];
    m_mip_en = a0_mode[20];
    m_rsvd   = a0_mode[31:21];
  end

  logic is_clut, is_16bpp, fmt_bad;
  always_comb begin
    is_clut  = (m_fmt == FMT_CLUT8) || (m_fmt == FMT_CLUT4);
    is_16bpp = (m_fmt == FMT_RGB565) || (m_fmt == FMT_ARGB1555) || (m_fmt == FMT_ARGB4444);
    fmt_bad  = !is_clut && !is_16bpp;
  end

  logic chain_max_lt, err_c, filter_eff;
  logic [3:0] chain_max, lvl_cap;
  always_comb begin
    chain_max    = (m_log2w < m_log2h) ? m_log2w : m_log2h;
    chain_max_lt = (m_maxlvl > chain_max);
    lvl_cap      = chain_max_lt ? chain_max : m_maxlvl;
    filter_eff   = m_filter && !is_clut;   // a palette is never filtered
    err_c        = (m_filter && is_clut) || (m_rsvd != 11'd0) || chain_max_lt || fmt_bad;
  end

  logic [3:0] lvl_req, level, log2w_l, log2h_l;
  logic [31:0] mask_u, mask_v, size_u, lvl_off;
  logic [5:0]  lvl_shift;
  always_comb begin
    lvl_req   = m_mip_en ? a0_lod[7:4] : 4'd0;
    level     = (lvl_req > lvl_cap) ? lvl_cap : lvl_req;
    log2w_l   = m_log2w - level;
    log2h_l   = m_log2h - level;
    size_u    = 32'd1 << log2w_l;
    mask_u    = size_u - 32'd1;
    mask_v    = (32'd1 << log2h_l) - 32'd1;
    lvl_shift = 6'({4'd0, m_log2w} + {4'd0, m_log2h}) - 6'({4'd0, (level - 4'd1)} << 1);
    lvl_off   = (level == 4'd0) ? 32'd0 : (REP4[level] << lvl_shift);
  end

  logic signed [47:0] tu_q, tv_q, tu_b, tv_b;
  logic signed [31:0] iu0, iv0;
  logic [7:0]         fu, fv;
  always_comb begin
    tu_q = $signed({{16{a0_u[31]}}, a0_u}) <<< log2w_l;
    tv_q = $signed({{16{a0_v_coord[31]}}, a0_v_coord}) <<< log2h_l;
    tu_b = filter_eff ? (tu_q - 48'sd32768) : tu_q;
    tv_b = filter_eff ? (tv_q - 48'sd32768) : tv_q;
    iu0  = tu_b[47:16];
    iv0  = tv_b[47:16];
    fu   = filter_eff ? tu_b[15:8] : 8'd0;
    fv   = filter_eff ? tv_b[15:8] : 8'd0;
  end

  function automatic logic [31:0] wrap_coord(input logic signed [31:0] t,
                                             input logic [1:0]         mode,
                                             input logic [31:0]        mask);
    logic [31:0] tu_, per, lo_;
    begin
      tu_ = $unsigned(t);
      case (mode)
        WRAP_CLAMP:  wrap_coord = t[31] ? 32'd0 : ((tu_ > mask) ? mask : tu_);
        WRAP_MIRROR: begin
          per = tu_ & ((mask << 1) | 32'd1);
          lo_ = per & mask;
          wrap_coord = (per > mask) ? (mask - lo_) : lo_;
        end
        WRAP_REPEAT: wrap_coord = tu_ & mask;
        default:     wrap_coord = tu_ & mask;
      endcase
    end
  endfunction

  // EACH UNIQUE COORDINATE IS WRAPPED ONCE. The serial block calls
  // `wrap_coord` eight times and shifts four rows, although a four-tap
  // footprint has only two distinct U, two distinct V and two distinct rows.
  logic [31:0] uw0, uw1, vw0, vw1, row0, row1;
  always_comb begin
    uw0  = wrap_coord(iu0, m_wrap_u, mask_u);
    uw1  = wrap_coord(iu0 + 32'd1, m_wrap_u, mask_u);
    vw0  = wrap_coord(iv0, m_wrap_v, mask_v);
    vw1  = wrap_coord(iv0 + 32'd1, m_wrap_v, mask_v);
    row0 = vw0 << log2w_l;
    row1 = vw1 << log2w_l;
  end

  logic [31:0] total [0:3];
  logic [31:0] addr  [0:3];
  always_comb begin
    total[0] = lvl_off + row0 + uw0;
    total[1] = lvl_off + row0 + uw1;
    total[2] = lvl_off + row1 + uw0;
    total[3] = lvl_off + row1 + uw1;
    for (int unsigned k = 0; k < 4; k++)
      addr[k] = is_16bpp ? (a0_base + (total[k] << 1))
              : (m_fmt == FMT_CLUT4) ? (a0_base + (total[k] >> 1))
              : (a0_base + total[k]);
  end

  function automatic logic [31:0] decode16(input logic [15:0] h, input logic [2:0] fmt);
    logic [7:0] a_, r_, g_, b_;
    begin
      case (fmt)
        FMT_ARGB1555: begin
          a_ = h[15] ? 8'd255 : 8'd0;
          r_ = {h[14:10], h[14:12]};
          g_ = {h[9:5], h[9:7]};
          b_ = {h[4:0], h[4:2]};
        end
        FMT_ARGB4444: begin
          a_ = {h[15:12], h[15:12]};
          r_ = {h[11:8], h[11:8]};
          g_ = {h[7:4], h[7:4]};
          b_ = {h[3:0], h[3:0]};
        end
        default: begin
          a_ = 8'd255;
          r_ = {h[15:11], h[15:13]};
          g_ = {h[10:5], h[10:9]};
          b_ = {h[4:0], h[4:2]};
        end
      endcase
      decode16 = {a_, r_, g_, b_};
    end
  endfunction

  // ==========================================================================
  // A1: the registered cache issue bundle
  // ==========================================================================
  // Held bit-stable until the cache accepts it. TEXTURE.CACHE's first-look
  // accounting assumes an offered access does not mutate before ready.
  logic         c_v;
  logic [  3:0] c_en;
  logic [127:0] c_addr;
  logic [ 15:0] c_src;
  logic [  2:0] c_fmt;
  logic         c_clut, c_filt, c_bytesel, c_nib, c_err;
  logic [  7:0] c_fu, c_fv;
  logic [ 31:0] c_pal;
  logic [$clog2(ROB_N)-1:0] c_rec;

  assign cac_valid_o  = c_v;
  assign cac_en_o     = c_en;
  assign cac_addr_o   = c_addr;
  assign cac_src_id_o = c_src;
  // A RESPONSE IS TAKEN UNLESS THE FILTER LANE IS STILL HOLDING ONE. There is
  // one channel lane, so a second filtered footprint arriving while the first
  // is mid-walk would overwrite `fl_rec` and its record would never complete --
  // the ROB head then waits forever on a filter that has forgotten it. That is
  // exactly what happened: the direct batch never finished while CLUT ran at 3
  // clocks a sample.
  //
  // Stalling the response port rather than the request port is the right place:
  // the filter always drains in at most four clocks with no external
  // dependency, so this cannot deadlock, and a nearest sample queued behind it
  // waits four clocks rather than being refused at the front door.
  assign cac_ready_o  = !fl_v;

  // ==========================================================================
  // the in-flight tag FIFO
  // ==========================================================================
  // TEXTURE.CACHE returns in ACCEPTANCE ORDER, so the record a response
  // belongs to is the oldest one still outstanding. That is a FIFO and not a
  // tag comparison — and it is an INTERNAL record id, never `src_id`, because
  // a source id can repeat on adjacent requests and this project has shipped
  // that bug before.
  logic [$clog2(ROB_N)-1:0] tagq   [ROB_N];
  // Which KIND of access each outstanding response belongs to. A CLUT sample
  // whose page is not resident makes a SECOND access for the palette entry, and
  // the two come back through the same port in the same order.
  logic                     tagp   [ROB_N];
  logic [$clog2(ROB_N):0]   tag_wp, tag_rp;
  logic                     tag_ne;
  assign tag_ne = (tag_wp != tag_rp);

  // ==========================================================================
  // the resident palette
  // ==========================================================================
  logic [31:0]  pal_tag_r [PAL_SLOTS];
  logic         pal_ten_r [PAL_SLOTS];
  logic [255:0] pal_val_r [PAL_SLOTS];
  logic [15:0]  pal_dat_r [PAL_SLOTS][256];
  localparam int PW = (PAL_SLOTS <= 1) ? 1 : $clog2(PAL_SLOTS);
  logic [PW-1:0] pal_vic_r;

  // ==========================================================================
  // the reorder buffer
  // ==========================================================================
  localparam int RW = (ROB_N <= 1) ? 1 : $clog2(ROB_N);
  logic         rb_used [ROB_N];
  logic         rb_done [ROB_N];
  // THE RECORD CARRIES ITS OWN PLAN. A response must be steered by the plan of
  // the request that caused it, and the single `c_*` issue register has moved
  // on by then -- that is the whole reason a pipelined sampler needs a record
  // and a serial one does not.
  logic [ 2:0]  rb_fmt  [ROB_N];
  logic         rb_clut [ROB_N];
  logic         rb_filt [ROB_N];
  logic         rb_bsel [ROB_N];
  logic         rb_nib  [ROB_N];
  logic [31:0]  rb_pal  [ROB_N];
  logic [ 7:0]  rb_fu   [ROB_N];
  logic [ 7:0]  rb_fv   [ROB_N];
  logic [23:0]  rb_rgb  [ROB_N];
  logic [ 7:0]  rb_a    [ROB_N];
  logic [ 7:0]  rb_idx  [ROB_N];
  logic [15:0]  rb_src  [ROB_N];
  logic         rb_err  [ROB_N];
  logic [RW:0]  rb_wp, rb_rp;
  logic [RW:0]  rb_occ;
  assign rb_occ = rb_wp - rb_rp;

  logic rob_full_c, accept_c;
  assign rob_full_c = (rb_occ >= (RW+1)'(ROB_N));
  // A REQUEST IS ACCEPTED EVERY CLOCK THERE IS ROOM. That is the whole point
  // of the rebuild: the serial block's `req_ready_o = (st_r == ST_IDLE)` is
  // what held CLUT to five clocks a sample however fast its arithmetic was.
  // Refused while a cold palette fetch is pending, because that fetch owns the
  // cache issue register, and while the single filter lane is busy, because a
  // second footprint would have nowhere to sit.
  assign req_ready_o = !rob_full_c && !pf_v && (!a0_v || !c_v || cac_ready_i);
  assign accept_c    = req_valid_i && req_ready_o;

  assign idle_o = (rb_occ == '0) && !a0_v && !c_v;

  // WHEN A0'S REQUEST ACTUALLY LEAVES. The issue register can be free while a
  // cold palette fetch is taking it, and A0 must NOT be cleared then -- its
  // plan has not been issued and clearing it drops the request silently. That
  // is one condition written once, because the first version had it spelled
  // out separately at the clear site and at the issue site and they disagreed:
  // 30 of 32 requests in a batch vanished.
  logic issue_fire_c;
  assign issue_fire_c = a0_v && !pf_v && (!c_v || cac_ready_i);

  // ---- the mode verdict, taken from the PINS at accept --------------------
  // `mode_error_o` belongs to the request accepted on the previous clock --
  // that is the relationship the serial block has and the shared harness reads.
  // Deriving it from the CAPTURED mode instead makes it one clock later than
  // the harness expects, and all five mode-error checks fail while every
  // sampled value is right.
  //
  // This is the one thing allowed to look at a request pin, and it is allowed
  // because it is not the deep arithmetic the A0 register exists to cut off:
  // a handful of comparisons on the mode word into a flop. The addresses still
  // come from the captured copy.
  logic [2:0]  p_fmt;
  logic [3:0]  p_log2w, p_log2h, p_maxlvl;
  logic        p_is_clut, p_fmt_bad, err_pin_c;
  always_comb begin
    p_fmt     = req_mode_i[2:0];
    p_log2w   = req_mode_i[11:8];
    p_log2h   = req_mode_i[15:12];
    p_maxlvl  = req_mode_i[19:16];
    p_is_clut = (p_fmt == FMT_CLUT8) || (p_fmt == FMT_CLUT4);
    p_fmt_bad = !p_is_clut && (p_fmt != FMT_RGB565) && (p_fmt != FMT_ARGB1555) &&
                (p_fmt != FMT_ARGB4444);
    err_pin_c = (req_mode_i[3] && p_is_clut)
              || (req_mode_i[31:21] != 11'd0)
              || (p_maxlvl > ((p_log2w < p_log2h) ? p_log2w : p_log2h))
              || p_fmt_bad;
  end

  // ---- the head retires ----------------------------------------------------
  logic [RW-1:0] head_c;
  assign head_c      = rb_rp[RW-1:0];
  assign smp_valid_o = rb_used[head_c] && rb_done[head_c];
  assign smp_rgb_o   = rb_rgb[head_c];
  assign smp_a_o     = rb_a[head_c];
  assign smp_idx_o   = rb_idx[head_c];
  assign smp_src_id_o= rb_src[head_c];

  // ==========================================================================
  // sequential
  // ==========================================================================
  // ---- the cold palette fetch -------------------------------------------
  // A CLUT sample whose page is resident completes in the response clock. One
  // whose page is not takes the second access the serial block always took,
  // and fills the page on the way past, so the next sample on that page is
  // free. `pf_v` holds that fetch; new requests are refused while it is
  // pending, which is what keeps the tag FIFO's order meaning what it says.
  logic        pf_v;
  logic [31:0] pf_addr;
  logic [ 7:0] pf_idx;
  logic [$clog2(ROB_N)-1:0] pf_rec;

  // ---- the filter lane ---------------------------------------------------
  // ONE channel per clock, which is what the workload asks for: 45,000 cloud
  // fragments x 4 channels is 180,000 channel jobs against 541,640 cache
  // accesses, so the filter is the smallest term in the resource maximum.
  logic        fl_v;
  logic [ 1:0] fl_ch;
  logic [ 7:0] fl_t [0:3][0:3];   // [channel][tap]
  logic [ 7:0] fl_fu, fl_fv;
  logic [ 7:0] fl_res [0:3];
  logic [$clog2(ROB_N)-1:0] fl_rec;
  logic        fl_argb;

  logic [7:0] bl_out;
  zhao_texture_bilerp u_bl (.t00_i(fl_t[fl_ch][0]),
                            .t10_i(fl_t[fl_ch][1]),
                            .t01_i(fl_t[fl_ch][2]),
                            .t11_i(fl_t[fl_ch][3]),
                            .fu_i (fl_fu),
                            .fv_i (fl_fv),
                            .out_o(bl_out));

  // ---- what the response means -------------------------------------------
  logic [$clog2(ROB_N)-1:0] rsp_rec;
  logic                     rsp_is_pal;
  assign rsp_rec    = tagq[tag_rp[RW-1:0]];
  assign rsp_is_pal = tagp[tag_rp[RW-1:0]];

  // The CLUT index the returned byte names, and where its palette entry lives.
  logic [7:0]  rsp_byte, rsp_idx;
  logic [31:0] rsp_pal_addr;
  always_comb begin
    rsp_byte = rb_bsel[rsp_rec] ? cac_data_i[15:8] : cac_data_i[7:0];
    rsp_idx  = (rb_fmt[rsp_rec] == FMT_CLUT4)
             ? (rb_nib[rsp_rec] ? {4'd0, rsp_byte[7:4]} : {4'd0, rsp_byte[3:0]})
             : rsp_byte;
    rsp_pal_addr = rb_pal[rsp_rec] + {23'd0, rsp_idx, 1'b0};
  end

  // Is this record's palette page resident, and where?
  logic pal_hit_c, pal_ent_c;
  logic [PW-1:0] pal_way_c;
  always_comb begin
    pal_hit_c = 1'b0;
    pal_ent_c = 1'b0;
    pal_way_c = '0;
    for (int unsigned w = 0; w < PAL_SLOTS; w++)
      if (pal_ten_r[w] && (pal_tag_r[w] == rb_pal[rsp_rec])) begin
        pal_hit_c = 1'b1;
        pal_way_c = PW'(w);
        pal_ent_c = pal_val_r[w][rsp_idx];
      end
  end

  // ---- the decodes, NAMED ---------------------------------------------------
  // Quartus 17.0.2 cannot part-select a FUNCTION CALL RESULT:
  // `decode16(h, fmt)[23:0]` is a syntax error there while Verilator and slang
  // both accept it, so it only surfaced in the composed fit. Naming the decodes
  // is the fix and also stops the identical call being written twice for the
  // colour and the alpha of the same texel.
  logic [31:0] dec_pal565_c;      // a palette page word, always RGB565
  logic [31:0] dec_clut565_c;     // a CLUT entry from the resident page
  logic [31:0] dec_direct_c;      // direct nearest, in the record's own format
  logic [31:0] dec_tap_c [0:3];   // the four bilinear taps
  always_comb begin
    dec_pal565_c  = decode16(cac_data_i[15:0], FMT_RGB565);
    dec_clut565_c = decode16(pal_dat_r[pal_way_c][rsp_idx], FMT_RGB565);
    dec_direct_c  = decode16(cac_data_i[15:0], rb_fmt[rsp_rec]);
    for (int unsigned k = 0; k < 4; k++)
      dec_tap_c[k] = decode16(cac_data_i[16*k +: 16], rb_fmt[rsp_rec]);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      a0_v <= 1'b0;
      c_v  <= 1'b0;
      a0_rec <= '0;
      tag_wp <= '0;
      tag_rp <= '0;
      rb_wp <= '0;
      rb_rp <= '0;
      pal_vic_r <= '0;
      pf_v <= 1'b0;
      fl_v <= 1'b0;
      fl_ch <= 2'd0;
      mode_error_o <= 1'b0;
      texture_samples_o <= 32'd0;
      rob_full_clocks_o <= 32'd0;
      cache_wait_clocks_o <= 32'd0;
      filter_busy_clocks_o <= 32'd0;
      out_stall_clocks_o <= 32'd0;
      pal_fallback_o <= 32'd0;
      for (int unsigned k = 0; k < ROB_N; k++) begin
        rb_used[k] <= 1'b0;
        rb_done[k] <= 1'b0;
      end
      for (int unsigned w = 0; w < PAL_SLOTS; w++) begin
        pal_ten_r[w] <= 1'b0;
        pal_tag_r[w] <= 32'd0;
        pal_val_r[w] <= 256'd0;
      end
    end else begin
      // ---- counters, each for exactly one reason -------------------------
      if (req_valid_i && rob_full_c)                 rob_full_clocks_o <= rob_full_clocks_o + 32'd1;
      if (c_v && !cac_ready_i)                       cache_wait_clocks_o <= cache_wait_clocks_o + 32'd1;
      if (smp_valid_o && !smp_ready_i)               out_stall_clocks_o <= out_stall_clocks_o + 32'd1;

      // ---- A0 -------------------------------------------------------------
      if (accept_c) begin
        a0_v       <= 1'b1;
        a0_u       <= req_u_i;
        a0_v_coord <= req_v_i;
        a0_base    <= req_base_i;
        a0_pal     <= req_pal_base_i;
        a0_mode    <= req_mode_i;
        a0_lod     <= req_lod_i;
        a0_src     <= req_src_id_i;
        a0_rec     <= rb_wp[RW-1:0];
        // The ROB entry is claimed at ACCEPT, which is what makes retirement
        // acceptance-ordered without any sorting later.
        rb_used[rb_wp[RW-1:0]] <= 1'b1;
        rb_done[rb_wp[RW-1:0]] <= 1'b0;
        rb_src [rb_wp[RW-1:0]] <= req_src_id_i;
        rb_wp <= rb_wp + (RW+1)'(1);
      end else if (issue_fire_c) begin
        a0_v <= 1'b0;
      end

      // `mode_error_o` is the ACCEPTED request's verdict, one clock later --
      // the same relationship the serial block has, so the shared harness
      // reads it the same way.
      // A PULSE, not a level. It is high for exactly the clock after the
      // accept it belongs to. The serial block can hold it because it accepts
      // one request at a time and the next accept overwrites it; a pipeline
      // accepting every clock would leave a stale verdict standing over
      // requests that did not earn it, which the harness reports as
      // "mode_error_o pulsed with no request to attribute it to".
      mode_error_o <= accept_c && err_pin_c;

      // ---- A1 -> the cache issue register ---------------------------------
      if (!c_v || cac_ready_i) begin
        // THE COLD PALETTE FETCH OUTRANKS A NEW REQUEST. It belongs to a record
        // that is already in the ROB and ahead of anything still being
        // accepted, and leaving it behind a stream of new texel accesses would
        // let the head of the ROB wait on the one access nobody is issuing.
        if (pf_v) begin
          c_v    <= 1'b1;
          c_en   <= 4'b0001;
          c_addr <= {96'd0, pf_addr};
          c_src  <= rb_src[pf_rec];
          tagq[tag_wp[RW-1:0]] <= pf_rec;
          tagp[tag_wp[RW-1:0]] <= 1'b1;
          tag_wp <= tag_wp + (RW+1)'(1);
          pf_v   <= 1'b0;
        end else begin
          c_v <= a0_v;
        end
        if (issue_fire_c) begin
          c_en      <= filter_eff ? 4'b1111 : 4'b0001;
          c_addr    <= {addr[3], addr[2], addr[1], addr[0]};
          c_src     <= a0_src;
          c_fmt     <= fmt_bad ? FMT_RGB565 : m_fmt;
          c_clut    <= is_clut;
          c_filt    <= filter_eff;
          c_fu      <= fu;
          c_fv      <= fv;
          c_bytesel <= addr[0][0];
          c_nib     <= total[0][0];
          c_pal     <= a0_pal;
          c_rec     <= a0_rec;
          c_err     <= err_c;
          tagq[tag_wp[RW-1:0]] <= a0_rec;
          tagp[tag_wp[RW-1:0]] <= 1'b0;
          tag_wp <= tag_wp + (RW+1)'(1);
          // The plan travels with the record, not with the issue register.
          rb_fmt [a0_rec] <= fmt_bad ? FMT_RGB565 : m_fmt;
          rb_clut[a0_rec] <= is_clut;
          rb_filt[a0_rec] <= filter_eff;
          rb_bsel[a0_rec] <= addr[0][0];
          rb_nib [a0_rec] <= total[0][0];
          rb_pal [a0_rec] <= a0_pal;
          rb_fu  [a0_rec] <= fu;
          rb_fv  [a0_rec] <= fv;
          rb_idx [a0_rec] <= 8'd0;
          rb_a   [a0_rec] <= 8'd0;
          rb_rgb [a0_rec] <= 24'd0;
        end
      end

      // ---- the cache response ---------------------------------------------
      // A RESPONSE IS CONSUMED ONLY ON ITS OWN HANDSHAKE. This read
      // `cac_valid_i && tag_ne` and ignored `cac_ready_o` -- so every clock the
      // filter held the port closed, the SAME response was popped again and
      // applied to the NEXT record. CLUT never noticed, because CLUT never
      // stalls the port; the bilinear batch hung outright.
      if (cac_valid_i && cac_ready_o && tag_ne) begin
        tag_rp <= tag_rp + (RW+1)'(1);

        if (rsp_is_pal) begin
          // The cold fetch came back. Complete the record and FILL the page, so
          // the next sample on this index never makes this access again.
          rb_rgb [rsp_rec] <= dec_pal565_c[23:0];
          rb_a   [rsp_rec] <= 8'd255;
          rb_done[rsp_rec] <= 1'b1;
          if (pal_hit_c) begin
            pal_dat_r[pal_way_c][rb_idx[rsp_rec]] <= cac_data_i[15:0];
            pal_val_r[pal_way_c][rb_idx[rsp_rec]] <= 1'b1;
          end else begin
            // A slot holds ONE page, so claiming it clears the previous
            // occupant's entries -- a stale bit would be served as this page's
            // colour.
            pal_tag_r[pal_vic_r]                     <= rb_pal[rsp_rec];
            pal_ten_r[pal_vic_r]                     <= 1'b1;
            pal_val_r[pal_vic_r]                     <= 256'd0;
            pal_val_r[pal_vic_r][rb_idx[rsp_rec]]    <= 1'b1;
            pal_dat_r[pal_vic_r][rb_idx[rsp_rec]]    <= cac_data_i[15:0];
            pal_vic_r <= (PAL_SLOTS == 1) ? '0
                       : ((pal_vic_r == PW'(PAL_SLOTS - 1)) ? '0 : pal_vic_r + PW'(1));
          end
        end else if (rb_clut[rsp_rec]) begin
          // A CLUT texel. The raw index is reported whatever happens next: the
          // fragment's alpha test is an INDEX test and its glow strength is the
          // texel's CLUT intensity, so the palette colour alone answers neither.
          rb_idx[rsp_rec] <= rsp_idx;
          rb_a  [rsp_rec] <= 8'd255;
          if (pal_hit_c && pal_ent_c) begin
            rb_rgb [rsp_rec] <= dec_clut565_c[23:0];
            rb_done[rsp_rec] <= 1'b1;
          end else begin
            pf_v    <= 1'b1;
            pf_addr <= rsp_pal_addr;
            pf_idx  <= rsp_idx;
            pf_rec  <= rsp_rec;
            pal_fallback_o <= pal_fallback_o + 32'd1;
          end
        end else if (!rb_filt[rsp_rec]) begin
          // Direct nearest: the texel is the answer. No filter pass, no DSP.
          rb_rgb [rsp_rec] <= dec_direct_c[23:0];
          rb_a   [rsp_rec] <= dec_direct_c[31:24];
          rb_done[rsp_rec] <= 1'b1;
        end else begin
          // Direct bilinear: hand the four decoded taps to the channel lane.
          for (int unsigned k = 0; k < 4; k++) begin
            fl_t[0][k] <= dec_tap_c[k][23:16];
            fl_t[1][k] <= dec_tap_c[k][15:8];
            fl_t[2][k] <= dec_tap_c[k][7:0];
            fl_t[3][k] <= dec_tap_c[k][31:24];
          end
          fl_fu   <= rb_fu[rsp_rec];
          fl_fv   <= rb_fv[rsp_rec];
          fl_rec  <= rsp_rec;
          fl_argb <= (rb_fmt[rsp_rec] == FMT_ARGB1555) || (rb_fmt[rsp_rec] == FMT_ARGB4444);
          fl_ch   <= 2'd0;
          fl_v    <= 1'b1;
        end
      end

      // ---- the filter lane, one channel a clock ---------------------------
      // RGB565's alpha is always exactly 255, so filtering four identical 255
      // taps to prove it is a job this lane does not run.
      if (fl_v) begin
        filter_busy_clocks_o <= filter_busy_clocks_o + 32'd1;
        fl_res[fl_ch] <= bl_out;
        if (fl_ch == 2'd2) begin
          if (fl_argb) begin
            fl_ch <= 2'd3;
          end else begin
            rb_rgb [fl_rec] <= {fl_res[0], fl_res[1], bl_out};
            rb_a   [fl_rec] <= 8'd255;
            rb_done[fl_rec] <= 1'b1;
            fl_v <= 1'b0;
          end
        end else if (fl_ch == 2'd3) begin
          rb_rgb [fl_rec] <= {fl_res[0], fl_res[1], fl_res[2]};
          rb_a   [fl_rec] <= bl_out;
          rb_done[fl_rec] <= 1'b1;
          fl_v <= 1'b0;
        end else begin
          fl_ch <= fl_ch + 2'd1;
        end
      end

      // ---- palette invalidate, last so it beats a fill in the same clock --
      if (pal_inv_valid_i)
        for (int unsigned w = 0; w < PAL_SLOTS; w++) begin
          pal_ten_r[w] <= 1'b0;
          pal_val_r[w] <= 256'd0;
        end

      // ---- retire ----------------------------------------------------------
      if (smp_valid_o && smp_ready_i) begin
        rb_used[head_c] <= 1'b0;
        rb_done[head_c] <= 1'b0;
        rb_rp <= rb_rp + (RW+1)'(1);
        if (texture_samples_o != 32'hFFFF_FFFF)
          texture_samples_o <= texture_samples_o + 32'd1;
      end
    end
  end

  logic unused_ok;
  // The issue register's plan copies are dead now that the RECORD carries the
  // plan; they are kept because the cache bundle is formed from them and the
  // linter should say so rather than be waived.
  assign unused_ok = |{size_u, c_bytesel, c_nib, c_fmt, c_clut, c_filt,
                       c_fu, c_fv, c_pal, c_rec, c_err, pf_idx, 1'b0};

endmodule : zhao_texture_tmu_pipe

`default_nettype wire
