// zhao_geom_vattr.sv -- GEOM.VATTR: the vertex-attribute store and its writer.
// Per landed vertex, per view: invw24, u_over_w, v_over_w; per vertex: lit r, g,
// b and alpha. Keyed EXACTLY like the arena, and read by GEOM.REPLAY's own three
// lookups with the arena's own one-clock timing.
//
//     GEOM.VDECODE -- u, v (decode order) ----------------------\
//     GEOM.LIGHT   -- lit r, g, b (decode order) --------------->\
//     GEOM.GROUP_SEQ -- opens (the batch's arenas) -------------->  GEOM.VATTR
//     GEOM.PROJ_LANE -- LANDINGS {arena, index, w, profile} ---->/     |
//     GEOM.REPLAY  -- look_* (the arena's nets) ---------------->/      v
//                                             rep_* (same clock as the arena)
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS: ENTRY I46, OWNER RULING R11, AND R31's REPLAY RATE
// ---------------------------------------------------------------------------
// R11 (provisional): "A vertex-attribute store keyed like the arena, M10K-
// backed, written at the same moment and by the same producers that write the
// arena position (VDECODE/SKIN for u/v; zhao_light_stream per R2 for
// rgb/alpha). The palette/skin/group payloads are NOT widened. Replay reads it
// with the same per-triangle lookups, so there is no timing join."
//
// The join problem entry I46 names -- "keying a value that exists at decode
// time by an arena index that exists only at GEOM.GROUP_SEQ's issue" -- is
// solved by OWNING the index's definition rather than joining two streams:
//
//   * GROUP_SEQ's arena index IS the vertex's ordinal among the batch's
//     DECODED vertices: it counts skinned vertices, SKIN is one-in-one-out in
//     order, and a record VDECODE refuses is a hole that advances nothing
//     (R31; such a batch is poisoned and never read). This block counts the
//     SAME events -- decoded vertices, in the same order -- so the ordinal it
//     stages u/v and colour under is that index by construction, not by a
//     comparison between two streams.
//   * The per-VIEW attributes are written at the one moment {arena, index}
//     exists as a fact rather than a count: the LANDING, when the projector's
//     result writes the arena position. The rider on that landing is the
//     arena address, so the store row is the arena row, by the same bits.
//
// And R31 moved GEOM.DEPTHQUANT here. REPLAY ran three DEPTHQUANT lanes per
// TRIANGLE CORNER -- 56 clocks per view-triangle, dominated by the reciprocal
// round trip, 2.7x the frame at the 120,000-vertex tier. invw24 is a property
// of the landed VERTEX in its view, so it is computed once per landing on
// `zhao_geom_depthquant_stream` (one law, see that file), stored beside the
// perspective attributes that need it, and REPLAY simply reads it.
//
// ---------------------------------------------------------------------------
// THE PERSPECTIVE ATTRIBUTE LAW, DERIVED -- NOT CHOSEN
// ---------------------------------------------------------------------------
// spec/qformats.md 8 fixes the per-pixel recovery `u = rescale((s64)u_over_w *
// rcp_u24(invw24_interp))`, and `zhao_raster_perspuv` derives its shift from
// three published formats: u_over_w S8.24, invw24 U0.24, the TMU's coordinate
// S15.16. The per-VERTEX side is the inverse of that recovery on the same
// three formats, so it is not free either:
//
//     value(u_over_w) = value(u) * value(invw24)
//     raw(u_over_w)   = (raw(u)/2^16) * (invw24/2^24) * 2^24
//                     = rescale_s(sext(u) * invw24, 16)          (qformats 4)
//
// with u the record's s16 UV field in fx16 (GEOM.VDECODE's layout),
// sign-extended to the TMU's S15.16. |u| < 2^15 and invw24 < 2^24 bound the
// product below 2^39, so the result fits s24 and NO saturation case exists.
// The reference is `zref::geom_over_w` (zref_depth.hpp), written with this
// block; tests/geometry/geom_vattr_directed.cpp differences every row against it
// and against `zref::depth_of_raw` for invw24.
//
// ---------------------------------------------------------------------------
// COLOUR AND ALPHA
// ---------------------------------------------------------------------------
// r, g, b are `zhao_light_stream`'s lit channels (17 bits, 1.0 = 0x10000),
// zero-extended into their 32-bit slots unchanged. ALPHA HAS NO PRODUCER: the
// format-0 vertex record carries none (GEOM.VDECODE's layout) and GEOM.LIGHT
// emits none, so R11's "rgb/alpha from zhao_light_stream" names a quantity that
// does not exist. The slot is the named constant ALPHA_C, fx16 1.0 (opaque) --
// an editable value, not a derived one (CLAUDE.md rule 6), recorded as a
// decision in the geom2 findings.
//
// Colour arrives once per VERTEX but is read per VIEW, so each lit result is
// written into every arena the batch opened (one or two rows). It may arrive
// before those arenas are known (GEOM.GROUP_SEQ's allocation can wait while the
// normal path runs ahead), so `lit_ready_o` holds until the batch's opens are
// in -- backpressure into GEOM.LIGHT, which nothing upstream waits on.
//
// ---------------------------------------------------------------------------
// THE BATCH IS COMPLETE ONLY WHEN THE STORE SAYS SO (`done_o`)
// ---------------------------------------------------------------------------
// GEOM.GROUP_SEQ seals on LANDINGS; the rows those landings owe are written
// here ~45 clocks later (the reciprocal), and the last colour can be later
// still. A handle released at the seal would let REPLAY read rows that are not
// written yet -- a stale-attribute race that no pixel count would ever show.
// So the composer gates the handle on `done_o`: every landing of the batch has
// its row, and every decoded vertex has its colour.
//
// ---------------------------------------------------------------------------
// THE MEMORY TRADE (standing owner direction: M10K is the slack, ALMs are not)
// ---------------------------------------------------------------------------
// Rows = ARENAS * VSLOTS = 4 * 64 = 256 (GEOM.ASSETFETCH's MAX_VERTICES bounds
// a batch). Per-view plane 24+32+32 = 88 bits, colour plane 3*17 = 51 bits, the
// u/v stage 64 x 32. All three are single-write, single-read, synchronous-read
// arrays -- the M10K template -- so the state is ~6 M10K rather than ~35,000
// flops. The arithmetic is ONE shared 16x24 product, used twice per row (u then
// v): time-multiplexed, because rows arrive at most one per four clocks.
// `zhao_raster_rcp24_v4` is the tree's latest reciprocal (a second INSTANCE of
// the one law -- the `zhao_field_isqrt` precedent), at NCTX 16.
//
// ---------------------------------------------------------------------------
// FAULTS, COUNTED
// ---------------------------------------------------------------------------
//   lq_overflow_o  a landing arrived with the landing queue full. The lane's
//                  result port has no backpressure, so it is DROPPED -- and
//                  counted as a row so `done_o` cannot hang on it. Reachable by
//                  a landing burst above the reciprocal's rate; fired by
//                  stimulus in the directed test.
//   index_oob_o    a landing outside ARENAS x VSLOTS: no row is written (its
//                  colour and u/v are not staged either, silently -- one
//                  event, counted once, here).
//   look_oob_o     a lookup outside ARENAS x VSLOTS; answered with zeros.
//   profile_mixed_o one arena landed under two depth profiles (moved here from
//                  GEOM.REPLAY with the depth law itself).
//   dq_refused_o / dq_stray_o  the depth stream's own, forwarded (stray is
//                  fired in geom_depthquant_stream_directed case 2).
//   poison_o       not a counter: the batch lost a row (a drop or an
//                  out-of-store landing), ORed into REPLAY's handle poison.
//   uv_waits_o     not a fault: depth results that waited for their u/v.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_geom_vattr #(
    parameter int unsigned ARENAS   = 4,
    parameter int unsigned ARENA_W  = 3,
    parameter int unsigned INDEX_W  = 12,
    // Rows per arena: the most vertices one batch can hold (GEOM.ASSETFETCH's
    // MAX_VERTICES). A power of two, so a row address is {arena, index}.
    parameter int unsigned VSLOTS   = 64,
    parameter int unsigned RCP_NCTX = 16,
    parameter int unsigned DQ_SLOTS = 16,
    // The landing queue. The lane's result port cannot stall, so this absorbs
    // bursts (two views of one vertex land back to back) against a depth
    // stream that takes one landing a clock while a context is free.
    parameter int unsigned LQ_DEPTH = 8,
    // Opaque, fx16 1.0: format 0 carries no alpha (see the header).
    parameter logic [31:0] ALPHA_C  = 32'h0001_0000
) (
    input  wire clk,
    input  wire rst_n,

    // ---- the batch: the dispatcher took a meshlet; its records start --------
    input  wire                     batch_i,
    input  wire [1:0]               batch_views_i,   // its visible mask

    // ---- GEOM.GROUP_SEQ's opens: the batch's arenas, in slot order ----------
    input  wire                     op_valid_i,
    input  wire [ARENA_W-1:0]       op_arena_i,

    // ---- u, v at decode: one per DECODED vertex, in batch order -------------
    input  wire                     uv_valid_i,
    input  wire signed [15:0]       uv_u_i,
    input  wire signed [15:0]       uv_v_i,

    // ---- the lit colour: one per decoded vertex, in batch order -------------
    input  wire                     lit_valid_i,
    output wire                     lit_ready_o,
    input  wire [16:0]              lit_r_i,
    input  wire [16:0]              lit_g_i,
    input  wire [16:0]              lit_b_i,

    // ---- the landing: the arena position is written now ---------------------
    input  wire                     fl_valid_i,
    input  wire [ARENA_W-1:0]       fl_arena_i,
    input  wire [INDEX_W-1:0]       fl_index_i,
    input  wire [30:0]              fl_w_i,
    input  wire [1:0]               fl_profile_i,

    // ---- every write the current batch owes is in the store -----------------
    output wire                     done_o,
    // The batch lost a row -- a landing dropped by the full queue, or outside
    // the store -- so some corner REPLAY reads for it is not this batch's.
    // The composer ORs it into the handle's poison beside GROUP_SEQ's (R31:
    // the batch drops, not the frame). Cleared at the next `batch_i`.
    output logic                    poison_o,

    // ---- lookups: GEOM.REPLAY's own, as the arena accepted them -------------
    input  wire                     look_valid_i,
    input  wire [ARENA_W-1:0]       look_arena_i,
    input  wire [INDEX_W-1:0]       look_index_i,
    output logic                    rep_valid_o,
    output logic [23:0]             rep_invw24_o,
    // {alpha, b, g, r, v_over_w, u_over_w}, 32 bits a slot, low slot first --
    // the ruling-5 packet's slots 1..6 in order.
    output logic [6*32-1:0]         rep_data_o,

    // ---- evidence ------------------------------------------------------------
    output logic [31:0]             landings_o,
    output logic [31:0]             rows_written_o,
    output logic [31:0]             colours_written_o,
    output logic [31:0]             uv_staged_o,
    output logic [31:0]             lq_overflow_o,
    output logic [31:0]             index_oob_o,
    output logic [31:0]             look_oob_o,
    output logic [31:0]             profile_mixed_o,
    output logic [31:0]             dq_refused_o,
    output logic [31:0]             dq_stray_o,
    // A depth result whose vertex's u/v was NOT yet staged: the row writer
    // WAITS for it (see "u/v BEFORE ITS ROW") and this counts each such result
    // once. Not a fault -- the wait makes the row right -- but the evidence
    // that the join is a HANDSHAKE rather than a timing assumption.
    output logic [31:0]             uv_waits_o
);

  localparam int unsigned VIW  = $clog2(VSLOTS);
  localparam int unsigned AW   = $clog2(ARENAS);
  localparam int unsigned ROWS = ARENAS * VSLOTS;
  localparam int unsigned RW   = AW + VIW;
  localparam int unsigned NA   = 1 << ARENA_W;
  localparam int unsigned TAGW = ARENA_W + INDEX_W;
  localparam int unsigned LQW  = (LQ_DEPTH <= 2) ? 1 : $clog2(LQ_DEPTH);

  initial begin
    if ((VSLOTS & (VSLOTS - 1)) != 0 || VSLOTS < 2)
      $fatal(1, "zhao_geom_vattr: VSLOTS (%0d) must be a power of two >= 2", VSLOTS);
    if ((ARENAS & (ARENAS - 1)) != 0 || ARENAS < 2)
      $fatal(1, "zhao_geom_vattr: ARENAS (%0d) must be a power of two >= 2", ARENAS);
    if (ARENAS > NA)
      $fatal(1, "zhao_geom_vattr: ARENAS (%0d) exceeds what ARENA_W (%0d) can name", ARENAS, ARENA_W);
    if (VSLOTS > (1 << INDEX_W))
      $fatal(1, "zhao_geom_vattr: VSLOTS (%0d) exceeds what INDEX_W (%0d) can name", VSLOTS, INDEX_W);
    if ((LQ_DEPTH & (LQ_DEPTH - 1)) != 0 || LQ_DEPTH < 2)
      $fatal(1, "zhao_geom_vattr: LQ_DEPTH (%0d) must be a power of two >= 2", LQ_DEPTH);
  end

  // A row address, or "no row" when {arena, index} is outside the store.
  function automatic logic in_store(input logic [ARENA_W-1:0] a, input logic [INDEX_W-1:0] ix);
    in_store = (32'(a) < ARENAS) && (32'(ix) < VSLOTS);
  endfunction
  // Only the low bits address a row; in_store has already judged the rest.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic [RW-1:0] row_of(input logic [ARENA_W-1:0] a, input logic [INDEX_W-1:0] ix);
    row_of = {a[AW-1:0], ix[VIW-1:0]};
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  // ==========================================================================
  // THE BATCH: its expected views, its arenas, its ordinals
  // ==========================================================================
  logic [1:0]         views_need_q;   // 0..2 arenas the batch will open
  logic [1:0]         op_n_q;         // opens seen this batch
  logic [ARENA_W-1:0] sl_arena_q [2];
  logic [VIW:0]       uv_ord_q;       // decoded vertices staged this batch
  logic [VIW:0]       lit_ord_q;      // colours taken this batch
  logic [31:0]        lands_b_q;      // landings this batch (all, incl. dropped)
  logic [31:0]        rows_b_q;       // rows finished this batch (incl. dropped)

  wire [1:0] views_c = 2'(batch_views_i[0]) + 2'(batch_views_i[1]);

  // THE BATCH'S ORDINALS AS SEEN THIS CLOCK. On the `batch_i` clock they are
  // already the NEW batch's zeros, so an event coinciding with the boundary is
  // counted, addressed and stored as the new batch's first -- the arrays and
  // the ordinals cannot disagree (review of d52ae6c0: they used to, the arrays
  // taking the old ordinal while the counters held). In composition no event
  // coincides: the job accept that raises `batch_i` precedes the meshlet's
  // first vertex record (GEOM.ASSETFETCH's S_HAND -> S_SERVE), and `done_o`
  // gates the previous batch's handle, so its last event is already in.
  wire [1:0]   op_n_c    = batch_i ? 2'd0 : op_n_q;
  wire [VIW:0] uv_ord_c  = batch_i ? '0   : uv_ord_q;
  wire [VIW:0] lit_ord_c = batch_i ? '0   : lit_ord_q;
  wire [31:0]  lands_b_c = batch_i ? '0   : lands_b_q;
  wire [31:0]  rows_b_c  = batch_i ? '0   : rows_b_q;

  // ==========================================================================
  // THE u/v STAGE: decode order, keyed by ordinal == the arena index
  // ==========================================================================
  logic [31:0] uv_mem [VSLOTS];
  logic [VIW-1:0] uv_ra;
  logic [31:0] uv_rd_q;

  // ==========================================================================
  // THE LANDING QUEUE, and the per-arena profile evidence
  // ==========================================================================
  logic [TAGW-1:0] lq_tag_q  [LQ_DEPTH];
  logic [30:0]     lq_w_q    [LQ_DEPTH];
  logic [1:0]      lq_prof_q [LQ_DEPTH];
  logic [LQW:0]    lq_wr_q, lq_rd_q;
  wire lq_empty_c = (lq_wr_q == lq_rd_q);
  wire lq_full_c  = ((lq_wr_q - lq_rd_q) == (LQW+1)'(LQ_DEPTH));

  logic [1:0]    prof_q [NA];
  logic [NA-1:0] prof_seen_q;

  // ==========================================================================
  // THE DEPTH STREAM and its reciprocal
  // ==========================================================================
  wire             dq_v_ready;
  wire             dq_d_valid;
  logic            dq_d_ready;
  wire [23:0]      dq_invw;
  wire [TAGW-1:0]  dq_tag;
  wire             rcp_v_valid, rcp_v_ready, rcp_r_valid, rcp_r_ready;
  wire [23:0]      rcp_d, rcp_r;
  wire [5:0]       rcp_k;
  wire [7:0]       rcp_v_tok, rcp_r_tok;
  /* verilator lint_off UNUSEDSIGNAL */
  // Throughput observers and the law's normal clamps: read by the directed
  // test through the hierarchy, not faults.
  wire [31:0] dq_vertices, dq_near, dq_far, dq_sat;
  wire        dq_idle;
  wire        rcp_d_zero, rcp_qerr, rcp_idle;
  wire [31:0] rcp_accepted, rcp_completed, rcp_mul_jobs, rcp_zero_jobs,
              rcp_phase_jobs, rcp_negcorr_jobs;
  wire [5:0]  rcp_occupancy;
  /* verilator lint_on UNUSEDSIGNAL */

  wire lq_pop_c = !lq_empty_c && dq_v_ready;

  zhao_geom_depthquant_stream #(
      .TAGW (TAGW),
      .NSLOT(DQ_SLOTS)
  ) u_dq (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (!lq_empty_c),
      .v_ready_o     (dq_v_ready),
      .v_w_i         ({9'd0, lq_w_q[lq_rd_q[LQW-1:0]]}),
      .v_profile_i   (lq_prof_q[lq_rd_q[LQW-1:0]]),
      .v_tag_i       (lq_tag_q[lq_rd_q[LQW-1:0]]),
      .d_valid_o     (dq_d_valid),
      .d_ready_i     (dq_d_ready),
      .d_invw24_o    (dq_invw),
      .d_tag_o       (dq_tag),
      .rcp_valid_o   (rcp_v_valid),
      .rcp_ready_i   (rcp_v_ready),
      .rcp_d_o       (rcp_d),
      .rcp_tok_o     (rcp_v_tok),
      .rcp_rvalid_i  (rcp_r_valid),
      .rcp_rready_o  (rcp_r_ready),
      .rcp_r_i       (rcp_r),
      .rcp_k_i       (rcp_k),
      .rcp_tok_i     (rcp_r_tok),
      .vertices_o    (dq_vertices),
      .clamped_near_o(dq_near),
      .clamped_far_o (dq_far),
      .saturated_o   (dq_sat),
      .refused_o     (dq_refused_o),
      .tok_stray_o   (dq_stray_o),
      .idle_o        (dq_idle)
  );

  zhao_raster_rcp24_v4 #(
      .NCTX(RCP_NCTX),
      .TOKW(8)
  ) u_rcp (
      .clk           (clk),
      .rst_n         (rst_n),
      .v_valid_i     (rcp_v_valid),
      .v_ready_o     (rcp_v_ready),
      .d_i           (rcp_d),
      .v_tok_i       (rcp_v_tok),
      .r_valid_o     (rcp_r_valid),
      .r_ready_i     (rcp_r_ready),
      .r_o           (rcp_r),
      .k_o           (rcp_k),
      .d_zero_o      (rcp_d_zero),
      .r_tok_o       (rcp_r_tok),
      .accepted_o    (rcp_accepted),
      .completed_o   (rcp_completed),
      .mul_jobs_o    (rcp_mul_jobs),
      .zero_jobs_o   (rcp_zero_jobs),
      .phase_jobs_o  (rcp_phase_jobs),
      .negcorr_jobs_o(rcp_negcorr_jobs),
      .occupancy_o   (rcp_occupancy),
      .qerr_o        (rcp_qerr),
      .idle_o        (rcp_idle)
  );

  // ==========================================================================
  // THE ROW WRITER: invw24 in, two products, one row out
  // ==========================================================================
  // W_IDLE takes a depth result and issues the u/v stage read; W_U forms
  // u*invw (the stage word is there now); W_V forms v*invw and writes the row.
  localparam logic [1:0] W_IDLE = 2'd0, W_U = 2'd1, W_V = 2'd2;
  logic [1:0]         w_st_q;
  logic [23:0]        w_invw_q;
  logic [ARENA_W-1:0] w_arena_q;
  logic [INDEX_W-1:0] w_index_q;
  logic signed [31:0] w_uow_q;

  // u/v BEFORE ITS ROW -- A HANDSHAKE, NOT A TIMING (review of d52ae6c0). A
  // row needs its vertex's u/v, staged at decode under the ordinal the landing
  // names. Decode precedes projection for the same vertex in composition, but
  // that is an ORDER between two independent paths, and the first version read
  // `uv_mem` at the landing's index with no check that the ordinal had been
  // staged: a landing that outran its decode would have multiplied the
  // PREVIOUS batch's u/v into this one's row, silently. Now the depth result
  // is not taken until its ordinal is staged (`uv_ord_q` has passed it); an
  // index outside the store has no u/v to wait for (it is index_oob's).
  wire [INDEX_W-1:0] dq_index_c = dq_tag[INDEX_W-1:0];
  wire dq_in_store_c = in_store(dq_tag[TAGW-1 -: ARENA_W], dq_index_c);
  wire uv_have_c     = !dq_in_store_c || (32'(dq_index_c) < 32'(uv_ord_q));
  assign dq_d_ready  = (w_st_q == W_IDLE) && uv_have_c;
  logic uv_wait_seen_q;   // this result's wait is already counted

  // THE ONE MULTIPLIER, and the one rounding (qformats 4, round-half-up).
  logic signed [15:0] mul_a_c;
  logic signed [40:0] mul_p_c;
  logic signed [31:0] mul_r_c;
  always_comb begin
    mul_a_c = (w_st_q == W_U) ? $signed(uv_rd_q[15:0]) : $signed(uv_rd_q[31:16]);
    mul_p_c = 41'(mul_a_c) * $signed({17'd0, w_invw_q});
    // rescale_s(p, 16) = (p + 2^15) >>> 16. |p| < 2^39, so the result is s24
    // and the s32 slot holds it with no saturation case (see the header).
    mul_r_c = 32'((mul_p_c + 41'sd32768) >>> 16);
  end

  // ---- the per-view plane: {invw24, v_over_w, u_over_w} -------------------
  logic [87:0]   pv_mem [ROWS];
  logic          pv_we_c;
  logic [RW-1:0] pv_wa_c;
  logic [87:0]   pv_wd_c;
  wire           w_in_store_c = in_store(w_arena_q, w_index_q);
  always_comb begin
    pv_we_c = (w_st_q == W_V) && w_in_store_c;
    pv_wa_c = row_of(w_arena_q, w_index_q);
    pv_wd_c = {w_invw_q, mul_r_c, w_uow_q};
  end

  assign uv_ra = (w_st_q == W_IDLE) ? dq_tag[VIW-1:0] : w_index_q[VIW-1:0];

  // ==========================================================================
  // THE COLOUR WRITER: one lit result, one row per opened arena
  // ==========================================================================
  localparam logic [1:0] C_IDLE = 2'd0, C_W0 = 2'd1, C_W1 = 2'd2;
  logic [1:0]     c_st_q;
  logic [50:0]    c_rgb_q;
  logic [VIW:0]   c_ord_q;

  assign lit_ready_o = (c_st_q == C_IDLE) && (op_n_q == views_need_q);
  wire lit_take_c = lit_valid_i && lit_ready_o;

  logic [50:0]   cl_mem [ROWS];
  logic          cl_we_c;
  logic [RW-1:0] cl_wa_c;
  wire [ARENA_W-1:0] c_arena_c = (c_st_q == C_W1) ? sl_arena_q[1] : sl_arena_q[0];
  wire               c_in_c    = in_store(c_arena_c, INDEX_W'(c_ord_q));
  always_comb begin
    cl_we_c = ((c_st_q == C_W0) || (c_st_q == C_W1)) && c_in_c;
    cl_wa_c = row_of(c_arena_c, INDEX_W'(c_ord_q));
  end

  // ==========================================================================
  // THE READ PORT: the arena's timing, one clock after the lookup
  // ==========================================================================
  wire [RW-1:0] rd_row_c = row_of(look_arena_i, look_index_i);
  logic [87:0]  pv_rd_q;
  logic [50:0]  cl_rd_q;
  logic         rd_in_q;

  always_ff @(posedge clk) begin
    // The three arrays: one synchronous write and one synchronous read each.
    if (uv_valid_i && (32'(uv_ord_c) < VSLOTS)) uv_mem[uv_ord_c[VIW-1:0]] <= {uv_v_i, uv_u_i};
    uv_rd_q <= uv_mem[uv_ra];
    if (pv_we_c) pv_mem[pv_wa_c] <= pv_wd_c;
    pv_rd_q <= pv_mem[rd_row_c];
    if (cl_we_c) cl_mem[cl_wa_c] <= c_rgb_q;
    cl_rd_q <= cl_mem[rd_row_c];
  end

  always_comb begin
    rep_invw24_o = rd_in_q ? pv_rd_q[87:64] : 24'd0;
    rep_data_o   = '0;
    if (rd_in_q) begin
      rep_data_o[ 31:  0] = pv_rd_q[31:0];                 // u_over_w
      rep_data_o[ 63: 32] = pv_rd_q[63:32];                // v_over_w
      rep_data_o[ 95: 64] = {15'd0, cl_rd_q[16:0]};        // r
      rep_data_o[127: 96] = {15'd0, cl_rd_q[33:17]};       // g
      rep_data_o[159:128] = {15'd0, cl_rd_q[50:34]};       // b
      rep_data_o[191:160] = ALPHA_C;                        // alpha
    end
  end

  // ==========================================================================
  // ROWS FINISHED THIS CLOCK: a written row, an out-of-store row, or a landing
  // dropped by the full queue (a row that will never be written, counted as
  // finished so `done_o` cannot wait for it -- and counted as a FAULT).
  // ==========================================================================
  wire lq_drop_c = fl_valid_i && lq_full_c && !lq_pop_c;
  wire [1:0] rows_inc_c = 2'(lq_drop_c) + 2'(w_st_q == W_V);

  // ==========================================================================
  // DONE: every landing has its row, every decoded vertex its colour
  // ==========================================================================
  wire row_lost_c = lq_drop_c || ((w_st_q == W_V) && !w_in_store_c);
  assign done_o = (rows_b_q == lands_b_q) && (lit_ord_q == uv_ord_q) &&
                  lq_empty_c && dq_idle && (w_st_q == W_IDLE) && (c_st_q == C_IDLE);

  // ==========================================================================
  // THE MACHINE
  // ==========================================================================
  integer ai;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      views_need_q     <= '0;
      op_n_q           <= '0;
      sl_arena_q[0]    <= '0;
      sl_arena_q[1]    <= '0;
      uv_ord_q         <= '0;
      lit_ord_q        <= '0;
      lands_b_q        <= '0;
      rows_b_q         <= '0;
      lq_wr_q          <= '0;
      lq_rd_q          <= '0;
      prof_seen_q      <= '0;
      for (ai = 0; ai < NA; ai = ai + 1) prof_q[ai] <= '0;
      w_st_q           <= W_IDLE;
      w_invw_q         <= '0;
      w_arena_q        <= '0;
      w_index_q        <= '0;
      w_uow_q          <= '0;
      c_st_q           <= C_IDLE;
      c_rgb_q          <= '0;
      c_ord_q          <= '0;
      poison_o         <= 1'b0;
      uv_wait_seen_q   <= 1'b0;
      uv_waits_o       <= '0;
      rep_valid_o      <= 1'b0;
      rd_in_q          <= 1'b0;
      landings_o       <= '0;
      rows_written_o   <= '0;
      colours_written_o<= '0;
      uv_staged_o      <= '0;
      lq_overflow_o    <= '0;
      index_oob_o      <= '0;
      look_oob_o       <= '0;
      profile_mixed_o  <= '0;
    end else begin
      // --- the batch boundary ---------------------------------------------
      // One meshlet is between GEOM.ASSETFETCH and GEOM.REPLAY at a time (the
      // dispatcher takes a meshlet only when REPLAY is idle), so the previous
      // batch's writes are all in and read by now; see the header.
      if (batch_i) begin
        views_need_q <= views_c;
        op_n_q       <= '0;
        uv_ord_q     <= '0;
        lit_ord_q    <= '0;
        lands_b_q    <= '0;
        rows_b_q     <= '0;
      end

      // --- opens: the batch's arenas, and a fresh profile per arena --------
      if (op_valid_i) begin
        if (op_n_c < 2'd2) sl_arena_q[op_n_c[0]] <= op_arena_i;
        op_n_q <= op_n_c + 2'd1;
        prof_seen_q[op_arena_i] <= 1'b0;
      end

      // --- u/v, staged by ordinal ------------------------------------------
      if (uv_valid_i) begin
        // An ordinal past VSLOTS is not staged; the landing it would serve is
        // outside the store too and is counted there, once, on index_oob_o.
        if ((32'(uv_ord_c) < VSLOTS) && (uv_staged_o != 32'hFFFF_FFFF))
          uv_staged_o <= uv_staged_o + 32'd1;
        uv_ord_q <= uv_ord_c + (VIW+1)'(1);
      end

      // --- landings: queued, or dropped and counted ------------------------
      if (fl_valid_i) begin
        if (landings_o != 32'hFFFF_FFFF) landings_o <= landings_o + 32'd1;
        lands_b_q <= lands_b_c + 32'd1;
        if (!prof_seen_q[fl_arena_i]) begin
          prof_q[fl_arena_i]      <= fl_profile_i;
          prof_seen_q[fl_arena_i] <= 1'b1;
        end else if ((prof_q[fl_arena_i] != fl_profile_i) &&
                     (profile_mixed_o != 32'hFFFF_FFFF)) begin
          profile_mixed_o <= profile_mixed_o + 32'd1;
        end
      end
      if (fl_valid_i && !lq_drop_c) begin
        lq_tag_q [lq_wr_q[LQW-1:0]] <= {fl_arena_i, fl_index_i};
        lq_w_q   [lq_wr_q[LQW-1:0]] <= fl_w_i;
        lq_prof_q[lq_wr_q[LQW-1:0]] <= fl_profile_i;
        lq_wr_q <= lq_wr_q + (LQW+1)'(1);
      end
      if (lq_pop_c) lq_rd_q <= lq_rd_q + (LQW+1)'(1);

      // --- rows finished (see rows_inc_c) ------------------------------------
      if (lq_drop_c && (lq_overflow_o != 32'hFFFF_FFFF)) lq_overflow_o <= lq_overflow_o + 32'd1;
      if (w_st_q == W_V) begin
        if (w_in_store_c) begin
          if (rows_written_o != 32'hFFFF_FFFF) rows_written_o <= rows_written_o + 32'd1;
        end else if (index_oob_o != 32'hFFFF_FFFF) begin
          index_oob_o <= index_oob_o + 32'd1;
        end
      end
      if (batch_i || (rows_inc_c != 2'd0)) rows_b_q <= rows_b_c + 32'(rows_inc_c);

      // --- the batch's poison: a row it owes will never be right -------------
      if (batch_i)         poison_o <= row_lost_c;
      else if (row_lost_c) poison_o <= 1'b1;

      // --- a depth result that had to wait for its u/v, counted once ---------
      if (dq_d_valid && (w_st_q == W_IDLE) && !uv_have_c && !uv_wait_seen_q) begin
        uv_wait_seen_q <= 1'b1;
        if (uv_waits_o != 32'hFFFF_FFFF) uv_waits_o <= uv_waits_o + 32'd1;
      end
      if (dq_d_valid && dq_d_ready) uv_wait_seen_q <= 1'b0;

      // --- the row writer ----------------------------------------------------
      case (w_st_q)
        W_IDLE: if (dq_d_valid && dq_d_ready) begin   // the SAME handshake the stream sees
          w_invw_q  <= dq_invw;
          w_arena_q <= dq_tag[TAGW-1 -: ARENA_W];
          w_index_q <= dq_tag[INDEX_W-1:0];
          w_st_q    <= W_U;
        end
        W_U: begin
          w_uow_q <= mul_r_c;
          w_st_q  <= W_V;
        end
        W_V:     w_st_q <= W_IDLE;
        default: w_st_q <= W_IDLE;
      endcase

      // --- the colour writer -------------------------------------------------
      case (c_st_q)
        C_IDLE: if (lit_take_c) begin
          c_rgb_q <= {lit_b_i, lit_g_i, lit_r_i};
          c_ord_q <= lit_ord_c;
          lit_ord_q <= lit_ord_c + (VIW+1)'(1);
          c_st_q  <= (views_need_q == 2'd0) ? C_IDLE : C_W0;
        end
        C_W0: begin
          if (c_in_c && (colours_written_o != 32'hFFFF_FFFF))
            colours_written_o <= colours_written_o + 32'd1;
          c_st_q <= (views_need_q == 2'd2) ? C_W1 : C_IDLE;
        end
        C_W1: begin
          if (c_in_c && (colours_written_o != 32'hFFFF_FFFF))
            colours_written_o <= colours_written_o + 32'd1;
          c_st_q <= C_IDLE;
        end
        default: c_st_q <= C_IDLE;
      endcase

      // --- the read port's valid, the arena's one-clock timing -------------
      rep_valid_o <= look_valid_i;
      rd_in_q     <= look_valid_i && in_store(look_arena_i, look_index_i);
      if (look_valid_i && !in_store(look_arena_i, look_index_i) &&
          (look_oob_o != 32'hFFFF_FFFF))
        look_oob_o <= look_oob_o + 32'd1;
    end
  end

endmodule : zhao_geom_vattr

`default_nettype wire
