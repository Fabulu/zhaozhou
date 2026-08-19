// zhao_texture_aux.sv — TEXTURE.AUX: the restricted auxiliary source (phase 6,
// ZH-060, cut-order 2).
//
// Law, in citation order:
//   design/contracts/TEXTURE.AUX.md — the block contract.
//   design/blocks.yml — `inputs: [aux_requests]`, `outputs: [aux_samples]`,
//       `upstream: [RASTER.FRAGMENT]`, `backpressure: ready_valid`,
//       `latency: variable_bounded:8`, "1 aux sample per clock", counter
//       `texture_samples`, `source_ids: true`, `cut_order: 2`, and the purpose
//       line this file is built around: "Restricted aux texel source (surface
//       sheets, light/shadow compare, distortion) — deliberately NOT a general
//       second TMU (26)."
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md 15 — the aux bullet list, verbatim:
//       "Restricted auxiliary source: terrain surface sheet; light/mask map;
//        shadow compare; distortion map. It must not become a second
//        unrestricted full TMU."
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md 26 — "a second unrestricted TMU"
//       is REFUSED; cut order 2 is "auxiliary filtering"; and "terrain surface
//       sheets" is on the NEVER CUT list. See WHAT 26 COSTS THIS FILE.
//   spec/terrain_rules.md 6.5 — "Surface sheet (layer F) stays the
//       restricted-aux sample (charter 15 aux list) for tag/strength effects —
//       the aux budget holds: ONE aux consumer on terrain fragments, because
//       tint moved to vertices."
//   reference/src/zrender/terrain.cpp `zref::render::sample_sheet` — THE
//       ratified world -> texel mapping, executed by the software console
//       every frame. Reproduced here line for line.
//   reference/include/zref/zref_aux.hpp `zref::AuxSource` — the oracle. It did
//       not exist before 2026-08-19; that header says so and argues every
//       chosen rule a second time.
//   fpga/rtl/surface/zhao_surface_sheet.sv — the resident layer-F store, and
//       the port this block masters. Its header sizes its slots at "2 = one
//       being stamped while one is being sampled, which is the smallest set
//       that does not serialise the two consumers". THIS BLOCK IS THAT SECOND
//       CONSUMER; the sheet was built expecting it and never named it.
//
// ---------------------------------------------------------------------------
// WHAT 26 COSTS THIS FILE, WHICH IS THE POINT OF THE WHOLE FILE
// ---------------------------------------------------------------------------
// The charter refuses "a second unrestricted full TMU" and the ledger repeats
// the refusal in this block's own purpose line. design/contracts/TEXTURE.TMU.md
// already read that refusal correctly for the primary side — "every sampling
// mode the machine will ever have has to fit through this request channel,
// because there is nowhere else for one to live" — and made nearest a SPECIAL
// CASE of bilinear rather than a second datapath. This file is the mirror
// image of that reasoning:
//
//     THIS SOURCE RETURNS BYTES AND NEVER INTERPRETS THEM.
//
// All four of 15's aux uses are the same operation, "read a byte pair out of a
// resident 64x64 page at a world position":
//   * terrain surface sheet — layer F's {tag, strength} (terrain_rules 6.5);
//   * light/mask map        — the strength byte IS the mask;
//   * shadow compare        — the COMPARE is RASTER.FRAGMENT's, which already
//                             owns a threshold test (its contract's "the alpha
//                             test is an INDEX test"). A comparator here would
//                             be the first component of a second sampler;
//   * distortion map        — the offset arithmetic belongs to whoever
//                             perturbs a coordinate, not to the fetch.
//
// So the file contains NO mode word, NO format decoder, NO palette, NO mip
// selector, NO wrap function and NO FILTER. That is a statement about THIS
// SOURCE TEXT and its port list, which a reader checks by reading them — not a
// runtime invariant, and there is nothing for a test to enforce. Its
// consequence is what matters: cut-order 2 is "auxiliary filtering", there is
// none here to cut, so this block already sits at its cut-order-2 floor; and
// "terrain surface sheets" is on the NEVER CUT list, so what remains is
// exactly what must survive. Both halves of 26 are met without a conditional.
//
// What IS here is one address generator — `sample_sheet`'s floor-across-the-
// envelope mapping — and the handshake that turns it into a sheet read.
//
// REJECTED ALTERNATIVE: a TMU-shaped mode word with a format field, a filter
// bit and per-axis wrap modes. It is the natural design, it would have made
// this block look like its sibling, and it is precisely what 26 forbids: every
// bit of such a word is a bit somebody later fills in, and the machine acquires
// its second unrestricted TMU one field at a time.
//
// ---------------------------------------------------------------------------
// THE DIVIDE, AND WHY IT IS SIX STEPS AND NOT THIRTY-NINE
// ---------------------------------------------------------------------------
// `sample_sheet`'s per-axis mapping is
//     i = ((s64)w - e0) * 64 / (e1 - e0),  then clamp to [0, 63]
// which is a division by a VARIABLE divisor — the widest arithmetic in the
// block by far. Done naively that is a 39-bit numerator over a 32-bit divisor,
// 39 restoring steps per axis, twice per sample.
//
// The clamp collapses it. Write N = (w - e0)*64 and D = e1 - e0 >= 1 (the
// degenerate envelope is rejected before any of this):
//
//   * N < 0  =>  the quotient is <= 0 and the clamp answers 0. C++ `/`
//     truncates TOWARD ZERO, so the quotient is 0 or negative and either way
//     the clamp lands on 0 — no floor/truncate correction is needed anywhere,
//     which is the one place the reference's truncation semantics visibly buy
//     something.
//   * N >= 64*D  =>  the quotient is >= 64 and the clamp answers 63. That is
//     ONE compare against D << 6, taken before any division.
//   * otherwise the quotient is in [0, 63] — SIX BITS — so six restoring steps
//     produce it exactly.
//
// One compare, then six steps, never thirty-nine, and the six run on both axes
// in parallel. Each step is the classic `if (r >= D<<k) { r -= D<<k; q[k] = 1; }`.
//
// ---------------------------------------------------------------------------
// LAWS FOUND (not invented)
// ---------------------------------------------------------------------------
// F1. THE MAPPING IS A FLOOR ACROSS THE ENVELOPE, NOT A TEXEL CENTRE. This is
//     deliberately NOT the rule SURFACE.STAMP uses on the write side
//     (wx = ex0 + (ex1-ex0)*(2i+1)/128, a texel CENTRE). The two mappings are
//     different on purpose, and design/contracts/SURFACE.STAMP.md names the
//     asymmetry as its own tripwire: "a u/v transposition or a dropped 64x
//     survives both standalone suites and dies there". No half-texel bias and
//     no rounding appear in this file.
// F2. THE *64 HAPPENS BEFORE THE DIVIDE. Dividing first would quantise to
//     whole envelopes. The numerator lane is therefore 40 bits, not 34.
// F3. THE CLAMP IS PER AXIS AND INDEPENDENT. A position outside the envelope
//     in x but inside in z samples the edge COLUMN at the correct ROW; it does
//     not fall back to a corner or to zero.
// F4. A DEGENERATE ENVELOPE ANSWERS ZERO AND READS NOTHING. `sample_sheet`
//     opens `if (ex1 <= ex0 || ez1 <= ez0) return 0;` — before any arithmetic
//     and before any sheet access. This block reproduces both halves: the
//     answer is {0,0} and NO read is issued, so a malformed envelope cannot
//     consume a sheet-port cycle.
//     ENFORCED-BY: tests/texture/texture_aux_directed.cpp:test_degenerate
// F5. LAYER F HAS TWO BYTES AND BOTH ARE RETURNED. terrain_rules 6.5 asks for
//     "tag/strength effects" (plural) and charter 12 spends both bytes;
//     SURFACE.SHEET's read port already hands back both. `sample_sheet`
//     returns strength alone only because its single caller
//     (terrain.cpp's `sheet_factor`) needs no more.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN (no spec states these; recorded as decisions with the rejected
// alternative — zref_aux.hpp argues each one a second time)
// ---------------------------------------------------------------------------
// A1. THE TEXEL SOURCE IS SURFACE.SHEET'S READ PORT, NOT TEXTURE.CACHE.
//     REJECTED ALTERNATIVE: arbitrating TEXTURE.CACHE's `acc_*` port. That
//     block is phase-5 frozen with exactly ONE access port, no MSHR and no
//     hit-under-miss ("the master IS the queue, and it is one deep"), and its
//     contract states that withdrawing an offered access and substituting a
//     different one before acceptance "is outside what this accounting
//     models" — so an arbiter would have to be transaction-atomic and would
//     park every aux sample behind up to four line fills. It would also return
//     halfwords with no tag/strength split, so the {tag, strength} pair
//     terrain_rules 6.5 asks for would have to be re-derived here. The sheet
//     port returns exactly the two bytes, needs no arbiter, and the sheet's own
//     header already sized its slots for a second consumer.
// A2. A NON-RESIDENT SHEET READS AS ZERO and raises `smp_miss_o`.
//     SURFACE.SHEET's found law is that "a sheet which has never been stamped
//     reads as ZERO everywhere", so an absent sheet reading zero paints the
//     same picture as an unstamped one — no scar, which is the truthful answer.
//     REJECTED ALTERNATIVE: stalling the fragment until the handle is
//     resident. SURFACE.SHEET NEVER EVICTS (its chosen law C2), so a
//     non-resident handle may never become resident and the stall would be
//     permanent — a fragment path deadlocked on a residency that is not coming.
//     ENFORCED-BY: tests/texture/texture_aux_directed.cpp:test_miss_reads_zero
// A3. THE ENVELOPE RIDES THE REQUEST rather than living in a register file.
//     REJECTED ALTERNATIVE: an envelope register file addressed by patch
//     handle. It is the right shape once a patch-descriptor cache exists;
//     today it would be a cache with one client and no filler, and it needs a
//     write port and an invalidate the ledger does not give this block.
// A4. ONE REQUEST IN FLIGHT, NO PIPELINE — the shape zhao_texture_tmu chose,
//     for the same reason: the sheet read cannot start until the divide
//     answers, and there is nothing to overlap it with. The cost is stated
//     rather than hidden: the sustained rate is ONE SAMPLE PER SIX CLOCKS
//     against the ledger's "1 aux sample per clock", and
//     design/contracts/TEXTURE.AUX.md's Target throughput section says so with
//     the measurement. terrain_rules 6.5's budget — ONE aux consumer per
//     terrain fragment — is what makes that survivable, and reaching one per
//     clock is a change to THIS FILE only (two divider copies and a fetch
//     stage), not to the ports or to the oracle.
//     ENFORCED-BY: tests/texture/texture_aux_directed.cpp:test_latency_bound
//
// Conservative SystemVerilog subset only (charter 2). Lint: clean under
// `-Wall` (lint_texture_aux).

