// zhao_twod_band.sv -- the HUD band store: descriptor order in, raster order out.
//
// ---------------------------------------------------------------------------
// THE SEAM, IN ENTRY I17's OWN SENTENCE
// ---------------------------------------------------------------------------
//   "TWOD.SPRITE walks in DESCRIPTOR order, one whole sprite at a time;
//    `hud_*` is a random access in RASTER order."
//
// POST.COMPOSITE asks for one HUD pixel per composited pixel and sweeps (x, y)
// monotonically. The sprite walker delivers a whole sprite at a time, wherever
// it happens to sit. Something has to hold the pixels in between. THIS BLOCK IS
// THAT SOMETHING AND NOTHING ELSE.
//
// Owner ruling R233 chose the shape from three that were costed:
//
//     frame store (2x)   704/553 M10K   fits             OVER THE DEVICE
//     line ring            ~12 M10K     133% of frame    cannot draw a HUD
//     BAND (B=4, L=16)      12 M10K     11.1% of frame   TAKEN
//
// AND THE THING THAT KILLED STRUCTURE 2 WAS NOT ITS MEMORY. It cost
// `sum of HEIGHTS x a line-time`; the band costs `sum of AREAS`. The two use the
// IDENTICAL memory. What separated them was TWOD.SPRITE holding `busy_q` for one
// whole descriptor -- DESCRIPTOR-MAJOR ORDER -- and that is what this block
// changes, by handing the walker BAND-CLIPPED descriptors instead of whole ones.
//
// ---------------------------------------------------------------------------
// IT REUSES `zhao_twod_sprite`. IT IS NOT A SECOND WALKER.
// ---------------------------------------------------------------------------
// HUDBAND's words, which R233 adopted: "I17's risk this time is rebuilding what
// exists, not missing it." The entry has twice recorded a BUILT block as
// missing. `e_*` below is the sprite walker's own `d_*` record, field for field,
// with three substitutions -- y, h and the (u, v) row origin -- and nothing
// else. No field is invented, widened or dropped.
//
// ---------------------------------------------------------------------------
// THE ADMISSION LAW -- OWNER RULING R235
// ---------------------------------------------------------------------------
//   "Refuse the sprite WHOLE, and COUNT the refusal."
//
// `TWOD.SPRITE.md`'s formal property is "a dropped sprite draws no pixels at
// all -- never a partial sprite", so a sprite that will not fit must be disposed
// of BEFORE rasterising. R235's reasoning is R221's, applied to a place players
// look: THE ALTERNATIVES MAKE AN ABSENCE LOOK LIKE A RESULT, and dropping a HUD
// sprite silently is W10.
//
// B and L come from a LEAKY BUCKET: drain `LINE_W` pixels per scanned line,
// burst `BURST_PX = (L-B)*LINE_W` -- the FIFO slack. Two accounts:
//
//     rate_q    the committed per-LINE pixel load of the sprites now live
//     burst_q   the unreserved part of the bucket
//
// At the FIRST band a sprite touches, once per sprite per frame:
//
//     new_rate = rate_q + w
//     excess   = (new_rate <= LINE_W) ? 0 : min(w, new_rate - LINE_W)
//     need     = excess * rows_remaining_on_screen          (saturating)
//     ADMIT iff need <= burst_q
//
// `excess` IS MARGINAL, AND THAT IS THE WHOLE CORRECTNESS OF IT. Charging each
// sprite the full over-rate of the stack it joins would price R233's own worked
// example at 78,720 pixels instead of 3,840 and REFUSE A HUD THE CONSOLE CAN
// DRAW. The marginal form reproduces R233's two numbers exactly:
//
//   * a full-width 32-row status bar alone: excess = 0, need = 0 -- "sits
//     exactly at rate", admitted free;
//   * 40 glyphs of 8x12 over it: each charges 8*12 = 96, total 3,840 = TEN
//     LINES OF BURST, inside 4,608. Admitted.
//
// WHY IT BOUNDS THE DEFICIT: over any window the store falls behind at
// (sum of live w - LINE_W) clamped at zero, per line; by construction that is at
// most the sum of the live `excess` values; each sprite contributes its excess
// for at most `rows_i` lines, i.e. `need_i`; and admission enforces
// `sum need_i <= BURST_PX`. So the filler is never more than BURST_PX pixels
// behind, which is the FIFO slack, which is `band_underrun_o` never firing.
//
// ADMISSION IS DECIDED ONCE. A sprite admitted in its first band is drawn in
// every later band WITHOUT RE-TESTING. That is what makes "whole" true -- a
// per-band re-test would admit a sprite in band 3 and refuse it in band 4, which
// is a partial sprite with a counter attached.
//
// ---------------------------------------------------------------------------
// FRESHNESS: WHY THERE IS A GENERATION TAG AND A SCRUB, AND WHY NEITHER IS
// OPTIONAL. THIS IS THE COST R233's THREE-STRUCTURE TABLE DID NOT CARRY.
// ---------------------------------------------------------------------------
// A band slot is re-used every `L/B` bands. It must not show the previous
// occupant's pixels. There are exactly three ways to get that, and the obvious
// one is unaffordable:
//
//  * CLEAR THE SLOT. B*LINE_W = 1,536 writes per band against 1,536 clocks of
//    band drain -- the clear alone is 100% of the write port, for the whole
//    frame. Measured, not guessed: 60 bands x 1,536 = 92,160 = exactly one
//    frame of clocks. A clear is not affordable AT ANY LEAD, because leading by
//    N bands multiplies both sides.
//
//  * TAG EVERY WORD WITH SOMETHING THAT CANNOT REPEAT. Impossible in a fixed
//    width: any tag wraps, and the content's lifetime is unbounded. R233's "the
//    17th bit is free at every L" is true and it does NOT buy freshness.
//
//  * A PER-ROW GENERATION TAG WIDE ENOUGH THAT IT DOES NOT WRAP IN A SESSION,
//    which is what this block does. `gen_q[row]` is bumped when that row's band
//    opens; a word reads as HUD only when its stored tag equals it. The tag runs
//    1..GEN_MAX and NEVER 0, so a word that has never been written -- an M10K
//    powers up at zero -- can never read as a HUD pixel. THE WHOLE CLEAR IS
//    `B` COUNTER INCREMENTS, one per row of the band being opened.
//
// A SCRUB WAS BUILT FIRST AND IT WAS UNSOUND. It wrote generation 0 into one
// word per idle write cycle, over the slots the filler and reader were both
// finished with -- and in a FIFO that is running at all, THERE ARE NO SUCH
// SLOTS: with L/B slots, one is being filled and the rest hold bands the reader
// has not reached yet. The directed bench caught it as missing bands 4 and 5 of
// a 20-row sprite, which is the scrub erasing a band between the fill and the
// read. Recorded rather than deleted, because the mistake is a general one: a
// "free" resource in a pipelined store is free only in the window between the
// last consumer and the next producer, and a full pipeline has no such window.
//
// SO THE TAG CARRIES IT ALONE, AND IT HAS TO BE WIDE. GENW = 24 gives
// 16,777,215 opens before a row's tag repeats; a row is opened MAX_H/L = 15
// times per frame, so that is ~1.1 million frames, about five hours at 60 Hz --
// and it is a bound, not an absence. It is stated here rather than left to be
// found.
//
// THE PRICE IS THE HONEST FINDING OF THIS PACKET. R233 costed the band at
// `ceil(0.75 * L)` = 12 M10K, which is right FOR A WORD OF 20 BITS OR FEWER:
// an M10K is 512 x 20. A 16-bit colour plus a 24-bit tag is 40 bits, which is
// the 256 x 40 configuration -- so the store is `ceil(LINE_W * L / 256)` = 24
// M10K. R233's "the 17th bit is free at every L" is true and it buys a VALID
// BIT; it does not buy FRESHNESS, and freshness is not optional.
//
// THE CHEAPER EXACT FORM IS PRICED AND NOT TAKEN, so the owner can buy it: a
// separate one-bit-per-pixel PRESENCE plane (6,144 bits, 1 M10K) can be cleared
// a WORD at a time -- 77 writes per band instead of 1,536 -- which would keep
// the colour store at 20 bits and 12 M10K, for 13 total. It costs a
// write-combining buffer on the colour path, because setting one bit of a
// 20-bit mask word is a read-modify-write. 23 M10K against 34, for a block that
// has to be right the first time with no fit available to check it.
//
// ---------------------------------------------------------------------------
// THE READ PORT HOLDS THROUGH A STALL FOR FREE, AND THE REASON IS WORTH STATING
// ---------------------------------------------------------------------------
// POST.COMPOSITE's header: "EVERY 1-CYCLE-LATENCY RESPONSE MUST BE HELD THROUGH
// A STALL ... a synchronous memory does this for free". `rd_x_i`/`rd_y_i` do not
// advance while its pipe is stalled, so the same word is re-read and the output
// is stable.
//
// `rd_valid_o` IS DERIVED FROM THE SAME RAM WORD AS `rd_rgb_o` -- the generation
// bits live in that word -- so the two cannot disagree. That is deliberate, and
// it is CLAUDE.md's metadata-swap law read FORWARDS: the defect there was two
// quantities that moved together when they should not have; here they are ONE
// WORD and must.
//
// THE SCANOUT ADDRESS IS COMPUTED FROM (x, y), NOT FROM A COUNTER, AND THE
// COUNTER IS THE INSTRUMENT. R233 records that `hud_req_*` is a monotonic sweep
// and that the address is therefore a counter. It is -- but a counter that is
// right only while the promise holds is a picture that is wrong when it does
// not. So the READ uses the arithmetic address (a multiply by a CONSTANT, which
// is shift-add, not a DSP) and a free-running counter is compared against it:
// `scan_addr_mismatch_o`. The two sides are clocked by different things in
// different modules, which is exactly what CLAUDE.md demands of a checker.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_twod_band #(
    // The widest view this instance serves. 384 = Z60; Storm is 320 and a Duo
    // VIEW is 256, so 384 covers every mode -- the same parameter, for the same
    // reason, as POST.COMPOSITE's ring.
    parameter int unsigned LINE_W   = 384,
    parameter int unsigned MAX_H    = 240,
    // B: the band height in rows.  L: the store height in rows.
    // L must be a multiple of B and a power of two (the row slot is y[..]).
    parameter int unsigned B        = 4,
    parameter int unsigned L        = 16,
    // The display list. M10K is WIDTH-bound at this depth, so 64 and 256 cost
    // the same; 64 is what R233's 4.2%-of-frame re-walk figure was measured at.
    parameter int unsigned MAX_DESC = 64,
    parameter int unsigned UVW      = 32,
    // 24 bits of generation and 16 of colour is exactly the M10K's 256 x 40
    // configuration. See the header: this is what freshness costs.
    parameter int unsigned GENW     = 24,
    // THE BUCKET. Default = the FIFO slack, which is what R233's sizing means.
    // RAISING IT PAST (L-B)*LINE_W REMOVES THE UNDERRUN GUARANTEE ON PURPOSE:
    // tests/mutants/zhao_twod_band_burst_mutant.sv does exactly that, and its
    // driver passes when `band_underrun_o` FIRES.
    parameter int unsigned BURST_PX = (L - B) * LINE_W
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the frame this band serves -----------------------------------------
    input  var logic                        frame_start_i,
    input  var logic [$clog2(LINE_W+1)-1:0] frame_w_i,
    input  var logic [$clog2(MAX_H +1)-1:0] frame_h_i,
    // THE BAND OWNS THE VIEW OF THE FILL WALK, and that is not a convenience.
    // The filler leads the reader by up to L/B bands, so driving the walker
    // from the compositor's `post_view_o` -- which tracks the READER -- would
    // fill the rows either side of a Duo seam with the other view's HUD. The
    // seam is a ROW, so the band derives the view from the row it is filling:
    // `view_split_i` is the first row of view 1, and 0 means a single view.
    input  var logic [$clog2(MAX_H +1)-1:0] view_split_i,

    // ---- the display list in (the CMD seam; today the core's twod_sd_*) -----
    input  var logic                    d_valid_i,
    output var logic                    d_ready_o,
    input  var logic signed [15:0]      d_x_i,
    input  var logic signed [15:0]      d_y_i,
    input  var logic [15:0]             d_w_i,
    input  var logic [15:0]             d_h_i,
    input  var logic signed [UVW-1:0]   d_u_i,
    input  var logic signed [UVW-1:0]   d_v_i,
    input  var logic signed [UVW-1:0]   d_a00_i,
    input  var logic signed [UVW-1:0]   d_a01_i,
    input  var logic signed [UVW-1:0]   d_a10_i,
    input  var logic signed [UVW-1:0]   d_a11_i,
    input  var logic [2:0]              d_format_i,
    input  var logic [7:0]              d_palette_i,
    input  var logic [15:0]             d_tint_i,
    input  var logic [1:0]              d_blend_i,
    input  var logic [1:0]              d_view_mask_i,
    input  var logic [7:0]              d_order_i,
    input  var logic [15:0]             d_src_id_i,

    // ---- the band-clipped slice out, to zhao_twod_sprite's d_* --------------
    output var logic                    e_valid_o,
    input  var logic                    e_ready_i,
    output var logic signed [15:0]      e_x_o,
    output var logic signed [15:0]      e_y_o,
    output var logic [15:0]             e_w_o,
    output var logic [15:0]             e_h_o,
    output var logic signed [UVW-1:0]   e_u_o,
    output var logic signed [UVW-1:0]   e_v_o,
    output var logic signed [UVW-1:0]   e_a00_o,
    output var logic signed [UVW-1:0]   e_a01_o,
    output var logic signed [UVW-1:0]   e_a10_o,
    output var logic signed [UVW-1:0]   e_a11_o,
    output var logic [2:0]              e_format_o,
    output var logic [7:0]              e_palette_o,
    output var logic [15:0]             e_tint_o,
    output var logic [1:0]              e_blend_o,
    output var logic [1:0]              e_view_mask_o,
    output var logic [7:0]              e_order_o,
    output var logic [15:0]             e_src_id_o,
    // the walker's `view_sel_i`, for the band being filled
    output var logic [1:0]              e_view_sel_o,

    // ---- the colours in, from zhao_twod_sampler's sc_* ----------------------
    input  var logic                    c_valid_i,
    output var logic                    c_ready_o,
    input  var logic [15:0]             c_rgb_i,
    input  var logic signed [15:0]      c_x_i,
    input  var logic signed [15:0]      c_y_i,
    input  var logic [15:0]             c_tint_i,
    input  var logic [1:0]              c_blend_i,
    input  var logic [7:0]              c_order_i,
    input  var logic [15:0]             c_src_id_i,
    input  var logic                    c_last_i,

    // ---- POST.COMPOSITE's hud_* raster read --------------------------------
    input  var logic                        rd_req_v_i,
    input  var logic [$clog2(LINE_W+1)-1:0] rd_x_i,
    input  var logic [$clog2(MAX_H +1)-1:0] rd_y_i,
    output var logic                        rd_valid_o,
    output var logic [15:0]                 rd_rgb_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]             descriptors_o,
    output var logic [31:0]             desc_overflow_o,
    output var logic [31:0]             sprites_admitted_o,
    output var logic [31:0]             sprites_refused_budget_o,
    output var logic [31:0]             slices_emitted_o,
    output var logic [31:0]             pixels_written_o,
    output var logic [31:0]             pixels_clipped_o,
    output var logic [31:0]             write_oob_o,
    output var logic [31:0]             band_underrun_o,
    output var logic [31:0]             scan_addr_mismatch_o,
    output var logic [31:0]             tint_dropped_o,
    output var logic [31:0]             blend_dropped_o,
    // TWOD.SPRITE.md: "composited output order equals descriptor `order` under
    // arbitrary backpressure". The band composites by LAST WRITE, so that law
    // holds exactly while the list is in `order` -- and this counter is what
    // says whether it was. It reads `c_order_i` and `c_src_id_i`, which is why
    // the band takes the whole `sc_*` group rather than the four fields it
    // stores: a port with no consumer is a tie-off, and an UNCHECKED law is a
    // sentence.
    output var logic [31:0]             order_inversion_o,
    output var logic [31:0]             bands_o
);

  // ==========================================================================
  // SHAPES
  // ==========================================================================
  localparam int unsigned XW      = $clog2(LINE_W + 1);
  localparam int unsigned YW      = $clog2(MAX_H + 1);
  localparam int unsigned WORDS   = LINE_W * L;
  localparam int unsigned AW      = $clog2(WORDS);
  localparam int unsigned LB      = $clog2(L);            // row slot bits
  localparam int unsigned BB      = $clog2(B);            // band index shift
  localparam int unsigned SLOTS   = L / B;
  localparam int unsigned DW      = 16 + GENW;            // the band store word
  localparam int unsigned GEN_MAX = (1 << GENW) - 1;      // generations 1..GEN_MAX, never 0
  localparam int unsigned IW      = $clog2(MAX_DESC);
  localparam int unsigned BANDS   = (MAX_H + B - 1) / B;
  localparam int unsigned KW      = $clog2(BANDS + 1);    // band index
  localparam int unsigned NEEDW   = $clog2(BURST_PX + 2) + 1;
  localparam int unsigned RATEW   = 24;                   // sum of widths, 64 x 16 bits

  // The display list word. Offsets are named so a reader can check the packing
  // against the port list without counting bits.
  localparam int unsigned F_X    = 0;                       // 16 signed
  localparam int unsigned F_Y    = F_X   + 16;              // 16 signed
  localparam int unsigned F_W    = F_Y   + 16;              // 16
  localparam int unsigned F_H    = F_W   + 16;              // 16
  localparam int unsigned F_U    = F_H   + 16;              // UVW
  localparam int unsigned F_V    = F_U   + UVW;             // UVW
  localparam int unsigned F_A00  = F_V   + UVW;             // UVW
  localparam int unsigned F_A01  = F_A00 + UVW;             // UVW
  localparam int unsigned F_A10  = F_A01 + UVW;             // UVW
  localparam int unsigned F_A11  = F_A10 + UVW;             // UVW
  localparam int unsigned F_FMT  = F_A11 + UVW;             // 3
  localparam int unsigned F_PAL  = F_FMT + 3;               // 8
  localparam int unsigned F_TINT = F_PAL + 8;               // 16
  localparam int unsigned F_BLND = F_TINT + 16;             // 2
  localparam int unsigned F_VM   = F_BLND + 2;              // 2
  localparam int unsigned F_ORD  = F_VM  + 2;               // 8
  localparam int unsigned F_SRC  = F_ORD + 8;               // 16
  // the mutable half
  localparam int unsigned F_CU   = F_SRC + 16;              // UVW  cursor u
  localparam int unsigned F_CV   = F_CU  + UVW;             // UVW  cursor v
  localparam int unsigned F_ST   = F_CV  + UVW;             // 2    lifecycle
  localparam int unsigned F_NEED = F_ST  + 2;               // NEEDW reserved burst
  localparam int unsigned DESCW  = F_NEED + NEEDW;

  localparam logic [1:0] ST_FREE = 2'd0;   // slot unused
  localparam logic [1:0] ST_NEW  = 2'd1;   // captured, never touched a band
  localparam logic [1:0] ST_LIVE = 2'd2;   // admitted; drawn in every band it spans
  localparam logic [1:0] ST_DEAD = 2'd3;   // refused, or finished

  // Quartus 17.0 rejects a bare module-scope elaboration check --
  // `syntax error near text: "if"; expecting "endmodule"` -- and needs it
  // inside `initial begin ... end`. CLAUDE.md records both forms and the fact
  // that `--lint-only` does not run `initial` blocks, so a clean lint says
  // nothing whatever about these.
  // synthesis translate_off
  initial begin
    if ((L % B) != 0)
      $fatal(1, "zhao_twod_band: L (%0d) must be a multiple of B (%0d)", L, B);
    if ((1 << LB) != L)
      $fatal(1, "zhao_twod_band: L (%0d) must be a power of two", L);
    if ((1 << BB) != B)
      $fatal(1, "zhao_twod_band: B (%0d) must be a power of two", B);
    if (SLOTS < 2)
      $fatal(1, "zhao_twod_band: L/B (%0d) must be at least 2 -- the filler needs a slot the reader is not draining", SLOTS);
    if (GEN_MAX < ((MAX_H + L - 1) / L))
      $fatal(1, "zhao_twod_band: GENW %0d gives %0d generations, fewer than the %0d laps a row takes per frame -- a SAME-FRAME ghost", GENW, GEN_MAX, (MAX_H + L - 1) / L);
  end
  // synthesis translate_on

  // ==========================================================================
  // THE BAND STORE -- one write port, one read port. The shape TWOD.SAMPLER's
  // ring already has, which is the shape `check_ram_inference.py` recognises.
  // ==========================================================================
  logic [DW-1:0] band_q [0:WORDS-1];
  logic          bw_en_c;
  logic [AW-1:0] bw_addr_c;
  logic [DW-1:0] bw_data_c;
  logic [AW-1:0] br_addr_c;
  logic [DW-1:0] br_data_q;

  always_ff @(posedge clk) begin
    if (bw_en_c) band_q[bw_addr_c] <= bw_data_c;
    br_data_q <= band_q[br_addr_c];
  end

  // The row base is a multiply by a CONSTANT, which is a shift-add and not a
  // DSP: LINE_W is a parameter, never a signal.
  function automatic logic [AW-1:0] row_base(input logic [LB-1:0] slot);
    row_base = AW'(32'(slot) * LINE_W);
  endfunction

  // ==========================================================================
  // THE DISPLAY LIST -- one write port, one read port.
  // ==========================================================================
  logic [DESCW-1:0] desc_q [0:MAX_DESC-1];
  logic             dw_en_c;
  logic [IW-1:0]    dw_addr_c;
  logic [DESCW-1:0] dw_data_c;
  logic [IW-1:0]    dr_addr_c;
  logic [DESCW-1:0] dr_data_q;

  always_ff @(posedge clk) begin
    if (dw_en_c) desc_q[dw_addr_c] <= dw_data_c;
    dr_data_q <= desc_q[dr_addr_c];
  end

  // ==========================================================================
  // FRAME AND SWEEP
  // ==========================================================================
  // THE FILLER IS RESTARTED BY THE FRAME TICK, NOT BY THE SWEEP, AND THE
  // DIFFERENCE IS THE WHOLE STARTUP MARGIN. `frame_start_i` is the console's
  // frame edge (`gpu_tick_o`); the compositor's pass does not begin until
  // render and resolve are done, so the filler gets that entire interval as a
  // head start on the reader. Restarting on the sweep's own origin instead
  // would hand the filler zero lead and underrun band 0 of every frame.
  //
  // A SECOND SWEEP WITHOUT A NEW TICK IS NOT A REFILL. The generations are not
  // bumped, so the store still holds the frame that was filled, and the pass
  // re-reads it. That is the correct answer for a repeated pass and it costs
  // nothing to get right.
  logic restart_c;
  assign restart_c = frame_start_i;

  // The sweep's origin resynchronises the read-address INSTRUMENT only.
  logic sweep_org_c, sweep_org_q, sweep_sync_c;
  assign sweep_org_c = rd_req_v_i && (rd_x_i == XW'(0)) && (rd_y_i == YW'(0));
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) sweep_org_q <= 1'b0;
    else        sweep_org_q <= sweep_org_c;
  end
  assign sweep_sync_c = (sweep_org_c && !sweep_org_q) || restart_c;

  // ==========================================================================
  // THE READ SIDE
  // ==========================================================================
  logic [LB-1:0]  rd_slot_c;
  logic [KW-1:0]  rd_band_c;
  assign rd_slot_c = rd_y_i[LB-1:0];
  assign rd_band_c = KW'(rd_y_i >> BB);
  assign br_addr_c = row_base(rd_slot_c) + AW'(rd_x_i);

  logic [GENW-1:0] gen_q [0:L-1];

  logic [KW-1:0]   fill_band_q;      // the band the filler is working on
  logic            rd_ok_c;
  assign rd_ok_c = rd_req_v_i && (rd_band_c < fill_band_q);

  logic            rd_v_q, rd_ok_q, rd_first_q;
  logic [GENW-1:0] rd_gen_q;
  logic [AW-1:0]   rd_addr_q;
  logic [AW-1:0]   rd_count_q;       // the INSTRUMENT, never the address
  logic            rd_hold_v_q;
  logic [15:0]     rd_hold_rgb_q;

  assign rd_valid_o = rd_hold_v_q;
  assign rd_rgb_o   = rd_hold_rgb_q;

  // ==========================================================================
  // THE BAND SCHEDULE
  // ==========================================================================
  // `fill_band_q` is the band being filled. A band is readable once the filler
  // has moved PAST it. The filler may run at most SLOTS-1 bands ahead of the
  // reader, which is the FIFO slack the bucket is sized against.
  logic [KW-1:0] rd_band_q;          // the band the reader is in, registered
  logic          band_open_q;        // a band is open for fill
  logic [KW-1:0] outstanding_q;      // slices emitted whose last pixel has not arrived

  logic signed [31:0] fill_lead_c;
  assign fill_lead_c = 32'(fill_band_q) - 32'(rd_band_q);

  logic can_open_c;
  assign can_open_c = (fill_band_q < KW'(BANDS)) && (fill_lead_c < 32'(SLOTS));

  // The view of the band being FILLED, published to the walker.
  logic [1:0] fill_view_c;
  assign fill_view_c = ((view_split_i != YW'(0)) &&
                        (32'(band_top_q) >= 32'(view_split_i))) ? 2'b10 : 2'b01;
  assign e_view_sel_o = fill_view_c;

  // ==========================================================================
  // THE WRITE SIDE
  // ==========================================================================
  // A ROW, not a band index: `KW` is wide enough to count bands and NOT wide
  // enough to hold `band * B`.
  logic [YW:0]   band_top_q;         // first screen row of the open band
  logic [15:0]   last_src_q;         // the sprite whose pixels are arriving
  logic [7:0]    last_order_q;
  logic          last_src_v_q;
  logic          c_in_band_c, c_in_x_c;
  logic [LB-1:0] c_slot_c;

  assign c_slot_c    = c_y_i[LB-1:0];
  assign c_in_band_c = band_open_q && (c_y_i >= 0)
                    && (32'($unsigned(c_y_i)) >= 32'(band_top_q))
                    && (32'($unsigned(c_y_i)) <  32'(band_top_q) + 32'(B))
                    && (32'($unsigned(c_y_i)) <  32'(frame_h_i));
  assign c_in_x_c    = (c_x_i >= 0) && (32'($unsigned(c_x_i)) < 32'(frame_w_i));

  assign c_ready_o = 1'b1;   // the write port is one pixel per clock and the
                             // band is open whenever a slice is in flight.

  logic          c_take_c;
  assign c_take_c = c_valid_i && c_ready_o;


  // ==========================================================================
  // THE SHARED SERIAL SHIFT-ADD -- the only multiply in the block, and it is
  // NOT a DSP. It runs ONCE PER SPRITE PER FRAME and early-exits on a zero
  // multiplier, which is the common case: a sprite whose first live row is its
  // own top row needs no cursor fast-forward at all.
  // ==========================================================================
  logic signed [31:0] mul_a_q;
  logic [15:0]        mul_b_q;
  logic signed [31:0] mul_acc_q;
  logic               mul_sat_q;
  logic               mul_busy_q;

  // ==========================================================================
  // THE BUCKET
  // ==========================================================================
  logic [RATEW-1:0] rate_q;
  logic [NEEDW-1:0] burst_q;

  // ==========================================================================
  // THE SCAN FSM
  // ==========================================================================
  localparam logic [3:0] S_IDLE  = 4'd0;
  localparam logic [3:0] S_OPEN  = 4'd1;
  localparam logic [3:0] S_REQ   = 4'd2;
  localparam logic [3:0] S_WAIT  = 4'd3;
  localparam logic [3:0] S_EVAL  = 4'd4;
  localparam logic [3:0] S_FFU   = 4'd5;
  localparam logic [3:0] S_FFV   = 4'd6;
  localparam logic [3:0] S_NEED  = 4'd7;
  localparam logic [3:0] S_ADMIT = 4'd8;
  localparam logic [3:0] S_EMIT  = 4'd9;
  localparam logic [3:0] S_ADV   = 4'd10;
  localparam logic [3:0] S_WB    = 4'd11;
  localparam logic [3:0] S_NEXT  = 4'd12;
  localparam logic [3:0] S_CLOSE = 4'd13;

  logic [3:0]     st_q;
  logic [IW:0]    idx_q;
  logic [IW:0]    count_q;          // descriptors captured this frame
  logic [DESCW-1:0] cur_q;          // the descriptor under the scan
  logic signed [YW:0] top_q, rows_band_q, rows_left_q;
  logic [2:0]     adv_q;            // rows still to fold into the cursor
  logic           last_band_q;
  logic           e_valid_q;

  // ---- fields of the descriptor under the scan -----------------------------
  logic signed [15:0] cd_y_c;
  logic [15:0]        cd_w_c, cd_h_c;
  logic signed [UVW-1:0] cd_a01_c, cd_a11_c;
  logic [1:0]         cd_vm_c, cd_st_c;
  logic [NEEDW-1:0]   cd_need_c;
  logic signed [UVW-1:0] cd_cu_c, cd_cv_c;

  assign cd_y_c   = cur_q[F_Y   +: 16];
  assign cd_w_c   = cur_q[F_W   +: 16];
  assign cd_h_c   = cur_q[F_H   +: 16];
  assign cd_a01_c = cur_q[F_A01 +: UVW];
  assign cd_a11_c = cur_q[F_A11 +: UVW];
  assign cd_vm_c  = cur_q[F_VM  +: 2];
  assign cd_st_c  = cur_q[F_ST  +: 2];
  assign cd_need_c= cur_q[F_NEED+: NEEDW];
  assign cd_cu_c  = cur_q[F_CU  +: UVW];
  assign cd_cv_c  = cur_q[F_CV  +: UVW];

  // ---- the band-clipped geometry, all adders -------------------------------
  logic signed [31:0] spr_top_c, spr_bot_c, band_lo_c, band_hi_c;
  logic signed [31:0] clip_top_c, clip_bot_c;
  assign spr_top_c = 32'(cd_y_c);
  assign spr_bot_c = 32'(cd_y_c) + 32'({16'd0, cd_h_c});
  assign band_lo_c = 32'({{(32-KW){1'b0}}, band_top_q});
  assign band_hi_c = band_lo_c + 32'(B);

  function automatic logic signed [31:0] smax(input logic signed [31:0] a,
                                              input logic signed [31:0] b);
    smax = (a > b) ? a : b;
  endfunction
  function automatic logic signed [31:0] smin(input logic signed [31:0] a,
                                              input logic signed [31:0] b);
    smin = (a < b) ? a : b;
  endfunction

  assign clip_top_c = smax(smax(spr_top_c, band_lo_c), 32'sd0);
  assign clip_bot_c = smin(smin(spr_bot_c, band_hi_c), 32'(frame_h_i));

  logic sprite_ends_c;
  assign sprite_ends_c = (smin(spr_bot_c, 32'(frame_h_i)) <= band_hi_c);

  // ---- the admission arithmetic -------------------------------------------
  logic [RATEW-1:0] new_rate_c, over_c, excess_c;
  assign new_rate_c = rate_q + RATEW'(cd_w_c);
  assign over_c     = (new_rate_c > RATEW'(LINE_W)) ? (new_rate_c - RATEW'(LINE_W))
                                                    : RATEW'(0);
  assign excess_c   = (over_c < RATEW'(cd_w_c)) ? over_c : RATEW'(cd_w_c);

  // ==========================================================================
  // OUTPUT: the band-clipped slice
  // ==========================================================================
  assign e_valid_o     = e_valid_q;
  assign e_x_o         = cur_q[F_X   +: 16];
  assign e_y_o         = 16'(top_q);
  assign e_w_o         = cur_q[F_W   +: 16];
  assign e_h_o         = 16'(rows_band_q);
  assign e_u_o         = cur_q[F_CU  +: UVW];
  assign e_v_o         = cur_q[F_CV  +: UVW];
  assign e_a00_o       = cur_q[F_A00 +: UVW];
  assign e_a01_o       = cur_q[F_A01 +: UVW];
  assign e_a10_o       = cur_q[F_A10 +: UVW];
  assign e_a11_o       = cur_q[F_A11 +: UVW];
  assign e_format_o    = cur_q[F_FMT +: 3];
  assign e_palette_o   = cur_q[F_PAL +: 8];
  assign e_tint_o      = cur_q[F_TINT+: 16];
  assign e_blend_o     = cur_q[F_BLND+: 2];
  assign e_view_mask_o = cur_q[F_VM  +: 2];
  assign e_order_o     = cur_q[F_ORD +: 8];
  assign e_src_id_o    = cur_q[F_SRC +: 16];

  // ==========================================================================
  // INTAKE
  // ==========================================================================
  // TWOD.SPRITE.md: "descriptor count over the frame budget -> DROP THE TAIL,
  // deterministically by `order`, and count -- the HUD must not fault a frame."
  // So the handshake always completes and the surplus descriptor is dropped
  // here. Lowering `ready` instead would BACKPRESSURE THE COMMAND STREAM on a
  // HUD overflow, which stalls the whole console for the least important thing
  // on the screen -- and it also makes the refusal uncountable, because a held
  // offer and a new offer are indistinguishable on the wire.
  assign d_ready_o = 1'b1;
  logic list_full_c;
  assign list_full_c = (count_q >= (IW+1)'(MAX_DESC));

  // ==========================================================================
  // THE WRITE PORT -- one colour per clock, and nothing else ever writes.
  // ==========================================================================
  always_comb begin
    bw_en_c   = 1'b0;
    bw_addr_c = '0;
    bw_data_c = '0;
    if (c_take_c && c_in_band_c && c_in_x_c) begin
      bw_en_c   = 1'b1;
      bw_addr_c = row_base(c_slot_c) + AW'($unsigned(c_x_i[XW-1:0]));
      bw_data_c = {gen_q[c_slot_c], c_rgb_i};
    end
  end


  // ==========================================================================
  // THE DISPLAY LIST WRITE PORT -- intake beats write-back, AND THE WRITE-BACK
  // RETRIES RATHER THAN BEING DROPPED.
  // ==========================================================================
  // The first version registered the write-back and let intake win, which SILENTLY
  // LOST IT: descriptors arrive while the scan is already running, so a sprite
  // whose ST_NEW -> ST_LIVE write-back collided with an arriving descriptor was
  // admitted TWICE -- charged to the bucket twice, and refused the second time.
  // The directed bench saw it as one refusal in a list that fits, which is the
  // R235 counter reporting a defect in its own accounting rather than a HUD that
  // does not fit. The write is now COMBINATIONAL out of `cur_q` and `S_WB` holds
  // until the port is free, which also deletes DESCW + IW + 1 registers.
  always_comb begin
    dw_en_c   = 1'b0;
    dw_addr_c = '0;
    dw_data_c = '0;
    if (d_valid_i && !list_full_c) begin
      dw_en_c   = 1'b1;
      dw_addr_c = count_q[IW-1:0];
      dw_data_c = '0;
      dw_data_c[F_X   +: 16] = d_x_i;
      dw_data_c[F_Y   +: 16] = d_y_i;
      dw_data_c[F_W   +: 16] = d_w_i;
      dw_data_c[F_H   +: 16] = d_h_i;
      dw_data_c[F_U   +: UVW] = d_u_i;
      dw_data_c[F_V   +: UVW] = d_v_i;
      dw_data_c[F_A00 +: UVW] = d_a00_i;
      dw_data_c[F_A01 +: UVW] = d_a01_i;
      dw_data_c[F_A10 +: UVW] = d_a10_i;
      dw_data_c[F_A11 +: UVW] = d_a11_i;
      dw_data_c[F_FMT +: 3]  = d_format_i;
      dw_data_c[F_PAL +: 8]  = d_palette_i;
      dw_data_c[F_TINT+: 16] = d_tint_i;
      dw_data_c[F_BLND+: 2]  = d_blend_i;
      dw_data_c[F_VM  +: 2]  = d_view_mask_i;
      dw_data_c[F_ORD +: 8]  = d_order_i;
      dw_data_c[F_SRC +: 16] = d_src_id_i;
      dw_data_c[F_CU  +: UVW] = d_u_i;
      dw_data_c[F_CV  +: UVW] = d_v_i;
      dw_data_c[F_ST  +: 2]   = ST_NEW;
      dw_data_c[F_NEED+: NEEDW] = NEEDW'(0);
    end else if (st_q == S_WB) begin
      dw_en_c   = 1'b1;
      dw_addr_c = idx_q[IW-1:0];
      dw_data_c = cur_q;
    end
  end

  assign dr_addr_c = idx_q[IW-1:0];

  // ==========================================================================
  // THE SEQUENTIAL HALF
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    // Declared INSIDE the block. `check_quartus17_syntax.py` FORM 7: a
    // module-scope loop variable assigned inside a procedural block nested in a
    // conditional infers a latch and Quartus refuses the whole design, and the
    // lint here stays clean while it does.
    integer ri;
    logic [LB-1:0] ft_row;
    if (!rst_n) begin
      st_q            <= S_IDLE;
      idx_q           <= '0;
      count_q         <= '0;
      cur_q           <= '0;
      top_q           <= '0;
      rows_band_q     <= '0;
      rows_left_q     <= '0;
      adv_q           <= '0;
      last_band_q     <= 1'b0;
      e_valid_q       <= 1'b0;
      fill_band_q     <= '0;
      rd_band_q       <= '0;
      band_open_q     <= 1'b0;
      band_top_q      <= '0;
      outstanding_q   <= '0;
      last_src_q      <= '0;
      last_order_q    <= '0;
      last_src_v_q    <= 1'b0;
      rate_q          <= '0;
      burst_q         <= NEEDW'(BURST_PX);
      mul_a_q         <= '0;
      mul_b_q         <= '0;
      mul_acc_q       <= '0;
      mul_sat_q       <= 1'b0;
      mul_busy_q      <= 1'b0;
      rd_v_q          <= 1'b0;
      rd_ok_q         <= 1'b0;
      rd_first_q      <= 1'b0;
      rd_gen_q        <= '0;
      rd_addr_q       <= '0;
      rd_count_q      <= '0;
      rd_hold_v_q     <= 1'b0;
      rd_hold_rgb_q   <= '0;
      for (ri = 0; ri < L; ri = ri + 1) gen_q[ri] <= GENW'(0);
      descriptors_o            <= '0;
      desc_overflow_o          <= '0;
      sprites_admitted_o       <= '0;
      sprites_refused_budget_o <= '0;
      slices_emitted_o         <= '0;
      pixels_written_o         <= '0;
      pixels_clipped_o         <= '0;
      write_oob_o              <= '0;
      band_underrun_o          <= '0;
      scan_addr_mismatch_o     <= '0;
      tint_dropped_o           <= '0;
      blend_dropped_o          <= '0;
      order_inversion_o        <= '0;
      bands_o                  <= '0;
    end else begin
      // ---- the read side, every cycle ------------------------------------
      rd_v_q     <= rd_req_v_i;
      rd_ok_q    <= rd_ok_c;
      rd_gen_q   <= gen_q[rd_slot_c];
      rd_addr_q  <= br_addr_c;
      // "FIRST" MEANS FIRST FOR THIS REQUEST, NOT MERELY A NEW ADDRESS. The
      // compositor's raster pointer sits at (0, 0) while it is idle, so the
      // first pixel of a sweep offers the address that was ALREADY on the port
      // -- and an address-change test alone never captures it. Found by the
      // directed bench: pixel (0, 0) read back dark with every counter at zero,
      // because nothing counts a response that was never latched.
      rd_first_q <= rd_req_v_i && ((br_addr_c != rd_addr_q) || !rd_v_q);
      if (rd_req_v_i) begin
        rd_band_q <= rd_band_c;
        // THE INSTRUMENT. A counter that tracks the sweep it was PROMISED,
        // compared against the address actually offered. The two sides are
        // clocked by different things in different modules, so the comparison
        // can see a timing fault and not merely a value one. It is resynced --
        // never merely silenced -- at the sweep's own origin, so the first
        // pixel of a pass is not a false alarm.
        if (sweep_sync_c) begin
          rd_count_q <= AW'(1);
        end else begin
          if (br_addr_c != rd_count_q)
            scan_addr_mismatch_o <= scan_addr_mismatch_o + 32'd1;
          rd_count_q <= (rd_count_q == AW'(WORDS - 1)) ? AW'(0) : (rd_count_q + AW'(1));
        end
        if (!rd_ok_c) band_underrun_o <= band_underrun_o + 32'd1;
      end
      // The response is captured ONCE, when the address first appears. Holding
      // it here is what makes a compositor stall safe without a handshake.
      if (rd_first_q) begin
        rd_hold_v_q   <= rd_v_q && rd_ok_q && (br_data_q[DW-1 -: GENW] == rd_gen_q)
                                 && (rd_gen_q != GENW'(0));
        rd_hold_rgb_q <= br_data_q[15:0];
      end

      // ---- intake ---------------------------------------------------------
      if (d_valid_i) begin
        if (!list_full_c) begin
          count_q       <= count_q + (IW+1)'(1);
          descriptors_o <= descriptors_o + 32'd1;
        end else begin
          // R235's shape, at the other door: the tail is dropped WHOLE, before
          // rasterising, and COUNTED.
          desc_overflow_o <= desc_overflow_o + 32'd1;
        end
      end

      // ---- the colour write port ------------------------------------------
      if (c_take_c) begin
        if (!c_in_band_c) begin
          write_oob_o <= write_oob_o + 32'd1;
        end else if (!c_in_x_c) begin
          pixels_clipped_o <= pixels_clipped_o + 32'd1;
        end else begin
          pixels_written_o <= pixels_written_o + 32'd1;
        end
        if (c_tint_i != 16'hFFFF)  tint_dropped_o  <= tint_dropped_o  + 32'd1;
        if (c_blend_i != 2'd0)     blend_dropped_o <= blend_dropped_o + 32'd1;
        // A new sprite has started when its src_id changes. Its `order` must
        // not be below the one that has just finished, or the last write does
        // not win for the sprite the contract says should be on top.
        if (c_src_id_i != last_src_q) begin
          if (last_src_v_q && (c_order_i < last_order_q))
            order_inversion_o <= order_inversion_o + 32'd1;
          last_src_q   <= c_src_id_i;
          last_order_q <= c_order_i;
          last_src_v_q <= 1'b1;
        end
        if (c_last_i && (outstanding_q != KW'(0))) outstanding_q <= outstanding_q - KW'(1);
      end

      // ---- the serial shift-add -------------------------------------------
      if (mul_busy_q) begin
        if (mul_b_q[0]) begin
          mul_acc_q <= mul_acc_q + mul_a_q;
          if (((mul_acc_q + mul_a_q) >>> 31) != (mul_acc_q >>> 31)) mul_sat_q <= 1'b1;
        end
        mul_a_q <= mul_a_q <<< 1;
        mul_b_q <= mul_b_q >> 1;
        if ((mul_b_q >> 1) == 16'd0) mul_busy_q <= 1'b0;
      end

      // ---- restart ---------------------------------------------------------
      if (restart_c) begin
        st_q          <= S_IDLE;
        idx_q         <= '0;
        count_q       <= '0;
        fill_band_q   <= '0;
        rd_band_q     <= '0;
        band_open_q   <= 1'b0;
        band_top_q    <= '0;
        outstanding_q <= '0;
        last_src_v_q  <= 1'b0;
        e_valid_q     <= 1'b0;
        rate_q        <= '0;
        burst_q       <= NEEDW'(BURST_PX);
        rd_hold_v_q   <= 1'b0;
        mul_busy_q    <= 1'b0;
      end else begin
        case (st_q)
          // case0: idle -- open the next band when a slot is free.
          S_IDLE: begin
            if (can_open_c) st_q <= S_OPEN;
          end

          // case1: open -- bump the generation of the band's B rows. THAT IS
          // THE WHOLE CLEAR: B two-bit-and-a-bit counters instead of B*LINE_W
          // write cycles.
          S_OPEN: begin
            band_top_q  <= (YW+1)'(32'(fill_band_q) << BB);
            band_open_q <= 1'b1;
            idx_q       <= '0;
            for (ri = 0; ri < B; ri = ri + 1) begin
              ft_row = LB'(32'(fill_band_q << BB) + ri);
              gen_q[ft_row] <= (gen_q[ft_row] == GENW'(GEN_MAX))
                                 ? GENW'(1) : (gen_q[ft_row] + GENW'(1));
            end
            st_q <= S_REQ;
          end

          // case2: request the next descriptor.
          S_REQ: begin
            if (idx_q >= count_q) begin
              st_q        <= S_CLOSE;
            end else begin
              st_q <= S_WAIT;
            end
          end

          // case3: one cycle of display-list read latency.
          S_WAIT: begin
            cur_q <= dr_data_q;
            st_q  <= S_EVAL;
          end

          // case4: the band-clipped geometry and the first-touch branch.
          S_EVAL: begin
            top_q       <= (YW+1)'(clip_top_c);
            rows_band_q <= (YW+1)'(clip_bot_c - clip_top_c);
            rows_left_q <= (YW+1)'(smin(spr_bot_c, 32'(frame_h_i)) - clip_top_c);
            last_band_q <= sprite_ends_c;
            if ((cd_st_c == ST_FREE) || (cd_st_c == ST_DEAD)) begin
              st_q <= S_NEXT;
            end else if ((cd_vm_c & fill_view_c) == 2'd0) begin
              st_q <= S_NEXT;                       // not for this view: no charge
            end else if (clip_bot_c <= clip_top_c) begin
              // Nothing of this sprite in this band. If it is wholly behind us,
              // release its reservation.
              if (sprite_ends_c) begin
                cur_q[F_ST +: 2] <= ST_DEAD;
                if (cd_st_c == ST_LIVE) begin
                  rate_q  <= rate_q - RATEW'(cd_w_c);
                  burst_q <= ((32'(burst_q) + 32'(cd_need_c)) > 32'(BURST_PX))
                               ? NEEDW'(BURST_PX) : (burst_q + cd_need_c);
                end
                st_q <= S_WB;
              end else begin
                st_q <= S_NEXT;
              end
            end else if (cd_st_c == ST_NEW) begin
              // FAST-FORWARD THE CURSOR. `clip_top_c - cd_y_c` is zero for every
              // sprite whose first live row is its own top row, and the serial
              // unit exits in one cycle on a zero multiplier.
              mul_a_q    <= cd_a01_c;
              mul_b_q    <= 16'(clip_top_c - 32'(cd_y_c));
              mul_acc_q  <= '0;
              mul_sat_q  <= 1'b0;
              mul_busy_q <= (16'(clip_top_c - 32'(cd_y_c)) != 16'd0);
              st_q       <= S_FFU;
            end else begin
              st_q <= S_EMIT;
              e_valid_q <= 1'b1;
            end
          end

          // case5: the u cursor, then start the v cursor.
          S_FFU: begin
            if (!mul_busy_q) begin
              cur_q[F_CU +: UVW] <= cd_cu_c + UVW'(mul_acc_q);
              mul_a_q    <= cd_a11_c;
              mul_b_q    <= 16'(32'(top_q) - 32'(cd_y_c));
              mul_acc_q  <= '0;
              mul_sat_q  <= 1'b0;
              mul_busy_q <= (16'(32'(top_q) - 32'(cd_y_c)) != 16'd0);
              st_q       <= S_FFV;
            end
          end

          // case6: the v cursor, then the admission cost.
          S_FFV: begin
            if (!mul_busy_q) begin
              cur_q[F_CV +: UVW] <= cd_cv_c + UVW'(mul_acc_q);
              mul_a_q    <= 32'({8'd0, excess_c});
              mul_b_q    <= 16'(rows_left_q);
              mul_acc_q  <= '0;
              mul_sat_q  <= 1'b0;
              mul_busy_q <= (excess_c != RATEW'(0)) && (16'(rows_left_q) != 16'd0);
              st_q       <= S_NEED;
            end
          end

          // case7: wait for the cost.
          S_NEED: begin
            if (!mul_busy_q) st_q <= S_ADMIT;
          end

          // case8: R235. THE DECISION IS MADE HERE, BEFORE ONE PIXEL OF THIS
          // SPRITE IS RASTERISED, AND IT IS MADE ONCE FOR THE WHOLE FRAME.
          S_ADMIT: begin
            if (!mul_sat_q && (mul_acc_q <= 32'({{(32-NEEDW){1'b0}}, burst_q}))) begin
              cur_q[F_ST   +: 2]     <= ST_LIVE;
              cur_q[F_NEED +: NEEDW] <= NEEDW'(mul_acc_q);
              rate_q             <= new_rate_c;
              burst_q            <= burst_q - NEEDW'(mul_acc_q);
              sprites_admitted_o <= sprites_admitted_o + 32'd1;
              e_valid_q          <= 1'b1;
              st_q               <= S_EMIT;
            end else begin
              // REFUSE THE SPRITE WHOLE, AND COUNT IT. ST_DEAD is sticky for
              // the frame, so no later band draws a row of it either.
              cur_q[F_ST +: 2] <= ST_DEAD;
              sprites_refused_budget_o <= sprites_refused_budget_o + 32'd1;
              st_q <= S_WB;
            end
          end

          // case9: hand the slice to the walker. It is held until accepted and
          // never withdrawn.
          S_EMIT: begin
            if (e_ready_i) begin
              e_valid_q        <= 1'b0;
              slices_emitted_o <= slices_emitted_o + 32'd1;
              outstanding_q    <= outstanding_q + KW'(1);
              adv_q            <= 3'(rows_band_q);
              st_q             <= S_ADV;
            end
          end

          // case10: fold the emitted rows into the cursor -- ONE ADD PER ROW,
          // at most B of them. THE CURSOR NEEDS NO MULTIPLY (R233): it
          // accumulates across bands exactly as the walker accumulates across
          // rows inside a slice.
          S_ADV: begin
            if (adv_q != 3'd0) begin
              cur_q[F_CU +: UVW] <= cd_cu_c + cd_a01_c;
              cur_q[F_CV +: UVW] <= cd_cv_c + cd_a11_c;
              adv_q <= adv_q - 3'd1;
            end else begin
              if (last_band_q) begin
                cur_q[F_ST +: 2] <= ST_DEAD;
                rate_q  <= rate_q - RATEW'(cd_w_c);
                burst_q <= ((32'(burst_q) + 32'(cd_need_c)) > 32'(BURST_PX))
                             ? NEEDW'(BURST_PX) : (burst_q + cd_need_c);
              end
              st_q <= S_WB;
            end
          end

          // case11: write the mutable half back. It RETRIES until the port is
          // free; an intake that wins this cycle does not cost the write.
          S_WB: begin
            if (!(d_valid_i && !list_full_c)) st_q <= S_NEXT;
          end

          // case12: next descriptor.
          S_NEXT: begin
            idx_q <= idx_q + (IW+1)'(1);
            st_q  <= S_REQ;
          end

          // case13: the band is closed once every slice it emitted has landed.
          S_CLOSE: begin
            if (outstanding_q == KW'(0)) begin
              band_open_q <= 1'b0;
              fill_band_q <= fill_band_q + KW'(1);
              bands_o     <= bands_o + 32'd1;
              st_q        <= S_IDLE;
            end
          end

          default: st_q <= S_IDLE;
        endcase
      end
    end
  end

endmodule

`default_nettype wire
