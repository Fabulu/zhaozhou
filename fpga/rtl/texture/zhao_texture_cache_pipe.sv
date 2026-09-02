// zhao_texture_cache_pipe.sv — the texture cache, staged, with fill multicast.
//
// BESIDE `zhao_texture_cache.sv`, which stays the cache of record. Nothing
// instantiates this yet.
//
// ---------------------------------------------------------------------------
// TWO DEFECTS THE BRIEF NAMES
// ---------------------------------------------------------------------------
// 1. THE OUTPUT READY REACHES REQUEST ACCEPTANCE.
//
//        assign acc_ready_o = (need_c == 0) && !fill_busy_r
//                          && (!s1_v_r || smp_ready_i);
//
//    `smp_ready_i` is the CONSUMER's ready and it lands on the accept line
//    through one combinational path. The brief: "No cache output-ready signal
//    reaches TMU request acceptance in one combinational path." Here the accept
//    line reads a local request FIFO and nothing else.
//
// 2. FOUR LANES WANTING ONE LINE CAUSE FOUR FILLS.
//
//    The shipped `pick_lane` chooses the lowest-numbered needing lane, fills
//    it, and re-checks -- so a four-tap footprint landing inside one physical
//    line fetches that line FOUR TIMES. The brief:
//
//      > A miss should capture fill_lane_mask and line identity. Every returned
//      > fill beat is written into every matching lane in the mask. Do not
//      > fetch the same line separately for each lane.
//
//    That is not a small saving. A bilinear footprint on an interior texel has
//    all four taps within one 16-byte line most of the time, so the common
//    case is 4x the memory traffic it needs.
//
// ---------------------------------------------------------------------------
// WHAT IS DELIBERATELY NOT DONE
// ---------------------------------------------------------------------------
//   > Keep one blocking miss initially. Do not add MSHRs or hit-under-miss
//   > until real traces prove the blocking miss engine, rather than hit-path
//   > timing, is the remaining limiter.
//
// So one miss at a time, and the fill blocks. Adding hit-under-miss here would
// be building for a bottleneck nobody has measured -- the same mistake as
// four bilinear lanes, or BINNER being named an offender for eleven fits
// without appearing in a single worst-100.
//
// The geometry is transcribed from the shipped cache, not chosen:
//     tag  = addr[OFF_W+IDX_W +: TAG_W]      idx = addr[OFF_W +: IDX_W]
//     beat = addr[1 +: BEAT_W]
// with LANES=4, LINES=16, LINE_BYTES=16 giving OFF_W=4, IDX_W=4, BEAT_W=3,
// TAG_W=24. Guessing any of those would produce plausible addresses that are
// wrong, which this session has already done once.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_cache_pipe #(
    parameter int unsigned LANES      = 4,
    parameter int unsigned LINES      = 16,
    parameter int unsigned LINE_BYTES = 16,
    parameter int unsigned REQN       = 4    // local request FIFO
) (
    input var logic clk,
    input var logic rst_n,

    // ---- access --------------------------------------------------------------
    input  var logic                acc_valid_i,
    output var logic                acc_ready_o,
    input  var logic [LANES-1:0]    acc_en_i,
    input  var logic [LANES*32-1:0] acc_addr_i,
    input  var logic [15:0]         acc_src_id_i,

    // ---- response ------------------------------------------------------------
    output var logic                smp_valid_o,
    input  var logic                smp_ready_i,
    output var logic [LANES*16-1:0] smp_data_o,
    output var logic [15:0]         smp_src_id_o,

    // ---- fill ----------------------------------------------------------------
    output var logic                fill_valid_o,
    input  var logic                fill_ready_i,
    output var logic [31:0]         fill_addr_o,
    input  var logic                fill_data_valid_i,
    input  var logic [15:0]         fill_data_i,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]         cache_hits_o,
    output var logic [31:0]         cache_misses_o,
    output var logic [31:0]         fills_o,        // LINE fetches, not lane misses
    output var logic [31:0]         multicast_o     // lanes served by one fill
);

  localparam int unsigned OFF_W  = $clog2(LINE_BYTES);   // 4
  localparam int unsigned IDX_W  = $clog2(LINES);        // 4
  localparam int unsigned HW_PL  = LINE_BYTES / 2;       // 8
  localparam int unsigned BEAT_W = $clog2(HW_PL);        // 3
  localparam int unsigned TAG_W  = 32 - OFF_W - IDX_W;   // 24

  // ---- storage, same shape as the shipped cache ---------------------------
  logic [15:0]      data_r  [LANES][LINES * HW_PL];
  logic [TAG_W-1:0] tag_r   [LANES][LINES];
  logic             valid_r [LANES][LINES];

  // ======================================================================= C0
  // The local request FIFO. THIS is what acc_ready_o depends on -- nothing
  // downstream, and never smp_ready_i.
  logic [LANES-1:0]    rq_en   [REQN];
  logic [LANES*32-1:0] rq_addr [REQN];
  logic [15:0]         rq_src  [REQN];
  logic [1:0]          rq_wp, rq_rp;
  logic [2:0]          rq_n;

  assign acc_ready_o = (rq_n != 3'(REQN));

  logic rq_head_v;
  assign rq_head_v = (rq_n != 3'd0);

  // ======================================================================= C1
  // Per-lane tag/index/beat from the head request.
  logic [TAG_W-1:0]  h_tag  [LANES];
  logic [IDX_W-1:0]  h_idx  [LANES];
  logic [BEAT_W-1:0] h_beat [LANES];
  logic [LANES-1:0]  h_hit, h_need;
  always_comb begin
    for (int unsigned k = 0; k < LANES; k++) begin
      h_tag[k]  = rq_addr[rq_rp][32*k + OFF_W + IDX_W +: TAG_W];
      h_idx[k]  = rq_addr[rq_rp][32*k + OFF_W +: IDX_W];
      h_beat[k] = rq_addr[rq_rp][32*k + 1 +: BEAT_W];
      h_hit[k]  = valid_r[k][h_idx[k]] && (tag_r[k][h_idx[k]] == h_tag[k]);
    end
    h_need = rq_en[rq_rp] & ~h_hit;
  end

  // ---- the miss engine: ONE line, and every lane that wants it -----------
  // fill_lane_mask is the whole point. The lowest-numbered needing lane names
  // the line; every OTHER needing lane whose (tag,idx) matches joins the mask
  // and is written by the same beats.
  logic [TAG_W-1:0]  m_tag_c;
  logic [IDX_W-1:0]  m_idx_c;
  logic [LANES-1:0]  m_mask_c;
  logic              m_any_c;
  always_comb begin
    m_any_c  = 1'b0;
    m_tag_c  = '0;
    m_idx_c  = '0;
    m_mask_c = '0;
    for (int unsigned k = 0; k < LANES; k++) begin
      if (!m_any_c && h_need[k]) begin
        m_any_c = 1'b1;
        m_tag_c = h_tag[k];
        m_idx_c = h_idx[k];
      end
    end
    if (m_any_c) begin
      for (int unsigned k = 0; k < LANES; k++)
        if (h_need[k] && h_tag[k] == m_tag_c && h_idx[k] == m_idx_c) m_mask_c[k] = 1'b1;
    end
  end

  logic [2:0] mask_pop_c;
  always_comb begin
    mask_pop_c = 3'd0;
    for (int unsigned k = 0; k < LANES; k++) mask_pop_c = mask_pop_c + 3'(m_mask_c[k]);
  end

  logic              fb_busy_r, fb_req_r;
  logic [TAG_W-1:0]  fb_tag_r;
  logic [IDX_W-1:0]  fb_idx_r;
  logic [BEAT_W-1:0] fb_beat_r;
  logic [LANES-1:0]  fb_mask_r;

  assign fill_valid_o = fb_req_r;
  assign fill_addr_o  = {fb_tag_r, fb_idx_r, {OFF_W{1'b0}}};

  // ======================================================================= C4
  // The local response FIFO. Output ready terminates HERE.
  logic [LANES*16-1:0] rs_data [REQN];
  logic [15:0]         rs_src  [REQN];
  logic [1:0]          rs_wp, rs_rp;
  logic [2:0]          rs_n;

  assign smp_valid_o  = (rs_n != 3'd0);
  assign smp_data_o   = rs_data[rs_rp];
  assign smp_src_id_o = rs_src[rs_rp];

  logic rs_room;
  assign rs_room = (rs_n != 3'(REQN));

  // An all-hit head retires when the response FIFO has room. A head with a
  // miss starts a fill instead and retires on a later pass.
  logic head_all_hit, head_go;
  assign head_all_hit = rq_head_v && (h_need == '0);
  assign head_go      = head_all_hit && rs_room;

  logic [2:0] en_pop_c;
  always_comb begin
    en_pop_c = 3'd0;
    for (int unsigned k = 0; k < LANES; k++) en_pop_c = en_pop_c + 3'(rq_en[rq_rp][k]);
  end

  logic [LANES*16-1:0] head_data_c;
  always_comb begin
    for (int unsigned k = 0; k < LANES; k++)
      head_data_c[16*k +: 16] = data_r[k][{h_idx[k], h_beat[k]}];
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rq_wp <= '0; rq_rp <= '0; rq_n <= '0;
      rs_wp <= '0; rs_rp <= '0; rs_n <= '0;
      fb_busy_r <= 1'b0;
      fb_req_r  <= 1'b0;
      cache_hits_o   <= 32'd0;
      cache_misses_o <= 32'd0;
      fills_o        <= 32'd0;
      multicast_o    <= 32'd0;
      for (int unsigned k = 0; k < LANES; k++)
        for (int unsigned i = 0; i < LINES; i++) valid_r[k][i] <= 1'b0;
    end else begin
      // ---- C0: accept; the count moves ONCE ------------------------------
      begin
        automatic logic psh = acc_valid_i && acc_ready_o;
        automatic logic pop = head_go;
        if (psh && !pop)      rq_n <= rq_n + 3'd1;
        else if (!psh && pop) rq_n <= rq_n - 3'd1;
        if (psh) begin
          rq_en[rq_wp]   <= acc_en_i;
          rq_addr[rq_wp] <= acc_addr_i;
          rq_src[rq_wp]  <= acc_src_id_i;
          rq_wp <= rq_wp + 2'd1;
        end
        if (pop) rq_rp <= rq_rp + 2'd1;
      end

      // ---- C4: retire an all-hit access ----------------------------------
      begin
        automatic logic rpsh = head_go;
        automatic logic rpop = smp_valid_o && smp_ready_i;
        if (rpsh && !rpop)      rs_n <= rs_n + 3'd1;
        else if (!rpsh && rpop) rs_n <= rs_n - 3'd1;
        if (rpsh) begin
          rs_data[rs_wp] <= head_data_c;
          rs_src[rs_wp]  <= rq_src[rq_rp];
          rs_wp <= rs_wp + 2'd1;
          // Same fault, same fix: one add of the enabled-lane popcount.
          cache_hits_o <= cache_hits_o + 32'(en_pop_c);
        end
        if (rpop) rs_rp <= rs_rp + 2'd1;
      end

      // ---- the miss engine -----------------------------------------------
      if (!fb_busy_r) begin
        if (rq_head_v && m_any_c) begin
          fb_busy_r <= 1'b1;
          fb_req_r  <= 1'b1;
          fb_tag_r  <= m_tag_c;
          fb_idx_r  <= m_idx_c;
          fb_mask_r <= m_mask_c;
          fb_beat_r <= '0;
          fills_o   <= fills_o + 32'd1;
          // COUNT ONCE, NOT PER LANE IN A LOOP. Four non-blocking
          // `cache_misses_o <= cache_misses_o + 1` all read the SAME old value,
          // so only one lands: the counter reported 1 lane miss for a
          // four-lane miss. The popcount is computed and added once.
          //
          // Worth fixing rather than shrugging at, because this is an EVIDENCE
          // output. A counter that silently under-reports is exactly the kind
          // of number that gets trusted later precisely because a tool
          // produced it.
          cache_misses_o <= cache_misses_o + 32'(mask_pop_c);
          // Lanes served by this ONE fetch beyond the first -- the multicast
          // saving, which is zero when only one lane wanted the line.
          multicast_o <= multicast_o + 32'(mask_pop_c) - 32'd1;
          // The lines being filled are invalid until the last beat lands.
          for (int unsigned k = 0; k < LANES; k++)
            if (m_mask_c[k]) valid_r[k][m_idx_c] <= 1'b0;
        end
      end else begin
        if (fb_req_r && fill_ready_i) fb_req_r <= 1'b0;
        if (fill_data_valid_i) begin
          // EVERY matching lane is written by the SAME beat. This loop is the
          // multicast; the shipped cache writes one lane and refetches.
          for (int unsigned k = 0; k < LANES; k++)
            if (fb_mask_r[k]) data_r[k][{fb_idx_r, fb_beat_r}] <= fill_data_i;
          if (fb_beat_r == BEAT_W'(HW_PL - 1)) begin
            for (int unsigned k = 0; k < LANES; k++)
              if (fb_mask_r[k]) begin
                tag_r[k][fb_idx_r]   <= fb_tag_r;
                valid_r[k][fb_idx_r] <= 1'b1;
              end
            fb_busy_r <= 1'b0;
          end
          fb_beat_r <= fb_beat_r + BEAT_W'(1);
        end
      end
    end
  end

endmodule : zhao_texture_cache_pipe

`default_nettype wire
