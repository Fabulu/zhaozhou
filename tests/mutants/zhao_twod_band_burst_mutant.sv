// zhao_twod_band_burst_mutant.sv -- the positive control for `band_underrun_o`,
// which no legal stimulus can fire in the composed console.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// Owner ruling R235 makes the band REFUSE A SPRITE WHOLE when the leaky bucket
// cannot cover it, and `zhao_twod_band.sv`'s header argues that this is exactly
// what keeps the filler within `BURST_PX` pixels of the reader -- i.e. what
// makes `band_underrun_o` unreachable. That argument is the design's own, and
// `CLAUDE.md` is explicit about what an argument is worth here:
//
//   "A detector reading zero is a claim, and it is the claim to check hardest.
//    Fire it deliberately on a fault it SHOULD catch before quoting its
//    silence."
//
// The directed bench CAN fire the counter by starving the sampler, and does.
// That proves the INSTRUMENT works. It does not prove the LAW is load-bearing,
// because a starved sampler is a fault on a different port. This file is the
// other half: with the admission test effectively removed, a perfectly healthy
// sampler still underruns.
//
// ---------------------------------------------------------------------------
// IT IS A WRAPPER, NOT A COPY -- AND THAT IS THE WHOLE POINT
// ---------------------------------------------------------------------------
// CLAUDE.md: "a wrapper that instantiates the production module cannot drift,
// and must not be counted as a copy". There is no copied BODY here: everything
// below the port list is one instantiation of `zhao_twod_band` with `.*`, so a
// port gained or lost in production FAILS TO ELABORATE here and says so. That
// is the opposite of a stale copy, which fails by continuing to pass.
//
// THE ONE SUBSTANTIVE CHANGE:   BURST_PX = (L - B) * LINE_W  ->  1,000,000
//
// That is legal to write because `BURST_PX` is a real design knob -- the size of
// the leaky bucket -- and `zhao_twod_band.sv`'s own parameter comment says in as
// many words that raising it past the FIFO slack removes the guarantee. At a
// million pixels every sprite is admitted, `sprites_refused_budget_o` can never
// move, and the filler is free to fall arbitrarily far behind the sweep.
//
// THE PORT LIST IS GENERATED FROM PRODUCTION'S, VERBATIM. Regenerate it if
// `zhao_twod_band`'s parameter or port block changes: take the module line
// through the closing `);`, rename the module, and edit the one BURST_PX line.
// `tools/design/wrapper_port_parity.py` checks both directions.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_twod_band_burst_mutant #(

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
    parameter int unsigned BURST_PX = 32'd1000000
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
    input  var logic                    list_busy_i,
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
    output var logic [31:0]             bands_o,
    // Added 2026-09-22 with the production ports. THIS WRAPPER CANNOT DRIFT IN
    // ITS BODY -- it instantiates production with `.*` -- but it CAN go stale
    // in its PORT LIST, and `.*` turns that into an elaboration error rather
    // than a silent tie-off. `tools/design/wrapper_port_parity.py` checks the
    // half `.*` cannot.
    output var logic                    list_restart_o,
    output var logic [31:0]             desc_mid_sweep_o
);

  zhao_twod_band #(
      .LINE_W   (LINE_W),
      .MAX_H    (MAX_H),
      .B        (B),
      .L        (L),
      .MAX_DESC (MAX_DESC),
      .UVW      (UVW),
      .GENW     (GENW),
      .BURST_PX (BURST_PX)
  ) u_dut (.*);

endmodule

`default_nettype wire
