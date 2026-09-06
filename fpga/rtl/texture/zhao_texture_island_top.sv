// zhao_texture_island_top.sv — the COMPOSED texture island.
// Authored 2026-09-05 (roadmap G1-D).
//
// ===========================================================================
// WHY THIS FILE EXISTS
// ===========================================================================
// Every island number quoted so far is a SUM OF STANDALONE PER-BLOCK FITS, and
// that sum is not the island's size. Two things make it wrong, and they pull in
// opposite directions, so the error does not even have a known sign:
//
//   * every standalone fit wraps its block in VIRTUAL PINS and the registers
//     that feed them, so each row carries I/O cost that vanishes once the block
//     is wired to a neighbour instead of to a pad;
//   * nothing is shared across block boundaries -- no common control, no merged
//     constants, no retiming across the seam.
//
// The tell that the sum is meaningless is already in the census: totalled by
// row it comes to **342 DSP blocks against the device's 112**, which is
// impossible. The rows include mutually exclusive variants (`@lanes2`,
// `@lanes1-io`, `@pre-rearch`, `svcseed2/3`) and no cross-block packing. A
// number that cannot be true is a good place to stop adding.
//
// `zhao_prod_top` does not answer this either, and its own header says so: it
// drives every block from a SEPARATE LFSR, which measures eleven blocks that
// happen to share a die rather than an island that works. The roadmap is blunt
// about the distinction -- it calls that file "a resource-counting harness, not
// the console".
//
// So this top WIRES THE BLOCKS TO EACH OTHER. Every internal signal below is a
// real connection between two island components. The only tie-offs are at the
// island's true external boundary: the fragment stream in from the rasteriser,
// the memory fill interface, the palette upload port, and the fragment stream
// out.
//
// ===========================================================================
// THE DATAFLOW, WHICH IS THE POINT
// ===========================================================================
//
//   depth ---> RCP24 ---> PERSPUV ---> FRAGROB ---> COMBINE.V1 ---> out
//                                        |  ^
//                              tmu req   |  | sample responses
//                                        v  |
//                        TMU_PLAN -> CACHE_PIPE -> RSP_DISPATCH
//                                                    |      |
//                                              bilinear   palette
//                                              BILERP     PALETTE_RES
//                                        |  ^
//                              aux req   |  | aux responses
//                                        v  |
//                                     AUX_PIPE
//
//   MOSAIC sits on the pre-TMU u/v path.
//
// ===========================================================================
// THE THREE PLACES THE BLOCKS DO NOT YET MEET
// ===========================================================================
// These are REAL INTEGRATION FINDINGS and they are named rather than papered
// over. Each is marked `GLUE:` at its site. Glue here means a few gates that a
// block should eventually own, written in the top so the composition is
// honest about where the seam is -- NOT an LFSR standing in for a neighbour,
// which is the harness pattern this file exists to replace.
//
//   1. SAMPLE BANKING -- FOUND AND FIXED PROPERLY, recorded because the wrong
//      fix would have measured fine. FRAGROB appeared to expose one colour, so
//      the first draft held two past retirements in a shift register here and
//      called that a sample bank. It banks all three internally already
//      (`res_rgb_m [3][DEPTH]`); its retire read took `res_rgb_m[0]` and
//      dropped the rest -- the same "returns sample 0 for every recipe" fault
//      MATERIAL.RESOLVE.md attributes to the surviving TEXJOIN, sitting one
//      block upstream of where anyone was looking. FRAGROB now has
//      `o_s_rgb_o[3]`/`o_s_a_o[3]` and this top wires them straight through.
//      The shift-register version would have fitted, reported numbers, and
//      blended each fragment with its two predecessors.
//
//   2. TEXEL-TO-CHANNEL. RSP_DISPATCH hands the bilinear class 64 bits -- four
//      RGB565 texels. BILERP_LANE consumes four EIGHT-BIT channel values plus
//      the two fractions. Nothing between them extracts a channel, and nothing
//      sequences the three channels through the one serial lane. The
//      extraction below does one channel; the three-channel sequencing is NOT
//      built and the lane will under-report work until it is.
//
//   3. RESPONSE CLASS. RSP_DISPATCH needs `rsp_class_i` to route, and
//      CACHE_PIPE does not carry a class alongside its sample data -- only
//      `smp_src_id_o`. The class is therefore carried in the TOP TWO BITS of
//      the source id, which works only because TMU_PLAN's `SRCW` is 16 and the
//      island's live source ids are far below 2^14. That is a real constraint
//      on the id space and it is nowhere written down but here.