module zhao_texture_aux (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // aux_requests in (RASTER.FRAGMENT). No mode word — see WHAT 26 COSTS
    // THIS FILE. The envelope rides the packet (A3).
    // -----------------------------------------------------------------------
    input  logic               req_valid_i,
    output logic               req_ready_o,
    input  logic signed [31:0] req_wx_i,      // fx16 world metres
    input  logic signed [31:0] req_wz_i,
    input  logic signed [31:0] req_env_x0_i,  // the patch envelope, fx16
    input  logic signed [31:0] req_env_z0_i,
    input  logic signed [31:0] req_env_x1_i,
    input  logic signed [31:0] req_env_z1_i,
    input  logic        [31:0] req_handle_i,  // handle32, the sheet to read
    input  logic        [15:0] req_src_id_i,

    // -----------------------------------------------------------------------
    // the SURFACE.SHEET read master (A1). Request out, response in — the
    // two-channel shape zhao_texture_tmu uses for TEXTURE.CACHE.
    // -----------------------------------------------------------------------
    output logic        shr_valid_o,
    input  logic        shr_ready_i,
    output logic [ 1:0] shr_op_o,      // always OpRead
    output logic [31:0] shr_handle_o,
    output logic [11:0] shr_texel_o,   // j*64 + i, the sheet's scan order
    output logic [15:0] shr_src_id_o,
    input  logic        shp_valid_i,
    output logic        shp_ready_o,
    input  logic [ 1:0] shp_status_i,  // StHit / StMiss
    input  logic [ 7:0] shp_tag_i,
    input  logic [ 7:0] shp_strength_i,

    // -----------------------------------------------------------------------
    // aux_samples out — BOTH layer-F bytes (F5), the texel they came from, and
    // the two reasons a sample can be empty.
    // -----------------------------------------------------------------------
    output logic        smp_valid_o,
    input  logic        smp_ready_i,
    output logic [ 7:0] smp_tag_o,
    output logic [ 7:0] smp_strength_o,
    output logic [ 5:0] smp_u_o,
    output logic [ 5:0] smp_v_o,
    output logic        smp_degenerate_o,  // F4: the envelope was degenerate
    output logic        smp_miss_o,        // A2: the sheet was not resident
    output logic [15:0] smp_src_id_o,

    // status and counters
    output logic        idle_o,
    output logic [31:0] texture_samples_o
);

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  // SURFACE.SHEET's opcode and status codes, mirrored (zhao_surface_sheet.sv).
  localparam logic [1:0] OpRead = 2'd1;
  localparam logic [1:0] StMiss = 2'd3;

  localparam logic [2:0] ST_IDLE = 3'd0;
  localparam logic [2:0] ST_DIV0 = 3'd1;  // restoring steps 5,4,3
  localparam logic [2:0] ST_DIV1 = 3'd2;  // restoring steps 2,1,0
  localparam logic [2:0] ST_REQ  = 3'd3;  // offer the sheet read
  localparam logic [2:0] ST_RSP  = 3'd4;  // take the sheet response
  localparam logic [2:0] ST_OUT  = 3'd5;  // present the sample

  // ---------------------------------------------------------------------------
  // widths, stated rather than assumed
  // ---------------------------------------------------------------------------
  // NUM_W: N = (w - e0) * 64. |w - e0| <= 2^32 - 1, so |N| < 2^38; 40 bits
  // leaves the sign and a bit of headroom and cannot wrap for any input word.
  // DEN_W: D = e1 - e0 with e1 > e0, both s32, so 1 <= D <= 2^32 - 1: 32 bits
  // unsigned, exactly.
  localparam int unsigned NUM_W = 40;
  localparam int unsigned DEN_W = 32;
  // The shifted divisor D << 5 is the widest thing a step subtracts, so the
  // remainder lane must hold it: 32 + 5 = 37, and the pre-step remainder is
  // below 64*D < 2^38. 39 bits covers both.
  localparam int unsigned REM_W = 39;

  // ---------------------------------------------------------------------------
  // the accepted request, registered
  // ---------------------------------------------------------------------------
  logic [ 2:0] st_r;
  logic [31:0] handle_r;
  logic [15:0] src_r;
  logic        degen_r;

  logic [REM_W-1:0] ru_r;  // the u-axis remainder
  logic [REM_W-1:0] rv_r;
  logic [DEN_W-1:0] du_r;  // the u-axis divisor, D = e1 - e0
  logic [DEN_W-1:0] dv_r;
  logic [ 5:0]      qu_r;  // the quotient, 6 bits by the clamp argument
  logic [ 5:0]      qv_r;
  logic             satu_r;  // N >= 64*D on this axis: the answer is 63
  logic             satv_r;

  logic [7:0] out_tag_r;
  logic [7:0] out_str_r;
  logic       out_miss_r;

  // ---------------------------------------------------------------------------
  // accept-time arithmetic: the numerator, the divisor and the two rails
  // ---------------------------------------------------------------------------
  logic signed [NUM_W-1:0] nu_c;
  logic signed [NUM_W-1:0] nv_c;
  logic        [DEN_W-1:0] du_c;
  logic        [DEN_W-1:0] dv_c;
  logic                    degen_c;
  logic                    negu_c, negv_c;  // N < 0: the clamp answers 0
  logic                    satu_c, satv_c;  // N >= 64*D: the clamp answers 63

  always_comb begin
    // F4: the degenerate test is the reference's own, and it gates everything.
    degen_c = (req_env_x1_i <= req_env_x0_i) || (req_env_z1_i <= req_env_z0_i);

    // D = e1 - e0. Both are s32; the difference is taken in the unsigned
    // 32-bit lane it fits in exactly, and it is >= 1 whenever !degen_c.
    du_c = $unsigned(req_env_x1_i) - $unsigned(req_env_x0_i);
    dv_c = $unsigned(req_env_z1_i) - $unsigned(req_env_z0_i);

    // N = (w - e0) * 64, in s64 in the reference (F2). The subtraction is done
    // at NUM_W so it cannot wrap for any pair of s32 words.
    nu_c = (NUM_W'($signed(req_wx_i)) - NUM_W'($signed(req_env_x0_i))) <<< 6;
    nv_c = (NUM_W'($signed(req_wz_i)) - NUM_W'($signed(req_env_z0_i))) <<< 6;

    negu_c = nu_c[NUM_W-1];
    negv_c = nv_c[NUM_W-1];
    // N >= 64*D, i.e. the quotient is >= 64 — one compare, taken BEFORE any
    // division, and it is what bounds the quotient to six bits.
    satu_c = !negu_c && ($unsigned(nu_c) >= {2'b00, du_c, 6'b000000});
    satv_c = !negv_c && ($unsigned(nv_c) >= {2'b00, dv_c, 6'b000000});
  end

  // ---------------------------------------------------------------------------
  // one restoring step, written out so nothing indexes a function's result
  // (zhao_geom_binner's Quartus 17.0 rejection, 2026-08-18)
  // ---------------------------------------------------------------------------
  // `k` is a constant at every call site below, so `d << k` is a wiring change
  // and not a barrel shifter.
  function automatic logic [REM_W-1:0] div_sub(input logic [REM_W-1:0] r,
                                               input logic [DEN_W-1:0] d, input int unsigned k);
    logic [REM_W-1:0] shifted;
    begin
      shifted = {7'b0000000, d} << k;
      div_sub = (r >= shifted) ? (r - shifted) : r;
    end
  endfunction

  function automatic logic div_bit(input logic [REM_W-1:0] r, input logic [DEN_W-1:0] d,
                                   input int unsigned k);
    logic [REM_W-1:0] shifted;
    begin
      shifted = {7'b0000000, d} << k;
      div_bit = (r >= shifted);
    end
  endfunction

  // three steps per state, the same three wires on both axes
  logic [REM_W-1:0] ru_a, ru_b, ru_c2;
  logic [REM_W-1:0] rv_a, rv_b, rv_c2;
  logic [ 2:0]      qu_hi, qv_hi, qu_lo, qv_lo;

  always_comb begin
    // ST_DIV0: bits 5, 4, 3
    qu_hi[2] = div_bit(ru_r, du_r, 5);
    ru_a     = div_sub(ru_r, du_r, 5);
    qu_hi[1] = div_bit(ru_a, du_r, 4);
    ru_b     = div_sub(ru_a, du_r, 4);
    qu_hi[0] = div_bit(ru_b, du_r, 3);
    ru_c2    = div_sub(ru_b, du_r, 3);

    qv_hi[2] = div_bit(rv_r, dv_r, 5);
    rv_a     = div_sub(rv_r, dv_r, 5);
    qv_hi[1] = div_bit(rv_a, dv_r, 4);
    rv_b     = div_sub(rv_a, dv_r, 4);
    qv_hi[0] = div_bit(rv_b, dv_r, 3);
    rv_c2    = div_sub(rv_b, dv_r, 3);
  end

  logic [REM_W-1:0] ru_d, ru_e, ru_f;
  logic [REM_W-1:0] rv_d, rv_e, rv_f;

  always_comb begin
    // ST_DIV1: bits 2, 1, 0
    qu_lo[2] = div_bit(ru_r, du_r, 2);
    ru_d     = div_sub(ru_r, du_r, 2);
    qu_lo[1] = div_bit(ru_d, du_r, 1);
    ru_e     = div_sub(ru_d, du_r, 1);
    qu_lo[0] = div_bit(ru_e, du_r, 0);
    ru_f     = div_sub(ru_e, du_r, 0);

    qv_lo[2] = div_bit(rv_r, dv_r, 2);
    rv_d     = div_sub(rv_r, dv_r, 2);
    qv_lo[1] = div_bit(rv_d, dv_r, 1);
    rv_e     = div_sub(rv_d, dv_r, 1);
    qv_lo[0] = div_bit(rv_e, dv_r, 0);
    rv_f     = div_sub(rv_e, dv_r, 0);
  end

  // the clamped texel index: 63 on the saturating rail, the quotient otherwise
  // (the negative rail already loaded a zero remainder, so its quotient is 0)
  logic [5:0] tex_u_c;
  logic [5:0] tex_v_c;
  assign tex_u_c = satu_r ? 6'd63 : qu_r;
  assign tex_v_c = satv_r ? 6'd63 : qv_r;

  // ---------------------------------------------------------------------------
  // the handshake. Hygiene: every output valid/ready is a function of
  // registers only, never of the incoming ready (zhao_texture_tmu's rule).
  // ---------------------------------------------------------------------------
  assign req_ready_o  = (st_r == ST_IDLE);
  assign idle_o       = (st_r == ST_IDLE);
  assign shr_valid_o  = (st_r == ST_REQ);
  assign shr_op_o     = OpRead;
  assign shr_handle_o = handle_r;
  assign shr_texel_o  = {tex_v_c, tex_u_c};  // j*64 + i, the sheet's scan order
  assign shr_src_id_o = src_r;
  assign shp_ready_o  = (st_r == ST_RSP);

  assign smp_valid_o      = (st_r == ST_OUT);
  assign smp_tag_o        = out_tag_r;
  assign smp_strength_o   = out_str_r;
  assign smp_u_o          = degen_r ? 6'd0 : tex_u_c;
  assign smp_v_o          = degen_r ? 6'd0 : tex_v_c;
  assign smp_degenerate_o = degen_r;
  assign smp_miss_o       = out_miss_r;
  assign smp_src_id_o     = src_r;

  // The sheet's response also carries an op echo and other statuses this block
  // never asks for: it only ever issues OpRead, so the only two answers it can
  // receive are StHit and StMiss.
  logic unused_ok;
  always_comb begin
    unused_ok = |shp_status_i[0];
    unused_ok = unused_ok & 1'b0;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r              <= ST_IDLE;
      handle_r          <= 32'd0;
      src_r             <= 16'd0;
      degen_r           <= 1'b0;
      ru_r              <= {REM_W{1'b0}};
      rv_r              <= {REM_W{1'b0}};
      du_r              <= {DEN_W{1'b0}};
      dv_r              <= {DEN_W{1'b0}};
      qu_r              <= 6'd0;
      qv_r              <= 6'd0;
      satu_r            <= 1'b0;
      satv_r            <= 1'b0;
      out_tag_r         <= 8'd0;
      out_str_r         <= 8'd0;
      out_miss_r        <= 1'b0;
      texture_samples_o <= 32'd0;
    end else begin
      case (st_r)
        ST_IDLE: begin
          if (req_valid_i) begin
            handle_r <= req_handle_i;
            src_r    <= req_src_id_i;
            degen_r  <= degen_c;
            du_r     <= du_c;
            dv_r     <= dv_c;
            satu_r   <= satu_c;
            satv_r   <= satv_c;
            qu_r     <= 6'd0;
            qv_r     <= 6'd0;
            // A negative numerator and a saturating one both bypass the
            // division: load a zero remainder so the six steps produce zero,
            // and let `satu_r` pick 63 at the end. Nothing corrects a floor
            // afterwards, because the clamp already decided (F1/the divide
            // argument in the header).
            ru_r <= (negu_c || satu_c) ? {REM_W{1'b0}} : REM_W'($unsigned(nu_c));
            rv_r <= (negv_c || satv_c) ? {REM_W{1'b0}} : REM_W'($unsigned(nv_c));
            out_tag_r  <= 8'd0;
            out_str_r  <= 8'd0;
            out_miss_r <= 1'b0;
            st_r       <= ST_DIV0;
          end
        end

        ST_DIV0: begin
          qu_r[5:3] <= qu_hi;
          qv_r[5:3] <= qv_hi;
          ru_r      <= ru_c2;
          rv_r      <= rv_c2;
          st_r      <= ST_DIV1;
        end

        ST_DIV1: begin
          qu_r[2:0] <= qu_lo;
          qv_r[2:0] <= qv_lo;
          ru_r      <= ru_f;
          rv_r      <= rv_f;
          // F4: a degenerate envelope reads NOTHING — it never reaches ST_REQ,
          // so it cannot consume a sheet-port cycle.
          st_r      <= degen_r ? ST_OUT : ST_REQ;
        end

        ST_REQ: begin
          if (shr_ready_i) st_r <= ST_RSP;
        end

        ST_RSP: begin
          if (shp_valid_i) begin
            // A2: a non-resident sheet reads as ZERO and says so; it is never
            // presented as stale bytes.
            if (shp_status_i == StMiss) begin
              out_tag_r  <= 8'd0;
              out_str_r  <= 8'd0;
              out_miss_r <= 1'b1;
            end else begin
              out_tag_r  <= shp_tag_i;
              out_str_r  <= shp_strength_i;
              out_miss_r <= 1'b0;
            end
            st_r <= ST_OUT;
          end
        end

        ST_OUT: begin
          if (smp_ready_i) begin
            st_r <= ST_IDLE;
            if (texture_samples_o != CNT_MAX) texture_samples_o <= texture_samples_o + 32'd1;
          end
        end

        default: st_r <= ST_IDLE;
      endcase
    end
  end

endmodule