module zhao_texture_island_top #(
    parameter int unsigned DEPTH   = 16,   // FRAGROB reorder depth
    parameter int unsigned CTXW    = 64,
    parameter int unsigned BINDW   = 8,
    // LODW IS 8 BECAUSE THE PLANNER'S LOD IS Q4.4, NOT AN INTEGER LEVEL.
    // `zhao_texture_tmu_plan` selects the level with `t0_lod[7:4]` and leaves
    // [3:0] as the FRACTION. This was 4, and the connection below padded it
    // into the LOW nibble -- so every possible value became 0x00..0x0F, the
    // planner's integer level was ALWAYS ZERO, and enabling MIP_EN could not
    // produce a non-zero mip however the binding was set. Everything the
    // planner already implements -- clamping, selected-level UV scaling,
    // packed mip-chain offsets -- was unreachable through that wiring.
    //
    // Inert at this commit: the composed test drives `bind_mode_i = 0`, so
    // MIP_EN is low and `lvl_req` is 0 either way. The fix is here because the
    // FRACTION it recovers is the blend weight the two-level mip blend needs,
    // so the defect and that feature share one repair (docket D23).
    parameter int unsigned LODW    = 8,
    parameter int unsigned GENW    = 8,
    parameter int unsigned LANES   = 4,    // CACHE_PIPE lanes
    parameter int unsigned SRCW    = 16,
    parameter int unsigned DATAW   = 64,   // RSP_DISPATCH payload = LANES*16
    parameter int unsigned TOKW    = 16,
    parameter int unsigned PAL_SLOTS   = 4,
    parameter int unsigned PAL_ENTRIES = 256,
    // AUX_TOKW IS NOT 8, AND THAT WAS AN INTEGRATION BUG. FRAGROB
    // validates a sample response against the slot AND generation it
    // issued, which is $clog2(DEPTH) + GENW = 12 bits. AUX_PIPE's TOKW
    // defaults to 8, so the identity could not round-trip and every aux
    // response came back with the slot sitting where the generation
    // should be. FRAGROB counted them -- 7 ID errors against exactly 7
    // aux requests -- which is how it was found. The token is a
    // parameter; it just had to be told how wide the identity is.
    parameter int unsigned AUX_TOKW = $clog2(DEPTH) + GENW
) (
    input  var logic        clk,
    input  var logic        rst_n,

    // ======================= island boundary: fragments in ==================
    input  var logic        frag_valid_i,
    output var logic        frag_ready_o,
    input  var logic [23:0] frag_depth_i,       // w, for the reciprocal
    input  var logic [31:0] frag_u_over_w_i,
    input  var logic [31:0] frag_v_over_w_i,
    input  var logic [1:0]  frag_sample_count_i,
    input  var logic [BINDW-1:0] frag_binding_i,
    input  var logic [LODW-1:0]  frag_lod_i,
    input  var logic [2:0]  frag_recipe_i,
    input  var logic [7:0]  frag_weight_i,
    input  var logic [CTXW-1:0] frag_ctx_i,
    input  var logic        frag_aux_i,
    input  var logic [23:0] frag_base_rgb_i,
    input  var logic [7:0]  frag_base_a_i,
    // THE SAMPLE CLASS, per fragment. It was a hardcoded CLASS_BILINEAR, which
    // left the palette path wired and permanently idle -- `palette 0` in every
    // composed run. The class belongs at the boundary because in the machine it
    // comes from the material binding, which is upstream of this island.
    input  var logic [1:0]  frag_class_i,

    // THE FRAGMENT'S PALETTE BINDING -- which CLUT it samples and which upload
    // generation it expects. This is a MATERIAL BINDING and belongs to the
    // fragment. It used to be taken from the response routing token, which is
    // a different namespace entirely; see the palette wiring below.
    input  var logic [$clog2(PAL_SLOTS)-1:0] frag_pal_slot_i,
    input  var logic [GENW-1:0]              frag_pal_gen_i,

    // ======================= island boundary: binding table =================
    input  var logic [31:0] bind_base_i,
    input  var logic [31:0] bind_mode_i,

    // ======================= island boundary: memory fill ===================
    output var logic        fill_valid_o,
    input  var logic        fill_ready_i,
    output var logic [31:0] fill_addr_o,
    input  var logic        fill_data_valid_i,
    input  var logic [15:0] fill_data_i,

    // ======================= island boundary: palette upload ================
    input  var logic        pal_ld_valid_i,
    input  var logic [1:0]  pal_ld_op_i,
    input  var logic [$clog2(PAL_SLOTS)-1:0]   pal_ld_slot_i,
    input  var logic [GENW-1:0]                pal_ld_gen_i,
    input  var logic [$clog2(PAL_ENTRIES)-1:0] pal_ld_idx_i,
    input  var logic [15:0] pal_ld_rgb565_i,
    input  var logic        pal_ld_crc_ok_i,

    // ======================= island boundary: aux sheet =====================
    input  var logic        sheet_rvalid_i,
    input  var logic [7:0]  sheet_tag_i,
    input  var logic [7:0]  sheet_str_i,
    input  var logic [AUX_TOKW-1:0] sheet_rtok_i,
    output var logic        sheet_valid_o,
    input  var logic        sheet_ready_i,
    output var logic [5:0]  sheet_u_o,
    output var logic [5:0]  sheet_v_o,
    // THE SHEET REQUEST'S TOKEN, which the first draft left
    // unconnected -- `.sheet_tok_o()`. AUX_PIPE matches a sheet response
    // to its request by this token, so with it dangling the responder
    // had nothing to echo, every aux result carried a wrong identity,
    // and FRAGROB rejected all of them. Because FRAGROB retires in
    // ALLOCATION ORDER, one aux fragment stuck at the head blocked the
    // entire island: 48 samples fetched, 0 fragments out.
    output var logic [AUX_TOKW-1:0] sheet_tok_o,

    // ======================= island boundary: fragments out =================
    output var logic        out_valid_o,
    input  var logic        out_ready_i,
    output var logic [23:0] out_rgb_o,
    output var logic [7:0]  out_a_o,
    output var logic [15:0] out_tag_o,
    output var logic        out_refused_o,
    // Times a fragment finished ahead of an earlier one and had to wait at the
    // boundary (audit R6). Zero on a run means the ordering guarantee was
    // never TESTED, not that it holds.
    output var logic [31:0] cnt_reorder_held_o,

    // ======================= one summary counter per block ==================
    // Deliberately NOT every counter every block owns. Each output is a pin,
    // and 60 counter pins would add I/O registers that inflate the very number
    // this file exists to measure honestly. One per block keeps each block's
    // counter logic alive -- so it is not optimised away and the measurement
    // stays truthful -- without paying for the whole census.
    output var logic [31:0] cnt_fragments_o,
    output var logic [31:0] cnt_cache_hits_o,
    output var logic [31:0] cnt_cache_misses_o,
    output var logic [31:0] cnt_palette_lookups_o,
    output var logic [31:0] cnt_bilerp_jobs_o,
    output var logic [31:0] cnt_mosaic_samples_o,
    output var logic [31:0] cnt_aux_accepted_o,
    output var logic [31:0] cnt_combine_refused_o,
    output var logic [31:0] cnt_rcp_completed_o,
    output var logic [31:0] cnt_persp_fragments_o,
    output var logic [31:0] cnt_dispatch_accepted_o,
    output var logic [31:0] cnt_plan_accepted_o,
    // FRAGROB rejects a sample response whose slot/sidx/generation does
    // not match what it issued. Exposed because a composed island that
    // accepts fragments and retires none is either starved or REJECTING,
    // and those need different fixes.
    output var logic [31:0] cnt_fragrob_id_errors_o,
    // COMBINE.V1's per-recipe product-job counts, at the island boundary.
    //
    // §15.4 requires actual product jobs recorded by recipe, and until now they
    // stopped inside the combiner. Bringing them out is what lets a composed
    // test prove PER-FRAGMENT RECIPE IDENTITY: if the recipe did not travel
    // with its fragment, every fragment would combine with whichever recipe
    // arrived last and exactly one counter would move. That gap was recorded
    // against the ENFORCED-BY note below and this closes it.
    output var logic [31:0] cnt_combine_jobs_o [8],

    // PALETTE RESIDENCY OUTCOME. These two are counters and not diagnostics
    // because "the lookup happened" and "the lookup found its palette" are
    // different facts, and only the first was observable. Every CLUT fragment
    // retired black for a whole pass while `cnt_palette_lookups_o` moved
    // healthily; the fault was 96 lookups all STALE, and nothing exposed that.
    // A path that answers with a miss indication is doing no work.
    output var logic [31:0] cnt_palette_stale_o,
    output var logic [31:0] cnt_palette_cold_o,

    // STICKY: a sample response was produced and nobody took it. One bit and
    // not a count, because the question is "did this ever happen", and one
    // occurrence already means a fragment waits forever. See the completion
    // merger below for why this is currently unreachable and why that is an
    // assumption rather than a property.
    output var logic        err_rsp_dropped_o,

    // Sticky: the bilinear lane retired a channel out of order. The three-
    // channel accumulator pairs R, G and B by ARRIVAL ORDER, so a reordering
    // would silently combine one sample's red with another's blue and every
    // direct-colour pixel would be wrong with no counter moving.
    output var logic        err_bil_chan_o
);

  // ==========================================================================
  // RCP24 -> PERSPUV
  // ==========================================================================
  logic        rcp_r_valid, rcp_r_ready;
  logic [23:0] rcp_r;
  logic [5:0]  rcp_k;
  logic        rcp_dzero;
  logic [7:0]  rcp_tok;
  logic        rcp_v_ready;
  logic [31:0] rcp_accepted, rcp_mul_busy;
  logic [3:0]  rcp_occ;

  // The island's own fragment counter, used as the token so a response can be
  // matched to its request. Eight bits is RCP24's TOKW.
  logic [7:0] tok_r;

  zhao_raster_rcp24_svc #(.NCTX(8), .TOKW(8)) u_rcp (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(frag_valid_i), .v_ready_o(rcp_v_ready),
      .d_i(frag_depth_i), .v_tok_i(tok_r),
      .r_valid_o(rcp_r_valid), .r_ready_i(rcp_r_ready),
      .r_o(rcp_r), .k_o(rcp_k), .d_zero_o(rcp_dzero), .r_tok_o(rcp_tok),
      .accepted_o(rcp_accepted), .completed_o(cnt_rcp_completed_o),
      .mul_busy_o(rcp_mul_busy), .occupancy_o(rcp_occ));

  assign frag_ready_o = rcp_v_ready;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) tok_r <= 8'd0;
    else if (frag_valid_i && frag_ready_o) tok_r <= tok_r + 8'd1;
  end

  // ==========================================================================
  // PER-FRAGMENT ATTRIBUTE CARRIAGE
  // ==========================================================================
  // EVERY per-fragment attribute below used to be tapped straight off this
  // module's input pins at the point it was CONSUMED. That is wrong, and it was
  // wrong for ten separate signals. A fragment spends ~12 clocks in RCP24 and
  // PERSPUV before FRAGROB accepts it, so by the time the tap was read the
  // boundary was presenting a DIFFERENT fragment -- and when the boundary
  // stalled or went idle it presented the same one repeatedly.
  //
  // Measured, in `island_composed_directed`: of 64 fragments submitted, the
  // first one to reach FRAGROB arrived carrying fragment 12's tag, only 25
  // distinct fragments were ever seen, 39 were lost outright, and the last
  // fragment's attributes were delivered 24 times because the input pins simply
  // held their final value. FRAGROB's own slot allocation and ordered retire
  // were PERFECT throughout -- the head slot walked 0..15 exactly four times.
  // The reorder buffer was innocent; it was handed the wrong data.
  //
  // The token needed to fix it already existed and was already carried end to
  // end: `tok_r` is stamped on admission, RCP24 returns it as `r_tok_o`, and
  // PERSPUV carries it through as `tag_o`. Nothing consulted it. So the fix is
  // to store each fragment's attributes at admission and read them back at the
  // two points where its token reappears.
  //
  // Depth 64 against at most RCP24's 8 contexts plus PERSPUV's 16 tokens in
  // flight, so a token cannot wrap onto a live entry. The reads are
  // combinational (MLAB/LUT-RAM, not M10K) because PERSPUV's input handshake is
  // combinational off RCP24's output, and inserting a cycle here would need a
  // skid buffer for no gain.
  localparam int unsigned FCTXN = 64;
  localparam int unsigned FCTXW = 6;
  // The combiner tag carries the caller's 16-bit tag AND the submission
  // sequence, so the island can restore order after a block that retires out
  // of order by design.
  localparam int unsigned ROBTAGW = 16 + FCTXW;

  logic [63:0]           uvw_m   [FCTXN];   // {u_over_w, v_over_w}
  logic [CTXW-1:0]       fctx_m  [FCTXN];   // tag + token + material fields
  logic [31:0]           fbase_m [FCTXN];   // {base_rgb, base_a}
  // ONE NAMED ARRAY PER FIELD, and deliberately not one packed word.
  //
  // The packed form costs nothing to write and one whole defect to find: the
  // palette lookup in this file took its slot and generation from two
  // hand-written slices of the same token that OVERLAPPED, so the "slot" was
  // the low bits of the "generation" and every CLUT fragment came out black
  // while the lookup counter looked healthy. Named fields cannot overlap.
  localparam int unsigned PSW = $clog2(PAL_SLOTS);
  logic [BINDW-1:0] fbind_m [FCTXN];
  logic [LODW-1:0]  flod_m  [FCTXN];
  logic [1:0]       fcls_m  [FCTXN];
  logic             faux_m  [FCTXN];
  logic [PSW-1:0]   fpsl_m  [FCTXN];
  logic [GENW-1:0]  fpgn_m  [FCTXN];
  logic [1:0]       fsc_m   [FCTXN];
  logic [2:0]       frec_m  [FCTXN];
  logic [7:0]       fwt_m   [FCTXN];
  // THE SUBMISSION SEQUENCE NUMBER (audit R6). Stamped at admission and
  // carried out through the combiner so the island can restore the caller's
  // order at its own boundary.
  //
  // It is a counter and not the token, because the token comes from a POOL:
  // a slot is reused as soon as its fragment retires, so token order is
  // allocation order only while nothing has retired yet. A free-running count
  // is submission order by construction.
  //
  // FCTXW bits is exact, not a guess. FRAGROB admits at most FCTXN fragments,
  // so two in flight never differ by FCTXN or more, and the low FCTXW bits
  // therefore distinguish every live fragment. The counter is allowed to wrap.
  logic [FCTXW-1:0] fseq_m  [FCTXN];
  logic [FCTXW-1:0] seq_alloc_r;

  wire [FCTXW-1:0] fc_wp = tok_r[FCTXW-1:0];

  // THE CALLER'S CONTEXT IS STORED VERBATIM AND NEVER REPACKED.
  //
  // An earlier version of this capture wrote the recipe, weight, sample count
  // and token into bits [34:16] of the caller's own context word. That is not
  // free space -- the caller interprets that word as its own data, and the
  // owner's recovery architecture v2 (2.3) rules it out directly: "Packing
  // recipe bits into that word is not a valid way to retain an independently
  // opaque context and world X/Z ... Do not silently overwrite caller-owned
  // bits."
  //
  // The island's own fields live in the named arrays above, and the token
  // travels through FRAGROB as a TYPED FIELD beside the context rather than
  // inside it, so nothing the caller owns is touched.
  // HONEST LIMIT: this island consumes only the low 16 bits of the context,
  // as the fragment tag behind out_tag_o. The remaining bits are stored and
  // carried through FRAGROB intact, but nothing downstream reads them and no
  // port exposes them, so NO TEST CAN OBSERVE that they survive. What is
  // established is the weaker and still necessary property that the island no
  // longer OVERWRITES them. Surfacing them would cost 64 output pins on a
  // block whose timing is already boundary-sensitive, so it waits for a
  // consumer that actually needs them.
  logic [CTXW-1:0] fr_f_ctx_in;
  assign fr_f_ctx_in = frag_ctx_i;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      seq_alloc_r <= '0;
    end else if (frag_valid_i && frag_ready_o) begin
      fseq_m[fc_wp] <= seq_alloc_r;
      seq_alloc_r   <= seq_alloc_r + 1'b1;
    end
  end

  always_ff @(posedge clk) begin
    if (frag_valid_i && frag_ready_o) begin
      uvw_m[fc_wp]   <= {frag_u_over_w_i, frag_v_over_w_i};
      fctx_m[fc_wp]  <= fr_f_ctx_in;
      fbase_m[fc_wp] <= {frag_base_rgb_i, frag_base_a_i};
      fbind_m[fc_wp] <= frag_binding_i;
      flod_m [fc_wp] <= frag_lod_i;
      fcls_m [fc_wp] <= frag_class_i;
      faux_m [fc_wp] <= frag_aux_i;
      fpsl_m [fc_wp] <= frag_pal_slot_i;
      fpgn_m [fc_wp] <= frag_pal_gen_i;
      fsc_m  [fc_wp] <= frag_sample_count_i;
      frec_m [fc_wp] <= frag_recipe_i;
      fwt_m  [fc_wp] <= frag_weight_i;
    end
  end

  // Read point 1: RCP24's answer, for PERSPUV's numerators.
  wire [63:0] uvw_rd = uvw_m[rcp_tok[FCTXW-1:0]];

  logic        pu_valid, pu_ready;
  logic [31:0] pu_u, pu_v;
  logic [15:0] pu_tag;
  logic        pu_sat, pu_dzero;
  logic [31:0] pu_products;
  logic [3:0]  pu_occ;

  zhao_raster_perspuv_svc #(.NTOK(16), .TAGW(16)) u_persp (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(rcp_r_valid), .v_ready_o(rcp_r_ready),
      .u_over_w_i(uvw_rd[63:32]), .v_over_w_i(uvw_rd[31:0]),
      .r_mant_i(rcp_r), .r_k_i(rcp_k), .depth_zero_i(rcp_dzero),
      .tag_i({8'd0, rcp_tok}),
      .r_valid_o(pu_valid), .r_ready_i(pu_ready),
      .u_o(pu_u), .v_o(pu_v), .tag_o(pu_tag), .sat_o(pu_sat),
      .depth_zero_o(pu_dzero),
      .fragments_o(cnt_persp_fragments_o), .products_o(pu_products),
      .occupancy_o(pu_occ));

  // Read point 2: PERSPUV's answer, for everything that consumes a fragment
  // once its texture coordinates exist.
  wire [FCTXW-1:0]      fc_rp    = pu_tag[FCTXW-1:0];
  wire [CTXW-1:0]       fctx_rd  = fctx_m[fc_rp];
  wire [31:0]           fbase_rd = fbase_m[fc_rp];
  wire [BINDW-1:0] f_binding_c  = fbind_m[fc_rp];
  wire [LODW-1:0]  f_lod_c      = flod_m [fc_rp];
  wire [1:0]       f_class_c    = fcls_m [fc_rp];
  wire             f_aux_c      = faux_m [fc_rp];
  wire [PSW-1:0]   f_pal_slot_c = fpsl_m [fc_rp];
  wire [GENW-1:0]  f_pal_gen_c  = fpgn_m [fc_rp];
  wire [1:0]       f_scount_c   = fsc_m  [fc_rp];
  wire [2:0]       f_recipe_c   = frec_m [fc_rp];
  wire [7:0]       f_weight_c   = fwt_m  [fc_rp];

  // ==========================================================================
  // MOSAIC, on the pre-TMU u/v path
  // ==========================================================================
  // It observes the same u/v the TMU planner will use and picks a tile. It is
  // fed rather than bypassed so its logic is real in this measurement; its
  // pick is consumed by the counter only, because the tile selection's
  // consumer (the plan's base address) is a binding-table concern that this
  // composition does not yet own.
  logic mos_req_ready, mos_pick_valid;
  logic [7:0] mos_tile;
  logic [5:0] mos_tx, mos_ty;
  logic [15:0] mos_src;
  logic mos_idle;

  zhao_texture_mosaic u_mosaic (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(pu_valid && pu_ready), .req_ready_o(mos_req_ready),
      .req_u_i(pu_u), .req_v_i(pu_v),
      .req_mat_a_i(fbase_rd[31:24]), .req_mat_b_i(fbase_rd[23:16]),
      .req_weight_i(f_weight_c), .req_mosaic_i(1'b1),
      .req_src_id_i(pu_tag),
      .pick_valid_o(mos_pick_valid), .pick_ready_i(1'b1),
      .pick_tile_o(mos_tile), .pick_tx_o(mos_tx), .pick_ty_o(mos_ty),
      .pick_src_id_o(mos_src), .idle_o(mos_idle),
      .texture_samples_o(cnt_mosaic_samples_o));

  // ==========================================================================
  // FRAGROB — the hub
  // ==========================================================================
  logic        fr_f_ready;
  logic        fr_tmu_valid, fr_tmu_ready;
  logic [31:0] fr_tmu_u, fr_tmu_v;
  logic [BINDW-1:0] fr_tmu_binding;
  logic [LODW-1:0]  fr_tmu_lod;
  logic [$clog2(DEPTH)-1:0] fr_tmu_slot;
  logic [1:0]  fr_tmu_sidx;
  logic [GENW-1:0] fr_tmu_gen;
  logic        fr_tmu_rvalid, fr_tmu_rready;
  logic [23:0] fr_tmu_rgb;
  logic [7:0]  fr_tmu_a;
  logic [$clog2(DEPTH)-1:0] fr_tmu_rslot;
  logic [1:0]  fr_tmu_rsidx;
  logic [GENW-1:0] fr_tmu_rgen;

  logic        fr_aux_valid, fr_aux_ready;
  logic [CTXW-1:0] fr_aux_ctx;
  logic [$clog2(DEPTH)-1:0] fr_aux_slot;
  logic [GENW-1:0] fr_aux_gen;
  logic        fr_aux_rvalid, fr_aux_rready;
  logic [23:0] fr_aux_rgb;
  logic [7:0]  fr_aux_a;
  logic [$clog2(DEPTH)-1:0] fr_aux_rslot;
  logic [GENW-1:0] fr_aux_rgen;

  logic        fr_o_valid, fr_o_ready;
  logic [$clog2(DEPTH)-1:0] fr_alloc_slot;
  logic        fr_alloc_valid;
  logic [CTXW-1:0] fr_o_ctx;
  logic [FCTXW-1:0] fr_o_tok;
  logic [23:0] fr_o_rgb, fr_o_aux_rgb;
  logic [7:0]  fr_o_a, fr_o_aux_a;
  logic        fr_o_has_aux, fr_o_uv_sat;
  logic [23:0] fr_o_s_rgb [3];
  logic [7:0]  fr_o_s_a   [3];
  logic [31:0] fr_samples, fr_full_clocks;
  logic        fr_wq_overflow, fr_id_error, fr_combiner_unfrozen;

  assign pu_ready = fr_f_ready;

  // FRAGROB takes u/v/binding/lod PER SAMPLE -- three of each. The island's
  // boundary supplies one u/v pair (the fragment's) and one binding/lod; the
  // per-sample variation is a binding-table concern this composition does not
  // own yet, so all three samples are given the same coordinates and are
  // distinguished by their binding index. That is a real limitation of the
  // composition, not of FRAGROB, and it is why the binding is offset per
  // sample rather than replicated: three identical bindings would make the
  // cache serve one line three times and understate the miss traffic.
  // THE RECIPE TRAVELS WITH THE FRAGMENT, in FRAGROB's context word.
  //
  // The first version wired the combiner's recipe/weight/sample_count straight
  // from the island's INPUT ports. With a reorder buffer in between, a fragment
  // retiring after N others was combined with whatever recipe happened to be
  // arriving at that moment -- and the composed test still passed every
  // handshake check, because a wrong recipe is a wrong picture, not a stall.
  //
  // ENFORCED-BY: fpga/rtl/texture/zhao_texture_fragrob.sv -- its `ctx_m` array
  // is written at allocation and read at `head_slot_c` on retirement, so the
  // context word leaves FRAGROB with the fragment it was allocated for. This
  // top relies on that and does not re-derive it; the packing below and the
  // unpacking at the combiner instance are the two halves of one layout and
  // are deliberately adjacent in this file so they cannot drift apart.
  //
  // ENFORCED-BY: tests/texture/island_composed_directed.cpp -- it cycles all
  // eight recipes and asserts the per-recipe job counts `cnt_combine_jobs_o`
  // reports. If the recipe did not travel with its fragment, every fragment
  // would combine with whichever recipe arrived last and exactly one counter
  // would move. This was an open gap until the counters were brought out to
  // the boundary in the same pass that re-fits the island.
  //
  // CTXW is 64 and the low 16 bits are the tag, so the material fields ride
  // above it.
  // The context that reaches FRAGROB is the caller's own, stored verbatim at
  // admission and recovered by this fragment's token.
  logic [CTXW-1:0] fr_f_ctx;
  assign fr_f_ctx = fctx_rd;

  logic signed [31:0] fr_f_u [3];
  logic signed [31:0] fr_f_v [3];
  logic [BINDW-1:0]   fr_f_binding [3];
  logic [LODW-1:0]    fr_f_lod [3];
  always_comb begin
    for (int s = 0; s < 3; s++) begin
      fr_f_u[s]       = pu_u;
      fr_f_v[s]       = pu_v;
      fr_f_binding[s] = f_binding_c + BINDW'(s);
      fr_f_lod[s]     = f_lod_c;
    end
  end

  zhao_texture_fragrob #(
      .DEPTH(DEPTH), .CTXW(CTXW), .TOKW_F(FCTXW), .BINDW(BINDW),
      .LODW(LODW), .GENW(GENW)
  ) u_fragrob (
      .clk(clk), .rst_n(rst_n),
      .f_valid_i(pu_valid), .f_ready_o(fr_f_ready),
      .alloc_slot_o(fr_alloc_slot), .alloc_valid_o(fr_alloc_valid),
      .f_sample_count_i(f_scount_c),
      .f_u_i(fr_f_u), .f_v_i(fr_f_v),
      .f_binding_i(fr_f_binding), .f_lod_i(fr_f_lod),
      .f_recipe_i(f_recipe_c), .f_ctx_i(fr_f_ctx), .f_tok_i(fc_rp),
      .f_aux_i(f_aux_c), .f_uv_sat_i(pu_sat),
      .tmu_valid_o(fr_tmu_valid), .tmu_ready_i(fr_tmu_ready),
      .tmu_u_o(fr_tmu_u), .tmu_v_o(fr_tmu_v),
      .tmu_binding_o(fr_tmu_binding), .tmu_lod_o(fr_tmu_lod),
      .tmu_slot_o(fr_tmu_slot), .tmu_sidx_o(fr_tmu_sidx), .tmu_gen_o(fr_tmu_gen),
      .tmu_rvalid_i(fr_tmu_rvalid), .tmu_rready_o(fr_tmu_rready),
      .tmu_rgb_i(fr_tmu_rgb), .tmu_a_i(fr_tmu_a),
      .tmu_rslot_i(fr_tmu_rslot), .tmu_rsidx_i(fr_tmu_rsidx),
      .tmu_rgen_i(fr_tmu_rgen),
      .aux_valid_o(fr_aux_valid), .aux_ready_i(fr_aux_ready),
      .aux_ctx_o(fr_aux_ctx), .aux_slot_o(fr_aux_slot), .aux_gen_o(fr_aux_gen),
      .aux_rvalid_i(fr_aux_rvalid), .aux_rready_o(fr_aux_rready),
      .aux_rgb_i(fr_aux_rgb), .aux_a_i(fr_aux_a),
      .aux_rslot_i(fr_aux_rslot), .aux_rgen_i(fr_aux_rgen),
      .o_valid_o(fr_o_valid), .o_ready_i(fr_o_ready),
      .o_ctx_o(fr_o_ctx), .o_tok_o(fr_o_tok), .o_rgb_o(fr_o_rgb), .o_a_o(fr_o_a),
      .o_s_rgb_o(fr_o_s_rgb), .o_s_a_o(fr_o_s_a),
      .o_aux_rgb_o(fr_o_aux_rgb), .o_aux_a_o(fr_o_aux_a),
      .o_has_aux_o(fr_o_has_aux), .o_uv_sat_o(fr_o_uv_sat),
      .fragments_o(cnt_fragments_o), .samples_o(fr_samples),
      .full_clocks_o(fr_full_clocks), .id_errors_o(cnt_fragrob_id_errors_o),
      .wq_overflow_o(fr_wq_overflow), .id_error_o(fr_id_error),
      .combiner_unfrozen_o(fr_combiner_unfrozen));

  // ==========================================================================
  // FRAGROB -> TMU_PLAN -> CACHE_PIPE
  // ==========================================================================
  // GLUE 3: the response CLASS. See the header. The source id carries the
  // request's identity so a sample can be returned to the right FRAGROB slot;
  // its top two bits carry the class RSP_DISPATCH routes on, because
  // CACHE_PIPE has no class lane of its own.
  // THE CLASS IS KEYED BY SLOT, not read off the input pin. A TMU request
  // carries `fr_tmu_slot` and nothing else that identifies its fragment, and
  // requests are issued long after admission, so the pin holds a later
  // fragment's class. Written when FRAGROB reports where the fragment landed.
  logic [1:0] class_m [DEPTH];
  // The palette binding is keyed the same way and for the same reason: a
  // sample response identifies its fragment by FRAGROB slot and nothing else.
  logic [$clog2(PAL_SLOTS)-1:0] palslot_m [DEPTH];
  logic [GENW-1:0]              palgen_m  [DEPTH];
  always_ff @(posedge clk) begin
    if (fr_alloc_valid) begin
      class_m  [fr_alloc_slot] <= f_class_c;
      palslot_m[fr_alloc_slot] <= f_pal_slot_c;
      palgen_m [fr_alloc_slot] <= f_pal_gen_c;
    end
  end

  // THE RESPONSE ROUTING TOKEN'S FIELDS, named once. The palette wiring below
  // used to slice this word by hand with two overlapping ranges, so nothing
  // caught that they overlapped.
  localparam int unsigned SRC_GEN_LO  = 0;
  localparam int unsigned SRC_SIDX_LO = GENW;
  localparam int unsigned SRC_SLOT_LO = GENW + 2;
  localparam int unsigned SRC_SLOT_HI = SRC_SLOT_LO + $clog2(DEPTH) - 1;

  logic [SRCW-1:0] plan_src_id;
  assign plan_src_id = {class_m[fr_tmu_slot],
                        {(SRCW-2-$clog2(DEPTH)-2-GENW){1'b0}},
                        fr_tmu_slot, fr_tmu_sidx, fr_tmu_gen};

  // THE CLUT BYTE SELECT, CARRIED PER SAMPLE. AUDIT R5 / docket D23.
  //
  // For CLUT8 the planner's address is BYTE granular -- `t3_base + total_c[k]`,
  // one byte per texel -- while CACHE_PIPE returns 16-bit halfwords. Which of
  // the two texels in that halfword was asked for is therefore `addr[0]`, and
  // the palette lookup read `disp_clut_data[7:0]` unconditionally: ALWAYS the
  // low byte, so every odd texel decoded its neighbour's palette index.
  //
  // The selector cannot simply be read at the palette. That lookup happens in
  // the RESPONSE path, keyed by the routing token, long after the request; and
  // `plan_acc_addr` at that moment belongs to whatever request the planner is
  // emitting now. Reading it there would be the late-read defect this island
  // was already repaired for once, and `check_ingress_capture.py` exists
  // because of it.
  //
  // So it travels, keyed by the SAMPLE's identity -- FRAGROB slot and sample
  // index -- exactly as the sample class and the palette binding already do. It
  // is per SAMPLE and not per fragment, because a fragment's three samples have
  // three different coordinates and so three different byte positions.
  //
  // The response token has no spare bits ({class 2, slot 4, sidx 2, gen 8} is
  // exactly SRCW = 16), which is why this is a side table rather than another
  // field in the token.
  // {fv, fu, bytesel} per sample. The byte select was the first field to need
  // carrying; the bilinear FRACTIONS need it for the same reason and were the
  // "additional source-level risk" the audit named without testing:
  //
  //   .fu_i(plan_acc_fu), .fv_i(plan_acc_fv)
  //
  // read the planner's CURRENT fractions while the sample data arrives through
  // cache and dispatch latency. Whatever request the planner happens to be
  // emitting when a response lands supplies that response's weights. With one
  // fragment in flight and uniform coordinates nothing moves; with varying
  // coordinates and a cache miss the filter weights belong to a different
  // texel, and every colour is subtly wrong in a way no counter shows.
  //
  // Same fix as the class, the palette binding and the byte select: store at
  // REQUEST, keyed by the identity the response returns under.
  logic [16:0] sampmeta_m [DEPTH][3];   // {fv[8], fu[8], bytesel[1]}

  logic        plan_req_ready, plan_acc_valid, plan_acc_ready;
  logic [3:0]  plan_acc_en;
  logic [127:0] plan_acc_addr;
  logic [SRCW-1:0] plan_acc_src;

  // Written on the request handshake, indexed by the identity the response will
  // come back under, so the read below cannot pick up a different sample's bit.
  always_ff @(posedge clk) begin
    if (plan_acc_valid && plan_acc_ready)
      sampmeta_m[plan_acc_src[SRC_SLOT_HI:SRC_SLOT_LO]]
                [plan_acc_src[SRC_SIDX_LO+1:SRC_SIDX_LO]] <=
          {plan_acc_fv, plan_acc_fu, plan_acc_addr[0]};
  end
  logic        plan_acc_filter, plan_acc_err;
  logic [7:0]  plan_acc_fu, plan_acc_fv;
  logic [2:0]  plan_acc_fmt;
  logic [3:0]  plan_occ;

  assign fr_tmu_ready = plan_req_ready;

  zhao_texture_tmu_plan #(.SRCW(SRCW)) u_plan (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(fr_tmu_valid), .req_ready_o(plan_req_ready),
      .req_u_i(fr_tmu_u), .req_v_i(fr_tmu_v),
      .req_base_i(bind_base_i), .req_mode_i(bind_mode_i),
      .req_lod_i(fr_tmu_lod),   // already Q4.4; see LODW above
      .req_src_id_i(plan_src_id),
      .acc_valid_o(plan_acc_valid), .acc_ready_i(plan_acc_ready),
      .acc_en_o(plan_acc_en), .acc_addr_o(plan_acc_addr),
      .acc_src_id_o(plan_acc_src), .acc_filter_o(plan_acc_filter),
      .acc_err_o(plan_acc_err), .acc_fu_o(plan_acc_fu), .acc_fv_o(plan_acc_fv),
      .acc_fmt_o(plan_acc_fmt),
      .accepted_o(cnt_plan_accepted_o), .occupancy_o(plan_occ));

  logic        cache_smp_valid, cache_smp_ready;
  logic [LANES*16-1:0] cache_smp_data;
  logic [15:0] cache_smp_src;
  logic [31:0] cache_fills, cache_multicast, cache_replays;

  zhao_texture_cache_pipe #(
      .LANES(LANES), .LINES(16), .LINE_BYTES(16), .REQN(4)
  ) u_cache (
      .clk(clk), .rst_n(rst_n),
      .acc_valid_i(plan_acc_valid), .acc_ready_o(plan_acc_ready),
      .acc_en_i(plan_acc_en), .acc_addr_i(plan_acc_addr),
      .acc_src_id_i(plan_acc_src[15:0]),
      .smp_valid_o(cache_smp_valid), .smp_ready_i(cache_smp_ready),
      .smp_data_o(cache_smp_data), .smp_src_id_o(cache_smp_src),
      .fill_valid_o(fill_valid_o), .fill_ready_i(fill_ready_i),
      .fill_addr_o(fill_addr_o),
      .fill_data_valid_i(fill_data_valid_i), .fill_data_i(fill_data_i),
      .cache_hits_o(cnt_cache_hits_o), .cache_misses_o(cnt_cache_misses_o),
      .fills_o(cache_fills), .multicast_o(cache_multicast),
      .replays_o(cache_replays));

  // ==========================================================================
  // CACHE_PIPE -> RSP_DISPATCH -> {BILERP, PALETTE}
  // ==========================================================================
  logic        disp_rsp_ready;
  logic        disp_clut_valid, disp_clut_ready;
  logic [DATAW-1:0] disp_clut_data;
  logic [TOKW-1:0]  disp_clut_tok;
  logic        disp_near_valid;
  logic [DATAW-1:0] disp_near_data;
  logic [TOKW-1:0]  disp_near_tok;
  logic        disp_bil_valid, disp_bil_ready;
  logic [DATAW-1:0] disp_bil_data;
  logic [TOKW-1:0]  disp_bil_tok;
  logic [31:0] disp_hol;
  logic [2:0]  disp_occ;

  assign cache_smp_ready = disp_rsp_ready;

  zhao_texture_rsp_dispatch #(
      .RAWN(4), .CHN(4), .DATAW(DATAW), .TOKW(TOKW)
  ) u_dispatch (
      .clk(clk), .rst_n(rst_n),
      .rsp_valid_i(cache_smp_valid), .rsp_ready_o(disp_rsp_ready),
      .rsp_data_i(cache_smp_data), .rsp_tok_i(cache_smp_src),
      .rsp_class_i(cache_smp_src[15:14]),   // GLUE 3, see the header
      .clut_valid_o(disp_clut_valid), .clut_ready_i(disp_clut_ready),
      .clut_data_o(disp_clut_data), .clut_tok_o(disp_clut_tok),
      .near_valid_o(disp_near_valid), .near_ready_i(1'b1),
      .near_data_o(disp_near_data), .near_tok_o(disp_near_tok),
      .bil_valid_o(disp_bil_valid), .bil_ready_i(disp_bil_ready),
      .bil_data_o(disp_bil_data), .bil_tok_o(disp_bil_tok),
      .accepted_o(cnt_dispatch_accepted_o), .hol_stall_o(disp_hol),
      .occupancy_o(disp_occ));

  // GLUE 2: texel -> channel. The dispatcher hands four RGB565 texels; the
  // serial bilinear lane consumes four 8-bit channel values. This extracts ONE
  // channel (the low byte of each texel). The three-channel sequencing the
  // lane is designed for -- "serial bilinear CHANNEL engine" -- is NOT built,
  // so the lane's job counter under-reports by a factor of three until it is.
  // Written here rather than silently: an under-reporting counter that nobody
  // has flagged is worse than a missing one.
  logic        bil_out_valid, bil_out_ready;
  logic        bil_lane_valid, bil_lane_ready;
  logic [7:0]  bil_out, bil_lane_out;
  logic [23:0] bil_rgb;
  logic [1:0]  bil_expect_r;
  logic [TOKW-1:0] bil_lane_tok;
  logic [TOKW-1:0] bil_out_tok;
  logic [1:0]  bil_out_chan;
  logic        bil_job_ready;
  logic [1:0]  bil_occ;

  // The metadata belonging to the sample whose texels just arrived.
  wire [16:0] bil_meta = sampmeta_m[disp_bil_tok[SRC_SLOT_HI:SRC_SLOT_LO]]
                                   [disp_bil_tok[SRC_SIDX_LO+1:SRC_SIDX_LO]];

  // ==========================================================================
  // THREE-CHANNEL BILINEAR SEQUENCING. AUDIT R5.
  // ==========================================================================
  // This issued ONE job with `chan_i = 2'd0`, filtered the LOW BYTE of each of
  // the four texel halfwords, and replicated the single result to all three
  // output channels: `{bil_out, bil_out, bil_out}`. Every direct-colour
  // fragment therefore came out grey, and no exact-colour oracle could be
  // written for the bilinear half at all -- which is why R5 could only say the
  // composed test checks "nonzero colours".
  //
  // The lane was always built for this: it carries `chan_i` in and
  // `out_chan_o` out, and never used them. What was missing was the
  // SEQUENCER around it.
  //
  // THE SHAPE. Each cache response carries four RGB565 texels, one per lane.
  // A channel is filtered independently, so one response becomes THREE jobs --
  // R, then G, then B -- each with that channel extracted from all four texels
  // and expanded to 8 bits by REPLICATION, the same law the palette path uses
  // and the one zref_texture.hpp names. The dispatch entry is held until the
  // third job is accepted, so a response is consumed exactly once.
  //
  // COLLECTION IS IN ORDER, and that is a property of the lane rather than an
  // assumption: jobs for one sample are issued back to back and the lane
  // retires in order, so R, G and B return in the order they were issued and
  // carry `out_chan_o` to prove it. The channel is CHECKED on arrival rather
  // than inferred from a counter -- see `err_bil_chan_o`.
  logic [1:0] bil_phase_r;

  // The four texels, as halfwords rather than as their low bytes.
  wire [15:0] btx [4];
  assign btx[0] = disp_bil_data[15:0];
  assign btx[1] = disp_bil_data[31:16];
  assign btx[2] = disp_bil_data[47:32];
  assign btx[3] = disp_bil_data[63:48];

  // Channel extraction with the ABI's replication, per texel.
  function automatic logic [7:0] chan8(input logic [15:0] h, input logic [1:0] c);
    case (c)
      2'd0:    chan8 = {h[15:11], h[15:13]};   // red   5 -> 8
      2'd1:    chan8 = {h[10:5],  h[10:9]};    // green 6 -> 8
      default: chan8 = {h[4:0],   h[4:2]};     // blue  5 -> 8
    endcase
  endfunction

  // Hold the response until its third job is taken.
  assign disp_bil_ready = bil_job_ready && (bil_phase_r == 2'd2);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) bil_phase_r <= 2'd0;
    else if (disp_bil_valid && bil_job_ready)
      bil_phase_r <= (bil_phase_r == 2'd2) ? 2'd0 : (bil_phase_r + 2'd1);
  end

  zhao_texture_bilerp_lane #(.TOKW(TOKW)) u_bilerp (
      .clk(clk), .rst_n(rst_n),
      .job_valid_i(disp_bil_valid), .job_ready_o(bil_job_ready),
      .t00_i(chan8(btx[0], bil_phase_r)),
      .t10_i(chan8(btx[1], bil_phase_r)),
      .t01_i(chan8(btx[2], bil_phase_r)),
      .t11_i(chan8(btx[3], bil_phase_r)),
      // THE SAMPLE'S OWN fractions, recovered by the identity the response
      // came back under -- not whatever the planner is emitting right now.
      .fu_i(bil_meta[8:1]), .fv_i(bil_meta[16:9]),
      .tok_i(disp_bil_tok), .chan_i(bil_phase_r),
      .out_valid_o(bil_lane_valid), .out_ready_i(bil_lane_ready),
      .out_o(bil_lane_out), .out_tok_o(bil_lane_tok), .out_chan_o(bil_out_chan),
      .jobs_o(cnt_bilerp_jobs_o), .occupancy_o(bil_occ));

  // ---- collect R, G, B into one colour --------------------------------------
  // Only the BLUE result presents a fragment sample downstream; R and G are
  // absorbed into the accumulator. So the response port sees one answer per
  // sample, exactly as it did before, and nothing downstream had to change.
  logic [7:0] bil_r_r, bil_g_r;

  assign bil_lane_ready = (bil_out_chan != 2'd2) ? 1'b1 : bil_out_ready;
  assign bil_out_valid  = bil_lane_valid && (bil_out_chan == 2'd2);
  assign bil_out        = bil_lane_out;
  assign bil_out_tok    = bil_lane_tok;
  assign bil_rgb        = {bil_r_r, bil_g_r, bil_lane_out};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      bil_r_r <= 8'd0;
      bil_g_r <= 8'd0;
      err_bil_chan_o <= 1'b0;
    end else if (bil_lane_valid && bil_lane_ready) begin
      case (bil_out_chan)
        2'd0: bil_r_r <= bil_lane_out;
        2'd1: bil_g_r <= bil_lane_out;
        default: ;  // blue is used combinationally above
      endcase
      // THE ORDER IS CHECKED, NOT ASSUMED. If the lane ever retires out of
      // order, the accumulator would silently pair one sample's red with
      // another's blue and every direct-colour pixel would be quietly wrong.
      if (bil_out_chan != bil_expect_r) err_bil_chan_o <= 1'b1;
      bil_expect_r <= (bil_out_chan == 2'd2) ? 2'd0 : (bil_out_chan + 2'd1);
    end
  end

  logic        pal_lu_valid_o;
  logic [15:0] pal_lu_rgb565;
  logic        pal_lu_stale, pal_lu_resident;
  logic [31:0] pal_stale, pal_cold, pal_e0, pal_e1, pal_e2, pal_e3, pal_ok;

  assign disp_clut_ready = 1'b1;  // the palette lookup is unconditional

  zhao_texture_palette_res #(
      .SLOTS(PAL_SLOTS), .ENTRIES(PAL_ENTRIES), .GENW(GENW)
  ) u_palette (
      .clk(clk), .rst_n(rst_n),
      .ld_valid_i(pal_ld_valid_i), .ld_ready_o(),
      .ld_op_i(pal_ld_op_i), .ld_slot_i(pal_ld_slot_i), .ld_gen_i(pal_ld_gen_i),
      .ld_idx_i(pal_ld_idx_i), .ld_rgb565_i(pal_ld_rgb565_i),
      .ld_crc_ok_i(pal_ld_crc_ok_i),
      .lu_valid_i(disp_clut_valid),
      // THE PALETTE'S SLOT AND GENERATION ARE THE FRAGMENT'S BINDING, not the
      // response's routing token. These two ports used to read
      // `disp_clut_tok[$clog2(PAL_SLOTS)-1:0]` and `disp_clut_tok[GENW-1:0]`
      // -- OVERLAPPING slices of the same word, so the "slot" was the low two
      // bits of the "generation", and the generation was FRAGROB's residency
      // counter, which has nothing to do with a palette upload.
      //
      // MEASURED before the repair: 96 lookups, 96 STALE, 0 cold. The slot was
      // resident and the generation never matched, so every CLUT fragment
      // retired black while the lookup counter moved and looked healthy.
      .lu_slot_i(palslot_m[disp_clut_tok[SRC_SLOT_HI:SRC_SLOT_LO]]),
      .lu_gen_i (palgen_m [disp_clut_tok[SRC_SLOT_HI:SRC_SLOT_LO]]),
      // THE ADDRESSED BYTE, not always the low one. See bytesel_m above.
      .lu_idx_i(sampmeta_m[disp_clut_tok[SRC_SLOT_HI:SRC_SLOT_LO]]
                          [disp_clut_tok[SRC_SIDX_LO+1:SRC_SIDX_LO]][0]
                ? disp_clut_data[15:8]
                : disp_clut_data[7:0]),
      .lu_valid_o(pal_lu_valid_o), .lu_rgb565_o(pal_lu_rgb565),
      .lu_stale_o(pal_lu_stale), .lu_resident_o(pal_lu_resident),
      .lookups_o(cnt_palette_lookups_o),
      .stale_o(cnt_palette_stale_o), .cold_o(cnt_palette_cold_o),
      .err_write_outside_o(pal_e0), .err_same_gen_o(pal_e1),
      .err_incomplete_o(pal_e2), .err_crc_o(pal_e3), .loads_ok_o(pal_ok));

  // ---- sample responses back into FRAGROB ---------------------------------
  // The bilinear lane's byte becomes the sample's luminance-carrying channel
  // and the palette's RGB565 is expanded. Which of the two answers a given
  // request is decided by the class the request was tagged with, which is the
  // same two bits GLUE 3 carries.
  // THE RESPONSE IDENTITY MUST COME FROM WHICHEVER PATH ANSWERED.
  //
  // This derived slot/sidx/generation from `bil_out_tok` unconditionally, so a
  // palette answer carried the BILINEAR path's identity. FRAGROB rejected it
  // and nothing retired. The bug survived every earlier composed run because
  // the island tagged every request bilinear, leaving the CLUT path wired and
  // permanently idle -- `palette 0` in the trace, which read as "expected" and
  // was in fact "never tested".
  //
  // The palette's answer arrives a cycle after its request, so its token is
  // latched rather than read live.
  logic [TOKW-1:0] clut_tok_r;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)               clut_tok_r <= '0;
    else if (disp_clut_valid) clut_tok_r <= disp_clut_tok;
  end

  wire [TOKW-1:0] rsp_tok = pal_lu_valid_o ? clut_tok_r : bil_out_tok;

  // TWO SOURCES, ONE PORT, SO ONE MUST WAIT.
  //
  // The bilinear lane and the palette both answer into FRAGROB's single sample
  // response port. `bil_out_ready = fr_tmu_rready` accepted the bilinear answer
  // whenever FRAGROB was ready -- including cycles where the palette also
  // answered and the mux chose the palette. The bilinear result was then
  // consumed by nobody while its producer believed it had been taken, and the
  // fragment waiting on it never completed.
  //
  // This could not happen while every request was tagged bilinear, because the
  // two paths were never active at once. It appeared the moment the class was
  // allowed to vary, which is the second bug in this response path that being
  // permanently idle had hidden.
  //
  // The palette has no ready of its own -- it is a fixed-latency lookup -- so
  // it wins and the lane back-pressures.
  //
  // THIS IS SAFE ONLY BECAUSE `zhao_texture_fragrob.tmu_rready_o` IS TIED
  // HIGH. That is a cross-module invariant, it is load-bearing, and nothing
  // stated it until now:
  //
  //   * PALETTE_RES has no `lu_ready_i`. Its answer is valid for exactly one
  //     clock and cannot be held.
  //   * `disp_clut_ready` is tied high, so a lookup is issued without asking
  //     whether its answer can be taken.
  //   * So if FRAGROB ever stops being unconditionally ready, the palette
  //     answer is DROPPED. The fragment waiting on that sample never
  //     completes, and because FRAGROB retires in allocation order, one stuck
  //     head blocks the whole island -- the exact signature the aux-token bug
  //     produced earlier ("48 samples fetched, 0 fragments out").
  //
  // The strict priority is therefore not a fairness choice; it is the only
  // safe order given one lane cannot wait. Making the merger round-robin
  // WITHOUT first giving the palette a holding register would introduce the
  // very drop this flag watches for.
  //
  // Unreachable today, which is why it is a flag and not a repair: a tripwire
  // on an assumption, so the day the assumption changes it is reported instead
  // of being discovered as a hang.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)                                err_rsp_dropped_o <= 1'b0;
    else if (pal_lu_valid_o && !fr_tmu_rready) err_rsp_dropped_o <= 1'b1;
  end

  assign bil_out_ready  = fr_tmu_rready && !pal_lu_valid_o;
  assign fr_tmu_rvalid  = bil_out_valid || pal_lu_valid_o;
  // RGB565 -> RGB888 BY REPLICATION, not by zero-fill. AUDIT R5/D23.
  //
  // This appended zeros: {r5, 3'b000}. The ABI is written down --
  // zref_texture.hpp names `zref::sky::rgb565::to_rgb888` as THE expansion, and
  // that function replicates the high bits:
  //
  //     r = (r5 << 3) | (r5 >> 2);      31 -> 255
  //     g = (g6 << 2) | (g6 >> 4);      63 -> 255
  //     b = (b5 << 3) | (b5 >> 2);
  //
  // Zero-fill caps every channel below its intended maximum: 31 became 248, so
  // full white returned 248, 252, 248. The error is exactly zero at the bottom
  // of each channel and worst at the top, which is why it survived every
  // "did anything paint" check the composed test had.
  //
  // It also made the reference unusable as an oracle for this path: every
  // palette pixel differed from zref by this amount, so a per-texel comparison
  // could not have been written against it until this matched.
  wire [7:0] pal_r8 = {pal_lu_rgb565[15:11], pal_lu_rgb565[15:13]};
  wire [7:0] pal_g8 = {pal_lu_rgb565[10:5],  pal_lu_rgb565[10:9]};
  wire [7:0] pal_b8 = {pal_lu_rgb565[4:0],   pal_lu_rgb565[4:2]};

  assign fr_tmu_rgb     = pal_lu_valid_o
                          ? {pal_r8, pal_g8, pal_b8}
                          // THREE CHANNELS, not one replicated three times.
                          : bil_rgb;
  assign fr_tmu_a       = 8'hFF;
  assign fr_tmu_rslot   = rsp_tok[$clog2(DEPTH)+2+GENW-1 -: $clog2(DEPTH)];
  assign fr_tmu_rsidx   = rsp_tok[GENW+1 -: 2];
  assign fr_tmu_rgen    = rsp_tok[GENW-1:0];

  // ==========================================================================
  // AUX
  // ==========================================================================
  logic        aux_req_ready, aux_out_valid;
  logic [AUX_TOKW-1:0] aux_out_tok;
  logic [7:0]  aux_out_tag, aux_out_str;
  logic        aux_out_degenerate;
  logic [31:0] aux_sheet_reads, aux_degenerate;

  assign fr_aux_ready = aux_req_ready;

  zhao_texture_aux_pipe #(.TOKW(AUX_TOKW)) u_aux (
      .clk(clk), .rst_n(rst_n),
      .req_valid_i(fr_aux_valid), .req_ready_o(aux_req_ready),
      .req_wx_i(fr_aux_ctx[31:0]), .req_wz_i(fr_aux_ctx[63:32]),
      .req_env_x0_i(32'sd0), .req_env_x1_i(32'sd65536),
      .req_env_z0_i(32'sd0), .req_env_z1_i(32'sd65536),
      .req_tok_i({fr_aux_slot, fr_aux_gen}),
      .sheet_valid_o(sheet_valid_o), .sheet_ready_i(sheet_ready_i),
      .sheet_u_o(sheet_u_o), .sheet_v_o(sheet_v_o), .sheet_tok_o(sheet_tok_o),
      .sheet_rvalid_i(sheet_rvalid_i), .sheet_tag_i(sheet_tag_i),
      .sheet_str_i(sheet_str_i), .sheet_rtok_i(sheet_rtok_i),
      .out_valid_o(aux_out_valid), .out_ready_i(fr_aux_rready),
      .out_tok_o(aux_out_tok), .out_tag_o(aux_out_tag),
      .out_str_o(aux_out_str), .out_degenerate_o(aux_out_degenerate),
      .accepted_o(cnt_aux_accepted_o), .sheet_reads_o(aux_sheet_reads),
      .degenerate_o(aux_degenerate));

  assign fr_aux_rvalid = aux_out_valid;
  assign fr_aux_rgb    = {aux_out_tag, aux_out_str, 8'd0};
  assign fr_aux_a      = aux_out_degenerate ? 8'd0 : 8'hFF;
  assign fr_aux_rslot  = aux_out_tok[AUX_TOKW-1 -: $clog2(DEPTH)];
  assign fr_aux_rgen   = aux_out_tok[GENW-1:0];

  // ==========================================================================
  // FRAGROB -> MATERIAL.COMBINE.V1
  // ==========================================================================
  // GLUE 1 IS GONE. The first draft of this file held the last two retirements
  // in a shift register here and called it a sample bank, because FRAGROB
  // appeared to expose only one colour. It banks all three internally --
  // `res_rgb_m [3][DEPTH]` -- and its retire read simply took `res_rgb_m[0]`
  // and dropped the rest. So the missing piece was two output ports, not a
  // buffer in the top, and the combiner now reads REAL per-sample results.
  //
  // Worth stating plainly because the wrong version would have measured fine:
  // a shift register of past fragments has a size, fits, and reports numbers.
  // It would have composed an island whose combiner blended a fragment with
  // its two predecessors and called that a three-sample material.

  logic comb_f_ready;
  assign fr_o_ready = comb_f_ready;

  logic [31:0] comb_refused_recipe, comb_sat_add, comb_sat_2x;
  logic [31:0] comb_jobs [8];
  assign cnt_combine_jobs_o = comb_jobs;

  // ==========================================================================
  // THE ORDERING BOUNDARY (audit R6)
  //
  // The audit's finding, and it was a fair one: the composed test asserted a
  // BOUNDED displacement -- no fragment more than 8 places from where it was
  // submitted -- and a bounded defect is not an ordering guarantee. Raster and
  // blend semantics need the boundary itself.
  //
  // The reordering is not a fault to be removed. TMU responses come back out of
  // order because the memory does, and `zhao_texture_material_combine_v1`
  // retires out of order on purpose -- its own leaf test exercises that, because
  // a one-sample recipe genuinely finishes before a three-sample one that
  // started earlier and holding it back would cost throughput for nothing. So
  // the machine reorders INSIDE and orders at its EDGE, which is where the
  // caller's semantics live.
  //
  // A REORDER BUFFER, NOT A CAM AND NOT A STALL. The sequence stamped at
  // admission indexes an FCTXN-entry table directly; completed fragments are
  // written wherever they land and the head pointer walks forward as entries
  // become present. It cannot overflow: FRAGROB admits at most FCTXN
  // fragments, so at most FCTXN sequence numbers are live and each has its own
  // slot. The combiner's `o_ready_i` is therefore tied high -- a completing
  // fragment always has somewhere to go, and back-pressure from downstream
  // parks in the buffer instead of stalling the pipe.
  logic        comb_o_valid, comb_o_ready;
  logic [23:0] comb_o_rgb;
  logic [7:0]  comb_o_a;
  logic [ROBTAGW-1:0] comb_o_tag;
  logic        comb_o_refused;

  zhao_texture_material_combine_v1 #(.RECS(2), .TAGW(ROBTAGW)) u_combine (
      .clk(clk), .rst_n(rst_n),
      .f_valid_i(fr_o_valid), .f_ready_o(comb_f_ready),
      .f_sample_count_i(fsc_m[fr_o_tok]), .f_recipe_i(frec_m[fr_o_tok]),
      .f_weight_i(fwt_m[fr_o_tok]),
      .f_s0_rgb_i(fr_o_s_rgb[0]), .f_s0_a_i(fr_o_s_a[0]),
      .f_s1_rgb_i(fr_o_s_rgb[1]), .f_s1_a_i(fr_o_s_a[1]),
      // AUX, when the fragment has it, genuinely IS the third sample -- that
      // is what the aux pipeline computes. Sample bank 2 is the fallback for
      // fragments that do not.
      .f_s2_rgb_i(fr_o_has_aux ? fr_o_aux_rgb : fr_o_s_rgb[2]),
      .f_s2_a_i  (fr_o_has_aux ? fr_o_aux_a   : fr_o_s_a[2]),
      .f_base_rgb_i(fbase_m[fr_o_tok][31:8]),
      .f_base_a_i(fbase_m[fr_o_tok][7:0]),
      .f_tag_i({fseq_m[fr_o_tok], fr_o_ctx[15:0]}),
      .o_valid_o(comb_o_valid), .o_ready_i(comb_o_ready),
      .o_rgb_o(comb_o_rgb), .o_a_o(comb_o_a), .o_tag_o(comb_o_tag),
      .o_refused_o(comb_o_refused),
      .refused_recipe_o(comb_refused_recipe),
      .refused_missing_o(cnt_combine_refused_o),
      .saturated_add_o(comb_sat_add), .saturated_mul2x_o(comb_sat_2x),
      .jobs_by_recipe_o(comb_jobs));

  // -------- reorder buffer --------------------------------------------------
  logic [32:0] rob_m [FCTXN];      // {refused, a, rgb}
  logic        rob_full_m [FCTXN];
  logic [15:0] rob_tag_m [FCTXN];
  logic [FCTXW-1:0] seq_head_r;

  wire [FCTXW-1:0] comb_seq = comb_o_tag[ROBTAGW-1:16];
  assign comb_o_ready = 1'b1;      // cannot back up: one slot per live fragment

  // HEAD-OF-LINE STALL IS DELIBERATE, and it is the honest cost of this
  // boundary. If a fragment is admitted and never completes -- the condition
  // `err_rsp_dropped_o` counts -- the head stops and the island stops emitting,
  // rather than quietly skipping it and shipping a reordered stream. A lost
  // fragment was already a fault; this makes it a LOUD one. A timeout that
  // released the head would turn a stuck machine back into a silently
  // out-of-order one, which is the thing this block exists to prevent.
  wire rob_hit = rob_full_m[seq_head_r];

  assign out_valid_o   = rob_hit;
  assign out_rgb_o     = rob_m[seq_head_r][23:0];
  assign out_a_o       = rob_m[seq_head_r][31:24];
  assign out_refused_o = rob_m[seq_head_r][32];
  assign out_tag_o     = rob_tag_m[seq_head_r];

  // OBSERVABILITY, because an ordering boundary that works silently is
  // indistinguishable from one that was never needed. `cnt_reorder_held_o`
  // counts cycles in which a completed fragment was waiting behind an earlier
  // one; a run where it stays at zero has not exercised this at all, and a
  // test asserting strict order on such a run proves nothing.
  logic [31:0] rob_held_r;
  assign cnt_reorder_held_o = rob_held_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      seq_head_r <= '0;
      rob_held_r <= 32'd0;
      for (int i = 0; i < FCTXN; i++) rob_full_m[i] <= 1'b0;
    end else begin
      if (comb_o_valid) begin
        rob_m[comb_seq]      <= {comb_o_refused, comb_o_a, comb_o_rgb};
        rob_tag_m[comb_seq]  <= comb_o_tag[15:0];
        rob_full_m[comb_seq] <= 1'b1;
        // A fragment that completed while an EARLIER one is still missing is
        // exactly the event this boundary exists for. Counted at arrival, not
        // per stalled cycle, so the number is "how many times did order have
        // to be restored" rather than "how long was the queue".
        if (comb_seq != seq_head_r) rob_held_r <= rob_held_r + 32'd1;
      end
      if (out_valid_o && out_ready_i) begin
        rob_full_m[seq_head_r] <= 1'b0;
        seq_head_r             <= seq_head_r + 1'b1;
      end
    end
  end

endmodule
